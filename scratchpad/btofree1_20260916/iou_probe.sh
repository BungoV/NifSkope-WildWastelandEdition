#!/bin/bash
# Lane BTOFREE1, 2026-09-16 -- reproduce native_open.sh check (c)'s two renders
# OUTSIDE the harness, so the pixels survive for a picture and a diagnosis.
#
# Same camera, same size, same env as tests/spells/native_open.sh lines 105-300.
set -u
R="E:/Projects/NifskopeWildWastelandEdition"
L="$R/scratchpad/btofree1_20260916"
NS="${NS:-$R/scratchpad/showcase1_20260912/ns_run/NifSkope.exe}"
SC="$R/scratchpad/showcase1_20260912/out"
NATIVE="${NATIVE:-$SC/look/native}"
OBJ="${OBJ:-$SC/look/obj}"
RES="$R/scratchpad/nativeview1_20260912/resroot"
WS=Commonwealth
CX=-20; CY=24; DIM=4
OUT="${OUT:-$L/iou}"
PORT="${PORT:-42933}"
mkdir -p "$OUT"

if tasklist 2>/dev/null | grep -qi "Fallout4.exe"; then echo "REFUSED: game is up"; exit 2; fi

CENTER="$(( (CX * 4096 + (CX + DIM) * 4096) / 2 )),$(( (CY * 4096 + (CY + DIM) * 4096) / 2 )),0"
ORTHO=$(( DIM * 4096 / 2 ))
SIZE=1024x1024
echo "centre $CENTER ortho $ORTHO"

shot() {
	local out="$1" file="$2"; shift 2
	rm -f "$out"
	env "$@" WW_RENDER_SHOT="$out" WW_RENDER_SIZE="$SIZE" WW_RENDER_VIEW=1 \
		WW_RENDER_CENTER="$CENTER" WW_RENDER_ORTHO="$ORTHO" WW_RENDER_CLEAN=1 \
		timeout 300 "$NS" --port "$PORT" "$file" >/dev/null 2>&1
	[ -s "$out" ] && echo "  wrote $(basename "$out") $(stat -c%s "$out") bytes" || echo "  FAILED $out"
}

echo "lodi:"
shot "$OUT/lodi.png" "$NATIVE/$WS.lodi" WW_LODI_REGION="$CX,$CY,$((CX+DIM-1)),$((CY+DIM-1))"
echo "bto:"
shot "$OUT/bto.png" "$OBJ/$WS.$DIM.$CX.$CY.BTO" WW_LODGEN_RESOURCES="$RES;$OBJ"
