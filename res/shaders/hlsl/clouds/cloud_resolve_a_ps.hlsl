// HLSL source of features/clouds/cloud_resolve_a.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CloudsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// lane CLOUDBAND1: cloud_resolve.frag with Cloud Accumulation (the fresh march blended into the clamped history); off
// draws cloud_resolve.prog unchanged.

#define WW_CLOUD_ACCUM 1
#include "cloud_resolve_ps.hlsl"
