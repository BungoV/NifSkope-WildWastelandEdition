// Air rays: NifSkope's shim for the kernel's own buffers (lane VKCOMPUTE1, 2026-11-12): src/features/gi/rtddgi/airrays.comp's
// set 1, filled by airrays_vk.inc. Set 0 is the DDGI volume's (DDGIBindingsVk.hlsli, included first). Fusion Shaders (F4FS)
// writes its own with the same names; DDGIAirRays.hlsli stays as it is. DXIL takes the same slots in space 1.
#ifndef DDGI_AIR_RAYS_BINDINGS_VK_HLSLI
#define DDGI_AIR_RAYS_BINDINGS_VK_HLSLI

#include "DDGIAirRaysTypes.hlsli"

#ifdef __spirv__
#define AIR_BIND( b ) [[vk::binding( b, 1 )]]
#else
#define AIR_BIND( b )
#endif

AIR_BIND( 0 ) StructuredBuffer<AirParams> AirParamBuf : register( t0, space1 );
AIR_BIND( 1 ) RWStructuredBuffer<float4> AirAcc : register( u1, space1 );         // running means: num, den, frames, 0
AIR_BIND( 2 ) StructuredBuffer<float4> AirAccPrev : register( t2, space1 );       // the previous frame's
AIR_BIND( 3 ) RWStructuredBuffer<float2> AirOutRatio : register( u3, space1 );    // the ratio, its frames (0: none)
#define AIR_A AirParamBuf[0]

#endif
