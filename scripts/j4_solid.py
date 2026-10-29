# J4 solid from the manufacturer STEP, in board coordinates.
# The KiCad footprint places the WRL. The STEP and the WRL share the ear
# span (board y 35.2–52.8) and differ by a constant along board x. This
# module shifts the STEP so the slider's +x face lands on the silk mouth.
# Parts are not watertight, so a single +z ray is sometimes odd. contains()
# votes across a small cross of rays and keeps only even hit counts.
# Faces are rasterized triangle areas, not clusters of corner vertices.
import os
import numpy as np
import trimesh

_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_STEP = os.path.join(_ROOT, 'kicad', 'tstat.3dshapes', 'FPC-SMD_AFC07-S24FCA-00.step')
_MOUTH_X = 103.77


def load_board_mesh():
    import cascadio
    data = open(_STEP, 'rb').read()
    glb = cascadio.load(data, file_type='step', merge_primitives=True,
                        tol_linear=0.00005, tol_angular=0.4)
    scene = trimesh.load(trimesh.util.wrap_as_stream(glb), file_type='glb')
    parts = []
    for g in scene.geometry.values():
        v = np.asarray(g.vertices) * 1000.0
        b = np.column_stack([100.0 - v[:, 1], 44.0 - v[:, 0], v[:, 2]])
        parts.append(trimesh.Trimesh(b, np.asarray(g.faces), process=False))
    raw = trimesh.util.concatenate(parts)
    ears = raw.vertices[raw.vertices[:, 1] > 52.0]
    if len(ears) < 10:
        raise RuntimeError('J4 STEP has no high-ear vertices')
    shift = _MOUTH_X - float(ears[:, 0].max())
    raw.vertices[:, 0] += shift
    return raw, shift


def _intervals(hits, z):
    inside = False
    for i in range(0, len(hits) - 1, 2):
        if hits[i] - 0.02 <= z <= hits[i + 1] + 0.02:
            inside = True
            break
    return inside


