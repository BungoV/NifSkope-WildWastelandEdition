"""BAKE2, ruling (a), the post-build gate G2 for one Nuka-World bake: the wide-scale data is there and only there.
Pre-registered 2026-09-25 before the build. Independent of the exe: the patched Python decoder reads the .lodi, and
the expected set comes from the PLUGINS (overscale.py over his load order: refs with XSCL > 7.99988 whose STAT base
carries an MNAM LOD model).
  W0  the bake exited 0 and wrote a .lodi (the rung exe refuses here: that is the RED half)
  W1  the independent decoder accepts it, and it is version 10
  W2  the set of refFormIds carrying bit 7 == the plugin's over-line LOD refs, both directions
  W3  each wide instance decodes to its XSCL within 1/16384 (the step is 1/8192 on both sides)
  W4  no instance without bit 7 decodes above 7.99988; no instance with it at or below
  W5  the census line 'native-wide-scale: N of M' agrees with W2's count and says version 10
usage: python widescale_check.py <lodi> <chunks.log> <overscale.txt> [rc]
Refuter: run on a v7 file (pre-war) with the Nuka-World list -> RED on W1/W2."""
import sys, re
sys.path.insert(0, 'E:/Projects/NifskopeWWE-bake2/tests/spells')
import lodgen_native_decode as D

lodi, log, osc = sys.argv[1:4]
rc = int(sys.argv[4]) if len(sys.argv) > 4 else 0
ok = True
def line(n, good, d):
    global ok; ok &= bool(good); print('%s %s: %s' % ('PASS' if good else 'FAIL', n, d))

want = {}
for l in open(osc):
    m = re.match(r'\s+ref ([0-9a-f]{8}) base [0-9a-f]{8} scale ([0-9.]+) \(.*\) lod True', l)
    if m: want[int(m.group(1), 16)] = float(m.group(2))
line('W0 bake rc', rc == 0, 'rc %d' % rc)
try:
    T = D.read_lodi(lodi)
except Exception as e:
    line('W1 decoder accepts', False, '%s: %s' % (type(e).__name__, e)); print('WIDESCALE G2 FAIL'); sys.exit(1)
v = T['header']['version']
line('W1 decoder accepts, version 10', v == 10, 'version %d, %d instances' % (v, len(T['instances'])))
wide = {}
for r, c in zip(T['instances'], T['cold']):
    if r['flags'] & 0x80: wide.setdefault(c['refFormId'], []).append(r['scaleF'])
line('W2 wide refs == plugin over-line LOD refs', set(wide) == set(want),
     'file %s; plugins %s' % (sorted('%08X' % k for k in wide), sorted('%08X' % k for k in want)))
bad = [(k, s, want.get(k)) for k, ss in wide.items() for s in ss if k not in want or abs(s - want[k]) > 1 / 16384.0]
line('W3 wide scales decode to XSCL within 1/16384', not bad and wide,
     '; '.join('%08X %.5f vs %.6f' % (k, max(ss), want.get(k, 0)) for k, ss in sorted(wide.items())) + (' BAD %s' % bad if bad else ''))
over = sum(1 for r in T['instances'] if not r['flags'] & 0x80 and r['scaleF'] > 65535 / 8192.0)
under = sum(1 for r in T['instances'] if r['flags'] & 0x80 and r['scaleF'] <= 65535 / 8192.0)
line('W4 bit 7 exactly on the scales above 7.99988', over == 0 and under == 0, 'narrow above %d, wide at/below %d' % (over, under))
txt = open(log, 'rb').read().decode('utf-8', 'replace')
m = re.search(r'native-wide-scale: (\d+) of (\d+) placements .*?\.lodi version (\d+)', txt)
n = sum(len(x) for x in wide.values())
line('W5 census agrees', m and int(m.group(1)) == n and int(m.group(2)) == len(T['instances']) and int(m.group(3)) == v,
     m.group(0) if m else 'no native-wide-scale line')
print('WIDESCALE G2 %s' % ('PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
