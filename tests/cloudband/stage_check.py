"""Lane CLOUDBAND1: its switches are live (stage 0) in the stage table the exe compiles in.
    python -I tests/cloudband/stage_check.py <stages.inc> [NAME ...]     exit 0 = every name listed at stage 0
Names default to WW_CLOUDVOL_ACCUM (item a) and WW_CLOUDVOL_AMBDARK (item b). The red is the base's table
(git show f46b1c76:src/wwswitchstages.inc): the names are missing there, so a set would be treated as a relaunch."""
import re
import sys

text = open(sys.argv[1], 'rb').read().decode('latin-1')
names = sys.argv[2:] or ['WW_CLOUDVOL_ACCUM', 'WW_CLOUDVOL_AMBDARK']
ok = True
for n in names:
    m = re.search(r'\{ "%s", (\d+),' % re.escape(n), text)
    good = bool(m) and m.group(1) == '0'
    ok &= good
    print('STAGE %s  %s %s' % ('PASS' if good else 'FAIL', n, ('stage ' + m.group(1)) if m else 'missing (relaunch)'))
sys.exit(0 if ok else 1)
