#!/bin/bash
#
# A loading screen whose effects are still RUNNING.
#
# WHY THIS EXISTS
#
# The other route to a Tesla loading screen is the bake (effect_bake.sh): freeze
# the arcs and sprites into static geometry. That is the safe one -- 0 of the 173
# vanilla loading screens contain a particle system or a procedural lightning
# controller, so a baked screen is the only kind with precedent.
#
# This is the other route, the one the effects were authored for: keep the
# ArtObject branch whole and let it generate. What makes it plausible is the rest
# of the corpus -- 18 of the 173 animate node transforms and CreatureBloatfly.nif
# carries a full NiControllerManager and sequence, so the menu does step them.
#
# TWO THINGS HAVE TO HOLD, AND THEY FAIL SEPARATELY
#
# 1. The branch still runs. Block counts cannot see this: a particle system's
#    geometry is not in the file, so a file with every block present can draw
#    nothing at all. Only the renderer can answer, which is what the GUI harness
#    is for.
#
# 2. It runs WHERE IT WAS. The convert deletes the skeleton, and a branch whose
#    attach bone went with it collapses to the origin -- geometry generates, has
#    extent, and sits in a heap at the feet. The world transform of every kept
#    object is compared against the merged rig it came from, which is exact:
#    those coordinates should not move by so much as a rounding step.
#
# USAGE
#   bash tests/loadingscreen/live_effects.sh [time]
# Needs release/NifSkope.exe, the FO4 corpus and the X01Tesla mod.

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
SK="${SK:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/actors/powerarmor/CharacterAssets/skeleton.nif}"
X="${X:-/e/Projects/Fallout 4 Mods/mods/X01Tesla/meshes/actors/powerarmor/x01}"
TIME="${1:-2.5}"
LOG="$ROOT/release/ww_livefx_test.log"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass=0; fail=0
ok()  { pass=$((pass+1)); }
bad() { fail=$((fail+1)); echo "  FAIL: $*"; }

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SK" ]  || { echo "no skeleton at $SK"; exit 2; }
[ -d "$X" ]   || { echo "no X01Tesla meshes at $X"; exit 2; }

# --- 1. assemble, effects live ----------------------------------------------
add=()
for p in Helmet Torso ArmLeft ArmRight LegLeft LegRight; do
	for f in "X01_$p.nif" "X01_${p}_Tesla.nif" "X01_${p}_Tesla_VFX.nif"; do
		[ -f "$X/$f" ] && add+=( --add "$X/$f" )
	done
done
echo "merging $(( ${#add[@]} / 2 )) donors onto the skeleton (effects live)"
"$EXE" -no-gui merge "$SK" "${add[@]}" -o "$TMP/rig.nif" > "$TMP/merge.log" 2>&1 \
	|| { echo "  merge FAILED"; cat "$TMP/merge.log"; exit 1; }

# One animation graph, however many ArtObjects went in.
for type in NiControllerManager NiDefaultAVObjectPalette NiMultiTargetTransformController; do
	n="$("$EXE" -no-gui info "$TMP/rig.nif" 2>/dev/null | awk -v t="$type" '$1==t {print $2}' | tr -d 'x')"
	if [ "${n:-0}" = "1" ]; then ok
	else bad "the merged rig has ${n:-0} $type(s), expected 1"; fi
done

# --- 2. convert, keeping the effects ----------------------------------------
echo "converting with --keep-effects"
"$EXE" -no-gui loading-screen "$TMP/rig.nif" --keep-effects -o "$TMP/screen.nif" \
	> "$TMP/convert.log" 2>&1 || { echo "  convert FAILED"; cat "$TMP/convert.log"; exit 1; }
sed -n 's/^  \([0-9]* effect branch\)/  \1/p' "$TMP/convert.log"

branches="$(sed -n 's/^  \([0-9]*\) effect branch.*/\1/p' "$TMP/convert.log")"
if [ "${branches:-0}" -ge 6 ]; then ok
else bad "only ${branches:-0} effect branch(es) kept, expected one per limb at least"; fi

for type in NiParticleSystem BSProceduralLightningController; do
	n="$("$EXE" -no-gui info "$TMP/screen.nif" 2>/dev/null | awk -v t="$type" '$1==t {print $2}' | tr -d 'x')"
	if [ "${n:-0}" -gt 0 ]; then ok; echo "  ${n} $type survived"
	else bad "the converted screen has no $type -- the branches were flattened"; fi
done

# The skeleton is the thing a loading screen does not have.
skins="$("$EXE" -no-gui info "$TMP/screen.nif" 2>/dev/null | grep -cE "BSSkin::Instance|NiSkinInstance")"
if [ "$skins" = "0" ]; then ok
else bad "$skins skin block(s) survived -- the skeleton came back with the effects"; fi

# Every palette entry has to resolve, or a sequence binds to nothing.
pal="$("$EXE" -no-gui list "$TMP/screen.nif" 2>/dev/null \
	| sed -n 's/^\[\([0-9]*\)\] NiDefaultAVObjectPalette.*/\1/p' | head -1)"
if [ -n "$pal" ]; then
	dangling="$("$EXE" -no-gui dump "$TMP/screen.nif" -b "$pal" -d 3 -n 4000 2>/dev/null \
		| grep -c 'AV Object  <Ptr>  = -1')"
	if [ "$dangling" = "0" ]; then ok
	else bad "$dangling object palette entr(ies) point at deleted blocks"; fi
