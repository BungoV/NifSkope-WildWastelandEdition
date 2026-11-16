#!/bin/bash
# Lane VKWATER3 (2026-11-13, row G5): the water's shaded draw on the present device (vkwater.inc waterRun) against GL's
# own water program (fo4_default.vert [+ fo4_waterdisp.tesc + .tese] + its .frag, the FOGLEAK + FRAME_UBO build the twins
# carry), per sample, colour + depth. bash tests/vkframe/waterdraw.sh <out dir> [program, default fo4_waterlook; item 4:
# fo4_waterlookd, the tessellated twin with its reds 16 / 32]. Needs MSYS2 UCRT64 g++ and python.
# Not while NifSkope or the game runs (a window of ours): the script refuses then.
# Lane VKWATER4: [extra defines, comma separated] as argv 3 (e.g. WW_CELL_RESTIR,WW_CELL_RTDDGI,WW_CELL_RTDDGI_CASCADES:
# the all-on RT build, which the twin must refuse by name, never draw).
# Last line: `WATERDRAW GATE OK` or `WATERDRAW GATE FAILED`.
set -u
OUT="${1:?out dir}"
PROG="${2:-fo4_waterlook}"
DEFS="${3:-}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
PY="${PY:-$(command -v python)}"	# before the UCRT64 PATH
export PATH="/c/msys64/ucrt64/bin:$PATH"
if powershell -NoProfile -Command "if (Get-Process NifSkope,Fallout4 -ErrorAction SilentlyContinue) { 'busy' }" | grep -q busy; then echo "WATERDRAW GATE FAILED (NifSkope or the game runs: no window of ours now)"; exit 1; fi
mkdir -p "$OUT"
# lane VKFLIGHT1: WD_FLIGHT=1 makes cellIsLut the app's own 16^3 LUT in the format celllights.cpp uploads (bungo's flight:
# "1 of 26 inputs: other formats cellIsLut(0x8051)"); WD_LUTFMT set by hand overrides it (the red: 0x8051)
if [ -n "${WD_FLIGHT:-}" ] && [ -z "${WD_LUTFMT:-}" ]; then
	LF=$(grep -o 'glTexImage3D( GL_TEXTURE_3D, 0, GL_RGBA\?8, 16, 16, 16' "$ROOT/src/gl/celllights.cpp" | grep -o 'GL_RGBA\?8')
	case "$LF" in GL_RGB8) export WD_LUTFMT=0x8051 ;; GL_RGBA8) export WD_LUTFMT=0x8058 ;; *) echo "WATERDRAW GATE FAILED (the app's LUT upload not found: '$LF')"; exit 1 ;; esac
fi
[ -n "${WD_LUTFMT:-}" ] && echo "WATERDRAW flight arm: cellIsLut 16^3 fmt $WD_LUTFMT"
"$PY" -I "$ROOT/tests/vkframe/waterdraw_prep.py" "$OUT" "$PROG" "$DEFS" || { echo "WATERDRAW GATE FAILED (job)"; exit 1; }
g++ -O2 -std=c++17 -pthread -I"$ROOT/src" "$ROOT/tests/vkframe/waterdraw.cpp" -o "$OUT/waterdraw.exe" -lopengl32 -lgdi32 -luser32 \
	|| { echo "WATERDRAW GATE FAILED (g++)"; exit 1; }
"$OUT/waterdraw.exe" "$OUT" "$PROG" "$DEFS" 2>&1 | tee "$OUT/waterdraw.txt"
tail -1 "$OUT/waterdraw.txt" | grep -q "WATERDRAW GATE OK"
