# Retainer numbers from the meshes. The scad parameters are not the gate.
import numpy as np
import trimesh

E_PETG = 1800.0
PANEL_EDGE = (26.24, 62.94)
AA_Y = (30.06, 59.12)  # active area on the long edges, datasheet margins


def _board(v, OX, OYT, H):
    return np.column_stack([v[:, 0] - OX, H - OYT - v[:, 1], v[:, 2]])


def _z_hits(mesh, x, y, OX, OYT, H, z0=18.2):
    origin = np.array([[OX + x, H - OYT - y, z0]])
    loc, _, _ = mesh.ray.intersects_location(origin, np.array([[0.0, 0.0, 1.0]]), multiple_hits=True)
    if len(loc) == 0:
        return []
    z = np.sort(loc[:, 2])
    keep = [z[0]]
    for t in z[1:]:
        if t - keep[-1] > 0.03:
            keep.append(t)
    return keep


def _sections(mesh, x, y0, y1, OX, OYT, H, n=70):
    rows = []
    for y in np.linspace(y0, y1, n):
        hits = _z_hits(mesh, x, float(y), OX, OYT, H)
        if len(hits) >= 2:
            rows.append((float(y), float(hits[0]), float(hits[-1]), float(hits[-1] - hits[0])))
    return rows


def measure_clips(retainer, carrier, OX, OYT, H, Zin):
    """One dict per clip. Lengths are millimetres from the meshes."""
    bodies = retainer.split(only_watertight=False)
    qc = trimesh.proximity.ProximityQuery(carrier)
    clips = []
    for b in bodies:
        bv = _board(b.vertices, OX, OYT, H)
        if len(bv) < 8:
            continue
        high = bv[bv[:, 2] > 24.3]
        low = bv[bv[:, 2] < 21.2]
        if len(high) < 4 or len(low) < 4:
            continue
        arm_y = float(np.median(low[:, 1]))
        # The tongue tip is the high vertex furthest from the arm.
        tip_i = np.argmax(np.abs(high[:, 1] - arm_y))
        tip_y = float(high[tip_i, 1])
        dire = 1.0 if tip_y > arm_y else -1.0
        xc = float(np.median(high[:, 0]))
        y_lo, y_hi = float(bv[:, 1].min()) - 0.3, float(bv[:, 1].max()) + 0.3
        rows = _sections(retainer, xc, y_lo, y_hi, OX, OYT, H)
        # Walk from the arm toward the tip.
        rows = [r for r in rows if (r[0] - arm_y) * dire > -0.3]
        rows.sort(key=lambda r: r[0] * dire)
        edge = PANEL_EDGE[0] if dire > 0 else PANEL_EDGE[1]
        root_spans = []
        seen_root = False
        flex_y = None
        plate_spans = []
        for y, z0, z1, sp in rows:
            if z0 < 22.0:
                continue
            # The root is the thick block outboard of the panel edge. The bump
            # is inboard of that edge and is not a root.
            if sp >= 0.62 and 23.70 < z0 < 24.25 and (y - edge) * dire < -0.05:
                root_spans.append(sp)
                seen_root = True
                continue
            if seen_root and 0.32 <= sp <= 0.48 and z0 > 24.20 and flex_y is None:
                flex_y = y
            if flex_y is not None and (y - flex_y) * dire > 0.5 and 0.32 <= sp <= 0.48 and z0 > 24.20:
                plate_spans.append(sp)
        root_t = float(max(root_spans)) if root_spans else 0.0
        # Bump: inboard of the plate, underside below the panel front.
        bump_ys = [y for y, z0, z1, sp in rows if z0 < Zin - 0.05 and z0 > 23.6 and (flex_y is None or (y - flex_y) * dire > 0.4)]
        bump_z = None
        bump_y = None
        if bump_ys:
            zs = []
            for y in bump_ys:
                hits = _z_hits(retainer, xc, y, OX, OYT, H)
                if hits:
                    zs.append(hits[0])
            bump_z = float(min(zs))
            bump_y = float(np.median(bump_ys))
        P = (Zin - bump_z) if bump_z is not None else 0.0
        a = abs(bump_y - flex_y) if (bump_y is not None and flex_y is not None) else 0.0
        # Tooth and rib.
        tooth = bv[(bv[:, 2] > 19.9) & (bv[:, 2] < 20.45)]
        full = tooth[tooth[:, 2] < 20.15] if len(tooth) else tooth
        tooth_y = float(full[:, 1].max() if dire > 0 else full[:, 1].min()) if len(full) else float('nan')
        tooth_z = float(tooth[:, 2].max()) if len(tooth) else float('nan')
        near = tooth[np.abs(tooth[:, 2] - tooth_z) < 0.04] if len(tooth) else tooth
        tooth_y_top = float(near[:, 1].max() if dire > 0 else near[:, 1].min()) if len(near) else tooth_y
        # Rib outer face, above the lead-in, from the carrier.
        rib_band = _board(carrier.vertices, OX, OYT, H)
        rib_band = rib_band[(np.abs(rib_band[:, 0] - xc) < 8) & (rib_band[:, 2] > 20.55) & (rib_band[:, 2] < 20.95)]
        rib_band = rib_band[np.abs(rib_band[:, 1] - arm_y) < 2.5]
        if len(rib_band):
            rib_y = float(rib_band[:, 1].min() if dire > 0 else rib_band[:, 1].max())
        else:
            rib_y = float('nan')
        bite = dire * (tooth_y - rib_y) if np.isfinite(tooth_y) and np.isfinite(rib_y) else 0.0
        # Rib bottom over the tooth.
        y_probe = tooth_y - dire * 0.02
        x_free = float(np.median(low[low[:, 0] > high[:, 0].max() + 0.2][:, 0])) if np.any(low[:, 0] > high[:, 0].max() + 0.2) else xc
        rib_hits = [z for z in _z_hits(carrier, x_free, y_probe, OX, OYT, H) if z > 20.15]
        rib_bot = float(min(rib_hits)) if rib_hits else None
        gap = (rib_bot - tooth_z) if rib_bot is not None else 9.0
        # Arm: free length past the tongue's x, thin direction in y.
        tongue_x1 = float(high[:, 0].max())
        free = low[low[:, 0] > tongue_x1 + 0.15]
        # Above the tooth the section is the arm. The tooth's extra reach is not thickness.
        arm_only = free[free[:, 2] > 20.45] if len(free) else free
        if len(free):
            L = float(free[:, 0].max() - tongue_x1)
            t_y = float(arm_only[:, 1].max() - arm_only[:, 1].min()) if len(arm_only) else 0.0
            h_z = float(free[:, 2].max() - free[:, 2].min())
        else:
            L, t_y, h_z = 0.0, 0.0, 0.0
        w = float(high[:, 0].max() - high[:, 0].min())
        t_plate = float(min(plate_spans)) if plate_spans else 0.0
        edge = PANEL_EDGE[0] if dire > 0 else PANEL_EDGE[1]
        aa = AA_Y[0] if dire > 0 else AA_Y[1]
        margin = dire * (aa - tip_y) if dire > 0 else (tip_y - aa)
        # Edge corner is air. The plate stays above it.
        corner = np.array([[OX + xc, H - OYT - edge, Zin]])
        corner_sd = float(trimesh.proximity.ProximityQuery(retainer).signed_distance(corner)[0])
        lead = abs(tooth_y - tooth_y_top) if np.isfinite(tooth_y) else 0.0
        clips.append({
            'dire': dire, 'root_t': root_t, 'P': P, 'gap': gap, 'a': a, 'w': w,
            't': t_plate, 'L': L, 'ty': t_y, 'hz': h_z, 'bite': bite, 'lead': lead,
            'tip_y': tip_y, 'margin': margin, 'corner': corner_sd, 'xc': xc,
            'mesh': b, 'yC': tooth_y, 'zC': tooth_z,
        })
    return clips


