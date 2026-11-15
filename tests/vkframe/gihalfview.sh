#!/bin/bash
# Lane VKFAR3 (2026-11-15, plan row A1 step b): the screen GI's DDGI read on the present device (gihalf_vk.hlsl through
# lightRun) against GL's cell_gihalf program on one shared cascades' atlas (tests/vkframe/gihalfview.cpp: 2 greens with
# their seen checks, the uniform table, the atlas lookup, 3 reds FAIL). bash tests/vkframe/gihalfview.sh <out dir>.
# Not while NifSkope or the game runs (a window of ours). Last line: `GIHALFVIEW GATE OK` or `GIHALFVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
if powershell -NoProfile -Command "if (Get-Process -Name NifSkope,Fallout4 -ErrorAction SilentlyContinue) { exit 0 } else { exit 1 }"; then
	echo "GIHALFVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1
fi
mkdir -p "$OUT"
python -I "$ROOT/tests/vkframe/gihalf_src.py" "$OUT/cell_gihalf.glsl" || { echo "GIHALFVIEW GATE FAILED (source)"; exit 1; }
export PATH="/c/msys64/ucrt64/bin:$PATH"
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/gihalfview.cpp" -o "$OUT/gihalfview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "GIHALFVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/gihalfview.exe" "$OUT/cell_gihalf.glsl" > "$OUT/gihalfview.txt" 2>&1
cat "$OUT/gihalfview.txt"
N=$(grep -c '^GIHALFVIEW .* OK (' "$OUT/gihalfview.txt")
[ "$N" = 15 ] || { echo "GIHALFVIEW GATE FAILED ($N of 15 checks OK)"; exit 1; }
tail -1 "$OUT/gihalfview.txt" | grep -q "GIHALFVIEW GATE OK" || { echo "GIHALFVIEW GATE FAILED"; exit 1; }
echo "GIHALFVIEW GATE OK"
