"""TINT1 step 1: whole-map vertex-colour census of the object LOD models the bake places in bungo's load order.
No build, read only. Placements = the INSTALLED library's .lodi instances (BAKE1, v4); base -> MNAM slot meshes
(rep0..rep3) from the installed .lodo; each model resolved through his MO2 stack (BAKE1 resources.txt, lowest
first): loose files highest-priority first, then GNRL BA2s highest-priority first (the engine lets loose beat
archives). A shape "carries colour" as SEAM1's writer decides it (src/lodofile.h:80): a colour channel in the
vertex descriptor AND SLSF2 Vertex_Colors (flags2 bit 5) on its NIF shader. "Hue" = at least one vertex with
R, G, B not all equal. "Shade" = R=G=B but not all white.
usage: census.py <out tsv>   (prints one verdict block)"""
import sys, os, collections, struct
import numpy as np
T = r'E:/Projects/Claude/.claude/skills/fo4-nif-vertex-channel-census/tools'
sys.path.insert(0, T)
sys.path.insert(0, r'E:/Projects/NifskopeWWE-tint1/tests/spells')
import ba2lib, nifwind
import lodgen_native_decode as dec
BS = chr(92)
D = r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth'
RES = [l.strip() for l in open(r'E:/Projects/NifskopeWWE-bake1/scratchpad/bake1_20260925/resources.txt') if l.strip()]
L = dec.read_lodo(D + '/Commonwealth.lodo')
Ti = dec.read_lodi(D + '/Commonwealth.lodi')
names = [L['string_at'](m['modelStringOffset']) for m in L['meshes']]

# ---- resolver
arcs = []
for r in reversed(RES):
    try:
        fs = sorted(f for f in os.listdir(r) if f.lower().endswith('.ba2'))
    except OSError:
        continue
    for f in fs:
        a = ba2lib.load(r + '/' + f)
        if a is not None:
            arcs.append((r + '/' + f, a))
where = collections.Counter()


def getnif(path):
    n = path.lower().replace('/', BS)
    if not n.startswith('meshes' + BS):
        n = 'meshes' + BS + n
    for r in reversed(RES):
        p = r + '/' + n.replace(BS, '/')
        if os.path.isfile(p):
            where['loose:' + os.path.basename(r)] += 1
            return open(p, 'rb').read(), 'loose:' + os.path.basename(r)
    for an, a in arcs:
        if n in a['recs']:
            where[os.path.basename(an)] += 1
            return ba2lib.get(a, n), os.path.basename(an)
    return None, None


# ---- per mesh
mesh = {}
for i, nm in enumerate(names):
    data, src = getnif(nm)
    if data is None:
        mesh[i] = None
        continue
    N = nifwind.Nif(data)
    shapes = []
    for k, (t, o, sz) in enumerate(N.blocks):
        if t not in nifwind.SHAPES:
            continue
        sh = N.shape(k)
        f1, f2 = 0, 0
        if 0 <= sh['shader'] < len(N.blocks):
            f1, f2, _ = N.shader_flags(sh['shader'])
        vc = sh['cols'] is not None and bool((f2 or 0) & 0x20)
        hue = shade = False
        if vc:
            C = np.asarray(sh['cols'])[:, :3].astype(int)
            hue = bool(np.any((C[:, 0] != C[:, 1]) | (C[:, 1] != C[:, 2])))
            shade = (not hue) and bool(np.any(C != 255))
        shapes.append((vc, hue, shade))
    mesh[i] = dict(src=src, shapes=len(shapes), vc=sum(s[0] for s in shapes), hue=sum(s[1] for s in shapes),
                   shade=sum(s[2] for s in shapes))

# ---- placements
place = collections.Counter(r['baseId'] for r in Ti['instances'])
tot = collections.Counter()
perMesh = collections.Counter()      # placements of bases whose slot-0 mesh is m
for b, c in place.items():
    B = L['bases'][b]
    reps = [B['rep%d' % k] for k in range(4)]
    ms = [m for m in reps if m != 0xFFFF]
    tot['placements'] += c
    m0 = ms[0] if ms else None
    if m0 is None:
        tot['placements_noMesh'] += c
        continue
    perMesh[m0] += c
    M0 = mesh[m0]
    if M0 is None:
        tot['placements_nifMissing'] += c
        continue
    tot['placementShapes_slot0'] += c * M0['shapes']
    if M0['vc']:
        tot['placements_vc_slot0'] += c
        tot['placementShapes_vc_slot0'] += c * M0['vc']
    if M0['hue']:
        tot['placements_hue_slot0'] += c
        tot['placementShapes_hue_slot0'] += c * M0['hue']
    if any(mesh[m] and mesh[m]['vc'] for m in ms):
        tot['placements_vc_anySlot'] += c
    if any(mesh[m] and mesh[m]['hue'] for m in ms):
        tot['placements_hue_anySlot'] += c
