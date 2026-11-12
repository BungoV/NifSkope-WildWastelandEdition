#!/bin/bash
# Lane VKSHARE1 (2026-11-12): the Modern TAA at High on the present device (vkpresent.cpp taaRun: vectors, resolve, RCAS
# copy-back into the view) against GL's own game_taa_mv / taa2 / taa2_rcas passes drawn the way gametaa.cpp draws them,
# frame by frame on the same shared inputs. Also item 1's shared R8 3D and RGBA16F array round trips.
# bash tests/vkframe/taaview.sh <out dir> [W H]. Needs MSYS2 UCRT64 g++ and python (the include expansion).
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `TAAVIEW GATE OK` or `TAAVIEW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if tasklist | grep -qiE '^(NifSkope|Fallout4)'; then echo "TAAVIEW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vkframe/taaview_expand.py" "$OUT" || { echo "TAAVIEW GATE FAILED (shader expansion)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/taaview.cpp" -o "$OUT/taaview.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "TAAVIEW GATE FAILED (g++)"; exit 1; }
"$OUT/taaview.exe" "$OUT" "${2:-640}" "${3:-360}" 2>&1 | tee "$OUT/taaview.txt"
tail -1 "$OUT/taaview.txt" | grep -q "TAAVIEW GATE OK"
