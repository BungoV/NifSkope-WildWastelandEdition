// HLSL source of features/far-lod/impostor_cellinstez.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "FarLodBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// lane PERFFIX4: impostor_cellinst.frag with gl_FragDepth declared depth_greater (the quad stands in front of it,
// impostor_instez.vert), so the hardware's early depth test runs before the sheets are marched.
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#define WW_CARDINST 1
#define WW_CARD_EARLYZ 1
#include "impostor_oct_ps.hlsl"
