#!/bin/bash
#
# LANE CELLVIEW2's gate: CLICKING a reference, the `.lodi` IDENTITY overlay,
# the PAINTED ground, and the MAGENTA repair.
#
# NOT RUN AS WRITTEN (2026-09-19 17:xx). Every row below is written against an
# exe that does not exist yet: this lane may not build, and the hook-up
# (scratchpad/cellview2_20260919/hookup_cellview2.py) has not been applied. The
# first row refuses for exactly that reason, so a green log can never be
# mistaken for a measured one.
#
# WHY IT IS SHAPED LIKE THIS
#
# cell_open.sh already proves the SCENE: the placements are the ones the plugin
# holds, at the positions the plugin gives, under the engine's rotation
# convention. It cannot prove any of this lane's four things, because all four
# are about what happens AFTER the scene exists:
#
#   (1) THE CLICK. It starts in GLView::mouseReleaseEvent, runs a ray test
#       against the placement table, moves a shape in the document and ends in
#       a dock. None of that is visible in a dump file, and a screenshot of a
#       highlighted box is exactly what a broken pick that highlights the WRONG
#       box also looks like. So it is measured inside the running window by
#       src/cellpicktest.cpp (`WW_CELLPICK_TEST=<report>`), which re-derives the
#       pick independently from the same boxes and refuses to accept "hit
#       something" as a pass. Row 3 is this gate's control ON THAT REPORT: a run
#       with no cell scene must make the report say FAIL, or the report is a
#       rubber stamp.
#
#   (2) THE IDENTITY OVERLAY. Judged from the LEGEND, never the picture: the
#       number of distinct colours is the number of LOD groups the bake put the
#       block's references in. Its red control is the same cell with NO bake,
#       which must come back REFUSED and single-coloured -- an overlay that
#       looks the same with and without its input is reading nothing.
#
#   (3) THE PAINTED GROUND. Judged from the mosaic census: how many landscape
#       textures the rectangle used and how many quads took an ATXT layer over
#       the quadrant's BTXT. One texture is what a broken layer reader produces
#       and it still renders a plausible ground. Its red control is
#       WW_CELL_NOTERRAIN=1, which must remove the line entirely.
#
#   (4) THE MAGENTA. A SOURCE row, and deliberately so. The defect is that an
#       already-absolute Bethesda build path had `materials/` prepended to it
#       (src/lodgen.cpp ~2212 and ~4758), which resolves to nothing, leaves the
#       diffuse slot empty and binds the missing-texture magenta under
#       Scene::DoErrorColor. Whether the repair makes a particular street stop
#       being pink is bungo's eye on a picture; whether the broken shape still
#       exists anywhere in the file is a fact, and row 8 is that fact. Row 9 is
#       its control: the same search against git HEAD must FIND the old shape.
#
# WHAT THIS GATE CANNOT TEST: that the ground looks like the game's, that the
# identity colours are the join bungo wants, that the pick feels right. Those
# are pictures, and pictures are his.
#
# USAGE
#   bash tests/spells/cell_pick.sh              the gate
#   LODI=<path to a .lodi bake> bash ...        rows 4-5 need one; without it
#                                               they SKIP BY NAME, never pass

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
WORLD="${WORLD:-Commonwealth}"
LODI="${LODI:-}"
OUT="$REPO/release"
IMG="$REPO/scratchpad/cellview2_20260919/images"
LOG="$OUT/ww_cell_pick.log"
PORT="${PORT:-14733}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
SIZE="${SIZE:-1822x960}"
CELLX="${CELLX:--20}"
CELLY="${CELLY:-7}"

mkdir -p "$IMG"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
skips=0
check() {   # check "<what>" <0|1>
	if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi
}
skip() {    # skip "<what>" -- a NAMED skip, never a pass
	say "  SKIP  $1"; skips=$((skips+1))
}

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$ESM" ] || { echo "no plugin at $ESM"; exit 2; }

