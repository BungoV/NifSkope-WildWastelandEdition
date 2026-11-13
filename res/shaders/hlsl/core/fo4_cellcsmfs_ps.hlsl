// HLSL source of fo4_cellcsmfs.frag (fragment shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// lane FARLOOK1101 (FARSHIMMER2): fo4_cellcsm.frag drawing a far row at its splat's coverage (fo4_defaultfs.vert).
// Chosen by name in Renderer::setupProgram for the instanced far rings only while WW_CELL_FARSPLAT is set.
#define WW_FOG 1
#define WW_CELLLIGHTS 1
#define WW_SUNSHADOW 1
#define WW_FAR_SPLAT 1
#include "fo4_default_ps.hlsl"
