# J4 solid from the manufacturer STEP, in board coordinates.
# The KiCad footprint places the WRL. The STEP and the WRL share the ear
# span (board y 35.2–52.8) and differ by a constant along board x. This
# module shifts the STEP so the slider's +x face lands on the silk mouth.
# A point is inside when a +z ray's hit list puts it in an odd interval.
# Empty vertex bins are not treated as air.
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
    # The ears are the vertices past board y 52. The slider front is their max x.
    ears = raw.vertices[raw.vertices[:, 1] > 52.0]
    if len(ears) < 10:
        raise RuntimeError('J4 STEP has no high-ear vertices')
    shift = _MOUTH_X - float(ears[:, 0].max())
    raw.vertices[:, 0] += shift
    return raw, shift


class J4Solid:
    def __init__(self):
        self.mesh, self.shift = load_board_mesh()
        self._inter = self.mesh.ray

    def column_hits(self, x, y):
        origin = np.array([[x, y, -2.0]])
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
        h = self.column_hits(x, y)
        if len(h) < 2:
            return False
        inside = False
        for i in range(0, len(h) - 1, 2):
            if h[i] - 0.02 <= z <= h[i + 1] + 0.02:
                inside = True
                break
        return inside

    def contains_points(self, pts):
        """pts are (n,3) board x, y, z-above-PCB."""
        out = np.zeros(len(pts), dtype=bool)
        # Group by rounded x,y so each column is cast once.
        key = np.round(pts[:, :2], 2)
        # unique rows
        _, inv = np.unique(key, axis=0, return_inverse=True)
        for k in range(inv.max() + 1 if len(inv) else 0):
            sel = inv == k
            p = pts[sel][0]
            h = self.column_hits(float(p[0]), float(p[1]))
            if len(h) < 2:
                continue
            zz = pts[sel, 2]
            inside = np.zeros(zz.shape, dtype=bool)
            for i in range(0, len(h) - 1, 2):
                inside |= (zz >= h[i] - 0.02) & (zz <= h[i + 1] + 0.02)
            out[sel] = inside
        return out
