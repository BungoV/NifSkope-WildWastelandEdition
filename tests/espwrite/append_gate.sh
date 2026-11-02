#!/bin/bash
# lane BUGS1102D (ESPWRITE2-RENUMBER-RED): the "Add to existing" gate on a built exe, green and red in one script.
# bash tests/espwrite/append_gate.sh <NifSkope.exe> <cell.wwcell> <scratch>
#   <cell.wwcell>: an interior spec line, e.g. "<Data>/Fallout4.esm|interior|PrydwenHull01"
# green: append to a contiguous file AND to the gap fixture keeps the old refs and starts new ids past next.
# red WW_ESPWRITE_RED=renumber runs on the GAP fixture only and must fail: on contiguous ids from 0x800 renumber hands
# out the same ids, so a red there reads rc 0 whatever the code does (celledit_gates.py append --red says VACUOUS).
T=$(cd "$(dirname "$0")/../.." && pwd)
X=${1:?exe}; W=${2:?wwcell}; O=${3:?scratch dir}
PY="${PY:-$(command -v python)}"
G() { "$PY" -I "$T/tests/espwrite/celledit_gates.py" "$@" < /dev/null; }
E() { timeout 300 "$X" -no-gui espwrite place "$W" "$@" < /dev/null; }
rm -rf "$O"; mkdir -p "$O/c" "$O/g" "$O/r"
unset WW_ESPWRITE_RED WW_CELLEDIT_RED
E "$O/c/a.esp" XMarkerHeading 99 4343.2 -1024 91.578 > "$O/c/a.log" || { echo "place a failed"; exit 2; }
cp "$O/c/a.esp" "$O/c/b.esp"
E "$O/c/b.esp" --append XMarker 150 4300 -1024 30 > "$O/c/b.log"
G append "$O/c/a.esp" "$O/c/b.esp" --added 1; c=$?
G gap "$O/c/a.esp" "$O/g/a.esp" > /dev/null || exit 2
cp "$O/g/a.esp" "$O/g/b.esp"; cp "$O/g/a.esp" "$O/r/b.esp"
E "$O/g/b.esp" --append XMarker 150 4300 -1024 30 > "$O/g/b.log"
G append "$O/g/a.esp" "$O/g/b.esp" --added 1; g=$?
WW_ESPWRITE_RED=renumber E "$O/r/b.esp" --append XMarker 150 4300 -1024 30 > "$O/r/b.log"
G append "$O/g/a.esp" "$O/r/b.esp" --added 1 --red | tail -2; r=${PIPESTATUS[0]}
G append "$O/c/a.esp" "$O/c/b.esp" --added 1 --red | tail -1; v=${PIPESTATUS[0]}
echo "append contiguous rc=$c gap rc=$g (both 0); red renumber on gap rc=$r (must be 1); contiguous red rc=$v (must be 2: vacuous)"
[ $c = 0 ] && [ $g = 0 ] && [ $r = 1 ] && [ $v = 2 ]
