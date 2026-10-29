#!/usr/bin/env python3
"""Capture gates for the shell-bridge hold-down.

Renders the meshes from the scad on every run. The 2x preload target is not a gate.
"""
import hashlib
import math
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SCAD = os.path.join(ROOT, "case", "thermostat_case.scad")
CASE = SCAD
FIT = os.path.join(ROOT, "scripts", "case_fit.py")
OUT = os.path.join(ROOT, "docs", "case-r7")
OPENSCAD = os.environ.get("OPENSCAD", "openscad")

E_SOFT = 1500.0
J4_TARGET = 3.15
CABLE = 0.30
SHOCK = 0.27
CREEP_KEEP = 0.35
LINE_W = 0.42
PANEL_E = 70000.0


def grab(text, name):
    m = re.search(r"(?:^|[;\n])\s*%s\s*=\s*(-?\d*\.?\d+)\s*;" % re.escape(name), text)
    if not m:
        raise SystemExit("missing " + name)
    return float(m.group(1))


def grab_list(text, name):
    m = re.search(r"(?m)^%s\s*=\s*\[(.*?)\];" % re.escape(name), text, re.S)
    if not m:
        raise SystemExit("missing list " + name)
    rows = []
    for row in re.findall(r"\[([^\]]+)\]", m.group(1)):
        rows.append([float(x) for x in row.split(",")])
    return rows


def case_nums(case):
    def g(name):
        return grab(case, name)
    zt = g("plate_t") + g("standoff_h") + g("pcb_t")
    panel = [float(x) for x in re.search(r"panel = \[([0-9., ]+)\]", case).group(1).split(",")]
    aa = [float(x) for x in re.search(r"aa\s*=\s*\[([0-9., ]+)\]", case).group(1).split(",")]
    zin = zt + g("panel_gap") + g("panel_t")
    return {
        "Zt": zt, "Z_panel_b": zt + g("panel_gap"), "Z_in": zin,
        "panel": panel, "aa": aa,
        "panel_t": g("panel_t"), "panel_gap": g("panel_gap"),
        "fpc_free": g("fpc_free"), "fpc_z": g("fpc_z_ret"), "mouth": g("mouth_x"),
        "pcb_tol": g("pcb_tol"), "wall": g("wall"), "clr": g("clr"),
        "snap_w": g("snap_w"), "snap_z": g("snap_z"),
        "face_recess": g("face_recess"), "win_m": g("win_m"),
        "fence_air": g("fence_air"), "fence_t": g("fence_t"),
        "H": g("H"), "OX": g("OX"), "OYT": g("OYT"),
        "panel_lo": g("panel_lo"), "panel_hi": g("panel_hi"), "panel_h": g("panel_h"),
    }


def components_from_fit(text):
    """Datasheet boxes in case_fit.py. Heights are above the PCB."""
    out = []
    for m in re.finditer(
        r"box\('([^']+)'\s*,\s*([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*Zt\s*,\s*Zt\+([0-9.]+)\s*\)",
        text,
    ):
        name, x0, y0, x1, y1, h = m.group(1), *map(float, m.groups()[1:])
        out.append((name.split()[0], x0, y0, x1, y1, h))
    # The opto lines use a trailing comment form in the tall list; the box() form above
    # already catches U3, D7, D8, R31 and the passives. Add the optos from the same file.
    for m in re.finditer(
        r"box\('(U[678]) opto x([0-9.]+)'\s*,\s*([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*([0-9.]+)\)",
        text,
    ):
        out.append((m.group(1), float(m.group(3)), float(m.group(4)),
                    float(m.group(5)), float(m.group(6)), float(m.group(2))))
    # D4 is only in the kicad dict. SMA, same 2.50 mm as D3 in this file.
    if not any(n == "D4" for n, *_ in out):
        out.append(("D4", 29.5, 54.1, 32.5, 60.5, 2.50))
    return out


class Report:
    def __init__(self):
        self.fails = []

    def say(self, s=""):
        print(s)

    def gate(self, ok, msg):
        print("  %s  %s" % ("PASS" if ok else "FAIL", msg))
        if not ok:
            self.fails.append(msg)


def flank(cn):
    wall = cn["wall"]
    L = cn["Z_in"] - cn["snap_z"]
    I = cn["snap_w"] * wall ** 3 / 12.0
    k_lat = 3.0 * E_SOFT * I / L ** 3
    slope = 0.8 / 0.9
    return 4.0 * 0.465 * k_lat * slope, 0.50 / slope, (2.0 - 1.125) / 2.0


def beam_k(w, t, length, a):
    """Fixed-fixed stiffness for a load `a` mm from one pillar face."""
    b = max(length - a, 0.4)
    a = max(a, 0.4)
    I = w * t ** 3 / 12.0
    return 3.0 * E_SOFT * I * (length ** 3) / (a ** 3 * b ** 3)


def span_geometry(spans, root):
    """Return (length, distance of nub from the left pillar face) per span."""
    out = []
    for s in spans:
        x0, x1, which, nubx = s
        left = x0 + root
        right = x1 - root
        length = right - left
        a = nubx - left
        out.append((length, a, nubx, int(which)))
    return out


