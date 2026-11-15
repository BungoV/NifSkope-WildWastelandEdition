#!/bin/bash
# Lane VKFAR3 (2026-11-15, plan row A1): the DDGI probe atlas imported on the present device (vkddgi.inc) and read back
# through its texel view against what the exporting device wrote (tests/vkframe/ddgiview.cpp: greens EXACT, 2 reds FAIL,
# 2 refusals). bash tests/vkframe/ddgiview.sh <out dir>. Not while NifSkope or the game runs (a window of ours).
# Last line: `DDGIVIEW GATE OK` or `DDGIVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
if powershell -NoProfile -Command "if (Get-Process -Name NifSkope,Fallout4 -ErrorAction SilentlyContinue) { exit 0 } else { exit 1 }"; then
	echo "DDGIVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1
fi
mkdir -p "$OUT"
export PATH="/c/msys64/ucrt64/bin:$PATH"
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/ddgiview.cpp" -o "$OUT/ddgiview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "DDGIVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/ddgiview.exe" > "$OUT/ddgiview.txt" 2>&1
cat "$OUT/ddgiview.txt"
N=$(grep -c '^DDGIVIEW .* OK (' "$OUT/ddgiview.txt")
[ "$N" = 17 ] || { echo "DDGIVIEW GATE FAILED ($N of 17 checks OK)"; exit 1; }
tail -1 "$OUT/ddgiview.txt" | grep -q "DDGIVIEW GATE OK" || { echo "DDGIVIEW GATE FAILED"; exit 1; }
echo "DDGIVIEW GATE OK"
