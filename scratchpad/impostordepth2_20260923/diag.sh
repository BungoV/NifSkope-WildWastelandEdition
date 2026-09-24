#!/bin/bash
# IMPOSTORDEPTH2 crisp-end discriminator: which input thins the snap's trunk at el 0?
# Same orbit + bar as tests/spells/impostor_trunk.sh, new exe, coverage channel.
. /e/Projects/NifskopeWildWastelandEdition/tests/spells/_harness.sh
ROOT=/e/Projects/NifskopeWildWastelandEdition
L=$ROOT/scratchpad/impostordepth2_20260923
PY=/c/Users/bungo/AppData/Local/Programs/Python/Python39/python
MESH="E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Landscape/Trees/TreeMapleInstitute06Green.nif"
W=$L/diag; mkdir -p $W
PORT=46400
orbit() { # orbit <name> <lodm> <el> [ENV...]
	local d="$W/$1" lodm="$2" el="$3"; shift 3
	rm -rf "$d"; mkdir -p "$d"
	V=$($PY -c "print(','.join('%d:$el' % a for a in range(360)))")
	for try in 1 2 3; do
		PORT=$((PORT+1))
		env "$@" WW_IMPOSTOR_PREVIEW=orbit WW_IMPOSTOR_LODM="$(winpath "$lodm")" \
			WW_IMPOSTOR_LOG="$(winpath "$d.log")" WW_IMPOSTOR_SHOT="$(winpath "$d/v")" \
			WW_IMPOSTOR_ORBIT_VIEWS="$V" WW_RENDER_CLEAN=1 WW_RENDER_SIZE=1000x1000 \
			WW_IMPOSTOR_CHANNEL=2 WW_IMPOSTOR_MESH_CHANNEL=8 \
			timeout 3000 $ROOT/release/NifSkope.exe "$MESH" --port "$PORT" >/dev/null 2>&1
		grep -q "orbit counted 0 of" "$d.log" 2>/dev/null || break
	done
	REF_POP=0.117029 REF_TEAR=0.0096148 $PY $ROOT/tests/spells/impostor_trunkbar.py $1=$d@$el 2>&1 | grep -E "T1|T2|T3|TRUNK|whole"
}
BC7=$L/n8_2k_bc7/cards/000531b3_oct.lodm
RAW=$ROOT/scratchpad/impostordepth1_20260923/bakes_raw/n8_2k/cards/000531b3_oct.lodm
DXT=$ROOT/scratchpad/impostor16_20260923/n8/bakes/n8_2k/cards/000531b3_oct.lodm
for spec in "$@"; do
	IFS=: read name set el env <<< "$spec"
	case $set in bc7) S=$BC7;; raw) S=$RAW;; dxt) S=$DXT;; esac
	echo "== $name ($set, el $el, ${env:-default})"
	orbit $name $S $el $env
	rm -rf "$W/$name"
done
echo DIAG-DONE
