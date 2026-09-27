"""IDENT1: street eye points from the data. For each landmark (model-name words), the placements' centroid and
extent; then a candidate point is walked outward from the centroid, 8 directions x 256 u steps, to the first
point that lies in NO placement's footprint (xy box of its placed triangles) within 12 km. Prints one line each.
usage: python eyes.py <lod base>"""
import sys, math
import numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWWE-ident1/tests/spells')
import lodgen_native_decode as ND
from lodi_occluder_building import placed_tris, first_mesh, NO_MESH

MARKS = [('trinity', ['churchtrin']), ('towers', ['hitext']), ('diamond_city', ['\\dext', '/dext'])]


def main():
    base = sys.argv[1]
    L = ND.read_lodo(base + '.lodo'); T = ND.read_lodi(base + '.lodi')
    s_at = L['string_at']
    boxes = []; names = []
    for r in T['instances']:
        if not (-9 * 4096 <= r['x'] < 4 * 4096 and -13 * 4096 <= r['y'] < 0):
            continue
        me = first_mesh(L, r)
        if me == NO_MESH:
            continue
        t = placed_tris(L, r, me)
        if not len(t):
            continue
        v = t.reshape(-1, 3)
        boxes.append((v[:, 0].min(), v[:, 1].min(), v[:, 0].max(), v[:, 1].max()))
        names.append(s_at(L['meshes'][me]['modelStringOffset']).lower())
    B = np.array(boxes)

    def free(x, y):
        return not np.any((B[:, 0] <= x) & (x <= B[:, 2]) & (B[:, 1] <= y) & (y <= B[:, 3]))
    for key, words in MARKS:
        idx = [i for i, n in enumerate(names) if any(w in n for w in words)]
        if not idx:
            print(key, 'no pieces'); continue
        sub = B[idx]
        cx = float((sub[:, 0].min() + sub[:, 2].max()) / 2); cy = float((sub[:, 1].min() + sub[:, 3].max()) / 2)
        best = None
        for k in range(8):
            a = k * math.pi / 4
            for s in range(1, 60):
                x, y = cx + math.cos(a) * s * 256, cy + math.sin(a) * s * 256
                if free(x, y):
                    if best is None or s < best[0]:
                        best = (s, round(x), round(y))
                    break
        print(key, 'pieces', len(idx), 'centre', round(cx), round(cy), 'extent', round(float(sub[:, 2].max() - sub[:, 0].min())),
              round(float(sub[:, 3].max() - sub[:, 1].min())), 'nearest free street point', best)


if __name__ == '__main__':
    main()
