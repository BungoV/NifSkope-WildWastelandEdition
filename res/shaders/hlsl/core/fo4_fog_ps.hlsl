// HLSL source of fo4_fog.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
// stage structs and main() come with the included fo4_default_ps.hlsl
#define WW_REGS_SET   // this program keeps the includes' registers

// fo4_default.frag with the Lookdev weather fog compiled in. The loader drops the included file's #version line.
// Renderer::setupProgram picks it by name only while a draw fogs.
#define WW_FOG 1
#include "fo4_default_ps.hlsl"
