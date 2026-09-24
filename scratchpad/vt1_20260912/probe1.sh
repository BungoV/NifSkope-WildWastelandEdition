#!/bin/bash
# VT1 probe 1: WHICH property of the guide's macro gradient makes the pyramid
# path and the direct path disagree?  Same pair of bakes as V9a-1, one pair per
# configuration, on the exe on disk.
set -u
R=E:/Projects/NifskopeWildWastelandEdition
NS=$R/release/NifSkope.exe
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
W=$R/scratchpad/vt1_20260912/probe1
CHUNKS="Commonwealth.4.-24.24 Commonwealth.4.-20.24 Commonwealth.4.-24.28 Commonwealth.4.-20.28"
mkdir -p "$W"

run() {		# run <tag> <region x0 y0 x1 y1> <switches...>
	local tag="$1"; shift
	local x0="$1" y0="$2" x1="$3" y1="$4"; shift 4
	mkdir -p "$W/$tag.vt/mod" "$W/$tag.vt/obj" "$W/$tag.vt/tex" "$W/$tag.d/obj" "$W/$tag.d/tex"
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region $x0 $y0 $x1 $y1 --dim 4 \
		--out-dir "$W/$tag.vt/obj" --data-root "$DATA" \
		--vt "$W/$tag.vt/mod" --tex-dir "$W/$tag.vt/tex" "$@" > "$W/$tag.vt.log" 2>&1
	"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region $x0 $y0 $x1 $y1 --dim 4 \
		--out-dir "$W/$tag.d/obj" --data-root "$DATA" \
		--tex-dir "$W/$tag.d/tex" "$@" > "$W/$tag.d.log" 2>&1
	local line=""
	for c in $CHUNKS; do
		local n
		n=$(cmp -l "$W/$tag.vt/tex/$c.DDS" "$W/$tag.d/tex/$c.DDS" 2>/dev/null | wc -l)
		line="$line $c=$n"
	done
	printf '%-24s%s\n' "$tag" "$line"
}
echo "tag                      bytes differing, vt-assembled vs direct, per chunk"
# the reference: the ruled default, the V9a region
run default   -24 24 -17 31
# does the guide's REACH decide the width of the band?
run gscale256 -24 24 -17 31 --land-guide-scale 256
run gscale512 -24 24 -17 31 --land-guide-scale 512
run gscale2048 -24 24 -17 31 --land-guide-scale 2048
# does the REGION's own extent decide it?  same four chunks, region grown by
# one cell on every side, and by four cells north
run grow1     -25 23 -16 32
run grownorth -24 24 -17 35
echo "done $(date +%H:%M:%S)"
