#!/bin/bash
# GATEFIX1: native_open.sh leg (d) and (c) shots, kept on disk, for one exe.
# usage: d_shots.sh <exe> <outdir> [legs: c d]
set -u
W=/e/Projects/NifskopeWWE-gatefix1
. "$W/tests/spells/_harness.sh"
NS="$1"; O="$2"; LEGS="${3:-d}"
M=/e/Projects/NifskopeWildWastelandEdition
SC=$M/scratchpad/showcase1_20260912/out
LODL="${LODL:-$SC/lodl/Terrain/Commonwealth.lodl}"
NATIVE="${NATIVE:-$SC/look/native}"
SHEETS="${SHEETS:-$SC/look/mod/Terrain}"
OBJ="${OBJ:-$SC/look/obj}"
RES="${RES:-$M/scratchpad/nativeview1_20260912/resroot}"
PORT="${PORT:-47311}"
AUTH=$W/tests/spells/native_open_authority.py
PY="${PY:-$(command -v python)}"
CX=-20; CY=24; DIM=4; OX=-16; OY=24
CENTER="$(( (CX * 4096 + (CX + DIM) * 4096) / 2 )),$(( (CY * 4096 + (CY + DIM) * 4096) / 2 )),0"
ORTHO=$(( DIM * 4096 / 2 ))
LCENTER="$(( DIM * 4096 / 2 )),$(( DIM * 4096 / 2 )),0"
SIZE=1024x1024
mkdir -p "$O"
shot() {
	local out="$1" file="$2"; shift 2
	local ctr="${SHOT_CENTER:-$CENTER}"
	rm -f "$out"
	env "$@" WW_RENDER_SHOT="$(winpath "$out")" WW_RENDER_SIZE="$SIZE" WW_RENDER_VIEW=1 \
		WW_RENDER_CENTER="$ctr" WW_RENDER_ORTHO="$ORTHO" WW_RENDER_CLEAN=1 \
		timeout 300 "$NS" --port "$PORT" "$(winpath "$file")" >/dev/null 2>&1
	[ -s "$out" ]
}
case " $LEGS " in *" d "*)
	shot "$O/d_lit.png" "$LODL" WW_LODL_REGION="$CX,$CY,$((CX+DIM-1)),$((CY+DIM-1)),2" WW_LODL_SHEETS="$(winpath "$SHEETS")"
	SHOT_CENTER="$LCENTER" shot "$O/d_btr.png" "$OBJ/Commonwealth.4.$CX.$CY.BTR" WW_LODGEN_RESOURCES="$(winpath "$RES")"
	SHOT_CENTER="$LCENTER" shot "$O/d_btr_other.png" "$OBJ/Commonwealth.4.$OX.$OY.BTR" WW_LODGEN_RESOURCES="$(winpath "$RES");$(winpath "$OBJ")"
	shot "$O/d_data.png" "$LODL" WW_LODL_REGION="$CX,$CY,$((CX+DIM-1)),$((CY+DIM-1)),2"
	echo "same: $("$PY" "$AUTH" ncc "$O/d_lit.png" "$O/d_btr.png" | tr '\n' ' ')"
	echo "other: $("$PY" "$AUTH" ncc "$O/d_lit.png" "$O/d_btr_other.png" | tr '\n' ' ')"
	echo "data-vs-btr: $("$PY" "$AUTH" ncc "$O/d_data.png" "$O/d_btr.png" | tr '\n' ' ')"
;; esac
case " $LEGS " in *" c "*)
	shot "$O/b.png" "$NATIVE/Commonwealth.lodi" WW_LODI_REGION="$CX,$CY,$((CX+DIM-1)),$((CY+DIM-1))" \
		WW_LODGEN_RESOURCES="$(winpath "$RES");$(winpath "$OBJ")"
	shot "$O/c_bto.png" "$OBJ/Commonwealth.4.$CX.$CY.BTO" WW_LODGEN_RESOURCES="$(winpath "$RES");$(winpath "$OBJ")"
	echo "cover: $("$PY" "$AUTH" cover "$O/b.png" "$O/c_bto.png" | grep -E '^(COVER|FAT)' | tr '\n' ' ')"
;; esac
