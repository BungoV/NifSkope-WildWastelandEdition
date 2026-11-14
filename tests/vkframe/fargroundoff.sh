#!/bin/bash
# Lane VKFAR3 (2026-11-14, row G3): the far ground's GL uploads with the Vulkan view off, call for call against the rung
# (RUNG, default c7e7d1a2: the tree before the lane), and the shared inputs' route as the view turns on and off. CPU only:
# no window, no GL, no device. bash tests/vkframe/fargroundoff.sh <out dir>. Needs git, MSYS2 UCRT64 g++ and python.
# Last line: `FARGROUNDOFF GATE OK` or `FARGROUNDOFF GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
RUNG="${RUNG:-c7e7d1a2}"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
mkdir -p "$OUT"
git -C "$ROOT" show "$RUNG:src/features/far-ground/cellfarground.cpp" > "$OUT/rung.cpp" || { echo "FARGROUNDOFF GATE FAILED (rung $RUNG)"; exit 1; }
"$PY" -I "$ROOT/tests/vkframe/fargroundoff_prep.py" "$OUT/rung.cpp" "$ROOT/src/features/far-ground/cellfarground.cpp" "$OUT" \
	|| { echo "FARGROUNDOFF GATE FAILED (prep)"; exit 1; }
g++ -O1 -std=c++17 -I"$OUT" "$ROOT/tests/vkframe/fargroundoff.cpp" -o "$OUT/fargroundoff.exe" || { echo "FARGROUNDOFF GATE FAILED (g++)"; exit 1; }
"$OUT/fargroundoff.exe" 2>&1 | tee "$OUT/fargroundoff.txt"
tail -1 "$OUT/fargroundoff.txt" | grep -q "FARGROUNDOFF GATE OK"
