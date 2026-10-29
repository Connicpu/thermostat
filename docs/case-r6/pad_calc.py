#!/usr/bin/env python3
"""Numbers for the under-panel pad retainer. Not the case model."""
import math

E = 1800.0  # N/mm^2
G = 9.80665
M_PANEL = 5.5e-3  # kg
FPC_FREE = 14.40
FPC_STRAIGHT = 108.46 - 103.77  # 4.69
Z_PCB_T = 18.60
OPTO_TOP = Z_PCB_T + 4.00  # 22.60
Z_IN = 24.25
FACE_RECESS = 1.26
CEILING = Z_IN + FACE_RECESS  # 25.51
Z_OUT = 26.25
DZ = (0.0, 0.16, 0.26, 0.51)

# Contact plane is the carrier shoulder. The plate thickness is above it.
CONTACT = 24.30
PLATE_T = 0.40
WASHER_T = 0.20
# Local ceiling over the outboard washer only. Skin left on the 2.00 mm face.
POCKET = 25.80

# Pad. Free nub is 0.25 mm above the 1.1 back, so a 0.05 mm thin pad
# still has 0.20 mm of deflection there.
L = 12.7
W = 3.3
T = 1.40
NUB_H = 1.00
D_NOM = {1.1: 0.25, 1.2: 0.35, 1.3: 0.45}
# Thin pad: -0.05 mm free height and t' = 1.35. Thick pad: +0.05 and t' = 1.45.
D_THIN = {1.1: 0.20, 1.2: 0.30, 1.3: 0.40}
D_THICK = {1.1: 0.30, 1.2: 0.40, 1.3: 0.50}


def ins(gap, z_leg=0.50):
    return FPC_FREE - math.pi * (gap - z_leg) / 2.0 - FPC_STRAIGHT


def k_of(w, t, length):
    return E * w * t**3 / (4.0 * length**3)


