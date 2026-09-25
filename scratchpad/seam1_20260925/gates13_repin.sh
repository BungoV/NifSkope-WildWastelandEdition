#!/bin/bash
# The two re-pinned spells on the restored HEAD exe (cbdbffe7 relinked: 4 PE header bytes differ), one NifSkope at a time.
W=/e/Projects/NifskopeWWE-seam1; O=$W/scratchpad/seam1_20260925/gates13; cd $W
export WW_WINDOW_AT=1960,40
free() { n=$(powershell -NoProfile -Command "@(Get-Process NifSkope* -ErrorAction SilentlyContinue).Count; if (Get-Process Fallout4 -ErrorAction SilentlyContinue) { 'UP' }"); [ "$n" = 0 ] || { echo "busy: $n" >> $O/summary.txt; return 1; }; }
line() { echo "$1 rc=$2 | $(grep -a -E '[0-9]+ checks|^RESULT' $3 | tail -1 | tr -d '\r' | cut -c1-120)" >> $O/summary.txt; }
until free; do sleep 30; done
WIND_PORT=47812 bash tests/spells/impostor_wind.sh > $O/impostor_wind_repin.log 2>&1; line "impostor_wind REPIN" $? $O/impostor_wind_repin.log
until free; do sleep 30; done
CARDS=/e/Projects/NifskopeWWE-cardlink1/scratchpad/cardlink1_20260924/cards bash tests/spells/lodgen_cardlink.sh > $O/lodgen_cardlink_repin.log 2>&1; line "lodgen_cardlink REPIN" $? $O/lodgen_cardlink_repin.log
git checkout -- scratchpad/incr_gate_work scratchpad/nativeview2_20260912 2>/dev/null
echo "repin done $(date +%H:%M:%S)" >> $O/summary.txt
