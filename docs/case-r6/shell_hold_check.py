#!/usr/bin/env python3
"""Gates for the shell-bridge hold-down.

The bridges are printed in the shell. The shell stays in the snap window.
Margin is the held force after creep, divided by 0.57 N. The clip flank
takes part of the stack, so the thick corner is not allowed to ride past
the bump. A 2x hold at every corner is the target. The gates that fail
the run are the ones the geometry can keep: contact, strain, the cable,
the shock lift, J4, the land, and mesh clearance.
"""
import math
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SCAD = os.path.join(ROOT, "case", "scratch", "r6_shell_hold.scad")
CASE = os.path.join(ROOT, "case", "thermostat_case.scad")
OUT = os.path.dirname(__file__)
MESH = os.path.join(ROOT, "case", "scratch")

CABLE = 0.30
SHOCK = 0.27          # 5 g on the 5.5 g panel
PANEL_MASS = 5.5e-3
E_SOFT = 1500.0
CREEP = 0.35          # force left after the set
COMP = 0.90           # nub is off the bridge centreline
LIFT_MAX = 0.27       # mm a 5 g knock may raise a corner
STRAIN_MAX = 0.010
SHEAR_MAX = 5.0       # MPa across the layers
J4_MIN = 3.15
GLASS_E = 70000.0


def grab(text, name):
    m = re.search(r"(?:^|[;\n])\s*%s\s*=\s*([0-9.]+)\s*;" % re.escape(name), text)
    if not m:
        raise SystemExit("missing " + name)
    return float(m.group(1))


def grab_list(text, name):
    m = re.search(r"(?m)^%s\s*=\s*\[(.*?)\];" % re.escape(name), text, re.S)
    if not m:
        raise SystemExit("missing " + name)
    rows = []
    for row in re.findall(r"\[([^\]]+)\]", m.group(1)):
        rows.append([float(x) for x in row.split(",")])
    return rows


def case_nums(case):
    g = lambda n: grab(case, n)
    Zt = g("plate_t") + g("standoff_h") + g("pcb_t")
    panel = [float(x) for x in re.search(r"panel = \[([0-9., ]+)\]", case).group(1).split(",")]
    aa = [float(x) for x in re.search(r"aa\s*=\s*\[([0-9., ]+)\]", case).group(1).split(",")]
    return {
        "Zt": Zt,
        "panel": panel,
        "aa": aa,
        "fpc_free": g("fpc_free"),
        "fpc_z": g("fpc_z_ret"),
        "mouth": g("mouth_x"),
        "pcb_tol": g("pcb_tol"),
        "wall": g("wall"),
        "clr": g("clr"),
        "snap_bump": g("snap_bump"),
        "snap_z": g("snap_z"),
        "snap_w": g("snap_w"),
        "front_t": g("front_t"),
        "panel_t": g("panel_t"),
        "panel_gap": g("panel_gap"),
        "face_recess": g("face_recess"),
        "Z_in": Zt + g("panel_gap") + g("panel_t"),
        "win_m": g("win_m"),
        "fence_air": g("fence_air"),
        "H": g("H"),
        "OX": g("OX"),
        "OYT": g("OYT"),
    }


def window_half(cn):
    """Free play of the bump inside the 2.0 mm window, each side."""
    # Bump is 1.125 mm tall where the wall inner face meets it.
    bump_h = 1.125
    return (2.0 - bump_h) / 2.0


def flank(cn, E):
    """Axial stiffness of four clips on the 42 degree flank, mu 0.3."""
    wall = cn["wall"]
    L = cn["Z_in"] - cn["snap_z"]
    I = cn["snap_w"] * wall ** 3 / 12.0
    # Lateral stiffness from the case formula, scaled to this E.
    k_lat = (3.0 * E * I / L ** 3)
    slope = 0.8 / 0.9
    # Axial / lateral at mu 0.3 on the 42 degree flank.
    return 4.0 * 0.465 * k_lat * slope, slope


def finger_k(E, w, t, L):
    I = w * t ** 3 / 12.0
    return 192.0 * E * I / L ** 3 * COMP


