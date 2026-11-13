// HLSL source of fo4_effectcell.frag (fragment shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_AIRZONE, WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// fo4_effectshader.frag the game's way in the cell view (unlit, linear, soft and near fades, fog, imagespace).
// Renderer::setupProgram picks it by name only while a draw is cell-lit.
#define WW_CELLLIGHTS 1
#define WW_CELL_FX 1
#include "fo4_effectshader_ps.hlsl"