else
	bad "the converted screen has no object palette"
fi

# --- 3. did anything MOVE? --------------------------------------------------
# Exact comparison by name, over the objects the convert is supposed to leave
# alone: NODES, plus the particle systems. A SHAPE is a bad witness here -- a
# flattened one moves by design, since its vertices are rewritten into world
# space and its node takes the centroid, so comparing those would fail on
# correct behaviour. Nodes are never flattened, and a stub that dropped its
# bone's transform moves every node under it, so this is the sharp instrument
# for the claim being made.
"$EXE" -no-gui world "$TMP/rig.nif"    2>/dev/null | sed 's/^\[[0-9]*\] //' | sort > "$TMP/before.txt"
"$EXE" -no-gui world "$TMP/screen.nif" 2>/dev/null | sed 's/^\[[0-9]*\] //' | sort > "$TMP/after.txt"

moved=0; kept=0
while IFS= read -r line; do
	case "$line" in
		NiNode*|NiBillboardNode*|NiParticleSystem*) ;;
		*) continue ;;
	esac
	name="$(printf '%s' "$line" | sed -n "s/^[A-Za-z:]*  *'\([^']*\)'.*/\1/p")"
	[ -n "$name" ] || continue
	[ "$name" = "LoadingMenuZoomTarget" ] && continue        # added by the convert
	b="$(grep -F "'$name'" "$TMP/before.txt" | head -2)"
	[ "$(printf '%s\n' "$b" | wc -l)" = "1" ] || continue    # ambiguous name, skip
	[ -n "$b" ] || continue
	kept=$((kept+1))
	if [ "$b" != "$line" ]; then
		moved=$((moved+1))
		[ "$moved" -le 3 ] && { echo "    before: $b"; echo "    after : $line"; }
	fi
done < "$TMP/after.txt"
echo "  $kept effect node(s) compared, $moved moved"
if [ "$moved" = "0" ] && [ "$kept" -gt 20 ]; then ok
else bad "$moved of $kept kept node(s) changed world transform -- the attach stub did not carry the bone"; fi

# ...and nothing the ordinary convert keeps may go missing. Keeping a branch
# alive cut three leg shapes out of the file once: they were inside a branch but
# were not seeds, so a narrower rule orphaned them and the sweep took them --
# which left the particle emitters pointing at nothing and their sprites 90 units
# out to the side, on a figure 30 wide.
"$EXE" -no-gui loading-screen "$TMP/rig.nif" -o "$TMP/flat.nif" > "$TMP/flat.log" 2>&1
"$EXE" -no-gui list "$TMP/flat.nif"   2>/dev/null | grep TriShape | sed 's/^\[[0-9]*\] //' | sort > "$TMP/sh_flat.txt"
"$EXE" -no-gui list "$TMP/screen.nif" 2>/dev/null | grep TriShape | sed 's/^\[[0-9]*\] //' | sort > "$TMP/sh_live.txt"
lost="$(comm -23 "$TMP/sh_flat.txt" "$TMP/sh_live.txt")"
if [ -z "$lost" ]; then ok
else bad "shape(s) the ordinary convert keeps are missing: $(echo "$lost" | tr '\n' ' ')"; fi

# Every particle emitter must still name the mesh it emits from. A dangling
# Emitter Meshes pointer does not stop the emitter -- it relocates it.
dangling=0
for b in $("$EXE" -no-gui list "$TMP/screen.nif" 2>/dev/null \
	| sed -n 's/^\[\([0-9]*\)\] NiPSysMeshEmitter.*/\1/p'); do
	m="$("$EXE" -no-gui get "$TMP/screen.nif" -b "$b" -f "Emitter Meshes/0" 2>/dev/null)"
	[ "${m:--1}" = "-1" ] && dangling=$((dangling+1))
done
if [ "$dangling" = "0" ]; then ok
else bad "$dangling particle emitter(s) lost the mesh they emit from"; fi

# --- 4. does it RUN? --------------------------------------------------------
# The only place a particle or an arc exists is the rendered scene, so this half
# needs the GUI. Off to the side and on its own port, as the other GUI harnesses
# run: it opens a window for a couple of seconds.
echo "stepping the converted screen to t=$TIME in the GUI"
rm -f "$LOG"
WW_LIVEFX_TEST=1 WW_LIVEFX_TIME="$TIME" WW_WINDOW_AT="1960,20" \
	"$EXE" --port 41893 "$TMP/screen.nif" > /dev/null 2>&1

if [ ! -f "$LOG" ]; then
	bad "the GUI harness wrote no log -- it did not run"
else
	sed -n 's/^\(generated\|world Z\|[0-9]* particle\)/  &/p' "$LOG"
	if grep -q '^PASS' "$LOG"; then ok
	else
		bad "the GUI harness failed:"
		grep '^  FAIL' "$LOG" | sed 's/^/  /'
	fi
fi

# --- 5. the old path still behaves ------------------------------------------
# Without the flag the convert must still drop the effects and say so, or the
# 173-file precedent argument quietly stops applying to the default. (flat.nif
# was already built above, for the missing-shape comparison.)
n="$("$EXE" -no-gui info "$TMP/flat.nif" 2>/dev/null | grep -cE "NiParticleSystem|BSProceduralLightningController")"
if [ "$n" = "0" ]; then ok
else bad "the default convert kept $n effect block type(s) -- it should drop them"; fi

echo
echo "checks passed: $pass   failed: $fail"
[ "$fail" = "0" ] || exit 1
