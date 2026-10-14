#!/usr/bin/env python3
"""HOOTL scenario runner for the thermostat firmware simulator (tstat_sim).

Every scenario is a deterministic, seeded script of timed commands for the fake
hardware (see fake/*.c). The runner boots the simulator, restarts it whenever
the simulated chip resets (same state file = same NVS/otadata/Zigbee NVRAM),
then checks
  * the scenario's own expectations on the structured "EVT ..." lines, and
  * the W/Y/G safety monitor over the GPIO trace of the WHOLE run (all boots).

Usage: hootl.py [-k SUBSTRING] [-j JOBS] [--list] [--keep DIR] [--sim PATH]
"""
import argparse, os, re, shutil, subprocess, sys, tempfile, time, traceback
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_SIM = os.path.join(HERE, "build", "tstat_sim")

SEC = 1000
MIN = 60 * SEC
HOUR = 60 * MIN
W, Y, G, LED, WDI, WDEN = 0, 1, 2, 8, 10, 11
PIN_NAME = {W: "W", Y: "Y", G: "G"}

# timing rules (components/control/include/control.h CTRL_PARAMS_DEFAULT)
COMP_MIN_OFF = 5 * MIN
HEAT_MIN_OFF = 1 * MIN
CHANGEOVER = 5 * MIN
COMP_MIN_ON = 3 * MIN
HEAT_MIN_ON = 2 * MIN
# a run shorter than min-on is only legitimate if one of these happened within 1 s of the stop
MIN_ON_EXCUSE = re.compile(r"^(RESET|BOOT )|EVT ctrl fault=(?!OK)|EVT output fault|EVT fault output|EVT safe mode|"
                           r"rolling back|factory reset|OTA end|INTERLOCK")

SCENARIOS = []


class ScenarioFail(Exception):
    pass


def scenario(name, until, timeout=120, seed=1, max_boots=1, doc="", stuck_ok=(), tags=()):
    """Register a scenario. `until` = virtual ms to run; `timeout` = wall-clock s;
    `max_boots` = allowed boots (1 = no reset may happen); `stuck_ok` = pins the scenario
    deliberately breaks (safety rules that depend on them are not checked after injection)."""
    def deco(fn):
        SCENARIOS.append(dict(name=name, fn=fn, until=until, timeout=timeout, seed=seed,
                              max_boots=max_boots, doc=doc or (fn.__doc__ or "").strip(),
                              stuck_ok=tuple(stuck_ok), tags=tuple(tags)))
        return fn
    return deco


class Script:
    """Timed command list for the simulator (absolute virtual ms)."""
    DAY0 = 9497 * 86400   # 2026-01-01 00:00 in seconds since 2000-01-01 (Zigbee epoch)

    def __init__(self):
        self.cmds = []

    def at(self, t, cmd):
        self.cmds.append((int(t), cmd))
        return self

    # --- convenience -------------------------------------------------------
    def temp(self, t, c100):              return self.at(t, f"temp {int(c100)}")
    def set(self, t, key, val):           return self.at(t, f"set {key} {int(val)}")
    def zb(self, t, cmd):                 return self.at(t, "zb " + cmd)
    def mode(self, t, m):                 # 0 off, 3 cool, 4 heat
        return self.zb(t, f"write 0x0201 0x001c u8 {m}")
    def heat_sp(self, t, c100):           return self.zb(t, f"write 0x0201 0x0012 s16 {int(c100)}")
    def cool_sp(self, t, c100):           return self.zb(t, f"write 0x0201 0x0011 s16 {int(c100)}")
    def fan(self, t, on):                 return self.zb(t, f"write 0x0202 0x0000 u8 {4 if on else 5}")
    def clock(self, t, hh, mm=0, extra=""):
        """Coordinator Time cluster: local time hh:mm at virtual time t (UTC = local, no zone)."""
        epoch0 = self.DAY0 + hh * 3600 + mm * 60 - t // 1000
        return self.zb(t, f"time {epoch0} {extra}".strip())
    def press(self, t, btn, ms):
        self.at(t, f"btn {btn} press")
        return self.at(t + ms, f"btn {btn} release")
    def room(self, t, start_c100, ambient_c100, heat=50, cool=40, leak=1):
        for k, v in (("room.t", start_c100), ("room.ambient", ambient_c100), ("room.heat", heat),
                     ("room.cool", cool), ("room.leak", leak), ("room.model", 1)):
            self.set(t, k, v)
        return self

    def text(self):
        return "".join(f"{t} {c}\n" for t, c in sorted(self.cmds, key=lambda x: x[0]))