say "cell_pick.sh  $(date '+%Y-%m-%d %H:%M:%S')"
say "exe: $EXE  ($(stat -c %y "$EXE" | cut -c1-19))"
say "cell: $WORLD $CELLX,$CELLY"

# ---------------------------------------------------------------------------
# 0. THE EXE MUST CARRY THIS LANE. Every source below is new or hooked up by
#    scratchpad/cellview2_20260919/hookup_cellview2.py; an exe older than any of
#    them is the previous build and this gate would be measuring it.
# ---------------------------------------------------------------------------
newer=1
for s in src/cellclick.cpp src/cellclick.h src/cellground.cpp src/cellground.h \
         src/cellidentity.cpp src/cellidentity.h src/cellpanel.cpp src/cellpanel.h \
         src/cellpicktest.cpp src/cellpicktest.h \
         src/cellview.cpp src/glview.cpp src/nifskope_ui.cpp src/lodgen.cpp; do
	if [ ! -f "$REPO/$s" ] || [ "$REPO/$s" -nt "$EXE" ]; then
		say "  $s is missing or NEWER than the exe"
		newer=0
	fi
done
check "the exe is newer than every source this gate covers" "$newer"

run_cell() {    # run_cell <tag> [extra env assignments...]
	local tag=$1; shift
	local notes="$OUT/ww_pick_${tag}.notes"
	local shot="$IMG/${tag}.png"
	rm -f "$notes" "$shot"
	env "$@" \
		WW_CELL_DATAROOT="$DATA" \
		WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" \
		WW_RENDER_VIEW=1 WW_RENDER_CLEAN=1 \
		WW_RENDER_CENTER="${CENTER:-$(( CELLX * 4096 + 2048 )),$(( CELLY * 4096 + 2048 )),0}" \
		WW_RENDER_ORTHO=2457 \
		timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$notes" 2>&1
}

# ---------------------------------------------------------------------------
# 1-2. THE CLICK, measured inside the window (src/cellpicktest.cpp).
# ---------------------------------------------------------------------------
REPORT="$OUT/ww_cellpick.report"
rm -f "$REPORT"
run_cell pick \
	WW_CELL_OPEN="$ESM|$WORLD|$CELLX,$CELLY|1" \
	WW_CELLPICK_TEST="$(winpath "$REPORT")"
if [ -s "$REPORT" ]; then
	cat "$REPORT" >> "$LOG"
	rowcount=$(grep -c -E "^(PASS|FAIL)" "$REPORT")
	rfails=$(sed -n 's/^rows [0-9]* failures \([0-9]*\)$/\1/p' "$REPORT" | tail -1)
	say "  pick self-test: $rowcount rows, ${rfails:-?} failures"
	check "the pick self-test ran and wrote its report" \
		"$([ "${rowcount:-0}" -ge 10 ] && echo 1 || echo 0)"
	check "every pick row passed (the click, the dock's rows, the highlight, and "\
"its two red controls)" "$([ "${rfails:-1}" = "0" ] && echo 1 || echo 0)"
else
	check "the pick self-test wrote its report" 0
	check "every pick row passed" 0
fi

# ---------------------------------------------------------------------------
# 3. THE CONTROL ON THE REPORT ITSELF. No cell scene: the report must say FAIL.
#    A report that only ever prints PASS is not evidence of anything.
# ---------------------------------------------------------------------------
RED_REPORT="$OUT/ww_cellpick_nocell.report"
rm -f "$RED_REPORT"
run_cell pick_nocell WW_CELLPICK_TEST="$(winpath "$RED_REPORT")"
if [ -s "$RED_REPORT" ]; then
	nf=$(sed -n 's/^rows [0-9]* failures \([0-9]*\)$/\1/p' "$RED_REPORT" | tail -1)
	say "  with no cell scene: ${nf:-?} failures"
	check "RED: with no cell scene open the pick report FAILS" \
		"$([ "${nf:-0}" -ge 1 ] && echo 1 || echo 0)"
else
	check "RED: with no cell scene open the pick report FAILS" 0
fi

