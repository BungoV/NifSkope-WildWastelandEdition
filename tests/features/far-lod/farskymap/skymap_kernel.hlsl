// FARSKYMAP1 G2: the far sky map's shader math (res/shaders/hlsl/far-lod/far_skymap.hlsli) on the GPU, per case,
// against its C++ twin (src/features/far-lod/farskymap.h) in skymapmatch.cpp. Texture sampling is not in it.
#define WW_HLSL_SRC 1
#define FARSKY_MATH_ONLY 1
cbuffer LooseUniforms : register(b0, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "../../../../res/shaders/hlsl/far-lod/far_skymap.hlsli"
#undef WW_LOOSE_UNIFORMS
};
#include "../../../../res/shaders/hlsl/far-lod/far_skymap.hlsli"

// per case 4 float4 in (a, b, n.xyz + base, p.xyz), 2 float4 out
RWStructuredBuffer<float4> io : register(u1, space0);

[numthreads(64, 1, 1)]
void main( uint3 id : SV_DispatchThreadID )
{
	uint i = id.x;
	float4 a = io[i * 6 + 0], b = io[i * 6 + 1], nb = io[i * 6 + 2], p = io[i * 6 + 3];
	// p.w = the shape: 0 near, 1..3 a far draw's baked type, 4 a far placement (farInst)
	bool far = farSkyShapeFar( p.w > 3.5, p.w > 3.5 ? 0.0 : p.w );
	float2 vv = farSkyDir6( a, b, nb.xyz );
	float c = farSkyCombine( nb.w, farSkyOn && far, vv );
	float2 uv = farSkyMapUv( p.xyz );
	io[i * 6 + 4] = float4( vv, c, uv.x );
	io[i * 6 + 5] = float4( uv.y, far ? 1.0 : 0.0, 0.0, 1.0 );
}
