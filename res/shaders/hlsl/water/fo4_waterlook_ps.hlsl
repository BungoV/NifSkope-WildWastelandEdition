// HLSL source of features/water/fo4_waterlook.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "WaterBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// lane WATERCOMBINE1 (src/features/water/cellwater.h): fo4_water.frag with every water look compiled in (depth colour, caustics,
// soft edge + shore foam, swell); each look is switched by its own uniform, so any set of the rows draws together.
// Chosen by name while any of the water look rows is on; with all off fo4_water.prog draws (its machine code kept).
#define WW_WATER_DEPTH 1
#define WW_WATER_CAUSTICS 1
#define WW_WATER_SHORE 1
#define WW_WATER_WAVES 1
#define WW_WATER_REFL 1
#define WW_WATER_OCEAN 1
#define WW_WATER_SKYFAR 1
#define WW_WATER_CLEAR 1
#define WW_WATER_SKYVIS 1
#define WW_WATER_SURF 1
#define WW_WATER_CONTACT 1
#define WW_WATER_RIVERFOAM 1
#define WW_WATER_UNDER 1
#ifndef WW_WATER_PROBES	// lane WATERFIX7: the probe tail tipped the look over a compiler cliff (+0.18 ms); fo4_waterlookp
#define WW_WATER_NOPROBE 1
#endif
#include "fo4_water_ps.hlsl"
