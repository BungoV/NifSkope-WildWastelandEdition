"""Lane MODULES1 RESUME 15: the bars of tests/airrays/cli_gate.sh over its stdout files. Prints AIRRAYS CLI GATE OK or FAILED."""
import re, sys

out = sys.argv[1]
def line(name):
    try:
        t = open(out + '/' + name + '/stdout.txt').read()
    except OSError:
        return None
    m = re.search(r'(\d+) froxels lit, (\d+) with no gpu lamp; \|gpu - exact\| mean ([\d.]+) max ([\d.]+), within 0.05 ([\d.]+) %, '
                  r'within 0.10 ([\d.]+) %', t)
    return None if not m else dict(lit=int(m[1]), missing=int(m[2]), mean=float(m[3]), max=float(m[4]), w10=float(m[6]))

bad = []
s, rs, rh = line('still'), line('red_shadow'), line('red_hist')
if not s:
    bad.append('still: no result line')
else:
    print('still: %d froxels lit, missing %d, mean %.4f, within 0.10 %.2f %%' % (s['lit'], s['missing'], s['mean'], s['w10']))
    if s['lit'] < 100: bad.append('still: under 100 lit froxels (the scene did not reach the grid)')
    if s['missing'] != 0: bad.append('still: froxels with no gpu lamp')
    if s['mean'] > 0.02: bad.append('still: mean > 0.02')
    if s['w10'] < 99.0: bad.append('still: within 0.10 < 99 %')
if not rs or rs['mean'] <= 0.05:
    bad.append('red_shadow did not bite' + ('' if rs else ' (no result line)'))
else:
    print('red_shadow: mean %.4f (bites)' % rs['mean'])
if not rh or not s or rh['mean'] <= 3 * s['mean']:
    bad.append('red_hist did not bite' + ('' if rh else ' (no result line)'))
else:
    print('red_hist: mean %.4f (bites)' % rh['mean'])
try:
    c = re.search(r'airrays cost: .*', open(out + '/cost/stdout.txt').read())
    print(c[0] if c else 'cost: no line')
except OSError:
    print('cost: no stdout')
for b in bad:
    print('FAIL ' + b)
print('AIRRAYS CLI GATE ' + ('FAILED' if bad else 'OK'))
sys.exit(1 if bad else 0)