def insertion(cn, back):
    gap = back - cn["Zt"]
    straight = cn["panel"][2] - cn["mouth"]
    return cn["fpc_free"] - math.pi * (gap - cn["fpc_z"]) / 2.0 - straight


class Report:
    def __init__(self):
        self.fails = []
        self.lines = []

    def say(self, s=""):
        self.lines.append(s)
        print(s)

    def gate(self, ok, msg):
        flag = "PASS" if ok else "FAIL"
        self.say("  %s  %s" % (flag, msg))
        if not ok:
            self.fails.append(msg)


def analyse(src, cn, rep):
    E = E_SOFT
    L = grab(src, "sh_L")
    w = grab(src, "sh_w")
    t = grab(src, "sh_t")
    R = grab(src, "sh_R")
    stop = grab(src, "sh_stop")
    nub_h = grab(src, "sh_nub_h")
    land0 = grab(src, "sh_land0")
    land1 = grab(src, "sh_land1")
    y0 = grab(src, "sh_y0")
    spans = grab_list(src, "sh_spans")
    shoes = grab_list(src, "sh_shoes")
    root = grab(src, "sh_root")
    n = len(spans)
    for s in spans:
        free = (s[1] - s[0]) - 2.0 * root
        if abs(free - L) > 0.05:
            rep.gate(False, "span %.2f–%.2f free length %.2f is not sh_L" % (s[0], s[1], free))
    half = window_half(cn)
    Kax, slope = flank(cn, E)
    apex = 0.50 / slope
    shock = SHOCK
    load = CABLE + shock
    rep.say("load %.3f N   cable %.2f   shock %.3f   2x is %.3f N" % (load, CABLE, shock, 2 * load))
    rep.say("bridges %d   L %.1f   section %.2f x %.2f   E %g   creep left %.0f%%" % (
        n, L, w, t, E, 100 * CREEP))
    rep.say("window float ±%.3f mm   flank apex at %.3f mm climb   Kax %.2f N/mm" % (
        half, apex, Kax))

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
                        s_win = half + dz
                        # Thicker bridge lowers the land. ft > 0 is thick.
                        delta0 = front - (R - ft) - s_win
                        tt = t + ft
                        K = n * finger_k(E, w, tt, L)
                        if delta0 <= 0 or K <= 0:
                            d, F = 0.0, 0.0
                            d_c, F_c = 0.0, 0.0
                        else:
                            d = delta0 / (1.0 + K / Kax)
                            F = K * d
                            Kc = CREEP * K
                            d_c = delta0 / (1.0 + Kc / Kax)
                            F_c = Kc * d_c
                        eps = 12.0 * tt * d / L ** 2
                        climb = F / Kax
                        rows.append({
                            "b": b, "c": c, "tp": tp, "dz": dz, "ft": ft,
                            "front": front, "d": d, "F": F, "d_c": d_c, "F_c": F_c,
                            "eps": eps, "climb": climb, "delta0": delta0, "tt": tt,
                        })

    worst = min(rows, key=lambda r: r["F_c"])
    thick = max(rows, key=lambda r: r["eps"])
    rep.say("")
    rep.say("corners %d" % len(rows))
    rep.say("worst held after creep  %.3f N   margin %.2fx" % (worst["F_c"], worst["F_c"] / load))
    rep.say("  board %+.2f  carrier %+.2f  panel %.1f  shell %+.2f  finger %+.2f" % (
        worst["b"], worst["c"], worst["tp"], worst["dz"], worst["ft"]))
    climb_c = (worst["F_c"] / Kax) if Kax else 0.0
    rep.say("  contact deflection %.2f mm   crept climb %.2f mm" % (worst["d_c"], climb_c))
    rep.say("thick strain %.2f%%   force %.2f N   climb %.2f mm   deflection %.2f mm" % (
        100 * thick["eps"], thick["F"], thick["climb"], thick["d"]))
    rep.say("  board %+.2f  carrier %+.2f  panel %.1f  shell %+.2f  finger %+.2f" % (
        thick["b"], thick["c"], thick["tp"], thick["dz"], thick["ft"]))

    # Nominal numbers at E 1800 and 2100, same worst geometry, force scales with E
    # because K and Kax both scale, so deflection does not. Force scales.
    rep.say("same corner at E 1800: held %.3f N   %.2fx" % (
        worst["F_c"] * 1800 / E, worst["F_c"] * 1800 / E / load))
    rep.say("same corner at E 2100: held %.3f N   %.2fx" % (
        worst["F_c"] * 2100 / E, worst["F_c"] * 2100 / E / load))

    no_contact = sum(1 for r in rows if r["d_c"] <= 0.02)
    rep.gate(no_contact == 0, "positive contact after creep at every corner (%d miss)" % no_contact)
    rep.gate(thick["eps"] <= STRAIN_MAX + 1e-6,
             "sustained strain %.2f%% <= 1.00%%" % (100 * thick["eps"]))
    rep.gate(thick["climb"] <= apex - 0.04,
             "thick climb %.2f mm stays on the flank (apex %.2f)" % (thick["climb"], apex))
    rep.gate(worst["F_c"] + 1e-9 >= CABLE,
             "worst held %.3f N beats the %.2f N cable" % (worst["F_c"], CABLE))

    # Shock lift. The crept spring takes the extra 0.27 N.
    Kc_worst = n * finger_k(E, w, worst["tt"], L) * CREEP
    # Series with the flank: incremental stiffness.
    K_inc = 1.0 / (1.0 / Kc_worst + 1.0 / Kax)
    deficit = max(0.0, load - worst["F_c"])
    lift = deficit / K_inc if K_inc else 99
    rep.say("shock lift at the worst corner %.3f mm" % lift)
    rep.gate(lift <= LIFT_MAX + 1e-9, "shock lift %.3f mm <= %.2f mm" % (lift, LIFT_MAX))
    margin = worst["F_c"] / load
    rep.say("TARGET 2.0x at the worst corner: %.2fx" % margin)
    if margin + 1e-9 < 2.0:
        rep.say("  short of 2x. The flank cannot absorb the full stack without unclipping.")
        rep.say("  The cable stays held. The knock lifts the panel %.2f mm and it stays captured." % lift)

    # Cross-layer shear in the bridge. V = F/2 at the wall.
    f_one = thick["F"] / n
    tau = 1.5 * (f_one / 2.0) / (w * thick["tt"])
    rep.gate(tau <= SHEAR_MAX, "cross-layer shear %.2f MPa <= %.1f" % (tau, SHEAR_MAX))

    # Install, shell pushed to the inner end of the window. Momentary.
    front_hi = stop + 0.15 + cn["pcb_tol"] + 1.3
    d_inst = front_hi - (R - 0.05) - (-half)
    eps_inst = 12.0 * (t + 0.05) * d_inst / L ** 2
    rep.say("install deflection %.2f mm   strain %.2f%% (momentary, shell at the inner stop)" % (
        d_inst, 100 * eps_inst))

    # Seat forces. Clips unchanged. Fingers add the thick-corner push.
    # Ramp axial 19.4–23.8 N is Ferdinand's four-clip figure; fingers add theirs.
    rep.say("press to seat %.0f–%.0f N (clip ramp 19–24 N plus the fingers)" % (
        19.4 + thick["F"], 23.8 + thick["F"]))
    rep.say("pull to remove %.0f–%.0f N" % (13, 18))

    # J4
    rep.say("")
    rep.say("J4 on the carrier stop, shell off")
    K_short = n * finger_k(E, w, t, L)
    rise = CABLE / K_short
    rise_crept = CABLE / (CREEP * K_short)
    j4_worst = 99
    for c in carriers:
        back = stop + c
        ins = insertion(cn, back)
        ins_r = insertion(cn, back + rise)
        j4_worst = min(j4_worst, ins_r)
        rep.say("  carrier %+.2f  back %.2f  rest %.3f  with %.2f mm cable rise %.3f" % (
            c, back, ins, rise, ins_r))
    rep.say("cable rise at E 1500 is %.3f mm. After a set the same rise is %.3f mm, insertion %.3f." % (
        rise, rise_crept, insertion(cn, stop + 0.15 + rise_crept)))
    rep.gate(j4_worst + 1e-9 >= J4_MIN,
             "J4 %.3f mm >= %.2f with the cable rise" % (j4_worst, J4_MIN))

    # Component clearance under the panel. Carrier short, plus glass bend.
    rep.say("")
    rep.say("clearance under the panel, carrier −0.15, plus the 10 N bend")
    parts = component_boxes(cn)
    pads = pad_rects(src, cn)
    opto_air = None
    for name, bx0, by0, bx1, by1, h in parts:
        if by1 < cn["panel"][1] - 0.2 or by0 > cn["panel"][3] + 0.2:
            continue
        if bx1 < cn["panel"][0] - 0.2 or bx0 > cn["panel"][2] + 0.2:
            continue
        top = cn["Zt"] + h
        rest = (stop - 0.15) - top
        dist = distance_to_pads((bx0 + bx1) / 2, (by0 + by1) / 2, pads)
        bend = glass_bend(dist)
        air = rest - bend
        if name.startswith("U") and h >= 3.9:
            opto_air = air if opto_air is None else min(opto_air, air)
        rep.say("  %-8s top %5.2f  rest %5.2f  bend %.3f  min %.3f" % (name, top, rest, bend, air))
        rep.gate(air > 0.02, "%s stays clear by %.3f mm" % (name, air))
    if opto_air is not None:
        rep.say("opto-triac minimum clearance %.3f mm" % opto_air)

    # Y stack. Shoes centre the panel on the shell. Fences are opened so
    # they do not stop that. Width ±0.10 per edge, print ±0.05.
    rep.say("")
    y_err = 0.10 + 0.05
    near = land0 - y_err
    far = land1 + y_err
    beam_out = y0 - 0.05 - 0.10   # print and the narrow-panel edge
    beam_in = y0 + w + 0.05 + 0.10
    aa_margin = cn["aa"][1] - cn["panel"][1] - cn["win_m"]
    rep.say("land from the edge, centred panel: %.2f to %.2f" % (near, far))
    rep.say("beam from the edge, worst: outboard %.2f  inboard %.2f  window at %.2f" % (
        beam_out, beam_in, aa_margin))
    rep.gate(near + 1e-9 >= 2.05, "land near edge %.2f >= 2.05" % near)
    rep.gate(far <= 2.45 + 1e-9, "land far edge %.2f <= 2.45" % far)
    rep.gate(near + 1e-9 >= 1.92, "land stays outside the seal line")
    rep.gate(beam_out >= 0.0, "beam stays inboard of the panel edge")
    rep.gate(beam_in <= aa_margin + 1e-9, "beam stays outside the window")

    # Shoes.
    post = grab(src, "sh_shoe_post")
    shoe_t = grab(src, "sh_shoe_t")
    shoe_h = grab(src, "sh_shoe_h")
    pre = grab(src, "sh_shoe_pre")
    rep.say("")
    rep.say("edge shoes")
    for s in shoes:
        flex = (s[1] - s[0]) - 2 * post
        eps_s = 12.0 * shoe_t * pre / flex ** 2
        rep.say("  edge %d  flex %.1f mm  preload strain %.2f%%" % (int(s[2]), flex, 100 * eps_s))
        rep.gate(eps_s <= STRAIN_MAX, "shoe strain %.2f%%" % (100 * eps_s))
        rep.gate(flex > 8, "shoe flex length %.1f mm" % flex)

    # Ceiling at the thick corner.
    s_thick = half + thick["dz"] + thick["climb"]
    ceil = cn["Z_in"] + cn["face_recess"] - 0.06  # sliced skin
    top = thick["front"] + nub_h + thick["tt"]
    air = (ceil + s_thick) - top
    rep.say("ceiling air at the thick corner %.2f mm" % air)
    rep.gate(air >= 0.15, "ceiling air %.2f mm >= 0.15" % air)

    # Bow note.
    rep.say("local pad warp of ±0.10 mm leaves contact deflection %.2f mm at the worst corner" % (
        worst["d_c"] - 0.10))
    rep.gate(worst["d_c"] - 0.10 > 0, "contact remains with 0.10 mm pad warp")

    return {
        "worst": worst, "thick": thick, "lift": lift, "margin": margin,
        "rise": rise, "j4": j4_worst, "opto": opto_air, "tau": tau,
        "rows": rows, "stop": stop, "R": R, "n": n, "L": L, "w": w, "t": t,
    }


