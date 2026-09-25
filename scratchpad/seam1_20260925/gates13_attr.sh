#!/bin/bash
# Attribution runs (17:2x) for the two spells still red on this exe: native_lighting gate (a) on main's director
# exe 828ac612 (control), then impostor_wind on the bisect exes a6e5e8de and 62e53a3b.
W=/e/Projects/NifskopeWWE-seam1; O=$W/scratchpad/seam1_20260925/gates13; cd $W
export WW_WINDOW_AT=1960,40 WIND_PORT=47811
gate() { G=$(powershell -NoProfile -Command "if (Get-Process Fallout4 -ErrorAction SilentlyContinue) { 'UP' } else { 'DOWN' }"); [ "$G" = DOWN ]; }
line() { echo "$1 rc=$2 | $(grep -a -E '[0-9]+ checks' $3 | tail -1 | tr -d '\r' | cut -c1-120)" >> $O/summary.txt; }
gate && { EXE=$W/release/NifSkope_main828.exe PORT=47937 bash tests/spells/native_lighting.sh > $O/native_lighting_main828.log 2>&1; line "native_lighting CONTROL exe 828ac612" $? $O/native_lighting_main828.log; }
gate && { PORT=47938 bash tests/spells/native_lighting.sh > $O/native_lighting_fixture.log 2>&1; line "native_lighting RERUN resroot copied" $? $O/native_lighting_fixture.log; }
for s in a6e5e8de 62e53a3b; do
  gate && { EXE=$W/release/NifSkope_bis_$s.exe bash tests/spells/impostor_wind.sh > $O/impostor_wind_bis_$s.log 2>&1; line "impostor_wind exe bis_$s" $? $O/impostor_wind_bis_$s.log; }
done
echo "attr done $(date +%H:%M:%S)" >> $O/summary.txt
