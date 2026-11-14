// Air rays (lane VKCOMPUTE1, 2026-11-12; from src/features/gi/rtddgi/airrays.comp). airrays.comp's header:
// MODULES1 RESUME 15 item 2, route (b) of notes RESEARCH_AIR.md (MegaLights-style rays, Narkowicz & Costa 2025 pp. 14-20,
// 57-63): the placed lamps' shadow in the air from ray queries on the DDGI volume's BVH, one invocation a froxel of
// cell_volair's own grid. Per froxel: 2 lamps by weighted reservoir sampling over every lamp whose sphere holds the
// froxel centre (weight = luminance x falloff x spot; the air has no normal), one shadow ray each, the ratio estimator
// (shadowed / unshadowed of the sampled lamps, Heitz et al. 2018, as restir.comp mode 3) as running means of num and den
// reprojected from the previous frame's froxel. cell_volair.frag mode 2 multiplies the exact unshadowed lamp sum by it.
// Set 0 = the DDGI volume's descriptor set (ddgi_common.glsl), set 1 = the buffers below. No CPU twin yet (owed).
// HLSL: set 0 = DDGIBindingsVk.hlsli, set 1 = DDGIAirRaysBindingsVk.hlsli. Shared, no bindings: it reads DDGI_P, DDGILights (set 0 shim) and AIR_A, AirAcc, AirAccPrev, AirOutRatio (the air shim)
// by name. Ray queries: DXIL cs_6_5.
#ifndef DDGI_AIR_RAYS_HLSLI
#define DDGI_AIR_RAYS_HLSLI

#include "DDGIAirRaysTypes.hlsli"
#include "DDGIRayQuery.hlsli"
#include "RestirCommon.hlsli"

// a lamp's weight at an air point: restirTarget without the surface's cosine (the air scatters every way)
float Air_Target( uint b, float3 Pw )
{
	const float4 l0 = DDGILights[b], l1 = DDGILights[b + 1u], l2 = DDGILights[b + 2u], l3 = DDGILights[b + 3u];
	const float3 Lv = l0.xyz - Pw;
	const float d = length( Lv );
	if ( d >= l0.w )
		return 0.0;
	const int shape = int( DDGILights[b + 7u].x + 0.5 );
	if ( shape == 1 && dot( Pw - l0.xyz, l2.xyz ) < 0.0 )
		return 0.0;
	if ( shape == 2 ) {
		const float4 b0 = DDGILights[b + 4u], b1 = DDGILights[b + 5u], b2 = DDGILights[b + 6u];
		if ( abs( dot( b0.xyz, Pw ) + b0.w ) > 1.0 || abs( dot( b1.xyz, Pw ) + b1.w ) > 1.0 || abs( dot( b2.xyz, Pw ) + b2.w ) > 1.0 )
			return 0.0;
	}
	float att = DDGI_Radial( d, l0.w, l3.x, l3.y, l3.z );
	if ( l1.w > 0.5 )
		att = att * DDGI_Spot( -dot( Lv, l2.xyz ) / max( d, 0.001 ), l2.w, l3.w );
	return max( Restir_Lum( l1.x, l1.y, l1.z ) * att, 0.0 );
}

uint Air_Hash( uint x )
{
	x ^= x >> 16u;
	x *= 0x7feb352du;
	x ^= x >> 15u;
	x *= 0x846ca68bu;
	x ^= x >> 16u;
	return x;
}

float Air_Rand( inout uint s )
{
	s = Air_Hash( s );
	return float( s >> 8u ) * ( 1.0 / 16777216.0 );
}

// MIRROR restir.comp rsToWorld (rows = view -> world; their inverse below is the transpose about .w)
float3 Air_ToWorld( float4 r0, float4 r1, float4 r2, float3 v ) { return float3( dot( r0.xyz, v ) + r0.w, dot( r1.xyz, v ) + r1.w, dot( r2.xyz, v ) + r2.w ); }

// froxel (x, y, k centre) -> view point, as cell_volair.frag main: t along the normalized eye ray, exponential slices
float3 Air_View( float x, float y, float k, float4 pr )
{
	const float t = AIR_A.slice.x * exp( k / float( AIR_A.size.z ) * AIR_A.slice.y );
	const float2 ndc = float2( x / float( AIR_A.size.x ), y / float( AIR_A.size.y ) ) * 2.0 - 1.0;
	return normalize( float3( ( ndc.x + pr.z ) / pr.x, ( ndc.y + pr.w ) / pr.y, -1.0 ) ) * ( t / AIR_A.slice.w );
}

