cd /e/Projects/NifskopeWildWastelandEdition
S=$PWD/scratchpad/pbrr4_20260924
for r in nodiv notintmask emitraw emitmul nocomp f90scaled lambert; do
  bash tests/spells/pbr_r4_gates.sh --out "$S/red_$r" --red $r > "$S/red_$r.log" 2>&1
  echo "RED $r: $(grep "^RED" "$S/red_$r.log" | tail -1)"
done
echo ALLDONE
