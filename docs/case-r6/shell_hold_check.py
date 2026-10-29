#!/usr/bin/env python3
"""Gates for the shell hold-down. Numbers come from the scratch scad and the case file."""
import math
import os
import re

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SCAD = os.path.join(ROOT, "case", "scratch", "r6_shell_hold.scad")
CASE = os.path.join(ROOT, "case", "thermostat_case.scad")


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
    fpc_free = grab(case, "fpc_free")
    fpc_z = grab(case, "fpc_z_ret")
    mouth = grab(case, "mouth_x")
    straight = panel[2] - mouth
    E = 1800.0
    L = grab(src, "sh_L")
    t = grab(src, "sh_t")
    nub_h = grab(src, "sh_nub_h")
    free = grab(src, "sh_free")
    stop = grab(src, "sh_stop")
    shock = 5.0 * 9.80665 * 5.5e-3
    cable = 0.30
    need = 2.0 * (shock + cable)
    # snap, copied from the case formulas
    wall, snap_w, snap_defl = 2.4, 11.0, 0.50
    snap_L = (Zt + 4.45 + 1.2) - 3.2
    snap_F = 3 * E * (snap_w * wall ** 3 / 12) * snap_defl / snap_L ** 3

    def k_of(w, th):
        return E * w * th ** 3 / (4.0 * L ** 3)

    def ins(back):
        gap = back - Zt
        return fpc_free - math.pi * (gap - fpc_z) / 2.0 - straight

    print("Z_pcb_t %.2f  shock %.3f  need %.3f N" % (Zt, shock, need))
    print("snap one clip %.2f N, four %.2f N" % (snap_F, 4 * snap_F))
    print("free nub %.2f  backstop %.2f  nominal compression %.2f" % (free, stop, free - stop))
    print()
    print("preload if the shell has pushed the panel onto the backstop")
    print("pad   dmin  dmax  held   margin  strain  Fmax4")
    worst = None
    for w, name in ((2.80, "top"), (2.50, "bot")):
        for pe in (-0.05, 0.0, 0.05):
            dmin = (free - stop) + pe - 0.0
            # pad error moves the nub. Thin pad lowers it.
            d = (free + pe) - stop
            kk = k_of(w, t + pe)
            # four leaves, use the narrower k for the held total at this pad error
        # report the thin and thick corners for each width, then the mixed set
    widths = (2.80, 2.80, 2.50, 2.50)
    for pe, label in ((-0.05, "thin pad"), (0.05, "thick pad")):
        ds = [(free + pe) - stop]
        held = 0.0
        fmax = 0.0
        strains = []
        for w in widths:
            d = (free + pe) - stop
            kk = k_of(w, t + (0.05 if pe > 0 else -0.05) if False else t + pe)
            held += kk * d * 0.35
            fmax += kk * d
            strains.append(1.5 * (t + pe) * d / L ** 2)
        print("  %-10s d %.2f  held %.3f  margin %.2fx  strain %.2f%%  spring %.2f N" % (
            label, (free + pe) - stop, held, held / need, 100 * max(strains), fmax))
    print()
    print("J4, shell off, panel on the free nub")
    print("carrier  back   gap   inserted")
    for c in (-0.15, 0.0, 0.15):
        for pe in (-0.05, 0.05):
            back = free + c + pe
            print("  %+5.2f %+5.2f  %.2f  %.2f  %.3f" % (c, pe, back, back - Zt, ins(back)))
    print()
    print("opto air when the panel is on the backstop")
    opto = Zt + 4.0
    for c in (-0.15, 0.0, 0.15):
        air = (stop + c) - opto
        print("  carrier %+5.2f  stop %.2f  air %.2f" % (c, stop + c, air))
    print()
    print("shell rib tip nominal %.2f (backstop + 1.20)" % (stop + 1.20))
    print("dz    carrier  t     front   rib    reach")
    rib0 = stop + 1.20
    lost = 0
    total = 0
    for dz in (0.0, 0.16, 0.26, 0.51):
        for c in (-0.15, 0.0, 0.15):
            for tp in (1.1, 1.2, 1.3):
                front = free + c + tp
                rib = rib0 + dz
                reach = rib <= front + 1e-9
                total += 1
                if not reach:
                    lost += 1
                if dz in (0.0, 0.51) and c in (-0.15, 0.15) and tp in (1.1, 1.3):
                    print("  %.2f  %+5.2f  %.1f  %.2f  %.2f  %s" % (
                        dz, c, tp, front, rib, "touch" if reach else "MISS"))
    print("rib misses %d of %d corners" % (lost, total))
    # air of the recess ceiling, sliced skin 0.80, against the highest front
    z_out = Zt + 4.45 + 1.2 + 2.0
    ceil = z_out - 0.80
    front_hi = free + 0.15 + 1.3
    print("sliced recess ceiling %.2f  highest shell-off front %.2f  air %.2f" % (
        ceil, front_hi, ceil - front_hi))
    print("travel once the panel is on the backstop: 0")
    print("10 N press ends on the backstop, %.2f above the opto at carrier -0.15" % (
        (stop - 0.15) - opto))
    clash()
    draw()


