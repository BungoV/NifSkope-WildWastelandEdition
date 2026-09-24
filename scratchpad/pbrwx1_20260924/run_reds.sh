#!/usr/bin/env bash
# every red of pbr_wx1_gates.sh, one at a time; each must end FAIL
cd /e/Projects/NifskopeWildWastelandEdition || exit 2
B=/e/Projects/NifskopeWildWastelandEdition/scratchpad/pbrwx1_20260924/reds
mkdir -p "$B"
REDS="${REDS:-exegmst rgbblend nonightbranch sunfade2h sunalpha1 colorext05 phasestuck moonalpha1 nam1ignore speedswap cloudalphaone hourstuck cloudgametime todorder skyscale skygamma skyswap skyleak sunleak cloudleak moonleak nolive nosave}"
for r in $REDS; do
	rm -rf "$B/$r"
	timeout 590 bash tests/spells/pbr_wx1_gates.sh --out "$B/$r" --red "$r" > "$B/$r.log" 2>&1
	v="$(tail -1 "$B/$r.log")"
	n="$(grep -E '^[0-9]+ checks' "$B/$r.log" | tail -1)"
	if [ "$v" = "FAIL" ]; then echo "RED $r -> FAIL ($n) OK"; else echo "RED $r -> '$v' ($n) BROKEN"; fi
done
echo "REDS DONE"
