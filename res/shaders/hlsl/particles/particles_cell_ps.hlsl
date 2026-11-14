// HLSL source of features/particles/particles_cell.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_AIRZONE, WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "ParticlesBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// lane SUNCELL1: particles.frag the game's way in the cell view (linear, fogged, the HDR frame or the cell's
// imagespace). Chosen by name in Particles::drawShapes only while a draw is cell-lit.
#define WW_CELLLIGHTS 1
#include "particles_ps.hlsl"
