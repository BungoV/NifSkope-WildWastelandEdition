#!/bin/bash
# Lane VKAIR7 (2026-11-16): the air's sharedTarget keys survive a QHash move (airkeys.cpp has the bar and the red).
# bash tests/vkframe/airkeys.sh <out dir>. Needs MSYS2 UCRT64 g++ and Qt6Core. Not while NifSkope or the game runs.
# Last line: `AIRKEYS GATE OK` or `AIRKEYS GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "AIRKEYS GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
# the wire: the app keys every air target through VaVkKeys, never by an address inside the QHash value
F="$ROOT/src/features/air/cellvolair.cpp"
OLD=$(grep -c 'uintptr_t>( &g\.' "$F")
NEW=$(grep -c 'vkKeys\.at(' "$F")
echo "AIRKEYS wire: cellvolair.cpp member-address keys $OLD (bar 0), vkKeys.at uses $NEW (bar >= 6)"
WIRE=1
[ "$OLD" -eq 0 ] && [ "$NEW" -ge 6 ] || WIRE=0
QT=/c/msys64/ucrt64/include/qt6
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" -I"$QT" -I"$QT/QtCore" "$ROOT/tests/vkframe/airkeys.cpp" -o "$OUT/airkeys.exe" \
	-lQt6Core -lopengl32 -lgdi32 -luser32 || { echo "AIRKEYS GATE FAILED (g++)"; exit 1; }
"$OUT/airkeys.exe" "$ROOT" 2>&1 | tee "$OUT/airkeys.txt"
if [ "$WIRE" -ne 1 ]; then echo "AIRKEYS GATE FAILED (wire)"; exit 1; fi
tail -1 "$OUT/airkeys.txt" | grep -q "AIRKEYS GATE OK"
