// HLSL source of pbrm_cell.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
// stage structs and main() come with the included pbrm_default_ps.hlsl
#define WW_REGS_SET   // this program keeps the includes' registers

// pbrm_default.frag with the cell's own lights and the bake's bounce compiled in (src/gl/celllights.h).
// Renderer::setupProgram picks it by name only while a draw is cell-lit.
#define WW_CELLLIGHTS 1
#include "pbrm_default_ps.hlsl"
