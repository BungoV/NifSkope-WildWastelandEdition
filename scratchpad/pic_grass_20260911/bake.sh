#!/bin/bash
# PIC-GRASS: the two headless bakes behind cmp_grass_tint.png.
# ONE NifSkope instance ever -- the two runs are sequential and each is gated
# on the process list being empty first. Small region only: the single dim-4
# chunk (-20,20), into this lane's own out-dir. Never his installed Data.
set -u
ROOT=/e/Projects/NifskopeWildWastelandEdition
LANE="$ROOT/scratchpad/pic_grass_20260911"
NS="$ROOT/release/NifSkope.exe"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"

gate() {
	if tasklist | grep -i -E "Fallout4|NifSkope" ; then
		echo "REFUSED: a Fallout4 or NifSkope process is up"; exit 2
	fi
	echo "gate ok (rc=1, no Fallout4/NifSkope)"
}

bake() {   # bake <name> <extra...>
	local name="$1"; shift
	mkdir -p "$LANE/out/$name/obj" "$LANE/out/$name/tex"
	gate
	echo "== bake $name : $* =="
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region -20 20 -17 23 --dim 4 \
		--out-dir "$LANE/out/$name/obj" --tex-dir "$LANE/out/$name/tex" \
		--data-root "$DATA" "$@" > "$LANE/logs/$name.log" 2>&1
	echo "rc=$?"
	ls -l "$LANE/out/$name/tex"
}

ls -l --time-style=+%Y-%m-%d\ %H:%M:%S "$NS"
bake default --cover
bake tint0   --cover --grass-tint 0
gate
echo BAKES-DONE
