#!/bin/bash
#
# The Unfuck workspace — does it find, classify, colour, locate and repair?
#
# WHY THIS EXISTS
#
# The panel lists what is wrong with the open NIF. Five things can each be
# quietly wrong and not one of them is visible from a screenshot: the scan can
# find nothing, the severity can be flattened, the block number can fail to parse
# out of the message, Go to can fail to move the selection, and a fix button can
# be present but do nothing at all. That last one is not hypothetical — the
# Repairs button shipped in one commit doing literally nothing, because
# SpellBook::lookup matches on page() and every call site passed a bare name.
#
# THE FIXTURE HAS TO BE BROKEN ON PURPOSE
#
# This is the part worth stating. A stock FO4 effect mesh produces ZERO findings:
#
#   $ WW_UNFUCKPANEL_TEST=1 NifSkope ElectricalExplosionSmall.nif
#   status: No issues found.    (268 blocks)
#
# which is not evidence the panel works, and running the harness on a clean file
# is how you get three red checks that mean nothing. So the fixture is built here
# rather than assumed: a texture path is rewritten to drop its file extension,
# which spErrorInvalidPaths reports as an error against a named block.
#
# It also says something about coverage. The panel currently runs four checks —
# three spChecker subclasses plus Check Links by name — so "everything wrong with
# this file" is bounded by what those four look at, and on real Bethesda assets
# that is usually nothing.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/spells/unfuck_panel.sh

set -u

# These harnesses open a real window; keep it off the primary monitor so a
# suite run never takes focus from whoever is working. See _harness.sh.
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/Effects/ElectricalExplosionSmall.nif}"
# A stock static, UNMODIFIED: it exercises the material check and the block-scoped
# per-row fix, neither of which the effect file above can reach.
STATIC="${STATIC:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/SetDressing/35CourtSign/35CourtSign01.nif}"
PORT="${PORT:-45893}"
LOG="$ROOT/release/ww_unfuckpanel_test.log"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture at $SRC"; exit 2; }

fails=0

# --- build the broken fixture ----------------------------------------------
# The first BSEffectShaderProperty in the file, whatever number it happens to be,
# so this does not rot if the asset is replaced.
SHADER="$("$EXE" -no-gui list "$(winpath "$SRC")" 2>/dev/null \
	| sed -n 's/^\[\([0-9]*\)\] BSEffectShaderProperty *$/\1/p' | head -1)"
[ -n "$SHADER" ] || { echo "no BSEffectShaderProperty in $SRC"; exit 2; }

ORIG="$("$EXE" -no-gui get "$(winpath "$SRC")" -b "$SHADER" -f "Source Texture" 2>/dev/null | tr -d '\r')"
[ -n "$ORIG" ] || { echo "block $SHADER has no Source Texture"; exit 2; }
BROKEN="${ORIG%.dds}"
[ "$BROKEN" != "$ORIG" ] || { echo "Source Texture is not a .dds: $ORIG"; exit 2; }

echo "fixture: block $SHADER Source Texture"
echo "  '$ORIG'"
echo "  -> '$BROKEN'   (extension removed, so spErrorInvalidPaths reports it)"

"$EXE" -no-gui set "$(winpath "$SRC")" -b "$SHADER" -f "Source Texture" -v "$BROKEN" \
	-o "$(winpath "$TMP/broken.nif")" > /dev/null 2>&1
[ -s "$TMP/broken.nif" ] || { echo "could not write the broken fixture"; exit 2; }

# --- case 1: the deliberately broken effect file -----------------------------
echo
echo "=== case 1: broken texture path ==="
rm -f "$LOG"
WW_UNFUCKPANEL_TEST=1 "$EXE" --port "$PORT" "$(winpath "$TMP/broken.nif")" > /dev/null 2>&1
[ -f "$LOG" ] || { echo "harness produced no log"; exit 1; }
tr -d '\r' < "$LOG"
grep -q '^PASS' "$LOG" || fails=$((fails+1))

# --- case 2: an UNMODIFIED stock static --------------------------------------
#
# Two things this proves that case 1 cannot.
#
# The material check runs at all: it needs a shader with a real .bgsm, and the
# effect file above has no material of any kind. And the block-scoped per-row fix
# works: "Fill BSShaderTextureSet from BGSM" is cast on the shader BLOCK, not on
# an array inside it, which is a different target-rebuilding path from Collapse.
#
# It also pins the severity call. Every vanilla static disagrees with its own
# material — five of five sampled reported it — so these MUST come out as notes.
# If this file ever reports an error or a warning, either the severity split
# broke or the resource paths are not resolving, and both are worth stopping for.
if [ -f "$STATIC" ]; then
	echo
	echo "=== case 2: stock static, unmodified ==="
	cp "$STATIC" "$TMP/static.nif"
	rm -f "$LOG"
	WW_UNFUCKPANEL_TEST=1 "$EXE" --port "$((PORT + 1))" "$(winpath "$TMP/static.nif")" > /dev/null 2>&1
	if [ -f "$LOG" ]; then
		tr -d '\r' < "$LOG"
		grep -q '^PASS' "$LOG" || fails=$((fails+1))
		status="$(tr -d '\r' < "$LOG" | head -1)"
		case "$status" in
			*error*|*warning*)
				echo "  FAIL: a stock asset must not raise errors or warnings — got: $status"
				fails=$((fails+1)) ;;
			*note*)
				echo "  ok   drift on a stock asset reports as notes, not problems" ;;
			*)
				echo "  FAIL: expected notes on a stock static, got: $status"
				fails=$((fails+1)) ;;
		esac
	else
		echo "  FAIL: no log for case 2"; fails=$((fails+1))
	fi