# ---------------------------------------------------------------------------
# 4-5. THE IDENTITY OVERLAY, and the same cell without a bake.
# ---------------------------------------------------------------------------
if [ -z "$LODI" ] || [ ! -f "$LODI" ]; then
	skip "the identity overlay needs a .lodi bake for $WORLD (pass LODI=<path>)"
	skip "RED: the identity overlay without a bake (needs the same bake to contrast)"
else
	run_cell identity WW_CELL_OPEN="$ESM|$WORLD|$CELLX,$CELLY|1" \
		WW_CELL_OVERLAY=identity WW_CELL_LODI="$(winpath "$LODI")"
	n="$OUT/ww_pick_identity.notes"
	grep -E "references indexed|legend: " "$n" >> "$LOG" 2>&1
	groups=$(grep -o "[0-9,]* groups" "$n" | head -1 | tr -dc '0-9')
	buckets=$(grep -o "legend: [0-9]* buckets" "$n" | head -1 | tr -dc '0-9')
	say "  identity: ${groups:-0} groups in the bake, ${buckets:-0} colours drawn"
	check "the bake's group census reaches the notes" \
		"$([ "${groups:-0}" -ge 2 ] && echo 1 || echo 0)"
	check "the identity overlay produces at least two distinct colours" \
		"$([ "${buckets:-0}" -ge 2 ] && echo 1 || echo 0)"

	run_cell identity_red WW_CELL_OPEN="$ESM|$WORLD|$CELLX,$CELLY|1" \
		WW_CELL_OVERLAY=identity
	r="$OUT/ww_pick_identity_red.notes"
	rbuckets=$(grep -o "legend: [0-9]* buckets" "$r" | head -1 | tr -dc '0-9')
	say "  identity with no bake: ${rbuckets:-0} colours"
	check "RED: with no bake the overlay REFUSES by name" \
		"$(grep -q 'REFUSED: no `.lodi` bake' "$r" && echo 1 || echo 0)"
	# REWRITTEN 2026-09-19, lane CELLVIEW3. This row used to read "with no bake
	# every placement is one grey", floor `buckets <= 1`. That was true only
	# while there was exactly ONE sentinel for "this placement has no group".
	# There are now two -- "no LOD model on the base", which is correct and
	# grey, and "has a LOD model but no group", which is a defect and red --
	# and with no bake loaded EVERY placement falls into one of them, so the
	# count is 2 and the old row goes red for a reason that is the repair.
	# What the control is actually for is unchanged and is now stated exactly:
	# with no bake, not one legend key may be a REAL `.lodi` group key. The
	# group keys carry bit 0x400000000000 (src/cellview.cpp, the Identity
	# case); the sentinels are 0xfffffffffd..ff. An overlay that invented a
	# group without a bake would show up here and nowhere else.
	realkeys=$(grep -oE "key 0x[0-9a-f]+" "$r" | grep -vcE "key 0xfffffffff[d-f]$")
	check "RED: with no bake not one legend key is a real .lodi group" \
		"$([ "${realkeys:-1}" = "0" ] && echo 1 || echo 0)"
fi

# ---------------------------------------------------------------------------
# 6-7. THE PAINTED GROUND, and the same cell with the terrain off.
# ---------------------------------------------------------------------------
run_cell ground WW_CELL_OPEN="$ESM|$WORLD|$CELLX,$CELLY|1"
g="$OUT/ww_pick_ground.notes"
grep -E "^ *ground: " "$g" >> "$LOG" 2>&1
# TWO WORDINGS, ONE SET OF FACTS (lane CELLVIEW4B). The blend (src/cellsplat.cpp)
# and the mosaic fallback (src/cellground.cpp) both write this line and they say
# it differently. Reading only one of them is how four rows went red on 2026-09-19
# against a viewer that was working -- and reading only the BLEND would leave the
# fallback arm untested, which is worse. Every pattern below accepts either.
#
#   blend   ... drawn as 2234 passes over 11 landscape textures -- 1018 opaque
#               base, 1216 blended layer, 6 bare; ...
#   mosaic  ... quads over 11 landscape textures -- 1000 textured, 24 bare; ...
#               24 quads took an ATXT layer over the quadrant's BTXT
tex=$(sed -n 's/.*over \([0-9,]*\) landscape textures.*/\1/p' "$g" | head -1 | tr -dc '0-9')
quadstotal=$(sed -n 's/.*, \([0-9,]*\) land quads.*/\1/p' "$g" | head -1 | tr -dc '0-9')
if [ -z "${quadstotal:-}" ]; then
	quadstotal=$(sed -n 's/.*, \([0-9,]*\) quads over.*/\1/p' "$g" | head -1 | tr -dc '0-9')
