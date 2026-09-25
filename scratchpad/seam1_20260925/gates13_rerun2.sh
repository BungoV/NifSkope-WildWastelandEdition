#!/bin/bash
# Second follow-up (17:0x): impostor_wind G2 read 1 DIFF (TreeHero01 _oct_n.DDS vs the s5 rung) -> the same spell
# on a copy of main's director exe 828ac612 (control), then on this exe again; pbr_shade_ab once more (2 cases drew
# no picture); native_lighting with its fixtures copied in from main's untracked scratchpad.
W=/e/Projects/NifskopeWWE-seam1; O=$W/scratchpad/seam1_20260925/gates13; cd $W
export WW_WINDOW_AT=1960,40 WIND_PORT=47811 PBR_AB_PORT=47217
gate() { G=$(powershell -NoProfile -Command "if (Get-Process Fallout4 -ErrorAction SilentlyContinue) { 'UP' } else { 'DOWN' }"); [ "$G" = DOWN ]; }
line() { echo "$1 rc=$2 | $(grep -a -E '[0-9]+ checks|SUMMARY|RESULT|[0-9]+/[0-9]+ ok' $3 | tail -1 | tr -d '\r' | cut -c1-120)" >> $O/summary.txt; }
gate && { EXE=$W/release/NifSkope_main828.exe bash tests/spells/impostor_wind.sh > $O/impostor_wind_main828.log 2>&1; line "impostor_wind CONTROL exe 828ac612" $? $O/impostor_wind_main828.log; }
gate && { bash tests/spells/impostor_wind.sh > $O/impostor_wind_rerun.log 2>&1; line "impostor_wind RERUN" $? $O/impostor_wind_rerun.log; }
gate && { bash tests/spells/pbr_shade_ab.sh > $O/pbr_shade_ab_rerun.log 2>&1; line "pbr_shade_ab RERUN" $? $O/pbr_shade_ab_rerun.log; }
gate && { PORT=47937 bash tests/spells/native_lighting.sh > $O/native_lighting_rerun.log 2>&1; line "native_lighting RERUN" $? $O/native_lighting_rerun.log; }
echo "rerun2 done $(date +%H:%M:%S)" >> $O/summary.txt
