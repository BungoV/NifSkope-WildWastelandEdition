// HLSL source of features/weather/cell_precip_look.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "WeatherBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// lane PRECIPLOOK1: cell_precip_gpu.frag with the precipitation look (src/features/weather/cellprecip.cpp picks it under WW_CELL_PRECIP_LOOK).
#define PC_LOOK 1
#include "cell_precip_gpu_ps.hlsl"
