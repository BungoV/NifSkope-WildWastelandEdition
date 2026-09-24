import sys
sys.path.insert(0, r"E:\Projects\NifskopeWildWastelandEdition\tests\spells")
from gltf_nifread import Nif
n = Nif(r"E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Architecture/Quarry/QryCube01.nif")
lo = [1e9] * 3; hi = [-1e9] * 3
for i, sh in n.shapes.items():
    for v in sh['verts']:
        for k in range(3):
            lo[k] = min(lo[k], v[k]); hi[k] = max(hi[k], v[k])
    print(i, sh['t'], sh['s'], [tuple(round(c, 2) for c in v) for v in sh['verts']][:12])
print("bounds", lo, hi, "nodes", {k: (v.get('t'), v.get('s')) for k, v in n.nodes.items()})
