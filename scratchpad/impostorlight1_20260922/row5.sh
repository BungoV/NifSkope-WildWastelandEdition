#!/bin/sh
# IMPOSTORLIGHT1 -- row 5's 0.4979 < 0.50, taken apart. The gate's own fixture
# (impostorfin1 fixture, blast_n4 000531b3 + TreeMapleblasted05.nif) at the
# gate's own 16 kViews (8 azimuths x elevation 15 / 55), blend on, 1024x1024,
# on the RUNG exe (diag_run) with three shader folders:
#   s1   = the normals stage (a normal colour is never the clear colour: the TRUE silhouettes)
#   old  = bf6aa749's shaders, pristine
#   fix  = this lane's shaders, pristine
# then row5.py scores IoU with the harness's background rule and with the true
# card silhouette.
set -u
MINE="E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorlight1_20260922"
cd "$MINE" || exit 1
K="0:15,45:15,90:15,135:15,180:15,225:15,270:15,315:15,0:55,45:55,90:55,135:55,180:55,225:55,270:55,315:55"
FX="E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorfin1_20260922/fixture"
SETROOT="$FX" VIEWS="$K" PORTBASE=28870 sh diag_run.sh 1 "$MINE/row5/s1" "$MINE/shaders_bf6aa749" blast_n4
SETROOT="$FX" VIEWS="$K" PORTBASE=28872 sh diag_run.sh 0 "$MINE/row5/old" "$MINE/shaders_bf6aa749" blast_n4
SETROOT="$FX" VIEWS="$K" PORTBASE=28874 sh diag_run.sh 0 "$MINE/row5/fix" "$MINE/shaders_final" blast_n4
python "$MINE/row5.py" "$MINE/row5"
echo ROW5-DONE
