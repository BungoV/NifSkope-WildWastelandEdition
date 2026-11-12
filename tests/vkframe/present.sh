#!/bin/bash
# Lane VKFRAME1 (2026-11-12, PRESENT FIRST): GL's SwapBuffers against the Vulkan swapchain present (vkpresent.cpp) in a
# window of its own, behind the others. bash tests/vkframe/present.sh <out dir> [W H frames]. Needs MSYS2 UCRT64 g++.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `PRESENT GATE OK` or `PRESENT GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export PATH="/c/msys64/ucrt64/bin:$PATH"
if tasklist | grep -qiE '^(NifSkope|Fallout4)'; then echo "PRESENT GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
g++ -O2 -std=c++17 -I"$ROOT/src" "$ROOT/tests/vkframe/present.cpp" -o "$OUT/present.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "PRESENT GATE FAILED (g++)"; exit 1; }
"$OUT/present.exe" "${2:-1280}" "${3:-720}" "${4:-600}" 2>&1 | tee "$OUT/present.txt"
tail -1 "$OUT/present.txt" | grep -q "PRESENT GATE OK"
