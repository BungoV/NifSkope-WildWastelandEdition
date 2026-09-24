import sys, os, collections
sys.path.insert(0, 'tests/spells')
import lodgen_native_decode as D
BS = chr(92)
V = 'scratchpad/lodiv7_20260918/v7/nat/FO4CSLOD/Commonwealth/Commonwealth.'
L = D.read_lodo(V + 'lodo'); T = D.read_lodi(V + 'lodi'); S = L['string_at']
h = T['header']; n = h['instanceCount']
inst, cold, grp = T['instances'], T['cold'], T['group']
bm = [S(b['modelStringOffset']) for b in L['bases']]
bflags = [b['flags'] for b in L['bases']]

chunk_of = [0] * n
for ci, c in enumerate(T['chunks']):
    for i in range(c['instanceFirst'], c['instanceFirst'] + c['instanceCount']):
        chunk_of[i] = ci
gk = [(chunk_of[i], grp[i]) for i in range(n)]
size = collections.Counter(gk)

print('=== (b) the one split SCOL ===')
scol = collections.defaultdict(list)
for i in range(n):
    if cold[i]['scolPart'] >= 0:
        scol[cold[i]['refFormId']].append(i)
for ref, ii in scol.items():
    if len({gk[i] for i in ii}) != 1:
        print('  ref 0x%08X: %d parts over chunks %s, groups %s'
              % (ref, len(ii), sorted({chunk_of[i] for i in ii}), sorted({gk[i] for i in ii})))
withinchunk_bad = [ref for ref, ii in scol.items()
                   if any(len({gk[i] for i in ii if chunk_of[i] == c}) != 1
                          for c in {chunk_of[i] for i in ii})]
print('  SCOL refs split WITHIN one chunk:', len(withinchunk_bad))

print('=== (c) the merged trees ===')
# a tree by the .lodo base FLAGS, not by the seed byte
treebit = None
for b in range(16):
    bit = 1 << b
    withbit = [j for j in range(len(bm)) if bflags[j] & bit]
    if withbit and all('tree' in bm[j].lower() or 'landscape' in bm[j].lower() for j in withbit):
        treebit = bit
        print('  base flag bit 0x%04X looks like TREE: %d bases, e.g. %s'
              % (bit, len(withbit), os.path.basename(bm[withbit[0]])))
seedtrees = [i for i in range(n) if inst[i]['seed'] != 0]
flagtrees = [i for i in range(n) if treebit and bflags[inst[i]['baseId']] & treebit] if treebit else []
print('  trees by seed byte: %d; trees by base flag: %d' % (len(seedtrees), len(flagtrees)))
for label, pop in (('seed', seedtrees), ('flag', flagtrees)):
    merged = [i for i in pop if size[gk[i]] > 1]
    print('  [%s] %d of %d sit in a group of >1' % (label, len(merged), len(pop)))
    for i in merged[:8]:
        print('      inst %d ref 0x%08X scolPart %d base %s -- group %s has %d members'
              % (i, cold[i]['refFormId'], cold[i]['scolPart'],
                 os.path.basename(bm[inst[i]['baseId']]), gk[i], size[gk[i]]))

print('=== sky vs AO, same file, same placements ===')
for name, first, data, byt in (('AO ', T['vertexAoFirst'], T['vertexAo'], 'ao'),
                               ('SKY', T['vertexSkyFirst'], T['vertexSky'], 'sky')):
    if not first:
        print('  %s: absent' % name); continue
    d = []
    for i in range(n):
        lo, hi = first[i], first[i + 1]
        if hi > lo:
            d.append(abs(sum(data[lo:hi]) / float(hi - lo) - inst[i][byt]))
    d.sort(); t = len(d)
    print('  %s n=%d  median %.2f  within2 %.2f%%  within4 %.2f%%  within8 %.2f%%  within16 %.2f%%  max %.1f'
          % (name, t, d[t // 2], 100.*sum(1 for x in d if x <= 2)/t, 100.*sum(1 for x in d if x <= 4)/t,
             100.*sum(1 for x in d if x <= 8)/t, 100.*sum(1 for x in d if x <= 16)/t, d[-1]))

print('=== constant-slice buildings ===')
f, data = T['vertexSkyFirst'], T['vertexSky']
arch = [i for i in range(n) if 'architecture' in bm[inst[i]['baseId']].replace('/', BS).lower().split(BS)]
const = [i for i in arch if f[i + 1] > f[i] and len(set(data[f[i]:f[i + 1]])) == 1]
print('  %d architecture placements, %d with a constant slice' % (len(arch), len(const)))
vc = collections.Counter(f[i + 1] - f[i] for i in const)
print('  their vertex counts:', vc.most_common(6))
big = [i for i in const if f[i + 1] - f[i] > 8]
print('  constant AND more than 8 vertices: %d%s' % (len(big),
      '' if not big else ' e.g. %s (%d verts, value %d)'
      % (os.path.basename(bm[inst[big[0]]['baseId']]), f[big[0]+1]-f[big[0]], data[f[big[0]]])))
