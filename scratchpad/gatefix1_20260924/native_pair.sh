#!/bin/bash
# GATEFIX1: bake lodgen_btofree.sh's FO4CS region with two rungs, keep the .lodo/.lodi,
# and print their sizes plus the decoder census diff.  usage: native_pair.sh <rungA> <rungB>
set -u
W=/e/Projects/NifskopeWWE-gatefix1
L=$W/scratchpad/gatefix1_20260924
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
P=$L/npairs; mkdir -p $P
one() {
	local f="$1" o="$P/$1" extra
	[ -d "$o/nat" ] && [ -n "$(find "$o/nat" -name Commonwealth.lodi)" ] && return
	local ow; mkdir -p "$o"; ow="$(cd "$o" && { pwd -W 2>/dev/null || pwd; })"
	for extra in "--keep-bto --library near --native-ladder" "--keep-bto" ""; do
		rm -rf "$o"/*; mkdir -p "$o/tex" "$o/nat"
		# shellcheck disable=SC2086
		"$W/release/NifSkope.$f.exe" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -19 25 --dim 4 \
			--data-root "$DATA" --out-dir "$ow" --tex-dir "$ow/tex" --native "$ow/nat" \
			--cover --arrays --road-detail 1 $extra > "$o.log" 2>&1
		grep -q "unknown option" "$o.log" || break
	done
	find "$o" -type f ! -path "*/nat/*" ! -name '*.lodb' -delete 2>/dev/null
}
one "$1"; one "$2"
for n in lodo lodi; do
	a=$(find "$P/$1/nat" -name "Commonwealth.$n"); b=$(find "$P/$2/nat" -name "Commonwealth.$n")
	echo "$n: $(stat -c%s "$a") -> $(stat -c%s "$b") $(cmp -s "$a" "$b" && echo same || echo DIFFER)"
done
dec() { python $W/tests/spells/lodgen_native_decode.py "$(find "$P/$1/nat" -name Commonwealth.lodo)" "$(find "$P/$1/nat" -name Commonwealth.lodi)" 2>&1; }
diff <(dec "$1") <(dec "$2") | head -40
