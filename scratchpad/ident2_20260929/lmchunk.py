"""IDENT2: where a landmark's group lands in the FILE, chunk by chunk.
usage: python lmchunk.py <base (no extension)> <dump> [landmarks.txt]
For every landmark rule (same matching as the emitter: model path prefix + optional X/Y centre/radius), takes the
emitter group (dump root) of its pieces, maps every member (ref, scolPart) to its .lodi instances, and prints per
16,384 u chunk: the instances, the distinct group ids they carry in the file, and how many OTHER instances of that
chunk carry the same id (0 = the id is the landmark's alone there)."""
import sys, os, collections
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'tests', 'spells'))
import lodgen_native_decode as ND
import groups

base, dump = sys.argv[1], sys.argv[2]
lmf = sys.argv[3] if len(sys.argv) > 3 else os.path.join(os.path.dirname(__file__), '..', '..', 'res', 'lodgen_landmarks.txt')


def norm(p):
    p = p.strip().lower().replace('\\', '/').lstrip('/')
    if p.startswith('meshes/'):
        p = p[7:]
    if p.startswith('lod/'):
        p = p[4:]
    return p


rules = []
for line in open(lmf, encoding='utf-8'):
    s = line.strip()
    if not s or s.startswith('#'):
        continue
    f = [x.strip() for x in s.split('|')]
    c = f[2].split() if len(f) > 2 else []
    rules.append((f[0], [norm(x) for x in f[1].split(';') if x.strip()], tuple(float(v) for v in c) if c else None))

h, P, C = groups.load(dump)
T = ND.read_lodi(base + '.lodi')
cold, grp, chunks, H = T['cold'], T['group'], T['chunks'], T['header'] if 'header' in T else T['h']
w = H['chunkEast'] - H['chunkWest'] + 1
chunk_of = [None] * len(cold)
for ci, ch in enumerate(chunks):
    cx, cy = H['chunkWest'] + ci % w, H['chunkNorth'] - ci // w
    for ii in range(ch['instanceFirst'], ch['instanceFirst'] + ch['instanceCount']):
        chunk_of[ii] = (cx, cy)
key_to_ii = collections.defaultdict(list)
for ii, c in enumerate(cold):
    key_to_ii[(c['refFormId'], c['scolPart'])].append(ii)
print('file', base, 'instances', len(cold), 'group table', len(grp))
for name, pre, cen in rules:
    hit = []
    for p in P:
        nm = p['name']
        a = nm.find(' (')
        model = norm(nm[a + 2:-1]) if a >= 0 and nm.endswith(')') else ''
        if not any(model.startswith(x) for x in pre):
            continue
        if cen and ((p['x'] - cen[0]) ** 2 + (p['y'] - cen[1]) ** 2) ** 0.5 > cen[2]:
            continue
        hit.append(p)
    roots = collections.Counter(p['root'] for p in hit)
    print('\n%s: %d pieces in the dump, %d emitter group(s)' % (name, len(hit), len(roots)))
    if not hit:
        continue
    for r, k in roots.most_common():
        keys = [(p['ref'], p['part']) for p in P if p['root'] == r]
        iis = [ii for kk in keys for ii in key_to_ii.get(kk, [])]
        per = collections.defaultdict(list)
        for ii in iis:
            per[chunk_of[ii]].append(ii)
        mine = set(iis)
        print('  group root %d: %d members (%d landmark), %d file instances in %d chunk(s)' % (r, len(keys), k, len(iis), len(per)))
        for ch, v in sorted(per.items()):
            ids = collections.Counter(grp[ii] for ii in v)
            others = 0
            if ch is not None:
                ci = (H['chunkNorth'] - ch[1]) * w + (ch[0] - H['chunkWest'])
                c0 = chunks[ci]
                for jj in range(c0['instanceFirst'], c0['instanceFirst'] + c0['instanceCount']):
                    if jj not in mine and grp[jj] in ids:
                        others += 1
            print('    chunk %s: %d instances, file group id(s) %s, other instances sharing the id %d' % (
                ch, len(v), ' '.join('%d:%d' % kv for kv in sorted(ids.items())), others))
        allids = collections.Counter(grp[ii] for ii in iis)
        foreign = sum(1 for jj in range(len(grp)) if jj not in mine and grp[jj] in allids)
        print('  FILE-WIDE: %d id(s) over %d chunk(s) %s; other instances sharing them anywhere %d  [%s]' % (
            len(allids), len(per), ' '.join('%d:%d' % kv for kv in sorted(allids.items())), foreign,
            'ONE ID' if len(allids) == 1 and foreign == 0 else 'NOT ONE ID'))
