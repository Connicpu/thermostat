"""HOOTL scenarios. Every scenario also runs the W/Y/G safety monitor (hootl.safety_monitor).

Conventions: times are virtual ms after the first power-on. The coordinator is
up and permits joining unless a scenario says otherwise ("zb coord down").
Default settings: mode OFF, day heat 20.0 °C, day cool 24.5 °C, swing 0.5 °C.
"""
from hootl import scenario, SEC, MIN, HOUR, W, Y, G, WDI, WDEN

OFF, COOL, HEAT = 0, 3, 4
LINK_OFFLINE, LINK_PAIRING, LINK_JOINED, LINK_LOST, LINK_FAULT = range(5)


def heating(s, temp=1800, t=5 * SEC):
    s.temp(0, temp)
    s.mode(t, HEAT)


def ui_heat(s, temp=1800, t=5 * SEC):
    """Select HEAT with the buttons only (M menu, M edit Mode, UP = HEAT, M save)."""
    s.temp(0, temp)
    for k, b in enumerate(("mode", "mode", "up", "mode")):
        s.press(t + k * SEC, b, 200)


def cooling(s, temp=2700, t=5 * SEC):
    s.temp(0, temp)
    s.mode(t, COOL)


def heats(r, after, before=None, msg="W should come on"):
    r.check(any(r.pin(W, t) for t in range(after, before or after + 1, 250)) or r.rises(W, after, before), msg)


def w_on_between(r, t0, t1):
    return r.high_ms(W, t0, t1) > 0


# =============================================================================
# 0. basics
# =============================================================================

@scenario("boot_idle", until=2 * MIN)
def boot_idle(s):
    """Cold boot, mode OFF: config loaded, outputs off, joins, screen drawn."""
    s.temp(0, 2100)
    def check(r):
        r.expect(r"EVT cfg loaded")
        r.expect(r"EVT link 2", before=10 * SEC, msg="joins the coordinator")
        r.expect(r"EVT ctrl fault=OK", before=10 * SEC)
        r.expect(r"EPD refresh n=1 .*ink=([1-9]\d*)", before=5 * SEC, msg="first screen drawn")
        r.check(r.high_ms(W, 0, 2 * MIN) + r.high_ms(Y, 0, 2 * MIN) + r.high_ms(G, 0, 2 * MIN) == 0, "all outputs off")
    return check


@scenario("heat_cycles_room_model", until=3 * HOUR)
def heat_cycles(s):
    """Heat mode against a room model: W cycles around 20 °C; min on/off and fan run-on hold."""
    s.room(0, 1800, 1000, heat=40, leak=2)
    s.mode(5 * SEC, HEAT)
    def check(r):
        on = r.rises(W)
        r.check(len(on) >= 3, f"heating should cycle (W rises {on})")
        r.check(on[0] >= 60 * SEC, "heat min off after boot")
        temps = [int(x.split("= ")[1]) for t, x in r.find_all(r"ZB attr 0x0402/0x0000 = ", after=HOUR)]
        r.check(temps and min(temps) > 1900 and max(temps) < 2100, f"holds 20 °C +-1 (got {min(temps)}..{max(temps)})")
        r.check(r.high_ms(Y, 0, 3 * HOUR) == 0, "no cooling in heat mode")
    return check


@scenario("cool_cycles_room_model", until=3 * HOUR)
def cool_cycles(s):
    """Cool mode against a hot room: Y cycles, G always with Y, 5 min compressor off time."""
    s.room(0, 2700, 3200, cool=40, leak=2)
    s.mode(5 * SEC, COOL)
    def check(r):
        on = r.rises(Y)
        r.check(len(on) >= 3, f"cooling should cycle ({on})")
        r.check(on[0] >= 5 * MIN, "compressor waits 5 min after boot")
        r.check(r.high_ms(W, 0, 3 * HOUR) == 0, "no heat in cool mode")
    return check


@scenario("changeover_heat_to_cool", until=40 * MIN)
def changeover(s):
    """Heating, then mode -> COOL with a hot room: Y waits for the 5 min changeover gap."""
    s.temp(0, 1800)
    s.mode(5 * SEC, HEAT)
    s.temp(10 * MIN, 2800)
    s.mode(10 * MIN, COOL)
    def check(r):
        w_off = r.find(r"^GPIO 0 0$", after=2 * MIN)
        y_on = r.rises(Y)
        r.check(w_off and y_on, "W off then Y on")
        r.check(y_on[0] - w_off >= 5 * MIN, "changeover gap")
    return check


@scenario("fan_on_and_auto", until=10 * MIN)
def fan_on(s):
    """Fan ON with mode OFF runs G alone; back to AUTO stops it."""
    s.temp(0, 2100)
    s.fan(30 * SEC, True)
    s.fan(5 * MIN, False)
    def check(r):
        r.check(r.pin(G, 1 * MIN) == 1, "G on with Fan ON")
        r.check(r.pin(G, 6 * MIN) == 0, "G off with Fan AUTO")
        r.check(r.high_ms(W, 0, 10 * MIN) + r.high_ms(Y, 0, 10 * MIN) == 0, "no W/Y")
    return check


@scenario("menu_navigation_buttons", until=2 * MIN)
def menu_nav(s):
    """Buttons only: M opens the menu, M edits Mode, UP changes it, M saves -> mode leaves OFF."""
    s.temp(0, 2100)
    s.press(10 * SEC, "mode", 200)
    s.press(12 * SEC, "mode", 200)
    s.press(14 * SEC, "up", 200)
    s.press(16 * SEC, "mode", 200)
    def check(r):
        r.expect(r"EPD refresh", after=10 * SEC, before=12 * SEC, msg="menu drawn")
        sp = r.setpoint()
        r.check(sp["mode"] == HEAT, f"OFF -> UP -> HEAT from the menu ({sp})")
        r.expect(r"NVS set tstat/cfg3", after=16 * SEC, msg="saved")
    return check


@scenario("menu_temperature_offset", until=1 * MIN)
def menu_offset(s):
    """Menu Offset, next to Swing. Each UP adds 0.1 °C; M saves it.

    The reported room temperature rises by that same 0.1 °C, and Device info's raw
    reading does not. A second step saves 2.
    """
    s.temp(0, 2100)
    s.set(0, "sht.noise", 0)  # the 0.1 °C step has to be exact, not buried in sensor noise
    t = 10 * SEC
    s.press(t, "mode", 200)  # open the menu
    for i in range(5):       # Mode, Fan, Boost, Schedule, Swing -> Offset
        s.press(t + (i + 1) * SEC, "down", 200)
    s.press(t + 6 * SEC, "mode", 200)  # edit
    s.press(t + 7 * SEC, "up", 200)    # 0 -> +0.1 °C
    s.press(t + 8 * SEC, "mode", 200)  # save, back to the list
    s.press(t + 9 * SEC, "mode", 200)  # edit again from the saved offset
    s.press(t + 10 * SEC, "up", 200)   # +0.2 °C
    s.press(t + 11 * SEC, "mode", 200) # save
    for i in range(3):                 # Units, Zigbee, Device info
        s.press(t + (12 + i) * SEC, "down", 200)
    s.press(t + 15 * SEC, "mode", 200) # open Device info
    def check(r):
        r.expect(r"ZB attr 0x0201/0x0010 = 1", after=t + 8 * SEC, msg="first tenth republished")
        r.expect(r"ZB attr 0x0201/0x0010 = 2", after=t + 11 * SEC, msg="second tenth republished")
        r.expect(r"ZB report 0x0201/0x0010", after=t + 8 * SEC, msg="ZHA does not poll this attribute")
        r.expect(r"NVS set tstat/cfg3", after=t + 8 * SEC, msg="saved")
        temps = [int(x.split("= ")[1]) for _, x in r.find_all(r"ZB attr 0x0201/0x0000 = ")]
        temps = [v for v in temps if v != -32768]  # invalid until the first reading
        r.check(len(temps) >= 3, f"local temperature published ({temps})")
        base = temps[0]
        r.check(all(v == base for v in temps[:-2]), f"steady before the offset changes ({temps})")
        r.check(temps[-2] == base + 10, f"+0.1 °C raises the reported temperature by 0.1 °C ({temps})")
        r.check(temps[-1] == base + 20, f"the next 0.1 °C does it again ({temps})")
        raws = [int(x.split("=")[1]) for _, x in r.find_all(r"EVT raw temp=", after=t + 15 * SEC)]
        r.check(raws == [base], f"raw temperature stays {base} with calibration removed ({raws})")
    return check


