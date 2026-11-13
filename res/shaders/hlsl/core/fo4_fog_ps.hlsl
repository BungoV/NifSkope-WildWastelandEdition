// HLSL source of fo4_fog.frag (fragment shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// fo4_default.frag with the Lookdev weather fog compiled in. The loader drops the included file's #version line.
// Renderer::setupProgram picks it by name only while a draw fogs.
#define WW_FOG 1
#include "fo4_default_ps.hlsl"
