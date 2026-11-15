// HLSL source of fo4_defaultbl.vert (vertex shader), lane FO4MATCH1 part B: one readable file for every variant, the GLSL's
// comments, names and order kept (it compiles core/fo4_default_vs.hlsl, as the GLSL compiles fo4_default.vert).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// The row index (wwDrawBase + the draw's base instance: the BaseInstance builtin, SM 6.8 SV_StartInstanceLocation in DXIL)
// is set in main and goes to the pixel stage flat.
// profile: vs_6_8
#include "NearBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
// lane BINDLESS2: fo4_default.vert reading its matrices from the per-draw row (ww_bindless.glsl)
// lane BINDLESSMDI1: the row once per vertex, so a multi-draw's fragments read their own draw's maps
#define WW_BINDLESS 1
#define WW_BL_VERT 1
#include "../core/fo4_default_vs.hlsl"