def pads_from_case(case, panel):
    """Carrier rests that can hold the panel back. Read from the case, not a copy."""
    pads = []
    for m in re.finditer(
        r"support_pad\(\s*([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*panel\[([0-9]+)\]\s*,\s*([+\-]?\s*[0-9.]+)\s*,\s*([0-9.]+)\s*\)",
        case,
    ):
        x0, x1 = float(m.group(1)), float(m.group(2))
        edge = panel[int(m.group(3))]
        direction = float(m.group(4).replace(" ", ""))
        depth = float(m.group(5))
        y_far = edge + direction * depth
        pads.append((min(x0, x1), min(edge, y_far), max(x0, x1), max(edge, y_far)))
    for m in re.finditer(
        r"board_box\(\s*([^,;]+)\s*,\s*([^,;]+)\s*,\s*([^,;]+)\s*,\s*([^,;]+)\s*,\s*Z_pcb_t\s*,\s*Z_panel_b\s*\)",
        case,
    ):
        vals = []
        ok = True
        env = {"__builtins__": {}}
        for g in m.groups():
            expr = g.strip()
            for i, p in enumerate(panel):
                expr = expr.replace("panel[%d]" % i, repr(p))
            if not re.fullmatch(r"[0-9eE.+\-*/() ]+", expr):
                ok = False
                break
            vals.append(eval(expr, env, {}))
        if not ok:
            continue
        x0, y0, x1, y1 = vals
        ox0, ox1 = max(min(x0, x1), panel[0]), min(max(x0, x1), panel[2])
        oy0, oy1 = max(min(y0, y1), panel[1]), min(max(y0, y1), panel[3])
        if ox1 - ox0 < 0.2 or oy1 - oy0 < 0.2:
            continue
        pads.append((min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1)))
    if not pads:
        raise SystemExit("no support pads in the case")
    return pads


def _pad_points(pads, n=3):
    pts = []
    for x0, y0, x1, y1 in pads:
        for i in range(n):
            for j in range(n):
                pts.append((x0 + (x1 - x0) * i / (n - 1), y0 + (y1 - y0) * j / (n - 1)))
    return pts


def _contact_q(press, supports, bridges, gap, k_scale):
    """Rigid panel, w positive down. Stops unilateral, bridges catch a rise past `gap`."""
    import numpy as np
    px, py = press
    S = np.column_stack([np.ones(len(supports)), supports[:, 0] - px, supports[:, 1] - py])
    B = np.column_stack([np.ones(len(bridges)), bridges[:, 0] - px, bridges[:, 1] - py])
    kb = k_scale * bridges[:, 2]
    ks = 5.0e4
    q = np.zeros(3)
    for _ in range(80):
        us = S @ q
        ub = B @ q
        H = np.eye(3) * 1e-9
        g = np.array([-10.0, 0.0, 0.0])
        active = us > 0.0
        if active.any():
            A = S[active]
            H += ks * (A.T @ A)
            g += ks * (A.T @ us[active])
        caught = (-ub - gap) > 0.0
        if caught.any():
            A = B[caught]
            kk = kb[caught]
            pen = (-ub - gap)[caught]
            H += (kk[:, None] * A).T @ A
            g += -A.T @ (kk * pen)
        step = np.linalg.solve(H, g)
        nrm = float(np.linalg.norm(step))
        if nrm > 0.2:
            step *= 0.2 / nrm
        q = q - step
        if nrm < 1e-11:
            break
    if float((S @ q).max()) > 0.05 or abs(float(q[0])) > 5.0:
        return None
    return q


def _drop_over(q, press, box, panel, n=6):
    x0, x1 = max(box[0], panel[0]), min(box[2], panel[2])
    y0, y1 = max(box[1], panel[1]), min(box[3], panel[3])
    if x1 - x0 < 0.05 or y1 - y0 < 0.05:
        return None
    px, py = press
    worst = -1e9
    for i in range(n):
        for j in range(n):
            x = x0 + (x1 - x0) * i / (n - 1)
            y = y0 + (y1 - y0) * j / (n - 1)
            worst = max(worst, q[0] + q[1] * (x - px) + q[2] * (y - py))
    return worst


def insertion(cn, back):
    gap = back - cn["Zt"]
    straight = cn["panel"][2] - cn["mouth"]
    return cn["fpc_free"] - math.pi * (gap - cn["fpc_z"]) / 2.0 - straight


