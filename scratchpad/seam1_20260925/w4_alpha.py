"""W4 v5 design input: per shape with a colour stream, the Vertex_Colors, Vertex_Alpha and Tree_Anim bits,
and whether the stream's A is ever below 255 (w4_models.tsv models)."""
import sys, collections
sys.path.insert(0, r'E:/Projects/Claude/.claude/skills/fo4-nif-vertex-channel-census/tools')
import ba2lib, nifwind
arcs = [ba2lib.load(r'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4 - Meshes.ba2'),
        ba2lib.load(r'X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4 - MeshesExtra.ba2')]
BS = chr(92); c = collections.Counter()
for ln in list(open('w4_models.tsv'))[1:]:
    p = ln.split('\t'); path = p[0]
    n = ('meshes' + BS + path).lower(); d = None
    for a in arcs:
        try: d = ba2lib.get(a, n); break
        except KeyError: pass
    if d is None: continue
    N = nifwind.Nif(d)
    for k, (t, o, s) in enumerate(N.blocks):
        if t in nifwind.SHAPES:
            sh = N.shape(k)
            f1, f2, m = N.shader_flags(sh['shader']) if 0 <= sh['shader'] < len(N.blocks) else (0, 0, '')
            f1 = f1 or 0; f2 = f2 or 0
            if sh['cols'] is None and not f2 & 0x20: continue
            amin = min(c4[3] for c4 in sh['cols']) if sh['cols'] is not None else None
            key = ('stream' if sh['cols'] is not None else 'nostream', 'VC' if f2 & 0x20 else '-', 'VA' if f1 & 8 else '-',
                   'TreeAnim' if f2 & (1 << 29) else '-', 'A<255' if amin is not None and amin < 255 else '-')
            c[key] += 1
            if key[1] == 'VC' and key[0] == 'stream': print(path.split(BS)[-1], sh['name'], key[2:], 'Amin', amin)
for k, v in sorted(c.items()): print(k, v)
