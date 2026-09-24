#!/usr/bin/env bash
# lane PBRR1: the red controls, gate (f) + its shader red, and the lodgen unchanged check, in series.
cd /e/Projects/NifskopeWildWastelandEdition || exit 2
S="$PWD/scratchpad/pbrr1_20260924"
tasklist | grep -i -E "Fallout4" && { echo "GAME UP - stop"; exit 2; }
for r in coverage order order_e f0law nifx; do
	bash tests/spells/pbr_r1_gates.sh --red $r --out "$S/red_$r" > "$S/red_$r.log" 2>&1
	echo "RED $r rc=$? :: $(grep 'RED CONTROL' "$S/red_$r.log")"
done
bash tests/spells/pbr_shade_ab.sh --old release/before_pbrr1 --out "$S/ab" > "$S/ab.log" 2>&1
echo "AB rc=$? :: $(grep SUMMARY "$S/ab.log")"
bash tests/spells/pbr_shade_ab.sh --old release/before_pbrr1 --out "$S/ab_red_shader" --red shader > "$S/ab_red_shader.log" 2>&1
echo "AB-RED-SHADER rc=$? :: $(grep -E 'RED|SUMMARY' "$S/ab_red_shader.log" | tail -2)"
EXE="$PWD/release/before_pbrr1/NifSkope.exe" bash tests/spells/lodgen_terrain_pbrm.sh > "$S/lodgen_old.log" 2>&1
echo "LODGEN-OLD rc=$?"
bash tests/spells/lodgen_terrain_pbrm.sh > "$S/lodgen_new.log" 2>&1
echo "LODGEN-NEW rc=$?"
echo "ALL-DONE"
