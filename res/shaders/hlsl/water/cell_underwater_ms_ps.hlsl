// HLSL source of features/water/cell_underwater_ms.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "WaterBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// lane WATERUNDER4: cell_underwater.frag run per sample over the samples its per-pixel draw left (an edge pixel's
// nearer samples), each with its own depth. Selected by name in src/features/water/cellunderwater.cpp after cell_underwater.prog.
#define WW_UW_SAMPLE 1
#include "cell_underwater_ps.hlsl"
