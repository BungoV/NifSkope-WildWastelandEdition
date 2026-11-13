// HLSL source of features/post/game_doflens.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// lane DOFLENS1: game_dof.frag with the thin-lens composite (row WW_CELL_DOF_LENS)
#define WW_DOF_LENS 1
#include "game_dof_ps.hlsl"
