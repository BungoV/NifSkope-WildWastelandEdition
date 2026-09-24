"""Why does the per-vertex SKY stream agree with the 0x11 sky byte worse than
the per-vertex AO stream agrees with its own byte, on the identical placements?

Two mechanisms, both measurable on the shipped pair:

  H1 SCENE.  skyVisibility casts 9 rays UP to 300 x dim. A long ray leaves the
     placement's own neighbourhood and hits whatever else the scene holds, and
     the two scenes differ most at the chunk border (the byte's scene is the
     stock .BTO buckets plus a CELL of skirt; the stream's is nativeemit's own
     LodgenAoScene with SKIRT = 1). AO's 8 rays stay about the normal, so AO
     sees mostly the local patch both scenes share. PREDICTION: the sky error
     climbs towards the chunk edge much faster than the AO error does.

  H2 HEIGHT.  The byte is a mean over the .BTO chunk mesh's vertices for that
     placement; the stream is a mean over the authored LOD mesh's vertices. The
     two vertex populations differ in HEIGHT, and sky varies strongly with
     height (a base is occluded, a roof is open) while AO does not. PREDICTION:
     the sky error climbs with the placement's bound radius (its size), the AO
     error much less.
"""
import sys, collections
sys.path.insert(0, 'tests/spells')
import lodgen_native_decode as D
V = 'scratchpad/lodiv7_20260918/v7/nat/FO4CSLOD/Commonwealth/Commonwealth.'
L = D.read_lodo(V + 'lodo'); T = D.read_lodi(V + 'lodi')
h = T['header']; n = h['instanceCount']
inst, cold = T['instances'], T['cold']
br = [b['boundRadius'] for b in L['bases']]

w = h['chunkEast'] - h['chunkWest'] + 1
edge = [None] * n
for ci, c in enumerate(T['chunks']):
    for i in range(c['instanceFirst'], c['instanceFirst'] + c['instanceCount']):
        r = inst[i]
        fx = r['px'] / 65535.0 * 16384.0
        fy = r['py'] / 65535.0 * 16384.0
        edge[i] = min(fx, 16384.0 - fx, fy, 16384.0 - fy)


def err(first, data, key):
    e = [None] * n
    for i in range(n):
        lo, hi = first[i], first[i + 1]
        if hi > lo:
            e[i] = abs(sum(data[lo:hi]) / float(hi - lo) - inst[i][key])
    return e


eAo = err(T['vertexAoFirst'], T['vertexAo'], 'ao')
eSk = err(T['vertexSkyFirst'], T['vertexSky'], 'sky')


def band(name, bins, val):
    print('  %-22s %8s %8s %8s %8s' % (name, 'n', 'AO w2%', 'SKY w2%', 'SKY med'))
    for lo, hi in bins:
        ii = [i for i in range(n) if eAo[i] is not None and lo <= val[i] < hi]
        if not ii:
            continue
        a = 100.0 * sum(1 for i in ii if eAo[i] <= 2) / len(ii)
        s = 100.0 * sum(1 for i in ii if eSk[i] <= 2) / len(ii)
        m = sorted(eSk[i] for i in ii)[len(ii) // 2]
        print('  %-22s %8d %8.2f %8.2f %8.2f' % ('%g..%g' % (lo, hi), len(ii), a, s, m))


print('H1 SCENE -- by distance to the chunk edge (world units)')
band('edge distance', [(0, 512), (512, 2048), (2048, 6000), (6000, 1e9)], edge)
print()
print('H2 HEIGHT -- by the base bound radius (world units, scale 1)')
rad = [br[inst[i]['baseId']] if inst[i]['baseId'] < len(br) else 0.0 for i in range(n)]
band('bound radius', [(0, 64), (64, 256), (256, 1024), (1024, 1e9)], rad)
print()
print('vertex-count classes (the flat-card control that was REFUTED for AO)')
vc = [T['vertexAoFirst'][i + 1] - T['vertexAoFirst'][i] for i in range(n)]
band('vertices', [(1, 5), (5, 17), (17, 65), (65, 1e9)], vc)
print()
ii = [i for i in range(n) if eSk[i] is not None]
big = sorted(ii, key=lambda i: -eSk[i])[:5]
S = L['string_at']
bm = [S(b['modelStringOffset']) for b in L['bases']]
print('the five widest sky gaps')
for i in big:
    lo, hi = T['vertexSkyFirst'][i], T['vertexSkyFirst'][i + 1]
    sl = T['vertexSky'][lo:hi]
    print('  inst %-5d gap %6.1f  byte %3d  stream mean %6.1f (min %3d max %3d, %d verts)  edge %5.0f  %s'
          % (i, eSk[i], inst[i]['sky'], sum(sl) / float(len(sl)), min(sl), max(sl), len(sl),
             edge[i], bm[inst[i]['baseId']].split(chr(92))[-1]))