fi
bare=$(sed -n 's/.*[,-] \([0-9,]*\) bare[;,].*/\1/p' "$g" | head -1 | tr -dc '0-9')
# "textured" is the row's real subject: quads that resolved SOMETHING. The mosaic
# states it; the blend states the complement, so it is derived, and the derivation
# is printed so a reader can see which arm answered.
textured=$(sed -n 's/.*landscape textures -- \([0-9,]*\) textured.*/\1/p' "$g" | head -1 | tr -dc '0-9')
if [ -z "${textured:-}" ] && [ -n "${quadstotal:-}" ] && [ -n "${bare:-}" ]; then
	textured=$(( quadstotal - bare ))
	arm="blend"
else
	arm="mosaic"
fi
# the ATXT layers being READ, not just the quadrant base textures
layered=$(sed -n 's/.*; \([0-9,]*\) quads took an ATXT layer.*/\1/p' "$g" | head -1 | tr -dc '0-9')
if [ -z "${layered:-}" ]; then
	layered=$(sed -n 's/.*opaque base, \([0-9,]*\) blended layer.*/\1/p' "$g" | head -1 | tr -dc '0-9')
fi
# these two the blend deliberately words exactly as the mosaic does, so one
# pattern serves both and the split cannot be lost in a rewording again
unpainted=$(sed -n 's/.*bare quads \([0-9,]*\) are unpainted.*/\1/p' "$g" | head -1 | tr -dc '0-9')
notex=$(sed -n 's/.*and \([0-9,]*\) chose an LTEX that named no texture.*/\1/p' "$g" | head -1 | tr -dc '0-9')
say "  ground [$arm]: ${tex:-0} textures, ${textured:-0} textured quads of ${quadstotal:-0}, ${layered:-0} from a layer"
say "  ground bare: ${bare:-?} = ${unpainted:-?} unpainted + ${notex:-?} LTEX with no texture"
check "the ground is painted with more than one landscape texture" \
	"$([ "${tex:-0}" -ge 2 ] && echo 1 || echo 0)"

# ---------------------------------------------------------------------------
# ROWS ADDED BY LANE CELLVIEW4B: the vertex budget, and the refusal.
#
# The blend costs one quad per contributing layer, so a heavily painted block is
# a MULTIPLE of the mosaic's fixed 4096 verts per cell. cellSplatCountVerts()
# runs before a single vertex is allocated and the blend refuses past the cap.
# Both halves are rows here, because a refusal path nobody runs is a refusal
# path nobody knows works.
# ---------------------------------------------------------------------------
counted=$(sed -n 's/.*; \([0-9,]*\) land vertices counted before allocating.*/\1/p' "$g" | head -1 | tr -dc '0-9')
capsaid=$(sed -n 's/.*counted before allocating, against the \([0-9,]*\) cap.*/\1/p' "$g" | head -1 | tr -dc '0-9')
mult=$(sed -n 's/.*; \([0-9.]*\) passes per land quad.*/\1/p' "$g" | head -1)
say "  budget: ${counted:-?} land vertices counted, cap ${capsaid:-?}, ${mult:-?} passes per land quad"
check "the blend counts its vertices before allocating and names the cap" \
	"$([ -n "${counted:-}" ] && [ "${capsaid:-0}" = "12000000" ] && echo 1 || echo 0)"
check "the passes-per-quad multiplier is stated, so the cost is measured not guessed" \
	"$([ -n "${mult:-}" ] && echo 1 || echo 0)"
