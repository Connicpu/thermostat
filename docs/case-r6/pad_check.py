#!/usr/bin/env python3
"""Station and stack checks for the revised pad retainer. Not a case_fit gate."""
import math
import os

SHOCK = 5 * 9.80665 * 5.5e-3
# nub x, edge y, inboard sign (+1 top edge, -1 bottom edge)
STATIONS = ((64.0, 26.24, 1), (100.0, 26.24, 1), (72.0, 62.94, -1), (100.0, 62.94, -1))
# beam and plate in local y, then mapped
BEAM_Y = (1.00, 3.30)
PLATE_X = (-16.0, 6.0)   # 22 mm, same as the seat mesh
BEAM_X = (-18.0, 0.0)
ROOT_X = (-22.0, -18.0)

# name, x0, x1, y0, y1
# name, x0, x1, y0, y1, z top above the PCB top (18.60)
OBS = [
    ("U7 opto", 28.8, 40.2, 21.3, 29.1, 4.0),
    ("U8 opto", 28.8, 40.2, 36.3, 44.1, 4.0),
    ("U3 ESP32", 66.05, 84.05, 0.0, 25.5, 3.15),
    ("C1/C41", 42.9, 46.1, 54.25, 60.25, 2.5),
    ("C42", 47.75, 50.25, 54.4, 57.6, 2.5),
    ("L1", 21.1, 27.1, 53.5, 59.5, 4.5),
    ("C5", 24.8, 26.4, 61.2, 62.4, 0.9),
    ("D4", 29.5, 32.5, 54.1, 60.5, 1.0),
    ("J1/C43 tails", 49.5, 79.0, 56.5, 59.5, 2.0),
    ("R32", 29.425, 31.575, 30.50, 31.90, 0.65),
    ("R42", 29.425, 31.575, 45.50, 46.90, 0.65),
    ("J4", 98.5, 103.9, 34.8, 53.0, 2.0),
]


def box(nub, edge, dire, x0, x1, y0, y1):
    return (nub + x0, nub + x1,
            edge + dire * y1 if dire < 0 else edge + y0,
            edge + dire * y0 if dire < 0 else edge + y1)


def hits(b, z0, z1):
    x0, x1, y0, y1 = b
    if y0 > y1:
        y0, y1 = y1, y0
    out = []
    for n, a, c, d, e, zt in OBS:
        if z1 <= 18.60 or z0 >= 18.60 + zt:
            continue
        if x1 > a and x0 < c and y1 > d and y0 < e:
            out.append(n)
    return out


def main():
    bad = 0
    for nub, edge, dire in STATIONS:
        side = "top" if dire > 0 else "bot"
        for label, xs, ys, zs in (
            ("beam", BEAM_X, BEAM_Y, (19.55, 22.55)),
            ("plate", PLATE_X, (-1.05, 3.20), (23.90, 25.10)),
            ("root", ROOT_X, (-0.50, 1.10), (19.55, 23.77)),
        ):
            b = box(nub, edge, dire, xs[0], xs[1], ys[0], ys[1])
            h = hits(b, zs[0], zs[1])
            if h:
                bad += 1
                print("CLASH", side, "nub", nub, label, h)
            else:
                print("clear", side, "nub", nub, label)
    print("clashes", bad)
    # U7 bridge: stop top 22.75, U7 top 22.60, posts beside U7 not over it.
    print("U7 air when the panel is on the stop %.2f" % (22.75 - 22.60))
    shell = 2.16
    print("carrier outer to shell %.2f" % (shell - 1.90))


if __name__ == "__main__":
    main()
