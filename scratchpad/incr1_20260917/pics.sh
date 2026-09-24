#!/usr/bin/env bash
# INCR1 step 8 -- the two pictures.
#
#   1. native_full_vs_incremental.png -- the SAME cells of the FO4CS object
#      library, rendered from the pair a FULL bake wrote and from the pair a
#      NULL INCREMENTAL rewrote, same camera, side by side. The point of the
#      picture is that there is nothing to see: the two halves are the same
#      image because the two pairs are the same bytes, and the sha1s are
#      printed under them so the claim is checkable and not aesthetic.
#
#   2. incremental_census.png -- the census an --incremental run prints after
#      ONE cell of the plugin is edited: which chunks are dirty, why, and how
#      many spoke from their .lodj cache instead of being rebaked.
#
# The plugin is EDITED IN PLACE in a copy, and both bakes name that same copy,
# because the switch digest covers the argument vector -- passing a different
# path would move the digest and the run would refuse for the wrong reason.
#
# ONE GUI at a time, second monitor, its own port. Game check before every
# launch, as its own command.
set -u
R="/e/Projects/NifskopeWildWastelandEdition"
D="$R/scratchpad/incr1_20260917"
IMG="$D/images"
W="$D/work/pics"
EXE="${EXE:-$R/release/NifSkope.exe}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
ESM0="${ESM0:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
PY="${PY:-python}"
PORT="${PORT:-42977}"
REGION="-24 16 -17 23"
DIM=4
CX=-24; CY=16          # the chunk the picture frames
EDIT_X=-22; EDIT_Y=18  # a cell inside it
WS=Commonwealth

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

# ---------------------------------------------------------------- picture 1
echo "== A: the full bake"
bake A_full || { echo "the full bake failed"; tail -5 "$W/A_full.log"; exit 1; }
mkdir -p "$W/nat_full"; cp -pr "$W/nat/." "$W/nat_full/"
SHA_FULL="$(sha1sum "$W/nat_full/FO4CSLOD/$WS/$WS.lodi" | cut -c1-40)"

echo "== B: the null incremental"
bake B_null --incremental "$OUT" || { echo "the incremental failed"; tail -5 "$W/B_null.log"; exit 1; }
mkdir -p "$W/nat_incr"; cp -pr "$W/nat/." "$W/nat_incr/"
SHA_INCR="$(sha1sum "$W/nat_incr/FO4CSLOD/$WS/$WS.lodi" | cut -c1-40)"
echo "   .lodi full=$SHA_FULL  incremental=$SHA_INCR"

CENTER="$(( (CX * 4096 + (CX + DIM) * 4096) / 2 )),$(( (CY * 4096 + (CY + DIM) * 4096) / 2 )),0"
ORTHO=$(( DIM * 4096 / 2 ))
SIZE="${SIZE:-1024x1024}"

shot () {   # shot <out.png> <lodi>
	local out="$1" lodi="$2"
	gamecheck
	rm -f "$out"
	WW_WINDOW_AT=1960,40 \
	WW_RENDER_SHOT="$(winf "$out")" WW_RENDER_SIZE="$SIZE" WW_RENDER_VIEW=1 \
	WW_RENDER_CENTER="$CENTER" WW_RENDER_ORTHO="$ORTHO" WW_RENDER_CLEAN=1 \
	WW_LODI_REGION="$CX,$CY,$((CX+DIM-1)),$((CY+DIM-1))" \
	WW_LODGEN_RESOURCES="$DATA" \
		timeout 300 "$EXE" --port "$PORT" "$(winf "$lodi")" >/dev/null 2>&1
	[ -s "$out" ] && echo "   shot $(basename "$out") $(stat -c%s "$out") B" \
		|| echo "   NO SHOT for $(basename "$out")"
}

shot "$W/full.png" "$W/nat_full/FO4CSLOD/$WS/$WS.lodi"
shot "$W/incr.png" "$W/nat_incr/FO4CSLOD/$WS/$WS.lodi"

# ---------------------------------------------------------------- picture 2
echo "== C: edit ONE cell of the plugin, then an incremental"
"$PY" "$R/scratchpad/land1_20260912/b_esmedit.py" height \
	"$W/Fallout4.esm" "$W/edited.esm" $EDIT_X $EDIT_Y 40 > "$W/edit.log" 2>&1
rc=$?
cat "$W/edit.log" | sed 's/^/   /'
if [ $rc -ne 0 ]; then echo "   the plugin edit failed"; fi
mv -f "$W/edited.esm" "$W/Fallout4.esm"
bake C_edit --incremental "$OUT"
grep -E "^incremental:|^native cache:|^  \(" "$W/C_edit.log" | head -14 > "$W/census.txt"
cat "$W/census.txt" | sed 's/^/   /'

"$PY" "$D/pics_compose.py" "$W" "$IMG" "$SHA_FULL" "$SHA_INCR" \
	"$EDIT_X,$EDIT_Y"
echo "== done"
ls -l "$IMG"