def clash():
    # name, x0, x1, y0, y1, top above PCB
    obs = [
        ("U7", 28.8, 40.2, 21.3, 29.1, 4.0),
        ("U8", 28.8, 40.2, 36.3, 44.1, 4.0),
        ("U6", 28.8, 40.2, 6.3, 14.1, 4.0),
        ("U3", 66.05, 84.05, 0.0, 25.5, 3.15),
        ("R31", 42.0, 43.0, 27.24, 27.84, 0.55),
        ("D7", 83.65, 86.35, 59.60, 61.40, 1.35),
        ("D8", 89.65, 92.35, 59.60, 61.40, 1.35),
        ("C1", 42.9, 46.1, 54.25, 60.25, 2.5),
        ("C41", 42.9, 46.1, 54.25, 60.25, 2.5),
        ("tails", 49.5, 79.0, 56.5, 59.5, 2.0),
        ("C5", 24.8, 26.4, 61.2, 62.4, 0.90),
    ]
    leaves = [(48, 26.24, 1, 2.80), (86, 26.24, 1, 2.80),
              (58, 62.94, -1, 2.50), (93, 62.94, -1, 2.50)]
    z0 = 19.65
    print("\nleaf vs parts")
    bad = 0
    for root, edge, dire, w in leaves:
        x0, x1 = root, root + 4 + 10 + 1.4
        if dire > 0:
            y0, y1 = edge, edge + w
        else:
            y0, y1 = edge - w, edge
        for n, a, b, c, d, h in obs:
            top = 18.60 + h
            if x1 > a and x0 < b and y1 > c and y0 < d and z0 < top:
                bad += 1
                print("  CLASH", root, n)
    print("  leaf clashes", bad)
    stops = [(41.2, 27.6, 48.5, 30.2), (78, 27.2, 82.5, 30), (50, 60.7, 55, 62.6),
             (76, 60.8, 80.5, 62.7), (43.5, 37.5, 51.2, 41.5)]
    bad = 0
    for x0, y0, x1, y1 in stops:
        for n, a, b, c, d, h in obs:
            if x1 > a and x0 < b and y1 > c and y0 < d and 19.40 < 18.60 + h:
                bad += 1
                print("  STOP", x0, n)
    print("  stop clashes", bad)


def draw():
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle
    out = os.path.dirname(__file__)

    def box(ax, x, y, w, h, fc):
        ax.add_patch(Rectangle((x, y), w, h, facecolor=fc, edgecolor="#222", lw=0.6))

    fig, axes = plt.subplots(1, 2, figsize=(11.5, 5.2), dpi=140)
    # section, y inboard to the right, z up. Top edge.
    for ax, title, shell_on in (
        (axes[0], "Shell off. Panel sits on the leaves.", False),
        (axes[1], "Shell on. Rib holds the panel on the stop.", True),
    ):
        box(ax, 0, 18.60, 6, 1.60, "#2f6b3a")          # pcb
        box(ax, 0.2, 20.40, 2.6, 2.50, "#d7b56d")      # leaf, inboard of the edge
        box(ax, 2.05, 22.25, 0.40, 0.80, "#c4a574")    # nub
        if shell_on:
            box(ax, 0, 22.90, 5, 1.20, "#f4f4f4")      # panel on the stop
            box(ax, 2.05, 24.10, 0.40, 1.41, "#6d8ea8")  # rib
            box(ax, -0.4, 25.51, 6, 0.74, "#6d8ea8")
        else:
            box(ax, 0, 23.05, 5, 1.20, "#f4f4f4")
            box(ax, -0.4, 25.51, 6, 0.74, "#d0d5da")
        box(ax, 0.4, 20.40, 1.6, 2.50, "#b08968")      # backstop hint
        ax.set_xlim(-0.8, 6.2)
        ax.set_ylim(18.2, 26.6)
        ax.set_aspect("equal")
        ax.axis("off")
        ax.set_title(title, loc="left", fontsize=10)
    fig.tight_layout()
    fig.savefig(os.path.join(out, "shell-hold-section.png"), bbox_inches="tight", facecolor="white")
    plt.close()

    fig, ax = plt.subplots(figsize=(12.6, 6.2), dpi=140)
    box(ax, 29.46, 26.24, 79.0, 36.70, "#f7f7f7")
    for x, y, w, h in (
        (28.8, 21.3, 11.4, 7.8), (28.8, 36.3, 11.4, 7.8),
        (66.05, 0, 18, 25.5), (83.65, 59.6, 2.7, 1.8), (89.65, 59.6, 2.7, 1.8),
        (42.0, 27.24, 1.0, 0.6), (42.9, 54.25, 3.2, 6.0), (49.5, 56.5, 29.5, 3.0),
    ):
        box(ax, x, y, w, h, "#f6d0d0")
    for root, edge, dire, w in (
        (48, 26.24, 1, 2.80), (86, 26.24, 1, 2.80),
        (58, 62.94, -1, 2.50), (93, 62.94, -1, 2.50),
    ):
        y0 = edge if dire > 0 else edge - w
        box(ax, root, y0, 16, w, "#e7d7ad")
    ax.set_xlim(20, 120)
    ax.set_ylim(70, 4)
    ax.set_aspect("equal")
    ax.axis("off")
    ax.set_title("Plan. Leaves in tan, parts they must miss in red.", loc="left", fontsize=11)
    fig.tight_layout()
    fig.savefig(os.path.join(out, "shell-hold-plan.png"), bbox_inches="tight", facecolor="white")
    plt.close()
    print("wrote pngs")


if __name__ == "__main__":
    main()
