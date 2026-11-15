// HLSL source of fo4_cellbl.frag (fragment shader): fo4_cell (core/fo4_cell_ps.hlsl) with the material's maps in the draw's
// row (WW_BINDLESS: core/fo4_default_ps.hlsl's bindless branch), one readable source (lane VKWATER3 G4; it replaced the
// frozen flat port of HLSLBULK1). The Vulkan twins (src/features/vkframe/vkframe_spirv.sh fxb, -D WW_VK_BINDLESS) and GL's
// build (tests/hlsllive/build_spv.py adds WW_GL_BINDLESS for a source that names it) both compile this file.
// The plain variant (no loader define: the frozen flat port's code); the RT GI twin adds WW_CELL_RTDDGI, _CASCADES and
// WW_CELL_RESTIR.
// profile: ps_6_6
#define WW_BINDLESS 1
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#include "../core/fo4_default_ps.hlsl"
