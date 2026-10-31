#!/usr/bin/env python3
"""Lane RAINFOG2: the BUILT exe's weather reader against the independent census (tests/rainfog2/census_precip.tsv,
notes/fix1031a/rainfog2/tools/census_precip.py: its own ESM walk). Needs the game's Data folder; no window
(`NifSkope.exe -no-gui weather --plugins <the 7 masters> --precip`).

    python -I tests/rainfog2/exe_census.py <exe> <Data dir> [out.txt]
Every vanilla WTHR: the exe's EDID, flags, MNAM, SPGD type and density == the census's; the record rule's kind
(rain 10, snow 0, none 97) and every scale 1; the old class-flag kind (flagkind) gives rain 14 / snow 10 (the red the
rule replaced). Exit 0 = all match.
"""
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
exe, data = sys.argv[1], sys.argv[2]
outp = sys.argv[3] if len(sys.argv) > 3 else None
MASTERS = ['Fallout4.esm', 'DLCRobot.esm', 'DLCworkshop01.esm', 'DLCCoast.esm', 'DLCworkshop02.esm',
           'DLCworkshop03.esm', 'DLCNukaWorld.esm']
plugins = ','.join(os.path.join(data, m).replace('\\', '/') for m in MASTERS if os.path.exists(os.path.join(data, m)))
r = subprocess.run([exe, '-no-gui', 'weather', '--plugins', plugins, '--data', data, '--precip'], capture_output=True,
                   text=True, timeout=600, stdin=subprocess.DEVNULL)
txt = r.stdout
if outp:
    open(outp, 'w').write(txt + '\n# stderr\n' + r.stderr[-4000:])
rows = {}
for l in txt.split('\n'):
    m = re.match(r'precip id=(\S+) edid=(\S*) flags=(\d+) mnam=(\d) spgd=(\S+) type=(-?\d+) density=(\S+) kind=(\w+) '
                 r'scale=(\S+) flagkind=(\S+)', l)
    if m:
        rows[m.group(2)] = m.groups()
cen = [l.rstrip('\n').split('\t') for l in open(os.path.join(HERE, 'census_precip.tsv'))][1:]
bad = []
kinds, fkinds = {}, {}
for c in cen:
    e = rows.get(c[0])
    if not e:
        bad.append('missing ' + c[0])
        continue
    has = c[5] != ''
    want_type = int(c[7]) if has else -2
    want_den = float(c[6]) if has else -1.0
    if (int(e[2]) != int(c[2]) or (e[3] == '1') != has or int(e[5]) != want_type or abs(float(e[6]) - want_den) > 1e-6
            or (has and e[4] != c[5])):
        bad.append('%s: exe %s census flags %s spgd %s type %s density %s' % (c[0], e, c[2], c[5], want_type, want_den))
    kinds[e[7]] = kinds.get(e[7], 0) + 1
    for k in e[9].split('+'):
        fkinds[k] = fkinds.get(k, 0) + 1
    if float(e[8]) != 1.0:
        bad.append('%s scale %s' % (c[0], e[8]))
ok = (not bad and len(rows) >= len(cen) == 107 and kinds.get('rain', 0) == 10 and kinds.get('snow', 0) == 0
      and fkinds.get('rain', 0) == 14 and fkinds.get('snow', 0) == 10)
print('EXE-CENSUS %s  exe rc %d, %d precip lines, census %d; record kinds %s; old flag kinds %s; mismatches %d%s' % (
    'PASS' if ok else 'FAIL', r.returncode, len(rows), len(cen), dict(sorted(kinds.items())), dict(sorted(fkinds.items())),
    len(bad), (': ' + '; '.join(bad[:5])) if bad else ''))
sys.exit(0 if ok else 1)
