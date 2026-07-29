#!/bin/bash
#
# Snapshotting particles and procedural lightning into static geometry, then
# converting the result: does the effect survive all the way to a loading screen?
#
# WHY THIS EXISTS
#
# Neither a particle sprite nor a lightning bolt has any geometry in the NIF. A
# sprite is ONE POINT that particles.geom expands into a camera-facing quad; a
# bolt is a polyline regenerate() invents from the clock. Both exist only in the
# frame being drawn, which is why 0 of the 173 vanilla files under
# meshes/LoadScreenArt contain NiParticleSystem, NiPSys* or
# BSProceduralLightningController -- and why converting used to just drop them.
#
# Keeping the look means capturing one instant and writing it out as an ordinary
# BSTriShape. Two halves, two different ways to fail:
#
#   CAPTURE runs in the GUI, because it reads the live scene. It cannot be done
#   headlessly at all: with animation off no controller updates and there is
#   nothing to capture -- which is exactly how this suite went from green to
#   "captured 0" once, with no code change, because a GUI session had left
#   GLView/Enable Animations false in the registry.
#
#   EMISSION constructs FO4 vertex arrays, where a write can look successful and
#   be wrong three ways: the desc says half precision while the writer sets
#   Vector3 (nif.xml names both variants "Vertex", so the set silently does
#   nothing), the desc and Data Size disagree so the file will not re-read, or a
#   conditional array is never sized. The GUI harness catches all three with one
#   invariant -- save, re-read, and check translation + vertex reproduces the
#   captured world position.
#
# THE INVARIANTS HERE
#
#   1. The GUI harness passes: capture and emission both work, and the geometry
#      round-trips to where it was captured.
#   2. The baked file has NO emitter blocks left. Otherwise a converted screen
#      would carry both the snapshot and the emitter it replaces.
#   3. The baked shapes SURVIVE the loading-screen conversion with real triangles
#      still in them. They are ordinary geometry now, so the converter must treat
#      them as such rather than recognising them as effects and dropping them.
#
# USAGE
#   bash tests/loadingscreen/effect_bake.sh
# Needs release/NifSkope.exe and the X01Tesla mod. Opens a window briefly.

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
X="${X:-/e/Projects/Fallout 4 Mods/mods/X01Tesla/meshes/actors/powerarmor/x01}"
FX="${FX:-$X/X01_Torso_Tesla_VFX.nif}"
TIME="${TIME:-2.5}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass=0; fail=0
ok()  { pass=$((pass+1)); }
bad() { fail=$((fail+1)); echo "  FAIL: $*"; }

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$FX" ]  || { echo "no effect file at $FX"; exit 2; }

LOG="$ROOT/release/ww_effectbake_test.log"
rm -f "$LOG"

# --- 1. capture + emission, in the GUI because that is the only place the
# effects exist ---------------------------------------------------------------
echo "baking $(basename "$FX") at t=$TIME"
WW_EFFECTBAKE_TEST="$TMP/baked.nif" WW_EFFECTBAKE_TIME="$TIME" WW_WINDOW_AT="1960,20" \
	"$EXE" --port 41890 "$FX" > /dev/null 2>&1

if [ ! -f "$LOG" ]; then
	bad "the GUI harness wrote no log -- it did not run"
else
	sed -n 's/^\(captured\|wrote\|worst\) /  &/p' "$LOG"
	if grep -q '^PASS' "$LOG"; then ok
	else
		bad "the GUI harness failed:"
		grep '^  FAIL' "$LOG" | sed 's/^/  /'
	fi
fi

[ -s "$TMP/baked.nif" ] || { bad "no baked file was produced"; echo; echo "checks passed: $pass   failed: $fail"; exit 1; }

# --- 2. nothing live left behind --------------------------------------------
left="$("$EXE" -no-gui list "$TMP/baked.nif" 2>/dev/null \
	| grep -cE "NiParticleSystem|NiPSys|BSPSys|BSProceduralLightningController")"
if [ "$left" = "0" ]; then ok; echo "  no emitter blocks remain in the baked file"
else bad "$left emitter block(s) survived the bake"; fi

# The bake must have produced the shapes the emitters were replaced by.
baked_shapes="$("$EXE" -no-gui list "$TMP/baked.nif" 2>/dev/null \
	| grep -cE "BSTriShape '(BoltGeo|LightningArcs)")"
if [ "$baked_shapes" -ge 2 ]; then ok; echo "  $baked_shapes baked effect shape(s) present"
else bad "only $baked_shapes baked effect shape(s) -- expected at least 2"; fi

# --- 3. they survive the conversion -----------------------------------------
"$EXE" -no-gui loading-screen "$TMP/baked.nif" -o "$TMP/screen.nif" > "$TMP/conv.log" 2>&1
if [ ! -s "$TMP/screen.nif" ]; then
	bad "loading-screen produced nothing"; cat "$TMP/conv.log"
