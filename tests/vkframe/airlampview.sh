#!/bin/bash
# Lane VKAIR15 (2026-11-16): the air's stage 4, the lamp volume's walk (cellvolair.cpp vaPassInner, 64 layers, MRT 2) on the
# present device against GL's own cell_volair program; the bar and the reds are in airlampview.cpp's header.
# bash tests/vkframe/airlampview.sh <out dir>. Needs MSYS2 UCRT64 g++ and python.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `AIRLAMPVIEW GATE OK` or `AIRLAMPVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "AIRLAMPVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
# lane VKAIR17: the app makes its atlas through the helper the atlas arm calls, and asks for the share while the air is on Vulkan
n1=$(grep -c 'airAtlasEnsure( fn, atlasKey()' "$ROOT/src/features/air/airlampatlas.cpp")
n2=$(grep -c 'wwAirLampAtlasShare( s1Share )' "$ROOT/src/features/air/cellvolair.cpp")
echo "AIRLAMPVIEW app: airAtlasEnsure calls $n1, wwAirLampAtlasShare( s1Share ) calls $n2 (want 1 and 1)"
[ "$n1" = 1 ] && [ "$n2" = 1 ] || { echo "AIRLAMPVIEW GATE FAILED (the app does not make the atlas through the helper)"; exit 1; }
"$PY" -I "$ROOT/tests/vkframe/airinjectview_prep.py" "$OUT" || { echo "AIRLAMPVIEW GATE FAILED (prep)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/airlampview.cpp" -o "$OUT/airlampview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "AIRLAMPVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/airlampview.exe" "$OUT" 2>&1 | tee "$OUT/airlampview.txt"
tail -1 "$OUT/airlampview.txt" | grep -q "AIRLAMPVIEW GATE OK"