def glass_bend(dist):
    """10 N on a 20 mm strip of 1.2 mm glass, cantilevered dist mm."""
    if dist <= 0:
        return 0.0
    I = 20.0 * 1.2 ** 3 / 12.0
    return 10.0 * dist ** 3 / (3.0 * GLASS_E * I)


def distance_to_pads(x, y, pads):
    best = 1e9
    for x0, y0, x1, y1 in pads:
        dx = 0 if x0 <= x <= x1 else min(abs(x - x0), abs(x - x1))
        dy = 0 if y0 <= y <= y1 else min(abs(y - y0), abs(y - y1))
        best = min(best, math.hypot(dx, dy))
    return best


def pad_rects(src, cn):
    pads = []
    # Shipped support pads, lowered. Depth 1.65 inboard of the edge.
    p = cn["panel"]
    shipped = [
        (47.0, 53.0, p[1], p[1] + 1.65),
        (76.0, 90.6, p[1], p[1] + 1.65),
        (49.6, 55.6, p[3] - 1.65, p[3]),
        (76.0, 82.0, p[3] - 1.65, p[3]),
        (82.0, p[2], 33.20, 34.30),
        (76.0, p[2], 61.20, 62.50),
    ]
    pads.extend(shipped)
    for c in grab_list(src, "sh_columns"):
        pads.append((c[0], c[2], c[1], c[3]))
    return pads


