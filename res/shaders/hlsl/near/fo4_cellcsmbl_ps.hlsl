// HLSL source of features/near/fo4_cellcsmbl.frag (fragment shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_6
// bindless maps: ../near/ww_bindless_gl.hlsli in the GL build (WW_GL_BINDLESS), ww_bindless_vk.hlsli otherwise
#include "NearBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// lane BINDLESS2: fo4_cellcsm.frag with the material's maps read through the per-draw row's handles (the Bindless
// draws row on); the row off runs fo4_cellcsm as before.
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#define WW_SUNSHADOW 1
#define WW_BINDLESS 1
#include "../core/fo4_default_ps.hlsl"
