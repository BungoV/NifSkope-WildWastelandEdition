#!/bin/bash
# Lane UI6 -- the gate chain, SEQUENTIAL, one NifSkope instance ever.
# A lock directory makes a second copy of the chain impossible (lane
# WATER8-GATE ran its chain twice and its logs were rubbish).
cd /e/Projects/NifskopeWildWastelandEdition || exit 2
LOCK=scratchpad/ui6_20260910/.gatelock
mkdir "$LOCK" 2>/dev/null || { echo "a gate chain is already running ($LOCK)"; exit 9; }
trap 'rmdir "$LOCK" 2>/dev/null' EXIT

L=scratchpad/ui6_20260910/logs
I=/e/Projects/NifskopeWildWastelandEdition/scratchpad/ui6_20260910/images

run() {
	name="$1"; shift
	echo "=================== $name"
	timeout 900 "$@" > "$L/$name.log" 2>&1
	echo "spell-rc=$?"
	grep -aE "checks run|checks,|^PASS$|^FAIL$|RESULT|FAIL: " "$L/$name.log" | tail -12
	tasklist | grep -qi NifSkope && echo "WARNING: a NifSkope is still running after $name"
}

SHOT=$I/toprow_after.png STRIPSHOT=$I/strip4x_after.png LODSHOT=$I/lodtab_after.png \
	run water_ui bash tests/spells/water_ui.sh
run ui_align bash tests/spells/ui_align.sh
run top_bar bash tests/spells/top_bar.sh
run files_tab bash tests/spells/files_tab.sh
run animws bash tests/spells/animws.sh
run hkxanim_ui bash tests/spells/hkxanim_ui.sh
run loaded_nifs bash tests/spells/loaded_nifs.sh
echo "=================== chain done"
tasklist | grep -i -E "Fallout4|NifSkope"; echo "left-running-rc=$?"
