#!/bin/bash
# Lane VKFAR28 (2026-11-16): the far LOD rings under the RT GI keys on the present device (vkfarlodrt.inc farLodRtRun,
# twin 1 all-on RESTIR + RTDDGI + RTDDGI_CASCADES, twin 2 RTDDGI + RTDDGI_CASCADES) against GL's own fo4_cellcsm under the
# same keys, per sample, colour + depth; cellRestir / cellDdgiAtlas are the shared texel buffers (vkrtbuf.h rtBufShare).
# bash tests/vkframe/farlodrtdraw.sh <out dir>. Needs MSYS2 UCRT64 g++ and python. Bar and reds: farlodrtdraw.cpp's header.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Last line: `FARLODRTDRAW GATE OK` or `FARLODRTDRAW GATE FAILED`.
set -u
OUT="${1:?out dir}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "FARLODRTDRAW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
"$PY" -I "$ROOT/tests/vkframe/farlodrtdraw_prep.py" "$OUT" || { echo "FARLODRTDRAW GATE FAILED (job)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/farlodrtdraw.cpp" -o "$OUT/farlodrtdraw.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "FARLODRTDRAW GATE FAILED (g++)"; exit 1; }
"$OUT/farlodrtdraw.exe" "$OUT" 2>&1 | tee "$OUT/farlodrtdraw.txt"
tail -1 "$OUT/farlodrtdraw.txt" | grep -q "FARLODRTDRAW GATE OK"
