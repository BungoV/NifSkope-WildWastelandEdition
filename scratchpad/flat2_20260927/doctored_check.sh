#!/bin/bash
# FLAT2: the in-tree validator (--lodt-check) on the gates.py --make-doctored files, under the machine turn.
#   expected: control.lodt ok; R2_block.lodt and R3_pad.lodt refused (rule 16c)
cd /e/Projects/NifskopeWWE-flat2/scratchpad/flat2_20260927
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
bash $TURN acquire flat2 3600 || exit 1
for f in control R2_block R3_pad; do
	env WW_SETTINGS_SCOPE=flat2 run_new/NifSkope.exe -no-gui lodgen --lodt-check "doctored/$f.lodt" > "doctored/$f.check.txt" 2>&1
	echo "$f rc=$?"
	tail -n 4 "doctored/$f.check.txt"
done
bash $TURN release flat2