class Result:
    LINE = re.compile(r"^@(\d+) (.*)$")

    def __init__(self, lines, boots, outdir):
        self.ev = []
        for ln in lines:
            m = self.LINE.match(ln)
            if m:
                self.ev.append((int(m.group(1)), m.group(2)))
        self.boots = boots
        self.outdir = outdir
        self.boot_times = [t for t, x in self.ev if x.startswith("BOOT ")]
        self.resets = [(t, x.split(" ", 1)[1]) for t, x in self.ev if x.startswith("RESET ")]

    # --- queries -------------------------------------------------------------
    def find_all(self, rx, after=0, before=None):
        r = re.compile(rx)
        return [(t, x) for t, x in self.ev if t >= after and (before is None or t < before) and r.search(x)]

    def find(self, rx, after=0, before=None):
        f = self.find_all(rx, after, before)
        return f[0][0] if f else None

    def expect(self, rx, after=0, before=None, msg=""):
        t = self.find(rx, after, before)
        if t is None:
            raise ScenarioFail(f"expected /{rx}/ in [{after}, {before}) {msg}")
        return t

    def never(self, rx, after=0, before=None, msg=""):
        f = self.find_all(rx, after, before)
        if f:
            raise ScenarioFail(f"unexpected /{rx}/ at @{f[0][0]}: {f[0][1]} {msg}")

    def count(self, rx, after=0, before=None):
        return len(self.find_all(rx, after, before))

    def last(self, rx, before=None):
        f = self.find_all(rx, 0, before)
        return f[-1] if f else None

    def pin(self, pin, t):
        lvl = 0
        for tt, x in self.ev:
            if tt > t:
                break
            if x.startswith(f"GPIO {pin} "):
                lvl = int(x.split()[2])
        return lvl

    def high_ms(self, pin, t0, t1):
        lvl, since, total = self.pin(pin, t0), t0, 0
        for tt, x in self.ev:
            if tt <= t0 or tt >= t1 or not x.startswith(f"GPIO {pin} "):
                continue
            if lvl:
                total += tt - since
            lvl, since = int(x.split()[2]), tt
        if lvl:
            total += t1 - since
        return total

    def rises(self, pin, after=0, before=None):
        return [t for t, x in self.find_all(rf"^GPIO {pin} 1$", after, before)]

    def setpoint(self, before=None):
        """last 'EVT setpoint' fields as a dict"""
        f = self.last(r"EVT setpoint ", before)
        if not f:
            return None
        return {k: int(v) for k, v in re.findall(r"(\w+)=(-?\d+)", f[1])}

    def check(self, cond, msg):
        if not cond:
            raise ScenarioFail(msg)


def safety_monitor(r, stuck_ok=()):
    """Invariants over the whole W/Y/G trace (all boots). Returns a list of violations."""
    bad = []
    stuck_from = {}
    for t, x in r.ev:
        m = re.match(r"CMD set gpio\.(stuck|setfail|cfgfail)\.(\d+) ", x)
        if m and int(m.group(2)) in (W, Y, G):
            stuck_from.setdefault(int(m.group(2)), t)
    for p in stuck_ok:
        stuck_from.setdefault(p, 0)

    def broken(p, t):
        return p in stuck_from and t >= stuck_from[p]

    lvl = {W: 0, Y: 0, G: 0}
    last_fall = {W: None, Y: None, G: None}
    last_rise = {W: None, Y: None, G: None}
    boot_t, cfg = 0, False
    ctrl_fault, safe = False, False
    mode, excuses = None, []
    for t, x in r.ev:
        m = re.search(r"EVT setpoint mode=(\d+)", x)
        if m:
            if mode is not None and int(m.group(1)) != mode:
                excuses.append(t)
            mode = int(m.group(1))
        elif MIN_ON_EXCUSE.search(x):
            excuses.append(t)

    def excused(t):
        return any(abs(e - t) <= 1000 for e in excuses)

    i, ev = 0, r.ev
    while i < len(ev):
        t = ev[i][0]
        rises = []
        # apply every event of this millisecond, then check the resulting state
        while i < len(ev) and ev[i][0] == t:
            x = ev[i][1]
            if x.startswith("BOOT "):
                boot_t, cfg = t, False
                ctrl_fault, safe = False, False
                lvl = {W: 0, Y: 0, G: 0}
                last_rise = {W: None, Y: None, G: None}
            elif "EVT cfg loaded" in x:
                cfg = True
            elif "EVT ctrl fault=" in x:
                ctrl_fault = "fault=OK" not in x
            elif re.search(r"EVT safe mode: \d+ resets", x):
                safe = True
            elif x.startswith("GPIO "):
                _, p, v = x.split()
                p, v = int(p), int(v)
                if p in lvl:
                    if v and not lvl[p]:
                        rises.append(p)
                        last_rise[p] = t
                    if not v and lvl[p]:
                        last_fall[p] = t
                        mo = {W: HEAT_MIN_ON, Y: COMP_MIN_ON}.get(p)
                        if mo and last_rise[p] is not None and t - last_rise[p] < mo and not broken(p, t) \
                                and not excused(t):
                            bad.append(f"@{t}: {PIN_NAME[p]} ran only {t - last_rise[p]} ms (min on {mo}) with no fault/mode change/reset")
                    lvl[p] = v
            i += 1
        for p in rises:
            if broken(p, t):
                continue
            since_off = max([x for x in (last_fall[p], boot_t) if x is not None])
            if p == Y and t - since_off < COMP_MIN_OFF:
                bad.append(f"@{t}: Y on after only {t - since_off} ms off (compressor min off {COMP_MIN_OFF})")
            if p == W and t - since_off < HEAT_MIN_OFF:
                bad.append(f"@{t}: W on after only {t - since_off} ms off (heat min off {HEAT_MIN_OFF})")
            other = W if p == Y else Y if p == W else None
            if other is not None and last_fall[other] is not None and t - last_fall[other] < CHANGEOVER \
                    and last_fall[other] >= boot_t and not broken(other, t):
                bad.append(f"@{t}: {PIN_NAME[p]} on {t - last_fall[other]} ms after {PIN_NAME[other]} off (changeover {CHANGEOVER})")
        if lvl[W] and lvl[Y] and not (broken(W, t) or broken(Y, t)):
            bad.append(f"@{t}: W and Y on together")
        if lvl[Y] and not lvl[G] and not (broken(Y, t) or broken(G, t)):
            bad.append(f"@{t}: Y on without G")
        if (ctrl_fault or safe) and any(lvl[p] and not broken(p, t) for p in lvl):
            bad.append(f"@{t}: output on while {'safe mode' if safe else 'control failsafe'} is active "
                       f"(W={lvl[W]} Y={lvl[Y]} G={lvl[G]})")
        if not cfg and any(lvl[p] and not broken(p, t) for p in lvl):
            bad.append(f"@{t}: output on before the configuration was loaded (boot @{boot_t})")
        if len(bad) > 20:
            break
    return bad