@scenario("home_up_down_setpoint", until=2 * MIN)
def home_updown(s):
    """Main screen in HEAT: UP raises, DOWN lowers the heat setpoint one step each."""
    heating(s, 2100)
    s.press(20 * SEC, "up", 100)
    s.press(30 * SEC, "down", 100)
    s.press(40 * SEC, "down", 100)
    def check(r):
        a, b, c = r.setpoint(25 * SEC)["heat"], r.setpoint(35 * SEC)["heat"], r.setpoint(45 * SEC)["heat"]
        r.check(a > 2000 and b == 2000 and c < 2000, f"setpoints {a} {b} {c}")
    return check


@scenario("remote_auto_write_refused", until=1 * MIN)
def auto_refused(s):
    """ZHA writes SystemMode=Auto (1): refused, attribute put back to HEAT."""
    heating(s)
    s.mode(20 * SEC, 1)
    def check(r):
        r.expect(r"unsupported system mode 1 ignored")
        r.expect(r"ZB attr 0x0201/0x001c = 4", after=20 * SEC + 1, msg="real mode re-published")
        r.check(r.setpoint()["mode"] == HEAT, "stays in HEAT")
    return check


@scenario("remote_setpoint_clamped", until=1 * MIN)
def sp_clamped(s):
    """A remote heat setpoint of 40 °C is clamped into the allowed range."""
    heating(s)
    s.heat_sp(20 * SEC, 4000)
    def check(r):
        r.check(r.setpoint()["heat"] <= 3200, f"clamped ({r.setpoint()})")
    return check


@scenario("schedule_morning_boost_mocked_time", until=4 * HOUR)
def sched_boost(s):
    """Clock 05:30: night 18 °C until 06:00, then day 20 °C + 0.5 °C morning boost for 2 h, then 20 °C."""
    heating(s, 1950)
    s.clock(0, 5, 30)
    def check(r):
        r.check(r.setpoint(20 * MIN)["heat"] == 1800 and r.setpoint(20 * MIN)["sched"] == 2, "night program")
        r.check(r.setpoint(40 * MIN)["heat"] == 2050 and r.setpoint(40 * MIN)["sched"] == 3, "day + boost")
        r.check(r.setpoint(3 * HOUR)["heat"] == 2000 and r.setpoint(3 * HOUR)["sched"] == 1, "day after boost")
        r.check(not w_on_between(r, 5 * MIN, 29 * MIN), "no heat at night (19.5 > 18)")
        r.check(w_on_between(r, 31 * MIN, 60 * MIN), "boost heats")
    return check


@scenario("timed_boost_combo", until=30 * MIN)
def timed_boost(s):
    """UP+DOWN together starts a timed boost; it heats even at setpoint and ends by itself."""
    heating(s, 2000)
    s.at(2 * MIN, "btn up press"); s.at(2 * MIN + 50, "btn down press")
    s.at(2 * MIN + 300, "btn up release"); s.at(2 * MIN + 300, "btn down release")
    def check(r):
        r.expect(r"boost cmd 2 -> result", after=2 * MIN)
        r.check(w_on_between(r, 2 * MIN, 10 * MIN), "boost heats")
    return check



@scenario("boost_switch_timed_heat_to_target_cool", until=40 * MIN)
def boost_switch(s):
    """A timed heat boost is replaced by a cool-to-target boost (the card's "Switch"): W stops,
    Y starts only after the changeover / compressor min-off, never together (safety monitor)."""
    heating(s, 2000)
    boost = "0xfc57"
    s.zb(1 * MIN, f"write {boost} 0x0002 u8 1")          # direction heat
    s.zb(1 * MIN + 100, f"write {boost} 0x0001 u16 30")  # 30 min
    s.zb(1 * MIN + 200, f"write {boost} 0x0003 u8 2")    # start timed
    s.zb(8 * MIN, f"write {boost} 0x0002 u8 2")          # direction cool
    s.zb(8 * MIN + 100, f"write {boost} 0x0000 s16 1800")
    s.zb(8 * MIN + 200, f"write {boost} 0x0003 u8 1")    # start to target: replaces the timed boost
    def check(r):
        r.expect(r"boost cmd 2 -> result 1", after=1 * MIN, before=2 * MIN, msg="timed heat boost started")
        r.check(w_on_between(r, 1 * MIN, 8 * MIN), "timed heat boost runs W")
        r.expect(r"boost cmd 1 -> result 1", after=8 * MIN, before=9 * MIN, msg="cool-to-target boost replaced it")
        t_w_off = next((t for t in range(8 * MIN, 40 * MIN, 250) if not r.pin(W, t)), None)
        r.check(t_w_off is not None, "W drops after the switch")
        r.check(r.high_ms(Y, 8 * MIN, 40 * MIN) > 0, "the cool boost runs Y")
        t_y_on = next((t for t in range(8 * MIN, 40 * MIN, 250) if r.pin(Y, t)), None)
        r.check(t_y_on is not None and t_w_off is not None and t_y_on > t_w_off, "Y only after W is off")
        r.check(r.high_ms(W, (t_y_on or 40 * MIN), 40 * MIN) == 0, "no W while the cool boost runs")
    return check

# =============================================================================
# 1. watchdogs, resets, boot loops
# =============================================================================

@scenario("stwd100_missed_kick_resets", until=3 * MIN, max_boots=2)
def stwd_missed(s):
    """Main loop hangs after the STWD100 is armed: it resets the chip in ~1.6 s; outputs off; next boot normal."""
    heating(s)
    s.set(0, "hang.boots", 1)
    s.set(0, "hang.after", 70 * SEC)
    def check(r):
        r.check(r.resets and r.resets[0][1] == "STWD100", f"STWD100 reset ({r.resets})")
        hang = r.expect(r"HANG app task")
        kick = r.last(r"^GPIO 10 ", before=hang + 1)[0]
        r.check(1590 <= r.resets[0][0] - kick <= 1700, f"reset {r.resets[0][0] - kick} ms after the last WDI edge (tWD 1.6 s)")
        heats(r, 3 * MIN - 1, msg="heating resumes after the reboot")
    return check


@scenario("task_wdt_before_stwd_armed", until=1 * MIN, max_boots=2)
def twdt(s):
    """Hang before the STWD100 is armed (t < 10 s): the task watchdog (5 s) resets instead."""
    s.temp(0, 2100)
    s.set(0, "hang.boots", 1)
    s.set(0, "hang.after", 3 * SEC)
    def check(r):
        r.check(r.resets and r.resets[0][1] == "TASK_WDT", f"{r.resets}")
    return check


@scenario("reset_mid_cool_keeps_compressor_off_time", until=30 * MIN, max_boots=2)
def reset_mid_cool(s):
    """Power cut while cooling: after reboot Y stays off >= 5 min (monitor), then cools again."""
    cooling(s)
    s.at(8 * MIN, "power cycle")
    def check(r):
        r.check(r.pin(Y, 8 * MIN - 1) == 1, "cooling before the reset")
        boot2 = r.boot_times[1]
        rises = r.rises(Y, after=boot2)
        r.check(rises and rises[0] - boot2 >= 5 * MIN, f"Y after reboot at {rises}")
    return check


