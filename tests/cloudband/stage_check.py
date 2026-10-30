"""Lane CLOUDBAND1: WW_CLOUDVOL_ACCUM is a live switch (stage 0) in the stage table the exe compiles in.
    python -I tests/cloudband/stage_check.py <stages.inc>     exit 0 = listed at stage 0
The red is the base's table (git show f46b1c76:src/wwswitchstages.inc): the name is missing there, so a set would be
treated as a relaunch."""
import re
import sys

text = open(sys.argv[1], 'rb').read().decode('latin-1')
m = re.search(r'\{ "WW_CLOUDVOL_ACCUM", (\d+),', text)
ok = bool(m) and m.group(1) == '0'
print('STAGE %s  WW_CLOUDVOL_ACCUM %s' % ('PASS' if ok else 'FAIL', ('stage ' + m.group(1)) if m else 'missing (relaunch)'))
sys.exit(0 if ok else 1)
