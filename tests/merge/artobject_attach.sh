#!/bin/bash
#
# Merging ArtObjects (effect NIFs) onto a rig: do the effects land on the limb
# they belong to, and do two effects stay separate?
#
# WHY THIS EXISTS
#
# Two bugs found while building an actual loading screen, both of which produced
# a file that loaded fine and looked plausible until you measured it.
#
# 1. NAME FUSION. De-duplicating NiNodes by name is what lets merged armour share
#    one skeleton. Applied to effect files it is destructive: they are authored
#    from a shared template and reuse internal names wholesale.
#    X01_ArmLeft_Tesla_VFX and X01_ArmRight_Tesla_VFX have 20 nodes each and
#    **15 identical names** (LightningBolt_01, BoltGeo_01, LightningArcs_VFX...).
#    Fusing them hung the right arm's effects off the left arm's nodes and the
#    geometry landed 150 units away.
#
# 2. AUTHORING SPACE. A top-level branch named `NamedAttach<NodeName>` says which
#    skeleton node it belongs to. Those branches are authored in ACTOR space --
#    NamedAttachR_Pauldron sits at (20.96, -7.33, 126.14), a shoulder position on
#    the actor, not an offset from the shoulder. Parenting one straight under its
#    bone applies the bone's world transform again, and since arm bones carry a
#    large rotation the piece is flung rather than merely doubled.
#
#    But it is NOT universal: a file whose AttachT already names a node is
#    authored node-local (X01_LegLeft_Tesla_VFX's tops are small offsets from the
#    calf). Rebasing those breaks them -- it drove the helmet pulse from the head
#    down to Z = 5, at the ankles. So AttachT naming a node is the signal, and
#    only files that name nothing get rebased.
#
# THE INVARIANTS
#
#   1. Two effect files with colliding internal node names produce TWO nodes of
#      each colliding name, not one shared one.
#   2. Every effect shape lands near the body: |X| <= 60 for a figure whose own
#      shapes reach about +-35. A flung piece is the failure this catches.
#   3. A file whose AttachT names a node is left in its own space -- the helmet
#      effect stays at head height, not at the ankles.
#
# USAGE
#   bash tests/merge/artobject_attach.sh
# Needs release/NifSkope.exe, the FO4 corpus and the X01Tesla mod.

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
SK="${SK:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/actors/powerarmor/CharacterAssets/skeleton.nif}"
X="${X:-/e/Projects/Fallout 4 Mods/mods/X01Tesla/meshes/actors/powerarmor/x01}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass=0; fail=0
ok()  { pass=$((pass+1)); }
bad() { fail=$((fail+1)); echo "  FAIL: $*"; }

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SK" ]  || { echo "no skeleton at $SK"; exit 2; }

# Freeze the effects first, as the real pipeline does -- a live NiControllerManager
# is not what gets merged into a loading screen.
for limb in Helmet Torso ArmLeft ArmRight; do
	"$EXE" -no-gui freeze "$X/X01_${limb}_Tesla_VFX.nif" --sequence autoLoop --time 2.0 \
		-o "$TMP/${limb}_fx.nif" > /dev/null 2>&1
	[ -s "$TMP/${limb}_fx.nif" ] || { echo "freeze failed for $limb"; exit 2; }
done

echo "merging armour + both arm effects + helmet effect"
"$EXE" -no-gui merge "$SK" \
	--add "$X/X01_Helmet.nif"  --add "$X/X01_Torso.nif" \
	--add "$X/X01_ArmLeft.nif" --add "$X/X01_ArmRight.nif" \
	--add "$TMP/Helmet_fx.nif"  --add "$TMP/Torso_fx.nif" \
	--add "$TMP/ArmLeft_fx.nif" --add "$TMP/ArmRight_fx.nif" \
	-o "$TMP/rig.nif" > "$TMP/merge.log" 2>&1 || { echo "merge failed"; cat "$TMP/merge.log"; exit 2; }
sed -n 's/^  branches attached by name to/  attached by name ->/p' "$TMP/merge.log"

# --- 1. colliding effect node names stay separate ---------------------------
for name in LightningBolt_01 BoltGeo_01 LightningArcs_VFX; do
	n="$("$EXE" -no-gui list "$TMP/rig.nif" 2>/dev/null | grep -c "'$name'")"
	# One per arm effect. The helmet effect has no lightning nodes.
	if [ "$n" -ge 2 ]; then ok
	else bad "only $n node(s) named '$name' -- the two arm effects were fused"; fi
done

# --- 2 & 3. bake, then measure where every effect shape ended up ------------
"$EXE" -no-gui loading-screen "$TMP/rig.nif" -o "$TMP/screen.nif" > /dev/null 2>&1
[ -s "$TMP/screen.nif" ] || { bad "loading-screen produced nothing"; }

if [ -s "$TMP/screen.nif" ]; then
	blocks="$("$EXE" -no-gui info "$TMP/screen.nif" 2>/dev/null | sed -n 's/^blocks *\([0-9]*\)/\1/p')"
	: > "$TMP/pos.txt"
	for b in $(seq 0 $(( blocks - 1 )) ); do
		row="$("$EXE" -no-gui list "$TMP/screen.nif" 2>/dev/null | grep "^\[$b\]")"
		case "$row" in *TriShape*) ;; *) continue ;; esac
		v="$("$EXE" -no-gui get "$TMP/screen.nif" -b "$b" -f "Translation" 2>/dev/null)"
		echo "$(echo "$row" | sed "s/.*'\(.*\)'/\1/") $v" >> "$TMP/pos.txt"
	done

	# 2. nothing flung
	flung="$(awk '{x=$3; if(x<0)x=-x; if(x>60) print "    " $0}' "$TMP/pos.txt")"
	if [ -z "$flung" ]; then ok; echo "  no shape beyond |X| = 60"
	else bad "shape(s) flung away from the body:"; echo "$flung"; fi

	# 3b. the torso fan sits on the CHEST, not on the ground. This is the case the
	# actor-space rebase got backwards: NamedAttachTank_Armor has an identity
	# translation (its content is already relative to the chest bone) while the
	# arm file's NamedAttachR_Pauldron carries an actor-space position. Rebasing
	# the torso subtracted the bone height and dropped the fan from Z 128 to 24.
	fz="$(awk '/Torso_Tesla_Fan:0/ {print $7; exit}' "$TMP/pos.txt")"
	if [ -n "$fz" ]; then
		if [ "$(echo "$fz" | awk '{print ($1 > 100) ? 1 : 0}')" = "1" ]; then
			ok; echo "  torso fan at Z = $fz (chest height)"
		else bad "torso fan at Z = $fz -- a node-local branch was wrongly rebased"; fi
	fi

	# 3. the helmet effect stayed in its own (node-local) space
	hz="$(awk '/Helmet.*Pulse/ {print $7; exit}' "$TMP/pos.txt")"
	if [ -n "$hz" ]; then
		verdict="$(echo "$hz" | awk '{print ($1 > 120) ? "OK" : "LOW"}')"
		if [ "$verdict" = "OK" ]; then ok; echo "  helmet effect at Z = $hz (head height)"
		else bad "helmet effect at Z = $hz -- an AttachT-named file was wrongly rebased"; fi
	else
		echo "  (no helmet pulse shape to check)"
	fi
fi

echo
echo "checks passed: $pass   failed: $fail"
[ "$fail" -eq 0 ]
