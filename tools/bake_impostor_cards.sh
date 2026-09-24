#!/bin/bash
#
# LODGEN impostor card baking driver (docs/LODGEN_PLAN.md rung 3).
#
# Asks the CLI which bases in a region lack far MNAM slots, then photographs
# each one's best LOD model through the WW_IMPOSTOR_BAKE hook (orthographic
# front+side, two-pass matte alpha) and files the cards by FORM ID — the
# naming `lodgen --impostors <dir>` consumes. Cards convert to BC1
# punch-through DDS lazily at generation time; ship the directory as
# Data/FO4CSLOD/Cards (lane LAYOUT1, 2026-09-16: one root for every
# FO4CS-target output, and the cards are per TREE, shared by every
# worldspace, so they sit at the root rather than under a worldspace).
#
# One GUI launch per model (the hook quits after the grab), serialized under
# the harness lock. ~3 s per card.
#
# NOTHING OF THIS REACHES AN EYE, AND NONE OF IT IS ON THE PRIMARY MONITOR
# (2026-09-09). bungo gave two rules: "Agent is launching nifskope on my main
# monitor, which is a no no", and "when these trees bake their impostors, the
# screen is flashing white and black, that's a view hazard for epileptics".
#
# Each view costs NINE full repaints of the window -- the extent matte of pass
# one (two, black then white), the card matte of pass two (two more) and five
# channel renders -- so a model at OCT=8 repaints 64*9 + 4 = 580 times a few
# milliseconds apart, two in every nine of them alternating black and white,
# model after model. That strobe is HOW THE ALPHA IS MEASURED and it still
# happens; what changed is that no part of it is on a monitor anyone looks at.
#
# Since 2026-09-09 every headless run (src/nifskope_ui.cpp, createWindow)
# un-maximises its window, places it on a NON-PRIMARY screen and shows it at
# WINDOW OPACITY 0. It is deliberately still ON a screen: an earlier attempt
# moved it off every screen, and then nothing rendered at all -- GLView is a
# QOpenGLWindow, an unexposed surface never gets a context, and the bake wrote
# empty card sets while exiting 0. The pixels are unchanged either way --
# grabFramebuffer() reads the window's back buffer, not the desktop -- and
# WW_WINDOW_VISIBLE=1 makes the window opaque again when one bake has to be
# watched (it does NOT move it to the primary). The gate is
# tests/spells/render_shot.sh sections 5 and 6, which measures the desktop's own
# luminance as well as what Windows was told.
#
# THE CAMERA IS ORTHOGRAPHIC, AND UNTIL 2026-09-10 IT WAS NOT (lane CARDORTHO).
#
# Every extent this bake records -- the card's half-width and half-height, the
# per-frame offsets, the front/side sidecar lines -- is a world measurement taken
# off viewport pixels through ONE units-per-pixel constant, and only an
# orthographic camera makes that arithmetic true. The headless renderer never had
# one: restoreUi() hard-codes a 60-degree perspective and nothing in the bake ever
# called setProjection, so every card in every library baked before 2026-09-10 was
# DRAWN through a perspective frustum while being MEASURED as if it were not
# (lane HOOKCAM measured it, 2026-09-09; MISTAKES.md). orthographicHalfHeight()
# returns Dist/Zoom whatever the projection is, which is why the read-back beside
# the fit never caught it.
#
# The bake now asserts the projection, and the sidecar NAMES it -- `projection
# ortho` or `projection persp`, read back off the live viewport -- and the word
# travels into the .lodm, per card and per card-array layer. A library whose
# sidecars carry no such line is the older, foreshortened vintage: RE-BAKE IT.
# WW_IMPOSTOR_PERSP=1 restores the old camera exactly and is the control the
# gates in tests/spells/lodgen_octahedral.sh must fail against.
#
# USAGE
#   bash tools/bake_impostor_cards.sh <esm> <x0> <y0> <x1> <y1> <outdir>
#   MAX=5 bash tools/bake_impostor_cards.sh ...   # cap for a smoke run
#   CANDIDATES=trees bash tools/bake_impostor_cards.sh ...   # TREES AND ONLY TREES, for cards from ring 0
#   (default: `missing` -- bases whose far MNAM slots are empty. Until 2026-09-09
#   `trees` meant tree OR missing, so it was a SUPERSET of the default and 14 of a
#   33-candidate Sanctuary run were shacks and rock cliffs. It is tree-only now.
#   `all` was RETIRED on 2026-09-11 and refuses by name; the panel's "Trees only"
#   row ON is CANDIDATES=trees and OFF is CANDIDATES=missing.)
#   OCT=8 TILE=512 bash tools/bake_impostor_cards.sh ...   # plus octahedral sheets
#   REF=0 bash tools/bake_impostor_cards.sh ...   # every base at TILE, no size ladder
#
# The bake writes PNG sheets at full size either way; HALF-RESOLUTION normal, mask
# and emissive sheets are a GENERATION option (`lodgen --card-half-aux`), applied
# when the PNGs are converted, so a library does not need re-photographing to try
# it. The base colour never divides: its alpha is the coverage.
#
#   HALF_AUX=1 bash tools/bake_impostor_cards.sh ...
#
# records that choice in the library, as `<outdir>/library.txt`, so whoever
# converts it later does not have to remember: the flag belongs to `lodgen`, not
# to this script, and this script is where the decision is actually made. OFF by
# default, which is the 2026-09-06 ruling (bungo: "Maybe make it a toggle").
# Measured across a two-layer array set the payload falls 3,584 -> 1,664 bytes,
# 46.4%. Since 2026-09-09 the GAP between two neighbouring silhouettes -- bungo's
# own quantity, gap(side) = max(2, side/16) rounded UP TO EVEN, with the margin on
# each side of a frame half of it -- is rounded even precisely so a halving lands
# those margins on whole texels, and the aux mip count comes down with the gap:
# auxMips = 1 + log2(min(gapX,gapY)/auxDiv), floored at one level on the 16- and
# 32-texel frames where the gap is already at its floor of 2.
#
# TILE is what the run's LARGEST base gets. Every other base comes down a halving
# ladder by its own world size - half the size, half the long side, three rungs at
# most - and then again by its silhouette's aspect, so a thin tree gets a
# rectangular frame. The reference is measured here, from the candidate listing's
# extent column, because the bake hook only ever sees one model.
#   RESOURCES="E:/mods/BostonNaturalSurroundings;E:/mods/TrueGrass" bash tools/... # the resource stack, LAST WINS
#   MO2=1 bash tools/bake_impostor_cards.sh ...   # the stack from the profile's plugins.txt
#
# RESOURCES/MO2 reach BOTH halves of the bake: the CLI that lists the candidates
# and resolves each model (through --probe, so a mesh inside a .ba2 bakes too),
# and the GUI launch that photographs it (WW_LODGEN_RESOURCES / WW_LODGEN_MO2,
# read in main.cpp, session only). Without them a mod's trees photograph with
# vanilla's textures, or not at all.

