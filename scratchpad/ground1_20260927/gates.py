"""gates.py -- GROUND1 gates on the Boston box.

  python gates.py <rung bake> <off bake> <on bake> <lodl> <out.json>

1. byte identity: <off bake> == <rung bake>, every file under mod/ and scr/ (logs excluded), after two red
   controls on the same comparator (one flipped byte mid-file mid-list; one file removed).
2. independent recompute of the ramp (audit1 aud_ground.py method: .lodo vertices placed by the decoded
   instance transform, bilinear terrain from a .lodl, clamp(1 - (z - g) / 256)), against the v12 stream,
   ALL vertices of each drawn mesh (the stream's population), |stream - recompute| <= 1 share.
3. physical counts, classified by the recomputed height above terrain.
4. per placement: stream mean vs the placement byte 0x12 (and 0x12 == the rung's 0x12).
5. size: on .lodi - off .lodi; whole-map estimate from a whole-map .lodi's vertex-AO stream size.
Red controls for 2-4: the OLD per-vertex value (the placement byte broadcast over the placement's vertices),
and the stream shifted by +2.
Shares no code with the writer. Decoder = this worktree's tests/spells (reads v12).
"""
import os, sys, json, time, struct, math
import numpy as np

WT = 'E:/Projects/NifskopeWWE-ground1'
sys.path.insert(0, WT + '/tests/spells')
import lodgen_native_decode as ND
import lodl_open_authority as LA

NO_MESH = 0xFFFFFFFF
t0 = time.time()
RUNG, OFF, ON, LODL, OUT = sys.argv[1:6]
WHOLE = 'E:/Projects/NifskopeWWE-bake2/scratchpad/ao2_20260926/whole/after/mod/FO4CSLOD/Commonwealth/Commonwealth.lodi'
out = {}


def say(k, v):
    out[k] = v
    print('%-60s %s' % (k, v))


# ---------------------------------------------------------------- 1. byte identity
def tree(root):
    d = {}
    for sub in ('mod', 'scr'):
        base = os.path.join(root, sub)
        for dp, dn, fn in os.walk(base):
            for f in fn:
                if f.endswith('.log'):
                    continue
                p = os.path.join(dp, f)
                d[os.path.relpath(p, root).replace('\\', '/')] = p
    return d


def cmp_trees(A, B, overB=None, dropB=None):
    names = sorted(set(A) | set(B))
    both = differ = onlyA = onlyB = 0
    first = None
    for n in names:
        inA = n in A
        inB = n in B and n != dropB
        if inA and not inB:
            onlyA += 1; continue
        if inB and not inA:
            onlyB += 1; continue
        both += 1
        a = open(A[n], 'rb').read()
        b = overB[1] if (overB and overB[0] == n) else open(B[n], 'rb').read()
        if a != b:
            differ += 1
            if first is None:
                first = n
    return both, differ, onlyA, onlyB, first


