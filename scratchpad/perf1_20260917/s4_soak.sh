#!/usr/bin/env bash
# PERF1 step 4 -- the chunk fan-out, measured honestly.
#
#   bash s4_soak.sh <region> <x0> <y0> <x1> <y1> <reps>
#
# One reference tree at --chunk-threads 1 --threads 1 (the way back), then
# <reps> consecutive bakes at --chunk-threads 8 --threads 0 into the SAME
# out-dir, each compared file by file against the reference. A non-zero exit,
# a missing file or one differing byte stops the soak and says which run.
set -u
R="$(cd "$(dirname "$0")/../.." && pwd)"
D="$R/scratchpad/perf1_20260917"
EXE="$R/release/NifSkope.exe"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
VANILLA="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"

rn="$1"; x0="$2"; y0="$3"; x1="$4"; y1="$5"; reps="$6"
log="$D/s4_$rn.txt"
: > "$log"

bake () {          # bake <dir> <chunkthreads> <threads>
	local dir="$1" ct="$2" th="$3"
	rm -rf "$dir"; mkdir -p "$dir/obj" "$dir/tex" "$dir/nat"
	local da; da="$(cd "$dir" && { pwd -W 2>/dev/null || pwd; })"
	local t0 t1
	t0=$(date +%s%N)
	"$EXE" -no-gui lodgen "$VANILLA" --worldspace 3C \
		--terrain-region "$x0" "$y0" "$x1" "$y1" --dim 4 \
		--out-dir "$da/obj" --tex-dir "$da/tex" --data-root "$DATA" \
		--native "$da/nat" --cover --roads --road-detail 1 \
		--threads "$th" --chunk-threads "$ct" > "$dir/bake.log" 2>&1
	local rc=$?
	t1=$(date +%s%N)
	WALL=$(( (t1-t0)/1000000 ))
	return $rc
}

if tasklist 2>/dev/null | grep -qi "Fallout4.exe"; then
	echo "REFUSED: Fallout4.exe is up" | tee -a "$log"; exit 3
fi

ref="$D/work/s4/${rn}_ref"
bake "$ref" 1 1 || { echo "REFERENCE FAILED rc=$?" | tee -a "$log"; exit 2; }
echo "reference (ct1 t1) wall_ms=$WALL" | tee -a "$log"

run="$D/work/s4/${rn}_run"
faults=0
for i in $(seq 1 "$reps"); do
	if tasklist 2>/dev/null | grep -qi "Fallout4.exe"; then
		echo "STOPPED at run $i: Fallout4.exe came up" | tee -a "$log"; exit 3
	fi
	bake "$run" 8 0
	rc=$?
	v=$(cd "$D" && python treecmp.py "work/s4/${rn}_ref" "work/s4/${rn}_run" \
		--record-mask --subst "work/s4/${rn}_ref" "ARM" --subst "work/s4/${rn}_run" "ARM" \
		| tail -2 | tr '\n' ' ')
	echo "run $i rc=$rc wall_ms=$WALL $v" | tee -a "$log"
	case "$v" in *"VERDICT IDENTICAL"*) ;; *) faults=$((faults+1));; esac
	[ "$rc" -eq 0 ] || faults=$((faults+1))
done
echo "SOAK $rn: $reps run(s) at --chunk-threads 8, $faults fault(s)" | tee -a "$log"
