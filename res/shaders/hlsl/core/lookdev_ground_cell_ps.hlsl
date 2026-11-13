// HLSL source of lookdev_ground_cell.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
// stage structs and main() come with the included lookdev_ground_ps.hlsl
#define WW_REGS_SET   // this program keeps the includes' registers

// lane LIGHTBOOTH1: lookdev_ground.frag with the placed cell lights compiled in (the light booth's floor).
#define WW_GROUND_CELL 1
#include "lookdev_ground_ps.hlsl"