set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NS="$ROOT/release/NifSkope.exe"
ESM="${1:?esm}"
X0="${2:?x0}"; Y0="${3:?y0}"; X1="${4:?x1}"; Y1="${5:?y1}"
OUT="${6:?outdir}"
DATAROOT="${DATAROOT:-E:/Tools/Fallout 4/DataUnpacked/Data}"
MAX="${MAX:-100000}"
# The octahedral grid, in FRAMES PER SIDE, and only the three the panel offers.
# An unvalidated value used to reach the hook untouched, so a typo baked a grid
# nobody chose and the card arrays fragmented by sheet size without a word.
case "${OCT:-8}" in
	4|6|8) ;;
	*) echo "OCT must be 4, 6 or 8 (frames per side); got '${OCT}'" >&2; exit 2 ;;
esac
OCT="${OCT:-8}"
# The frame's long side in texels for the largest base, and only the FOUR the
# panel offers. Cost goes as OCT^2 * TILE^2, so an unvalidated tile is a quiet
# factor of four. 512 added 2026-09-11 on bungo's "Add it, why not": at OCT=8
# that is a 4096-texel sheet per channel, about 12 MB for the largest tree type.
# DEFAULT 256 since 2026-09-23 (bungo: tree cards "8x8 at 2k", lane DEFAULTS2):
# OCT 8 x TILE 256 = a 2048-texel sheet for the largest base. Was 128 (1024).
case "${TILE:-256}" in
	64|128|256|512) ;;
	*) echo "TILE must be 64, 128, 256 or 512 (texels on the long side of a frame); got '${TILE}'" >&2; exit 2 ;;
esac
TILE="${TILE:-256}"
# Which bases get a card baked, and only the two that are left: `all` was
# retired on 2026-09-11 (bungo: "I've only wanted trees for the impostors") and
# refuses here by name rather than quietly baking a library nobody asked for.
case "${CANDIDATES:-missing}" in
	trees|missing) ;;
	all) echo "CANDIDATES=all was retired on 2026-09-11: impostor cards are trees only. Use trees, or missing for any empty far slot." >&2; exit 2 ;;
	*) echo "CANDIDATES must be trees or missing; got '${CANDIDATES}'" >&2; exit 2 ;;
esac
CANDIDATES="${CANDIDATES:-missing}"
RESOURCES="${RESOURCES:-}"
MO2="${MO2:-}"

# the stack as CLI arguments, in the order it was given (the last one wins)
RESARGS=()
if [ -n "$RESOURCES" ]; then
	IFS=';' read -r -a _res <<< "$RESOURCES"
	for r in "${_res[@]}"; do
		[ -n "$r" ] && RESARGS+=( --resource "$r" )
	done
fi
[ "$MO2" = "1" ] && RESARGS+=( --mo2 )

. "$ROOT/tests/spells/_harness.sh"
mkdir -p "$OUT"
W="$(mktemp -d)"
trap 'rm -rf "$W"; rmdir "$ROOT/.harness.lock" 2>/dev/null' EXIT