def run_one(sc, sim, keep=None):
    s = Script()
    check = sc["fn"](s)
    tmp = tempfile.mkdtemp(prefix=f"hootl_{sc['name']}_", dir=keep) if keep else tempfile.mkdtemp(prefix="hootl_")
    script = os.path.join(tmp, "script.txt")
    state = os.path.join(tmp, "state.txt")
    with open(script, "w") as f:
        f.write(s.text())
    lines, boots, err = [], 0, ""
    deadline = time.time() + sc["timeout"]
    t0 = time.time()
    try:
        while True:
            boots += 1
            if boots > max(sc["max_boots"], 1) + 50:
                raise ScenarioFail(f"more than {sc['max_boots']} boots: reset loop")
            remaining = deadline - time.time()
            if remaining <= 0:
                raise ScenarioFail(f"wall-clock timeout {sc['timeout']} s")
            env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:abort_on_error=0", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
            p = subprocess.run([sim, "--state", state, "--script", script, "--until", str(sc["until"]),
                                "--seed", str(sc["seed"]), "--out", tmp],
                               capture_output=True, text=True, timeout=remaining, env=env)
            lines += p.stdout.splitlines()
            if p.stderr.strip():
                err += p.stderr
            if p.returncode == 3:
                continue
            if p.returncode != 0:
                raise ScenarioFail(f"simulator exit {p.returncode} (4 = deadlock, 5 = abort/bad command)")
            break
        r = Result(lines, boots, tmp)
        problems = []
        if err.strip():
            problems.append("sanitizer/stderr output:\n" + err[-3000:])
        if r.boots > sc["max_boots"]:
            problems.append(f"{r.boots} boots, at most {sc['max_boots']} allowed; resets: {r.resets}")
        for rx, what in ((r"^PANIC", "panic/abort"), (r"ZB WARN", "Zigbee API misuse"), (r"^CMDERR", "bad script command"),
                         (r"ESP_ERROR_CHECK failed", "ESP_ERROR_CHECK abort")):
            f = r.find_all(rx)
            if f:
                problems.append(f"{what} @{f[0][0]}: {f[0][1]}")
        problems += safety_monitor(r, sc["stuck_ok"])
        if check:
            try:
                check(r)
            except ScenarioFail as e:
                problems.append(str(e))
        ok = not problems
        return dict(name=sc["name"], ok=ok, problems=problems, boots=r.boots, wall=time.time() - t0,
                    events=len(r.ev), dir=tmp)
    except (ScenarioFail, subprocess.TimeoutExpired) as e:
        return dict(name=sc["name"], ok=False, problems=[str(e)], boots=boots, wall=time.time() - t0, events=len(lines), dir=tmp)
    except Exception:
        return dict(name=sc["name"], ok=False, problems=[traceback.format_exc()], boots=boots, wall=time.time() - t0,
                    events=len(lines), dir=tmp)
    finally:
        with open(os.path.join(tmp, "log.txt"), "w") as f:
            f.write("\n".join(lines) + "\n")
        if not keep:
            pass


