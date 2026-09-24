#!/bin/sh
# ---------------------------------------------------------------------------
# The two new gate rows, RUN AGAINST THE BUILD THEY ARE SUPPOSED TO CONVICT.
#
# A row that has never been seen red is a row nobody has tested. Row 14 was
# already watched failing on the shipped sheets (16 of 16 frames); this script
# does the same for row 15, whose two clauses fail for two different reasons
# and therefore have to be shown separately:
#
#   A. THE COUNT CLAUSE, against exe 88d6abb3 itself. That exe predates
#      WW_IMPOSTOR_ORBIT_VIEWS, so it orbits its own even ring instead and the
#      count does not match. This is the clause that stops an exe which IGNORES
#      the variable from quietly scoring some other set of views and passing.
#
#   B. THE EQUALITY CLAUSE, against the shipped SHADER. The ray repair lives in
#      res/shaders/impostor_oct.vert, a runtime asset, not in the exe -- so the
#      honest red control for it is the new exe with the OLD shader deployed,
#      and saying that out loud matters more than a tidier story. The old text
#      is reconstructed by reversing fix03, not taken from git (the file is not
#      in HEAD).
#
# Restores the deployed shader and verifies the restore with cmp.
# ---------------------------------------------------------------------------
set -u
R="E:/Projects/NifskopeWildWastelandEdition"
S="$R/scratchpad/impostorfix1_20260919"
LODM="$S/fixture/blast_n4/cards/000531b3_oct.lodm"
NIF="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape/Trees/TreeMapleblasted05.nif"
VIEWS=$( python "$R/tests/spells/impostor_bake_views.py" "$LODM" )
mkdir -p "$S/red"

run() {  # $1 exe  $2 blend  $3 log
	WW_IMPOSTOR_PREVIEW=orbit WW_IMPOSTOR_LODM="$LODM" \
	WW_IMPOSTOR_LOG="$S/red/$3" WW_IMPOSTOR_ORBIT_VIEWS="$VIEWS" \
	WW_IMPOSTOR_BLEND="$2" WW_RENDER_SIZE=1024x1024 WW_WINDOW_AT=1960,40 \
	timeout 900 "$1" "$NIF" --port 27717 > "$S/red/$3.stdout" 2>&1
	echo "  rc=$? $( grep -E '^orbit (counted|iou mean)' "$S/red/$3" | tr '\n' ' ' )"
}

echo "A. exe 88d6abb3 (predates WW_IMPOSTOR_ORBIT_VIEWS), asked for 16 bake directions:"
run "$R/release/NifSkope.before_impostorfix1.exe" 1 old_exe.log

echo "B. this exe with the SHIPPED shader deployed:"
cp "$R/release/shaders/impostor_oct.vert" "$S/red/new_shader.vert"
cp "$S/old_impostor_oct.vert" "$R/release/shaders/impostor_oct.vert"
run "$R/release/NifSkope.exe" 0 oldshader_off.log
run "$R/release/NifSkope.exe" 1 oldshader_on.log
cp "$S/red/new_shader.vert" "$R/release/shaders/impostor_oct.vert"
cmp "$R/res/shaders/impostor_oct.vert" "$R/release/shaders/impostor_oct.vert" \
	&& echo "  shader restored and verified against res/"
