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
        # Keep the plane the reviewer measured: +x skin near the silk mouth,
        # -x skin at the ear rear. A wide bin would glue the housing on too.
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
