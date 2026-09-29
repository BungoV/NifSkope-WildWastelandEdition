#!/bin/bash
# GPU1 gate bakes on the Boston box (-8 -12 3 -1), all from one run folder holding the new exe:
#   gpuA, gpuB : Use GPU on (own settings scope, nothing planted = the default)  -> must be identical
#   cpu2       : --no-gpu                                                          -> must equal bakes/cpu1
#   off        : Use GPU planted OFF in its own settings scope                     -> must equal bakes/cpu1
# Each bake takes and releases the machine-wide NifSkope turn (prof_bake.sh). Game gate before each.
G=/e/Projects/NifskopeWWE-gpu1/scratchpad/gpu1_20260926
RUN=/c/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad/gpu1/run_join
cd $G
one() {
	local name=$1 scope=$2; shift 2
	if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP before $name"; exit 1; fi
	rm -rf bakes/$name; mkdir -p bakes/$name
	echo "== $name scope=$scope args=$* start $(date +%H:%M:%S)"
	WW_SETTINGS_SCOPE=$scope bash prof_bake.sh $RUN bakes/$name -8 -12 3 -1 "$@"
	grep -E "gpu: " bakes/$name/bake.log | cut -c1-240
	grep -E "gpu: " bakes/$name/lodl.log | head -2 | cut -c1-240
}
one gpuA gpu1on
one gpuB gpu1on
one cpu2 gpu1on --no-gpu
reg add "HKCU\\Software\\NifTools\\NifSkope 2.0 gpu1off\\Settings\\Nif" //v "Use GPU" //t REG_SZ //d false //f > /dev/null
one off gpu1off
echo "== compare $(date +%H:%M:%S)"
echo "gpuA vs gpuB: $(bash cmp_trees.sh bakes/gpuA bakes/gpuB | tail -1)"
echo "cpu1 vs cpu2: $(bash cmp_trees.sh bakes/cpu1 bakes/cpu2 | tail -1)"
echo "cpu1 vs off : $(bash cmp_trees.sh bakes/cpu1 bakes/off | tail -1)"
echo "cpu1 vs gpuA: $(bash cmp_trees.sh bakes/cpu1 bakes/gpuA | tail -3 | tr '\n' ' ')"
echo GATES DONE
