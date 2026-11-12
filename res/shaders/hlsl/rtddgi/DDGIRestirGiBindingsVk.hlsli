// ReSTIR GI: NifSkope's shim for the kernel's own buffers (lane HLSLRESTIR1, 2026-11-12): src/features/gi/rtddgi/restirgi.comp's
// set 1, filled by restirgi_vk.inc. Set 0 is the DDGI volume's (DDGIBindingsVk.hlsli, included first). Fusion Shaders (F4FS)
// writes its own with the same names; DDGIRestirGi.hlsli stays as it is. DXIL takes the same slots in space 1.
#ifndef DDGI_RESTIRGI_BINDINGS_VK_HLSLI
#define DDGI_RESTIRGI_BINDINGS_VK_HLSLI

#include "DDGIRestirGiTypes.hlsli"
#include "restirgi_common.hlsli"

#ifdef __spirv__
#define RESTIRGI_BIND( b ) [[vk::binding( b, 1 )]]
#else
#define RESTIRGI_BIND( b )
#endif

RESTIRGI_BIND( 0 ) StructuredBuffer<RestirGiParams> RestirGiParamBuf : register( t0, space1 );
RESTIRGI_BIND( 1 ) StructuredBuffer<float4> RestirGiGbuf : register( t1, space1 );        // view normal, linear depth (1e6 none)
RESTIRGI_BIND( 2 ) StructuredBuffer<float4> RestirGiPrevGbuf : register( t2, space1 );
RESTIRGI_BIND( 3 ) RWStructuredBuffer<RgRes> RestirGiCur : register( u3, space1 );        // this frame's temporal reservoirs
RESTIRGI_BIND( 4 ) StructuredBuffer<RgRes> RestirGiHist : register( t4, space1 );         // the previous frame's
RESTIRGI_BIND( 5 ) RWStructuredBuffer<int> RestirGiHistPix : register( u5, space1 );      // the reprojected pixel (-1 none)
RESTIRGI_BIND( 6 ) RWStructuredBuffer<float4> RestirGiEst : register( u6, space1 );       // the frame's estimate: E rgb, 1 valid
RESTIRGI_BIND( 7 ) RWStructuredBuffer<float4> RestirGiAcc : register( u7, space1 );       // the running mean: E rgb, frames
RESTIRGI_BIND( 8 ) StructuredBuffer<float4> RestirGiAccPrev : register( t8, space1 );
RESTIRGI_BIND( 9 ) RWStructuredBuffer<float4> RestirGiOut : register( u9, space1 );       // the filtered E rgb, the linear depth it is for
RESTIRGI_BIND( 10 ) RWStructuredBuffer<float4> RestirGiCand : register( u10, space1 );    // the bounce's Lo rgb, hit (-1 no pixel)
RESTIRGI_BIND( 11 ) StructuredBuffer<uint> RestirGiStbn : register( t11, space1 );        // the STBN mask, 128 x 128 x 64 bytes, 4 a uint
RESTIRGI_BIND( 12 ) RWStructuredBuffer<float4> RestirGiAtrA : register( u12, space1 );    // the a-trous ping-pong: E rgb, its variance
RESTIRGI_BIND( 13 ) RWStructuredBuffer<float4> RestirGiAtrB : register( u13, space1 );
RESTIRGI_BIND( 14 ) RWStructuredBuffer<uint2> RestirGiPk : register( u14, space1 );       // depth bits, octahedral normal (snorm16 x 2)
#define RG_G RestirGiParamBuf[0]

#endif
