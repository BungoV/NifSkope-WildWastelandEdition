#!/bin/sh
# ---------------------------------------------------------------------------
# bake_time.sh -- how long a set takes to bake, per N, from a named log.
#
# Step 4 of the brief asks for bake seconds beside the pictures. This times the
# SHEET BAKE only -- the `WW_IMPOSTOR_BAKE` run that renders N x N frames out of
# the mesh -- and says so, because the other half of a set's cost (the BC
# compression and the `.lodm`, done inside lodgen) runs once for a whole card
# library and is not a per-set number.
#
# The clock is `date +%s` either side of the run, read in this shell. Nothing
# here is inferred from a file's mtime: an mtime says when a file was closed,
# not how long the work took, and three sets written a second apart would read
# as three one-second bakes.
#
#   sh bake_time.sh <FORMID> <Model\Path.nif> "<N list>" <TILE>
# ---------------------------------------------------------------------------
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
R="$ROOT/scratchpad/impostorshow_20260919"
NS="$ROOT/release/NifSkope.exe"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"

FORMID="$1"; MODEL="$2"; NS_LIST="$3"; TILE="${4:-128}"
MESH="$DATA/meshes/$(echo "$MODEL" | tr '\\' '/')"
BASE="$(basename "$(echo "$MODEL" | tr '\\' '/')" .nif | tr 'A-Z' 'a-z')"
LOG="$R/bake_seconds.log"
PORT="${PORT:-27861}"

[ -f "$MESH" ] || { echo "no mesh at $MESH"; exit 2; }
{
	echo "bake_seconds  $( date '+%Y-%m-%d %H:%M:%S' )"
	echo "exe $NS  $( stat -c %s "$NS" ) bytes"
	echo "subject $FORMID $MODEL tile $TILE"
} >> "$LOG"

for N in $NS_LIST; do
	W="$R/baketime/${FORMID}_n${N}"
	rm -rf "$W"; mkdir -p "$W"
	t0=$( date +%s )
	WW_IMPOSTOR_BAKE="$(cygpath -m "$W" 2>/dev/null || echo "$W")" \
	WW_IMPOSTOR_OCT="$N" WW_IMPOSTOR_TILE="$TILE" \
	WW_WINDOW_AT=1960,40 \
	timeout 900 "$NS" "$MESH" --port "$PORT" > "$W/bake.log" 2>&1
	rc=$?
	t1=$( date +%s )
	px=$( ls -l "$W/${BASE}_oct_albedo.png" 2>/dev/null | awk '{print $5}' )
	echo "N=$N  ${TILE}px tile  rc=$rc  seconds $(( t1 - t0 ))  albedo png ${px:-none} bytes" \
		| tee -a "$LOG"
done
echo "-> $LOG"