# A FLOOR UNDER THE SCRAPE ITSELF (lane CELLVIEW4B). Every row above reads a
# number out of one line of prose. When a rewording makes a pattern miss, the
# variable defaults and the row can still pass for the wrong reason -- which is
# how this lane shipped a broken `sed` inside a green run. This row fails when
# the scrape came back empty, so a future rewording is a RED row and not a
# silently wrong number.
check "the census scrape actually captured its fields (no silently-empty variable)" \
	"$([ -n "${tex:-}" ] && [ -n "${quadstotal:-}" ] && [ -n "${bare:-}" ] \
	   && [ -n "${layered:-}" ] && [ "${quadstotal:-0}" -eq 1024 ] && echo 1 || echo 0)"

# THE REFUSAL, forced with the harness hook so it costs one cell instead of a
# 38x38 block. WW_CELL_SPLAT_CAP is set to 1, which every painted cell exceeds.
run_cell ground_cap WW_CELL_OPEN="$ESM|$WORLD|$CELLX,$CELLY|1" WW_CELL_SPLAT_CAP=1
gc="$OUT/ww_pick_ground_cap.notes"
grep -E "^ *ground: " "$gc" >> "$LOG" 2>&1
capbare=$(sed -n 's/.*[,-] \([0-9,]*\) bare[;,].*/\1/p' "$gc" | head -1 | tr -dc '0-9')
say "  refusal: $(grep -c "the BLEND REFUSED" "$gc") refusal line(s), mosaic bare ${capbare:-?}"
check "past the cap the blend REFUSES BY NAME instead of stalling or dying" \
	"$(grep -q "the BLEND REFUSED" "$gc" && echo 1 || echo 0)"
check "the refusal survives the fallback: the mosaic legend is printed TOO, so the downgrade is not silent" \
	"$(grep -q "the BLEND REFUSED" "$gc" && grep -q "quads took an ATXT layer\|quads over" "$gc" && echo 1 || echo 0)"
check "RED: the UNCAPPED run of the same cell carries no refusal line at all" \
	"$(grep -q "the BLEND REFUSED" "$g" && echo 0 || echo 1)"

# ---------------------------------------------------------------------------
# ROW 7, REWRITTEN 2026-09-19 BY LANE CELLVIEW3, AND WHY.
#
# It used to read:
#
#     check "most quads resolved a texture rather than falling back to bare" \
#         "$([ "${textured:-0}" -ge 1024 ] && echo 1 || echo 0)"
#
# 1024 is 32x32 -- the WHOLE population of a one-cell rectangle. A row named
# "most" whose floor is "all" can only ever be green on a cell with nothing
# wrong with it, and on Sanctuary -20,7 it was red at 891 of 1024 from the day
# it was written. It was never a floor; it was a wish.
#
# It is NOT replaced by a lower number. A floor lowered until it passes measures
# nothing. It is replaced by an IDENTITY that holds for any cell in any
# worldspace and can be recomputed by hand from the plugin:
#
#     bare == unpainted + (chose an LTEX that named no texture)
#
# -- every bare quad has exactly one of two reasons, and the census must be able
# to say which. On the pre-repair build this row is impossible to even evaluate,
# because the two terms did not exist: a null-form ATXT layer could beat a good
# BTXT and blank the quad, and nothing counted or named that case. Sanctuary
# -20,7 measured 133 bare on that build and 0 of them explained.
#
# The number is stated too, but as a MEASUREMENT with its rule, not as a pass
# condition invented to be met: after the repair, a quad is bare only where the
# plugin painted nothing at all -- and the Commonwealth WRLD names no default
# landscape texture to inherit (every subrecord dumped in
# scratchpad/mountains_20260907/report_mountains.md R6: DNAM is two floats, land
# and water height, not formids), so there is nothing the viewer could honestly
# draw there. The independent prediction from ground_probe.py for this cell is
# 24; the row allows the measured bare count to be no worse than that, which is
# a ceiling on a defect and not a floor on a hope.
# ---------------------------------------------------------------------------
check "every bare quad is accounted for: bare == unpainted + LTEX-with-no-texture" \
	"$([ -n "${bare:-}" ] && [ -n "${unpainted:-}" ] && [ -n "${notex:-}" ] && \
	   [ "$(( unpainted + notex ))" = "$bare" ] && echo 1 || echo 0)"
