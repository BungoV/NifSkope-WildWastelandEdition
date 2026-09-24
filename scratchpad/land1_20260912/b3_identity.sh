#!/bin/sh
# LAND1 gate B3 -- THE BYTE-IDENTITY GATE for --incremental.
#
#   a dirty-chunk rebake must produce, byte for byte, what a full bake of the
#   same edited inputs produces.
#
# For each edit kind the script does three things and compares two of them:
#
#   base    a full bake of the UNEDITED inputs.  Writes the ledger.
#   full    a full bake of the EDITED inputs.            <-- the reference
#   incr    a COPY of base, then a bake with --incremental over it.
#
# PASS = incr == full, every file, every byte, the .lodb included.
#
# THE FLOOR, and why it is not optional.  `incr == full` is trivially true for
# an edit that reached nothing at all, so every arm also checks `base != full`:
# if the edit did not change the full bake, the identity comparison had nothing
# to be sensitive to and the arm is VACUOUS, not passing.  A gate that cannot
# fail on its input is not a gate.  The second floor is the census line: if the
# incremental run rebaked every chunk, identity is guaranteed by doing the full
# work and the arm proves nothing about the diff, so the script prints the
# dirty count beside each verdict and an arm that rebaked all 9 is called out.
#
#   sh b3_identity.sh <exe>  ->  logs/b3_<region>_<kind>.txt and logs/b3.txt
set -u
R="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${1:-$R/release/NifSkope.exe}"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
VANILLA="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
PY="C:/Users/bungo/AppData/Local/Programs/Python/Python39/python.exe"
W="$R/scratchpad/land1_20260912/out/b3"
L="$R/scratchpad/land1_20260912/logs"
SUM="$L/b3.txt"

if tasklist 2>/dev/null | grep -qi Fallout4.exe; then
	echo "REFUSED: Fallout4.exe is up; no exe may be launched." ; exit 3
fi
[ -x "$EXE" ] || { echo "no exe at $EXE"; exit 2; }

# Region A: 5x5 chunks of dim 4 around Sanctuary, land-guide OFF (the default).
# Region B: 5x5 chunks of dim 4 south-west of Lexington, land-guide ON with Part
# A's winner -- the brief asks for the winning switch as one arm, and an arm
# that is a whole region of it is a stronger answer than one chunk of it.
#
# FIVE by five and not three by three, and the reason is a measurement rather
# than taste. The widening marks a chunk dirty when a chunk WITHIN ONE CELL of
# it is, which for dim-4 chunks means all eight neighbours. In a 3x3 region an
# edit in the middle chunk therefore dirties all nine, and the gate -- while
# still true -- could not tell a working diff from a full bake wearing one.
# Measured: 3x3 gave "9 of 9 chunks dirty (1 inputs moved, 8 by neighbour)".
# At 5x5 the same edit dirties 9 of 25 and the subset is visible. On a real
# worldspace it is 9 of several hundred; the ratio here is the pessimistic end.
REGION_A="-24 16 -5 35"
REGION_B="-16 0 3 19"
GUIDE_A=""
GUIDE_B="--land-guide aspecthex:1.0 --land-guide-scale 256"

bake() {   # bake <dir> <esm> <region> <guide> [extra...]
	dir="$1"; esm="$2"; reg="$3"; guide="$4"; shift 4
	mkdir -p "$dir/obj" "$dir/tex"
	# shellcheck disable=SC2086
	"$EXE" -no-gui lodgen "$esm" --worldspace 3C \
		--terrain-region $reg --dim 4 \
		--out-dir "$dir/obj" --tex-dir "$dir/tex" --data-root "$DATA" \
		--cover --roads --road-detail 1 $guide "$@" \
		> "$dir/bake.log" 2>&1
	echo $?
}

echo "LAND1 gate B3 -- byte identity, dirty rebake == full bake" > "$SUM"
date +%H:%M >> "$SUM"
ls -l "$EXE" | sed 's/^/   /' >> "$SUM"
echo "" >> "$SUM"

run_region() {   # run_region <tag> <region> <guide> <kinds...>
	tag="$1"; reg="$2"; guide="$3"; shift 3
	base="$W/$tag/base"
	# THE LIVE PLUGIN. Every bake of this region names the SAME plugin path and
	# the variants are copied over it, because the switch digest covers the
	# argument vector and the plugin path is one of its arguments: baking the
	# edited copy from a second path would fire the "switches differ" refusal,
	# which is correct behaviour and the wrong experiment. A person editing
	# Fallout4.esm edits it where it lives, so this is also the real case.
	ACT="$W/esm/$tag.esm"
	if [ ! -f "$base/obj/Commonwealth.lodb" ]; then
		echo "[$tag] base full bake ..." | tee -a "$SUM"
		cp -f "$VANILLA" "$ACT"
		rc=$(bake "$base" "$ACT" "$reg" "$guide")
		grep -E "^chunk pass:|^ledger:" "$base/bake.log" | sed 's/^/    /' | tee -a "$SUM"
		[ "$rc" = 0 ] || { echo "    base bake rc=$rc -- see $base/bake.log" | tee -a "$SUM"; return 1; }
	else
		echo "[$tag] base cached" | tee -a "$SUM"
	fi

	for kind in "$@"; do
		echo "" | tee -a "$SUM"
		echo "[$tag/$kind] $(date +%H:%M)" | tee -a "$SUM"
		full="$W/$tag/$kind/full"
		incr="$W/$tag/$kind/incr"
		src="$VANILLA"
		extra=""
		mode=edit
		case "$kind" in
			land|refs|border) src="$W/esm/${tag}_${kind}.esm" ;;
			ltex)   extra="--resource $W/$tag/ltex_res" ;;
			lost)   mode=lost ;;
			null)   mode=null ;;
		esac
		cp -f "$src" "$ACT"

		rm -rf "$incr"
		mkdir -p "$incr"
		cp -r "$base/obj" "$incr/obj"
		cp -r "$base/tex" "$incr/tex"

		if [ "$kind" = lost ]; then
			# The "an output went missing" path: delete ONE .BTO from the copy.
			victim=$(ls "$incr/obj"/*.BTO | head -1)
			rm -f "$victim"
			echo "    deleted $(basename "$victim")" | tee -a "$SUM"
		fi

		if [ ! -f "$full/obj/Commonwealth.lodb" ]; then
			# shellcheck disable=SC2086
			rc=$(bake "$full" "$ACT" "$reg" "$guide" $extra)
			[ "$rc" = 0 ] || { echo "    FULL bake rc=$rc" | tee -a "$SUM"; continue; }
		fi
		# shellcheck disable=SC2086
		rc=$(bake "$incr" "$ACT" "$reg" "$guide" $extra --incremental "$incr/obj")
		grep -E "^incremental:" "$incr/bake.log" | sed 's/^/    /' | tee -a "$SUM"
		[ "$rc" = 0 ] || { echo "    INCR bake rc=$rc -- see $incr/bake.log" | tee -a "$SUM"; continue; }

		"$PY" "$R/scratchpad/land1_20260912/b3_compare.py" \
			"$base" "$full" "$incr" "$tag/$kind" "$mode" 2>&1 | tee -a "$SUM"
	done
}

run_region A "$REGION_A" "$GUIDE_A" null land refs border lost
run_region B "$REGION_B" "$GUIDE_B" null land refs

echo "" | tee -a "$SUM"
date +%H:%M | tee -a "$SUM"
