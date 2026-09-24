#!/bin/sh
# IMPOSTORLOOK1 -- NOT RUN BY THIS LANE. This lane is OFFLINE; lane CELLWORK1
# owns the build and exe slots.
#
# WHAT IS MISSING. bungo asked for every impostor beside the model it was baked
# from, AT EVERY BAKED ANGLE. Two of the five subjects already have mesh+card
# grabs at their own bake directions (IMPOSTORFIX5); three do not, and nothing
# in the tree can stand in for them -- every other run in scratchpad/ is at the
# 24 ORBIT views, which are not bake directions. Rather than substitute a
# nearby view, those cells say MISSING and this script shoots exactly them:
#
#     blast_n8   64 directions   0 shot   (its N=8 grid was never photographed)
#     dead_n4    16 directions   0 shot
#     rock_n4    16 directions   0 shot
#
# 96 directions, mesh + card = 192 grabs, three exe runs.
#
# WHY THESE VIEWS AND NOT A TYPED LIST: the directions come out of each card's
# own .lodm grid through tests/spells/impostor_bake_views.py, so this script
# cannot be edited to flatter a card.
#
# CHROME: the orbit preview hook clears Scene::ShowAxes | Scene::ShowGrid and
# ogl->showCursor unconditionally (src/nifskope_ui.cpp:22429-22432) and hides
# every dock and the viewport header. WW_RENDER_CLEAN=1 is passed as well so a
# reader does not have to take that on trust. The grid mattered: it painted a
# screen-plane lattice into axis-aligned ortho MESH grabs and contaminated a
# whole comparison once (skill ww-reference-card-diagnose section 3).
#
# SIZE IS PROVED FROM THE PNG HEADERS, not from ww_harness_window.log -- that
# guard measures the TOP-LEVEL WINDOW and refuses every offscreen orbit run it
# sees (IMPOSTORFIX5 section 2). This script reads the size back off the bytes.
#
# BEFORE RUNNING: Fallout4.exe and NifSkope.exe must both be down, and only one
# NifSkope instance may exist at a time -- the runs below are strictly serial.
set -u
REPO="E:/Projects/NifskopeWildWastelandEdition"
MINE="$REPO/scratchpad/impostorlook1_20260919"
EXE="$REPO/release/NifSkope.exe"
FX="$REPO/scratchpad/impostorfix5_20260919/fixture"
D="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape"
P=28500

tasklist | grep -i -E "Fallout4|NifSkope" && { echo "REFUSED: a game or a NifSkope is up"; exit 1; }

shoot() {   # $1 tag  $2 lodm  $3 mesh
	P=$((P+1))
	t="$1"
	V="$( python "$REPO/tests/spells/impostor_bake_views.py" "$2" )"
	n=$(printf '%s' "$V" | tr ',' '\n' | wc -l)
	mkdir -p "$MINE/shots/$t"; rm -f "$MINE/shots/$t"/*.png
	printf '%-10s %3d directions  ' "$t" "$n"
	WW_IMPOSTOR_PREVIEW=orbit \
	WW_IMPOSTOR_LODM="$2" \
	WW_IMPOSTOR_LOG="$MINE/shots/$t.log" \
	WW_IMPOSTOR_SHOT="$MINE/shots/$t/v" \
	WW_IMPOSTOR_ORBIT_VIEWS="$V" \
	WW_IMPOSTOR_BLEND=1 \
	WW_RENDER_CLEAN=1 \
	WW_RENDER_SIZE=1024x1024 \
	WW_WINDOW_AT=1960,40 \
	timeout 1800 "$EXE" "$3" --port "$P" > "$MINE/shots/$t.stdout" 2>&1
	rc=$?
	# the gate: one mesh and one card per DISTINCT direction, at 1024x1024,
	# read out of the PNG headers and not out of any log.
	python - "$MINE/shots/$t" "$V" <<'PY'
import sys, os, struct, glob
d, V = sys.argv[1], sys.argv[2]
want = set()
for v in V.split(','):
    az, el = v.split(':')
    want.add((int(float(az)), int(abs(float(el)))))
bad = []
for (az, el) in sorted(want):
    for k in ('mesh', 'card'):
        p = '%s/v_az%03d_el%02d_%s.png' % (d, az, el, k)
        if not os.path.exists(p):
            bad.append('MISSING ' + os.path.basename(p)); continue
        b = open(p, 'rb').read(33)
        w, h = struct.unpack('>II', b[16:24])          # IHDR, from the bytes
        if (w, h) != (1024, 1024):
            bad.append('%s is %dx%d' % (os.path.basename(p), w, h))
print('   distinct directions %d, expected files %d, faults %d' % (len(want), 2*len(want), len(bad)))
for x in bad[:8]: print('   ' + x)
sys.exit(1 if bad else 0)
PY
	echo "   rc=$rc gate=$?"
}

shoot blast_n8 "$FX/blast_n8/cards/000531b3_oct.lodm" "$D/Trees/TreeMapleblasted05.nif"
shoot dead_n4  "$FX/dead_n4/cards/001236b4_oct.lodm"  "$D/Trees/BlastedForestDestroyedTreeUpright01.nif"
shoot rock_n4  "$FX/rock_n4/cards/000211a3_oct.lodm"  "$D/Rocks/RockCliff02_Alt.nif"

# then, offline again, re-make the three sheets with the new grabs:
#   edit GRABS[] in make_A_sheets.py to point blast_n8 / dead_n4 / rock_n4 at
#   scratchpad/impostorlook1_20260919/shots/<tag>, then `python make_A_sheets.py`.
echo "done. re-run make_A_sheets.py with GRABS[] pointed at shots/<tag>."
