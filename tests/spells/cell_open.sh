#!/bin/bash
#
# Opening a WHOLE EXTERIOR CELL -- `.wwcell` / `WW_CELL_OPEN`, the Creation
# Kit view bungo asked for on 2026-09-19 ("I think a prerequisite would be, to
# be able to view a whole cell like the CK editor").
#
# WHERE THE NUMBERS COME FROM
#
# The expected values are PRE-REGISTERED: they come from an independent Python
# walk of Fallout4.esm (tests/spells/cell_census.py and cell_five_refs.txt,
# written 2026-09-19 12:1x and 15:1x, sharing no code with src/esmdata.cpp) and
# from the source -- NOT from a run of this code. A row that disagrees is
# EVIDENCE, not a number to edit until it matches.
#
# WHY THIS EXISTS
#
# A cell viewer is one of the easiest things in this repository to fake. A scene
# that drops a third of its references, or places them all at the cell origin,
# or silently skips every record type that is not a STAT, still renders a
# convincing street. And once the geometry is WELDED (src/cellview.h 1) the
# scene itself can no longer be asked how many references it drew.
#
# So nothing here is judged by eye:
#
#   (a) THE PLACEMENT CENSUS. The builder that places each reference writes it
#       down (`WW_CELL_DUMP`), and that file is compared against an INDEPENDENT
#       walk of the same plugin by a different reader in a different language
#       (tests/spells/cell_census.py, which parses the ESM itself). Three cells,
#       chosen to be different KINDS of cell and not three easy ones.
#
#   (b) FIVE NAMED REFERENCES, BY FORM ID, each with the world position the
#       plugin gives it. A census can be right in total and wrong per row. The
#       five are one per record type Sanctuary has, and FOUR of them are types
#       this lane's reader change added -- a viewer that has quietly fallen back
#       to STAT-only fails four rows here while its total still looks plausible.
#
#   (c) A RED CONTROL ON THE TRANSFORM. The dump carries each placement's world
#       AABB, which depends on the rotation convention and the scale, not only
#       on the parse. `--red` recomputes the boxes under the WRONG euler
#       convention (+x,+y,+z instead of the engine's -x,-y,-z) and the gate must
#       then report that the boxes MOVED and name references. A gate that cannot
#       tell the two conventions apart is measuring nothing.
#
#   (d) THE OVERLAYS. Each overlay must produce at least two distinct colours,
#       counted from the legend the census line prints, not from the picture.
#       An overlay that yields ONE colour is an overlay that is not reading its
#       field, and that is exactly how a broken one looks.
#
# WHAT THIS GATE CANNOT TEST: that the picture looks right, that the materials
# resolved, that the navigation feels like the CK's. Those are bungo's, and the
# pictures in the lane report are for him to judge.
#
# USAGE
#   bash tests/spells/cell_open.sh            the gate
#   bash tests/spells/cell_open.sh --red      the transform control; rows 1-3
#                                             pass only if the boxes MOVED

set -u

. "$(dirname "$0")/_harness.sh"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$REPO/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
WORLD="${WORLD:-Commonwealth}"
OUT="$REPO/release"
IMG="$REPO/scratchpad/cellview1_20260919/images"
LOG="$OUT/ww_cell_open.log"
PORT="${PORT:-14731}"
SPEC="$REPO/tests/fixtures/empty.wwcell"
# The main window is floored at about 1822 px wide on this machine (docks), so a
# narrower request comes back REFUSED in release/ww_harness_window.log. Ask for
# what it can give: nothing here measures pixels, but a refusal in the log next
# to a green gate is a thing someone later has to explain away.
SIZE="${SIZE:-1822x960}"
RED=""
[ "${1:-}" = "--red" ] && RED="1"

mkdir -p "$IMG"
: > "$LOG"
say() { echo "$@" | tee -a "$LOG"; }
fails=0
check() {   # check "<what>" <0|1>
	if [ "$2" = "1" ]; then say "  PASS  $1"; else say "  FAIL  $1"; fails=$((fails+1)); fi
}

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$ESM" ] || { echo "no plugin at $ESM"; exit 2; }

say "cell_open.sh  $(date '+%Y-%m-%d %H:%M:%S')${RED:+   RED CONTROL RUN}"
say "exe: $EXE  ($(stat -c %y "$EXE" | cut -c1-19))"
say "esm: $ESM"

# ---------------------------------------------------------------------------
# 0. the exe must be NEWER than every source this gate covers, or the gate is
#    measuring the last build and not this one.
# ---------------------------------------------------------------------------
newer=1
for s in src/cellview.cpp src/cellview.h src/cellpick.cpp src/cellpick.h \
         src/esmdata.cpp src/esmdata.h src/nifskope.cpp; do
	if [ "$REPO/$s" -nt "$EXE" ]; then
		say "  $s is NEWER than the exe"
		newer=0
	fi
done
check "the exe is newer than every source this gate covers" "$newer"

