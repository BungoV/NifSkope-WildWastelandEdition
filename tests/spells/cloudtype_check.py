"""Lane CLOUDTYPE2 (CLOUD6d's leftover row): the Low band's cloud type rule (src/gl/cloudmath.h lowTypeOf) judged over the
107 vanilla WTHR records' class inputs (tests/cloudtype/weathers.tsv, written by notes/fix1031a/cloud7/tools/census_type.py
from the masters: DATA flags byte 11, DATA thunder byte 10, the SNAM sounds of type 3 = thunder, MNAM precipitation, and
CLOUD6d's Low coverage). No GL, no NifSkope.

    python -I tests/spells/cloudtype_check.py <scratch dir> [--red thunderbyte] [--only ARNKBWD] [--src <dir of cloudmap.cpp/cloudvol.cpp>]

The judged rule = the one cloudvol.cpp draws for this env: the thunder sounds unless WW_CLOUDVOL_TYPERULE is 0 / off
(defaultOnSwitch, through the dump) or WW_CLOUDVOL_RED holds thunderbyte (or --red thunderbyte): then CLOUD5's rule.
Gates (bars written before the first run, 10-31 02:10):
  A agree     type 0.9 (cumulonimbus) <=> the weather thunders (a thunder SNAM: the game plays thunder there, and CLOUD6f
              flashes lightning in exactly those), in all 107. CLOUD5's rule: 92 of 107 (FAIL).
  R radstorm  every weather whose EDID names a radstorm and that thunders is 0.9 (not the Snow flag's 0.3).
  N rain      every Rainy-flag weather that does not thunder is 0.75 (rain is not a storm by itself); at least one exists.
  K kept      every weather that neither thunders nor carries the Rainy flag keeps CLOUD5's type and spread exactly
              (fair 0.6, cloudy 0.45, snow 0.3): the rule moves only the storm signal.
  B byte      INFO + a guard: the old predicate (thunder byte > 0) is true in >= 105 of 107 weathers (it decides nothing):
              if a later census finds it separating, this gate FAILS so the rule is re-derived.
  W wiring    cloudmap.cpp hands lowTypeOf the thunder sounds through wwCloudVolTypeRule(), cs() reads
              WW_CLOUDVOL_TYPERULE through defaultOnSwitch, wwCloudVolTypeRule() honours RED=thunderbyte. Red: the base
              tree's sources (run.sh: git show 563dcf97) FAIL.
  D default   the switch rule: unset / empty / 1 / on -> on, 0 / off / false / no -> off.
Exit 0 only when every gate run passes.
"""
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
TREE = os.path.dirname(os.path.dirname(HERE))
args = sys.argv[1:]
scratch = args[0] if args and not args[0].startswith('--') else os.path.join(TREE, '..', 'cloudtype_scratch')
red = args[args.index('--red') + 1] if '--red' in args else ''
only = set(args[args.index('--only') + 1]) if '--only' in args else set('ARNKBWD')
src = args[args.index('--src') + 1] if '--src' in args else os.path.join(TREE, 'src', 'gl')
FIX = os.path.join(TREE, 'tests', 'cloudtype', 'weathers.tsv')
os.makedirs(scratch, exist_ok=True)
results = []

ENV = dict(os.environ, PATH='C:/msys64/ucrt64/bin;' + os.environ['PATH'])
DUMP = os.path.join(scratch, 'cloudtype_dump.exe')
r = subprocess.run(['C:/msys64/ucrt64/bin/g++.exe', '-O2', '-std=c++17', '-I', os.path.join(TREE, 'src'),
                    os.path.join(TREE, 'tests', 'cloudtype', 'cloudtype_dump.cpp'), '-o', DUMP], env=ENV,
                   capture_output=True, text=True)
if r.returncode:
    raise SystemExit('BUILD FAIL ' + r.stderr[-2000:])


def dump(*a):
    return subprocess.run([DUMP] + [str(x) for x in a], env=ENV, capture_output=True, text=True,
                          check=True).stdout.split('\n')


def switch_on(value):
    return dump('switch', *([] if value is None else [value]))[0].split()[1] == 'on'


def gate(name, ok, msg):
    results.append(ok)
    print('%-11s %s  %s%s' % (name, 'PASS' if ok else 'FAIL', msg, ' (red %s)' % red if red else ''), flush=True)


env_v = os.environ.get('WW_CLOUDVOL_TYPERULE')
env_red = 'thunderbyte' in [w.strip() for w in os.environ.get('WW_CLOUDVOL_RED', '').lower().split(',')]
NEW = switch_on(None if env_v is None else env_v.strip().lower()) and not env_red and red != 'thunderbyte'
print('RULE        INFO  judged: %s (WW_CLOUDVOL_TYPERULE=%s, red %s)' % (
    'the thunder sounds' if NEW else "CLOUD5's thunder byte", env_v, red or env_red), flush=True)