@scenario("reset_mid_heat_keeps_heat_off_time", until=10 * MIN, max_boots=2)
def reset_mid_heat(s):
    """STWD100 reset while heating: W off through reset, back on >= 60 s after boot."""
    heating(s)
    s.set(0, "hang.boots", 1)
    s.set(0, "hang.after", 3 * MIN)
    def check(r):
        boot2 = r.boot_times[1]
        r.check(r.pin(W, boot2) == 0, "W low at reset")
        rises = r.rises(W, after=boot2)
        r.check(rises and rises[0] - boot2 >= 60 * SEC, f"W after reboot {rises}")
    return check


@scenario("boot_loop_enters_safe_mode", until=50 * MIN, max_boots=7)
def boot_loop(s):
    """Power-on + 4 STWD100 resets = 5 counted boots: the 5th stays up in SAFE MODE (outputs off,
    no STWD100), retries a normal boot after 30 min and heats again."""
    heating(s)
    s.set(0, "hang.boots", 4)
    s.set(0, "hang.after", 20 * SEC)
    def check(r):
        r.check(len(r.resets) >= 5, f"resets {r.resets}")
        t = r.expect(r"EVT safe mode: 5 resets")
        r.check(r.high_ms(W, t, t + 29 * MIN) == 0, "no heat in safe mode")
        r.never(r"arming STWD100", after=t, before=t + 29 * MIN)
        r.expect(r"EVT safe mode: retrying a normal boot", after=t + 29 * MIN)
        heats(r, 50 * MIN - 1, msg="heats after leaving safe mode")
    return check


# =============================================================================
# 2. SHT40
# =============================================================================

def sensor_fault(name, mode, doc, extra=None, recover=True):
    @scenario("sht40_" + name, until=20 * MIN, doc=doc)
    def fn(s):
        heating(s)
        s.fan(5 * SEC, True)                    # failsafe must stop even the Fan ON blower
        if extra:
            extra(s)
        s.set(3 * MIN, "sht.mode", mode)
        if recover:
            s.set(8 * MIN, "sht.mode", 0)
        def check(r):
            r.check(r.pin(W, 3 * MIN - 1) == 1 and r.pin(G, 3 * MIN - 1) == 1, "heating with fan before the fault")
            t = r.expect(r"EVT ctrl fault=SENSOR FAIL", after=3 * MIN, before=3 * MIN + 40 * SEC)
            r.check(r.high_ms(W, t + 1, 8 * MIN) + r.high_ms(G, t + 1, 8 * MIN) + r.high_ms(Y, t + 1, 8 * MIN) == 0,
                    "failsafe: everything off, including Fan ON")
            r.expect(r"EVT link 2", msg="Zigbee unaffected")
            if recover:
                ok = r.expect(r"EVT ctrl fault=OK", after=8 * MIN)
                r.check(ok - 8 * MIN >= 2 * 2000, "recovery only after 3 good readings")
                heats(r, 20 * MIN - 1, msg="heating resumes")
        return check
    return fn


sensor_fault("nak", 1, "SHT40 stops ACKing: failsafe (all off incl. Fan ON) within the 15 s timeout; recovers")
sensor_fault("bus_stuck_timeout", 2, "I2C bus stuck (every transfer times out): failsafe, bus resets, recovery")
sensor_fault("crc_error", 3, "every reading fails CRC: failsafe; recovery after 3 good readings")
sensor_fault("intermittent_100pct", 5, "intermittent NAK at 100%", extra=lambda s: s.set(0, "sht.p", 100))


@scenario("sht40_intermittent_30pct_keeps_control", until=30 * MIN, seed=7)
def sht_intermittent(s):
    """30% of transfers NAK (seeded): readings still arrive well within the 15 s timeout, control continues."""
    heating(s)
    s.set(0, "sht.p", 30)
    s.set(1 * MIN, "sht.mode", 5)
    def check(r):
        r.never(r"EVT ctrl fault=SENSOR FAIL", after=1 * MIN)
        r.check(r.count(r"I2C bus reset") > 10, "faults really injected")
        heats(r, 30 * MIN - 1)
    return check


@scenario("sht40_out_of_range", until=15 * MIN)
def sht_range(s):
    """Implausible 90 °C reading: TEMP RANGE fault, all off; normal value -> recovers."""
    heating(s)
    s.temp(3 * MIN, 9000)
    s.temp(8 * MIN, 1800)
    def check(r):
        t = r.expect(r"EVT ctrl fault=TEMP RANGE", after=3 * MIN, before=3 * MIN + 10 * SEC)
        r.check(r.high_ms(W, t + 1, 8 * MIN) == 0, "all off")
        r.expect(r"EVT ctrl fault=OK", after=8 * MIN)
    return check


@scenario("sht40_frozen_value", until=40 * MIN)
def sht_frozen(s):
    """Sensor keeps returning the identical reading: after 20 min it is treated as dead (failsafe)."""
    heating(s, 1980)
    s.set(1 * MIN, "sht.mode", 4)
    s.set(30 * MIN, "sht.mode", 0)
    def check(r):
        t = r.expect(r"EVT fault sensor frozen=1", after=20 * MIN, before=22 * MIN)
        r.expect(r"EVT ctrl fault=SENSOR FAIL", after=t, before=t + 40 * SEC)
        r.expect(r"EVT fault sensor frozen=0", after=30 * MIN)
        r.expect(r"EVT ctrl fault=OK", after=30 * MIN)
    return check


@scenario("sht40_absent_at_boot", until=10 * MIN)
def sht_absent(s):
    """No sensor from power-on: never leaves failsafe, nothing turns on, no reboot."""
    heating(s)
    s.fan(5 * SEC, True)
    s.set(0, "sht.mode", 6)
    def check(r):
        r.never(r"EVT ctrl fault=OK")
        r.check(r.high_ms(W, 0, 10 * MIN) + r.high_ms(G, 0, 10 * MIN) == 0, "all off")
    return check


@scenario("sensor_starting_clears_without_clock", until=20 * SEC)
def sensor_starting_clear(s):
    """No Zigbee clock, sensor healthy. The glass leaves 'Starting sensor...' within a couple of
    seconds of the fault clearing, instead of waiting out the 3 min background slot."""
    s.temp(0, 2100)
    def check(r):
        ok = r.expect(r"EVT ctrl fault=OK", before=12 * SEC)
        r.expect(r"EVT panel Idle", after=ok, before=ok + 3 * SEC)
        r.never(r"EVT panel Starting sensor", after=ok + 3 * SEC, before=20 * SEC)
    return check


@scenario("sensor_starting_absent_paints_fail", until=20 * SEC)
def sensor_starting_absent(s):
    """No clock, sensor never answers. SENSOR FAIL is on the glass just after 10 s, not at the
    next minute and not at the 3 min slot."""
    s.temp(0, 2100)
    s.set(0, "sht.mode", 6)
    def check(r):
        r.expect(r"EVT panel Starting sensor\.\.\.", before=5 * SEC)
        r.never(r"EVT panel SENSOR FAIL", before=10 * SEC)
        r.expect(r"EVT panel SENSOR FAIL", after=10 * SEC, before=13 * SEC)
    return check


