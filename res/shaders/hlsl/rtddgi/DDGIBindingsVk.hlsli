// DDGI volume: NifSkope's shim (lane VKCOMPUTE1, 2026-11-12). The ONE file that binds the shared DDGI kernels to
// NifSkope's Vulkan descriptor set (src/features/gi/rtddgi/ddgi_common.glsl's set 0, rtddgi_vk.inc fills it) and its
// dispatch split. Fusion Shaders (F4FS) writes its own shim with the same resource names on its D3D12 slots; the
// shared files (DDGICommon.hlsli, DDGIResources.hlsli, DDGI<Kernel>.hlsli) stay as they are.
// The register() numbers give DXIL the same slots (t = read-only, u = read-write), so this shim also compiles to DXIL.
// A kernel that traces defines DDGI_USE_TLAS before including this (ray queries: DXIL cs_6_5).
#ifndef DDGI_BINDINGS_VK_HLSLI
#define DDGI_BINDINGS_VK_HLSLI

#include "DDGICommon.hlsli"

#ifdef __spirv__
#define DDGI_BIND( b ) [[vk::binding( b, 0 )]]
#else
#define DDGI_BIND( b )
#endif

DDGI_BIND( 0 ) RWStructuredBuffer<float4> DDGIRays : register( u0 );          // per probe x ray: radiance rgb, signed distance
DDGI_BIND( 1 ) RWStructuredBuffer<float4> DDGIIrradiance : register( u1 );    // per probe: 8x8 encoded rgb, a = sky visibility (M1FIX)
DDGI_BIND( 2 ) RWStructuredBuffer<float2> DDGIDistance : register( u2 );      // per probe: 16x16 mean, mean^2
DDGI_BIND( 3 ) RWStructuredBuffer<float4> DDGIProbes : register( u3 );        // offset xyz, state
DDGI_BIND( 4 ) RWStructuredBuffer<uint> DDGIAge : register( u4 );             // per probe: updates blended
DDGI_BIND( 5 ) StructuredBuffer<float4> DDGITriAttr : register( t5 );         // per soup triangle: albedo rgb + two-sided, Le rgb + 0
#ifdef DDGI_USE_TLAS
DDGI_BIND( 6 ) RaytracingAccelerationStructure DDGITLAS : register( t6 );
#endif
DDGI_BIND( 7 ) StructuredBuffer<int> DDGIRemap : register( t7 );             // BLAS primitive order -> soup triangle
DDGI_BIND( 8 ) StructuredBuffer<float> DDGIVerts : register( t8 );            // the true triangles, BLAS order, 9 floats
DDGI_BIND( 9 ) StructuredBuffer<int> DDGITriOf : register( t9 );              // soup triangle -> masked record
DDGI_BIND( 10 ) StructuredBuffer<uint> DDGIMaskTris : register( t10 );
DDGI_BIND( 11 ) StructuredBuffer<uint4> DDGIMaps : register( t11 );
DDGI_BIND( 12 ) StructuredBuffer<uint> DDGIAlpha : register( t12 );
DDGI_BIND( 13 ) StructuredBuffer<float4> DDGILights : register( t13 );        // 8 vec4 a light (src/features/gi/rtddgi.cpp packLights)
DDGI_BIND( 14 ) StructuredBuffer<DDGIParams> DDGIParamBuf : register( t14 );
[[vk::push_constant]] ConstantBuffer<DDGIJob> DDGIJobPc : register( b0 );
#define DDGI_P DDGIParamBuf[0]
#define DDGI_JOB DDGIJobPc

// HLSL has no gl_NumWorkGroups: rtddgi_vk.inc ddgiDispatch splits a dispatch into x = min(groups, 65535) and y = the rest,
// so whenever y > 0 the x count is exactly 65535
uint DDGI_LinearId( uint3 tid ) { return tid.x + tid.y * ( 65535u * 64u ); }   // 64-wide groups
uint DDGI_GroupId( uint3 gid ) { return gid.x + gid.y * 65535u; }

#endif
