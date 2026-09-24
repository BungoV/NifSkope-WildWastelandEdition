#!/bin/bash
# Lane UI5's harness chain. SEQUENTIAL, one NifSkope instance ever: every spell
# launches its own instance with --port and kills it before the next begins, so
# they are run one after another and never in parallel.
#
# A lock directory makes a second copy of this script impossible rather than
# merely unlikely -- lane WATER8-GATE ran its chain twice and wrote two sets of
# logs, and said in its own mistakes that the next chain in this tree should
# have one.
#
# Env assignments go on the CHILD (env SHOT=... bash spell), never exported into
# this shell, so a spell that ignores a name cannot silently inherit another's.
#
# Which harnesses, and why those (CONSTITUTION 6, "run only the harnesses the
# change reaches"): the change is one QSS rule on QMenuBar::item and the skin
# helper that states it, applied inside wwAlignBarRow.
#   water_ui.sh   owns the bar row and now group M, the menu bar's titles
#   ui_align.sh   the seam and the row's other bars
#   top_bar.sh    the toolbars and the dock strip in the same row
#   files_tab.sh  the left dock's own page, one row below
#   animws.sh     the Animation dock, the other consumer of the shared skin
# Everything else (lodgen, terrain, impostor, gltf, hkx*, collision, block, the
# water solve/flow/mark/window suites) is not reached by a menu-item padding and
# is skipped with that as the reason. skeleton_overlay.sh is skipped as well:
# BUILD11's own four-run measurement calls it flaky and this lane does not touch it.
set -u
cd /e/Projects/NifskopeWildWastelandEdition || exit 9
S=scratchpad/ui5_20260910
OUT=$S/logs
mkdir "$S/.gatelock" 2>/dev/null || { echo "REFUSED: a chain is already running (.gatelock)"; exit 8; }
mkdir -p "$OUT" "$S/images"

run() {
	name="$1"; shift
	echo "=== $name"
	if tasklist 2>/dev/null | grep -qi "NifSkope.exe"; then
		echo "REFUSED: a NifSkope is already running -- one instance ever" | tee "$OUT/$name.log"
		return 8
	fi
	"$@" > "$OUT/$name.log" 2>&1
	rc=$?
	tail -6 "$OUT/$name.log"
	echo "$name RC=$rc"
	return 0
}

run water_ui env SHOT="$PWD/$S/images/toprow_after.png" \
	STRIPSHOT="$PWD/$S/images/strip4x_after.png" \
	LODSHOT="$PWD/$S/images/lodtab_lod.png" \
	LODSHOT2="$PWD/$S/images/lodtab_water.png" \
	bash tests/spells/water_ui.sh
run ui_align env SHOT="$PWD/$S/images/seam_after.png" bash tests/spells/ui_align.sh
run top_bar bash tests/spells/top_bar.sh
run files_tab bash tests/spells/files_tab.sh
run animws bash tests/spells/animws.sh

echo "=== counts"
for f in "$OUT"/*.log; do
	printf '%-28s %s\n' "$(basename "$f")" \
		"$(grep -aE '^[0-9]+ checks, ' "$f" | tail -1)"
done
rmdir "$S/.gatelock" 2>/dev/null
echo "CHAIN DONE"