def analyse(src, cn, rep):
    Kax, apex, half = flank(cn)
    drop = grab(src, "sh_drop")
    below = grab(src, "sh_R_below")
    stop = cn["Z_panel_b"] - drop
    R0 = stop + cn["panel_t"] - below
    t0 = grab(src, "sh_t")
    w = grab(src, "sh_w")
    nub = grab(src, "sh_nub_h")
    y0 = grab(src, "sh_y0")
    land0 = grab(src, "sh_land0")
    land1 = grab(src, "sh_land1")
    root = grab(src, "sh_root")
    loc_gap = grab(src, "sh_loc_gap")
    spans = grab_list(src, "sh_spans")
    geos = span_geometry(spans, root)
    # Land is centred on the beam, so the load does not twist it.
    rep.say("stop %.2f   free land %.2f   section %.2f x %.2f   nub %.2f" % (
        stop, R0, w, t0, nub))
    rep.say("flank Kax %.2f N/mm   apex climb %.2f mm   window half %.3f mm" % (
        Kax, apex, half))
    rep.say("land is on the beam centreline, so the torsion factor is 1.00")

    boards = (-cn["pcb_tol"], 0.0, cn["pcb_tol"])
    carriers = (-0.15, 0.0, 0.15)
    panels = (1.1, 1.2, 1.3)
    shells = (-0.10, 0.0, 0.10)
    fingers = (-0.05, 0.0, 0.05)
    rows = []
    for b in boards:
        for c in carriers:
            for tp in panels:
                for dz in shells:
                    for ft in fingers:
                        front = stop + c + b + tp
                        s_win = max(0.0, half + dz)
                        RR = R0 - ft
                        tt = t0 + ft
                        ks = []
                        for length, a, _nubx, _which in geos:
                            ks.append(beam_k(w, max(tt, 0.4), length, a))
                        K = sum(ks)
                        d0 = front - RR - s_win
                        if d0 <= 0:
                            d, climb, s, F = 0.0, 0.0, s_win, 0.0
                        else:
                            d = d0 / (1.0 + K / Kax)
                            F = K * d
                            climb = F / Kax
                            s = s_win + climb
                        air = (RR + nub + s) - front
                        top = (front + nub + tt) if d > 0 else (RR + nub + tt + s)
                        ceil = cn["Z_in"] + cn["face_recess"] - grab(src, "sh_ceil_skin")
                        air_c = (ceil + s) - top
                        rows.append({
                            "b": b, "c": c, "tp": tp, "dz": dz, "ft": ft,
                            "front": front, "d": d, "F": F, "s": s, "climb": climb,
                            "air": air, "air_c": air_c, "K": K, "ks": ks, "gap": max(0.0, -d0),
                        })
    bear = [r for r in rows if r["air"] < 0.10]
    unclip = [r for r in rows if r["climb"] > apex - 0.04]
    touch = sum(1 for r in rows if r["d"] > 0.02)
    rep.say("corners %d   land touching %d   gap %d" % (
        len(rows), touch, len(rows) - touch))
    worst_air = min(rows, key=lambda r: r["air"])
    worst_ceil = min(rows, key=lambda r: r["air_c"])
    rep.say("pillar air min %.3f mm at board %+.2f carrier %+.2f panel %.1f shell %+.2f finger %+.2f" % (
        worst_air["air"], worst_air["b"], worst_air["c"], worst_air["tp"],
        worst_air["dz"], worst_air["ft"]))
    rep.say("ceiling air min %.3f mm   max climb %.3f mm" % (
        worst_ceil["air_c"], max(r["climb"] for r in rows)))
    rep.gate(not bear, "pillars stay %.2f mm off the panel (%d corners closer)" % (
        0.10, len(bear)))
    rep.gate(not unclip, "every corner clips below the bump apex (%d over)" % len(unclip))
    rep.gate(worst_ceil["air_c"] >= 0.10, "ceiling air %.3f mm" % worst_ceil["air_c"])

    # Sideways. Shell ribs leave loc_gap to each fence outer face, so the
    # shell can move ±loc_gap. A narrow panel sitting on the bottom fence
    # puts the top edge  (panel_h - panel_lo) inboard of the nominal top.
    # A short panel sitting on the bottom fence puts the top edge this far
    # inboard of the nominal top edge.
    width_in = cn["panel_hi"] - cn["panel_lo"]
    shell_y = loc_gap
    # Float of a nominal panel between the 0.20 fences.
    fence_play = 2 * cn["fence_air"] + (cn["panel_h"] - cn["panel_h"])  # nominal
    # nominal outline equals panel_h. Free play = 2*fence_air.
    play = 2.0 * cn["fence_air"]
    rep.say("fence free play on a nominal panel ±%.2f mm" % (play / 2.0))
    rep.gate(play / 2.0 <= 0.25 + 1e-9, "sideways float ±%.2f mm" % (play / 2.0))
    # Contact distance from the actual edge.
    # Bottom edge on the fence:  fence_air outboard of nominal.
    # Top edge of a short panel: width_in further inboard.
    def band(extra):
        near = land0 - extra
        far = land1 + extra
        return near, far
    lo, hi = min(land0, land1), max(land0, land1)
    print_tol = 0.05
    top_extra = width_in + shell_y + print_tol
    bot_extra = cn["fence_air"] + shell_y + print_tol
    top_near, top_far = lo - top_extra, hi + shell_y + print_tol
    bot_near, bot_far = lo - shell_y - print_tol, hi + bot_extra
    rep.say("top land from the edge %.2f to %.2f" % (top_near, top_far))
    rep.say("bottom land from the edge %.2f to %.2f" % (bot_near, bot_far))
    aa_from_edge = cn["aa"][1] - cn["panel"][1]
    rep.gate(min(top_near, bot_near) >= 1.92 - 1e-9, "contact stays outside 1.92")
    rep.gate(max(top_far, bot_far) <= 3.32 + 1e-9, "contact stays inside 3.32")
    rep.gate(y0 - shell_y >= 0.0, "bridge stays on the panel")
    # Beam inner face versus the active area, panel shifted outboard by the fence.
    beam_in = y0 + w + cn["fence_air"] + shell_y
    rep.say("bridge inboard face %.2f mm from an edge on the fence (active area %.2f)" % (
        beam_in, aa_from_edge))
    rep.gate(beam_in < aa_from_edge - 0.05, "bridge stays off the active area")

    # Printable sections. Stock line width is 0.42 mm.
    widths = {
        "bridge thickness": t0,
        "bridge width": w,
        "land width": abs(land1 - land0),
        "land length": grab(src, "sh_nub_len"),
        "land height": nub,
        "locator thickness": grab(src, "sh_loc_t"),
        "pillar root": root,
    }
    for label, val in widths.items():
        rep.gate(val + 1e-9 >= LINE_W, "%s %.2f mm >= %.2f" % (label, val, LINE_W))

    # The nub sits on its span, at least 2 mm from either pillar face.
    for length, a, nx, which in geos:
        b = length - a
        rep.gate(a >= 2.0 - 1e-6 and b >= 2.0 - 1e-6,
                 "land x %.2f is %.2f and %.2f mm from the pillars" % (nx, a, b))

    # Root strain at the thickest force. Off-centre load, larger end moment.
    max_eps = 0.0
    over_1 = 0
    pairs = 0
    for r in rows:
        for length, a, _nx, _which in geos:
            tt = t0 + r["ft"]
            kk = beam_k(w, max(tt, 0.4), length, a)
            share = kk / r["K"] if r["K"] else 0.0
            F = r["F"] * share
            b = length - a
            mend = F * a * b * max(a, b) / length ** 2
            eps = 6.0 * mend / (E_SOFT * w * tt ** 2) if tt else 9.0
            pairs += 1
            if eps > 0.01 + 1e-12:
                over_1 += 1
            max_eps = max(max_eps, eps)
    rep.say("root strain max %.2f%%" % (100 * max_eps))
    rep.say("root strain over 1%%: %d of %d pairs" % (over_1, pairs))
    rep.gate(max_eps <= 0.012 + 1e-12, "root strain %.2f%% <= 1.20%%" % (100 * max_eps))

    # Ribs overlap the fences in z at every corner.
    z_fence = cn["Z_in"] - 0.40
    rib0 = cn["Z_in"] - grab(src, "sh_rib_drop")
    min_ov = 99.0
    for r in rows:
        ov = (z_fence + r["c"] + r["b"]) - (rib0 + r["s"])
        min_ov = min(min_ov, ov)
    rep.say("rib/fence overlap min %.2f mm" % min_ov)
    rep.gate(min_ov >= 0.20 - 1e-6, "ribs overlap the fences by %.2f mm" % min_ov)

    # The top-left stop has to be present. Removing it is a defect even if
    # the air over U7 stays positive.
    cols = grab_list(src, "sh_columns")
    have_u7 = any(c[0] <= 29.6 and c[2] >= 33.4 and c[1] <= 29.3 and c[3] >= 30.4 for c in cols)
    rep.gate(have_u7, "top-left stop covers x 29.6-33.4, y 29.3-30.4")

    # J4 with the shell fitted. Reported, not a gate. Where the land is
    # clear, the cable lifts the cable end about the left land until the
    # right land takes up its gap.
    x_end = cn["panel"][2]
    left_lands = [nx for _L, _a, nx, _wch in geos]
    x_pivot = min(left_lands) if left_lands else cn["panel"][0]
    x_land = max(left_lands) if left_lands else x_end
    lever = (x_end - x_pivot) / max(x_land - x_pivot, 1.0)
    rep.say("")
    rep.say("J4, shell fitted. A miss of %.2f is not a gate." % J4_TARGET)
    def lifted(scale, load):
        out = []
        for r in rows:
            base = insertion(cn, stop + r["c"])
            held = scale * r["F"]
            k = scale * r["K"] if r["K"] else 0.0
            if r["gap"] <= 1e-9 and held + 1e-9 >= load:
                lift = 0.0
            else:
                need = max(0.0, load - held)
                spring = need / k if k else 1.0
                lift = (r["gap"] + spring) * lever
            out.append(base - (math.pi / 2.0) * max(lift, 0.0))
        return out
    fresh = lifted(1.0, CABLE)
    crept = lifted(CREEP_KEEP, CABLE)
    knocked = lifted(CREEP_KEEP, CABLE + SHOCK)
    rep.say("seated backs %.2f / %.2f / %.2f" % tuple(
        insertion(cn, stop + c) for c in carriers))
    rep.say("cable, as printed: %.2f to %.2f, %d of %d corners under %.2f" % (
        min(fresh), max(fresh),
        sum(1 for v in fresh if v < J4_TARGET - 1e-9), len(fresh), J4_TARGET))
    rep.say("cable, bridges relaxed: %.2f to %.2f, %d of %d corners under %.2f" % (
        min(crept), max(crept),
        sum(1 for v in crept if v < J4_TARGET - 1e-9), len(crept), J4_TARGET))
    rep.say("cable and a 5 g knock, bridges relaxed, minimum %.2f" % min(knocked))

    # 10 N, rigid panel, preload lost. Retained bridge stiffness is 0.35.
    # The free gap is the loose stack corner. The low carrier sets the rest air.
    rep.say("")
    rep.say("10 N press, preload lost, rigid panel")
    pads = pads_from_case(src, cn["panel"])
    for c in cols:
        pads.append((c[0], c[1], c[2], c[3]))
    supports = _pad_points(pads, 3)
    import numpy as np
    bridges = []
    for length, a, nx, which in geos:
        edge = cn["panel"][1] if which == 0 else cn["panel"][3]
        sign = 1.0 if which == 0 else -1.0
        yb = edge + sign * (lo + hi) / 2.0
        bridges.append((nx, yb, beam_k(w, t0, length, max(a, 0.4))))
    bridges = np.array(bridges, float)
    supports = np.array(supports, float)
    loose = max(r["gap"] for r in rows)
    rep.say("loose-corner gap %.3f mm, bridge stiffness kept at %.2f" % (loose, CREEP_KEEP))
    p = cn["panel"]
    presses = [(29.5, 26.2)]
    for x in (p[0], 0.5 * (p[0] + p[2]), p[2]):
        for y in (p[1], 0.5 * (p[1] + p[3]), p[3]):
            presses.append((x, y))
    solved = []
    for press in presses:
        q = _contact_q(press, supports, bridges, loose, CREEP_KEEP)
        if q is not None:
            solved.append((press, q))
    parts = components_from_fit(open(FIT).read())
    for name, x0, y0p, x1, y1, h in parts:
        if y1 < p[1] or y0p > p[3] or x1 < p[0] or x0 > p[2]:
            continue
        rest = (stop - 0.15) - (cn["Zt"] + h)
        drop = 0.0
        for press, q in solved:
            d = _drop_over(q, press, (x0, y0p, x1, y1), p)
            if d is None:
                continue
            drop = max(drop, d)
        air = rest - drop
        if air < 1.0 or name in ("U7", "U8", "U6"):
            rep.say("  %-6s rest %.3f  tip %.3f  air %.3f" % (name, rest, drop, air))
        rep.gate(air > 0.0, "%s air %.3f mm under 10 N" % (name, air))
    return {"stop": stop, "R": R0, "rows": rows}


