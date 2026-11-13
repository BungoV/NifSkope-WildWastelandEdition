// HLSL source of fo4_effectdecal.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
// stage structs and main() come with the included fo4_effectshader_ps.hlsl
// this program's registers for blocks its includes declare (glslang numbers them per program and variant)
#ifndef WW_REGS_SET
#define WW_REGS_SET
#if defined(WW_CELL_FOGLEAK) && defined(WW_FRAME_UBO)
#define WW_REG_wwShadowFrame b1
#endif
#endif

// fo4_effectshader.frag for a Decal-flagged effect in the cell view: the game draws it into the surface under it,
// so it takes that surface's lights, ambient, sky, obscurance and fog. Debug: WW_CELL_DECALLIT_RED=unlit.
#define WW_CELLLIGHTS 1
#define WW_CELL_FX 1
#define WW_FX_DECALLIT 1
#include "fo4_effectshader_ps.hlsl"