check "no quad is bare for any reason other than the plugin painting nothing there \
(<=24 on $WORLD $CELLX,$CELLY, the independent count from ground_probe.py)" \
	"$([ "${bare:-9999}" -le 24 ] && echo 1 || echo 0)"
check "the ATXT layers are read, not just the quadrant base textures" \
	"$([ "${layered:-0}" -ge 1 ] && echo 1 || echo 0)"

# ---------------------------------------------------------------------------
# 7b. RED CONTROL ON THE LAYER CHOICE, IN THE SOURCE.
#
# The repair is one line: a layer whose LTEX is the null form may not win. git
# HEAD's cellground.cpp has no such guard, so the same expression must come out
# FALSE on it -- the same shape as rows 8-9 below, and for the same reason: the
# pre-repair state is reachable as text without launching an older exe (which
# this tree forbids, because an old rung rewrites bungo's Recent Files).
# ---------------------------------------------------------------------------
guard='if ( !layers\[li\].ltex )'
check "a layer naming the null form cannot win the quad" \
	"$(grep -q "$guard" "$REPO/src/cellground.cpp" && echo 1 || echo 0)"
git -C "$REPO" show HEAD:src/cellground.cpp > "$OUT/ww_cellground_head.cpp" 2>/dev/null
check "RED: git HEAD has no such guard, so the check can see the defect" \
	"$(grep -q "$guard" "$OUT/ww_cellground_head.cpp" && echo 0 || echo 1)"

run_cell ground_red WW_CELL_OPEN="$ESM|$WORLD|$CELLX,$CELLY|1" WW_CELL_NOTERRAIN=1
gr="$OUT/ww_pick_ground_red.notes"
check "RED: with the terrain off no mosaic is reported at all" \
	"$(grep -q "landscape textures" "$gr" && echo 0 || echo 1)"

# ---------------------------------------------------------------------------
# 10-11. THE LEGEND AND THE PICTURE MUST AGREE. A NEW ROW, 2026-09-19, lane
#        CELLVIEW3, and the first row in this gate that looks at a rendered
#        image at all.
#
# The defect it exists for: the draw site wrote flat grey 0.35 for the unknown
# bucket while the legend printed overlayColour() of the same key -- mauve
# 0.60,0.21,0.37. Two code paths, two answers, both green in every row above,
# because nobody had ever compared the legend to the pixels it describes.
#
# tests/spells/cell_legend_colour.py measures the overlay as what it is -- a
# vertex colour multiplying the shape's own texture -- by dividing the overlay
# frame by the plain frame of the SAME cell from the SAME camera (which is what
# run_cell gives us for free: `identity` and `ground` differ only by the
# overlay). Lighting and texture cancel; the median ratio IS the drawn colour.
#
# Its own control is printed in the same run: the distance from the drawn colour
# to the mauve the pre-repair legend printed. That number is the row failing on
# the pre-repair state, computed without launching the old exe -- which this
# tree forbids, since an old rung rewrites bungo's Recent Files list.
# ---------------------------------------------------------------------------
if [ -n "$LODI" ] && [ -f "$LODI" ] && [ -f "$IMG/identity.png" ] && [ -f "$IMG/ground.png" ]; then
	lc="$OUT/ww_legend_colour.txt"
	python "$REPO/tests/spells/cell_legend_colour.py" \
		"$IMG/ground.png" "$IMG/identity.png" "$OUT/ww_pick_identity.notes" > "$lc" 2>&1
	lrc=$?
	cat "$lc" >> "$LOG"
	grep -E "measured drawn colour|distance to" "$lc" | while read -r l; do say "  $l"; done
	check "the legend's printed rgb is the colour the picture draws" \
		"$([ "$lrc" = "0" ] && echo 1 || echo 0)"
	mauve=$(sed -n 's/.*PRE-REPAIR printed mauve [0-9.,]*: \([0-9.]*\)/\1/p' "$lc" | tail -1)
	check "RED: the colour the pre-repair legend printed is NOT the colour drawn \
(distance ${mauve:-?} from the measurement)" \
		"$(python -c "import sys; sys.exit(0 if float('${mauve:-0}') > 0.15 else 1)" \
		   && echo 1 || echo 0)"
