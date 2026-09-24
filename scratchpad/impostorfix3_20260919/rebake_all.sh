#!/bin/sh
# IMPOSTORFIX3 -- re-run the COMPRESSION half of all five fixture bakes with the
# 8-ring height fill. The photography half is untouched by the change (it writes
# the `_oct_*.png` sheets; `lodgenRepairOctHeight` runs in `lodgenCard()`, which
# reads them), so this re-runs lodgen over the PNGs already on disk.
#
# Three sheet sets end up side by side under each fixture:
#   cards_before/  the sheets exe 88d6abb3 shipped   (the flood)
#   cards_r3/      the sheets exe af457755 produces  (dilate inside coverage,
#                                                     card plane outside)
#   cards/         this lane's                       (dilate 8 rings outside)
set -u
R="E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix3_20260919"
for t in blast_n4 blast_n8 maple_n4 dead_n4 rock_n4; do
	O="$R/fixture/$t"
	[ -d "$O/cards" ] || { echo "$t: no cards/"; continue; }
	if [ ! -d "$O/cards_r3" ]; then
		mkdir -p "$O/cards_r3"
		cp "$O/cards/"*.DDS "$O/cards_r3/" 2>/dev/null
		cp "$O/cards/"*.lodm "$O/cards_r3/" 2>/dev/null
	fi
	echo "== $t"
	sh "E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfix1_20260919/rebake_dds.sh" "$O"
done
