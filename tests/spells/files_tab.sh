#!/bin/bash
#
# The FILES tab: the renames, the extensions the tree lists, and what an .hkx
# row does when it is opened.
#
# WHY THIS EXISTS
#
# bungo's ruling, 2026-09-10, verbatim: "add hkx files to the NIFs tab, search
# for them in already set game folders ... rename 'available NIFs' to 'available
# files', and rename NIFs tab to 'Files', then also rename Loaded NIFs to
# 'Loaded Files', Basically replace mentions about nifs to generic 'files',
# because now we'll be able to browse and open not just nifs, well, we already
# can, with stuff like bto or btr".
#
# A rename is the class of change that a grep says is finished and a screenshot
# says is not: the strings that survive are the ones built at run time -- a
# model's header label, a menu item, a status sentence -- and none of those are
# in the file you edited. So gate (1) reads the LIVE WIDGET TREE and lists what
# it finds, and it is seeded with a known offender first so that a scan which
# has never found anything cannot report zero.
#
# WHAT IS MEASURED (src/filestabtest.cpp holds the detail)
#
#   1. 0 user-visible strings under the Files page still say "NIF", read off the
#      widget tree -- plus the FLOOR: one seeded offender must be found, and
#      must be gone again when the seed is removed
#   2. the tree lists .hkx AND .btr beside .nif, counted per extension over the
#      model the browser is showing; the first .hkx path is printed so the log
#      says which archive served it
#   3. opening an .hkx row plays it on the open model and answers with
#      HkxPlayback's own summary: the pre-registered 78 matched / 17 unmatched /
#      4 case-folded of the player skeleton (lane HKX1's measurement)
#   4. unloading it restores the bind pose BYTE-identically, with the floor that
#      the clip must have moved the rig first
#   5. the panel-style counts, with the floor that a blanked tooltip is counted
#   6. with nothing open, an .hkx REFUSES IN WORDS and loads nothing
#
# THE RESOURCE ROOTS ARE FORCED, not inherited. Gate (2) over whatever happens
# to be in Settings > Resources is a measurement of the machine, not of the
# code, so the harness writes two roots into the game manager in memory (the
# user's saved settings are never touched):
#
#   A) a loose Data/meshes tree this script builds: a .nif, a .bto and a .btr
#   B) Fallout4 - Animations.ba2, so the .hkx come out of a real ARCHIVE and
#      the archive walker is what is being gated, not a loose-file walk
#
# FIXTURES
#   fixtures/human_male_vanilla.nif                    the rigged model (lane FIXTURE)
#   scratchpad/hkx1_20260910/clips/jog.hkx             the clip, with skeleton.hkx beside it
#   scratch_water/d0.btr                               any .btr, for the census
#
# USAGE
#   bash tests/spells/files_tab.sh
#   SHOT=/path/dock.png bash tests/spells/files_tab.sh

set -u

. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
PORT="${PORT:-45907}"
LOG="$ROOT/release/ww_filestab_test.log"

SRC="${SRC:-$ROOT/fixtures/human_male_vanilla.nif}"
CLIP="${CLIP:-$ROOT/scratchpad/hkx1_20260910/clips/jog.hkx}"
BTR="${BTR:-$ROOT/scratch_water/d0.btr}"
ANIMBA2="${ANIMBA2:-/x/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4 - Animations.ba2}"
EXPECT="${EXPECT:-78,17,4}"
SHOT="${SHOT:-}"

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -s "$SRC" ]  || { echo "FAIL: no rigged fixture at $SRC"; exit 2; }
[ -s "$CLIP" ] || { echo "FAIL: no clip at $CLIP"; exit 2; }
[ -s "$BTR" ]  || { echo "FAIL: no .btr for the census at $BTR"; exit 2; }
[ -s "$ANIMBA2" ] || { echo "FAIL: no animations archive at $ANIMBA2"; exit 2; }

# NOT mktemp -d (lane BUILD9, 2026-09-10): it returns /tmp/tmp.XXXX, and
# winpath() only rewrites DRIVE-style /x/... paths, so the Windows binary was
# handed the literal string "/tmp/tmp.XXXX/Data", could not open it, counted it
# into nifBrowserSkippedResources and indexed the archive alone -- which is why
# gate (2)'s floor read ".nif 0 | .bto 0 | .btr 0 | .hkx 14939" on the first
# run. The fixture tree lives under the repo, on a real drive letter, so
# winpath can convert it and the loose walker is actually exercised.
TMP="$ROOT/scratchpad/filestab_20260910/fixture_tree"
rm -rf "$TMP"
mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT
case "$(winpath "$TMP")" in
	[a-zA-Z]:/*) ;;
	*) echo "FAIL: the fixture root is not a Windows path: $(winpath "$TMP")"; exit 2 ;;
esac

# ROOT A -- a loose Data/meshes tree. .bto and .btr are what bungo means by
# "we already can, with stuff like bto or btr"; the .nif is the census floor.
mkdir -p "$TMP/Data/meshes/ww"
"$EXE" -no-gui new --cube -o "$(winpath "$TMP/Data/meshes/ww/probe.nif")" >/dev/null 2>&1
cp "$BTR" "$TMP/Data/meshes/ww/probe.btr"
cp "$BTR" "$TMP/Data/meshes/ww/probe.bto"
[ -s "$TMP/Data/meshes/ww/probe.nif" ] && [ -s "$TMP/Data/meshes/ww/probe.btr" ] \
	|| { echo "FAIL: could not build the loose fixture tree"; exit 1; }
echo "fixture: loose $TMP/Data/meshes/ww with probe.nif, probe.bto, probe.btr"
echo "fixture: archive $(basename "$ANIMBA2")"

rm -f "$LOG"
WW_FILESTAB_TEST=1 \
WW_FILESTAB_RESOURCES="$(winpath "$TMP/Data");$(winpath "$ANIMBA2")" \
WW_FILESTAB_CLIP="$(winpath "$CLIP")" \
WW_FILESTAB_EXPECT="$EXPECT" \
WW_FILESTAB_SHOT="${SHOT:+$(winpath "$SHOT")}" \
	"$EXE" --port "$PORT" "$(winpath "$SRC")" >/dev/null 2>&1 &
pid=$!
for _ in $(seq 1 120); do
	[ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
	kill -0 "$pid" 2>/dev/null || break
	sleep 1
done
kill "$pid" 2>/dev/null
wait "$pid" 2>/dev/null

[ -f "$LOG" ] || { echo "FAIL: the harness wrote no log"; exit 1; }
cat "$LOG"
grep -q '^PASS$' "$LOG" || exit 1
exit 0
