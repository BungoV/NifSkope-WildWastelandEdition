// HLSL source of features/far-ground/fo4_cellfgc.frag (fragment shader), lane FO4MATCH1: one readable file for every variant,
// the GLSL's comments, names and order kept (it compiles core/fo4_default_ps.hlsl, as the GLSL compiles fo4_default.frag).
// variants: #if WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "FarGroundBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// lane FARDRAW1: fo4_cell.frag with the far ring 0's live ground (cell_farground.glsl, option 2). Chosen by
// name in Renderer::setupProgram for ring 0's terrain shapes only while the Far ground row is live.
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#define WW_FARGROUND 2
#include "../core/fo4_default_ps.hlsl"
