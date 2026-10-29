#!/usr/bin/env python3
"""Capture gates for the shell-bridge hold-down.

Renders the meshes from the scad on every run. The 2x preload target is not a gate.
"""
import math
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SCAD = os.path.join(ROOT, "case", "scratch", "r6_shell_hold.scad")
CASE = os.path.join(ROOT, "case", "thermostat_case.scad")
FIT = os.path.join(ROOT, "scripts", "case_fit.py")
OUT = os.path.dirname(__file__)
OPENSCAD = os.environ.get("OPENSCAD", "openscad")

CABLE = 0.30
SHOCK = 0.27
E_SOFT = 1500.0
CREEP = 0.35
J4_FLOOR = 3.00
J4_TARGET = 3.15
LIFT_CAP = 0.25
PANEL_E = 70000.0


def grab(text, name):
    m = re.search(r"(?:^|[;\n])\s*%s\s*=\s*([0-9.]+)\s*;" % re.escape(name), text)
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


def insertion(cn, back):
    gap = back - cn["Zt"]
    straight = cn["panel"][2] - cn["mouth"]
    return cn["fpc_free"] - math.pi * (gap - cn["fpc_z"]) / 2.0 - straight


def tip_lift(springs, loads, stops, x_end):
    """Rigid panel. u = A + B x. Stops block downward motion. Loads are upward."""
    import numpy as np
    active = []
    for _ in range(16):
        n = 2 + len(active)
        M = np.zeros((n, n))
        rhs = np.zeros(n)
        for x, kx in springs:
            M[0, 0] += kx
            M[0, 1] += kx * x
            M[1, 0] += kx * x
            M[1, 1] += kx * x * x
        for i, xs in enumerate(active):
            M[0, 2 + i] -= 1.0
            M[1, 2 + i] -= xs
            M[2 + i, 0] = 1.0
            M[2 + i, 1] = xs
        rhs[0] = sum(F for _, F in loads)
        rhs[1] = sum(F * x for x, F in loads)
        sol = np.linalg.solve(M, rhs)
        A, B = float(sol[0]), float(sol[1])
        viol = None
        for xs in stops:
            if any(abs(xs - a) < 1e-3 for a in active):
                continue
            u = A + B * xs
            if u < -1e-5 and (viol is None or u < viol[1]):
                viol = (xs, u)
        drop = next((i for i in range(len(active)) if sol[2 + i] < -1e-5), None)
        if drop is not None:
            active.pop(drop)
            continue
        if viol:
            active.append(viol[0])
            continue
        return A + B * x_end
    return 99.0


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
                        air_c = (25.45 + s) - top
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
    top_extra = width_in + shell_y
    bot_extra = cn["fence_air"] + shell_y
    top_near, top_far = lo - top_extra, hi + shell_y
    bot_near, bot_far = lo - shell_y, hi + bot_extra
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

    # J4 with tipping. Preload lost.
    x_end = cn["panel"][2]
    x_cg = 0.5 * (cn["panel"][0] + cn["panel"][2])
    # Representative stop x positions: shipped pads plus the top-left column.
    stops = [47.0, 50.0, 76.0, 83.0, 31.5]
    nubx = {}
    for length, a, nx, which in geos:
        nubx.setdefault(which, []).append(nx)
    # Use the x of each land once (top and bottom share x).
    xs = sorted(set(round(nx, 2) for _L, _a, nx, _w in geos))

    def j4_at(carrier, factor, loads):
        springs = []
        for length, a, nx, _which in geos:
            springs.append((nx, beam_k(w, t0, length, a) * factor))
        lift = tip_lift(springs, loads, stops, x_end)
        back = stop + carrier
        return lift, insertion(cn, back) - (math.pi / 2.0) * max(lift, 0.0)

    rep.say("")
    rep.say("J4 after preload loss, panel free of a cable-end lip")
    worst_j = 99.0
    worst_lift = 0.0
    for c in carriers:
        lift_c, j_c = j4_at(c, CREEP, [(x_end, CABLE)])
        lift_k, j_k = j4_at(c, 1.0, [(x_end, CABLE), (x_cg, SHOCK)])
        worst_j = min(worst_j, j_c, j_k)
        worst_lift = max(worst_lift, lift_c, lift_k)
        rep.say("  carrier %+.2f  cable lift %.2f  J4 %.2f   cable+5g lift %.2f  J4 %.2f" % (
            c, lift_c, j_c, lift_k, j_k))
    rep.say("worst J4 without a lip %.2f mm   worst cable-end lift %.2f mm" % (worst_j, worst_lift))
    rep.gate(worst_lift <= LIFT_CAP + 1e-9, "cable-end lift %.2f mm <= %.2f" % (worst_lift, LIFT_CAP))
    rep.gate(worst_j + 1e-9 >= J4_FLOOR,
             "J4 after preload loss %.2f mm >= %.2f" % (worst_j, J4_FLOOR))
    # Not a mesh failure: the lip is an option, reported beside this.
    lip_gap = 0.15
    rep.say("")
    rep.say("Optional cable-end lip, nominal gap %.2f mm over a 1.20 mm panel" % lip_gap)
    rep.say("The lip is not in the carrier mesh.")
    # Gap shrinks on a thick panel and on a long lip, grows on a thin panel and a short lip.
    gaps = []
    for tp in panels:
        for lip in (-0.05, 0.0, 0.05):
            gap = lip_gap - (tp - cn["panel_t"]) + lip
            gaps.append((tp, lip, gap))
    rep.say("gap range %.2f to %.2f mm" % (min(g[2] for g in gaps), max(g[2] for g in gaps)))
    # Lift cannot pass the gap. Use the largest gap as X.
    x_max = max(g[2] for g in gaps)
    bears = min(g[2] for g in gaps)
    rep.say("largest lift with the lip %.2f mm   smallest gap %.2f mm" % (x_max, bears))
    for c in carriers:
        back = stop + c
        j = insertion(cn, back) - (math.pi / 2.0) * max(x_max, 0.0)
        rep.say("  carrier %+.2f  J4 with the lip %.2f mm" % (c, j))
    rep.say("J4 target while preload exists is %.2f mm. It is not a gate." % J4_TARGET)

    # 10 N press, rigid panel, preload lost. The top-left column is the support
    # under U7. Drop at a point is zero inside a stop, otherwise the lever
    # from the nearest stop until a bridge on the far side takes the load.
    rep.say("")
    rep.say("10 N press, preload lost, rigid panel")
    cols = grab_list(src, "sh_columns")
    pads = [
        (47.0, 26.24, 53.0, 27.89),
        (76.0, 26.24, 90.6, 27.89),
        (49.6, 61.29, 55.6, 62.94),
        (76.0, 61.29, 82.0, 62.94),
        (82.0, 33.20, cn["panel"][2], 34.30),
        (76.0, 61.20, cn["panel"][2], 62.50),
    ]
    for c in cols:
        pads.append((c[0], c[1], c[2], c[3]))
    parts = components_from_fit(open(FIT).read())
    for name, x0, y0, x1, y1, h in parts:
        if y1 < cn["panel"][1] or y0 > cn["panel"][3]:
            continue
        if x1 < cn["panel"][0] or x0 > cn["panel"][2]:
            continue
        cxp, cyp = (x0 + x1) / 2.0, (y0 + y1) / 2.0
        # Clamp the sample to the outline.
        cxp = min(max(cxp, cn["panel"][0]), cn["panel"][2])
        cyp = min(max(cyp, cn["panel"][1]), cn["panel"][3])
        dist = _dist_pads(cxp, cyp, pads)
        # A stop within the part footprint holds the back on the stop.
        drop_mm = 0.0 if dist < 0.4 else _tip_drop(cxp, cyp, pads, geos, w, t0, root)
        rest = (stop - 0.15) - (cn["Zt"] + h)
        air = rest - drop_mm
        if air < 1.0 or name in ("U7", "U8", "U6"):
            rep.say("  %-6s rest %.2f  tip %.3f  air %.3f" % (name, rest, drop_mm, air))
        rep.gate(air > 0.0, "%s air %.3f mm under 10 N" % (name, air))
    return {
        "stop": stop, "R": R0, "rows": rows, "xs": xs if False else None,
        "worst_j": worst_j,
    }


