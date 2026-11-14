// HLSL source of features/water/fo4_waterlookd.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "WaterBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// lane WATERLOOK1 step 2 (src/features/water/cellocean.h): fo4_waterlook.frag behind the split, displaced water (fo4_waterdisp.tesc /
// .tese): every water look, the spectrum read at the undisplaced point the tessellation hands over (wwOcBase).
// Chosen by name while the Ocean waves row is on (src/features/water/cellwater.cpp wwCellWaterProgram); else fo4_waterlook.prog.
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
#define WW_WATER_DISP 1
#ifndef WW_WATER_PROBES	// lane WATERFIX7: as fo4_waterlook.frag; the probes in fo4_waterlookdp
#define WW_WATER_NOPROBE 1
#endif
#include "fo4_water_ps.hlsl"
