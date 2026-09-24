#!/usr/bin/env bash
# INCR1 step 8 -- picture 2, REDONE on a region big enough to show the point.
#
# The first attempt framed 4 chunks. A one-cell edit dirties its own chunk and
# THE WIDENING dirties the ring around it, which on a 2x2 region is every other
# chunk -- so the census honestly read "4 of 4 dirty, 0 replayed" and the
# picture showed the widening rather than the cache. Sixteen chunks leave a
# clean interior, so the same edit shows both halves: what must be rebuilt, and
# what speaks from .lodj instead.
#
# Picture 1 is NOT re-made here; it is already the full-vs-incremental pair.
# ONE GUI at a time, second monitor, game check before every launch.
set -u
R="/e/Projects/NifskopeWildWastelandEdition"
D="$R/scratchpad/incr1_20260917"
IMG="$D/images"
W="$D/work/pics2"
EXE="${EXE:-$R/release/NifSkope.exe}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
ESM0="${ESM0:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
PY="${PY:-python}"
REGION="-24 16 -9 31"
DIM=4
EDIT_X=-22; EDIT_Y=18

gamecheck () {
	if tasklist 2>/dev/null | grep -qi "Fallout4.exe"; then
		echo "REFUSED: Fallout4.exe is up"; exit 3
	fi
}
win () { ( cd "$1" && { pwd -W 2>/dev/null || pwd; } ); }
winf () { echo "$(win "$(dirname "$1")")/$(basename "$1")"; }

gamecheck
[ -x "$EXE" ] || { echo "no exe at $EXE"; exit 2; }
mkdir -p "$IMG"
rm -rf "$W"; mkdir -p "$W/out/tex" "$W/nat"
OUT="$(win "$W/out")"; NAT="$(win "$W/nat")"

echo "== copying the plugin so it can be edited in place"
cp "$ESM0" "$W/Fallout4.esm" || exit 2
ESM="$(winf "$W/Fallout4.esm")"

bake () {   # bake <logname> [extra args...]
	local name="$1"; shift
	gamecheck
	# shellcheck disable=SC2086
	"$EXE" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region $REGION --dim $DIM --data-root "$DATA" \
		--out-dir "$OUT" --tex-dir "$OUT/tex" --native "$NAT" \
		--cover --roads --road-detail 1 "$@" > "$W/$name.log" 2>&1
	local rc=$?
	echo "   [$name] rc=$rc"
	return $rc
}

echo "== A: the full bake (16 chunks)"
bake A_full || { echo "the full bake failed"; tail -5 "$W/A_full.log"; exit 1; }
echo "   .lodj on disk: $(find "$W/nat" -name '*.lodj' | wc -l)"

echo "== B: edit ONE cell of the plugin, then an incremental"
"$PY" "$R/scratchpad/land1_20260912/b_esmedit.py" height \
	"$W/Fallout4.esm" "$W/edited.esm" $EDIT_X $EDIT_Y 40 > "$W/edit.log" 2>&1
sed 's/^/   /' "$W/edit.log"
mv -f "$W/edited.esm" "$W/Fallout4.esm"
bake B_edit --incremental "$OUT"
grep -E "^incremental:|^native cache:|^  \(" "$W/B_edit.log" | head -14 > "$W/census.txt"
sed 's/^/   /' "$W/census.txt"

"$PY" - "$D" "$W" "$IMG/incremental_census.png" "$EDIT_X,$EDIT_Y" <<'PYEOF'
import sys
sys.path.insert(0, sys.argv[1])
import pics_compose
pics_compose.picture_two(sys.argv[2], sys.argv[3], sys.argv[4])
PYEOF
echo "== done"
ls -l "$IMG"