def monitor_selftest():
    """The monitor must flag each seeded violation (and accept the clean trace)."""
    base = ["@0 BOOT n=1 reason=POWERON", "@0 GPIO 0 0", "@0 GPIO 1 0", "@0 GPIO 2 0", "@1 I evt: EVT cfg loaded mode=4"]
    cases = {
        "clean": (["@400000 GPIO 2 1", "@400000 GPIO 1 1", "@700000 GPIO 1 0", "@700000 GPIO 2 0"], None),
        "W+Y": (["@70000 GPIO 0 1", "@400000 GPIO 2 1", "@400000 GPIO 1 1"], "together"),
        "Y min off": (["@100000 GPIO 2 1", "@100000 GPIO 1 1"], "compressor min off"),
        "Y min on": (["@400000 GPIO 2 1", "@400000 GPIO 1 1", "@460000 GPIO 1 0"], "min on"),
        "Y min on excused": (["@400000 GPIO 2 1", "@400000 GPIO 1 1", "@460000 GPIO 1 0",
                              "@460000 GPIO 2 0", "@460000 I evt: EVT ctrl fault=SENSOR FAIL"], None),
        "Y without G": (["@400000 GPIO 1 1"], "without G"),
        "failsafe": (["@100000 I evt: EVT ctrl fault=SENSOR FAIL", "@100100 GPIO 2 1"], "failsafe"),
        "safe mode": (["@2 I evt: EVT safe mode: 5 resets", "@100100 GPIO 2 1"], "safe mode"),
        "before cfg": (["@0 BOOT n=2 reason=SW", "@0 GPIO 2 1"], "before the configuration"),
        "changeover": (["@70000 GPIO 0 1", "@300000 GPIO 0 0", "@400000 GPIO 2 1", "@400000 GPIO 1 1"], "changeover"),
    }
    ok = True
    for name, (extra, want) in cases.items():
        r = Result(base + extra, 1, None)
        got = safety_monitor(r)
        hit = any(want in g for g in got) if want else not got
        print(f"{'PASS' if hit else 'FAIL'}  monitor selftest: {name}" + ("" if hit else f"  got {got}"))
        ok &= hit
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-k", default="")
    ap.add_argument("-j", type=int, default=os.cpu_count() or 2)
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--keep", default=None, help="keep per-scenario logs/framebuffers under DIR")
    ap.add_argument("--sim", default=DEFAULT_SIM)
    ap.add_argument("--junit", default=None)
    ap.add_argument("--selftest", action="store_true", help="check the safety monitor flags seeded violations")
    a = ap.parse_args()
    if a.selftest:
        sys.exit(0 if monitor_selftest() else 1)
    import scenarios  # noqa: F401  (registers into hootl.SCENARIOS)
    import hootl as H
    sel = [s for s in H.SCENARIOS if a.k in s["name"]]
    if a.list:
        for s in sel:
            print(f"{s['name']:40s} {s['doc']}")
        return 0
    if not os.path.exists(a.sim):
        print(f"simulator not built: {a.sim}", file=sys.stderr)
        return 2
    if a.keep:
        os.makedirs(a.keep, exist_ok=True)
    with ThreadPoolExecutor(a.j) as ex:
        results = list(ex.map(lambda s: run_one(s, a.sim, a.keep), sel))
    fails = 0
    for r in results:
        print(f"{'PASS' if r['ok'] else 'FAIL'}  {r['name']:40s} boots={r['boots']:<3d} events={r['events']:<6d} {r['wall']:.1f}s")
        if not r["ok"]:
            fails += 1
            for p in r["problems"][:8]:
                print("      - " + p.replace("\n", "\n        "))
            print(f"      log: {r['dir']}/log.txt")
        elif not a.keep:
            shutil.rmtree(r["dir"], ignore_errors=True)
    print(f"\n{len(results) - fails}/{len(results)} scenarios passed")
    if a.junit:
        from xml.sax.saxutils import escape
        with open(a.junit, "w") as f:
            f.write(f'<testsuite name="hootl" tests="{len(results)}" failures="{fails}">\n')
            for r in results:
                f.write(f'  <testcase name="{r["name"]}" time="{r["wall"]:.2f}">')
                if not r["ok"]:
                    f.write(f'<failure>{escape(chr(10).join(r["problems"]))}</failure>')
                f.write("</testcase>\n")
            f.write("</testsuite>\n")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.path.insert(0, HERE)
    sys.exit(main())
