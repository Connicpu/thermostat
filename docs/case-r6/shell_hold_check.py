#!/usr/bin/env python3
"""Gates for the shell-finger hold-down. Margin is held / 0.57 N."""
import math
import os
import re

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SCAD = os.path.join(ROOT, "case", "scratch", "r6_shell_hold.scad")
CASE = os.path.join(ROOT, "case", "thermostat_case.scad")
OUT = os.path.dirname(__file__)


def grab(text, name):
    m = re.search(r"(?m)^%s\s*=\s*([0-9.]+)\s*;" % name, text)
    if not m:
        raise SystemExit("missing " + name)
    return float(m.group(1))


def main():
    case = open(CASE).read()
    src = open(SCAD).read()
    Zt = grab(case, "plate_t") + grab(case, "standoff_h") + grab(case, "pcb_t")
    panel = [float(x) for x in re.search(r"panel = \[([0-9., ]+)\]", case).group(1).split(",")]
    straight = panel[2] - grab(case, "mouth_x")
    fpc_free = grab(case, "fpc_free")
    fpc_z = grab(case, "fpc_z_ret")
    E = 1800.0
    L = grab(src, "sh_L")
    w = grab(src, "sh_w")
    t = grab(src, "sh_t")
    stop = grab(src, "sh_stop")
    R = grab(src, "sh_R")
    shock = 5.0 * 9.80665 * 5.5e-3
    load = 0.30 + shock
    wall, snap_w, snap_defl = 2.4, 11.0, 0.50
    snap_L = (Zt + 4.45 + 1.2) - 3.2
    snap_F = 3 * E * (snap_w * wall ** 3 / 12.0) * snap_defl / snap_L ** 3
    k_lo = E * w * (t - 0.05) ** 3 / (4.0 * L ** 3)
    k_hi = E * w * (t + 0.05) ** 3 / (4.0 * L ** 3)

    def delta(c, tp, dz, fe):
        # fe > 0 is a thick finger: the free tip is lower, so the bend is larger.
        return (stop - R) + c + tp - dz + fe

    def ins(back):
        gap = back - Zt
        return fpc_free - math.pi * (gap - fpc_z) / 2.0 - straight

    dzs = (-0.10, -0.05, 0.05, 0.10)
    cs = (-0.15, 0.0, 0.15)
    ts = (1.1, 1.2, 1.3)
    corners = [(dz, c, tp) for dz in dzs for c in cs for tp in ts]
    deltas = [delta(c, tp, dz, 0.0) for dz, c, tp in corners]
    d_lo = min(delta(c, tp, dz, -0.05) for dz, c, tp in corners)
    d_hi = max(delta(c, tp, dz, 0.05) for dz, c, tp in corners)
    held = 4.0 * k_lo * d_lo * 0.35
    f_hi = 4.0 * k_hi * d_hi
    eps = 1.5 * (t + 0.05) * d_hi / L ** 2
    print("load %.3f N   2x is %.3f N held" % (load, 2 * load))
    print("36 corners, deflection %.2f to %.2f mm" % (min(deltas), max(deltas)))
    print("with finger ±0.05: %.2f to %.2f" % (d_lo, d_hi))
    print("held %.3f N   margin %.2fx" % (held, held / load))
    print("thick spring force %.2f N   clip hold %.2f N   left %.2f N" % (
        f_hi, 4 * snap_F, 4 * snap_F - f_hi))
    print("strain %.2f%%" % (100 * eps))
    print("clip snap %.2f N each, %.2f N for four" % (snap_F, 4 * snap_F))
    print("finger press to seat, worst corner %.1f N" % (4 * snap_F + f_hi))
    print("corners with no contact %d" % sum(1 for d in deltas if d <= 0))
    print()
    print("J4, shell off, panel on the backstop")
    for c in cs:
        back = stop + c
        print("  carrier %+5.2f  back %.2f  inserted %.3f" % (c, back, ins(back)))
    opto = Zt + 4.0
    print("opto air at carrier -0.15: %.2f" % ((stop - 0.15) - opto))
    z_out = Zt + 4.45 + 1.2 + 2.0
    ceil = z_out - 0.80
    # beam top at the thick corner sits sh_tab above the panel front
    front_hi = stop + 0.15 + 1.3
    beam_top = front_hi + 0.60
    print("sliced ceiling %.2f  beam top at tight corner %.2f  air %.2f" % (
        ceil, beam_top, ceil - beam_top))
    clash()
    draw()