TA, TB = tree(RUNG), tree(OFF)
names = sorted(set(TA) & set(TB))
say('identity.files_rung', len(TA)); say('identity.files_off', len(TB))
mid = names[len(names) // 2]
buf = bytearray(open(TB[mid], 'rb').read())
if len(buf) == 0:
    mid = [n for n in names if os.path.getsize(TB[n]) > 0][len(names) // 3]
    buf = bytearray(open(TB[mid], 'rb').read())
buf[len(buf) // 2] ^= 0x01
r1 = cmp_trees(TA, TB, overB=(mid, bytes(buf)))
r2 = cmp_trees(TA, TB, dropB=names[len(names) // 3])
say('identity.red_flip (both,differ,onlyA,onlyB,first)', r1)
say('identity.red_drop', r2)
red_ok = r1[1] >= 1 and r2[2] >= 1
say('identity.red_controls_fail_as_they_must', red_ok)
r = cmp_trees(TA, TB)
say('identity.off_vs_rung (both,differ,onlyA,onlyB,first)', r)
say('GATE identity', 'PASS' if red_ok and r[0] > 0 and r[1:4] == (0, 0, 0) else 'FAIL')

# ---------------------------------------------------------------- decode
lodo = os.path.join(ON, 'mod/FO4CSLOD/Commonwealth/Commonwealth.lodo')
L = ND.read_lodo(lodo)
Ton = ND.read_lodi(os.path.join(ON, 'mod/FO4CSLOD/Commonwealth/Commonwealth.lodi'))
Trung = ND.read_lodi(os.path.join(RUNG, 'mod/FO4CSLOD/Commonwealth/Commonwealth.lodi'))
say('decode.on_version', Ton['header']['version']); say('decode.rung_version', Trung['header']['version'])
say('decode.rung_has_stream', bool(Trung.get('vertexGroundFirst')))
F, G = Ton['vertexGroundFirst'], Ton['vertexGround']
FA = Ton['vertexAoFirst']
n = len(Ton['instances'])
say('decode.instances', n); say('decode.stream_bytes_payload', len(G))
same_byte = all(a['ground'] == b['ground'] for a, b in zip(Ton['instances'], Trung['instances'])) and n == len(Trung['instances'])
say('placement byte 0x12 unchanged vs rung', same_byte)
say('stream slice lengths == AO slice lengths', all(F[i + 1] - F[i] == FA[i + 1] - FA[i] for i in range(n)))

# mesh local positions (all clusters: the stream's population)
vx = np.array([(v['px'], v['py'], v['pz']) for v in L['vertices']], dtype=np.float64)
meshLocal = {}


def mesh_local(mi):
    if mi in meshLocal:
        return meshLocal[mi]
    m = L['meshes'][mi]
    cs = L['clusters'][m['clusterFirst']:m['clusterFirst'] + m['clusterCount']]
    lo = min(c['vertexBase'] for c in cs); hi = max(c['vertexBase'] + c['vertexCount'] for c in cs)
    lvl0 = np.zeros(hi - lo, bool)
    for k, c in enumerate(cs):
        cl = L['clusterLods'][m['clusterFirst'] + k]
        if cl['level'] == 0 or (cl['level'] < 0 and cl['parentCount'] == 0):
            lvl0[c['vertexBase'] - lo:c['vertexBase'] - lo + c['vertexCount']] = True
    p = np.array(m['aabbMin']) + vx[lo:hi] / 65535.0 * np.array(m['aabbExtent'])
    meshLocal[mi] = (p, lvl0)
    return meshLocal[mi]


# terrain (audit1 aud_lib.Terrain, restated)
D = LA.Lodt(LODL)
s = D.spc
# every streamed placement's cell, 2 cells of margin (a piece reaches past its origin cell)
sx = [math.floor(Ton['instances'][i]['x'] / 4096.0) for i in range(n) if F[i + 1] > F[i]]
sy = [math.floor(Ton['instances'][i]['y'] / 4096.0) for i in range(n) if F[i + 1] > F[i]]
x0, y0, x1, y1 = min(sx) - 2, min(sy) - 2, max(sx) + 2, max(sy) + 2
x0, y0 = max(x0, D.minX), max(y0, D.minY); x1, y1 = min(x1, D.maxX), min(y1, D.maxY)
say('terrain.lodl_cells', (D.minX, D.minY, D.maxX, D.maxY)); say('terrain.window', (x0, y0, x1, y1))
gx0, gy0 = (x0 - D.minX) * s, (y0 - D.minY) * s
nx, ny = (x1 - x0 + 1) * s + 1, (y1 - y0 + 1) * s + 1
H = np.array([[D.height(gx0 + i, gy0 + j) for i in range(nx)] for j in range(ny)])
wx0, wy0, step = x0 * 4096.0, y0 * 4096.0, 4096.0 / s


def terr(x, y):
    fx = (x - wx0) / step; fy = (y - wy0) / step
    i = np.clip(np.floor(fx).astype(int), 0, nx - 2); j = np.clip(np.floor(fy).astype(int), 0, ny - 2)
    tx = np.clip(fx - i, 0, 1); ty = np.clip(fy - j, 0, 1)
    return (H[j, i] * (1 - tx) * (1 - ty) + H[j, i + 1] * tx * (1 - ty) + H[j + 1, i] * (1 - tx) * ty + H[j + 1, i + 1] * tx * ty)


say('terrain.samples', H.size); print('decoded', round(time.time() - t0, 1), 's')

# ---------------------------------------------------------------- per placement
st, rc, dz_all, own, bytes_ = [], [], [], [], []
lvl0mask = []
amb = nomatch = outside = empty = 0
means = []
for i, inst in enumerate(Ton['instances']):
    ln = F[i + 1] - F[i]
    if ln == 0:
        empty += 1; continue
    b = L['bases'][inst['baseId']]
    reps = {b['rep%d' % k] for k in range(4)} - {NO_MESH}
    cand = [m for m in reps if m < len(L['meshes']) and len(mesh_local(m)[0]) == ln]
    if not cand:
        nomatch += 1; continue
    if len(cand) > 1:
        amb += 1; continue
    p, l0 = mesh_local(cand[0])
    M = np.array(inst['m']).reshape(3, 3)
    w = np.array((inst['x'], inst['y'], inst['z'])) + (p * inst['scaleF']) @ M.T
    cx = np.floor(w[:, 0] / 4096.0); cy = np.floor(w[:, 1] / 4096.0)
    if (cx < x0).any() or (cx > x1).any() or (cy < y0).any() or (cy > y1).any():
        outside += 1; continue
    g = terr(w[:, 0], w[:, 1])
    dz = w[:, 2] - g
    rb = np.rint(np.clip(1.0 - dz / 256.0, 0, 1) * 255.0)
    sv = np.frombuffer(bytes(G[F[i]:F[i + 1]]), np.uint8).astype(np.float64)
    st.append(sv); rc.append(rb); dz_all.append(dz); own.append(np.full(ln, inst['ground'], np.float64)); lvl0mask.append(l0)
    means.append((sv.mean(), inst['ground'], sv[l0].mean() if l0.any() else sv.mean(), sv.max() - sv.min(), i, ln))
say('placements.measured', len(means)); say('placements.empty_slice', empty)
say('placements.skipped_ambiguous_slot', amb); say('placements.skipped_no_mesh_match', nomatch)
say('placements.skipped_outside_terrain_window', outside)
S = np.concatenate(st); R = np.concatenate(rc); DZ = np.concatenate(dz_all); O = np.concatenate(own)
say('vertices.measured', int(len(S)))


def gates(tag, V):
    d = np.abs(V - R)
    share1 = float((d <= 1).mean())
    say('%s.recompute share |d|<=1' % tag, round(share1, 6))
    say('%s.recompute share |d|==0' % tag, round(float((d == 0).mean()), 6))
    say('%s.recompute max |d|' % tag, float(d.max()))
    say('%s.recompute vertices |d|>1' % tag, int((d > 1).sum()))
    near16 = np.abs(DZ) <= 16.0
    near_above16 = (DZ >= 0) & (DZ <= 16.0)
    law5 = DZ <= 5.0
    high = DZ > 256.0
    say('%s.phys vertices |dz|<=16' % tag, int(near16.sum()))
    say('%s.phys  of those reading >=250' % tag, int((V[near16] >= 250).sum()))
    say('%s.phys vertices 0<=dz<=16 (above, within 16)' % tag, int(near_above16.sum()))
    say('%s.phys  of those reading >=250' % tag, int((V[near_above16] >= 250).sum()))
    say('%s.phys  of those reading >=239 (the law at 16 u)' % tag, int((V[near_above16] >= 239).sum()))
    say('%s.phys vertices dz<=5 (the law gives >=250 there)' % tag, int(law5.sum()))
    say('%s.phys  of those reading >=250' % tag, int((V[law5] >= 250).sum()))
    say('%s.phys vertices dz>256' % tag, int(high.sum()))
    say('%s.phys  of those reading 0' % tag, int((V[high] == 0).sum()))
    ok_rec = share1 >= 0.999
    ok_hi = (V[high] == 0).all()
    ok_law = (V[law5] >= 250).all() and (V[near_above16] >= 239).all()
    ok_lit16 = (V[near16] >= 250).all()
    return ok_rec, ok_hi, ok_law, ok_lit16


ok = gates('stream', S)
say('GATE recompute >=99.9% within 1', 'PASS' if ok[0] else 'FAIL')
say('GATE physical >256 u reads 0', 'PASS' if ok[1] else 'FAIL')
say('GATE physical law (dz<=5 >=250, 0..16 >=239)', 'PASS' if ok[2] else 'FAIL')
say('GATE physical as briefed (|dz|<=16 >=250)', 'PASS' if ok[3] else 'FAIL')
mm = np.array(means)
dmean = np.abs(mm[:, 0] - mm[:, 1])
say('placement mean vs 0x12: share within 2', round(float((dmean <= 2).mean()), 6))
say('placement mean vs 0x12: share within 4', round(float((dmean <= 4).mean()), 6))
say('placement mean vs 0x12: mean |d|', round(float(dmean.mean()), 4))
say('placement mean vs 0x12: max |d|', round(float(dmean.max()), 3))
say('placement mean vs 0x12: corr', round(float(np.corrcoef(mm[:, 0], mm[:, 1])[0, 1]), 6))
d0 = np.abs(mm[:, 2] - mm[:, 1])
say('placement level-0 mean vs 0x12: share within 2', round(float((d0 <= 2).mean()), 6))
say('placement level-0 mean vs 0x12: mean |d|', round(float(d0.mean()), 4))
say('GATE placement mean within 2 of 0x12 (all)', 'PASS' if (dmean <= 2).all() else 'FAIL')
say('placements whose stream spans >=128 levels', int((mm[:, 3] >= 128).sum()))
bad = dmean > 2
say('placements off by more than 2', int(bad.sum()))
say('  of those spanning >=128 levels', int((bad & (mm[:, 3] >= 128)).sum()))
say('  share of all placements spanning >=128 that are off by >2', round(float((bad & (mm[:, 3] >= 128)).sum() / max(1, (mm[:, 3] >= 128).sum())), 4))
say('  of those with stream all 0 or all 255 (flat)', int((bad & (mm[:, 3] == 0)).sum()))
byModel = {}
for row, dd in zip(mm[bad], dmean[bad]):
    ii = int(row[4]); bs = L['bases'][Ton['instances'][ii]['baseId']]
    nm = L['string_at'](bs['modelStringOffset']).replace('\\', '/').split('/')[-1]
    byModel.setdefault(nm, []).append(dd)
top = sorted(byModel.items(), key=lambda kv: -len(kv[1]))[:12]
say('off-by->2 models (count, mean |d|)', [(k, len(v), round(float(np.mean(v)), 1)) for k, v in top])
worst = np.argsort(-dmean)[:8]
say('worst placements (inst, verts, stream mean, byte, span)',
    [(int(mm[w, 4]), int(mm[w, 5]), round(float(mm[w, 0]), 1), int(mm[w, 1]), int(mm[w, 3])) for w in worst])
# red controls
print('-- red: the old per-vertex value (placement byte on every vertex)')
okr = gates('red_old', O)
say('RED old value fails recompute', not okr[0]); say('RED old value fails >256 reads 0', not okr[1])
say('RED old value fails law', not okr[2])
print('-- red: stream + 2')
okp = gates('red_plus2', np.clip(S + 2, 0, 255))
say('RED stream+2 fails recompute', not okp[0])

# ---------------------------------------------------------------- 5. size
lon = os.path.getsize(os.path.join(ON, 'mod/FO4CSLOD/Commonwealth/Commonwealth.lodi'))
loff = os.path.getsize(os.path.join(OFF, 'mod/FO4CSLOD/Commonwealth/Commonwealth.lodi'))
say('size.lodi_off', loff); say('size.lodi_on', lon); say('size.cost_bytes', lon - loff)
say('size.cost_share_of_lodi', round((lon - loff) / loff, 4))
say('size.header_vertexGroundBytes', Ton['header'].get('vertexGroundBytes'))
say('size.header_vertexAoBytes', Ton['header'].get('vertexAoBytes'))
if os.path.exists(WHOLE):
    b = open(WHOLE, 'rb').read(0x200)
    ver = struct.unpack_from('<I', b, 4)[0]
    aob = struct.unpack_from('<I', b, 0xFC)[0]
    say('size.whole_map_lodi', os.path.getsize(WHOLE)); say('size.whole_map_lodi_version', ver)
    say('size.whole_map_vertexAoBytes (= the stream estimate)', aob)
    est = (aob + 4095) // 4096 * 4096
    say('size.whole_map_estimate_cost_bytes (4096-aligned)', est)
    say('size.whole_map_estimate_share', round(est / os.path.getsize(WHOLE), 4))
json.dump(out, open(OUT, 'w'), indent=1, default=str)
print('done', round(time.time() - t0, 1), 's')
