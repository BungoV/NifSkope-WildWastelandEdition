// Bake tracer: NifSkope's shim (lane VKCOMPUTE1, 2026-11-12). The ONE file that binds the shared bake-tracer kernels to
// bakert.cpp's descriptor set 0 (src/features/gi/bakert/common.glsl + each .comp's bindings 6-8) and its 32-byte push
// constants. The entry defines BAKERT_RQ, BAKERT_BVH or BAKERT_RQX before including it. Fusion Shaders (F4FS) writes its
// own with the same names. register() gives DXIL the same slots (t = read-only, u = read-write).
#ifndef BAKERT_BINDINGS_VK_HLSLI
#define BAKERT_BINDINGS_VK_HLSLI

#include "BakeRTTypes.hlsli"

#ifdef __spirv__
#define BAKERT_BIND( b ) [[vk::binding( b, 0 )]]
#else
#define BAKERT_BIND( b )
#endif

#ifdef BAKERT_RQX
BAKERT_BIND( 0 ) StructuredBuffer<BakeRTXRay> BakeRTXRays : register( t0 );
#else
BAKERT_BIND( 0 ) StructuredBuffer<BakeRTRay> BakeRTRays : register( t0 );
#endif
// binding 1, see each kernel: bvh / rq (common.glsl)
// t bits, tri (0xFFFFFFFF = miss)
// rqx.comp (tri, then the unsure reason bits):
// tri (0xFFFFFFFF = miss), 1 = unsure
BAKERT_BIND( 1 ) RWStructuredBuffer<uint2> BakeRTHits : register( u1 );   // see each kernel
BAKERT_BIND( 2 ) StructuredBuffer<int> BakeRTTriOf : register( t2 );      // soup triangle -> masked record
BAKERT_BIND( 3 ) StructuredBuffer<uint> BakeRTMaskTris : register( t3 );  // 12 words a masked triangle
BAKERT_BIND( 4 ) StructuredBuffer<uint4> BakeRTMaps : register( t4 );     // byte offset, w, h, 0
BAKERT_BIND( 5 ) StructuredBuffer<uint> BakeRTAlpha : register( t5 );     // the maps' alpha bytes, 4 a word
#ifdef BAKERT_BVH
BAKERT_BIND( 6 ) StructuredBuffer<uint> BakeRTNodes : register( t6 );     // 9 words a node: lo3 hi3 left first count
BAKERT_BIND( 7 ) StructuredBuffer<int> BakeRTIdx : register( t7 );
BAKERT_BIND( 8 ) StructuredBuffer<float> BakeRTTris : register( t8 );     // 9 floats a triangle
#else
BAKERT_BIND( 6 ) RaytracingAccelerationStructure BakeRTTLAS : register( t6 );
// rqx.comp's words for binding 7:
// BLAS primitive -> soup triangle
BAKERT_BIND( 7 ) StructuredBuffer<int> BakeRTRemap : register( t7 );      // BLAS primitive order -> soup triangle
#endif
#ifdef BAKERT_RQX
// the true triangle:
BAKERT_BIND( 8 ) StructuredBuffer<float> BakeRTVerts : register( t8 );    // 9 floats a BLAS primitive
#endif
[[vk::push_constant]] ConstantBuffer<BakeRTPc> BakeRTPcCb : register( b0 );
#define BAKERT_PC BakeRTPcCb

#endif