else
	skip "the legend-vs-picture row needs LODI= and both renders"
	skip "RED: the same row against the pre-repair colour"
fi

# ---------------------------------------------------------------------------
# 12. THE IDENTITY BUCKET IS SPLIT, AND THE DEFECT HALF IS EMPTY.
#
# Measured first, then coded (scratchpad/cellview3_20260919/join_probe.py, an
# independent .lodi + plugin join): of Sanctuary -20,7's 142 REFRs, 26 are in
# the bake and 116 are not, and the count of "drawn, the base HAS a LOD model,
# and yet no group" is ZERO. So the grey was the RIGHT answer and could not say
# so. The two facts now have two keys and two colours, and this row asserts the
# defect half is empty -- if it ever stops being empty, a real join defect has
# appeared and the picture will have loud red in it.
# ---------------------------------------------------------------------------
if [ -n "$LODI" ] && [ -f "$LODI" ]; then
	n="$OUT/ww_pick_identity.notes"
	nolod=$(sed -n 's/.*key 0xffffffffff  \([0-9]*\) placements.*/\1/p' "$n" | head -1)
	orphan=$(sed -n 's/.*key 0xfffffffffe  \([0-9]*\) placements.*/\1/p' "$n" | head -1)
	say "  identity buckets: ${nolod:-0} no LOD model (correct), ${orphan:-0} with a LOD model and no group"
	check "the legend names the two reasons a placement has no group" \
		"$(grep -q 'no LOD model on the base (correct)' "$n" && echo 1 || echo 0)"
	check "no drawn placement has a LOD model and no .lodi group" \
		"$([ "${orphan:-0}" = "0" ] && echo 1 || echo 0)"
	check "the expected-grey bucket is not empty either, so the split is real" \
		"$([ "${nolod:-0}" -ge 1 ] && echo 1 || echo 0)"
else
	skip "the identity bucket split needs LODI="
	skip "RED: the defect half of the identity split"
	skip "the expected-grey half of the identity split"
fi

# ---------------------------------------------------------------------------
# 13. THE EFFECT MATERIALS. A cell with cars in it, because that is where the
#     `.bgem` shapes are (measured on Vehicles\Automotive\Sedan02_Postwar.nif:
#     five shapes, one BSEffectShaderProperty naming Car_Glass01.BGEM, four
#     texture sets -- the glass had no texture from either source and came back
#     with an empty diffuse, which is the missing-texture magenta).
#
#     The row is on the CENSUS, not on the picture: "how pink does it look" is
#     bungo's eye. What is a fact is how many shapes were textured from a
#     `.bgem` and how many were drawn neutral because nothing resolved.
# ---------------------------------------------------------------------------
DCX="${DCX:-5}"; DCY="${DCY:--11}"
run_cell downtown WW_CELL_OPEN="$ESM|$WORLD|$DCX,$DCY|1" \
	WW_RENDER_CENTER="$(( DCX * 4096 + 2048 )),$(( DCY * 4096 + 2048 )),0"
