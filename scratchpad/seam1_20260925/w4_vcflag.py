"""W4a addendum: over every shape of the Boston LOD models (w4_models.tsv), the shader's Vertex_Colors bit
(Shader Flags 2 bit 5) against whether the shape carries a colour stream, weighted by placements."""
import sys, collections
sys.path.insert(0, r'E:/Projects/Claude/.claude/skills/fo4-nif-vertex-channel-census/tools')
import ba2lib, nifwind
arcs = [ba2lib.load(r'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4 - Meshes.ba2'),
        ba2lib.load(r'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4 - MeshesExtra.ba2')]
BS = chr(92); c = collections.Counter(); pc = collections.Counter()
for ln in list(open('w4_models.tsv'))[1:]:
    p = ln.split('\t'); path, placed = p[0], int(p[2])
    n = ('meshes' + BS + path).lower(); d = None
    for a in arcs:
        try: d = ba2lib.get(a, n); break
        except KeyError: pass
    if d is None: continue
    N = nifwind.Nif(d)
    for k, (t, o, s) in enumerate(N.blocks):
        if t in nifwind.SHAPES:
            sh = N.shape(k)
            f1, f2, m = N.shader_flags(sh['shader']) if 0 <= sh['shader'] < len(N.blocks) else (None, None, '')
            key = ('stream' if sh['cols'] is not None else 'nostream', 'VCbit' if (f2 or 0) & 0x20 else 'noVCbit')
            c[key] += 1; pc[key] += placed
print('shapes', dict(c)); print('placement-weighted', dict(pc))