@scenario("sensor_fault_after_start_repaints", until=50 * SEC)
def sensor_fault_after_start(s):
    """No clock. A sensor that dies after the starting window paints SENSOR FAIL within a couple
    of seconds of the fault, which is the edge the starting-line check does not see."""
    s.temp(0, 2100)
    s.set(20 * SEC, "sht.mode", 1)
    def check(r):
        r.expect(r"EVT panel Idle", before=15 * SEC)
        fault = r.expect(r"EVT ctrl fault=SENSOR FAIL", after=20 * SEC, before=40 * SEC)
        r.expect(r"EVT panel SENSOR FAIL", after=fault, before=fault + 3 * SEC)
    return check


@scenario("sensor_log_heartbeat_not_every_edge_window", until=70 * SEC)
def sensor_log_heartbeat(s):
    """EVT sensor is the first sample and the minute heartbeat. A healthy sensor does not log
    again when the 10 s edge window expires."""
    s.temp(0, 2100)
    def check(r):
        first = r.expect(r"EVT sensor ", before=2 * SEC)
        r.never(r"EVT sensor ", after=first + 1, before=first + 59 * SEC)
        r.expect(r"EVT sensor ", after=first + 59 * SEC, before=first + 61 * SEC)
    return check


@scenario("sensor_log_keeps_a_suppressed_fail", until=70 * SEC)
def sensor_log_suppressed(s):
    """A failure inside the 10 s quiet window is stored, so it is not logged as a new edge the
    moment that window ends. The next line is still the minute heartbeat."""
    s.temp(0, 2100)
    s.set(3 * SEC, "sht.mode", 1)
    def check(r):
        first = r.expect(r"EVT sensor ", before=2 * SEC)
        r.never(r"EVT sensor ", after=first + 1, before=first + 59 * SEC)
        r.expect(r"EVT sensor ", after=first + 59 * SEC, before=first + 61 * SEC)
    return check


@scenario("sht40_bus_init_fails", until=5 * MIN)
def sht_bus_init(s):
    """I2C driver init fails: failsafe forever, rest of the system runs."""
    heating(s)
    s.set(0, "sht.busfail", 1)
    def check(r):
        r.expect(r"EVT fault sensor init")
        r.never(r"EVT ctrl fault=OK")
        r.expect(r"EVT link 2")
    return check


# =============================================================================
# 3. e-paper
# =============================================================================

def epd_fault(name, key, doc, display_fault_evt):
    @scenario("epd_" + name, until=15 * MIN, doc=doc)
    def fn(s):
        heating(s)
        s.set(30 * SEC if key != "epd.init_fail" else 0, key, 1)
        s.press(2 * MIN, "up", 100)            # a refresh is attempted with the fault present
        def check(r):
            heats(r, 15 * MIN - 1, msg="heating continues without a working screen")
            if display_fault_evt:
                r.expect(r"EVT display fault=1")
            r.never(r"RESET")
        return check
    return fn


epd_fault("busy_stuck", "epd.busy_stuck", "BUSY stuck high: refreshes time out and retry; control unaffected", False)
epd_fault("spi_error", "epd.spi_fail", "every SPI transfer fails: refresh reports the error; control unaffected", False)
epd_fault("init_fail", "epd.init_fail", "SPI bus init fails at boot: no display task, fault reported, control runs", True)
epd_fault("spi_hang", "epd.hang", "SPI transfer never completes: display task stalls -> fault reported, no reboot", True)


@scenario("epd_framebuffer_shows_radio_fault", until=1 * MIN)
def epd_fb_fault(s):
    """Framebuffer assertion: the screen differs between 'connected' and 'radio fault'."""
    s.temp(0, 2100)
    s.zb(0, "fail start")
    def check(r):
        r.expect(r"EVT link 4")
        r.expect(r"EPD refresh .*ink=[1-9]")
    return check


# =============================================================================
# 4. buttons
# =============================================================================

@scenario("button_mode_stuck_at_boot", until=3 * MIN)
def btn_stuck_boot(s):
    """M shorted from power-on for 2 min: no pairing, no factory reset; settings intact."""
    heating(s)
    s.at(0, "btn mode press")
    s.at(2 * MIN, "btn mode release")
    def check(r):
        r.never(r"factory reset|ZB steering attempt [2-9]|NVS erase")
        heats(r, 3 * MIN - 1)
    return check


@scenario("button_up_stuck_runtime", until=5 * MIN)
def btn_up_stuck(s):
    """UP stuck down for 3 min: auto-repeat stops after 30 s, setpoint stays bounded."""
    heating(s, 2100)
    s.at(1 * MIN, "btn up press")
    s.at(4 * MIN, "btn up release")
    def check(r):
        n = r.count(r"EVT setpoint", after=1 * MIN, before=4 * MIN)
        r.check(n < 160, f"{n} setpoint changes while stuck")
        r.check(r.count(r"EVT setpoint", after=1 * MIN + 31 * SEC, before=4 * MIN) == 0, "no changes after 30 s")
    return check


@scenario("button_bounce_single_step", until=1 * MIN, seed=3)
def btn_bounce(s):
    """Contact bounce for 25 ms on press: exactly one setpoint step."""
    heating(s, 2100)
    s.at(20 * SEC, "btn up press")
    s.at(20 * SEC, "btn up bounce 25")
    s.at(20 * SEC + 150, "btn up release")
    s.at(20 * SEC + 150, "btn up bounce 25")
    def check(r):
        r.check(r.count(r"EVT setpoint", after=20 * SEC, before=25 * SEC) == 1, "one step")
    return check


@scenario("factory_reset_hold_15s", until=2 * MIN, max_boots=2)
def factory_reset(s):
    """Hold M 15 s on the main screen: settings + Zigbee erased, reboot, defaults, re-pairs."""
    heating(s)
    s.press(30 * SEC, "mode", 15500)
    def check(r):
        r.expect(r"NVS erase_all tstat", after=45 * SEC)
        r.expect(r"ZB factory reset", after=45 * SEC)
        r.check(r.boots == 2, "rebooted")
        r.expect(r"EVT cfg loaded mode=0", after=r.boot_times[1])
    return check


@scenario("factory_reset_release_at_14_9s_cancels", until=1 * MIN)
def factory_cancel(s):
    """Release M at 14.9 s: warning shown, nothing erased."""
    heating(s)
    s.press(20 * SEC, "mode", 14900)
    def check(r):
        r.never(r"NVS erase|factory reset")
    return check


@scenario("pairing_window_5_to_8s", until=1 * MIN)
def pairing_window(s):
    """Coordinator down so we are not joined: release at 4.9 s does nothing, at 6 s starts pairing."""
    s.temp(0, 2100)
    s.zb(0, "coord down")
    s.press(20 * SEC, "mode", 4900)
    s.press(35 * SEC, "mode", 6000)
    def check(r):
        n1 = r.count(r"ZB steering attempt", after=20 * SEC, before=25 * SEC + 900)
        n2 = r.count(r"ZB steering attempt", after=41 * SEC, before=41 * SEC + 500)
        r.check(n1 == 0, "4.9 s hold: no pairing")
        r.check(n2 == 1, "6 s hold: pairing started at release")
    return check


# =============================================================================
# 5. NVS / flash
# =============================================================================

def nvs_boot(name, doc, setup, expect_mode=OFF, extra=None):
    @scenario("nvs_" + name, until=2 * MIN, doc=doc)
    def fn(s):
        s.temp(0, 1800)
        setup(s)
        def check(r):
            r.expect(rf"EVT cfg loaded mode={expect_mode}")
            r.never(r"RESET")
            if extra:
                extra(r)
        return check
    return fn


nvs_boot("empty", "blank NVS: defaults", lambda s: None)
nvs_boot("corrupt_blob", "garbage cfg3 blob: rejected, defaults", lambda s: s.at(0, "nvs tstat cfg3 deadbeefcafebabe00ff"),
         extra=lambda r: r.expect(r"unknown settings blob"))