mkdir "$ROOT/.harness.lock" 2>/dev/null || { echo "harness lock busy"; exit 1; }

# The candidate list ONCE, to a file, because it is read twice: for the largest
# base's extent and then for the bake itself. Piping it into the loop would also
# have put the loop in a subshell.
# tr strips the CLI's CRLF: a \r glued to the model path fails every -f test
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region "$X0" "$Y0" "$X1" "$Y1" \
	"${RESARGS[@]+"${RESARGS[@]}"}" \
	--list-impostor-candidates --candidates "$CANDIDATES" 2>/dev/null | tr -d '\r' > "$W/cands.txt"

# THE SIZE LADDER'S REFERENCE: the largest extent in the list, in world units.
# REF=0 turns the ladder off and bakes every base at TILE. A list whose extents
# are all zero (nothing loaded) does the same, rather than dividing by nothing.
if [ "${REF:-}" = "0" ]; then
	REF=0
	echo "size ladder OFF: every base at ${TILE} px"
else
	REF="$(awk '{ if ($2+0 > m) m = $2+0 } END { printf "%.1f", m }' "$W/cands.txt")"
	[ "$(awk -v r="$REF" 'BEGIN{print (r+0 > 0) ? 1 : 0}')" = "1" ] || REF=0
	if [ "$REF" = "0" ]; then
		echo "size ladder OFF: no candidate reported an extent"
	else
		echo "size ladder: largest base ${REF} units gets ${TILE} px; half that gets $((TILE/2)), a quarter $((TILE/4))"
	fi
fi

# the half-aux choice, recorded beside the library it applies to
{
	echo "oct ${OCT}"
	echo "tile ${TILE}"
	echo "ref ${REF}"
	echo "half_aux ${HALF_AUX:-0}"
	echo "candidates $CANDIDATES"
} > "$OUT/library.txt"
if [ "${HALF_AUX:-0}" = "1" ]; then
	echo "half-aux ON: convert this library with 'lodgen --card-half-aux' (normal, mask and emissive at half of each side)"
else
	echo "half-aux off (the 2026-09-06 default): all four sheets convert at full size"
fi

n=0
while read -r formid extent model; do
	[ -n "$formid" ] || continue
	[ "$n" -ge "$MAX" ] && break
	n=$((n+1))
	[ -f "$OUT/${formid}.txt" ] && { echo "[$n] $formid cached"; continue; }
	mesh="$DATAROOT/meshes/${model//\\//}"
	if [ ! -f "$mesh" ] && [ ${#RESARGS[@]} -gt 0 ]; then
		# not loose under DATAROOT: pull it out of the stack, archive or not,
		# because the GUI can only be handed a real file on disk. It keeps its
		# own name - the hook names every output after the file it loaded.
		rm -rf "$W/ex"; mkdir -p "$W/ex"
		exname="$(basename "${model//\\//}")"
		if "$NS" -no-gui lodgen "${RESARGS[@]}" --probe "meshes/${model//\\//}" \
			--probe-out "$(winpath "$W/ex")/$exname" >/dev/null 2>&1; then
			mesh="$W/ex/$exname"
		fi
	fi
	if [ ! -f "$mesh" ]; then
		echo "[$n] $formid MISSING $mesh"
		continue
	fi
	rm -rf "$W/bake"; mkdir "$W/bake"
	# OCT=N adds the octahedral sheets (N x N views, TILE px each) for FO4CS.
	# The GUI gets the same stack, so the card photographs the mod's textures.
	WW_IMPOSTOR_BAKE="$(winpath "$W/bake")" WW_IMPOSTOR_OCT="${OCT:-}" WW_IMPOSTOR_TILE="$TILE" \
		WW_IMPOSTOR_REF="$REF" \
		WW_LODGEN_RESOURCES="$RESOURCES" WW_LODGEN_MO2="$MO2" \
		"$NS" "$(winpath "$mesh")" --port 45917 >/dev/null 2>&1
	base="$(basename "${model//\\//}" .nif | tr 'A-Z' 'a-z')"
	if [ -f "$W/bake/${base}_front.png" ]; then
		mv "$W/bake/${base}_front.png" "$OUT/${formid}_front.png"
		mv "$W/bake/${base}_side.png" "$OUT/${formid}_side.png" 2>/dev/null
		# the four sheets of docs/LODGEN_IMPOSTOR_SPEC.md; the third and the
		# emissive (_g legacy, _e pbr) are named by the set's family
		for s in albedo normal gsaos rmaos g e; do
			[ -f "$W/bake/${base}_oct_$s.png" ] && mv "$W/bake/${base}_oct_$s.png" "$OUT/${formid}_oct_$s.png"
		done
		mv "$W/bake/${base}.txt" "$OUT/${formid}.txt"
		echo "[$n] $formid baked ($model)"
	else
		echo "[$n] $formid FAILED ($model)"
	fi
done < "$W/cands.txt"
echo done
