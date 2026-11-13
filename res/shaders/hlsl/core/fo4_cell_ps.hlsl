// HLSL source of fo4_cell.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
// stage structs and main() come with the included fo4_default_ps.hlsl
#define WW_REGS_SET   // this program keeps the includes' registers

// fo4_default.frag with the cell's own lights (and the Lookdev fog) compiled in.
// Renderer::setupProgram picks it by name only while a draw is cell-lit.
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#include "fo4_default_ps.hlsl"
