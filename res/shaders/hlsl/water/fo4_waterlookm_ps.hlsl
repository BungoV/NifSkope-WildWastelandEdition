// HLSL source of features/water/fo4_waterlookm.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "WaterBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// lane FIX1101A (src/features/water/waterpreset.h): fo4_waterlook.frag for the Medium water preset: only the looks Medium draws
// (clear colour on the bed, physical reflections, soft edge, river foam; the far sky byte). The full twin compiles every
// look in, and its registers cost every look even when its rows are off (+0.07 ms at 1080p, tests/features/water/watercost). Picked by
// name while the preset is Medium and no other water look row is on (src/features/water/cellwater.cpp wwCellWaterProgram).
#define WW_WATER_DEPTH 1
#define WW_WATER_SHORE 1
#define WW_WATER_REFL 1
#define WW_WATER_SKYFAR 1
#define WW_WATER_CLEAR 1
#define WW_WATER_RIVERFOAM 1
#define WW_WATER_NOPROBE 1
#include "fo4_water_ps.hlsl"
