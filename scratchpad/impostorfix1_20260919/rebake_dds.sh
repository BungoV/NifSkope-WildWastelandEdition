#!/bin/sh
# Re-run ONLY the compression half of a fixture bake.
#
# `lodgenRepairOctHeight` lives in `lodgenCard()`, which reads the bake's
# `_oct_*.png` sheets and writes the `.DDS` beside them. The photography half --
# the expensive one -- produces those PNGs and is untouched by the repair, so
# re-running it would burn ten minutes per fixture to write the same bytes.
# This script re-runs the lodgen step over the PNGs already in `cards/` and
# re-lays the `textures/` tree the viewer resolves the sheets through.
set -u
ROOT="E:/Projects/NifskopeWildWastelandEdition"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"

OUT="$1"
[ -d "$OUT/cards" ] || { echo "no cards/ under $OUT"; exit 2; }
# keep the shipped sheets as the BEFORE, once and only once
if [ ! -d "$OUT/cards_before" ]; then
	mkdir -p "$OUT/cards_before"
	cp "$OUT/cards/"*.DDS "$OUT/cards_before/" 2>/dev/null
	cp "$OUT/cards/"*.lodm "$OUT/cards_before/" 2>/dev/null
	echo "  before/ kept: $(ls "$OUT/cards_before" | wc -l) files"
fi
# `lodgenCard` writes a sheet only when the file is NOT already there ("converted
# once from the bake's PNGs"), so a re-run with the same DDS on disk applies the
# repair to the QImage and then writes nothing at all. Measured the hard way:
# six fixtures re-baked, six DDS byte-identical, the repair's own log line
# printed each time. Clear them, and the .lodm with them so the manifest cannot
# describe sheets that no longer exist.
rm -f "$OUT/cards/"*.DDS "$OUT/cards/"*_oct.lodm
REGION="${REGION:--32 16}"
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects $REGION --dim "${DIM:-16}" --no-ao ${EXTRA:-} \
	--impostors "$OUT/cards" --data-root "$DATA" -o "$OUT/chunk.bto" \
	>"$OUT/lodgen_repair.log" 2>&1
echo "  lodgen rc=$?"
grep -E "height repaired" "$OUT/lodgen_repair.log" | head -4
TEX="$OUT/textures/data/fo4cslod/cards"
mkdir -p "$TEX"
for f in "$OUT/cards/"*.DDS; do
	[ -e "$f" ] || continue
	cp "$f" "$TEX/$(basename "$f" | tr 'A-Z' 'a-z')"
done
echo "  sheets: $(ls "$TEX" | wc -l)"