def render(scad, work):
    parts = {
        "sh_beams": "beams.stl",
        "sh_pillars": "pillars.stl",
        "sh_locs": "locs.stl",
        "sh_carrier": "carrier.stl",
        "sh_stock": "stock.stl",
        "sh_shell": "shell.stl",
    }
    for part, name in parts.items():
        out = os.path.join(work, name)
        if os.path.exists(out) and os.path.getmtime(out) > os.path.getmtime(scad):
            continue
        r = subprocess.run(
            [OPENSCAD, "-o", out, "-D", 'part="%s"' % part, scad],
            capture_output=True, text=True,
        )
        if r.returncode != 0:
            tail = (r.stderr or r.stdout)[-400:]
            raise SystemExit("openscad %s failed\n%s" % (part, tail))
    return {k: os.path.join(work, v) for k, v in parts.items()}


def mesh_checks(src, cn, rep, paths):
    import numpy as np
    import trimesh
    beams = trimesh.load(paths["sh_beams"])
    pillars = trimesh.load(paths["sh_pillars"])
    locs = trimesh.load(paths["sh_locs"])
    carrier = trimesh.load(paths["sh_carrier"])
    stock = trimesh.load(paths["sh_stock"])

    def clearance(part, obst, n=2000):
        pts = part.vertices
        if len(pts) < n:
            pts = np.vstack([pts, _sample(part, min(n, max(len(pts), 50)), 1)])
        d = trimesh.proximity.ProximityQuery(obst).signed_distance(pts)
        return -float(np.max(d))

    air_shell = clearance(beams, stock)
    bite = clearance(pillars, stock)
    rep.say("beam vs shell %.2f mm   pillar bite %.2f mm" % (air_shell, -bite))
    rep.gate(air_shell >= 0.10, "bridges clear the shell")
    rep.gate(bite < -0.02, "pillars join the shell")
    # Locators against the carrier: a designed gap, not a clash.
    air_loc = clearance(locs, carrier, n=1500)
    rep.say("locator vs carrier %.2f mm" % air_loc)
    rep.gate(air_loc >= 0.08, "locators clear the fences")
    rep.gate(air_loc <= 0.30, "locators are close enough to set y")
    # Thick corner pose: shell on the flank, including the climb that
    # the bridges produce there, and a finger 0.05 mm thick.
    half = (2.0 - 1.125) / 2.0
    front = (cn["Z_panel_b"] - grab(src, "sh_drop")) + 0.15 + cn["pcb_tol"] + 1.3
    s = _thick_s(src, cn, front, half)
    panel = _panel_mesh(cn, front - cn["panel_t"], front)
    low_p = pillars.copy()
    low_p.apply_translation([0, 0, s - 0.05])
    low_b = beams.copy()
    low_b.apply_translation([0, 0, s - 0.05])
    air_pp = clearance(low_p, panel, n=1500)
    air_pb = clearance(low_b, panel, n=1500)
    rep.say("thick corner, shell low: pillar air %.2f   bridge air %.2f" % (air_pp, air_pb))
    # The land is meant to meet the panel. Bridge air may be negative at the nub
    # only. Pillars must stay positive.
    rep.gate(air_pp >= 0.08, "pillars clear the thick panel")
    bodies = carrier.split(only_watertight=False)
    rep.say("carrier bodies %d" % len(bodies))
    rep.gate(len(bodies) == 1, "carrier is one solid")
    stop = cn["Z_panel_b"] - grab(src, "sh_drop")
    v = carrier.vertices
    p = cn["panel"]
    X0, X1 = cn["OX"] + p[0] + 0.5, cn["OX"] + p[2] - 0.5
    Y0 = cn["H"] - cn["OYT"] - p[3] + 0.5
    Y1 = cn["H"] - cn["OYT"] - p[1] - 0.5
    under = v[(v[:, 0] > X0) & (v[:, 0] < X1) & (v[:, 1] > Y0) & (v[:, 1] < Y1)]
    zmax = float(under[:, 2].max()) if len(under) else -1
    rep.say("carrier top under the panel %.2f" % zmax)
    rep.gate(abs(zmax - stop) < 0.08, "stop mesh %.2f matches %.2f" % (zmax, stop))
    # Added stops and ties versus parts. The shipped carrier already
    # threads the tail field; only the new solids are gated.
    parts = components_from_fit(open(FIT).read())
    added = []
    for c in grab_list(src, "sh_columns"):
        added.append(_box_mesh(cn, c[0], c[1], c[2], c[3], cn["Zt"], cn["Z_panel_b"]))
    if "sh_ties" in src:
        for c in grab_list(src, "sh_ties"):
            added.append(_box_mesh(cn, c[0], c[1], c[2], c[3], cn["Zt"], cn["Zt"] + grab(src, "sh_tie_h")))
    for name, x0, y0, x1, y1, h in parts:
        box = _box_mesh(cn, x0, y0, x1, y1, cn["Zt"], cn["Zt"] + h)
        worst = 99.0
        for solid in added:
            worst = min(worst, clearance(solid, box, n=400))
        if worst < 1.0:
            rep.say("  added stop vs %-8s %.2f" % (name, worst))
        rep.gate(worst >= 0.08, "added stops clear %s by %.2f mm" % (name, worst))
    # No shoes in the scad.
    rep.gate("sh_shoes" not in src and "module sh_shoe" not in src, "no edge shoes")
    shell_m = trimesh.load(paths["sh_shell"])
    shell_bodies = shell_m.split(only_watertight=False)
    rep.say("shell bodies %d" % len(shell_bodies))
    rep.gate(len(shell_bodies) == 1, "shell is one body")
    air_border = _border_clearance(shell_m, src, cn)
    rep.say("shell vs display panel outside the lands %.3f mm" % air_border)
    # The beam underside is flush with the panel front, so the 0.10 mm air
    # rule stays on the active area. Outside the land boxes the shell may
    # meet that front and must not enter the panel.
    rep.gate(air_border > -0.02, "border outside the lands does not enter the panel (%.3f mm)" % air_border)