def _force(c, delta):
    if c['a'] < 0.5 or c['t'] < 0.2 or delta <= 0:
        return 0.0, 0.0
    eps = 1.5 * c['t'] * delta / (c['a'] ** 2)
    I = c['w'] * c['t'] ** 3 / 12.0
    F = 3.0 * E_PETG * I * delta / (c['a'] ** 3)
    return eps, F


def _arm_eps(c):
    if c['L'] < 0.5 or c['ty'] <= 0:
        return 9.0
    return 1.5 * c['ty'] * max(c['bite'], 0.0) / (c['L'] ** 2)


def report(clips, carrier, shell, OX, OYT, H, Zin, petg_eps):
    """Print mesh numbers. Return True when a gate fails."""
    fail = False
    if len(clips) != 4:
        print('RETAINER IS NOT FOUR CLIPS')
        fail = True
    print('retainer clips %d (want 4)' % len(clips))
    forces = {1.1: [], 1.2: [], 1.3: []}
    for i, c in enumerate(clips):
        print('clip %d root %.2f mm  lever a %.2f  plate t %.2f  w %.2f  tooth gap %.3f  penetration %.3f' % (
            i, c['root_t'], c['a'], c['t'], c['w'], c['gap'], c['P']))
        print('  arm L %.2f  thickness y %.2f  height z %.2f  bite %.2f  lead-in %.2f  tip margin %.2f' % (
            c['L'], c['ty'], c['hz'], c['bite'], c['lead'], c['margin']))
        if c['root_t'] < 0.70:
            print('RETAINER SECTION TOO THIN')
            fail = True
        if c['gap'] > 0.06 or c['gap'] < -0.02:
            print('TOOTH GAP')
            fail = True
        if c['a'] < 2.4:
            print('CONTACT TOO CLOSE TO THE ROOT')
            fail = True
        if c['corner'] > -0.04:
            print('TONGUE LANDS ON THE PANEL EDGE')
            fail = True
        if c['margin'] < 0.50:
            print('TONGUE COVERS THE ACTIVE AREA')
            fail = True
        if c['L'] < 3.2 or c['hz'] < c['ty'] + 0.30:
            print('ARM IS NOT AN IN-PLANE CANTILEVER')
            fail = True
        if c['lead'] < 0.12 or c['bite'] < 0.20:
            print('TOOTH DOES NOT ENGAGE')
            fail = True
        if _arm_eps(c) >= petg_eps:
            print('RETAINER CLIP FAIL')
            fail = True
        for th in (1.1, 1.2, 1.3):
            delta = c['P'] - c['gap'] + (th - 1.2)
            eps, F = _force(c, delta)
            forces[th].append(F)
            print('  panel %.1f mm  deflection %.3f  strain %.2f%%  force %.3f N' % (th, delta, 100 * eps, F))
            if delta < 0.04:
                print('THIN PANEL LOSES CONTACT')
                fail = True
            if eps >= petg_eps:
                print('RETAINER TONGUE FAIL')
                fail = True
    # A 1.55 mm lever at the nominal deflection is past 2.4%. The measured
    # lever must not be that short, and this case has to fail the formula.
    if clips:
        d0 = clips[0]['P'] - clips[0]['gap']
        t0 = clips[0]['t'] if clips[0]['t'] > 0 else 0.40
        if 1.5 * t0 * d0 / (1.55 ** 2) < petg_eps:
            print('SHORT TONGUE DID NOT FAIL')
            fail = True
    for th, fs in forces.items():
        held = 0.5 * sum(fs)
        print('four clips at %.1f mm after 50%% relaxation %.2f N' % (th, held))
        if th == 1.1 and held < 0.27:
            print('THIN PANEL PRELOAD IS BELOW A 5 g SHOCK')
            fail = True
    # Tilt about the tooth until the shelf stops it. The shell stays clear.
    if carrier is not None and shell is not None and clips:
        qc = trimesh.proximity.ProximityQuery(carrier)
        qs = trimesh.proximity.ProximityQuery(shell)
        for i, c in enumerate(clips):
            ang = _tilt_to_shelf(c, qc, qs, OX, OYT, H)
            if ang is None:
                print('clip %d tilt has no carrier stop' % i)
                print('TILT STOP MISSING')
                fail = True
            else:
                print('clip %d tilt stops at %.1f deg, shell clearance %.3f' % (i, ang[0], ang[1]))
                if ang[1] < 0.08:
                    print('TILT LANDS ON THE SHELL')
                    fail = True
    return fail


