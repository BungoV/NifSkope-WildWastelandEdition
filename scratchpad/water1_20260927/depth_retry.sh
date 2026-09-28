#!/bin/bash
# WATER1: RESUME 1 depth pass with retries (Avast sandbox); waits for any other chain4 copy to end first.
ME=/e/Projects/NifskopeWWE-water1/scratchpad/water1_20260927; cd $ME || exit 1
while powershell -NoProfile -Command "@(Get-CimInstance Win32_Process | ? { \$_.CommandLine -like '*chain4.sh*' -and \$_.Name -eq 'bash.exe' -and \$_.CommandLine -notlike '*-c *' }).Count" | tr -d '\r' | grep -qv '^0$'; do sleep 20; done
for t in 1 2 3 4 5 6; do
  [ -s $ME/pics_depth/A_depth.png ] && { echo "A_depth on disk"; break; }
  PORTBASE=$((46700 + t*20)) bash $ME/resume12.sh depth > $ME/depth_try_$t.out 2>&1
  tail -4 $ME/depth_try_$t.out
  grep -q "RENDER PASS DONE depth" $ME/depth_try_$t.out && { echo "DEPTH DONE try $t"; exit 0; }
  echo "$(date +%H:%M) depth try $t stopped; waiting 600 s"; sleep 600
done
[ -s $ME/pics_depth/A_depth.png ] && echo "DEPTH DONE" || echo "DEPTH GAVE UP"
