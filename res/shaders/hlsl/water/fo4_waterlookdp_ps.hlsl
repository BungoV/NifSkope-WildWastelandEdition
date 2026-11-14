// HLSL source of features/water/fo4_waterlookdp.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "WaterBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// lane WATERFIX7: fo4_waterlookd.frag with the probes (WW_CELL_WATER_PROBE); src/features/water/cellwater.cpp picks it while a probe is set.
#define WW_WATER_PROBES 1
#include "fo4_waterlookd_ps.hlsl"
