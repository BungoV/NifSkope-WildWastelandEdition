"""Lane CLOUDBAND2: Cloud Accumulation (WW_CLOUDVOL_ACCUM) and the column ambient (WW_CLOUDVOL_AMBDARK) are ON BY DEFAULT.
    python -I tests/cloudband/default_check.py <cloudvol.cpp> <cloudband_dump.exe>     exit 0 = every rule holds
Rules (each one a PASS/FAIL line):
  R1 cloudvol.cpp cs() reads both switches through cloudvolmath.h V::defaultOnSwitch (the one rule the dump runs), and
     no longer through the old "== 1 || == on" test (unset = Off).
  R2 the dump (the same header) over values: unset, "", "1", "on", "yes", "2" -> on; "0", "off", "false", "no" -> off,
     each lower-cased and trimmed as cs() hands it (" OFF " -> off).
Red: today's main cloudvol.cpp (git show e8fad765:src/gl/cloudvol.cpp): R1 FAILS (the old test, unset = Off)."""
import os
import re
import subprocess
import sys

src = open(sys.argv[1], 'rb').read().decode('latin-1')
dump = sys.argv[2]
ok = True


def line(name, good, msg):
    global ok
    ok &= good
    print('%s %s  %s' % (name, 'PASS' if good else 'FAIL', msg), flush=True)


for env, member in (('WW_CLOUDVOL_ACCUM', 's.accum'), ('WW_CLOUDVOL_AMBDARK', 's.ambDark')):
    m = re.search(r'wwEnvStr\( "%s" \)[^;]*;\s*%s = ([^;]*);' % (env, re.escape(member)), src)
    expr = m.group(1) if m else ''
    good = 'V::defaultOnSwitch(' in expr and 'QLatin1StringView( "1" )' not in expr
    line('R1', good, '%s = %s' % (member, expr.strip() or '(read not found)'))

base = {k: v for k, v in os.environ.items() if k not in ('WW_CLOUDVOL_ACCUM', 'WW_CLOUDVOL_AMBDARK')}
base['PATH'] = 'C:/msys64/ucrt64/bin;' + base.get('PATH', '')
cases = [(None, 1), ('', 1), ('1', 1), ('on', 1), ('yes', 1), ('2', 1), ('0', 0), ('off', 0), (' OFF ', 0), ('false', 0),
         ('no', 0)]
for v, want in cases:
    env = dict(base)
    if v is not None:
        env['WW_CLOUDVOL_ACCUM'] = v
        env['WW_CLOUDVOL_AMBDARK'] = v
    out = subprocess.run([dump], env=env, capture_output=True, text=True, check=True).stdout
    dl = [l.split() for l in out.split('\n') if l.startswith('default ')][0]
    got = (int(dl[2]), int(dl[4]))
    line('R2', got == (want, want), 'value %r -> accum %d ambdark %d (want %d)' % (v, got[0], got[1], want))

print('CLOUDBAND-DEFAULT %s' % ('PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
