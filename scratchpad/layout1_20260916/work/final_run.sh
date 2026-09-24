#!/bin/bash
# lane LAYOUT1: the final sweep on the final exe -- the layout gate, the two
# harnesses whose tools this lane changed after their last run, the .lodt bake
# for the second picture, and the panel self-test with its screenshot.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
cd "$ROOT" || exit 2
export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"
export USER=bungo
L="$ROOT/scratchpad/layout1_20260916/work/harness"
W="$ROOT/scratchpad/layout1_20260916/work/pics"
NS="$ROOT/release/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
mkdir -p "$L" "$W"

# --- the .lodt, baked to its new path by the exe under test ----------------
VT="$W/vt"
if ! find "$VT" -name '*.lodt' 2>/dev/null | grep -q lodt; then
	rm -rf "$VT"; mkdir -p "$VT/mod" "$VT/obj" "$VT/tex"
	VA="$(cd "$VT" && pwd -W)"
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region -20 24 -19 25 --dim 4 --data-root "$DATA" \
		--out-dir "$VA/obj" --vt "$VA/mod" --tex-dir "$VA/tex" --cover \
		> "$W/vt.log" 2>&1
	echo "vt bake rc=$?"
fi
find "$VT" -name '*.lodt' -o -name '*.lodm' | sed 's/^/  /'

# --- the panel self-test, with the screenshot ------------------------------
SHOT="$(cd "$W" && pwd -W)/panel.png" bash "$ROOT/tests/spells/lod_generation.sh" \
	> "$W/panel_selftest.log" 2>&1
echo "panel rc=$?, shot $(stat -c%s "$W/panel.png" 2>/dev/null || echo 0) bytes"
tail -3 "$W/panel_selftest.log" | sed 's/^/  | /'

for h in lodgen_layout lodgen_native lodgen_btofree; do
	printf '=== %s  %s\n' "$h" "$(date +%H:%M:%S)"
	timeout 3600 bash "tests/spells/$h.sh" > "$L/$h.log" 2>&1
	echo "   rc=$?"
	tail -3 "$L/$h.log" | sed 's/^/   | /'
done
echo "all done $(date +%H:%M:%S)"
