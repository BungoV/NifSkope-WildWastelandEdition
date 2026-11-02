#!/bin/bash
# lane BUGS1102B (PRECIPROOF-GPU2): the GPU field's roof test vs WwRoofMap::open (preciproof.h), on the real driver.
# usage: bash tests/precipgpuroof/run.sh <scratch dir>     green must PASS, both reds must FAIL; exit 0 only then.
T=$(cd "$(dirname "$0")/../.." && pwd)
O=${1:?scratch dir}
mkdir -p "$O"
OW=$(cygpath -m "$O")
TW=$(cygpath -m "$T")
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc "cd '$TW' && g++ -O2 -std=c++17 -I src tests/precipgpuroof/precipgpuroof_dump.cpp -o '$OW/precipgpuroof_dump.exe'" < /dev/null || { echo "BUILD FAIL"; exit 2; }
PY=${PY:-python}
"$PY" -I "$TW/tests/precipgpuroof/precipgpuroof_check.py" "$OW/precipgpuroof_dump.exe" < /dev/null; g=$?
"$PY" -I "$TW/tests/precipgpuroof/precipgpuroof_check.py" "$OW/precipgpuroof_dump.exe" --red off < /dev/null; r1=$?
"$PY" -I "$TW/tests/precipgpuroof/precipgpuroof_check.py" "$OW/precipgpuroof_dump.exe" --red axes < /dev/null; r2=$?
echo "green rc=$g red off rc=$r1 red axes rc=$r2"
[ $g = 0 ] && [ $r1 != 0 ] && [ $r2 != 0 ] && { echo "PRECIPGPUROOF UNIT PASS"; exit 0; }
echo "PRECIPGPUROOF UNIT FAIL"; exit 1