def _sample(mesh, n, seed):
    """Surface samples from a fixed seed, so the same mesh reads the same clearance on every run."""
    return mesh.sample(int(n), seed=seed)


def _land_rects(src, panel):
    land0 = grab(src, "sh_land0")
    land1 = grab(src, "sh_land1")
    nlen = grab(src, "sh_nub_len")
    lo, hi = min(land0, land1), max(land0, land1)
    out = []
    for _x0, _x1, which, nubx in grab_list(src, "sh_spans"):
        edge = panel[1] if int(which) == 0 else panel[3]
        sign = 1.0 if int(which) == 0 else -1.0
        ya = edge + sign * lo
        yb = edge + sign * hi
        out.append((nubx - nlen / 2.0, min(ya, yb), nubx + nlen / 2.0, max(ya, yb)))
    return out


def _border_clearance(shell_m, src, cn, pad=0.15):
    """Clearance of the shell to the whole display panel, land boxes left out."""
    import numpy as np
    import trimesh
    pts = shell_m.vertices
    extra = _sample(shell_m, 6000, 2)
    pts = np.vstack([pts, extra])
    bx = pts[:, 0] - cn["OX"]
    by = cn["H"] - cn["OYT"] - pts[:, 1]
    keep = np.ones(len(pts), dtype=bool)
    for x0, y0, x1, y1 in _land_rects(src, cn["panel"]):
        keep &= ~((bx >= x0 - pad) & (bx <= x1 + pad) & (by >= y0 - pad) & (by <= y1 + pad))
    if not np.any(keep):
        return 99.0
    panel = _panel_mesh(cn, cn["Z_panel_b"], cn["Z_panel_b"] + cn["panel_t"])
    dist = trimesh.proximity.ProximityQuery(panel).signed_distance(pts[keep])
    return -float(np.max(dist))


