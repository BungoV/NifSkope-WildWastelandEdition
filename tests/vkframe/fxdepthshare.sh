#!/bin/bash
# Lane VKOBJ5 (11-15): the soft effects' depth retries its shared target after a refusal. bash tests/vkframe/fxdepthshare.sh <out dir>
# Green must print FXDEPTHSHARE OK; the red (-DFXDEPTHSHARE_RED, the old no-retry rule) must fail.
# Last line: `FXDEPTHSHARE GATE OK` / `FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
mkdir -p "$OUT"
g++ -std=c++17 -Wall -Wextra -I "$ROOT/src" "$ROOT/tests/vkframe/fxdepthshare_test.cpp" -o "$OUT/fxdepthshare.exe" || { echo "FXDEPTHSHARE GATE FAILED (g++)"; exit 1; }
g++ -std=c++17 -Wall -Wextra -DFXDEPTHSHARE_RED -I "$ROOT/src" "$ROOT/tests/vkframe/fxdepthshare_test.cpp" -o "$OUT/fxdepthshare_red.exe" || { echo "FXDEPTHSHARE GATE FAILED (g++ red)"; exit 1; }
"$OUT/fxdepthshare.exe"; g=$?
"$OUT/fxdepthshare_red.exe" > "$OUT/red.txt"; r=$?
echo "red: $(grep -c '^FAIL' "$OUT/red.txt") checks fail, last line: $(tail -1 "$OUT/red.txt")"
if [ $g -eq 0 ] && [ $r -ne 0 ]; then echo "FXDEPTHSHARE GATE OK"; else echo "FXDEPTHSHARE GATE FAILED"; exit 1; fi
