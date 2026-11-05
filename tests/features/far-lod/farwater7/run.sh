#!/bin/bash
# lane FARWATER7 (FARWATER7-SAT): flow encoding 2, the three-quarter-octave log speed nibble (src/features/far-lod/lodtflowcode.h,
# WW_LODL_FLOW_LOG=2), offline: g++ compiles the header alone (decode checks + the real coder over a bake's river
# dumps), judge7.py bars C1-C4 with encoding 1 as the red.
#   bash tests/features/far-lod/farwater7/run.sh [DUMPS=<dir of river_*.wwfd>] [OUT=<dir>] [PY=<python with numpy>]
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
DUMPS=${DUMPS:-E:/Projects/Claude/notes/fix1031a/farwater4/tmp/ldump}
OUT=${OUT:-${TMPDIR:-$HERE/../../../../drop}/farwater7_run}
PY=${PY:-/c/Users/bungo/AppData/Local/Programs/Python/Python39/python}
mkdir -p "$OUT"
export PATH="/c/msys64/ucrt64/bin:$PATH"
g++ -O2 -std=c++17 -o "$OUT/flowcode7_unit.exe" "$HERE/flowcode7_unit.cpp" || { echo "FARWATER7 RESULT FAIL (compile)"; exit 1; }
fail=0
"$OUT/flowcode7_unit.exe" --self || fail=1
"$PY" -I "$HERE/../farwater5/judge.py" export "$DUMPS" "$OUT" < /dev/null || fail=1
"$OUT/flowcode7_unit.exe" "$OUT/ratios.f64" "$OUT/codes.out" || fail=1
"$PY" -I "$HERE/judge7.py" "$OUT" < /dev/null || fail=1
[ $fail = 0 ] && echo "FARWATER7 RUN PASS" || echo "FARWATER7 RUN FAIL"
exit $fail