void Air_RaysCS( uint id )
{
	const uint n = uint( AIR_A.size.x * AIR_A.size.y * AIR_A.size.z );
	if ( id >= n )
		return;
	const int x = int( id ) % AIR_A.size.x, y = ( int( id ) / AIR_A.size.x ) % AIR_A.size.y, k = int( id ) / ( AIR_A.size.x * AIR_A.size.y );
	const float3 Pw = Air_ToWorld( AIR_A.rows[0], AIR_A.rows[1], AIR_A.rows[2], Air_View( float( x ) + 0.5, float( y ) + 0.5, float( k ) + 0.5, AIR_A.proj ) );

	// one pass over the lamps: S independent reservoirs of one lamp each (weighted reservoir sampling)
	const int S = clamp( AIR_A.opts.x, 1, 4 );
	uint seed = Air_Hash( id * 9781u + uint( AIR_A.size.w ) * 6271u + 1u );
	float wsum = 0.0;
	int pick[4] = { -1, -1, -1, -1 };
	for ( int i = 0; i < DDGI_P.counts.y; i++ ) {
		const float w = Air_Target( uint( i ) * 8u, Pw );
		if ( !( w > 0.0 ) )
			continue;
		wsum += w;
		for ( int s = 0; s < S; s++ )
			if ( Air_Rand( seed ) * wsum < w )
				pick[s] = i;
	}
	float num = 0.0, den = 0.0;
	if ( wsum > 0.0 ) {
		// each sample's w / pdf is wsum: num / den is the share of the sampled lamps' light that reaches the point
		for ( int s = 0; s < S; s++ ) {
			const uint b = uint( pick[s] ) * 8u;
			bool lit = true;
			if ( ( AIR_A.opts.z & AIR_BIT_NOSHADOW ) == 0 && DDGILights[b + 7u].z > 0.5 ) {
				const float3 L = DDGILights[b].xyz - Pw;
				const float d = length( L );
				const float3 dir = L / max( d, 1e-4 );
				lit = !DDGI_AnyHit( Pw + dir * AIR_A.slice.z, dir, d - 2.0 * AIR_A.slice.z, false, ( AIR_A.opts.z & AIR_BIT_NOHOLE ) != 0 );
			}
			num += lit ? wsum : 0.0;
			den += wsum;
		}
		num /= float( S );
		den /= float( S );
	}

	// history: the previous frame's froxel holding this world point, running means clamped to opts.y frames
	float4 a = float4( num, den, 1.0, 0.0 );
	if ( AIR_A.opts.w == 1 && ( AIR_A.opts.z & AIR_BIT_NOHIST ) == 0 ) {
		const float3 t = Pw - float3( AIR_A.prows[0].w, AIR_A.prows[1].w, AIR_A.prows[2].w );
		const float3 v = float3( dot( float3( AIR_A.prows[0].x, AIR_A.prows[1].x, AIR_A.prows[2].x ), t ), dot( float3( AIR_A.prows[0].y, AIR_A.prows[1].y, AIR_A.prows[2].y ), t ),
			dot( float3( AIR_A.prows[0].z, AIR_A.prows[1].z, AIR_A.prows[2].z ), t ) );
		const float tv = length( v ) * AIR_A.slice.w;
		if ( v.z < 0.0 && tv > AIR_A.slice.x ) {
			const float z = -v.z;
			const float2 ndc = float2( v.x / z * AIR_A.pproj.x - AIR_A.pproj.z, v.y / z * AIR_A.pproj.y - AIR_A.pproj.w );
			const float3 f = float3( ( ndc * 0.5 + 0.5 ) * float2( AIR_A.size.xy ), log( tv / AIR_A.slice.x ) / AIR_A.slice.y * float( AIR_A.size.z ) );
			const int3 c = int3( floor( f ) );
			if ( all( c >= int3( 0, 0, 0 ) ) && all( c < AIR_A.size.xyz ) ) {
				const float4 h = AirAccPrev[uint( c.x + ( c.y + c.z * AIR_A.size.y ) * AIR_A.size.x )];
#ifndef AIRRAYS_RED
				const float fr = min( h.z + 1.0, float( max( AIR_A.opts.y, 1 ) ) );
#else
				const float fr = h.z + 1.0;   // the gate's red: the history not clamped
#endif
				a = float4( lerp( h.xy, float2( num, den ), 1.0 / fr ), fr, 0.0 );
			}
		}
	}
	AirAcc[id] = a;
	AirOutRatio[id] = float2( a.y > 0.0 ? clamp( a.x / a.y, 0.0, 1.0 ) : 1.0, a.y > 0.0 ? a.z : 0.0 );
}

#endif
