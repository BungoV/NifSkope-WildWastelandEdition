#!/bin/bash
# Lane SHOWCASE1 -- every 3D render the pictures are built from.
# ONE camera definition per view, used for the .BTR and the .BTO alike; the only
# difference between the two is the look-at, and that difference is EXACTLY the
# chunk origin, because a .BTR is written in chunk-local coordinates and a .BTO
# in worldspace ones.  Chunk (-20,24) origin = (-20*4096, 24*4096) = (-81920, 98304),
# so local (8192,8192) and world (-73728,106496) are the same point on the ground.
# Measured, not assumed: an unpinned auto-fit of the .BTR looks at (8158,9641,8938)
# and of the .BTO at (-74063,105690,10010).
set -u
L="E:/Projects/NifskopeWildWastelandEdition/scratchpad/showcase1_20260912"
S="$L/shots"; mkdir -p "$S"
LOC="8192,8192,9000"; WLD="-73728,106496,9000"
export SIZE=1400x900

sh () { bash "$L/shot.sh" "$@"; }            # out file res view centre ortho [env...]

# ---- picture 2 + 4: terrain and objects, three views, three identity states ---
for b in on noid look; do
  case $b in on) R=res_on;; noid) R=res_noid;; look) R=res_look;; esac
  sh "$S/p2_${b}_btr_top.png"  "$L/out/$b/obj/Commonwealth.4.-20.24.BTR" "$L/$R" 1 "$LOC" 13500 WW_RENDER_SS=1
  sh "$S/p2_${b}_bto_top.png"  "$L/out/$b/obj/Commonwealth.4.-20.24.BTO" "$L/$R" 1 "$WLD" 13500 WW_RENDER_SS=1
  sh "$S/p2_${b}_btr_front.png" "$L/out/$b/obj/Commonwealth.4.-20.24.BTR" "$L/$R" 5 "$LOC" 8600 WW_RENDER_SS=1
  sh "$S/p2_${b}_bto_front.png" "$L/out/$b/obj/Commonwealth.4.-20.24.BTO" "$L/$R" 5 "$WLD" 8600 WW_RENDER_SS=1
  sh "$S/p2_${b}_btr_obl.png"  "$L/out/$b/obj/Commonwealth.4.-20.24.BTR" "$L/$R" 8 "$LOC" 14000 WW_RENDER_SS=1
  sh "$S/p2_${b}_bto_obl.png"  "$L/out/$b/obj/Commonwealth.4.-20.24.BTO" "$L/$R" 8 "$WLD" 14000 WW_RENDER_SS=1
done

# vanilla's own BTO on the SAME camera -- the refuter for red 1
sh "$S/p2_vanilla_bto_top.png" \
   "E:/Tools/Fallout 4/DataUnpacked/Data/meshes/Terrain/Commonwealth/Objects/Commonwealth.4.-20.24.BTO" \
   "E:/Tools/Fallout 4/DataUnpacked/Data" 1 "$WLD" 13500 WW_RENDER_SS=1

# ---- picture 5: the AO channels, drawn flat (WW_LOD_CHANNEL=3 = "Ambient occlusion (B)")
for v in "top 1 $LOC 13500" "obl 8 $LOC 14000"; do set -- $v
  sh "$S/p5_btr_ao_$1.png" "$L/out/on/obj/Commonwealth.4.-20.24.BTR" "$L/res_on" "$2" "$3" "$4" WW_LOD_CHANNEL=3 WW_RENDER_SS=1
done
for v in "top 1 $WLD 13500" "obl 8 $WLD 14000"; do set -- $v
  sh "$S/p5_bto_ao_$1.png" "$L/out/on/obj/Commonwealth.4.-20.24.BTO" "$L/res_on" "$2" "$3" "$4" WW_LOD_CHANNEL=3 WW_RENDER_SS=1
done

# ---- the resource-root gate ------------------------------------------------
# A root rooted at the wrong level MISSES SILENTLY and the before/after pair
# comes out byte-identical (nifskope-ww-render-shot).  So: the SAME .BTR file,
# rendered twice, once against our ON sheets and once against the all-off ones.
# If those two PNGs are byte-identical the pictures are of vanilla's sheets and
# nothing in this lane is a picture of our bake.
sh "$S/gate_root_look.png" "$L/out/look/obj/Commonwealth.4.-20.24.BTR" "$L/res_look" 1 "$LOC" 13500
sh "$S/gate_root_off.png"  "$L/out/look/obj/Commonwealth.4.-20.24.BTR" "$L/res_off"  1 "$LOC" 13500
if cmp -s "$S/gate_root_look.png" "$S/gate_root_off.png"; then
	echo "GATE FAILED: the resource root is not being read -- both renders identical"
else
	echo "gate ok: our sheets and the all-off sheets render differently ($(stat -c %s "$S/gate_root_look.png") vs $(stat -c %s "$S/gate_root_off.png") bytes)"
fi