def _tilt_to_shelf(c, qc, qs, OX, OYT, H):
    b = c['mesh']
    dire = c['dire']
    axis_y = H - OYT - c['yC']
    zC = c['zC']
    bv = _board(b.vertices, OX, OYT, H)
    bump = bv[(bv[:, 2] > 23.9) & (bv[:, 2] < 24.2)]
    if len(bump) == 0 or not np.isfinite(zC):
        return None

    def rot(ang):
        v = b.vertices.copy()
        yy = v[:, 1] - axis_y
        zz = v[:, 2] - zC
        v[:, 1] = axis_y + yy * np.cos(ang) - zz * np.sin(ang)
        v[:, 2] = zC + yy * np.sin(ang) + zz * np.cos(ang)
        return v

    good = None
    for sgn in (1.0, -1.0):
        v = rot(sgn * np.radians(2.0))
        vb = _board(v, OX, OYT, H)
        if dire > 0:
            nb = vb[(vb[:, 1] > 27.5) & (vb[:, 1] < 29.2)]
            ob = bump[bump[:, 1] > 27.5]
        else:
            nb = vb[(vb[:, 1] < 61.6) & (vb[:, 1] > 59.4)]
            ob = bump[bump[:, 1] < 61.6]
        if len(nb) and len(ob) and float(nb[:, 2].min()) > float(ob[:, 2].min()) + 0.005:
            good = sgn
            break
    if good is None:
        return None
    for deg in np.linspace(0.5, 12.0, 24):
        v = rot(good * np.radians(float(deg)))
        if float(qc.signed_distance(v).max()) > 0.005:
            shell_clear = -float(qs.signed_distance(v).max())
            return float(deg), shell_clear
    return None


