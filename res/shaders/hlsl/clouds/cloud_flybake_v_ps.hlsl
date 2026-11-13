// HLSL source of features/clouds/cloud_flybake_v.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CloudsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// lane CLOUD5: cloud_flybake.frag's occupancy bound for the Cloud Variety density; off draws cloud_flybake.prog unchanged.

#define WW_CLOUD_VARIETY 1
#include "cloud_flybake_ps.hlsl"