d="$OUT/ww_pick_downtown.notes"
grep -E "^ *materials: " "$d" >> "$LOG" 2>&1
fromeff=$(sed -n 's/.*materials: \([0-9,]*\) shapes textured.*/\1/p' "$d" | head -1 | tr -dc '0-9')
neutral=$(sed -n 's/.*effect material, \([0-9,]*\) drawn neutral.*/\1/p' "$d" | head -1 | tr -dc '0-9')
say "  downtown $DCX,$DCY: ${fromeff:-0} shapes from a .bgem, ${neutral:-0} drawn neutral"
check "the census states what the effect materials did" \
	"$(grep -q "shapes textured from a \`.bgem\` effect material" "$d" && echo 1 || echo 0)"
check "at least one shape in a cell full of cars is textured from a .bgem" \
	"$([ "${fromeff:-0}" -ge 1 ] && echo 1 || echo 0)"
# THE CONTROL ON THE NUMBER ABOVE, AND WHY IT IS NOT A GIT-HEAD GREP.
#
# The pre-repair cell viewer is in the WORKING TREE, not in a commit: nothing of
# this lane's line of work is committed, so `git show HEAD:src/cellview.cpp` is
# an older file that has no `.bgem` in it at all and can neither confirm nor
# deny the defect. The number is controlled at its SOURCE instead. Every one of
# those 9 shapes was textured out of `LodSrcShape::effectTex0`, a field that did
# not exist before today -- so on any build before it the same census line could
# only ever have printed 0, by construction and not by luck. git HEAD is used
# here purely as a version known to predate the field.
#
# The other half of the defect is in the viewer: `isMaterialFile()` accepted a
# `.bgem` and the writer then put it in a BSLightingShaderProperty's **Name**,
# which the renderer cannot resolve -- ten empty slots, and an empty diffuse is
# the missing-texture magenta. The row below pins the predicate to `.bgsm` only.
check "a .bgem is no longer offered to the shader property's Name" \
	"$(grep -q 'return s.endsWith( QStringLiteral( ".bgsm" ), Qt::CaseInsensitive );' \
	   "$REPO/src/cellview.cpp" && echo 1 || echo 0)"
heff=$(git -C "$REPO" show HEAD:src/lodgen.cpp 2>/dev/null | grep -c effectTex0)
say "  git HEAD mentions effectTex0 $heff times"
check "RED: the field those shapes were textured from does not exist on git HEAD, \
so that count could only have been 0" \
	"$([ "${heff:-1}" = "0" ] && echo 1 || echo 0)"

# ---------------------------------------------------------------------------
# 8-9. THE MAGENTA REPAIR, in the source, with the control on git HEAD.
#
# The broken shape is a bare `prepend( "materials/" )` that is NOT preceded by
# the `lastIndexOf( "materials/" )` cut. Counted with grep -B1 so the two are
# judged together rather than by eye.
# ---------------------------------------------------------------------------
count_paths() {   # count_paths <file-or-> : "<prepends> <cuts>"
	local src
	src=$(cat "$1")
	local p c
	p=$(printf '%s\n' "$src" | grep -c 'prepend( QStringLiteral( "materials/" ) )')
	c=$(printf '%s\n' "$src" | grep -c 'lastIndexOf( QStringLiteral( "materials/" )')
	echo "$p $c"
}

set -- $(count_paths "$REPO/src/lodgen.cpp")
prep=$1; cuts=$2
say "  lodgen.cpp: $prep materials/ prepends, $cuts absolute-path cuts"
check "every materials/ prepend in lodgen.cpp is paired with the absolute-path cut" \
	"$([ "${cuts:-0}" -ge "${prep:-1}" ] && echo 1 || echo 0)"

git -C "$REPO" show HEAD:src/lodgen.cpp > "$OUT/ww_lodgen_head.cpp" 2>/dev/null
set -- $(count_paths "$OUT/ww_lodgen_head.cpp")
hprep=$1; hcuts=$2
say "  git HEAD: $hprep prepends, $hcuts cuts"
# HEAD is the shared tree's last commit and carries other lanes' work too; it is
# used here ONLY as a version known to lack the cut, which is what makes it a
# control: the same expression must come out FALSE on it.
check "RED: the same expression is FALSE on git HEAD, so it can see the defect" \
	"$([ "${hcuts:-9}" -lt "${hprep:-0}" ] && echo 1 || echo 0)"

say ""
say "skips: $skips"
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
say "done"
exit "$fails"
