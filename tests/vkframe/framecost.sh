#!/bin/bash
# Lane VKFRAME1 (2026-11-12, build 79 fix): what GL pays to fill, resolve and read the shared MSAA frame against its own.
# bash tests/vkframe/framecost.sh <out dir> [W H]   (default 947 991, build 79's window). Needs MSYS2 UCRT64 g++.
# Green: makeFrame's import (asis) and both semaphore hand-offs (general, readonly) within 5 % of GL's own frame and in
# the device-local heap (VK_EXT_memory_budget, makeFrame's check). Red: a memory type outside the device-local heap must FAIL.
# Last line: `VKFRAME FRAMECOST GATE OK` or `... FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
export FC_W="${2:-947}" FC_H="${3:-991}"
mkdir -p "$OUT"
g++ -O1 -std=c++17 "$ROOT/tests/vkframe/framecost.cpp" -o "$OUT/framecost.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "VKFRAME FRAMECOST GATE FAILED (g++)"; exit 1; }
rc=0
"$OUT/framecost.exe" asis general readonly > "$OUT/green.txt" 2>&1 || rc=1
grep -E "^FRAMECOST" "$OUT/green.txt"
[ $rc -eq 0 ] || echo "GREEN FAILED"
"$OUT/framecost.exe" sysmem > "$OUT/red.txt" 2>&1
echo "red (sysmem): $(grep -E '^FRAMECOST (sysmem|GATE)' "$OUT/red.txt" | tr '\n' ' ')"
grep -q "FRAMECOST GATE FAILED" "$OUT/red.txt" || { echo "RED DID NOT FAIL"; rc=1; }
if [ $rc -eq 0 ]; then echo "VKFRAME FRAMECOST GATE OK"; else echo "VKFRAME FRAMECOST GATE FAILED"; exit 1; fi
