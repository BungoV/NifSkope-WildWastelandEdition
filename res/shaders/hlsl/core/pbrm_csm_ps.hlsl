// HLSL source of pbrm_csm.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
// stage structs and main() come with the included pbrm_default_ps.hlsl
// this program's registers for blocks its includes declare (glslang numbers them per program and variant)
#ifndef WW_REGS_SET
#define WW_REGS_SET
#if defined(WW_CELL_FOGLEAK) && defined(WW_FRAME_UBO)
#define WW_REG_wwShadowFrame b1
#endif
#endif

// pbrm_default.frag with the cascaded sun shadows compiled in. The loader drops the included file's #version line.
// Renderer::setupProgram picks it by name only while a draw receives shadows.
#define WW_SUNSHADOW 1
#include "pbrm_default_ps.hlsl"