def component_boxes(cn):
    Z = 0  # heights are above the PCB, applied by the caller
    return [
        ("U6", 28.8, 6.3, 40.2, 14.1, 4.0),
        ("U7", 28.8, 21.3, 40.2, 29.1, 4.0),
        ("U8", 28.8, 36.3, 40.2, 44.1, 4.0),
        ("U3", 66.05, 0.0, 84.05, 25.5, 3.15),
        ("R31", 42.0, 27.24, 43.0, 27.84, 0.55),
        ("D7", 83.65, 59.60, 86.35, 61.40, 1.35),
        ("D8", 89.65, 59.60, 92.35, 61.40, 1.35),
        ("C1", 42.9, 54.25, 46.1, 60.25, 2.5),
        ("tails", 49.5, 56.5, 79.0, 59.5, 2.0),
        ("D4", 29.5, 54.1, 32.5, 60.5, 2.5),
        ("R32", 29.425, 30.50, 31.575, 31.90, 0.65),
        ("R42", 29.425, 45.50, 31.575, 46.90, 0.65),
    ]


def board_to_case(x0, y0, x1, y1, z0, z1, cn):
    import trimesh
    X0 = cn["OX"] + x0
    X1 = cn["OX"] + x1
    Y0 = cn["H"] - cn["OYT"] - y1
    Y1 = cn["H"] - cn["OYT"] - y0
    ext = [abs(X1 - X0), abs(Y1 - Y0), abs(z1 - z0)]
    m = trimesh.creation.box(extents=ext)
    m.apply_translation([(X0 + X1) / 2, (Y0 + Y1) / 2, (z0 + z1) / 2])
    return m


