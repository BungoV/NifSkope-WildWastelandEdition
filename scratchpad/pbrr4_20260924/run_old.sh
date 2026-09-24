cd /e/Projects/NifskopeWildWastelandEdition
S=$PWD/scratchpad/pbrr4_20260924
run() { n=$1; shift; "$@" > "$S/$n.log" 2>&1; echo "== $n rc=$?"; grep -E "(-> (PASS|FAIL)$|^RED|VERDICT|verdict)" "$S/$n.log" | grep -vE "^  " | tail -12; }
run r3 bash tests/spells/pbr_r3_gates.sh --out "$S/r3"
for r in noms nosplit f0law fo4csweight notint q9legacy; do run r3red_$r bash tests/spells/pbr_r3_gates.sh --out "$S/r3red_$r" --red $r; done
python tests/spells/pbr_r1_fixtures.py > "$S/r1fix.log" 2>&1; echo "r1 fixtures rc=$?"
run r1 bash tests/spells/pbr_r1_gates.sh --out "$S/r1"
run r1red_f0law bash tests/spells/pbr_r1_gates.sh --out "$S/r1red_f0law" --red f0law
run r2a bash tests/spells/pbr_r2a_gates.sh --out "$S/r2a"
run r2b bash tests/spells/pbr_r2b_gates.sh --out "$S/r2b"
run zero bash tests/spells/pbr_shade_ab.sh --old release/before_pbrr4 --out "$S/zero"
run zerored_shader bash tests/spells/pbr_shade_ab.sh --old release/before_pbrr4 --out "$S/zerored_shader" --red shader
echo ALLDONE
