#!/bin/bash
# lane PERFFIX3 D (2026-11-01): the GPU precipitation field vs its CPU twin (precipmath.h), on the real driver.
# usage: bash tests/features/weather/precipgpu/run.sh <scratch dir>     green must PASS, both reds must FAIL; exit 0 only then.
T=$(cd "$(dirname "$0")/../../../.." && pwd)
O=${1:?scratch dir}
mkdir -p "$O"
OW=$(cygpath -m "$O")
TW=$(cygpath -m "$T")
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc "cd '$TW' && g++ -O2 -std=c++17 -I src tests/features/weather/precipgpu/precipgpu_dump.cpp -o '$OW/precipgpu_dump.exe'" < /dev/null || { echo "BUILD FAIL"; exit 2; }
PY=${PY:-python}
"$PY" -I "$TW/tests/features/weather/precipgpu/precipgpu_check.py" "$OW/precipgpu_dump.exe" < /dev/null; g=$?
"$PY" -I "$TW/tests/features/weather/precipgpu/precipgpu_check.py" "$OW/precipgpu_dump.exe" --red field < /dev/null; r1=$?
"$PY" -I "$TW/tests/features/weather/precipgpu/precipgpu_check.py" "$OW/precipgpu_dump.exe" --red cull < /dev/null; r2=$?
echo "green rc=$g red field rc=$r1 red cull rc=$r2"
[ $g = 0 ] && [ $r1 != 0 ] && [ $r2 != 0 ] && { echo "PRECIPGPU UNIT PASS"; exit 0; }
echo "PRECIPGPU UNIT FAIL"; exit 1