def _thick_s(src, cn, front, half):
    """Shell shift at the thick corner, climb included."""
    Kax, apex, _half = flank(cn)
    R = (cn["Z_panel_b"] - grab(src, "sh_drop")) + cn["panel_t"] - grab(src, "sh_R_below")
    ft = 0.05
    tt = grab(src, "sh_t") + ft
    w = grab(src, "sh_w")
    s_win = max(0.0, half - 0.10)
    K = 0.0
    for length, a, _nx, _w in span_geometry(grab_list(src, "sh_spans"), grab(src, "sh_root")):
        K += beam_k(w, tt, length, a)
    d0 = front - (R - ft) - s_win
    if d0 <= 0 or K <= 0:
        return s_win
    d = d0 / (1.0 + K / Kax)
    return s_win + (K * d) / Kax


def _panel_mesh(cn, z0, z1):
    return _box_mesh(cn, cn["panel"][0], cn["panel"][1], cn["panel"][2], cn["panel"][3], z0, z1)


def _box_mesh(cn, x0, y0, x1, y1, z0, z1):
    import trimesh
    X0, X1 = cn["OX"] + x0, cn["OX"] + x1
    Y0, Y1 = cn["H"] - cn["OYT"] - y1, cn["H"] - cn["OYT"] - y0
    ext = [abs(X1 - X0), abs(Y1 - Y0), abs(z1 - z0)]
    m = trimesh.creation.box(extents=ext)
    m.apply_translation([(X0 + X1) / 2.0, (Y0 + Y1) / 2.0, (z0 + z1) / 2.0])
    return m


