#!/usr/bin/env bash
# INCR1 step 1(a) -- B3's arms, AS THEY STAND, on the STOCK target, measured on
# the exe at launch BEFORE any source change.
#
# Region: 3x3 chunks of dim 4, `-24 16 -13 27` (cells x -24..-13, y 16..27), the
# nine chunks (-24,-20,-16) x (16,20,24).  B3 itself used 5x5 and said why: the
# widening marks a chunk dirty when any chunk within ONE CELL of it is, which
# for dim-4 chunks is all eight neighbours, so an edit in the MIDDLE chunk of a
# 3x3 dirties all nine and the arm cannot tell a working diff from a full bake
# wearing one.  The brief asks for nine chunks, so every edit here is made in a
# CORNER chunk (-24,16), whose neighbourhood inside the region is three chunks:
# the dirty set is 4 of 9 and is a strict subset.
#
#   bash s1_b3_stock.sh [<exe>]   ->  s1_b3_stock.txt beside this script
set -u
R="$(cd "$(dirname "$0")/../.." && pwd)"
D="$R/scratchpad/incr1_20260917"
EXE="${EXE:-$R/release/NifSkope.exe}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
VANILLA="${VANILLA:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
REGION="${REGION:--24 16 -13 27}"
W="${OUT:-$D/work/s1}"
SUM="$D/s1_b3_stock.txt"
EDIT="$R/scratchpad/land1_20260912/b_esmedit.py"
PICK="$R/scratchpad/land1_20260912/b_pickref.py"

if tasklist 2>/dev/null | grep -qi "Fallout4.exe"; then
	echo "REFUSED: Fallout4.exe is up"; exit 3
fi
[ -x "$EXE" ] || { echo "no exe at $EXE"; exit 2; }

mkdir -p "$W/esm"
ACT="$W/esm/act.esm"

bake () {   # bake <dir> <plugin> [extra...]
	local dir="$1" esm="$2"; shift 2
	mkdir -p "$dir/obj" "$dir/tex"
	local da
	da="$(cd "$dir" && { pwd -W 2>/dev/null || pwd; })"
	# shellcheck disable=SC2086
	"$EXE" -no-gui lodgen "$esm" --worldspace 3C \
		--terrain-region $REGION --dim 4 \
		--out-dir "$da/obj" --tex-dir "$da/tex" --data-root "$DATA" \
		--cover --roads --road-detail 1 "$@" \
		> "$dir/bake.log" 2>&1
	echo $?
}

{
echo "INCR1 step 1(a) -- B3's arms on the STOCK target, exe at launch"
date
ls -l "$EXE" | sed 's/^/   /'
echo "region $REGION dim 4 (9 chunks), plugin copied to $ACT"
echo
} > "$SUM"

say () { echo "$*" | tee -a "$SUM"; }

# ---- the base bake: unedited inputs, writes the record -----------------------
cp -f "$VANILLA" "$ACT"
if [ ! -f "$W/base/obj/Commonwealth.lodb" ]; then
	say "[base] full bake of the unedited plugin ..."
	t0=$(date +%s)
	rc=$(bake "$W/base" "$ACT")
	t1=$(date +%s)
	say "    rc=$rc  $((t1-t0)) s"
	[ "$rc" = 0 ] || exit 1
else
	say "[base] cached"
fi
say ""

# the reference the `refs` arm moves: one the bake DEMONSTRABLY DRAWS, picked
# out of the base bake's own manifest (B3.1 -- an arm whose only witness is the
# artefact under test is not a witness).
PICKED="$("$PY" "$PICK" "$W/base/obj/Commonwealth.4.-24.16.BTO.manifest.txt" "$ACT" -24 16 -21 19 2>/dev/null | tail -1)"
say "[pick] drawn reference in chunk (-24,16): ${PICKED:-NONE}"
PCX="$(echo "$PICKED" | awk '{print $1}')"
PCY="$(echo "$PICKED" | awk '{print $2}')"
PFORM="$(echo "$PICKED" | awk '{print $3}')"

arm () {   # arm <name> <mode>
	local name="$1" mode="$2"
	say ""
	say "== arm $name ($(date +%H:%M:%S)) =="
	local full="$W/$name/full" incr="$W/$name/incr"
	rm -rf "$W/$name"
	mkdir -p "$incr"
	cp -r "$W/base/obj" "$incr/obj"
	cp -r "$W/base/tex" "$incr/tex"

	cp -f "$VANILLA" "$ACT"
	case "$mode" in
		land)
			"$PY" "$EDIT" height "$VANILLA" "$ACT" -23 17 8 2>&1 | sed 's/^/    /' | tee -a "$SUM" ;;
		refs)
			if [ -z "${PFORM:-}" ]; then say "    SKIP: no drawn reference found"; return 0; fi
			"$PY" "$EDIT" moveid "$VANILLA" "$ACT" "$PCX" "$PCY" 512 "$PFORM" 2>&1 | sed 's/^/    /' | tee -a "$SUM" ;;
		border)
			"$PY" "$EDIT" height "$VANILLA" "$ACT" -24 16 8 2>&1 | sed 's/^/    /' | tee -a "$SUM" ;;
		lost)
			rm -f "$incr/obj/Commonwealth.4.-24.16.BTO"
			say "    deleted Commonwealth.4.-24.16.BTO from the incremental tree" ;;
		null)
			say "    no edit (control)" ;;
	esac

	local t0 t1 t2 t3
	t0=$(date +%s)
	rc=$(bake "$full" "$ACT")
	t1=$(date +%s)
	[ "$rc" = 0 ] || { say "    FULL bake rc=$rc"; return 1; }
	local ia
	ia="$(cd "$incr" && { pwd -W 2>/dev/null || pwd; })"
	t2=$(date +%s)
	rc=$(bake "$incr" "$ACT" --incremental "$ia/obj")
	t3=$(date +%s)
	grep -E "^incremental:" "$incr/bake.log" | sed 's/^/    /' | tee -a "$SUM"
	[ "$rc" = 0 ] || { say "    INCR bake rc=$rc -- see $incr/bake.log"; return 1; }
	say "    full $((t1-t0)) s, incremental $((t3-t2)) s"
	if [ "$mode" = null ] || [ "$mode" = lost ]; then
		"$PY" "$D/incr_compare.py" "$full" "$incr" --tag "$name" 2>&1 | sed 's/$//' | tee -a "$SUM"
	else
		"$PY" "$D/incr_compare.py" "$full" "$incr" --base "$W/base" --tag "$name" 2>&1 | tee -a "$SUM"
	fi
}

arm null null
arm land land
arm refs refs
arm border border
arm lost lost

say ""
date | tee -a "$SUM"
