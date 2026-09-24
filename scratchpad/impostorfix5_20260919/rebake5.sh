#!/bin/sh
# IMPOSTORFIX5 -- re-run the COMPRESSION half of all five fixture bakes on the
# RAMPED exe. The photography half writes the `_oct_*.png` sheets and is
# untouched by hookup_ramp.py (`lodgenRepairOctHeight` runs in `lodgenCard()`,
# which READS those PNGs), so the PNGs are copied in and lodgen is re-run over
# them.
#
# The BEFORE root is IMPOSTORFIX3's own `fixture/` tree, left where it is: it
# already holds the 8-ring-cliff sheets and its own `textures/`, and
# `registerLooseSheets` walks UP to the nearest ancestor holding `textures/`,
# so two roots are required and this is one of them.
#
# THE ROCK NEEDS `--no-trees-only`. IMPOSTORFIX3's rebake_all.sh did not pass
# it and the rock's lodgen_repair.log is one line: "no LOD-bearing refs in
# chunk". Every subject's log is checked for the "height repaired" census line
# below, so a silent no-op cannot pass as a bake.
set -u
ROOT="E:/Projects/NifskopeWildWastelandEdition"
NS="$ROOT/release/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
SRC="$ROOT/scratchpad/impostorfix3_20260919/fixture"
MINE="$ROOT/scratchpad/impostorfix5_20260919/fixture"

for t in blast_n4 blast_n8 maple_n4 dead_n4 rock_n4; do
	O="$MINE/$t"
	if [ ! -d "$O/cards" ]; then
		mkdir -p "$O/cards"
		cp "$SRC/$t/cards/"*.png "$O/cards/" 2>/dev/null
		cp "$SRC/$t/cards/"*.txt "$O/cards/" 2>/dev/null
	fi
	rm -f "$O/cards/"*.DDS "$O/cards/"*_oct.lodm
	EXTRA=""
	case "$t" in rock_n4) EXTRA="--no-trees-only" ;; esac
	DIM=16
	echo "== $t  (extra: ${EXTRA:-none})"
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects -32 16 --dim "$DIM" --no-ao $EXTRA \
		--impostors "$O/cards" --data-root "$DATA" -o "$O/chunk.bto" \
		>"$O/lodgen_repair.log" 2>&1
	echo "  lodgen rc=$?"
	grep -E "height repaired" "$O/lodgen_repair.log" | head -2 \
		|| echo "  !! NO height-repair census line -- this bake did nothing"
	TEX="$O/textures/data/fo4cslod/cards"
	mkdir -p "$TEX"
	rm -f "$TEX"/*.dds
	for f in "$O/cards/"*.DDS; do
		[ -e "$f" ] || continue
		cp "$f" "$TEX/$(basename "$f" | tr 'A-Z' 'a-z')"
	done
	echo "  sheets: $(ls "$TEX" | wc -l)"
done