def main():
    shock = 5.0 * G * M_PANEL
    fpc = 0.06
    load = shock + fpc
    target = 2.0 * load
    print("shock %.4f N" % shock)
    print("fpc design %.2f N" % fpc)
    print("load %.4f N  target relaxed %.4f N" % (load, target))
    print("per pad at the thin 1.1 corner, before 50%%: %.4f N" % (target / 2.0))

    print("\nbacks and gaps at contact %.2f" % CONTACT)
    for th in (1.1, 1.2, 1.3):
        back = CONTACT - th
        gap = back - Z_PCB_T
        print(
            "  %.1f back %.2f gap %.2f ins %.3f opto %.2f"
            % (th, back, gap, ins(gap), back - OPTO_TOP)
        )
    print("  tall 1.1 gap %.2f ins %.3f" % (4.70, ins(4.70)))
    print("  short 1.3 back %.2f opto %.2f" % (22.90, 22.90 - OPTO_TOP))

    print("\npad L %.1f w %.1f t %.2f" % (L, W, T))
    for name, t, table in (
        ("nominal t", T, D_NOM),
        ("thin t", 1.35, D_THIN),
        ("thick t", 1.45, D_THICK),
    ):
        k = k_of(W, t, L)
        print("  %s k %.4f N/mm" % (name, k))
        for th, d in table.items():
            f = k * d
            e = 1.5 * t * d / L**2
            rel = 4.0 * f * 0.5
            print(
                "    %.1f d %.2f e %.2f%% F %.3f  relaxed %.3f  xload %.2f"
                % (th, d, 100 * e, f, rel, rel / load)
            )

    k = k_of(W, T, L)
    # Plate is a cantilever from the outboard clamp to the land centre.
    span = 2.45
    i_plate = 16.0 * PLATE_T**3 / 12.0
    k_plate = 3.0 * E * i_plate / span**3
    print("\nplate cantilever L %.2f k %.1f N/mm" % (span, k_plate))
    k_thin = k_of(W, 1.35, L)
    k_ser = 1.0 / (1.0 / k_thin + 1.0 / k_plate)
    f_worst = k_ser * 0.20
    print("delivered at thin 1.1: %.3f N/pad  relaxed %.3f  xload %.2f" % (
        f_worst, 4 * f_worst * 0.5, 4 * f_worst * 0.5 / load))

    # Install: +0.15 mm to slide, thick pad, 1.3 panel. Two pads, one edge.
    d_inst = 0.50 + 0.15
    e_inst = 1.5 * 1.45 * d_inst / L**2
    f_edge = 2.0 * k_of(W, 1.45, L) * d_inst
    print("install worst e %.2f%%  one edge %.2f N" % (100 * e_inst, f_edge))
    f_edge_nom = 2.0 * k * (0.45 + 0.15)
    print("install nominal 1.3 one edge %.2f N" % f_edge_nom)

    # Post. 8 x 1.6 mm, lever 0.80 mm, unrelaxed thick 1.3.
    f_post = k_of(W, 1.45, L) * 0.50
    lever = 0.80
    b, th = 8.0, 1.6
    moment = f_post * lever
    i_post = b * th**3 / 12.0
    sig = moment * (th / 2.0) / i_post
    print("post F %.3f M %.2f sig %.2f MPa (%.3f%%)" % (f_post, moment, sig, 100 * sig / E))

    # Ledge check from the fence-leaf section: w 1.60, t 1.00, overhang 0.70.
    k_ledge = 3.0 * E * (1.60 * 1.00**3 / 12.0) / 0.70**3
    print("ledge 1.60 x 1.00 x 0.70 k %.1f N/mm" % k_ledge)

    print("\nshell")
    plate_top = CONTACT + PLATE_T
    print("  plate top nominal %.2f  thick %.2f" % (plate_top, plate_top + 0.05))
    for dz in DZ:
        air = (CEILING - dz) - (plate_top + 0.05)
        print("    border plate dz %.2f air %.3f" % (dz, air))
    washer_top = plate_top + 0.05 + WASHER_T + 0.05  # thick plate and thick washer
    print("  washer top worst %.2f  pocket %.2f  skin %.2f" % (
        washer_top, POCKET, Z_OUT - POCKET))
    for dz in DZ:
        air = (POCKET - dz) - washer_top
        print("    washer dz %.2f air %.3f" % (dz, air))

    # Chamfer: window edge 3.32 inboard at z<=25.10, then 1.6 mm out over 1.15 mm.
    print("\nchamfer z where the window reaches a given inboard y")
    for y in (2.00, 2.05, 2.20, 2.40, 3.32):
        if y >= 3.32:
            z = 25.10
        else:
            z = 25.10 + (3.32 - y) / 1.6 * 1.15
        print("  y %.2f  z %.2f" % (y, z))

    # Band. Edge vs nominal: -0.20 against the fence, +0.40 narrow panel shifted in.
    ps_max = 0.40 + 1.52
    aa_min = -0.20 + 3.32
    print("\nband PS max %.2f  AA-0.5 min %.2f  window %.2f" % (ps_max, aa_min, 3.32))
    for outer, inner in ((2.05, 2.45),):
        print(
            "  land %.2f–%.2f  PS margin %.2f  AA margin %.2f  window margin %.2f"
            % (outer, inner, outer - ps_max, aa_min - inner, 3.32 - inner)
        )
    lift = 0.40 * math.tan(math.radians(5.0))
    print("5 deg across the 0.40 mm bridge lifts %.3f mm" % lift)

    # FPC elastic bound. Kapton HN modulus 2.5 GPa. The bend is past yield.
    t_pi, w_pi = 0.30, 12.50
    r = (4.45 - 0.50) / 2.0
    inertia = w_pi * t_pi**3 / 12.0
    e_pi = 2500.0
    ei = e_pi * inertia
    m_el = ei / r
    f_el = math.pi * ei / (4.0 * r**2)
    m_y = ei * (0.02 / (t_pi / 2.0))
    m_p = 1.5 * m_y
    f_p = math.pi * m_p / (4.0 * r)
    print("\nFPC R %.4f  outer strain %.2f%%" % (r, 100 * (t_pi / 2.0) / r))
    print("  elastic M %.1f N.mm  F %.1f N" % (m_el, f_el))
    print("  My %.1f  Mp %.1f  Fp %.1f" % (m_y, m_p, f_p))

    # x1.59 comparison, not the design. Force scales with t^3, strain with t.
    print("\nrejected air-layer thickness, not used")
    for scale, label in ((1.0, "as drawn"), (1.59 ** (1.0 / 3.0), "t x1.59^(1/3)")):
        tt = T * scale
        kk = k_of(W, tt, L)
        print("  %s t %.3f F@0.20 %.3f e@0.50 %.2f%%" % (
            label, tt, kk * 0.20, 100 * 1.5 * tt * 0.50 / L**2))

    # Option 2 leaf, same thin-corner force, width limited to the outboard strip.
    print("\noption 2")
    # epsilon 1% at delta 0.50, F = k*0.20 = 0.34
    # t = 0.01 * L^2 / (1.5 * 0.50)
    target_f = 0.34
    for w in (1.6,):
        # F = E*w*t^3/(4*L^3)*0.20, t = 0.01*L^2/(0.75)
        # t/L^2 = 0.013333
        # F = 1800*w*(0.013333*L^2)^3/(4*L^3)*0.20
        coeff = 1800.0 * (0.013333**3) / 4.0 * 0.20
        # F = coeff * w * L^3
        length = (target_f / (coeff * w)) ** (1.0 / 3.0)
        t = 0.013333 * length**2
        print("  w %.1f L %.2f t %.2f" % (w, length, t))


if __name__ == "__main__":
    main()