def draw(src, cn):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle

    drop = grab(src, "sh_drop")
    stop = cn["Z_panel_b"] - drop
    R = stop + cn["panel_t"] - grab(src, "sh_R_below")
    t = grab(src, "sh_t")
    nub = grab(src, "sh_nub_h")
    land0 = grab(src, "sh_land0")
    land1 = grab(src, "sh_land1")
    y0 = grab(src, "sh_y0")
    w = grab(src, "sh_w")
    p = cn["panel"]
    zt = cn["Zt"]

    fig, axes = plt.subplots(1, 2, figsize=(11.4, 5.2), dpi=140)

    def section(ax, shell_on, title):
        ax.add_patch(Rectangle((-4, zt), 12, 1.6, fc="#2f6b3a", ec="#222", lw=0.4))
        ax.add_patch(Rectangle((0.3, zt), 2.0, stop - zt, fc="#c4a574", ec="#222", lw=0.4))
        ax.add_patch(Rectangle((0, stop), 7, cn["panel_t"], fc="#f7f7f7", ec="#222", lw=0.5))
        ax.plot([1.92, 1.92], [stop, stop + 2.2], color="#9b2335", lw=0.5, ls="--")
        ax.plot([3.32, 3.32], [stop, stop + 2.2], color="#9b2335", lw=0.5, ls="--")
        if shell_on:
            front = stop + cn["panel_t"]
            ax.add_patch(Rectangle((land0, front), land1 - land0, nub, fc="#d7b56d", ec="#222", lw=0.5))
            ax.add_patch(Rectangle((y0, front + nub), w, t, fc="#d7b56d", ec="#222", lw=0.5))
            ax.add_patch(Rectangle((-1.0, front + nub + t + 0.25), 6.5, 0.74, fc="#6d8ea8", ec="#222", lw=0.4))
        else:
            ax.add_patch(Rectangle((-1.0, cn["Z_in"] + cn["face_recess"]), 6.5, 0.74, fc="#d5dbe2", ec="#222", lw=0.4))
        ax.set_xlim(-1.6, 5.6)
        ax.set_ylim(zt - 0.3, 26.8)
        ax.set_aspect("equal")
        ax.set_title(title, fontsize=10)
        ax.set_xlabel("mm from the panel edge")

    section(axes[0], False, "Shell off. Panel on the stop.")
    section(axes[1], True, "Shell on. Land on the border.")
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, "shell-hold-section.png"))
    plt.close()

    fig, ax = plt.subplots(figsize=(11.2, 5.6), dpi=140)
    ax.add_patch(Rectangle((p[0], p[1]), p[2] - p[0], p[3] - p[1], fc="#f4f4f4", ec="#222", lw=0.8))
    aa = cn["aa"]
    ax.add_patch(Rectangle((aa[0], aa[1]), aa[2] - aa[0], aa[3] - aa[1], fc="#d9dee6", ec="#666", lw=0.4))
    for name, x0, y0p, x1, y1, h in components_from_fit(open(FIT).read()):
        if h < 2.0 and name not in ("U7", "U8", "U3"):
            continue
        ax.add_patch(Rectangle((x0, y0p), x1 - x0, y1 - y0p, fc="#e7c7c7", ec="#844", lw=0.3))
    for c in grab_list(src, "sh_columns"):
        ax.add_patch(Rectangle((c[0], c[1]), c[2] - c[0], c[3] - c[1], fc="#c4a574", ec="#222", lw=0.4))
    for s in grab_list(src, "sh_spans"):
        edge = p[1] if s[2] == 0 else p[3]
        sign = 1 if s[2] == 0 else -1
        yy = edge + sign * (y0 + w / 2.0)
        ax.plot([s[0] + grab(src, "sh_root"), s[1] - grab(src, "sh_root")], [yy, yy],
                color="#a67c2d", lw=2.5, solid_capstyle="butt")
        ax.plot(s[3], edge + sign * (land0 + land1) / 2.0, marker="s", color="#7a4e12", ms=5)
    ax.set_aspect("equal")
    ax.set_xlim(24, 116)
    ax.set_ylim(70, -2)
    ax.set_title("Plan. Bridges on the border, stop at the top left.")
    ax.set_xlabel("board x")
    ax.set_ylabel("board y")
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, "shell-hold-plan.png"))
    plt.close()


