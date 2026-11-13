// HLSL source of fo4_defaultfs.vert (vertex shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
// lane FARLOOK1101 (FARSHIMMER2): fo4_default.vert with the far rows' blended splat (farSplat). Only the far splat
// twins (fo4_cellfs / fo4_cellcsmfs) use it, so every other program's binary stays as it was.
#define WW_FAR_SPLAT 1
#include "fo4_default_vs.hlsl"
