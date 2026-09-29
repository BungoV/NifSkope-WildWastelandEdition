#!/bin/bash
# FLAT2: --lodt-check on the Nuka-World ON files (expect ok) and on gates.py's Nuka-World doctored files
# (control ok; R2_block = a collapsed BC1 colour record swapped for a two-colour block, R3_pad refused, rule 16c).
cd /e/Projects/NifskopeWWE-flat2/scratchpad/flat2_20260927
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
W=E:/Projects/NifskopeWWE-flat2/scratchpad/flat2_20260927
bash $TURN acquire FLAT2 3600 || exit 1
for f in bakes/nw_on/vt/FO4CSLOD/NukaWorld/NukaWorld.VT.2.lodt bakes/nw_on/vt/FO4CSLOD/NukaWorld/NukaWorld.VT.4.lodt \
         bakes/sea_on/vt/FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt doctored_nw/control.lodt doctored_nw/R2_block.lodt doctored_nw/R3_pad.lodt; do
	[ -f "$f" ] || { echo "$f MISSING"; continue; }
	o=doctored_nw/$(echo "$f" | tr '/' '_').check.txt
	env WW_SETTINGS_SCOPE=flat2 run_new/NifSkope.exe -no-gui lodgen --lodt-check "$W/$f" > "$o" 2>&1
	echo "$f rc=$?"; grep -a -v "^gpu:" "$o" | tail -n 3
done
bash $TURN release FLAT2
