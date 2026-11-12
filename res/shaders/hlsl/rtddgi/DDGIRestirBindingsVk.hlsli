// ReSTIR DI: NifSkope's shim for the kernel's own buffers (lane VKCOMPUTE1, 2026-11-12): src/features/gi/rtddgi/restir.comp's
// set 1, filled by restir_vk.inc. Set 0 is the DDGI volume's (DDGIBindingsVk.hlsli, included first). Fusion Shaders (F4FS)
// writes its own with the same names; DDGIRestir.hlsli stays as it is. DXIL takes the same slots in space 1.
#ifndef DDGI_RESTIR_BINDINGS_VK_HLSLI
#define DDGI_RESTIR_BINDINGS_VK_HLSLI

#include "DDGIRestirTypes.hlsli"
#include "RestirCommon.hlsli"

#ifdef __spirv__
#define RESTIR_BIND( b ) [[vk::binding( b, 1 )]]
#else
#define RESTIR_BIND( b )
#endif

RESTIR_BIND( 0 ) StructuredBuffer<RestirParams> RestirParamBuf : register( t0, space1 );
RESTIR_BIND( 1 ) StructuredBuffer<float4> RestirGbuf : register( t1, space1 );          // view normal, linear depth (1e6 none)
RESTIR_BIND( 2 ) StructuredBuffer<float4> RestirPrevGbuf : register( t2, space1 );
RESTIR_BIND( 3 ) RWStructuredBuffer<RestirRes> RestirCur : register( u3, space1 );
RESTIR_BIND( 4 ) RWStructuredBuffer<RestirRes> RestirHist : register( u4, space1 );
RESTIR_BIND( 5 ) RWStructuredBuffer<int> RestirHistPix : register( u5, space1 );
RESTIR_BIND( 6 ) RWStructuredBuffer<float4> RestirNumDen : register( u6, space1 );      // num, den, blocked (0, 1; 2 no ray), 0
RESTIR_BIND( 7 ) RWStructuredBuffer<float4> RestirAcc : register( u7, space1 );         // the running means: num, den, frames, 0
RESTIR_BIND( 8 ) StructuredBuffer<float4> RestirAccPrev : register( t8, space1 );
RESTIR_BIND( 9 ) StructuredBuffer<int> RestirCand : register( t9, space1 );
RESTIR_BIND( 10 ) RWStructuredBuffer<float2> RestirOutRatio : register( u10, space1 );  // the ratio, the linear depth it is for
#define RESTIR_R RestirParamBuf[0]

#endif
