#!/bin/bash
# Lane VKFAR2 (2026-11-14, row G2): the sky bodies (stars, moon globe) in the sky's Vulkan pass (vkskybody.inc) against GL's
# lookdev_clouds / sky_moon programs drawn as lookdevstage.cpp draws the backdrop, on the same kind of frame, per sample.
# bash tests/vkframe/skybody.sh <out dir> [W H]. Needs MSYS2 UCRT64 g++, python (the include expansion) and the G2 SPIR-V
# headers (src/features/vkframe/vkframe_spirv.sh).
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `SKYBODY GATE OK` or `SKYBODY GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if tasklist | grep -qiE '^(NifSkope|Fallout4)'; then echo "SKYBODY GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vkframe/skybody_expand.py" "$OUT" || { echo "SKYBODY GATE FAILED (shader expansion)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/skybody.cpp" -o "$OUT/skybody.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "SKYBODY GATE FAILED (g++)"; exit 1; }
"$OUT/skybody.exe" "$OUT" "${2:-640}" "${3:-360}" 2>&1 | tee "$OUT/skybody.txt"
tail -1 "$OUT/skybody.txt" | grep -q "SKYBODY GATE OK"