def clash():
    obs = [
        ("U7", 28.8, 40.2, 21.3, 29.1, 4.0),
        ("U8", 28.8, 40.2, 36.3, 44.1, 4.0),
        ("U6", 28.8, 40.2, 6.3, 14.1, 4.0),
        ("U3", 66.05, 84.05, 0.0, 25.5, 3.15),
        ("R31", 42.0, 43.0, 27.24, 27.84, 0.55),
        ("D7", 83.65, 86.35, 59.60, 61.40, 1.35),
        ("D8", 89.65, 92.35, 59.60, 61.40, 1.35),
        ("C1", 42.9, 46.1, 54.25, 60.25, 2.5),
        ("tails", 49.5, 79.0, 56.5, 59.5, 2.0),
    ]
    fingers = [(42, 26.24, 1), (76, 26.24, 1), (42, 62.94, -1), (76, 62.94, -1)]
    # beam is outboard, bottom about 22.3 when the tip is lowest
    bad = 0
    for root, edge, dire in fingers:
        x0, x1 = root, root + 4 + 28
        y_in = edge - dire * 0.40
        y_out = edge - dire * (0.40 + 2.40)
        y0, y1 = min(y_in, y_out), max(y_in, y_out)
        for n, a, b, c, d, h in obs:
            if x1 > a and x0 < b and y1 > c and y0 < d and 22.2 < 18.60 + h:
                bad += 1
                print("FINGER", root, n)
    stops = [(41.2, 27.6, 48.5, 30.2), (78, 27.2, 82.5, 30), (50, 60.7, 55, 62.6),
             (76, 60.8, 80.5, 62.7), (43.5, 37.5, 51.2, 41.5)]
    for x0, y0, x1, y1 in stops:
        for n, a, b, c, d, h in obs:
            if x1 > a and x0 < b and y1 > c and y0 < d and 19.40 < 18.60 + h:
                bad += 1
                print("STOP", x0, n)
    print("clashes", bad)


def draw():
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle

    def box(ax, x, y, w, h, fc):
        ax.add_patch(Rectangle((x, y), w, h, facecolor=fc, edgecolor="#222", lw=0.6))

    fig, axes = plt.subplots(1, 2, figsize=(11.6, 5.4), dpi=140)
    for ax, title, shell_on in (
        (axes[0], "Shell off. Panel on the backstop.", False),
        (axes[1], "Shell on. Finger on the land.", True),
    ):
        box(ax, -1.2, 18.60, 6.2, 1.60, "#2f6b3a")
        box(ax, 0.3, 20.6, 2.2, 2.30, "#b08968")
        box(ax, 0.0, 22.90, 4.6, 1.20, "#f4f4f4")
        if shell_on:
            box(ax, -2.8, 22.90, 2.4, 2.40, "#d7b56d")
            box(ax, -0.4, 24.10, 2.85, 0.60, "#d7b56d")
            box(ax, -3.2, 25.51, 7.4, 0.74, "#6d8ea8")
        else:
            box(ax, -3.2, 25.51, 7.4, 0.74, "#d0d5da")
        ax.plot([2.05, 2.05], [23.9, 25.2], color="#9b2335", lw=0.6, ls="--")
        ax.plot([2.45, 2.45], [23.9, 25.2], color="#9b2335", lw=0.6, ls="--")
        ax.set_xlim(-3.6, 5.4)
        ax.set_ylim(18.2, 26.6)
        ax.set_aspect("equal")
        ax.axis("off")
        ax.set_title(title, loc="left", fontsize=10)
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, "shell-hold-section.png"),
                bbox_inches="tight", facecolor="white")
    plt.close()

    fig, ax = plt.subplots(figsize=(12.6, 6.2), dpi=140)
    box(ax, 29.46, 26.24, 79.0, 36.70, "#f7f7f7")
    for x, y, ww, h in (
        (28.8, 21.3, 11.4, 7.8), (28.8, 36.3, 11.4, 7.8),
        (66.05, 0, 18, 25.5), (83.65, 59.6, 2.7, 1.8), (89.65, 59.6, 2.7, 1.8),
        (42.0, 27.24, 1.0, 0.6), (42.9, 54.25, 3.2, 6.0), (49.5, 56.5, 29.5, 3.0),
    ):
        box(ax, x, y, ww, h, "#f6d0d0")
    for root, edge, dire in ((42, 26.24, 1), (76, 26.24, 1), (42, 62.94, -1), (76, 62.94, -1)):
        y0 = edge - dire * 2.80
        box(ax, root, min(y0, edge - dire * 0.40), 32, 2.40, "#e7d7ad")
    ax.set_xlim(20, 120)
    ax.set_ylim(72, 2)
    ax.set_aspect("equal")
    ax.axis("off")
    ax.set_title("Plan. Fingers in tan, outboard of the edge.", loc="left", fontsize=11)
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, "shell-hold-plan.png"),
                bbox_inches="tight", facecolor="white")
    plt.close()


if __name__ == "__main__":
    main()