else
	echo "  (no stock static at $STATIC — skipping the material case)"
fi

# --- case 3: a path that is BOTH absolute and extensionless -------------------
#
# spErrorInvalidPaths tested these two faults with `if / else if`, so a path that
# had both reported only the missing extension and never the absolute path --
# and absolute is the more serious of the two, because it resolves on the machine
# that authored it and nowhere else. Exactly the case that hid the worse fault.
#
# Fails on the old code: it produces one finding, not two.
echo
echo "=== case 3: absolute AND extensionless ==="
"$EXE" -no-gui set "$(winpath "$SRC")" -b "$SHADER" -f "Source Texture" \
	-v 'C:\textures\effects\nowhere' -o "$(winpath "$TMP/both.nif")" > /dev/null 2>&1
if [ -s "$TMP/both.nif" ]; then
	rm -f "$LOG"
	WW_UNFUCKPANEL_TEST=1 "$EXE" --port "$((PORT + 2))" "$(winpath "$TMP/both.nif")" > /dev/null 2>&1
	if [ -f "$LOG" ]; then
		clean="$(tr -d '\r' < "$LOG")"
		printf '%s\n' "$clean" | sed -n '1,8p'
		if printf '%s' "$clean" | grep -q "has an absolute filepath"; then
			echo "  ok   the absolute path is reported"
		else
			echo "  FAIL: absolute path not reported — the else-if is back"
			fails=$((fails+1))
		fi
		if printf '%s' "$clean" | grep -q "without a file extension"; then
			echo "  ok   the missing extension is still reported alongside it"
		else
			echo "  FAIL: missing extension not reported"
			fails=$((fails+1))
		fi
	else
		echo "  FAIL: no log for case 3"; fails=$((fails+1))
	fi
else
	echo "  FAIL: could not write the case 3 fixture"; fails=$((fails+1))
fi

# --- case 4: a broken collision reference, and its per-row fix ----------------
#
# The collision lint reports one finding per block, and two of its findings now
# carry repairs hoisted out of the Collision Manager's modal "Apply Safe Fixes"
# pass. This breaks a real compiled collision object's Data link and requires
# that the panel (a) reports it as an ERROR, not a note -- a collision object
# pointing at nothing is genuinely broken, unlike most of what the lint says --
# and (b) that clicking the fix resolves the finding it named.
#
# FO4 ships compiled collision throughout, so the layer repair
# (Set Collision Layer from Motion, which needs an editable bhkRigidBody) is not
# reachable from this corpus and is NOT covered here. Said plainly rather than
# left to look covered.
if [ -n "$STATIC" ] && [ -f "$STATIC" ]; then
	echo
	echo "=== case 4: broken collision Data reference ==="
	COLL="$("$EXE" -no-gui list "$(winpath "$STATIC")" 2>/dev/null \
		| sed -n 's/^\[\([0-9]*\)\] bhkNPCollisionObject.*/\1/p' | head -1)"
	if [ -n "$COLL" ]; then
		"$EXE" -no-gui set "$(winpath "$STATIC")" -b "$COLL" -f "Data" -v "-1" \
			-o "$(winpath "$TMP/nocoll.nif")" > /dev/null 2>&1
		if [ -s "$TMP/nocoll.nif" ]; then
			rm -f "$LOG"
			WW_UNFUCKPANEL_TEST=1 "$EXE" --port "$((PORT + 3))" \
				"$(winpath "$TMP/nocoll.nif")" > /dev/null 2>&1
			clean="$(tr -d '\r' < "$LOG" 2>/dev/null)"
			printf '%s\n' "$clean" | sed -n '1,4p'
			printf '%s\n' "$clean" | grep -E "^  (ok|FAIL) " | tail -6
			grep -q '^PASS' "$LOG" || fails=$((fails+1))
			if printf '%s' "$clean" | head -1 | grep -q "error"; then
				echo "  ok   a dangling collision reference is an error, not a note"
			else
				echo "  FAIL: expected an error for a dangling collision reference"
				fails=$((fails+1))
			fi
			if printf '%s' "$clean" | grep -q "Remove Broken Collision Object ran"; then
				echo "  ok   the hoisted collision repair ran from the panel"
			else
				echo "  FAIL: the collision repair did not run"
				fails=$((fails+1))
			fi
		else
			echo "  FAIL: could not write the case 4 fixture"; fails=$((fails+1))
		fi
	else
		echo "  (no bhkNPCollisionObject in the static fixture — skipping)"
	fi
fi

echo
echo "cases failed: $fails"
[ "$fails" -eq 0 ]