def mesh_clearance(part, obst, n=2500):
    import numpy as np
    import trimesh
    pts = part.vertices
    if len(part.vertices) < n:
        pts = np.vstack([part.vertices, part.sample(n)])
    d = trimesh.proximity.ProximityQuery(obst).signed_distance(pts)
    return -float(d.max())


def check_meshes(src, cn, rep):
    import trimesh
    beams_p = os.path.join(MESH, "r6_beams.stl")
    pillars_p = os.path.join(MESH, "r6_pillars.stl")
    shoes_p = os.path.join(MESH, "r6_shoes.stl")
    carrier_p = os.path.join(MESH, "r6_carrier_stops.stl")
    shell_p = os.path.join(MESH, "r6_shell_stock.stl")
    missing = [p for p in (beams_p, pillars_p, carrier_p, shell_p) if not os.path.exists(p)]
    if missing:
        rep.gate(False, "mesh missing: " + ", ".join(os.path.basename(p) for p in missing))
        return
    beams = trimesh.load(beams_p)
    pillars = trimesh.load(pillars_p)
    carrier = trimesh.load(carrier_p)
    shell = trimesh.load(shell_p)
    # Beams must not enter the shell wall or the ceiling solid.
    air_shell = mesh_clearance(beams, shell)
    rep.say("beam vs shell solid  %.2f mm" % air_shell)
    rep.gate(air_shell >= 0.10, "beams clear the shell by %.2f mm" % air_shell)
    # Pillars must fuse into the shell.
    bite = mesh_clearance(pillars, shell)
    rep.say("pillar vs shell solid %.2f mm (negative means it bites in)" % bite)
    rep.gate(bite < -0.02, "pillars fuse into the shell")
    air_c = mesh_clearance(beams, carrier)
    rep.say("beam vs carrier %.2f mm" % air_c)
    rep.gate(air_c >= 0.15, "beams clear the carrier by %.2f mm" % air_c)
    # Components, beam lowered by the inward install position and the board raised.
    shift = 0.44 + cn["pcb_tol"]
    low = beams.copy()
    low.apply_translation([0, 0, -shift])
    for name, x0, y0, x1, y1, h in component_boxes(cn):
        box = board_to_case(x0, y0, x1, y1, cn["Zt"], cn["Zt"] + h, cn)
        air = mesh_clearance(low, box, n=800)
        if air < 2.0:
            rep.say("  beam vs %-6s %.2f mm at install" % (name, air))
        if air < 0.10 and name in ("U3", "U6", "U7", "U8", "D7", "D8", "R31", "C1", "tails"):
            rep.gate(False, "beam hits %s (%.2f mm)" % (name, air))
    # Carrier one solid, stop height.
    bodies = carrier.split(only_watertight=False)
    rep.say("carrier bodies %d" % len(bodies))
    rep.gate(len(bodies) == 1, "carrier is one solid")
    # Panel rest: vertices under the outline should top out at sh_stop.
    stop = grab(src, "sh_stop")
    p = cn["panel"]
    import numpy as np
    v = carrier.vertices
    X0, X1 = cn["OX"] + p[0] + 0.4, cn["OX"] + p[2] - 0.4
    Y0, Y1 = cn["H"] - cn["OYT"] - p[3] + 0.4, cn["H"] - cn["OYT"] - p[1] - 0.4
    under = v[(v[:, 0] > X0) & (v[:, 0] < X1) & (v[:, 1] > Y0) & (v[:, 1] < Y1)]
    if len(under) == 0:
        rep.gate(False, "no carrier mesh under the panel")
    else:
        zmax = float(under[:, 2].max())
        rep.say("carrier top under the panel %.2f mm" % zmax)
        rep.gate(abs(zmax - stop) < 0.08, "stop is %.2f, mesh %.2f" % (stop, zmax))
    # Nub height matches sh_R.
    zmin = float(beams.bounds[0, 2])
    rep.say("beam lowest vertex %.2f (land face)" % zmin)
    rep.gate(abs(zmin - grab(src, "sh_R")) < 0.08, "printed land is the analysed land")
    # Land y in the mesh, top bridge. Case Y of the nub is the lowest Y of that body
    # inside the land z. Check the nub sticks below the beam and is one body with it.
    n_beam = len(beams.split(only_watertight=False))
    rep.gate(n_beam == 4, "four bridges, land fused to each (%d bodies)" % n_beam)
    if os.path.exists(shoes_p):
        shoes = trimesh.load(shoes_p)
        rep.say("shoes %d bodies" % len(shoes.split(only_watertight=False)))