def _dist_pads(x, y, pads):
    best = 1e9
    for x0, y0, x1, y1 in pads:
        dx = 0.0 if x0 <= x <= x1 else min(abs(x - x0), abs(x - x1))
        dy = 0.0 if y0 <= y <= y1 else min(abs(y - y0), abs(y - y1))
        best = min(best, math.hypot(dx, dy))
    return best


def _tip_drop(x, y, pads, geos, w, t, root):
    """How far a 10 N press drops the panel at (x, y) past the stop plane.

    The panel pivots about the nearest stop. Bridges on the far side resist
    once the panel rises into them. Preload is gone, so only the new rise counts.
    """
    dist = _dist_pads(x, y, pads)
    if dist < 0.4:
        return 0.0
    # Lever to the nearest land, roughly along x.
    lands = []
    for s in geos:
        length, a, nubx, _which = s
        lands.append(nubx)
    far = max(lands, key=lambda nx: abs(nx - x))
    arm_press = dist
    arm_spring = abs(far - x)
    if arm_spring < 1.0:
        arm_spring = 1.0
    # Two bridges near `far` (top and bottom), short-term k.
    k = 0.0
    for length, a, nubx, _which in geos:
        if abs(nubx - far) < 2.0:
            k += beam_k(w, t, length, a)
    # 10 N * arm_press = k * rise * arm_spring, drop = rise * arm_press / arm_spring
    rise = 10.0 * arm_press / (k * arm_spring) if k else 5.0
    return rise * (arm_press / arm_spring)


