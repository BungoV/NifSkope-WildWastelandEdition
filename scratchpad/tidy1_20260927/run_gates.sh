#!/bin/bash
# TIDY1 resume: off bake -> byte gate, on bake -> drop/dedupe gates. Each bake queues behind the turn lock.
#   bash run_gates.sh            (run from anywhere; writes gates.log next to this script)
# The base snapshot base_ground1_on.json was taken 06:3x 2026-09-27 from GROUND1's bakes/on (exe = ground1
# release 04:50); it stays valid even if GROUND1 deletes that bake.
S=/e/Projects/NifskopeWWE-tidy1/scratchpad/tidy1_20260927
EXE=$S/run/NifSkope.exe
cd $S || exit 2
{
echo "== off bake $(date +%H:%M:%S)"
bash bake.sh $EXE $S/bakes/off WW_LODGEN_KEEP_BLACK_EMISSIVE=1 WW_LODGEN_NO_LAYER_DEDUPE=1 || { echo "off bake failed"; exit 1; }
python gates.py snapshot $S/bakes/off snap_off.json
python gates.py compare base_ground1_on.json snap_off.json off; offrc=$?
echo "== on bake $(date +%H:%M:%S)"
bash bake.sh $EXE $S/bakes/on || { echo "on bake failed"; exit 1; }
python gates.py snapshot $S/bakes/on snap_on.json
python gates.py compare base_ground1_on.json snap_on.json on; onrc=$?
grep -a -c "identical texels" $S/bakes/on/bake.log
echo "GATES off=$offrc on=$onrc $(date +%H:%M:%S)"
} 2>&1 | tee $S/gates.log
