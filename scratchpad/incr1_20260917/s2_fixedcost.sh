#!/usr/bin/env bash
# INCR1 step 2 -- WHAT AN INCREMENTAL FO4CS BAKE CAN ACTUALLY SAVE.
#
# `lodgenNativeWrite()` builds the `.lodo` library from the worldspace's FULL
# base census, so it runs whole whatever the dirty set is; only the CHUNK PASS
# is skippable. The question the route has to answer first is therefore not
# "can we skip chunks" but "how much of an FO4CS bake is the chunk pass".
#
# Measured by baking the same worldspace at 1 chunk and at 9, same switches,
# same out-dir shape: the difference is 8 chunks of chunk-pass work, and what is
# left at 1 chunk is (fixed cost + one chunk).
#
#   bash s2_fixedcost.sh   ->  s2_fixedcost.txt beside this script
set -u
R="$(cd "$(dirname "$0")/../.." && pwd)"
D="$R/scratchpad/incr1_20260917"
EXE="${EXE:-$R/release/NifSkope.exe}"
DATA="${DATA:-E:/Tools/Fallout 4/DataUnpacked/Data}"
VANILLA="${VANILLA:-X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm}"
W="$D/work/s2"
SUM="$D/s2_fixedcost.txt"

if tasklist 2>/dev/null | grep -qi "Fallout4.exe"; then
	echo "REFUSED: Fallout4.exe is up"; exit 3
fi
[ -x "$EXE" ] || { echo "no exe at $EXE"; exit 2; }

nat () {   # nat <name> <region...>
	local name="$1"; shift
	local dir="$W/$name"
	rm -rf "$dir"; mkdir -p "$dir/obj" "$dir/tex" "$dir/nat"
	local da
	da="$(cd "$dir" && { pwd -W 2>/dev/null || pwd; })"
	local t0 t1
	t0=$(date +%s%N)
	# shellcheck disable=SC2086
	"$EXE" -no-gui lodgen "$VANILLA" --worldspace 3C \
		--terrain-region $* --dim 4 \
		--out-dir "$da/obj" --tex-dir "$da/tex" --data-root "$DATA" \
		--native "$da/nat" \
		--cover --roads --road-detail 1 \
		> "$dir/bake.log" 2>&1
	local rc=$?
	t1=$(date +%s%N)
	echo "== $name  region '$*'  rc=$rc  $(( (t1-t0)/1000000 )) ms"
	grep -E "^stage times:|^bake census:|^\.lodo|^\.lodi|lodo |lodi " "$dir/bake.log" \
		| head -8 | sed 's/^/    /'
	grep -oE "Commonwealth\.lodi [0-9]+ bytes \(instances [0-9]+ from [0-9]+ arrivals over [0-9]+ census refs" \
		"$dir/bake.log" | sed 's/^/    /'
}

{
echo "INCR1 step 2 -- the fixed cost of an FO4CS bake"
date
ls -l "$EXE" | sed 's/^/   /'
echo
nat one   -24 16 -21 19
echo
nat nine  -24 16 -13 27
echo
date
} 2>&1 | tee "$SUM"