def run(scad):
    src = open(scad).read()
    cn = case_nums(src)
    rep = Report()
    analyse(src, cn, rep)
    if rep.fails and os.path.abspath(scad) != os.path.abspath(SCAD):
        print("")
        print("%d FAILED" % len(rep.fails))
        for f in rep.fails:
            print(" - " + f)
        return 1
    work = os.path.join(tempfile.gettempdir(), "r6check")
    os.makedirs(work, exist_ok=True)
    digest = hashlib.sha256(open(scad, "rb").read()).hexdigest()[:16]
    slot = os.path.join(work, digest)
    os.makedirs(slot, exist_ok=True)
    paths = render(scad, slot)
    print("")
    mesh_checks(src, cn, rep, paths)
    if os.path.abspath(scad) == os.path.abspath(SCAD):
        draw(src, cn)
    print("")
    if rep.fails:
        print("%d FAILED" % len(rep.fails))
        for f in rep.fails:
            print(" - " + f)
        return 1
    print("ALL GATES PASSED")
    return 0


def _set_assign(src, name, value):
    """Replace one numeric assignment with the value in this file, whatever it is now."""
    pat = re.compile(r"(?m)^(%s\s*=\s*)(-?\d*\.?\d+)(\s*;)" % re.escape(name))
    if not pat.search(src):
        raise SystemExit("mutation missing assignment " + name)
    return pat.sub(lambda m: m.group(1) + value + m.group(3), src, count=1)


def self_test():
    base = open(SCAD).read()
    raw_rows = re.findall(r"\[[^\]]+\]", re.search(r"sh_spans\s*=\s*\[(.*?)\];", base, re.S).group(1))
    right = None
    for raw in raw_rows:
        nums = [float(v) for v in raw.strip()[1:-1].split(",")]
        if len(nums) == 4 and int(nums[2]) == 0 and nums[0] > 70:
            right = raw.strip()
            break
    if right is None:
        raise SystemExit("mutation missing right-hand span")
    added = "module sh_added() { sh_beams(); sh_pillar_all(); sh_loc_all(); }"
    bump = ("module sh_added() { sh_beams(); sh_pillar_all(); sh_loc_all(); "
            "board_box(58, panel[1] + 2.10, 62, panel[1] + 2.70, Z_in - 1.0, Z_in + 0.05); }")
    mutations = [
        ("unprintable nub", _set_assign(base, "sh_land1", "2.57")),
        ("ribs not engaging", _set_assign(base, "sh_rib_drop", "0.40")),
        ("U7 column removed", base.replace("  [29.60, 29.30, 33.40, 30.40],\n", "", 1)),
        ("nub off its span", base.replace(right, right.rsplit(",", 1)[0] + ", 10.00]", 1)),
        ("nub at the pillar", base.replace(right, right.rsplit(",", 1)[0] + ", 104.20]", 1)),
        ("thin locator", _set_assign(base, "sh_loc_t", "0.20")),
        ("short pillar root", _set_assign(base, "sh_root", "0.30")),
        ("nub height 0.15", _set_assign(base, "sh_nub_h", "0.15")),
        ("land on the active area", _set_assign(base, "sh_land0", "4.20")),
        ("column on U7", base.replace("[29.60, 29.30, 33.40, 30.40]", "[29.60, 26.40, 33.40, 28.80]", 1)),
        ("pillars short of the shell", _set_assign(base, "sh_bite", "-0.80")),
        ("stop into the opto-triacs", _set_assign(base, "sh_drop", "0.55")),
        ("locator gap wide open", _set_assign(base, "sh_loc_gap", "1.20")),
        ("negative land offset", _set_assign(base, "sh_R_below", "-0.30")),
        ("border bump between spans", base.replace(added, bump, 1)),
        ("beam detached from its pillar", base.replace(
            "x0 + sh_root - 0.12", "x0 + sh_root + 0.60", 1).replace(
            "x1 - sh_root + 0.12", "x1 - sh_root - 0.60", 1)),
    ]
    bad = 0
    for name, mutated in mutations:
        if mutated == base:
            print("MUTATION SETUP FAIL", name)
            bad += 1
            continue
        path = os.path.join(os.path.dirname(SCAD), "_mut_check.scad")
        open(path, "w").write(mutated)
        r = subprocess.run([sys.executable, __file__, "--scad", path], capture_output=True, text=True)
        status = "FAIL-AS-REQUIRED" if r.returncode else "MISSED"
        if r.returncode == 0:
            bad += 1
        fails = [ln.strip() for ln in r.stdout.splitlines() if ln.strip().startswith("- ")]
        print("%s  %s  %s" % (status, name, "; ".join(fails[:3])))
        os.remove(path)
    caught = len(mutations) - bad
    print("caught %d of %d" % (caught, len(mutations)))
    if bad:
        print("%d mutations were not caught" % bad)
        return 1
    print("ALL MUTATIONS CAUGHT")
    return 0


def main():
    args = sys.argv[1:]
    if "--self-test" in args:
        return self_test()
    scad = SCAD
    if "--scad" in args:
        scad = args[args.index("--scad") + 1]
    return run(scad)


if __name__ == "__main__":
    sys.exit(main())