nvs_boot("wrong_size_blob", "truncated cfg3 (version 1, 5 bytes): rejected, defaults", lambda s: s.at(0, "nvs tstat cfg3 0104000000"),
         extra=lambda r: r.expect(r"unknown settings blob"))
nvs_boot("empty_blob", "zero-length cfg3: rejected without reading past the end", lambda s: s.at(0, "nvs tstat cfg3 -"),
         extra=lambda r: r.expect(r"unknown settings blob \(0 bytes"))
nvs_boot("old_v021_cfg_key", "0.2.1 prototype 'cfg' key present: ignored (new key cfg3), defaults",
         lambda s: s.at(0, "nvs tstat cfg 0204d007920905000000"))
nvs_boot("read_fail", "NVS reads fail: defaults, keeps running", lambda s: s.set(0, "nvs.read_fail", 1))
nvs_boot("open_fail_at_boot", "nvs_open fails at boot: defaults, keeps running", lambda s: s.set(0, "nvs.open_fail", 1))
nvs_boot("init_generic_fail", "nvs_flash_init fails (other error): defaults in RAM, storage fault, no reboot",
         lambda s: s.set(0, "nvs.init", 4), extra=lambda r: r.expect(r"EVT fault nvs init"))
nvs_boot("init_no_free_pages", "NVS has no free pages: erased and re-initialised", lambda s: s.set(0, "nvs.init", 1),
         extra=lambda r: r.expect(r"NVS erased"))
nvs_boot("init_new_version", "NVS written by a newer IDF: erased and re-initialised", lambda s: s.set(0, "nvs.init", 2),
         extra=lambda r: r.expect(r"NVS erased"))
nvs_boot("partition_missing", "NVS partition missing: defaults in RAM, storage fault shown, no reboot loop",
         lambda s: s.set(0, "nvs.init", 3), extra=lambda r: r.expect(r"EVT fault nvs init"))


@scenario("nvs_roundtrip_settings_survive_reboot", until=3 * MIN, max_boots=2)
def nvs_roundtrip(s):
    """Settings written over Zigbee are saved and restored after a power cycle."""
    heating(s)
    s.heat_sp(10 * SEC, 2150)
    s.at(1 * MIN, "power cycle")
    def check(r):
        r.expect(r"EVT cfg loaded mode=4 heat=2150", after=r.boot_times[1])
    return check


def nvs_write_fault(name, key, doc):
    @scenario("nvs_" + name, until=4 * MIN, doc=doc)
    def fn(s):
        heating(s)
        s.set(20 * SEC, key, 1)
        s.heat_sp(30 * SEC, 2200)
        s.set(3 * MIN, key, 0)
        def check(r):
            r.expect(r"EVT fault nvs write", after=30 * SEC)
            r.check(r.setpoint(2 * MIN)["heat"] == 2200, "new setpoint used from RAM")
            r.expect(r"NVS set tstat/cfg3", after=3 * MIN, msg="saved once the flash works again")
            r.check(r.count(r"EVT fault nvs write", after=30 * SEC, before=3 * MIN) <= 4, "retries once a minute, not every loop")
        return check
    return fn


nvs_write_fault("write_fail", "nvs.write_fail", "NVS writes fail: control uses the new value, save retried every minute")
nvs_write_fault("full", "nvs.full", "NVS full: same as write failure")
nvs_write_fault("commit_fail", "nvs.commit_fail", "nvs_commit fails: same as write failure")
nvs_write_fault("open_fail_runtime", "nvs.open_fail", "nvs_open fails at runtime: same as write failure")


# =============================================================================
# 6. OTA
# =============================================================================

@scenario("ota_pending_never_joins_rolls_back", until=15 * MIN, max_boots=2)
def ota_rollback(s):
    """New image, coordinator gone: not confirmed, rolled back at 10 min (outputs off first)."""
    heating(s)
    s.at(0, "ota pending other_valid")
    s.zb(0, "coord down")
    s.zb(0, "joined 1")
    def check(r):
        t = r.expect(r"rolling back")
        r.check(9 * MIN < t < 11 * MIN, f"rollback at {t}")
        r.check(r.resets and r.resets[0][1] == "OTA_ROLLBACK", f"{r.resets}")
        r.expect(r"BOOTLOADER running slot 1", after=t)
    return check


@scenario("ota_pending_joins_confirms", until=5 * MIN)
def ota_confirm(s):
    """New image that rejoins and is healthy for 60 s: confirmed, no rollback."""
    heating(s)
    s.at(0, "ota pending other_valid")
    s.zb(0, "joined 1")
    def check(r):
        t = r.expect(r"OTA mark valid slot 0")
        r.check(t >= 60 * SEC, "only after 60 s healthy")
        r.never(r"rolling back")
    return check


@scenario("ota_rollback_impossible_keeps_running", until=15 * MIN)
def ota_no_rollback(s):
    """Never joins, but the other slot is invalid: keeps this image, marks it valid, no reboot loop."""
    heating(s)
    s.at(0, "ota pending other_invalid")
    s.zb(0, "coord down")
    s.zb(0, "joined 1")
    def check(r):
        r.expect(r"rollback failed")
        r.expect(r"OTA mark valid")
        r.never(r"RESET")
        heats(r, 15 * MIN - 1)
    return check


@scenario("ota_full_update_and_confirm", until=10 * MIN, max_boots=2)
def ota_full(s):
    """Complete OTA over Zigbee: outputs off, reboot into slot 1, rejoin, confirm."""
    heating(s)
    s.zb(2 * MIN, "ota 8000")
    def check(r):
        r.expect(r"OTA end ok")
        r.check(r.resets and r.resets[0][1] == "SW", f"{r.resets}")
        r.check(r.pin(W, r.resets[0][0]) == 0, "outputs off before the OTA reboot")
        r.expect(r"BOOTLOADER running slot 1 state 1", msg="new image pending verify")
        r.expect(r"OTA mark valid slot 1")
    return check


def ota_fault(name, doc, setup, cmd="ota 8000", expect=None):
    @scenario("ota_" + name, until=8 * MIN, doc=doc)
    def fn(s):
        heating(s)
        setup(s)
        s.zb(2 * MIN, cmd)
        def check(r):
            if expect:
                r.expect(expect, after=2 * MIN)
            r.never(r"RESET")
            r.check(r.find(r"OTA done") is None, "no reboot into a bad image")
            heats(r, 8 * MIN - 1, msg="control continues")
        return check
    return fn


ota_fault("write_error", "flash write fails mid-image: transfer aborted, banner cleared, no reboot",
          lambda s: s.set(0, "ota.write_fail_after", 3000), expect=r"OTA write failed")
ota_fault("image_invalid", "image is not a valid app: esp_ota_end rejects it, boot partition unchanged",
          lambda s: None, cmd="ota 8000 bad=1", expect=r"ota end: ")
ota_fault("wrong_element_tag", "OTA file with an unknown element tag: rejected at the first block",
          lambda s: None, cmd="ota 8000 tag=1", expect=r"unsupported tag")
ota_fault("no_partition", "no OTA partition: start refused", lambda s: s.set(0, "ota.nopart", 1), expect=r"no OTA partition")
ota_fault("begin_fails", "esp_ota_begin fails: transfer refused", lambda s: s.set(0, "ota.begin_fail", 1), expect=r"client rejected|refused")
ota_fault("set_boot_fails", "setting the boot partition fails: stays on the old image", lambda s: s.set(0, "ota.setboot_fail", 1),
          expect=r"set boot: ")
ota_fault("server_abort_mid_transfer", "server aborts at block 10: handle released, banner cleared",
          lambda s: None, cmd="ota 8000 abort_at=10", expect=r"OTA aborted")