else
	ok
	blocks="$("$EXE" -no-gui info "$TMP/screen.nif" 2>/dev/null | sed -n 's/^blocks *\([0-9]*\)/\1/p')"
	kept=0; empty=0
	for b in $(seq 0 $(( blocks - 1 )) ); do
		row="$("$EXE" -no-gui list "$TMP/screen.nif" 2>/dev/null | grep "^\[$b\]")"
		case "$row" in *"BSTriShape 'BoltGeo"*|*"BSTriShape 'LightningArcs"*) ;; *) continue ;; esac
		kept=$((kept+1))
		# A shape that is present but empty is the failure that still counts as
		# "survived": the converter rebuilds vertex arrays, so it can hollow one.
		nt="$("$EXE" -no-gui get "$TMP/screen.nif" -b "$b" -f "Num Triangles" 2>/dev/null | tr -dc '0-9')"
		if [ -z "$nt" ] || [ "$nt" -lt 1 ]; then
			empty=$((empty+1))
			echo "    '$row' has $nt triangles"
		fi
	done
	if [ "$kept" -ge 2 ]; then ok; echo "  $kept baked effect shape(s) survived the conversion"
	else bad "only $kept baked effect shape(s) survived the conversion"; fi
	if [ "$empty" = "0" ]; then ok; echo "  every surviving effect shape still has triangles"
	else bad "$empty surviving effect shape(s) were hollowed out"; fi
fi

# --- 4. the name-collision case ---------------------------------------------
#
# One file has effects with distinct names, so it cannot catch the thing that
# actually went wrong. Both ARM effect files together can: they are authored from
# the same template and share 15 of 20 node names, including BoltGeo_01 and
# LightningArcs_VFX. Writing several shapes under one name made the GUI harness
# compare a helmet arc against a leg arc and report a 115-unit round-trip error
# that was not real -- so uniqueness is load-bearing, not tidiness.
SK="${SK:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/actors/powerarmor/CharacterAssets/skeleton.nif}"
if [ -f "$SK" ] && [ -f "$X/X01_ArmLeft_Tesla_VFX.nif" ] && [ -f "$X/X01_ArmRight_Tesla_VFX.nif" ]; then
	echo "both arm effects on one rig (the colliding-name case)"
	"$EXE" -no-gui merge "$SK" \
		--add "$X/X01_ArmLeft_Tesla_VFX.nif" --add "$X/X01_ArmRight_Tesla_VFX.nif" \
		-o "$TMP/arms.nif" > "$TMP/arms.log" 2>&1 || { bad "merge failed"; cat "$TMP/arms.log"; }

	if [ -s "$TMP/arms.nif" ]; then
		rm -f "$LOG"
		WW_EFFECTBAKE_TEST="$TMP/arms_baked.nif" WW_EFFECTBAKE_TIME="$TIME" WW_WINDOW_AT="1960,20" \
			"$EXE" --port 41891 "$TMP/arms.nif" > /dev/null 2>&1
		if [ ! -f "$LOG" ]; then
			bad "the GUI harness wrote no log for the arm rig"
		else
			sed -n 's/^\(captured\|wrote\|worst\) /  &/p' "$LOG"
			if grep -q '^PASS' "$LOG"; then ok
			else
				bad "the colliding-name case failed:"
				grep '^  FAIL' "$LOG" | sed 's/^/  /'
			fi
		fi

		# Independently of the harness: no two BAKED shapes share a name, and none
		# collides with a shape that was already there.
		#
		# Which are baked is decided by DIFFING against the pre-bake file, not by
		# guessing from the names. The first version of this check counted every
		# BSTriShape and failed on shapes the bake never touched: the arm files
		# carry their own duplicate Bolt_01/Bolt_02/Bolt_03 (0 vertices each, the
		# empty shapes the controllers targeted) and both name their pulse mesh
		# X01_ArmRight_Tesla_Pulse -- a naming slip in the source assets. That is a
		# property of the INPUT, and asserting it here measured nothing.
		if [ -s "$TMP/arms_baked.nif" ]; then
			shapenames() { "$EXE" -no-gui list "$1" 2>/dev/null \
				| grep "BSTriShape" | sed "s/.*'\(.*\)'/\1/" | sort; }
			shapenames "$TMP/arms.nif" > "$TMP/pre.txt"
			shapenames "$TMP/arms_baked.nif" > "$TMP/post.txt"
			new="$(comm -13 "$TMP/pre.txt" "$TMP/post.txt")"
			total="$(echo "$new" | grep -c .)"
			uniq_n="$(echo "$new" | sort -u | grep -c .)"
			if [ "$total" -ge 2 ] && [ "$total" = "$uniq_n" ]; then
				ok; echo "  $total baked shape name(s), all distinct and none pre-existing"
			else
				bad "$total baked shape(s) but $uniq_n distinct name(s)"
				echo "$new" | sed 's/^/    /'
			fi
		else
			bad "no baked file for the arm rig"
		fi
	fi
else
	echo "  (skipping the colliding-name case: skeleton or arm effects missing)"
fi

echo
echo "checks passed: $pass   failed: $fail"
[ "$fail" -eq 0 ]
