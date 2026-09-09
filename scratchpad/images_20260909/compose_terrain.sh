#!/bin/bash
# Compose the eight terrain pictures: lit pair and triangulation pair at each
# far level, vanilla left / ours right, the SAME crop box on both halves,
# labels and caption burned in (nifskope-ww-vanilla-compare step 5).
set -u
B=E:/Projects/NifskopeWildWastelandEdition/scratchpad/images_20260909
C=$B/../mountains_20260907/images/compose.py
CROP="430 280 1130 660"
CAM="camera WW_RENDER_VIEW=8 (ViewUser), WW_RENDER_CENTER=%s, WW_RENDER_DIST=%s, WW_RENDER_SIZE=1400x900 -> 1507x841, identical on both halves"

lit() {  # lit <dim> <tile> <cells> <ctr> <dist> <vtri> <otri>
  local d=$1 t=$2 cells=$3 ctr=$4 dist=$5 vt=$6 ot=$7
  python "$C" "$B/img/lit_van$d.png" "$B/img/lit_ours$d.png" \
    "$B/handoff_terrain_L${d}_lit.png" "vanilla" "ours (this build)" \
"$t  --  far level $d, cells $cells, Commonwealth.  Lit, both sides serving their OWN diffuse + _msn from a staged data root; the WATER node is hidden on BOTH halves (Flags 14 -> 15) because its shape has no source texture and NifSkope draws it opaque white.|Camera pinned identically: WW_RENDER_VIEW=8, WW_RENDER_CENTER=$ctr, WW_RENDER_DIST=$dist, 1400x900 requested / 1507x841 delivered.  Triangles vanilla $vt, ours $ot.|Ours ships Smoothness 1 / Specular #000000 / Clamp 3 against vanilla's Smoothness 0 / Specular #ffffff / Clamp 0 -- that difference is IN this frame, it is not a texture result.  Vertex descriptor identical on both sides (52776558133763, no vertex colours: --no-terrain-identity).|Regenerate: release/NifSkope.exe -no-gui lodgen <Fallout4.esm> --worldspace 3C --terrain-region <cells> --dim $d --no-terrain-identity --out-dir <abs> --tex-dir <abs>/textures/terrain/Commonwealth --data-root <abs unpacked Data>" \
    $CROP
}

wire() {  # wire <dim> <tile> <cells>
  local d=$1 t=$2 cells=$3
  python "$C" "$B/img/wire_van$d.png" "$B/img/wire_ours$d.png" \
    "$B/handoff_terrain_L${d}_triangulation.png" "vanilla" "ours (this build)" \
"$t  --  far level $d, cells $cells.  Every triangle EDGE of the LAND shape, orthographic from above, both halves on the same $((d*4096))-unit extent box, nothing scaled.|Drawn offline from each .BTR's own bytes (scratchpad/images_20260909/wireplan.py on scratchpad/btr_spacing_20260909/btrparse.py) because the renderer has no headless wireframe mode and LOD channel 8 photographs black on terrain -- no .BTR ships vertex normals.|Regenerate: python wireplan.py <abs .BTR> <abs out.png> $d \"<label>\"" \
    
}

lit 4  Commonwealth.4.28.24  "28..31 x 24..27"  8192,8192,0     14000  2084 3140
lit 8  Commonwealth.8.24.24  "24..31 x 24..31"  16384,16384,0   28000  2134 2364
lit 16 Commonwealth.16.16.16 "16..31 x 16..31"  32768,32768,0   55000  2429 2606
lit 32 Commonwealth.32.0.0   "0..31 x 0..31"    65536,65536,0  111000  3358 3450

wire 4  Commonwealth.4.28.24  "28..31 x 24..27"
wire 8  Commonwealth.8.24.24  "24..31 x 24..31"
wire 16 Commonwealth.16.16.16 "16..31 x 16..31"
wire 32 Commonwealth.32.0.0   "0..31 x 0..31"
echo COMPOSE-DONE