def render(scad, work):
    parts = {
        "sh_beams": "beams.stl",
        "sh_pillars": "pillars.stl",
        "sh_locs": "locs.stl",
        "sh_carrier": "carrier.stl",
        "sh_stock": "stock.stl",
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
            pts = np.vstack([pts, part.sample(min(n, max(len(pts), 50)))])
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
            added.append(_box_mesh(cn, c[0], c[1], c[2], c[3], cn["Zt"], cn["Zt"] + 1.2))
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
    case = open(CASE).read()
    cn = case_nums(case)
    rep = Report()
    analyse(src, cn, rep)
    work = os.path.join(tempfile.gettempdir(), "r6check")
    os.makedirs(work, exist_ok=True)
    # Key the render cache by the scad path so a mutation re-renders.
    slot = os.path.join(work, str(abs(hash(os.path.abspath(scad))) % 10**8))
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


def self_test():
    base = open(SCAD).read()
    mutations = [
        ("nub 0.15", "sh_nub_h = 0.45;", "sh_nub_h = 0.15;"),
        ("land on the active area", "sh_land0 = 2.50;", "sh_land0 = 4.20;"),
        ("column on U7", "[29.60, 29.30, 33.40, 30.40]", "[29.60, 26.40, 33.40, 28.80]"),
        ("pillars short of the shell", "sh_bite = 0.10;", "sh_bite = -0.80;"),
        ("stop into the opto-triacs", "sh_drop = 0.15;", "sh_drop = 0.55;"),
        ("locator gap wide open", "sh_loc_gap = 0.15;", "sh_loc_gap = 1.20;"),
    ]
    bad = 0
    for name, old, new in mutations:
        if old not in base:
            print("MUTATION SETUP FAIL", name)
            bad += 1
            continue
        path = os.path.join(os.path.dirname(SCAD), "_mut_check.scad")
        open(path, "w").write(base.replace(old, new, 1))
        r = subprocess.run([sys.executable, __file__, "--scad", path], capture_output=True, text=True)
        status = "FAIL-AS-REQUIRED" if r.returncode else "MISSED"
        if r.returncode == 0:
            bad += 1
        fails = [ln.strip() for ln in r.stdout.splitlines() if ln.strip().startswith("- ")]
        print("%s  %s  %s" % (status, name, "; ".join(fails[:2])))
        os.remove(path)
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