# ---------------------------------------------------------------------------
# 1..3. the placement census, three different KINDS of cell.
#
#   -30,-30  wilderness   -- 74 refrs, 23 distinct models, no SCOL at all
#   -20,7    Sanctuary    -- 142 refrs, 123 distinct models, 134 SCOL parts
#   5,-11    downtown 1x1 -- 1483 refrs, 375 distinct models, 178 SCOL parts
#
# Those three numbers are the PRE-REGISTERED census (cell_census.py, 2026-09-19
# 12:1x). Each run also writes its picture; the shots are the lane report's, the
# numbers are the gate's.
# ---------------------------------------------------------------------------
run_cell() {    # run_cell <x> <y> <n> <tag> [extra env assignments...]
	local x=$1 y=$2 n=$3 tag=$4; shift 4
	local dump="$OUT/ww_cell_${tag}.dump"
	local notes="$OUT/ww_cell_${tag}.notes"
	local shot="$IMG/${tag}.png"
	local ortho=$(( n * 4096 * 6 / 10 ))
	rm -f "$dump" "$notes" "$shot"
	env "$@" \
		WW_CELL_OPEN="$ESM|$WORLD|$x,$y|$n" \
		WW_CELL_DATAROOT="$DATA" \
		WW_CELL_DUMP="$(winpath "$dump")" \
		WW_RENDER_SHOT="$(winpath "$shot")" WW_RENDER_SIZE="$SIZE" \
		WW_RENDER_VIEW=1 WW_RENDER_CLEAN=1 \
		WW_RENDER_CENTER="$(( x * 4096 + 2048 )),$(( y * 4096 + 2048 )),0" \
		WW_RENDER_ORTHO="$ortho" \
		timeout 900 "$EXE" --port "$PORT" "$(winpath "$SPEC")" > "$notes" 2>&1
}

for spec in "-30 -30 1 wild" "-20 7 1 sanctuary" "5 -11 1 downtown"; do
	set -- $spec
	x=$1; y=$2; n=$3; tag=$4
	run_cell "$x" "$y" "$n" "$tag"
	dump="$OUT/ww_cell_${tag}.dump"
	notes="$OUT/ww_cell_${tag}.notes"
	grep -E "refrs read|distinct models|source triangles|built in" "$notes" >> "$LOG" 2>&1
	if [ -s "$dump" ]; then
		python "$REPO/tests/spells/cell_open_check.py" \
			--esm "$ESM" --data "$DATA" --world "$WORLD" \
			--cell "$x" "$y" --n "$n" --dump "$dump" --notes "$notes" \
			${RED:+--red} >> "$LOG" 2>&1
		rc=$?
		if [ -n "$RED" ]; then
			check "$tag $x,$y: the wrong euler convention MOVES the boxes (red control)" \
				"$([ "$rc" -eq 0 ] && echo 1 || echo 0)"
		else
			check "$tag $x,$y ${n}x${n}: every placement's world box matches the plugin" \
				"$([ "$rc" -eq 0 ] && echo 1 || echo 0)"
		fi
		say "  $tag: $(( $(wc -l < "$dump") - 1 )) placements dumped, shot $IMG/$tag.png"
	else
		check "$tag $x,$y: the builder wrote a placement dump at all" 0
	fi
done

# ---------------------------------------------------------------------------
# 4. FIVE NAMED REFERENCES, by form id, at the positions the plugin gives.
#    tests/spells/cell_five_refs.txt, picked by the INDEPENDENT Python walk
#    (scratchpad/cellview1_20260919/pick_five.py), never out of a dump this
#    code produced -- a reference chosen from our own output would only prove
#    the code agrees with itself.
# ---------------------------------------------------------------------------
if [ -n "$RED" ]; then
	say "  (row 4 is not part of the red control: it checks positions, which the"
	say "   euler convention does not move)"
else
	python "$REPO/tests/spells/cell_five_refs.py" "$OUT/ww_cell_sanctuary.dump" 2>&1 | tee -a "$LOG"
	check "five named references picked back by form id, at the plugin's positions" \
		"$([ "${PIPESTATUS[0]}" -eq 0 ] && echo 1 || echo 0)"
fi

# ---------------------------------------------------------------------------
# 5. THE OVERLAYS, on Sanctuary. Distinct colours in the legend, per overlay:
#      type         one per record type drawn
#      has-lod      one per distinct MNAM slot mask
#      layer        one per XLYR layer, plus one for "no layer"
# ---------------------------------------------------------------------------
if [ -n "$RED" ]; then
	say "  (row 5 is not part of the red control)"
else
	for ov in type has-lod layer; do
		run_cell -20 7 1 "overlay_${ov}" WW_CELL_OVERLAY="$ov"
		notes="$OUT/ww_cell_overlay_${ov}.notes"
		buckets=$(grep -o "legend: [0-9]* buckets" "$notes" | head -1 | tr -dc '0-9')
		say "  overlay $ov: ${buckets:-0} buckets"
		check "overlay $ov produces at least two distinct colours" \
			"$([ "${buckets:-0}" -ge 2 ] && echo 1 || echo 0)"
	done
fi

# ---------------------------------------------------------------------------
# 6. NOTHING ELSE CHANGED. The neighbours this route shares code with.
# ---------------------------------------------------------------------------
say "  neighbours are run separately: native_open.sh, lodl_open.sh, render_shot.sh"

say ""
if [ "$fails" = "0" ]; then say "PASS"; else say "FAIL ($fails)"; fi
say "done"
exit "$fails"
