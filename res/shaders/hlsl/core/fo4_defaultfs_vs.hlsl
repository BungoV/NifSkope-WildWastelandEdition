// HLSL source of fo4_defaultfs.vert, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: vs_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
// stage structs and main() come with the included fo4_default_vs.hlsl
// this program's registers for blocks its includes declare (glslang numbers them per program and variant)
#ifndef WW_REGS_SET
#define WW_REGS_SET
#define WW_REG_globalUniforms b1
#endif

// lane FARLOOK1101 (FARSHIMMER2): fo4_default.vert with the far rows' blended splat (farSplat). Only the far splat
// twins (fo4_cellfs / fo4_cellcsmfs) use it, so every other program's binary stays as it was.
#define WW_FAR_SPLAT 1
#include "fo4_default_vs.hlsl"
