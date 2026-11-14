// lane HLSLSRC3: the bindings of the GPU cull kernels (cellgpucull_hiz0_cs / _hizn_cs / _cull_cs.hlsl). SPIR-V takes the
// GLSL's own numbers (std430 buffers 0-5, depthTex 1, the loose uniforms' block 31) through [[vk::binding]]; DXIL takes the
// register beside each declaration.
#pragma once
// LooseUniforms keeps the GLSL default block's std140 offsets: an int2 after a single int sits at 8 there, HLSL packs it
// at 4, so a pad int goes between them.
#ifdef __spirv__
#define WW_CULL_BIND( b ) [[vk::binding( b, 0 )]]
#else
#define WW_CULL_BIND( b )
#endif
