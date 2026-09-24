#!/bin/bash
# GATEFIX1: find every rung on which the four files lodgen_btofree.sh reports as moved
# (the native Commonwealth.lodo / .lodi, the chunk .BTO, the cover sheet tex/...DDS)
# changed, by recursive bisection over the rung order from before_btofree1 to
# before_gatefix1. Each rung does the harness's own FO4CS bake (region -20 24 -19 25,
# dim 4, --cover --arrays --road-detail 1), with --keep-bto and the pre-2026-09-17
# defaults (--library near --native-ladder) where the exe accepts them.
# Output: sw/<rung>.sig (one "sha16 name" line per file), bisect.log (MOVE lines).
set -u
W=/e/Projects/NifskopeWWE-gatefix1
L=$W/scratchpad/gatefix1_20260924
D=$L/sw; mkdir -p "$D"
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
S=/tmp/gf1bisect; mkdir -p $S; SW="$(cd $S && { pwd -W 2>/dev/null || pwd; })"
KIND="${KIND:-native}"
sig() { # sig <rung-exe-name> -> prints the signature file path, baking once
	local f="$1" n extra o rc
	n=${f%.exe}; n=${n#NifSkope.}
	if [ ! -s "$D/$n.$KIND.sig" ]; then
		o="$S/$n.$KIND"
		for extra in "--keep-bto --library near --native-ladder" "--keep-bto" "--library near --native-ladder" ""; do
			[ "$KIND" = stock ] && [ -n "$extra" ] && continue
			rm -rf "$o"; mkdir -p "$o/tex" "$o/nat"
			local nat=""; [ "$KIND" = native ] && nat="--native $SW/$n.$KIND/nat"
			# shellcheck disable=SC2086
			"$W/release/$f" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -19 25 --dim 4 \
				--data-root "$DATA" --out-dir "$SW/$n.$KIND" --tex-dir "$SW/$n.$KIND/tex" $nat \
				--cover --arrays --road-detail 1 $extra > "$S/$n.$KIND.log" 2>&1
			rc=$?
			grep -q "unknown option" "$S/$n.$KIND.log" || break
		done
		( cd "$o" && find . -type f \( -name 'Commonwealth.lodo' -o -name 'Commonwealth.lodi' \
			-o -name 'Commonwealth.4.-20.24.BTO' -o -name 'Commonwealth.4.-20.24.DDS' \) -exec sha256sum {} + \
			| awk '{n=$2; sub(/.*\//,"",n); print substr($1,1,16), n}' | sort -k2 ) > "$D/$n.$KIND.sig"
		echo "# rc=$rc flags=[$extra]" >> "$D/$n.$KIND.sig"
		echo "baked $n $KIND rc=$rc flags=[$extra] $(date +%H:%M:%S)" >&2
		rm -rf "$o"
	fi
	echo "$D/$n.$KIND.sig"
}
same() { diff -q <(grep -v '^#' "$(sig "$1")") <(grep -v '^#' "$(sig "$2")") > /dev/null; }
mapfile -t R < <(awk '/before_btofree1/{f=1} f' $L/rung_order.txt; echo NifSkope.before_gatefix1.exe)
bis() { # bis lo hi (indices); both ends' signatures differ
	local lo=$1 hi=$2
	if [ $((hi - lo)) -le 1 ]; then
		echo "MOVE ${R[$lo]} -> ${R[$hi]}: $(diff <(grep -v '^#' "$(sig "${R[$lo]}")") <(grep -v '^#' "$(sig "${R[$hi]}")") | grep '^>' | awk '{print $3}' | tr '\n' ' ')"
		return
	fi
	local mid=$(( (lo + hi) / 2 ))
	same "${R[$lo]}" "${R[$mid]}" || bis $lo $mid
	same "${R[$mid]}" "${R[$hi]}" || bis $mid $hi
}
last=$(( ${#R[@]} - 1 ))
same "${R[0]}" "${R[$last]}" && echo "no move at all" || bis 0 $last
echo BISECT-DONE
