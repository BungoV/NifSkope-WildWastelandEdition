#!/bin/bash
#
# Gate F2 and gate F5, the bakes.
#
# Four bakes of the SAME one-chunk region, everything absolute (the exe resolves
# a relative output path against release/, not against the shell's cwd):
#
#   rung     the rung exe, no new flags          -- the floor
#   off      the new exe, no new flags           -- must be `cmp`-equal to rung
#   vanfmt   the new exe, --sheet-format vanilla
#   cache    the new exe, --msn-cache <bungo's cleaned 2K output>
#
# The cache directory is READ-ONLY input and is never written to.
# --road-detail 1 on every bake, per the standing rule.
set -u
ROOT=E:/Projects/NifskopeWildWastelandEdition
B=$ROOT/scratchpad/terrainfmt1_20260912/bake
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
CACHE=E:/Tools/Upscale/esrgan-bat/output
X0=-20; Y0=24; X1=-17; Y1=27

bake() {   # bake <exe> <name> <extra...>
	local exe="$1" name="$2"; shift 2
	rm -rf "$B/$name"
	mkdir -p "$B/$name/obj" "$B/$name/mod" "$B/$name/tex"
	"$exe" -no-gui lodgen "$ESM" --worldspace 3C \
		--terrain-region $X0 $Y0 $X1 $Y1 --dim 4 \
		--out-dir "$B/$name/obj" --data-root "$DATA" \
		--vt "$B/$name/mod" --tex-dir "$B/$name/tex" --cover \
		--road-detail 1 "$@" > "$B/$name.log" 2>&1
	local rc=$?
	echo "$name: rc=$rc  $(grep -ac . "$B/$name.log") log lines"
	return $rc
}

mkdir -p "$B"
bake "$ROOT/release/NifSkope.before_terrainfmt1.exe" rung    || exit 1
bake "$ROOT/release/NifSkope.exe"                    off     || exit 1
bake "$ROOT/release/NifSkope.exe"                    vanfmt  --sheet-format vanilla || exit 1
bake "$ROOT/release/NifSkope.exe"                    cache   --msn-cache "$CACHE"   || exit 1
# Two more for the RENDER proof (item 3). The shipped default copies vanilla's
# `_msn` byte for byte on a Commonwealth chunk, so the chunk that reads
# blue-purple in ROADS1's picture cannot be produced by the default any more:
# `--land-detail-source none` is the value that keeps OUR normal bake, which is
# the sheet the defect was attributed to. Same flag on both, so the only
# difference between the pair is the container.
bake "$ROOT/release/NifSkope.exe" ourleg --land-detail-source none || exit 1
bake "$ROOT/release/NifSkope.exe" ourvan --land-detail-source none --sheet-format vanilla || exit 1
echo DONE
