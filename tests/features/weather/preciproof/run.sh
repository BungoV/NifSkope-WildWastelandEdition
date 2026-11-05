#!/bin/bash
# lane INTSKY1: tests/features/weather/preciproof (pure header). Green must pass, the red noroof must FAIL "roofed".
# usage: bash tests/features/weather/preciproof/run.sh <out dir>
OUT=${1:?out dir}
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../.." && pwd)
mkdir -p "$OUT"
export PATH=/c/msys64/ucrt64/bin:$PATH
g++ -std=c++17 -O1 -Wall -I"$ROOT/src" "$HERE/preciproof_test.cpp" -o "$OUT/preciproof_test.exe" || { echo "BUILD FAILED"; exit 2; }
"$OUT/preciproof_test.exe" > "$OUT/green.txt"; rc=$?
tail -1 "$OUT/green.txt"
[ $rc = 0 ] || { grep FAIL "$OUT/green.txt"; exit 1; }
PRECIPROOF_RED=noroof "$OUT/preciproof_test.exe" > "$OUT/red.txt"; rrc=$?
if [ $rrc != 0 ] && grep -q "^FAIL roofed " "$OUT/red.txt"; then echo "red noroof: FAIL as it must"; else echo "red noroof DID NOT BITE"; exit 1; fi
# lane BUGS1102 (PRECIPROOF-GPU): the old pick (the GPU field, which has no roof map, in a roofed interior) must FAIL
PRECIPROOF_RED=gpuroof "$OUT/preciproof_test.exe" > "$OUT/red_gpuroof.txt"; grc=$?
if [ $grc != 0 ] && grep -q "^FAIL gpuroofed_grid " "$OUT/red_gpuroof.txt" && grep -q "^FAIL gpuroofed_- " "$OUT/red_gpuroof.txt" \
	&& ! grep -q "^FAIL roofed " "$OUT/red_gpuroof.txt"; then echo "red gpuroof: FAIL as it must"; else echo "red gpuroof DID NOT BITE"; exit 1; fi
# lane BUGS1102B (PRECIPROOF-GPU2): BUGS1102's pick (the CPU field in a roofed interior) must FAIL pick_roofed but still
# drop every particle under the roof (the grid)
PRECIPROOF_RED=cpuroof "$OUT/preciproof_test.exe" > "$OUT/red_cpuroof.txt"; crc=$?
if [ $crc != 0 ] && grep -q "^FAIL pick_roofed_- " "$OUT/red_cpuroof.txt" && ! grep -q "^FAIL gpuroofed_grid " "$OUT/red_cpuroof.txt"; then
	echo "red cpuroof: FAIL as it must"; else echo "red cpuroof DID NOT BITE"; exit 1; fi
# the pass picks its field through that rule (on 1d96238a this line was absent: the GPU field ignored the roof)
grep -q "wwPrecipGpuField( cpuFieldFromEnv(), .*roof != nullptr" "$ROOT/src/features/weather/cellprecip.cpp" || { echo "cellprecip.cpp does not pick its field through wwPrecipGpuField"; exit 1; }
grep -q "wwPrecipGpuRoofTest(" "$ROOT/src/features/weather/cellprecip.cpp" || { echo "cellprecip.cpp does not gate the GPU roof test"; exit 1; }
grep -q "return !pcRoofOn || pcRoofOpen( rel );" "$ROOT/res/shaders/features/weather/cell_precip_field.glsl" || { echo "cell_precip_field.glsl does not test the roof map"; exit 1; }
echo "preciproof unit: PASS (green + 3 reds)"