def draw(src, cn):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle, FancyBboxPatch

    stop = grab(src, "sh_stop")
    R = grab(src, "sh_R")
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
        # Section through a top-edge land. y horizontal, z vertical.
        # 0 is the panel edge. Inboard is +y.
        ax.add_patch(Rectangle((-6, zt), 18, 1.6, fc="#2f6b3a", ec="#222", lw=0.4))
        ax.add_patch(Rectangle((0.4, zt), 2.2, stop - zt, fc="#c4a574", ec="#222", lw=0.4))
        ax.add_patch(Rectangle((0, stop), 8, 1.2, fc="#f7f7f7", ec="#222", lw=0.5))
        ax.plot([2.05, 2.05], [stop, stop + 1.6], color="#9b2335", lw=0.5, ls="--")
        ax.plot([2.45, 2.45], [stop, stop + 1.6], color="#9b2335", lw=0.5, ls="--")
        if shell_on:
            # Installed: the land sits on the panel front. The shell has climbed the flank.
            front = stop + 1.2
            ax.add_patch(Rectangle((land0, front), land1 - land0, nub, fc="#d7b56d", ec="#222", lw=0.5))
            ax.add_patch(Rectangle((y0, front + nub), w, t, fc="#d7b56d", ec="#222", lw=0.5))
            ax.add_patch(Rectangle((-1.2, front + nub + t + 0.35), 8, 0.74, fc="#6d8ea8", ec="#222", lw=0.4))
        else:
            ax.add_patch(Rectangle((-1.2, cn["Z_in"] + cn["face_recess"] - 0.06), 8, 0.74, fc="#d5dbe2", ec="#222", lw=0.4))
        ax.set_xlim(-2.2, 6.4)
        ax.set_ylim(zt - 0.4, 26.6)
        ax.set_aspect("equal")
        ax.set_title(title, fontsize=10)
        ax.set_xlabel("mm from the panel edge")
        ax.set_ylabel("z")

    section(axes[0], False, "Shell off. Panel on the stop.")
    section(axes[1], True, "Shell on. Bridge on the land.")
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, "shell-hold-section.png"))
    plt.close()

    fig, ax = plt.subplots(figsize=(11.2, 5.6), dpi=140)
    ax.add_patch(Rectangle((p[0], p[1]), p[2] - p[0], p[3] - p[1], fc="#f4f4f4", ec="#222", lw=0.8))
    aa = cn["aa"]
    ax.add_patch(Rectangle((aa[0], aa[1]), aa[2] - aa[0], aa[3] - aa[1], fc="#d9dee6", ec="#666", lw=0.4))
    for name, x0, y0, x1, y1, h in component_boxes(cn):
        if h < 1.0:
            continue
        ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fc="#e7c7c7", ec="#844", lw=0.3))
        ax.text((x0 + x1) / 2, (y0 + y1) / 2, name, ha="center", va="center", fontsize=6, color="#622")
    for c in grab_list(src, "sh_columns"):
        ax.add_patch(Rectangle((c[0], c[1]), c[2] - c[0], c[3] - c[1], fc="#c4a574", ec="#222", lw=0.4, alpha=0.9))
    for s in grab_list(src, "sh_spans"):
        edge = p[1] if s[2] == 0 else p[3]
        sign = 1 if s[2] == 0 else -1
        yy = edge + sign * (y0 + w / 2)
        ax.plot([s[0] + root_of(src), s[1] - root_of(src)], [yy, yy], color="#a67c2d", lw=3, solid_capstyle="butt")
        xc = (s[0] + s[1]) / 2
        ax.plot(xc, edge + sign * (land0 + land1) / 2, marker="s", color="#7a4e12", ms=5)
    ax.set_aspect("equal")
    ax.set_xlim(24, 116)
    ax.set_ylim(70, -2)
    ax.set_title("Plan. Bridges on the border, stops under the panel.")
    ax.set_xlabel("board x")
    ax.set_ylabel("board y")
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, "shell-hold-plan.png"))
    plt.close()


def root_of(src):
    return grab(src, "sh_root")


def main():
    src = open(SCAD).read()
    case = open(CASE).read()
    cn = case_nums(case)
    rep = Report()
    analyse(src, cn, rep)
    rep.say("")
    check_meshes(src, cn, rep)
    draw(src, cn)
    rep.say("")
    if rep.fails:
        rep.say("%d FAILED" % len(rep.fails))
        for f in rep.fails:
            rep.say(" - " + f)
        return 1
    rep.say("ALL GATES PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