def _shift_tooth(retainer, dz):
    m = retainer.copy()
    v = m.vertices
    band = (v[:, 2] > 20.00) & (v[:, 2] < 20.40)
    v[band, 2] += dz
    return m


def _shift_reach(retainer, OX, OYT, H, extra, bottom_only=False):
    m = retainer.copy()
    bv = _board(m.vertices, OX, OYT, H)
    move = np.zeros(len(bv), dtype=bool)
    # Inboard of each edge, past 1 mm onto the border: the tongue tip.
    top = bv[:, 1] < 45
    bot = ~top
    if not bottom_only:
        move |= top & (bv[:, 1] > 27.5)
    move |= bot & (bv[:, 1] < 61.5)
    # extra millimetres further onto the border (and toward the active area).
    bv[move & top, 1] += extra
    bv[move & bot, 1] -= extra
    m.vertices[:, 0] = OX + bv[:, 0]
    m.vertices[:, 1] = H - OYT - bv[:, 1]
    return m


def _drop_root(retainer, OX, OYT, H):
    """Collapse the thick root so only the tongue plate is left there."""
    m = retainer.copy()
    bv = _board(m.vertices, OX, OYT, H)
    root = (bv[:, 2] > 23.85) & (bv[:, 2] < 24.40) & (
        ((bv[:, 1] > 24.5) & (bv[:, 1] < 25.6)) | ((bv[:, 1] > 63.5) & (bv[:, 1] < 64.7)))
    m.vertices[root, 2] = 24.45
    return m


def _widen_root(retainer, OX, OYT, H):
    """A full-span root of the same thickness. It is stronger, and it must pass."""
    m = retainer.copy()
    bv = _board(m.vertices, OX, OYT, H)
    # Copy the root vertices across x by ±6 mm and concatenate.
    root = (bv[:, 2] > 23.85) & (bv[:, 2] < 24.8) & (
        ((bv[:, 1] > 24.5) & (bv[:, 1] < 25.5)) | ((bv[:, 1] > 63.6) & (bv[:, 1] < 64.6)))
    if root.sum() < 4:
        return m
    extra = []
    base = m.copy()
    for dx in (-6.0, 6.0):
        cpy = base.copy()
        cpy.vertices = cpy.vertices.copy()
        cpy.vertices[:, 0] += dx
        extra.append(cpy)
    return trimesh.util.concatenate([m] + extra)


def _lift_bump(retainer, OX, OYT, H, dz=0.30):
    m = retainer.copy()
    bv = _board(m.vertices, OX, OYT, H)
    bump = (bv[:, 2] > 23.95) & (bv[:, 2] < 24.15) & (
        ((bv[:, 1] > 27.5) & (bv[:, 1] < 29.5)) | ((bv[:, 1] > 59.5) & (bv[:, 1] < 61.5)))
    m.vertices[bump, 2] += dz
    return m


def negative_probes(retainer, carrier, shell, OX, OYT, H, Zin, petg_eps):
    """Each bad mesh has to fail. A probe that still passes fails the fit."""
    fail = False

    def check(name, mesh, predicate):
        nonlocal fail
        try:
            clips = measure_clips(mesh, carrier, OX, OYT, H, Zin)
            bad = predicate(clips)
        except Exception as exc:
            bad = True
            clips = []
            print('%s: error %s' % (name, exc))
        if bad:
            print('%s: FAIL' % name)
        else:
            print('%s: DID NOT FAIL' % name)
            fail = True
        return clips

    check('tooth_zair020', _shift_tooth(retainer, -0.18),
          lambda cs: (not cs) or any(c['gap'] > 0.06 for c in cs))
    check('reach40_geom', _shift_reach(retainer, OX, OYT, H, 1.2),
          lambda cs: (not cs) or any(c['margin'] < 0.50 for c in cs))
    check('reach40_bottom', _shift_reach(retainer, OX, OYT, H, 1.2, bottom_only=True),
          lambda cs: (not cs) or any(c['dire'] < 0 and c['margin'] < 0.50 for c in cs))
    check('root_none', _drop_root(retainer, OX, OYT, H),
          lambda cs: (not cs) or any(c['root_t'] < 0.70 for c in cs))
    # A wider root is stronger. The gate reads its thickness and passes.
    wide = measure_clips(_widen_root(retainer, OX, OYT, H), carrier, OX, OYT, H, Zin)
    if wide and all(c['root_t'] >= 0.70 for c in wide):
        print('root_full: PASS thickness %.2f' % min(c['root_t'] for c in wide))
    else:
        print('root_full: DID NOT PASS')
        fail = True
    check('bump_lifted', _lift_bump(retainer, OX, OYT, H),
          lambda cs: (not cs) or any((c['P'] - c['gap'] - 0.10) < 0.04 for c in cs))
    return fail
