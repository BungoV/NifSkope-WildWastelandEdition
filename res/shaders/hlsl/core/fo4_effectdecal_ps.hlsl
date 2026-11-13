// HLSL source of fo4_effectdecal.frag (fragment shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_AIRZONE, WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// fo4_effectshader.frag for a Decal-flagged effect in the cell view: the game draws it into the surface under it,
// so it takes that surface's lights, ambient, sky, obscurance and fog. Debug: WW_CELL_DECALLIT_RED=unlit.
#define WW_CELLLIGHTS 1
#define WW_CELL_FX 1
#define WW_FX_DECALLIT 1
#include "fo4_effectshader_ps.hlsl"
