#!/bin/bash
#
# The Block List spell menu — is the new taxonomy the one that was declared?
#
# WHY THIS EXISTS
#
# The context menu's top level used to be decided by two accidents. Submenus are
# created the first time a spell on that page registers, so their ORDER was the
# order object files happen to appear in NifSkope.pro — reordering SOURCES
# silently reordered the user's menu. And their CONTENTS came from Spell::page(),
# which is not a menu label at all: it is also the CLI namespace ("-s Page/Name"),
# the Unfuck dialog's membership test, the spell-id syntax SpellBook::lookup
# parses, and the switch that decides whether a cast suppresses model signals.
#
# So the menu could not be fixed by editing page() strings — that would rename
# CLI spells and move things in and out of the Unfuck panel. Spell::group() is
# the separate axis the menu is now built from, and 106 spells declare one.
#
# WHAT IS MEASURED
#
# None of this is visible in a diff. 106 group() overrides all compile whether or
# not they landed on the right spell, and a submenu that still exists but is
# empty looks identical to one that was correctly retired. So the harness builds
# a real SpellBook against a real block and reads the menu back:
#
#   1. the declared order holds — Block, Add, Transform, … Info
#   2. most of the new groups exist at all
#   3. no retired page name (Havok, Node, Rigging, Skeleton, Texture, Shader,
#      Color, Mesh, Bounds, Morph, Header, Footer, String Palette, Array) is
#      still a submenu title
#   4. Batch / Sanitize / Error Checking / Optimize survive — they keep page()
#      on purpose — but are HIDDEN on a block row, because every one of them
#      casts against an invalid index and rewrites the whole file
#   5. no submenu offers the same enabled label twice. This is the specific
#      failure mode of merging pages: four spells are named "Choose" and three
#      "Update", and they were told apart only by living on different pages
#   6. the deleted entries are really gone, and Enforce Node Name Authority /
#      Decompile All Compiled Collision offer themselves on NO block row
#      (both used to appear on every one and then edit the entire file)
#   7. Material nests its legacy NiTexturingProperty spells under Textures
#
# Check 1 fails on the previous version: with the old table the order began
# Transform, Block, Node, Mesh, Havok. Checks 3 and 5 fail on it outright.
#
# The log also contains the full menu tree, which is the fastest way to see what
# a regrouping actually did.
#
# NOTE ON PORTS: NifSkope exits silently if it cannot bind its IPC port, and on
# this machine UDP above ~49152 is refused. Keep --port below that.
#
# USAGE
#   bash tests/spells/menu_taxonomy.sh

set -u

# These harnesses open a real window; keep it off the primary monitor so a
# suite run never takes focus from whoever is working. See _harness.sh.
. "$(dirname "$0")/_harness.sh"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="${EXE:-$ROOT/release/NifSkope.exe}"
SRC="${SRC:-/e/Tools/Fallout 4/DataUnpacked/Data/meshes/Effects/ElectricalExplosionSmall.nif}"
PORT="${PORT:-45891}"
LOG="$ROOT/release/ww_menutree_test.log"

[ -x "$EXE" ] || { echo "no NifSkope.exe at $EXE"; exit 2; }
[ -f "$SRC" ] || { echo "no fixture at $SRC"; exit 2; }

winpath() { printf '%s' "$1" | sed 's|^/\([a-zA-Z]\)/|\1:/|'; }

rm -f "$LOG"
WW_MENUTREE_TEST=1 "$EXE" --port "$PORT" "$(winpath "$SRC")" >/dev/null 2>&1 &
pid=$!
for _ in $(seq 1 60); do
	[ -f "$LOG" ] && grep -q '^done$' "$LOG" 2>/dev/null && break
	sleep 1
done
kill "$pid" 2>/dev/null
wait "$pid" 2>/dev/null

if [ ! -f "$LOG" ]; then
	echo "FAIL: harness produced no log (did NifSkope start?)"
	exit 1
fi

cat "$LOG"
grep -q '^PASS$' "$LOG" && exit 0
exit 1
