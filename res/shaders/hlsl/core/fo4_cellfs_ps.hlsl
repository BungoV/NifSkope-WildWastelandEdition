// HLSL source of fo4_cellfs.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
// stage structs and main() come with the included fo4_default_ps.hlsl
#define WW_REGS_SET   // this program keeps the includes' registers

// lane FARLOOK1101 (FARSHIMMER2): fo4_cell.frag drawing a far row at its splat's coverage (fo4_defaultfs.vert).
// Chosen by name in Renderer::setupProgram for the instanced far rings only while WW_CELL_FARSPLAT is set.
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#define WW_FAR_SPLAT 1
#include "fo4_default_ps.hlsl"
