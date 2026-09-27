#!/bin/bash
# GPU1: fair speed A/B of the GPU BC7 path on the Boston box, ABBA order (cpu, gpu, gpu, cpu) back to back,
# so drift on the machine lands on both sides. Same exe, same settings scope (nothing planted = GPU on).
# Prints each run's chunk stage wall time and the card-array interval; bakes are deleted after timing.
G=/e/Projects/NifskopeWWE-gpu1/scratchpad/gpu1_20260926
RUN=/c/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad/gpu1/run_join
cd $G
one() {
	local name=$1; shift
	if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP before $name"; exit 1; fi
	rm -rf bakes/$name; mkdir -p bakes/$name
	echo "== $name args=$* start $(date +%H:%M:%S)"
	WW_SETTINGS_SCOPE=gpu1on bash prof_bake.sh $RUN bakes/$name -8 -12 3 -1 "$@" | grep -E "rc=|chunk stage"
	grep -E "gpu: " bakes/$name/bake.log | tail -1 | cut -c1-200
	cp bakes/$name/bake.log ab_$name.bake.log.tmp
}
one abcpu1 --no-gpu
one abgpu1
one abgpu2
one abcpu2 --no-gpu
echo "gpu1 vs gpu2: $(bash cmp_trees.sh bakes/abgpu1 bakes/abgpu2 | tail -1)"
echo "cpu1 vs cpu2: $(bash cmp_trees.sh bakes/abcpu1 bakes/abcpu2 | tail -1)"
echo "AB DONE $(date +%H:%M:%S)"
