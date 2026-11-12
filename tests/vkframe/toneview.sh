#!/bin/bash
# Lane VKFRAME1 round 11 (2026-11-12, stage 1): the tone map on the present device (vkpresent.cpp toneFrame / toneRun)
# against GL's cell_hdr quads, drawn the way cellhdr.cpp wwCellHdrEnd draws them, on the same frame, per sample.
# bash tests/vkframe/toneview.sh <out dir> [W H]. Needs MSYS2 UCRT64 g++ and python (the include expansion).
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `TONEVIEW GATE OK` or `TONEVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if tasklist | grep -qiE '^(NifSkope|Fallout4)'; then echo "TONEVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vkframe/toneview_expand.py" "$OUT" || { echo "TONEVIEW GATE FAILED (shader expansion)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/toneview.cpp" -o "$OUT/toneview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "TONEVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/toneview.exe" "$OUT" "${2:-640}" "${3:-360}" 2>&1 | tee "$OUT/toneview.txt"
tail -1 "$OUT/toneview.txt" | grep -q "TONEVIEW GATE OK"