rows = []
for l in dump('rows', FIX):
    w = l.split()
    if not w or w[0] != 'row':
        continue
    e = dict(edid=w[1], flags=int(w[2]), byte=int(w[3]), snd=int(w[4]), nt=float(w[5]), ns=float(w[6]), ot=float(w[7]),
             os=float(w[8]))
    e['t'], e['s'] = (e['nt'], e['ns']) if NEW else (e['ot'], e['os'])
    rows.append(e)
n = len(rows)
if n != 107:
    raise SystemExit('FIXTURE %d rows (want 107)' % n)
storm = lambda e: abs(e['t'] - 0.9) < 1e-6

if 'A' in only:
    agree = [e for e in rows if storm(e) == (e['snd'] > 0)]
    bad = [e['edid'] for e in rows if storm(e) != (e['snd'] > 0)]
    gate('A agree', len(agree) == n, 'type 0.9 <=> a thunder SNAM in %d of %d (%d thunder)%s' % (
        len(agree), n, sum(1 for e in rows if e['snd'] > 0), ('; wrong: ' + ', '.join(bad[:8])) if bad else ''))
if 'R' in only:
    rad = [e for e in rows if 'radstorm' in e['edid'].lower() and e['snd'] > 0]
    bad = ['%s %.2f' % (e['edid'], e['t']) for e in rad if not storm(e)]
    gate('R radstorm', bool(rad) and not bad, '%d thundering radstorms typed 0.9: %d%s' % (
        len(rad), len(rad) - len(bad), ('; wrong: ' + ', '.join(bad[:6])) if bad else ''))
if 'N' in only:
    rain = [e for e in rows if e['flags'] & 4 and e['snd'] == 0]
    bad = ['%s %.2f' % (e['edid'], e['t']) for e in rain if abs(e['t'] - 0.75) > 1e-6]
    gate('N rain', bool(rain) and not bad, '%d Rainy-flag weathers that do not thunder typed 0.75: %d%s' % (
        len(rain), len(rain) - len(bad), ('; wrong: ' + ', '.join(bad[:6])) if bad else ''))
if 'K' in only:
    keep = [e for e in rows if e['snd'] == 0 and not e['flags'] & 4]
    bad = ['%s %.2f/%.2f vs %.2f/%.2f' % (e['edid'], e['t'], e['s'], e['ot'], e['os']) for e in keep
           if abs(e['t'] - e['ot']) > 1e-6 or abs(e['s'] - e['os']) > 1e-6]
    gate('K kept', bool(keep) and not bad, '%d quiet non-rainy weathers keep CLOUD5\'s type + spread: %d%s' % (
        len(keep), len(keep) - len(bad), ('; moved: ' + ', '.join(bad[:6])) if bad else ''))
if 'B' in only:
    nb = sum(1 for e in rows if e['byte'] > 0)
    vals = sorted(set(e['byte'] for e in rows if e['snd'] > 0) & set(e['byte'] for e in rows if e['snd'] == 0))
    gate('B byte', nb >= 105, 'thunder byte > 0 in %d of %d (byte values shared by thundering and quiet weathers: %s)'
         % (nb, n, vals))
if 'W' in only:
    cm = open(os.path.join(src, 'cloudmap.cpp'), encoding='latin1').read()
    cv = open(os.path.join(src, 'cloudvol.cpp'), encoding='latin1').read()
    w1 = re.search(r'wwLookdevWeatherClass\(\s*flags,\s*thunder,\s*&sounds\s*\)', cm) is not None and \
        re.search(r'lowTypeOf\([^;]*wwCloudVolTypeRule\(\)\s*\?\s*sounds\s*:\s*-1\s*\)\s*;', cm) is not None
    m = re.search(r'CvState & cs\(\)\s*\{(.*?)\n\}', cv, re.S)
    w2 = bool(m) and '"WW_CLOUDVOL_TYPERULE"' in m.group(1) and 'defaultOnSwitch' in m.group(1)
    w3 = re.search(r'bool wwCloudVolTypeRule\(\)[^}]*red\( "thunderbyte" \)', cv, re.S) is not None
    gate('W wiring', w1 and w2 and w3, 'cloudmap passes the sounds via wwCloudVolTypeRule() %s; cs() reads '
         'WW_CLOUDVOL_TYPERULE via defaultOnSwitch %s; RED=thunderbyte honoured %s (src %s)' % (w1, w2, w3, src))
if 'D' in only:
    want = {None: True, '': True, '1': True, 'on': True, 'yes': True, '0': False, 'off': False, 'false': False, 'no': False}
    bad = [k for k, v in want.items() if switch_on(k) != v]
    gate('D default', not bad, 'unset/empty/1/on/yes -> on, 0/off/false/no -> off%s' % ((' (wrong: %s)' % bad) if bad else ''))

print('CLOUDTYPE-CHECK %s%s' % ('PASS' if all(results) else 'FAIL', (' (red %s)' % red) if red else ''))
if __name__ == '__main__':
    sys.exit(0 if all(results) else 1)
