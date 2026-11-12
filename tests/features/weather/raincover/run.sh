#!/bin/bash
# lane WETWEATHER2: bash tests/features/weather/raincover/run.sh <outdir>. CPU: green must exit 0, the red (-DRAINCOVER_RED, the base:
# no cover test) must fail. GPU (raincover_gpu.py, the real driver): green PASS, reds cover and off FAIL. Pure headers,
# UCRT64 g++ (its runtime first on PATH in the same call).
set -u
OUT=${1:?outdir}
ROOT=$(cd "$(dirname "$0")/../../../.." && pwd)
mkdir -p "$OUT"
PY=${PY:-$(command -v python)}	# the Windows python with numpy, picked before ucrt64 (its own python has none) leads PATH
export PATH=/c/msys64/ucrt64/bin:$PATH
g++ -std=c++17 -O1 -I"$ROOT/src" "$ROOT/tests/features/weather/raincover/raincover.cpp" -o "$OUT/green.exe" || exit 2
g++ -std=c++17 -O1 -DRAINCOVER_RED -I"$ROOT/src" "$ROOT/tests/features/weather/raincover/raincover.cpp" -o "$OUT/red.exe" || exit 2
timeout 30 "$OUT/green.exe" > "$OUT/green.txt"; G=$?
timeout 30 "$OUT/red.exe" > "$OUT/red.txt"; R=$?
grep FAIL "$OUT/green.txt"; tail -1 "$OUT/green.txt"; tail -1 "$OUT/red.txt"
echo "cpu green rc=$G red rc=$R"
EXE=$(cygpath -m "$OUT/green.exe")
timeout 120 "$PY" -I "$(cygpath -m "$ROOT")/tests/features/weather/raincover/raincover_gpu.py" "$EXE" < /dev/null | tail -3; g=${PIPESTATUS[0]}
timeout 120 "$PY" -I "$(cygpath -m "$ROOT")/tests/features/weather/raincover/raincover_gpu.py" "$EXE" --red cover < /dev/null | tail -1; r1=${PIPESTATUS[0]}
timeout 120 "$PY" -I "$(cygpath -m "$ROOT")/tests/features/weather/raincover/raincover_gpu.py" "$EXE" --red off < /dev/null | tail -1; r2=${PIPESTATUS[0]}
echo "gpu green rc=$g red cover rc=$r1 red off rc=$r2"
[ $G -eq 0 ] && [ $R -ne 0 ] && [ $g -eq 0 ] && [ $r1 -ne 0 ] && [ $r2 -ne 0 ] && echo "RAINCOVER UNIT PASS" && exit 0
echo "RAINCOVER UNIT FAIL"; exit 1
