#!/usr/bin/env python3
"""Revised pad retainer. Cable allowance 0.30 N, 35% retained, P2S-printable sections."""
import math

E = 1800.0
SHOCK = 5.0 * 9.80665 * 5.5e-3
RETAIN = 0.35
D_MIN, D_MAX = 0.20, 0.70
L, W, T = 18.0, 2.4, 3.00
NUB_H = 1.00
FREE = 23.55
CONTACT = 24.30
PLATE_T = 0.40          # land, ceiling-limited
FRAME_T = 0.60          # 3 layers, off the display panel
LIP_T = 0.60
CEILING = 25.51
POCKET = 25.88          # over the lip only
Z_OUT = 26.25
Z_PCB = 18.60


def k_of(w, t, length=L):
    return E * w * t ** 3 / (4.0 * length ** 3)


def ins(gap):
    return 14.40 - math.pi * (gap - 0.50) / 2.0 - 4.69


def margins(f_min):
    print("fpc    load   held   margin")
    for fpc in (0.06, 0.15, 0.30):
        load = SHOCK + fpc
        held = 4.0 * f_min * RETAIN
        print(" %.2f  %.3f  %.3f  %.2fx" % (fpc, load, held, held / load))


def main():
    print("shock %.4f  retain %.0f%%" % (SHOCK, 100 * RETAIN))
    print("tip air %.2f" % (FREE - NUB_H - T - Z_PCB))
    kt, kn, kk = k_of(W, T - 0.05), k_of(W, T), k_of(W, T + 0.05)
    # Web from the lip (y ~ -0.8) to the nub (y 2.50): 3.3 mm, 0.60 thick, 24 mm long.
    span, b, tw = 3.30, 24.0, FRAME_T
    kp = 3 * E * (b * tw ** 3 / 12.0) / span ** 3
    ks = 1.0 / (1.0 / kt + 1.0 / kp)
    f_min = ks * D_MIN
    print("k thin/nom/thick %.3f / %.3f / %.3f" % (kt, kn, kk))
    print("web k %.1f  series thin k %.3f  Fmin %.3f" % (kp, ks, f_min))
    print("thickness only, same d=0.20: F %.3f to %.3f  (%.0f%% to %.0f%%)" % (
        kt * D_MIN, kk * D_MIN, 100 * (T - 0.05) / T, 100 * (T + 0.05) / T))
    # ratio vs nominal thickness at fixed deflection
    print("force ratio at fixed d: thin %.3f  thick %.3f" % (
        (T - 0.05) ** 3 / T ** 3, (T + 0.05) ** 3 / T ** 3))
    margins(f_min)
    emax = 1.5 * (T + 0.05) * D_MAX / L ** 2
    einst = 1.5 * (T + 0.05) * (D_MAX + 0.15) / L ** 2
    print("strain thick dmax %.2f%%  install %.2f%%" % (100 * emax, 100 * einst))
    print("install one station, thick, d %.2f: %.2f N" % (
        D_MAX + 0.15, kk * (D_MAX + 0.15)))
    # lever: shelf y=-1.3, lip y=-0.8, nub y=2.50
    ys, yl, yn = -1.30, -0.80, 2.50
    ratio = (yn - ys) / (yl - ys)
    print("H/F %.2f  (lip between shelf and nub)" % ratio)
    f_hi = kk * D_MAX
    print("H at thick dmax %.1f N" % (ratio * f_hi))
    print("\ninsertion tall 1.1 %.3f  +0.05 rise %.3f" % (ins(4.70), ins(4.75)))
    lip_top = 24.50 + LIP_T
    print("lip top %.2f pocket %.2f skin %.2f" % (lip_top, POCKET, Z_OUT - POCKET))
    for dz in (0, 0.16, 0.26, 0.51):
        print(" dz %.2f plate %.2f lip %.2f" % (
            dz, (CEILING - dz) - (CONTACT + PLATE_T), (POCKET - dz) - lip_top))
    print("plate air dz0.51 +0.03 tilt %.2f" % (
        (CEILING - 0.51) - (CONTACT + PLATE_T) - 0.03))
    print("lip air dz0.51 +0.03 tilt %.2f" % ((POCKET - 0.51) - lip_top - 0.03))


if __name__ == "__main__":
    main()