lib = collections.Counter()
for i, M in mesh.items():
    lib['meshes'] += 1
    if M is None:
        lib['nifMissing'] += 1
        continue
    lib['shapes'] += M['shapes']; lib['vcShapes'] += M['vc']; lib['hueShapes'] += M['hue']; lib['shadeShapes'] += M['shade']
    lib['vcMeshes'] += bool(M['vc']); lib['hueMeshes'] += bool(M['hue'])
print('library: %s' % dict(lib))
print('placements (.lodi instances, slot 0 = the base first MNAM mesh): %s' % dict(tot))
print('sources: %s' % where.most_common(12))
out = open(sys.argv[1], 'w')
out.write('rank\tmodel\tsource\tplacements_slot0\tshapes\tvcShapes\thueShapes\tshadeShapes\n')
rows = sorted(((c, m) for m, c in perMesh.items() if mesh[m] and mesh[m]['vc']), reverse=True)
for k, (c, m) in enumerate(rows):
    M = mesh[m]
    out.write('%d\t%s\t%s\t%d\t%d\t%d\t%d\t%d\n' % (k + 1, names[m], M['src'], c, M['shapes'], M['vc'], M['hue'], M['shade']))
print('top 15 coloured slot-0 models by placements:')
for k, (c, m) in enumerate(rows[:15]):
    M = mesh[m]
    print('  %2d %6d  %-60s vc %d hue %d shade %d  [%s]' % (k + 1, c, names[m][-60:], M['vc'], M['hue'], M['shade'], M['src']))
# where the tinted placements are: cells of hue placements, top 5 by count (for the picture)
cells = collections.Counter()
for r in Ti['instances']:
    B = L['bases'][r['baseId']]
    m0 = B['rep0'] if B['rep0'] != 0xFFFF else None
    if m0 is not None and mesh[m0] and mesh[m0]['hue']:
        cells[r['cell'] if 'cell' in r else None] += 1
print('hue placements by lodi cell field:', cells.most_common(5))
# ---- extra: white-only VC placements, every hue/shade model, channel-without-flag shapes
white = sum(c for m, c in perMesh.items() if mesh[m] and mesh[m]['vc'] and not mesh[m]['hue'] and not mesh[m]['shade'])
shadeP = sum(c for m, c in perMesh.items() if mesh[m] and mesh[m]['shade'] and not mesh[m]['hue'])
print('slot-0 placements whose VC is white-only (alpha only): %d; shade-only (R=G=B, not white): %d' % (white, shadeP))
print('every hue or shade model (placements slot0):')
for m in sorted((m for m in mesh if mesh[m] and (mesh[m]['hue'] or mesh[m]['shade'])), key=lambda m: -perMesh[m]):
    print('  %6d  %-70s hue %d shade %d' % (perMesh[m], names[m][-70:], mesh[m]['hue'], mesh[m]['shade']))
# where: dim-4 manifests (world x, y per placement) -> cells of hue placements
import glob
byFid = {B['formId']: B for B in L['bases']}
cellHue = collections.Counter(); cellShade = collections.Counter()
for f in glob.glob(D + '/Commonwealth.4.*.BTO.manifest.txt'):
    for ln in open(f):
        p = ln.split()
        if ln.startswith('#') or len(p) != 11:
            continue
        B = byFid.get(int(p[1], 16))
        if not B:
            continue
        ms = [B['rep%d' % k] for k in range(4) if B['rep%d' % k] != 0xFFFF]
        if not ms or not mesh[ms[0]]:
            continue
        cx, cy = int(np.floor(float(p[3]) / 4096)), int(np.floor(float(p[4]) / 4096))
        if mesh[ms[0]]['hue']:
            cellHue[(cx // 4 * 4, cy // 4 * 4)] += 1
        elif mesh[ms[0]]['shade']:
            cellShade[(cx // 4 * 4, cy // 4 * 4)] += 1
print('hue placements by 4x4-cell block (SW corner):', cellHue.most_common(6), 'total', sum(cellHue.values()))
print('shade placements by 4x4-cell block:', cellShade.most_common(4), 'total', sum(cellShade.values()))
