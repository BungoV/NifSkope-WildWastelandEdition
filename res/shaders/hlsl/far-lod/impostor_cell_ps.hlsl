// HLSL source of features/far-lod/impostor_cell.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "FarLodBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// lane FARLOD1: impostor_oct.frag with the cell's own lights and the Lookdev fog compiled in
// (fo4_cell.frag's pattern). Chosen by name only for the cell view's far cards.
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#include "impostor_oct_ps.hlsl"
