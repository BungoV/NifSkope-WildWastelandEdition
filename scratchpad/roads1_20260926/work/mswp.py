import sys, os, pickle, struct, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import roadgeo as rg
R = rg.Reader()
pl = pickle.load(open('out/road_placements.pkl', 'rb'))
c = collections.Counter()
for d in pl:
    if d['decision'] != 'stamped':
        continue
    for k in ('xmsp', 'mods'):
        if d[k]:
            c[(k, rg.is_sidewalk(d['modl']), d[k], d['modl'].lower())] += 1
seen = set()
for (k, sw, f, m), n in sorted(c.items(), key=lambda kv: -kv[1]):
    print('%s sw=%d %08X n=%d %s' % (k, sw, f, n, m))
    if f in seen:
        continue
    seen.add(f)
    b = R.W.base(f)
    if not b:
        print('   (no record)'); continue
    t, flags, fl, pi, masters = b
    edid = ''
    pairs = []
    cur = None
    for ft, p in fl:
        if ft == b'EDID': edid = p.split(b'\0')[0].decode('latin-1')
        elif ft == b'BNAM': cur = [p.split(b'\0')[0].decode('latin-1'), '']
        elif ft == b'SNAM' and cur is not None: cur[1] = p.split(b'\0')[0].decode('latin-1'); pairs.append(cur); cur = None
    print('   %s %s %s' % (t, edid, pairs))
