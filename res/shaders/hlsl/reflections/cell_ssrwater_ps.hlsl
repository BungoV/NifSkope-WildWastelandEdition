// HLSL source of features/reflections/cell_ssrwater.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "ReflectionsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// lane WATERREFL1 (src/features/reflections/cellssr.cpp): cell_ssr.frag for the Water reflections row. A water ray marches 64 steps,
// keeps far hits (screen-edge fade only, no travelled-distance or depth-gain fade) and their colour unclamped (HDR);
// opaque rays are the game's. Picked by name while the row is on; cell_ssr.prog keeps its machine code.
#define WW_SSR_WATER_LOOK 1
#include "cell_ssr_ps.hlsl"
