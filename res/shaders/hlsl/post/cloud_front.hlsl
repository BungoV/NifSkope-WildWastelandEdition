// Lane VKFRAME1 round 8 (2026-11-12): the volume clouds over the scene after it drew, cloud_composite.frag's twin
// (CloudFrontCommon.hlsli). As a fragment pass it is GL's draw (blend SRC_ALPHA, ONE_MINUS_SRC_ALPHA, alpha ZERO, ONE, over
// the frame); the compute kernel writes the colour before the blend. T0 cvHist (linear), T1 cvHistDepth, T2 cvScene (point),
// T3 cvCam's three GL columns (rgba32f 3 x 1). c[0] = cvRect, c[1] = cvSceneZ, c[2] = cvProj, c[3] = cvLtSrc,
// c[4] = cvLtPrm, c[5] = (cvLtCol, sceneExposure), c[6] = (cvCamKm, -); i = (viewTransform, cvFront, cvLtOn, -).
// PFX_RED drops the depth test against the scene.
#include "PostFxBindingsVk.hlsli"
#include "CloudFrontCommon.hlsli"

static PfxJob J;

// the flash's light at this pixel (premultiplied, as the history's), a = the clouds' opacity
float3 CloudLtLight( float2 uv, float a )
{
	const float4 q = PfxT1.GatherRed( PfxLinear, uv );
	const float dm = min( min( q.x, q.y ), min( q.z, q.w ) );
	if ( dm >= 1e3f )
		return 0.0f;
	const float2 ndc = uv * 2.0f - 1.0f;
	const float3 v = float3( ( ndc.x + J.c[2].z ) / J.c[2].x, ( ndc.y + J.c[2].w ) / J.c[2].y, -1.0f );
	// GL's mat3 x vec3 (DXC's mul( v, rows ) is the same SPIR-V matrix op)
	const float3x3 cam = float3x3( PfxT3.Load( int3( 0, 0, 0 ) ).xyz, PfxT3.Load( int3( 1, 0, 0 ) ).xyz, PfxT3.Load( int3( 2, 0, 0 ) ).xyz );
	const float3 d = normalize( mul( v, cam ) );
	const float kRbKm = 6360.0f;
	const float3 e = float3( d.xy * dm, kRbKm + J.c[6].z + d.z * dm );
	const float3 p = float3( J.c[6].xy + d.xy * dm, length( e ) - kRbKm );	// GL's altitude: 0.5 m float steps at 6360 km
	return a * J.c[5].xyz * F4FX_CloudLtField( p, d, J.c[3], J.c[4] );
}

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 osize;
	if ( !PfxTexel( id, osize ) )
		return;
	J = PfxJobPc;
	const float2 uv = ( float2( id.xy ) + 0.5f - J.c[0].xy ) * J.c[0].zw;
	if ( J.i.y >= 1 ) {
		const float z = PfxT2.SampleLevel( PfxPoint, uv, 0.0f ).r;
		if ( z >= 1.0f )
			PFX_DISCARD();
		const float2 ndc = uv * 2.0f - 1.0f;
		const float3 v = float3( ( ndc.x + J.c[2].z ) / J.c[2].x, ( ndc.y + J.c[2].w ) / J.c[2].y, -1.0f );
		const float tScene = length( v ) * J.c[1].y / ( 2.0f * z - 1.0f + J.c[1].x ) * J.c[1].z;
		uint hw, hh;
		PfxT1.GetDimensions( hw, hh );
		const int2 hs = int2( hw, hh );
		const float dm = PfxT1.Load( int3( clamp( int2( uv * float2( hs ) ), 0, hs - 1 ), 0 ) ).r;
#ifndef PFX_RED
		if ( J.c[1].w > 0.5f && dm >= tScene )
			PFX_DISCARD();
#endif
	}
	float4 h = PfxT0.SampleLevel( PfxLinear, uv, 0.0f );
	const float a = clamp( 1.0f - h.a, 0.0f, 1.0f );
	if ( a <= 1e-4f )
		PFX_DISCARD();
	if ( J.i.z != 0 )
		h.rgb += CloudLtLight( uv, a );
	PFX_STORE0( id.xy, J.i.y == 2 ? float4( max( h.rgb, 0.0f ), a ) : float4( F4FX_StudioOutput( h.rgb / a, J.c[5].w, J.i.x ), a ) );
}
