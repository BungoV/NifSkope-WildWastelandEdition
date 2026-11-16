#!/bin/bash
# Lane VKAIR11 (2026-11-16): cell_volair's stage-1 inputs shared by their owner read by GL exactly as GL's own (airshare.cpp
# has the bar and the reds). bash tests/vkframe/airshare.sh <out dir>. Needs MSYS2 UCRT64 g++ and Qt6Core.
# Not while NifSkope or the game runs. Last line: `AIRSHARE GATE OK` or `AIRSHARE GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "AIRSHARE GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
# the wire: every stage-1 site of cellvolair.cpp goes through the helper (hist, lvHist, lvNow, stbn, noise, terr)
F="$ROOT/src/features/air/cellvolair.cpp"
ENS=$(grep -c 'vaShareEnsure(' "$F")
REL=$(grep -c 'vaShareRelease(' "$F")
echo "AIRSHARE wire: cellvolair.cpp vaShareEnsure calls $ENS (bar >= 6), vaShareRelease calls $REL (bar >= 6)"
WIRE=1
[ "$ENS" -ge 6 ] && [ "$REL" -ge 6 ] || WIRE=0
QT=/c/msys64/ucrt64/include/qt6
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" -I"$QT" -I"$QT/QtCore" "$ROOT/tests/vkframe/airshare.cpp" -o "$OUT/airshare.exe" \
	-lQt6Core -lopengl32 -lgdi32 -luser32 || { echo "AIRSHARE GATE FAILED (g++)"; exit 1; }
"$OUT/airshare.exe" "$ROOT" 2>&1 | tee "$OUT/airshare.txt"
if [ "$WIRE" -ne 1 ]; then echo "AIRSHARE GATE FAILED (wire)"; exit 1; fi
tail -1 "$OUT/airshare.txt" | grep -q "AIRSHARE GATE OK"
