#!/bin/bash
#
# Does the merge carry EVERYTHING, or only the geometry?
#
# WHY THIS EXISTS
#
# "The shapes are there" is the easy half. A NIF's animation lives in blocks that
# nothing in the viewport points at directly -- NiControllerManager,
# NiControllerSequence, the interpolator/data pairs behind them, NiTextKeyExtraData
# for the annotations, NiDefaultAVObjectPalette for name lookup -- and a particle
# system is a dozen NiPSys* modifiers hanging off one NiParticleSystem. Any of
# those quietly not making it across gives a merged file that LOOKS right, loads
# fine, and does nothing when you press play.
#
# So this does not check shapes. It takes a block-type histogram of the target and
# every donor, merges, and requires the arithmetic to hold.
#
# THE ONE TYPE THAT IS ALLOWED TO SHRINK
#
# NiNode, on purpose. De-duplicating bones by name is the whole reason merged
# armour shares one skeleton instead of carrying six copies of it; a merge of two
# effect files onto the FO4 power-armour skeleton comes out 2 nodes short of the
# sum, and those 2 are shared bones, not losses. Every other type must add up
# exactly -- there is no legitimate reason to lose an interpolator.
#
# Counts are not quite enough for sequences: a NiControllerSequence that survived
# but got renamed is broken, because sequences are addressed by name. So the
# sequence NAMES from every donor are checked to still be present as well.
#
# USAGE
#   bash tests/merge/carries_everything.sh
# Needs release/NifSkope.exe, the FO4 corpus and the X01Tesla mod.

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
SK="${SK:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/actors/powerarmor/CharacterAssets/skeleton.nif}"
X="${X:-/e/Projects/Fallout 4 Mods/mods/X01Tesla/meshes/actors/powerarmor/x01}"
CA="${CA:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/actors/powerarmor/CharacterAssets}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass=0; fail=0
ok()  { pass=$((pass+1)); }
bad() { fail=$((fail+1)); echo "  FAIL: $*"; }

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SK" ]  || { echo "no skeleton at $SK"; exit 2; }

hist() {
	"$EXE" -no-gui list "$1" 2>/dev/null | sed 's/^\[[0-9]*\] //' \
		| awk '{print $1}' | sort | uniq -c | awk '{print $2, $1}' | sort
}
seqnames() {
	"$EXE" -no-gui list "$1" 2>/dev/null | grep "NiControllerSequence" \
		| sed "s/.*'\(.*\)'.*/\1/" | sort -u
}

# One case: a target plus one or more donors, all of which must survive whole.
run_case() {
	local label="$1"; shift
	local target="$1"; shift
	local donors=( "$@" )

	echo "$label"
	local add=()
	local d
	for d in "${donors[@]}"; do
		[ -f "$d" ] || { echo "  (missing $d, skipping case)"; return; }
		add+=( --add "$d" )
	done
	"$EXE" -no-gui merge "$target" "${add[@]}" -o "$TMP/m.nif" > "$TMP/merge.log" 2>&1 \
		|| { bad "$label: merge failed"; cat "$TMP/merge.log"; return; }

	# Self-proof. WW_BREAK=1 throws the merge away and checks the TARGET instead,
	# which is exactly "the donors did not make it". Every check below must then
	# fail; if they pass, the arithmetic is not measuring anything and the green run
	# above meant nothing. Run it once after touching this file.
	if [ "${WW_BREAK:-0}" = "1" ]; then
		cp -f "$target" "$TMP/m.nif"
		echo "  (WW_BREAK: measuring the bare target -- everything below SHOULD fail)"
	fi

	# expected count per type = target + every donor
	: > "$TMP/all.txt"
	hist "$target" >> "$TMP/all.txt"
	for d in "${donors[@]}"; do hist "$d" >> "$TMP/all.txt"; done
	awk '{s[$1]+=$2} END {for (t in s) print t, s[t]}' "$TMP/all.txt" | sort > "$TMP/exp.txt"
	hist "$TMP/m.nif" > "$TMP/got.txt"

	local lost=0 gained=0 nodedelta=0
	while read -r t n; do
		local m
		m="$(awk -v k="$t" '$1==k{print $2}' "$TMP/got.txt")"
		m="${m:-0}"
		if [ "$t" = "NiNode" ]; then
			nodedelta=$(( n - m ))
			continue
		fi
		if [ "$m" -lt "$n" ]; then
			lost=$(( lost + 1 ))
			echo "    LOST $(( n - m )) of $n  $t"
		elif [ "$m" -gt "$n" ]; then
			gained=$(( gained + 1 ))
			echo "    EXTRA $(( m - n ))      $t"
		fi
	done < "$TMP/exp.txt"

	if [ "$lost" = "0" ]; then ok; echo "  nothing lost: every non-NiNode type adds up"
	else bad "$label: $lost block type(s) lost in the merge"; fi
	# Extra blocks are as suspicious as missing ones -- a donor spliced twice would
	# show up here and nowhere else.
	if [ "$gained" = "0" ]; then ok
	else bad "$label: $gained block type(s) came out with MORE than the sum"; fi
	echo "  NiNode shrank by $nodedelta (shared bones; the only type allowed to)"
	if [ "$nodedelta" -ge 0 ]; then ok
	else bad "$label: NiNode count GREW -- bones were duplicated, not shared"; fi

	# sequence names, not just counts
	local missing=0 s
	for d in "$target" "${donors[@]}"; do
		while read -r s; do
			[ -n "$s" ] || continue
			if ! seqnames "$TMP/m.nif" | grep -qxF "$s"; then
				missing=$(( missing + 1 ))
				echo "    sequence '$s' from $(basename "$d") is gone"
			fi
		done < <(seqnames "$d")
	done
	if [ "$missing" = "0" ]; then ok; echo "  every sequence name survived"
	else bad "$label: $missing sequence name(s) lost"; fi
}

# Effects: controller managers, sequences, particle modifiers, procedural lightning
run_case "two Tesla effect files onto the skeleton" "$SK" \
	"$X/X01_ArmLeft_Tesla_VFX.nif" "$X/X01_Torso_Tesla_VFX.nif"

# Armour plus its hardware layer: skinned geometry and shader controllers
run_case "armour + Tesla hardware onto the skeleton" "$SK" \
	"$X/X01_Torso.nif" "$X/X01_Torso_Tesla.nif" "$X/X01_Helmet.nif"

# The whole rig, the case the loading-screen pipeline actually runs
if [ -f "$CA/Frame.nif" ]; then
	run_case "the full rig: armour + hardware + effects + frame" "$SK" \
		"$X/X01_Torso.nif" "$X/X01_Torso_Tesla.nif" "$X/X01_Torso_Tesla_VFX.nif" \
		"$X/X01_Helmet.nif" "$X/X01_Helmet_Tesla.nif" "$X/X01_Helmet_Tesla_VFX.nif" \
		"$X/X01_ArmLeft.nif" "$X/X01_ArmLeft_Tesla_VFX.nif" \
		"$X/X01_ArmRight.nif" "$X/X01_ArmRight_Tesla_VFX.nif" \
		"$X/X01_LegLeft.nif" "$X/X01_LegLeft_Tesla_VFX.nif" \
		"$X/X01_LegRight.nif" "$X/X01_LegRight_Tesla_VFX.nif" \
		"$CA/Frame.nif"
fi

echo
echo "checks passed: $pass   failed: $fail"
[ "$fail" -eq 0 ]
