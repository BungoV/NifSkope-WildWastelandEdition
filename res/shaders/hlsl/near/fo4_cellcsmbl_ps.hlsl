// HLSL source of fo4_cellcsmbl.frag (fragment shader), lane FO4MATCH1 part B: one readable file for every variant, the GLSL's
// comments, names and order kept (it compiles core/fo4_default_ps.hlsl, as the GLSL compiles fo4_default.frag).
// variants: #if WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// The maps come through near/ww_bindless.hlsli: the GL build (WW_GL_BINDLESS, tests/hlsllive/build_spv.py) reads the row's
// tex[i] as GL bindless handles (ww_bindless_gl.hlsli); Vulkan (WW_VK_BINDLESS) and Fusion Shaders (DXIL descriptor heaps,
// SM 6.6) read them as (image, sampler) indices (ww_bindless_vk.hlsli).
// profile: ps_6_6
#include "NearBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// lane BINDLESS2: fo4_cellcsm.frag with the material's maps read through the per-draw row's handles (the Bindless
// draws row on); the row off runs fo4_cellcsm as before.
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#define WW_SUNSHADOW 1
#define WW_BINDLESS 1
#include "../core/fo4_default_ps.hlsl"
