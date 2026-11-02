#!/bin/bash
# lane MATLOOK1 (TRANSLUC1): build + run the translucency law test, green and red (WWTRANSL_RED must fail).
# usage: bash tests/translucency/translucency.sh [outdir]   (run from anywhere; outdir defaults to $TMPDIR)
T="$(cd "$(dirname "$0")/../.." && pwd)"
O="${1:-${TMPDIR:?set TMPDIR off C:}}"
mkdir -p "$O"
export PATH="/c/msys64/ucrt64/bin:$PATH"
cd "$T" || exit 2
g++ -std=c++17 -O1 -I"$T/src" tests/translucency/translucency.cpp -o "$O/transl_green.exe" || exit 2
g++ -std=c++17 -O1 -DWWTRANSL_RED -I"$T/src" tests/translucency/translucency.cpp -o "$O/transl_red.exe" || exit 2
g++ -std=c++17 -O1 -DWWTRANSL_THICK_RED -I"$T/src" tests/translucency/translucency.cpp -o "$O/transl_thickred.exe" || exit 2
timeout 30 "$O/transl_green.exe"; G=$?
echo "--- red (WWTRANSL_RED, must FAIL)"
timeout 30 "$O/transl_red.exe"; R=$?
echo "--- red (WWTRANSL_THICK_RED, a thick object takes no sun, must FAIL)"
timeout 30 "$O/transl_thickred.exe"; RT=$?
# lane MATLOOK2: the base's text (1e433b7b: the twins and the NIF view without the term) must fail the pins
echo "--- red (base 1e433b7b shader text, must FAIL)"
B="$O/transl_base"
mkdir -p "$B/res/shaders" "$B/src/gl"
for f in res/shaders/cell_transl.glsl res/shaders/cell_lights.glsl res/shaders/fo4_default.frag src/gl/cellbindless.cpp; do
	"${GIT:-git}" show 1e433b7b:"$f" > "$B/$f" || exit 2
done
( cd "$B" && timeout 30 "$O/transl_green.exe" ); RB=$?
echo "green rc=$G red rc=$R thick red rc=$RT base-text red rc=$RB"
[ "$G" = 0 ] && [ "$R" != 0 ] && [ "$RT" != 0 ] && [ "$RB" != 0 ] && echo "GATE PASS" && exit 0
echo "GATE FAIL"; exit 1