class J4Solid:
    def __init__(self):
        self.mesh, self.shift = load_board_mesh()
        self._inter = self.mesh.ray

    def column_hits(self, x, y):
        origin = np.array([[float(x), float(y), -2.0]])
        direction = np.array([[0.0, 0.0, 1.0]])
        loc, _, _ = self._inter.intersects_location(origin, direction)
        if len(loc) == 0:
            return np.zeros(0)
        z = np.sort(loc[:, 2])
        keep = [z[0]]
        for t in z[1:]:
            if t - keep[-1] > 0.03:
                keep.append(t)
        return np.array(keep)

    def contains(self, x, y, z):
        # Odd columns are cracks in a non-watertight solid. Neighbors vote.
        votes = []
        for dx, dy in ((0.0, 0.0), (0.10, 0.0), (-0.10, 0.0), (0.0, 0.10), (0.0, -0.10)):
            h = self.column_hits(x + dx, y + dy)
            if len(h) >= 2 and len(h) % 2 == 0:
                votes.append(_intervals(h, z))
        if not votes:
            return False
        return sum(votes) * 2 >= len(votes)

    def contains_points(self, pts):
        out = np.zeros(len(pts), dtype=bool)
        if len(pts) == 0:
            return out
        key = np.round(pts[:, :2], 2)
        _, inv = np.unique(key, axis=0, return_inverse=True)
        for k in range(int(inv.max()) + 1):
            sel = np.where(inv == k)[0]
            p = pts[sel[0]]
            for i in sel:
                out[i] = self.contains(float(p[0]), float(p[1]), float(pts[i, 2]))
        return out

    def axis_faces(self, axis, sign, min_span_a=1.5, min_span_b=1.2, cell=0.20):
        """Connected regions of triangles whose normal faces along axis.

        axis 0 is board x. span_a is the first of the other two axes
        (y if axis is x), span_b the second (z). A flat face is one region
        even when its vertices sit only at the corners.
        """
        n = self.mesh.face_normals
        tri = self.mesh.triangles
        align = n[:, axis] * sign
        # KiCad's board frame is left-handed, so STEP normals flip. Accept both.
        sel = np.abs(n[:, axis]) > 0.85
        if not np.any(sel):
            return []
        other = [i for i in (0, 1, 2) if i != axis]
        a, b = other
        tris = tri[sel]
        # +x skin is the slider face at the silk mouth. -x skin is the ear rear.
        # A wide bin would glue the housing on too.
        plane = tris[:, :, axis].mean(axis=1)
        if axis == 0 and sign > 0:
            keep = plane > 103.55
        elif axis == 0 and sign < 0:
            keep = (plane > 102.55) & (plane < 103.05)
        else:
            keep = np.ones(len(plane), dtype=bool)
        tris = tris[keep]
        if len(tris) == 0:
            return []
        aa = tris[:, :, a]
        bb = tris[:, :, b]
        a0, a1 = float(aa.min()) - cell, float(aa.max()) + cell
        b0, b1 = float(bb.min()) - cell, float(bb.max()) + cell
        nu = int((a1 - a0) / cell) + 2
        nv = int((b1 - b0) / cell) + 2
        grid = np.zeros((nu, nv), dtype=np.uint8)
        for t in tris:
            umin = max(0, int((t[:, a].min() - a0) / cell))
            umax = min(nu - 1, int((t[:, a].max() - a0) / cell))
            vmin = max(0, int((t[:, b].min() - b0) / cell))
            vmax = min(nv - 1, int((t[:, b].max() - b0) / cell))
            grid[umin:umax + 1, vmin:vmax + 1] = 1
        seen = np.zeros_like(grid, dtype=bool)
        faces = []
        for u in range(nu):
            for v in range(nv):
                if grid[u, v] == 0 or seen[u, v]:
                    continue
                stack = [(u, v)]
                seen[u, v] = True
                cells = []
                while stack:
                    cu, cv = stack.pop()
                    cells.append((cu, cv))
                    for du, dv in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        nuu, nvv = cu + du, cv + dv
                        if nuu < 0 or nvv < 0 or nuu >= nu or nvv >= nv:
                            continue
                        if grid[nuu, nvv] == 0 or seen[nuu, nvv]:
                            continue
                        seen[nuu, nvv] = True
                        stack.append((nuu, nvv))
                uu = np.array([c[0] for c in cells])
                vv = np.array([c[1] for c in cells])
                span_a = (uu.max() - uu.min() + 1) * cell
                span_b = (vv.max() - vv.min() + 1) * cell
                if span_a >= min_span_a and span_b >= min_span_b:
                    faces.append({'span_a': float(span_a), 'span_b': float(span_b),
                                  'a0': a0 + uu.min() * cell, 'b0': b0 + vv.min() * cell})
        return faces

    def full_height_bands(self, axis, sign, min_height=1.40, cell=0.20):
        """Y spans where the face is at least min_height tall. Splits the
        short band over the tail from the full-height faces beside it."""
        n = self.mesh.face_normals
        tri = self.mesh.triangles
        sel = np.abs(n[:, axis]) > 0.85
        other = [i for i in (0, 1, 2) if i != axis]
        a, b = other  # a is y when axis is x, b is z
        tris = tri[sel]
        plane = tris[:, :, axis].mean(axis=1)
        if axis == 0 and sign > 0:
            tris = tris[plane > 103.55]
        elif axis == 0 and sign < 0:
            tris = tris[(plane > 102.55) & (plane < 103.05)]
        if len(tris) == 0:
            return []
        y0 = float(tris[:, :, a].min())
        y1 = float(tris[:, :, a].max())
        bins = np.arange(y0, y1 + cell, cell)
        bands = []
        run = None
        for i in range(len(bins) - 1):
            y = 0.5 * (bins[i] + bins[i + 1])
            zs = []
            for t in tris:
                for k in range(3):
                    p = t[k]
                    q = t[(k + 1) % 3]
                    dy = q[a] - p[a]
                    if abs(dy) < 1e-9:
                        continue
                    if (p[a] - y) * (q[a] - y) > 0:
                        continue
                    u = (y - p[a]) / dy
                    if not (0.0 <= u <= 1.0):
                        continue
                    px = p[axis] + u * (q[axis] - p[axis])
                    if axis == 0 and sign > 0 and px < 103.65:
                        continue
                    if axis == 0 and sign < 0 and not (102.60 <= px <= 102.95):
                        continue
                    zs.append(p[b] + u * (q[b] - p[b]))
            if len(zs) < 2:
                if run:
                    bands.append(run)
                    run = None
                continue
            z0 = float(min(zs))
            z1 = float(max(zs))
            if z1 - z0 >= min_height:
                if run is None:
                    run = [bins[i], bins[i + 1], z0, z1]
                else:
                    run[1] = bins[i + 1]
                    run[2] = min(run[2], z0)
                    run[3] = max(run[3], z1)
            elif run:
                bands.append(run)
                run = None
        if run:
            bands.append(run)
        return [{'y0': b[0], 'y1': b[1], 'z0': b[2], 'z1': b[3],
                 'dy': b[1] - b[0], 'dz': b[3] - b[2]} for b in bands]

    def _first_hit_x(self, origins, direction):
        """x of the first intersection along each ray. NaN if the ray misses."""
        n = len(origins)
        dirs = np.zeros((n, 3))
        dirs[:, 0] = direction
        loc, ray_idx, _ = self.mesh.ray.intersects_location(origins, dirs, multiple_hits=True)
        hit = np.full(n, np.nan)
        if len(loc) == 0:
            return hit
        order = np.argsort(loc[:, 0] * direction, kind='mergesort')
        loc = loc[order]
        ray_idx = ray_idx[order]
        _, uniq = np.unique(ray_idx, return_index=True)
        hit[ray_idx[uniq]] = loc[uniq, 0]
        return hit

    def visible_bands(self, side, cell=0.025, tol=0.03, min_run=1.60):
        """Faces a ray from open air hits first, as continuous full-height runs.

        side 'front' looks from +x at the slider face (x 103.77).
        side 'rear' looks from -x at the ear rear (x 102.77).
        A column counts only where the hits form one run at least min_run tall.
        A box beside the connector, or a face hidden behind the housing, does not
        join that run. A blocker in front of the ray removes the run.
        """
        if side == 'front':
            x_from, direction, plane = 109.0, -1.0, 103.77
            y0, y1, z0, z1 = 34.5, 53.5, 0.05, 2.10
        elif side == 'rear':
            x_from, direction, plane = 96.0, 1.0, 102.77
            y0, y1, z0, z1 = 34.8, 53.2, 0.05, 2.10
            tol = max(tol, 0.08)
            min_run = min(min_run, 1.40)
        else:
            raise ValueError(side)
        ys = np.arange(y0, y1 + cell * 0.5, cell)
        zs = np.arange(z0, z1 + cell * 0.5, cell)
        yy, zz = np.meshgrid(ys, zs, indexing='ij')
        origins = np.column_stack([np.full(yy.size, x_from), yy.ravel(), zz.ravel()])
        hit = self._first_hit_x(origins, direction).reshape(yy.shape)
        on = np.isfinite(hit) & (np.abs(hit - plane) <= tol)
        tall = []
        need = int(round(min_run / cell))
        for i, y in enumerate(ys):
            col = on[i]
            best = cur = 0
            z_a = z_b = 0.0
            ca = 0
            for k, flag in enumerate(col):
                if flag:
                    if cur == 0:
                        ca = k
                    cur += 1
                    if cur > best:
                        best = cur
                        z_a = float(zs[ca])
                        z_b = float(zs[k] + cell)
                else:
                    cur = 0
            if best >= need:
                xs = hit[i, on[i]]
                tall.append((float(y), z_a, z_b, float(np.median(xs))))
        bands = []
        run = None
        for y, za, zb, x in tall:
            if run is None or y - run['y1'] > cell * 1.5:
                if run is not None:
                    bands.append(run)
                run = {'y0': y, 'y1': y, 'z0': za, 'z1': zb, 'x': x}
            else:
                run['y1'] = y
                run['z0'] = min(run['z0'], za)
                run['z1'] = max(run['z1'], zb)
        if run is not None:
            bands.append(run)
        for b in bands:
            b['y1'] = b['y1'] + cell
            b['dy'] = b['y1'] - b['y0']
            b['dz'] = b['z1'] - b['z0']
            # The ray cell widens a face that is only a couple of millimetres.
            # Triangles on the same plane, inside that window, give the edge.
            if b['dy'] > 1.0:
                span = self._on_plane_span(plane, b['y0'] - 0.05, b['y1'] + 0.05, max(tol, 0.04))
                if span is not None and (span[1] - span[0]) > 1.0:
                    b['y0'], b['y1'], b['z0'], b['z1'] = span
                    b['dy'] = b['y1'] - b['y0']
                    b['dz'] = b['z1'] - b['z0']
        return bands

    def _on_plane_span(self, plane, y0, y1, tol):
        """y/z extent of triangles on the face plane inside the visible window."""
        tri = self.mesh.triangles
        nrm = self.mesh.face_normals
        px = tri[:, :, 0].mean(axis=1)
        sel = (np.abs(nrm[:, 0]) > 0.80) & (np.abs(px - plane) <= tol)
        if not np.any(sel):
            return None
        t = tri[sel]
        ty0 = t[:, :, 1].min(axis=1)
        ty1 = t[:, :, 1].max(axis=1)
        tz0 = t[:, :, 2].min(axis=1)
        tz1 = t[:, :, 2].max(axis=1)
        m = (ty1 >= y0) & (ty0 <= y1) & ((tz1 - tz0) > 0.25)
        # Keep the part of each triangle that lies in the window.
        if not np.any(m):
            return None
        ya = float(np.maximum(ty0[m], y0).min())
        yb = float(np.minimum(ty1[m], y1).max())
        # The face edge is the triangle edge, not the window, when the triangle
        # sits inside the window.
        inside = (ty0[m] >= y0 - 1e-6) & (ty1[m] <= y1 + 1e-6)
        if np.any(inside):
            ya = float(ty0[m][inside].min())
            yb = float(ty1[m][inside].max())
        return ya, yb, float(tz0[m].min()), float(tz1[m].max())

    def nail_hits_solid(self, bounds, skin=0.15, n=6):
        """True when the box enters the housing past a contact skin.

        bounds is (x0, y0, z0, x1, y1, z1) in board millimetres.
        A thumbnail may kiss the slider face. It may not occupy the housing.
        """
        x0, y0, z0, x1, y1, z1 = bounds
        xs = np.linspace(x0, x1, n)
        ys = np.linspace(y0, y1, max(2, n // 2))
        zs = np.linspace(z0, z1, n)
        for x in xs:
            for y in ys:
                for z in zs:
                    if abs(x - 103.77) <= skin or abs(x - 102.77) <= skin:
                        continue
                    if self.contains(float(x), float(y), float(z)):
                        return True
        return False

    def housing_stop(self, mouth_x):
        """Mouth to the cavity back wall the tail bottoms on.

        The outermost vertex is a solder tab, not that wall. A ray from the
        mouth, through the cavity, hits the wall.
        """
        walls = []
        for y in (43.0, 44.0, 45.0):
            for z in (0.70, 0.85, 0.95):
                origin = np.array([[mouth_x - 0.40, y, z]])
                hit = self._first_hit_x(origin, -1.0)
                if np.isfinite(hit[0]) and hit[0] < mouth_x - 0.8:
                    walls.append(float(hit[0]))
        if not walls:
            return None
        return float(mouth_x - np.median(walls))
