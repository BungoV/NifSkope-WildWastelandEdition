#!/bin/sh
# LAND1 gate A9 -- RECALIBRATE R5 to the --road-detail 1 default.
#
# The inherited red is `tests/spells/lodgen_roads.sh` R5 bar 2, and the brief
# says to recalibrate it with the SAME METHOD that set 0.3223.  The method IS
# recorded, in the docstrings of both `tests/spells/lodgen_roads_metric.py` and
# `scratchpad/roads1_20260911/road_metric.py`:
#
#     PASS = after >= 2 x floor  AND  after >= 0.8 x reference
#
# so 0.3223 is not a stored constant at all -- it is 0.8 x the SAME BAKE's own
# non-road background agreement, recomputed live every run.  The method already
# self-adapts to the detail default; what does NOT self-adapt is the 0.8, and
# ROADS1 pre-registered that with no recorded derivation.
#
# This script measures, on THIS exe and on the harness's own chunk, the four
# quantities at BOTH detail settings, so the recalibration is stated from a
# measurement rather than quoted from lane ROADS4:
#
#     d0   --road-detail 0   the configuration the 0.8 was calibrated on
#     d1   --road-detail 1   bungo's default since ROADS4
#
#     sh a9_r5.sh <exe>   ->  logs/a9_r5.txt
set -u
R="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${1:-$R/release/NifSkope.exe}"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
VAN="$DATA/Textures/Terrain/Commonwealth"
PY="C:/Users/bungo/AppData/Local/Programs/Python/Python39/python.exe"
C="Commonwealth.4.-20.20"
W="$R/scratchpad/land1_20260912/out/r5"
LOG="$R/scratchpad/land1_20260912/logs/a9_r5.txt"

if tasklist 2>/dev/null | grep -qi Fallout4.exe; then
	echo "REFUSED: Fallout4.exe is up; no exe may be launched." ; exit 3
fi
[ -x "$EXE" ] || { echo "no exe at $EXE"; exit 2; }

bake() {
	name="$1"; shift
	if [ -f "$W/$name/tex/$C.DDS" ]; then echo "   $name cached"; return 0; fi
	mkdir -p "$W/$name/mod" "$W/$name/obj" "$W/$name/tex"
	"$EXE" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region -20 20 -17 23 --dim 4 \
		--out-dir "$W/$name/obj" --data-root "$DATA" \
		--vt "$W/$name/mod" --tex-dir "$W/$name/tex" --cover "$@" \
		> "$W/$name.log" 2>&1
	rc=$?
	echo "   $name rc=$rc"
	return $rc
}

echo "LAND1 gate A9 -- R5 recalibration, measured on this exe" | tee "$LOG"
ls -l "$EXE" | sed 's/^/   /' | tee -a "$LOG"

bake d0_on  --roads    --road-detail 0 || exit 1
bake d0_off --no-roads --road-detail 0 || exit 1
bake d1_on  --roads    --road-detail 1 || exit 1
bake d1_off --no-roads --road-detail 1 || exit 1

for d in d0 d1; do
	echo "" | tee -a "$LOG"
	echo "== --road-detail ${d#d} ==" | tee -a "$LOG"
	"$PY" "$R/tests/spells/lodgen_roads_metric.py" \
		"$VAN/$C.DDS" "$W/${d}_off/tex/$C.DDS" "$W/${d}_on/tex/$C.DDS" \
		2>&1 | sed 's/^/   /' | tee -a "$LOG"
done
echo "" | tee -a "$LOG"
echo "(exit 1 above on a bar not met is the POINT of the measurement, not a failure of it)" | tee -a "$LOG"
