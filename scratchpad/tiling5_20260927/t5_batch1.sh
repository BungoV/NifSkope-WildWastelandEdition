#!/bin/bash
# TILING5 continuation 2 -- every bake of this session under ONE turn of the run lock.
#   c1 = the constants build (39cb880f), c2 = the mean-bias build (020ae381).
# Game gate inside t5_bake.sh (refuses per bake); this script also refuses up front.
set -u
HERE=/e/Projects/NifskopeWWE-tiling5/scratchpad/tiling5_20260927
C1=$HERE/run_c1/NifSkope.exe
C2=$HERE/run_c2/NifSkope.exe
TURN=/e/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP"; exit 1; fi
bash $TURN acquire TILING5 21600 || exit 1
trap 'bash $TURN release TILING5' EXIT
if tasklist //FI "IMAGENAME eq Fallout4.exe" 2>/dev/null | grep -q Fallout4.exe; then echo "GAME UP after turn"; exit 1; fi
echo "start $(date +%H:%M:%S)"
# 1-2. off-identity, both new exes, Boston box, no new switch
bash $HERE/t5_bake.sh "$C1" id_c1 boston | head -1
bash $HERE/t5_bake.sh "$C2" id_c2 boston | head -1
# 3. constant check: c1 height arm on two of the fourteen chunks (compare with s20, baked by env beta 2.0)
bash $HERE/t5_bake.sh "$C1" c1h r:-4,-20,-1,-17 --land-height-blend on | head -1
bash $HERE/t5_bake.sh "$C1" c1h r:20,-24,23,-21 --land-height-blend on | head -1
# 4. the corrected arm on the fourteen frozen chunks
EXE=$C2 bash $HERE/t5_arms.sh fix --land-height-blend on
# 5. thread invariance + timing, Boston box, corrected arm
THREADS=16 bash $HERE/t5_bake.sh "$C2" fixB16 boston --land-height-blend on | head -1
THREADS=1 bash $HERE/t5_bake.sh "$C2" fixB1 boston --land-height-blend on | head -1
# 6. the rural picture box, corrected arm
bash $HERE/t5_bake.sh "$C2" fix r:-36,4,-25,15 --land-height-blend on | head -1
echo "end $(date +%H:%M:%S)"
