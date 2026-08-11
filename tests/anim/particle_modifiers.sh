#!/bin/bash
#
# Particle modifiers — is a modifier's Active state read, and does an emitter
# parameter controller beat the authored field?
#
# WHY THIS EXISTS
#
# The simulator ignored two whole mechanisms. A modifier's own `Active` field was
# never read, so a modifier the author had switched off still ran; and the
# NiPSysModifierCtlr family was skipped entirely, so nothing could be switched on
# or off partway through an effect and no emitter parameter could be animated.
#
# Which of the engine's modifiers were worth adding was decided by counting them
# in FO4's 692 effect meshes rather than by reading the class list:
#
#   NiPSysModifierActiveCtlr        288      <- by far the most common
#   NiPSysEmitterSpeedCtlr           51
#   NiPSysEmitterInitialRadiusCtlr   21
#   NiPSysEmitterLifeSpanCtlr         6
#   NiPSysEmitterDeclinationCtlr      6
#   NiPSysGrowFadeModifier            0      <- would have been dead code
#   the six field modifiers           0
#   NiPSysMeshUpdateModifier          0
#   NiPSysSpawnModifier             347, but 66 of 67 sampled have
#                                   Num Spawn Generations = 0, so it spawns nothing
#
# THE MEASUREMENTS
#
# Switching an emitter's Active field off must produce NO particles. Verified to
# fail on the code this replaces, which baked the same 62 vertices either way
# because it never read the flag. That is the discriminating check here; §3 below
# says plainly what is NOT pinned and why.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/anim/particle_modifiers.sh

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
FX="${FX:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/Effects/Ambient/FXPaperBitInWind.nif}"
SPD="${SPD:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/Effects/ElectricalExplosionSmall.nif}"
PORT="${PORT:-45881}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass=0; fail=0
ok()  { pass=$((pass+1)); }
bad() { fail=$((fail+1)); echo "  FAIL: $*"; }

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$FX" ]  || { echo "no fixture at $FX"; exit 2; }

# Pure bash, deliberately no sed: when this script is launched from a Git-Bash
# parent, the MSYS2 shell inherits Git's sed, and BRE groups passed across the
# two runtimes silently stop matching — winpath then no-ops. Single argv/env
# paths still get rescued by MSYS2's automatic conversion, but a
# semicolon-joined LIST (WW_SAMPOSE_WEAPON) is not, so the weapon step quietly
# skips. Parameter expansion cannot be PATH-poisoned.
winpath() {
	case "$1" in
		/[a-zA-Z]/*) local d="${1:1:1}"; printf '%s' "${d}:${1:2}" ;;
		*) printf '%s' "$1" ;;
	esac
}

# total baked vertices for a file at a time; "0" when nothing bakes at all
baked() {
	local src="$1" t="$2" out="$TMP/baked.nif"
	rm -f "$out"
	WW_EFFECTBAKE_TEST="$out" WW_EFFECTBAKE_TIME="$t" \
		"$EXE" --port "$PORT" "$(winpath "$src")" > /dev/null 2>&1
	[ -s "$out" ] || { echo 0; return; }
	"$EXE" -no-gui list "$out" 2>/dev/null \
		| sed -n 's/^\[\([0-9]*\)\] BS[A-Za-z]*TriShape.*/\1/p' \
		| while read -r b; do "$EXE" -no-gui get "$out" -b "$b" -f "Num Vertices" 2>/dev/null | tr -d '\r'; done \
		| awk '{ s += $1 } END { print s+0 }'
}

# the file's first emitter block
emitter_of() {
	"$EXE" -no-gui list "$1" 2>/dev/null \
		| sed -n 's/^\[\([0-9]*\)\] NiPSys[A-Za-z]*Emitter .*/\1/p' | head -1
}

# --- 1. a modifier's Active field ------------------------------------------
cp "$FX" "$TMP/on.nif"
EB="$(emitter_of "$TMP/on.nif")"
[ -n "$EB" ] || { echo "no emitter in $FX"; exit 2; }
echo "fixture: $(basename "$FX"), emitter block $EB"

on="$(baked "$TMP/on.nif" 6.0)"
echo "  emitter Active=yes -> $on baked vertices"
if [ "${on:-0}" -gt 0 ] 2>/dev/null; then ok
else bad "the fixture bakes nothing even with its emitter on; nothing below can mean anything"; fi

"$EXE" -no-gui set "$TMP/on.nif" -b "$EB" -f Active -v no -o "$TMP/off.nif" > /dev/null 2>&1
if [ -s "$TMP/off.nif" ]; then ok
else bad "could not switch the emitter's Active field off"; fi

off="$(baked "$TMP/off.nif" 6.0)"
echo "  emitter Active=no  -> $off baked vertices"
if [ "${off:-1}" = "0" ]; then ok
else bad "an inactive emitter still produced $off vertices -- the Active field is not read"; fi

# --- 2. nothing keeps emitting after an Active curve goes false -------------
# This fixture's controller holds a bool curve that is 1 until t = 6.1667 and 0
# after, and its particles live 8.67 +/- 3.33 s, so few die in the window below.
#
# Weaker than it looks, and worth being honest about: the old code passes this
# too, because this asset's birth rate has already fallen to zero by 6.0 for
# reasons of its own. It is kept as a guard against a future change that starts
# emitting where the file says stop, not as evidence for the current one.
if "$EXE" -no-gui list "$FX" 2>/dev/null | grep -q "NiPSysModifierActiveCtlr"; then
	late="$(baked "$TMP/on.nif" 8.5)"
	echo "  t=6.0 -> $on vertices, t=8.5 -> $late (emission stops at 6.17)"
	if [ "${late:-0}" -le "${on:-0}" ] 2>/dev/null; then ok
	else bad "particles kept being emitted after the Active curve went false ($on -> $late)"; fi
else
	echo "  (no NiPSysModifierActiveCtlr in the fixture)"
	ok
fi

# --- 3. the emitter parameter curves --------------------------------------
#
# NOT asserted here, deliberately, and it is worth saying why rather than
# leaving a check that cannot fail. The obvious test -- rewrite the authored
# Speed and show the bake does not move, because the curve overrides it -- needs
# a file where exactly one emitter is curve-driven. FO4's speed-controller
# assets are not like that: ElectricalExplosionSmall.nif has three sphere
# emitters ALL named "NiPSysSphereEmitter:0", so a controller naming that name
# reaches all three and the authored field still governs the ones the curve does
# not cover. Rewriting Speed there moves one cloud whether or not the curve is
# read, and vertex COUNT -- the other thing that is cheap to measure -- does not
# depend on speed at all.
#
# What is known: wiring the sequence-driven case changed a baked bounding radius
# on that file from 121.721 to 125.322, so the path does reach the simulation.
# Pinning it properly needs a fixture built for it, which is a job for whoever
# next needs the emitter curves to be exactly right.

echo
echo "checks passed: $pass   failed: $fail"
[ "$fail" = "0" ] || exit 1
