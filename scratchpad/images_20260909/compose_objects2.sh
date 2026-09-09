#!/bin/bash
# The object-chunk pictures, IMAGES5.
#
# Three pictures instead of IMAGES4's two:
#   ring 0 dim 4    vanilla vs ours (--no-identity)
#   far ring dim 16 vanilla vs ours (--slot-fallback --no-identity)
#   the identity channel: ours WITH identity beside ours WITHOUT, same camera
#
# Every number in a caption is READ BACK from the chunk that is in the picture
# (geomstats.py), never typed.
set -u
B=E:/Projects/NifskopeWildWastelandEdition/scratchpad/images_20260909
C=$B/../mountains_20260907/images/compose.py
VO="E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Terrain/Commonwealth/Objects"

stats() { python "$B/geomstats.py" "$1"; }

vr=$(stats "$VO/Commonwealth.4.-20.24.BTO")
or=$(stats "$B/gen/obj_ring0_noid/Commonwealth.4.-20.24.BTO")
vf=$(stats "$VO/Commonwealth.16.16.16.BTO")
of=$(stats "$B/gen/obj_far16_noid/Commonwealth.16.16.16.BTO")
oid=$(stats "$B/gen/obj_ring0/Commonwealth.4.-20.24.BTO")
echo "ring0 vanilla: $vr"; echo "ring0 ours noid: $or"; echo "ring0 ours identity: $oid"
echo "far16 vanilla: $vf"; echo "far16 ours noid: $of"

python "$C" "$B/img/obj_van_ring0.png" "$B/img/obj_ours_ring0_noid.png" \
  "$B/handoff_objects_ring0_dim4.png" "vanilla" "ours (this build, --no-identity)" \
"Commonwealth.4.-20.24.BTO -- ring 0 (dim 4), Sanctuary, cells -20..-17 x 24..27.  Object chunk only, no terrain under it.|Camera pinned identically on both halves: WW_RENDER_VIEW=8, WW_RENDER_CENTER=-73400,106170,9800 (world), WW_RENDER_DIST=24000, 1400x900 requested / 1507x841 delivered, same crop box.|Vanilla $vr.  Ours $or -- the run reported 'merged: 10 shapes -> 10' and the far-ring simplification does not touch ring 0 by design (the byte-identity gates stand on it).|--no-identity is what makes this a like-for-like picture: our chunks write the FO4CS identity channel into Vertex Colors by default and this renderer multiplies vertex colour into the diffuse, so a default chunk photographs in the identity's hashed colours.  See handoff_objects_identity_channel.png.|Regenerate: release/NifSkope.exe -no-gui lodgen <Fallout4.esm> --worldspace 3C --terrain-region -20 24 -17 27 --dim 4 --no-identity --out-dir <abs> --tex-dir <abs>/textures/terrain/Commonwealth --data-root <abs unpacked Data>" \
  260 120 1260 780

python "$C" "$B/img/obj_van_far16.png" "$B/img/obj_ours_far16_noid.png" \
  "$B/handoff_objects_far_dim16.png" "vanilla" "ours (--slot-fallback --no-identity)" \
"Commonwealth.16.16.16.BTO -- far ring (dim 16), cells 16..31 x 16..31.  Object chunk only, no terrain under it.|Camera pinned identically: WW_RENDER_VIEW=8, WW_RENDER_CENTER=91800,99000,1000 (world), WW_RENDER_DIST=78000, same crop box on both halves.|Vanilla $vf.  Ours $of -- with --slot-fallback the ring takes a coarser LOD model where its own MNAM slot is empty; the run merged 46 shapes to 38 and cut 25 of them from 78,154 to 71,411 triangles and 120,761 to 111,132 vertices (13 shapes kept whole, 0 restored, worst error 511.8 units).  Not one of this chunk's refs fills the ring-16 slot, so without --slot-fallback the chunk is empty of them.|MAGENTA = a texture NifSkope could not resolve.  The run printed 'File \" materials/c:/projects/fallout4/build/pc/data/materials/lod/metalindbeamslod01.bgsm \" not found in archives' twice -- an absolute Bethesda BUILD path inside the record.  Stated as observed; no cause claimed.|Regenerate: release/NifSkope.exe -no-gui lodgen <Fallout4.esm> --worldspace 3C --terrain-region 16 16 31 31 --dim 16 --slot-fallback --no-identity --out-dir <abs> --tex-dir <abs>/textures/terrain/Commonwealth --data-root <abs unpacked Data>" \
  260 120 1260 780

python "$C" "$B/img/obj_ours_ring0_noid.png" "$B/img/obj_ours_ring0.png" \
  "$B/handoff_objects_identity_channel.png" "ours, --no-identity" "ours, identity ON (the default)" \
"Commonwealth.4.-20.24.BTO -- the SAME chunk, generated twice, on the SAME camera.  Left --no-identity, right the default.|Our object chunks write the FO4CS object-identity channel into Vertex Colors (docs/LODGEN_VERTEX_PACKING.md); vanilla's carry no vertex colours at all.  NifSkope multiplies vertex colour into the diffuse, so the right half is that channel's hashed per-object colour, not a texture failure.  A consumer that reads the channel sees an object id; a renderer that does not know about it sees this.|$or with --no-identity, $oid with it.  The chunk is 860,743 bytes without the channel and 1,280,239 with it.|The far-ring simplification also differs, because it groups by identity index: 25 shapes cut 78,154 -> 72,022 triangles with identity on (764 groups kept whole) and 78,154 -> 71,411 with it off (0 groups).  That is a real behavioural difference of the flag, measured, not a picture artefact.|Regenerate: the two commands above, with and without --no-identity." \
  260 120 1260 780
echo COMPOSE-OBJ2-DONE
