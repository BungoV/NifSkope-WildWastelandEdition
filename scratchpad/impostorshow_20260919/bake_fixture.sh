#!/bin/sh
# Bake ONE impostor set into a PERSISTENT directory, so the in-application half
# of tests/spells/impostor_draw.sh has something to open. The bake gate does
# exactly this and then deletes it (`trap rm -rf "$W" EXIT`), which is why this
# script exists rather than a flag on the gate.
#
#   sh scratchpad/impostorshow_20260919/bake_fixture.sh <FORMID> <Model\Path.nif> <N> <TILE> <outdir>
#
# Leaves in <outdir>:
#   bake/   the raw PNG sheets + <base>.txt sidecar the viewer bake writes
#   cards/  the same sheets filed under the form ID, then compressed to DDS by
#           the lodgen run, with <id>_oct.lodm beside them
#   chunk.bto(.manifest.txt)  the native chunk whose C line places the set
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NS="${EXE:-$ROOT/release/NifSkope.exe}"
ESM="${ESM:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
. "$ROOT/tests/spells/_harness.sh"

FORMID="$1"; MODEL="$2"; N="$3"; TILE="$4"; OUT="$5"
# `winpath` only rewrites /x/... -- a RELATIVE out-dir reaches the exe unchanged
# and is resolved against ITS cwd, which is how the first run wrote its sheets
# nowhere and said nothing. Absolute or refuse.
mkdir -p "$OUT" && OUT="$(cd "$OUT" && pwd)"
case "$OUT" in /[a-zA-Z]/*) ;; *) echo "REFUSED: out-dir does not absolutise to a drive: $OUT"; exit 2 ;; esac
PORT="${PORT:-45931}"
ID="$(echo "$FORMID" | tr 'A-F' 'a-f')"
MESH="$DATA/meshes/$(echo "$MODEL" | tr '\\' '/')"
BASE="$(basename "$(echo "$MODEL" | tr '\\' '/')" .nif | tr 'A-Z' 'a-z')"

[ -x "$NS" ] || { echo "no NifSkope.exe at $NS"; exit 2; }
[ -f "$MESH" ] || { echo "no mesh at $MESH"; exit 2; }

rm -rf "$OUT/bake" "$OUT/cards"
mkdir -p "$OUT/bake" "$OUT/cards"
echo "bake: $BASE  N=$N tile=$TILE  -> $OUT"
WW_IMPOSTOR_BAKE="$(winpath "$OUT/bake")" WW_IMPOSTOR_OCT="$N" WW_IMPOSTOR_TILE="$TILE" \
	timeout 600 "$NS" "$(winpath "$MESH")" --port "$PORT" >"$OUT/bake.log" 2>&1
ls -l "$OUT/bake" | tail -8
[ -s "$OUT/bake/${BASE}_oct_albedo.png" ] || { echo "REFUSED: the bake wrote no albedo sheet; see $OUT/bake.log"; exit 1; }

for f in "$OUT/bake/${BASE}"*; do
	n="$(basename "$f")"; cp "$f" "$OUT/cards/${ID}${n#$BASE}"
done

# The compression + `.lodm` step only runs for a reference the chunk actually
# places on a card, so the chunk has to be one that holds this object.
REGION="${REGION:--32 16}"
echo "lodgen: --objects $REGION --dim ${DIM:-16}"
"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects $REGION --dim "${DIM:-16}" --no-ao \
	--impostors "$(winpath "$OUT/cards")" --data-root "$DATA" -o "$(winpath "$OUT/chunk.bto")" \
	>"$OUT/lodgen.log" 2>&1
echo "lodgen rc=$?"
# THE SHEETS HAVE TO BE REACHABLE. The texture cache resolves a name through
# `GameManager::get_full_path`, which lowercases it and forces a `textures/`
# prefix; it has no absolute-path route, so the loose DDS beside the .lodm can
# never bind by its own path. The `.lodm` names them as
# `Data\FO4CSLOD\Cards\<id>_oct_d.DDS`, which becomes
# `textures/data/fo4cslod/cards/<id>_oct_d.dds` -- so lay that tree out beside
# the cards and let ImpostorDraw::registerLooseSheets put $OUT on the resource
# list (it walks up from the .lodm looking for an ancestor holding `textures`).
TEX="$OUT/textures/data/fo4cslod/cards"
mkdir -p "$TEX"
for f in "$OUT/cards/"*.DDS; do
	[ -e "$f" ] || continue
	cp "$f" "$TEX/$(basename "$f" | tr 'A-Z' 'a-z')"
done
echo "  resource tree: $TEX ($(ls "$TEX" | wc -l) sheets)"

grep -c "^C " "$OUT/chunk.bto.manifest.txt" 2>/dev/null | sed 's/^/  C lines: /'
grep "^C .*${ID}_oct\.lodm$" "$OUT/chunk.bto.manifest.txt" 2>/dev/null | head -2
ls -l "$OUT/cards" | grep -i -E "\.DDS|\.lodm"
