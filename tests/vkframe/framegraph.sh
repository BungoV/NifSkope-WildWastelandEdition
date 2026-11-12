#!/bin/bash
# Lane VKFRAME1 item 2: the frame graph's pure rules. bash tests/vkframe/framegraph.sh <out dir>
# Green must print FRAMEGRAPH OK; the red (-DVKFRAMEGRAPH_RED) must fail. Last line: `VKFRAME GRAPH GATE OK` / `FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
mkdir -p "$OUT"
g++ -std=c++17 -Wall -I "$ROOT/src" "$ROOT/tests/vkframe/framegraph_test.cpp" -o "$OUT/framegraph.exe" || { echo "VKFRAME GRAPH GATE FAILED (g++)"; exit 1; }
g++ -std=c++17 -Wall -DVKFRAMEGRAPH_RED -I "$ROOT/src" "$ROOT/tests/vkframe/framegraph_test.cpp" -o "$OUT/framegraph_red.exe" || { echo "VKFRAME GRAPH GATE FAILED (g++ red)"; exit 1; }
"$OUT/framegraph.exe"; g=$?
"$OUT/framegraph_red.exe" > "$OUT/red.txt"; r=$?
echo "red: $(grep -c '^FAIL' "$OUT/red.txt") checks fail, last line: $(tail -1 "$OUT/red.txt")"
if [ $g -eq 0 ] && [ $r -ne 0 ]; then echo "VKFRAME GRAPH GATE OK"; else echo "VKFRAME GRAPH GATE FAILED"; exit 1; fi
