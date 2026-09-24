#!/bin/sh
# IMPOSTORLIGHT1 -- the pictures. ONE exe (the lane's build), two shader
# folders: ab_before = bf6aa749's shaders (the old card lighting, byte for
# byte), ab_after = the fix. The harness change is harness-only, so the two
# columns differ by impostor_oct.frag and nothing else.
#   sh pics.sh            (all runs, sequential, one harness NifSkope at a time)
set -u
MINE="E:/Projects/NifskopeWildWastelandEdition/scratchpad/impostorlight1_20260922"
cd "$MINE" || exit 1
P="$MINE/pics"
mkdir -p "$P"
# A: five subjects, one ordinary (non-bake) view, the viewer's headlight
for side in before after; do
	RUN="$MINE/ab_$side" VIEWS="30:20" PORTBASE=28830 \
		sh diag_run.sh 0 "$P/head_$side" "$MINE/shaders_$( [ $side = before ] && echo bf6aa749 || echo final )"
done
# B: the side light -- one camera, the world-fixed light walked round the object
for pl in 0 90 180 270; do
	for side in before after; do
		WW_IMPOSTOR_LIGHT="60,$pl" RUN="$MINE/ab_$side" VIEWS="30:20" PORTBASE=28850 \
			sh diag_run.sh 0 "$P/side_${side}_p$pl" "$MINE/shaders_$( [ $side = before ] && echo bf6aa749 || echo final )" \
			blast_n4 dead_n4 rock_n4
	done
done
echo PICS-DONE