ota_fault("unavailable", "no image on the server: nothing happens", lambda s: None, cmd="ota none", expect=r"NO_IMAGE_AVAILABLE")


@scenario("ota_server_stalls_banner_cleared", until=12 * MIN)
def ota_stall(s):
    """Server stops sending mid-transfer: after 5 min without progress the 'Updating' banner is cleared."""
    heating(s)
    s.zb(2 * MIN, "ota 8000 stall_at=5")
    def check(r):
        r.expect(r"EVT ota stalled", after=7 * MIN, before=8 * MIN)
        heats(r, 12 * MIN - 1)
    return check


@scenario("ota_link_lost_mid_transfer", until=8 * MIN)
def ota_lost(s):
    """Parent lost during the transfer: aborted cleanly; rejoins."""
    heating(s)
    s.zb(2 * MIN, "ota 20000")
    s.zb(2 * MIN + 3 * SEC, "lost")
    def check(r):
        r.expect(r"OTA aborted", after=2 * MIN)
        r.never(r"RESET")
        r.expect(r"EVT link 2", after=2 * MIN + 4 * SEC)
    return check


# =============================================================================
# 7. time / clock
# =============================================================================

@scenario("time_never_synced", until=3 * HOUR)
def time_never(s):
    """Coordinator never answers Time reads: DAY program, retries 1, 2, 5 min then hourly."""
    heating(s)
    def check(r):
        r.never(r"EVT time known=1")
        r.check(r.setpoint()["sched"] == 0 and r.setpoint()["heat"] == 2000, "day program")
        reads = [t for t, x in r.find_all(r"ZB read 0x000a")]
        gaps = [b - a for a, b in zip(reads, reads[1:])]
        r.check(gaps[:3] == [60 * SEC, 120 * SEC, 300 * SEC] and all(g == HOUR for g in gaps[3:]), f"backoff {gaps}")
    return check


def bad_time(name, extra, doc):
    @scenario("time_" + name, until=10 * MIN, doc=doc)
    def fn(s):
        heating(s)
        s.clock(0, 12, 0, extra)
        def check(r):
            r.expect(r"coordinator returned no usable time")
            r.never(r"EVT time known=1")
        return check
    return fn


bad_time("status_unset", "status=0", "TimeStatus says neither Master nor Synchronized: ignored")


@scenario("time_bogus_epoch", until=10 * MIN)
def time_bogus(s):
    """Coordinator clock at 2000-01-01 01:00 (never set): ignored."""
    heating(s)
    s.zb(0, "time 3600")
    def check(r):
        r.expect(r"coordinator returned no usable time")
        r.never(r"EVT time known=1")
    return check


@scenario("time_jump_back_across_boost_window", until=4 * HOUR)
def time_jump(s):
    """Synced at 07:00 (boost window), coordinator clock jumps back to 05:00 at the hourly resync:
    program follows the new clock (night, 18 °C), then boost again at 06:00."""
    heating(s, 1950)
    s.clock(0, 7, 0)
    s.clock(30 * MIN, 4, 30)      # takes effect at the next hourly read (~1 h): 05:01
    def check(r):
        r.check(r.setpoint(20 * MIN)["sched"] == 3, "boost at 07:20")
        t = r.expect(r"time sync: local 05:", after=30 * MIN)
        r.check(r.setpoint(t + 5 * SEC)["sched"] == 2, "night after the jump back")
        r.expect(r"EVT setpoint mode=4 heat=2050 .*sched=3", after=t + 1)
    return check


@scenario("time_dst_shift", until=3 * HOUR)
def time_dst(s):
    """DST starts (+1 h DstShift) between hourly syncs: local time moves forward by 1 h."""
    heating(s)
    s.clock(0, 3, 0)
    s.at(30 * MIN, f"zb time {s.DAY0 + 3 * 3600 - 0} dst=3600")
    def check(r):
        r.expect(r"time sync: local 03:00")
        r.expect(r"time sync: local 05:", after=30 * MIN)
    return check


@scenario("time_sync_lost_then_restored", until=4 * HOUR)
def time_lost(s):
    """Coordinator stops answering Time reads: time stays known (extrapolated); later syncs resume."""
    heating(s)
    s.clock(0, 10, 0)
    s.zb(10 * MIN, "time off")
    s.clock(2 * HOUR + 30 * MIN, 12, 30)
    def check(r):
        r.never(r"EVT time known=0", after=1 * MIN)
        r.expect(r"time sync: local 1[23]:", after=2 * HOUR + 30 * MIN)
    return check


@scenario("schedule_night_wraps_midnight", until=3 * HOUR)
def sched_wrap(s):
    """23:30 -> 02:30: the night program (22:00-06:00) holds across midnight."""
    heating(s)
    s.clock(0, 23, 30)
    def check(r):
        for t in (20 * MIN, 1 * HOUR, 2 * HOUR + 50 * MIN):
            r.check(r.setpoint(t)["sched"] == 2, f"night at +{t // MIN} min")
    return check


# =============================================================================
# 8. Zigbee radio
# =============================================================================

def radio_init_fault(name, fail, doc):
    @scenario("radio_" + name, until=5 * MIN, doc=doc)
    def fn(s):
        ui_heat(s)
        s.zb(0, f"fail {fail}")
        def check(r):
            r.expect(r"EVT zb radio fault")
            r.expect(r"EVT link 4")
            r.never(r"RESET")
            heats(r, 5 * MIN - 1, msg="local control runs without Zigbee")
        return check
    return fn


radio_init_fault("platform_config_fails", "platform", "esp_zb_platform_config fails: radio fault shown, no reboot")
radio_init_fault("device_register_fails", "register", "esp_zb_device_register fails: radio fault shown, no reboot")
radio_init_fault("stack_start_fails", "start", "esp_zb_start fails: radio fault shown, no reboot")


@scenario("radio_zigbee_task_create_fails", until=3 * MIN)
def radio_task(s):
    """Not enough memory for the Zigbee task: radio fault, control runs."""
    ui_heat(s)
    s.set(0, "fault.task.zigbee", 1)
    def check(r):
        r.expect(r"EVT link 4")
        heats(r, 3 * MIN - 1)
    return check


@scenario("radio_stack_init_signal_fails_then_ok", until=5 * MIN)
def radio_init_signal(s):
    """BDB initialisation reports failure 3 times: retried with backoff (10, 30, 60 s), then joins."""
    s.temp(0, 2100)
    s.zb(0, "fail init 3")
    def check(r):
        r.check(r.count(r"stack init failed") == 3, "3 failures")
        r.expect(r"EVT link 2", after=100 * SEC)
    return check


@scenario("radio_join_never_succeeds", until=2 * HOUR)
def join_never(s):
    """Coordinator never accepts: steering retried with backoff (not every 10 s); pairing shown; heats."""
    heating(s)
    s.zb(0, "coord down")
    def check(r):
        n = r.count(r"ZB steering attempt")
        r.check(4 <= n <= 14, f"{n} steering attempts in 2 h (fixed 10 s retry would be ~550)")
        r.expect(r"EVT link 1")
        r.never(r"EVT link 2")
        heats(r, 2 * HOUR - 1)
    return check


@scenario("radio_link_lost_mid_run_rejoins", until=10 * MIN)
def link_lost(s):
    """Parent lost while running: 'lost parent' shown, rejoin after 10 s backoff, back to connected."""
    heating(s)
    s.zb(3 * MIN, "lost")
    def check(r):
        t = r.expect(r"EVT link 3", after=3 * MIN)
        a = r.expect(r"ZB steering attempt", after=t)
        r.check(9 * SEC <= a - 3 * MIN <= 11 * SEC, f"rejoin attempt after {a - 3 * MIN} ms")
        r.expect(r"EVT link 2", after=a)
        heats(r, 10 * MIN - 1)
    return check


