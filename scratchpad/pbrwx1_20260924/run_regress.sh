#!/usr/bin/env bash
# the zero set and the R1/R2a/R2b/R3/R4 gates on the lane's exe, one after another, our own ports
cd /e/Projects/NifskopeWildWastelandEdition || exit 2
B=/e/Projects/NifskopeWildWastelandEdition/scratchpad/pbrwx1_20260924/regress
mkdir -p "$B"
run() {  # run <tag> <cmd...>
	local tag="$1"; shift
	rm -rf "$B/$tag"
	"$@" --out "$B/$tag" > "$B/$tag.log" 2>&1
	echo "$tag rc=$? :: $(grep -E '^[0-9]+ checks|checks, [0-9]+ failures' "$B/$tag.log" | tail -1) :: $(tail -1 "$B/$tag.log")"
}
PBR_AB_PORT=43251 run zero bash tests/spells/pbr_shade_ab.sh --old release/before_pbrwx1
PBR_R1_PORT=43252 run r1 bash tests/spells/pbr_r1_gates.sh
PBR_R2A_PORT=43253 run r2a bash tests/spells/pbr_r2a_gates.sh
PBR_R2B_PORT=43254 run r2b bash tests/spells/pbr_r2b_gates.sh
PBR_R3_PORT=43255 run r3 bash tests/spells/pbr_r3_gates.sh
PBR_R4_PORT=43256 run r4 bash tests/spells/pbr_r4_gates.sh
echo "REGRESS DONE"
