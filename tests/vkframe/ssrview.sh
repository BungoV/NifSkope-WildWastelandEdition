#!/bin/bash
# Lane VKLIGHT2 (2026-11-13, row L4): the reflections on the present device (vkpresent.cpp ssrRun, stages 5 and 1-4) against
# GL's own cell_ssr / cell_ssrwater quads, drawn the way cellssr.cpp draws them, on the same inputs, into the same kind of targets.
# bash tests/vkframe/ssrview.sh <out dir> [W H]. Needs MSYS2 UCRT64 g++ and python (the include expansion).
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `SSRVIEW GATE OK` or `SSRVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if tasklist | grep -qiE '^(NifSkope|Fallout4)'; then echo "SSRVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vkframe/ssrview_expand.py" "$OUT" || { echo "SSRVIEW GATE FAILED (shader expansion)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/ssrview.cpp" -o "$OUT/ssrview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "SSRVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/ssrview.exe" "$OUT" "${2:-641}" "${3:-361}" 2>&1 | tee "$OUT/ssrview.txt"
tail -1 "$OUT/ssrview.txt" | grep -q "SSRVIEW GATE OK"
