// HLSL source of features/clouds/cloud_march_v.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CloudsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// lane CLOUD5: cloud_march.frag with Cloud Variety (types, weather noise, curl); off draws cloud_march.prog unchanged.

#define WW_CLOUD_VARIETY 1
#include "cloud_march_ps.hlsl"
