// Lane VKFRAME1 round 8 (2026-11-12): the lens-flare sprites added to the tone-mapped frame, cell_lensflare.vert/.frag's twin.
// A raster pass only (blend ONE, ONE): entry vs (6 vertices a sprite, corners 0 1 2 0 2 3, the sprite's four corners read from
// T1, rgba32f 4 x 4N: row = sprite * 4 + corner, columns = position, colour, uv, light as GL's four attributes) and main (the
// fragment). T0 the sprite texture (linear), T2 the opaque depth. c[0..3] = flareProj's GL columns,
// c[4] = (flareViewport, flareSlack, -), i.x = flareRed. PFX_RED never occludes.
// profile: ps_6_0
#include "PostFxBindingsVk.hlsli"

struct FlareV
{
	float4 pos : SV_Position;
	float2 uv : TEXCOORD0;
	float3 col : TEXCOORD1;
};

FlareV vs( uint v : SV_VertexID )
{
	const PfxJob j = PfxJobPc;
	const uint corner[6] = { 0, 1, 2, 0, 2, 3 };
	const int row = int( ( v / 6 ) * 4 + corner[v % 6] );
	const float4 vp = PfxT1.Load( int3( 0, row, 0 ) );
	const float4 vc = PfxT1.Load( int3( 1, row, 0 ) );
	const float4 vu = PfxT1.Load( int3( 2, row, 0 ) );
	const float4 vl = PfxT1.Load( int3( 3, row, 0 ) );
	float seen = 1.0f;
#ifndef PFX_RED
	if ( ( j.i.x & 1 ) == 0 ) {
		const int2 c = int2( ( vl.xy * 0.5f + 0.5f ) * j.c[4].xy );
		const int2 hi = int2( j.c[4].xy ) - 1;
		float n = 0.0f;
		for ( int y = -2; y <= 2; y++ ) {
			for ( int x = -2; x <= 2; x++ ) {
				const int2 p = clamp( c + int2( x, y ) * 2, 0, hi );
				const float d = PfxT2.Load( int3( p, 0 ) ).r;
				const float sceneW = j.c[3].z / ( j.c[2].z + ( 2.0f * d - 1.0f ) );
				n += ( d >= 1.0f || sceneW + j.c[4].z >= vl.z ) ? 1.0f : 0.0f;
			}
		}
		seen = n / 25.0f;
	}
#endif
	const float size = vc.a > 0.5f ? seen : 1.0f;
	FlareV o;
	o.pos = float4( vp.xy + vp.zw * size, 0.0f, 1.0f );
	o.uv = vu.xy;
	o.col = vc.rgb * seen;
	return o;
}

float4 main( FlareV i ) : SV_Target0
{
	return float4( PfxT0.Sample( PfxLinear, i.uv ).rgb * i.col, 0.0f );
}
