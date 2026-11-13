// HLSL source of features/sky/atmo_apl.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "SkyBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// lane PERFFIX3 (item 1): atmo_ap.frag with the slice from the geometry shader (atmo_apl.geom), one layered draw
#define WW_AP_LAYERED 1
#include "atmo_ap_ps.hlsl"
