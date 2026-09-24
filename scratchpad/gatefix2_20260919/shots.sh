#!/usr/bin/env bash
# GATEFIX2 -- render the calibration arms with native_lighting.sh's own framing.
#   bash shots.sh <port> <outdir> <arm>=<cache> [<arm>=<cache> ...]
# Every switch below is copied from tests/spells/native_lighting.sh so the
# frames are comparable with the gate's own.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
EXE="$ROOT/release/NifSkope.exe"
PORT="$1"; OUT="$2"; shift 2
S="$ROOT/scratchpad/showcase1_20260912/out"
R1="$ROOT/scratchpad/nativeview1_20260912"
LODL="$S/lodl/Terrain/Commonwealth.lodl"
SHEETS="$S/look/mod/Terrain"
OBJ="$S/look/obj"
winpath() { case "$1" in /[a-zA-Z]/*) printf '%s' "${1:1:1}:${1:2}" ;; *) printf '%s' "$1" ;; esac; }
RES="$(winpath "$R1/resroot");$(winpath "$OBJ")"
mkdir -p "$OUT"
for spec in "$@"; do
	arm="${spec%%=*}"; cache="${spec#*=}"
	for v in 1 8; do
		if [ "$v" = 1 ]; then n="t_${arm}_top"; c="-73728,106496,0"; else n="t_${arm}_obl"; c="-73728,106496,8500"; fi
		env WW_WINDOW_AT=1960,40 \
			WW_RENDER_SHOT="$OUT/$n.png" WW_RENDER_SIZE=1024x1024 \
			WW_RENDER_VIEW="$v" WW_RENDER_CLEAN=1 \
			WW_RENDER_CENTER="$c" WW_RENDER_ORTHO=8192 \
			WW_LODL_REGION="-20,24,-17,27,2" \
			WW_LODL_SHEETS="$SHEETS" \
			WW_LODL_SHEET_CACHE="$cache" \
			WW_PROGRAM_CENSUS="$OUT/$n.census.txt" \
			WW_LODGEN_RESOURCES="$RES" \
			timeout 300 "$EXE" --port "$PORT" "$LODL" >/dev/null 2>&1
		[ -s "$OUT/$n.png" ] || echo "  render $n produced nothing"
	done
	echo "  $arm done: $(sha1sum "$OUT/t_${arm}_obl.png" | cut -c1-12)"
done
