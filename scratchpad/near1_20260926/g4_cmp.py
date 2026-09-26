"""G4 comparator: every file of g4/old against g4/new, byte for byte (.log and the .lodb provenance
sidecar excluded: they carry the exe size, the clock and the output path). --sabotage flips byte 0x04
(the version word) of the first .lodo in memory: the comparator must go red."""
import os
import sys

sab = '--sabotage' in sys.argv
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'g4'))
tot = same = 0
bad = []
flipped = False
for root, _, files in os.walk('old'):
    for f in sorted(files):
        if f.endswith(('.log', '.lodb')):
            continue
        a = os.path.join(root, f)
        b = os.path.join('new', os.path.relpath(a, 'old'))
        tot += 1
        if not os.path.exists(b):
            bad.append('%s: absent from new' % b)
            continue
        x = open(a, 'rb').read()
        y = bytearray(open(b, 'rb').read())
        if sab and not flipped and f.endswith('.lodo'):
            y[4] ^= 1
            flipped = True
        if x == bytes(y):
            same += 1
            continue
        d = [i for i in range(min(len(x), len(y))) if x[i] != y[i]]
        bad.append('%s: %d vs %d bytes, %d differ, first %s' % (a, len(x), len(y), len(d), [hex(i) for i in d[:4]]))
extra = [os.path.join(r, f) for r, _, fs in os.walk('new') for f in fs
         if not os.path.exists(os.path.join('old', os.path.relpath(os.path.join(r, f), 'new')))]
ok = not bad and not extra
print('G4 %s: %d files compared, %d identical, %d only in new%s' % ('PASS' if ok else 'FAIL', tot, same, len(extra),
      ''.join('\n  ' + s for s in bad[:10])))
sys.exit(0 if ok else 1)