@scenario("radio_link_lost_coordinator_down", until=1 * HOUR)
def link_lost_down(s):
    """Parent lost and the network is gone: keeps 'lost parent', retries with backoff, control continues."""
    heating(s)
    s.zb(3 * MIN, "coord down")
    s.zb(3 * MIN, "lost")
    s.zb(40 * MIN, "coord up")
    def check(r):
        n = r.count(r"ZB steering attempt", after=3 * MIN, before=40 * MIN)
        r.check(3 <= n <= 10, f"{n} attempts in 37 min")
        r.expect(r"EVT link 2", after=40 * MIN, before=56 * MIN)
        heats(r, 1 * HOUR - 1)
    return check


@scenario("radio_leave_then_repair", until=10 * MIN)
def leave(s):
    """Coordinator removes the device (leave): offline, back to pairing with backoff, re-joins."""
    heating(s)
    s.zb(3 * MIN, "leave")
    def check(r):
        r.expect(r"EVT link 0", after=3 * MIN)
        r.expect(r"EVT link 2", after=3 * MIN + 10 * SEC)
    return check


@scenario("radio_flapping_link_backs_off", until=2 * HOUR)
def flapping(s):
    """Link drops 30 s after every rejoin for 2 h: rejoin attempts back off instead of hammering."""
    heating(s)
    for k in range(0, 240):
        s.zb(5 * MIN + k * 30 * SEC, "lost")
    def check(r):
        n = r.count(r"ZB steering attempt", after=5 * MIN)
        r.check(n <= 16, f"{n} rejoin attempts in ~2 h of flapping")
        heats(r, 2 * HOUR - 1)
    return check


@scenario("radio_zigbee_task_stalls", until=10 * MIN)
def zb_stall(s):
    """Zigbee task stalls holding the stack lock: radio fault after 60 s, control and UI keep working,
    pairing from the UI does not freeze the main loop."""
    heating(s)
    s.zb(3 * MIN, "stall")
    s.press(6 * MIN, "mode", 6000)
    def check(r):
        t = r.expect(r"EVT link 4", after=3 * MIN)
        r.check(t - 3 * MIN <= 70 * SEC, f"stall detected after {t - 3 * MIN} ms")
        r.never(r"RESET")
        heats(r, 10 * MIN - 1)
    return check


@scenario("radio_zigbee_task_dies", until=10 * MIN)
def zb_dies(s):
    """Zigbee task exits: radio fault after 60 s, control continues, no reboot."""
    heating(s)
    s.zb(3 * MIN, "die")
    def check(r):
        r.expect(r"EVT link 4", after=3 * MIN)
        r.never(r"RESET")
        heats(r, 10 * MIN - 1)
    return check


@scenario("radio_reboot_with_network_down", until=30 * MIN, max_boots=2)
def reboot_net_down(s):
    """Joined device reboots while the coordinator is off: lost parent, backoff retries, rejoins later."""
    heating(s)
    s.zb(2 * MIN, "coord down")
    s.at(2 * MIN, "power cycle")
    s.zb(15 * MIN, "coord up")
    def check(r):
        r.expect(r"EVT link 3", after=r.boot_times[1])
        r.expect(r"EVT link 2", after=15 * MIN)
        heats(r, 30 * MIN - 1)
    return check


@scenario("radio_remote_writes_while_lost", until=5 * MIN)
def writes_lost(s):
    """Reports are not sent while the link is down (no errors/crash); local changes still apply."""
    heating(s, 2100)
    s.zb(1 * MIN, "coord down")
    s.zb(1 * MIN, "lost")
    s.press(2 * MIN, "up", 100)
    def check(r):
        r.check(r.setpoint(3 * MIN)["heat"] > 2000, "local setpoint change applied")
    return check


# =============================================================================
# 9. outputs (GPIO)
# =============================================================================

@scenario("output_w_stuck_high", until=10 * MIN, stuck_ok=(W,))
def out_w_stuck(s):
    """W pad stuck high (driver short) while idle: detected by readback, OUTPUT FAULT latched,
    Y and G held low, no reboot."""
    cooling(s, 2700)
    s.set(1 * MIN, "gpio.stuck.0", 1)
    def check(r):
        r.expect(r"EVT output fault", after=1 * MIN, before=1 * MIN + 1 * SEC)
        r.check(r.high_ms(Y, 1 * MIN, 10 * MIN) == 0, "compressor never starts with W stuck on")
        r.never(r"RESET")
    return check


@scenario("output_y_stuck_low_while_cooling", until=15 * MIN, stuck_ok=(Y,))
def out_y_low(s):
    """Y pad stuck low: when cooling is commanded the readback mismatch latches OUTPUT FAULT, all off."""
    cooling(s, 2700)
    s.set(0, "gpio.stuck.1", 0)
    def check(r):
        t = r.expect(r"EVT output fault", after=5 * MIN)
        r.check(r.high_ms(G, t + 1, 15 * MIN) == 0, "G off as well")
    return check


@scenario("output_g_set_fails", until=15 * MIN, stuck_ok=(G,))
def out_g_setfail(s):
    """gpio_set_level fails on G while cooling is about to start: fault latched, Y never runs without G."""
    cooling(s, 2700)
    s.set(1 * MIN, "gpio.setfail.2", 1)
    def check(r):
        r.expect(r"EVT output fault")
        r.check(r.high_ms(Y, 0, 15 * MIN) == 0, "no compressor without the blower")
    return check


@scenario("output_w_config_fails_at_boot", until=5 * MIN, stuck_ok=(W,))
def out_cfg(s):
    """gpio_config fails for W at boot: no ESP_ERROR_CHECK reboot loop, OUTPUT FAULT shown, all off."""
    heating(s)
    s.set(0, "gpio.cfgfail.0", 1)
    def check(r):
        r.expect(r"EVT output fault: config pin 0")
        r.never(r"RESET")
        r.check(r.high_ms(Y, 0, 5 * MIN) + r.high_ms(G, 0, 5 * MIN) == 0, "all off")
    return check


# =============================================================================
# 10. readback debounce / re-test (review item 1)
# =============================================================================

@scenario("output_readback_single_glitch_tolerated", until=10 * MIN)
def out_glitch1(s):
    """One bad readback of W while heating (stale/noisy read): no fault, heating not interrupted."""
    heating(s)
    s.set(3 * MIN, "gpio.glitch.0", 1)
    def check(r):
        r.expect(r"GLITCH read pin 0", msg="glitch really injected")
        r.never(r"EVT output fault|EVT fault output")
        r.check(r.high_ms(W, 2 * MIN, 4 * MIN) == 2 * MIN, "W stays on through the glitch")
    return check


@scenario("output_readback_stale_read_model", until=5 * MIN)
def out_stale(s):
    """The fake returns the pre-write level for reads issued in the same instant as the write
    (C6 input synchroniser). Readback is done on the next pass, so normal switching never faults."""
    heating(s)
    s.fan(4 * MIN, True)
    def check(r):
        r.check(len(r.rises(W)) >= 1 and len(r.rises(G)) >= 1, "outputs switched")
        r.never(r"EVT output fault|EVT fault output")
    return check


