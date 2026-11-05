#!/bin/bash
# lane FARLOOK1101 (FARWATER5 L3): the log speed nibble (src/features/far-lod/lodtflowcode.h, flow encoding 1, WW_LODL_FLOW_LOG=1),
# offline: g++ compiles the header alone (decode checks + the real coder over a bake's river dumps), judge.py bars B1-B4.
#   bash tests/features/far-lod/farwater5/run.sh [DUMPS=<dir of river_*.wwfd>] [OUT=<dir>] [PY=<python with numpy>]
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
DUMPS=${DUMPS:-E:/Projects/Claude/notes/fix1031a/farwater4/tmp/ldump}
OUT=${OUT:-${TMPDIR:-$HERE/../../../../drop}/farwater5_run}
PY=${PY:-/c/Users/bungo/AppData/Local/Programs/Python/Python39/python}
mkdir -p "$OUT"
export PATH="/c/msys64/ucrt64/bin:$PATH"
g++ -O2 -std=c++17 -o "$OUT/flowcode_unit.exe" "$HERE/flowcode_unit.cpp" || { echo "FARWATER5 L3 RESULT FAIL (compile)"; exit 1; }
fail=0
"$OUT/flowcode_unit.exe" --self || fail=1
"$PY" -I "$HERE/judge.py" export "$DUMPS" "$OUT" < /dev/null || fail=1
"$OUT/flowcode_unit.exe" "$OUT/ratios.f64" "$OUT/codes.out" || fail=1
"$PY" -I "$HERE/judge.py" judge "$DUMPS" "$OUT" < /dev/null || fail=1
[ $fail = 0 ] && echo "FARWATER5 RUN PASS" || echo "FARWATER5 RUN FAIL"
exit $fail
