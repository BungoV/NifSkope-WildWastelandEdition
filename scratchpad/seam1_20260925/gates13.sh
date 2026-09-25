#!/bin/bash
# The 13 spells main's director re-ran at ea0ca708 ("13 gates green"), on THIS tree's release/NifSkope.exe.
# One harness at a time, second monitor, ports off the defaults (a sibling lane may hold those).
W=/e/Projects/NifskopeWWE-seam1; S=$W/scratchpad/seam1_20260925; O=$S/gates13; mkdir -p $O
cd $W
export WW_WINDOW_AT=1960,40 PBRM_PORT=47861 RING_PORT=47741 WIND_PORT=47811 PORT=47307 PBR_AB_PORT=47217
echo "exe $(sha1sum release/NifSkope.exe | cut -c1-8)  $(date +%H:%M:%S)" > $O/summary.txt
for s in impostor_pbrm impostor_ring impostor_wind lodgen_octahedral lodgen_impostor_cards lodgen_card_arrays \
         lodgen_cardlink lodgen_incremental lodgen_native lod_generation lodgen_loadorder pbr_shade_ab native_lighting; do
  G=$(powershell -NoProfile -Command "if (Get-Process Fallout4 -ErrorAction SilentlyContinue) { 'UP' } else { 'DOWN' }")
  [ "$G" = DOWN ] || { echo "$s SKIPPED game up" >> $O/summary.txt; continue; }
  t0=$(date +%s)
  if [ "$s" = native_lighting ]; then PORT=47937; fi
  bash tests/spells/$s.sh > $O/$s.log 2>&1; rc=$?
  echo "$s rc=$rc $(( $(date +%s) - t0 ))s | $(grep -a -E 'PASS|FAIL|passed|failed|checks|RESULT' $O/$s.log | tail -1 | tr -d '\r' | cut -c1-110)" >> $O/summary.txt
done
git checkout -- scratchpad/incr_gate_work 2>/dev/null
echo "done $(date +%H:%M:%S)" >> $O/summary.txt