@scenario("output_two_mismatches_latch_then_retest_recovers", until=40 * MIN)
def out_retest(s):
    """Two consecutive bad G readbacks while cooling: fault latched, all off; after 10 min the
    re-test passes and cooling resumes (monitor checks compressor min-off and min-on)."""
    cooling(s)
    s.set(8 * MIN, "gpio.glitch.2", 2)
    def check(r):
        t = r.expect(r"EVT output fault: readback", after=8 * MIN, before=8 * MIN + 1 * SEC)
        r.check(r.pin(Y, t + 1) == 0 and r.pin(G, t + 1) == 0, "all off at the latch")
        rt = r.expect(r"EVT output retest 1/3", after=t)
        r.check(10 * MIN <= rt - t <= 10 * MIN + 1 * SEC, f"re-test after {rt - t} ms")
        r.expect(r"EVT fault output cleared", after=rt)
        y = r.rises(Y, after=rt)
        r.check(y and y[0] - rt < 1 * SEC, "cooling resumes at the re-test (min-off already met)")
    return check


@scenario("output_y_stuck_low_retest_gives_up", until=75 * MIN, stuck_ok=(Y,))
def out_giveup(s):
    """Y pad stuck low (driver dead): each re-test re-latches; after 3 re-tests it stays off."""
    cooling(s)
    s.set(0, "gpio.stuck.1", 0)
    def check(r):
        r.check(r.count(r"EVT output retest") == 3, f"{r.count(r'EVT output retest')} re-tests")
        last = r.find_all(r"EVT output fault: readback")[-1][0]
        r.check(r.high_ms(G, last + 1, 75 * MIN) == 0, "stays off after giving up")
        r.never(r"RESET")
    return check


@scenario("output_w_stuck_high_no_retest", until=30 * MIN, stuck_ok=(W,))
def out_stuck_hi(s):
    """W pad stuck high: the pads never agree with 'all off', so no re-test re-enables anything."""
    cooling(s)
    s.set(1 * MIN, "gpio.stuck.0", 1)
    def check(r):
        r.never(r"EVT output retest")
        r.check(r.high_ms(Y, 0, 30 * MIN) == 0, "no compressor")
    return check


@scenario("output_w_stuck_low_transient_recovers", until=30 * MIN, stuck_ok=(W,))
def out_stuck_transient(s):
    """W pad held low for 2 min while heating (transient) then released: re-test clears the fault."""
    heating(s)
    s.set(3 * MIN, "gpio.stuck.0", 0)
    s.set(5 * MIN, "gpio.stuck.0", -1)
    def check(r):
        rt = r.expect(r"EVT output retest 1/3")
        r.expect(r"EVT fault output cleared", after=rt)
        heats(r, 30 * MIN - 1, msg="heating resumes")
    return check


# =============================================================================
# 11. boot-loop counting (review items 2 and 3)
# =============================================================================

def counts(r):
    return [int(x.split("count=")[1]) for t, x in r.find_all(r"EVT boot reason=")]


@scenario("bootloop_sw_resets_not_counted", until=3 * MIN, max_boots=7)
def sw_not_counted(s):
    """Six factory resets in a row (ESP_RST_SW each): counter never moves, no safe mode."""
    heating(s)
    for k in range(6):
        s.press(20 * SEC + k * 25 * SEC, "mode", 15500)
    def check(r):
        r.check(r.boots == 7, f"{r.boots} boots")
        r.check(counts(r) == [1] * 7, f"counts {counts(r)}")
        r.never(r"EVT safe mode")
    return check


@scenario("bootloop_ota_reboots_not_counted", until=12 * MIN, max_boots=6)
def ota_not_counted(s):
    """Five OTA updates in quick succession: software reboots, never safe mode."""
    heating(s)
    for k in range(5):
        s.zb(30 * SEC + k * 2 * MIN, "ota 3000")
    def check(r):
        r.check(r.boots == 6, f"{r.boots} boots")
        r.check(max(counts(r)) == 1, f"counts {counts(r)}")
        r.never(r"EVT safe mode")
    return check


@scenario("bootloop_power_cycles_counted", until=10 * MIN, max_boots=5)
def power_counted(s):
    """Power-on resets count (an STWD100 reset looks like one): 4 quick power cycles -> 5th boot is safe mode."""
    heating(s)
    for k in range(4):
        s.at(30 * SEC + k * 30 * SEC, "power cycle")
    def check(r):
        r.check(counts(r) == [1, 2, 3, 4, 5], f"counts {counts(r)}")
        r.expect(r"EVT safe mode: 5 resets", after=r.boot_times[4])
        r.check(r.high_ms(W, r.boot_times[4], 10 * MIN) == 0, "outputs off in safe mode")
    return check


@scenario("bootloop_counter_clears_after_3_min", until=12 * MIN, max_boots=8)
def counter_clears(s):
    """3 quick resets, then 3+ min of stable uptime clears the counter; 4 more resets stay below the limit."""
    heating(s)
    for k in range(3):
        s.at(20 * SEC + k * 20 * SEC, "power cycle")
    for k in range(4):
        s.at(5 * MIN + k * 20 * SEC, "power cycle")
    def check(r):
        r.check(counts(r) == [1, 2, 3, 4, 1, 2, 3, 4], f"counts {counts(r)}")
        r.never(r"EVT safe mode")
        r.expect(r"NVS set boot/n", after=60 * SEC + 3 * MIN, before=60 * SEC + 3 * MIN + 2 * SEC)
    return check


@scenario("bootloop_safe_mode_single_trial_boot", until=60 * MIN, max_boots=8)
def single_trial(s):
    """Boot loop -> safe mode -> 30 min -> one trial boot (count = LIMIT-1). The trial hangs once
    (STWD100 reset): straight back to safe mode on the next boot, not 5 more tries."""
    heating(s)
    s.set(0, "hang.boots", 4)
    s.set(0, "hang.after", 20 * SEC)
    s.set(31 * MIN, "hang.boots", 1)      # read by the trial boot
    def check(r):
        sm = [t for t, x in r.find_all(r"EVT safe mode: 5 resets")]
        r.check(len(sm) == 2, f"safe mode entered {len(sm)} times")
        r.check(r.boots == 7, f"{r.boots} boots (4 loop + safe + trial + safe)")
        r.check(counts(r)[5] == BOOT_LIMIT - 1, f"trial boot count {counts(r)}")
        r.check(r.resets[5][1] == "STWD100", f"trial ended by the STWD100 ({r.resets})")
    return check


@scenario("bootloop_count_survives_factory_reset", until=5 * MIN, max_boots=7)
def survives_factory(s):
    """The safe-mode counter is not in the settings namespace: a factory reset does not clear it."""
    heating(s)
    for k in range(3):
        s.at(20 * SEC + k * 20 * SEC, "power cycle")     # counts 1..4
    s.press(100 * SEC, "mode", 15500)                     # factory reset: SW, count stays 4
    s.at(150 * SEC, "power cycle")                        # -> 5: safe mode
    def check(r):
        r.check(counts(r) == [1, 2, 3, 4, 4, 5], f"counts {counts(r)}")
        r.expect(r"EVT safe mode: 5 resets")
    return check


BOOT_LIMIT = 5


@scenario("radio_reboot_network_down_reboot_fail_variant", until=30 * MIN, max_boots=2)
def reboot_fail_variant(s):
    """Same as radio_reboot_with_network_down, but the stack reports the failed rejoin as
    DEVICE_REBOOT(ESP_FAIL) (the other signal ordering seen on ZBOSS): lost parent, backoff, rejoins."""
    heating(s)
    s.set(0, "zb.reboot_fail", 1)
    s.zb(2 * MIN, "coord down")
    s.at(2 * MIN, "power cycle")
    s.zb(15 * MIN, "coord up")
    def check(r):
        b = r.boot_times[1]
        r.expect(r"stack init failed", after=b)
        r.expect(r"EVT link 3", after=b)
        r.check(r.count(r"stack init failed", after=b, before=15 * MIN) <= 8, "backoff, not a tight retry loop")
        r.expect(r"EVT link 2", after=15 * MIN)
        heats(r, 30 * MIN - 1)
    return check
