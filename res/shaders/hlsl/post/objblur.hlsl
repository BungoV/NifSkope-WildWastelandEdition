// Lane VKFRAME1 P7 (2026-11-12): per-object motion blur pass 4 on Vulkan, cell_objblur.frag's twin (ObjBlurCommon.hlsli): a
// depth-compared gather over the neighbour max. T0 the frame, T1 the pixel vectors, T2 the neighbour max, T3 the depth;
// i = (tile, samples), c[0] = (near, far, noise seed, -). PFX_RED counts every sample fully (no reach, no depth test).
#include "PostFxBindingsVk.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !PfxTexel( id, size ) )
		return;
	const PfxJob j = PfxJobPc;
	const int2 X = int2( id.xy );
	const float2 fc = float2( id.xy ) + 0.5f;
	uint cw, ch;
	PfxT0.GetDimensions( cw, ch );
	const int2 sz = int2( cw, ch );
	const float4 cX = PfxT0.Load( int3( X, 0 ) );
	const float2 vn = PfxT2.Load( int3( X / j.i.x, 0 ) ).xy * 0.5f;
	const float ln = length( vn );
	if ( ln < 0.5f ) {
		PfxOut0[id.xy] = cX;
		return;
	}
	const float2 vs = vn * ( ( ln + 0.5f ) / ln );
	const float rx = length( PfxT1.Load( int3( X, 0 ) ).xy ) * 0.5f;
	const float zNear = j.c[0].x, zFar = j.c[0].y;
	const float zx = F4FX_ObjBlurDist( PfxT3.Load( int3( X, 0 ) ).r, zNear, zFar );
	const float k = max( 0.02f * zx, 1.0f );
	float wsum = 1.0f;
	float4 sum = cX;
	const float jit = F4FX_ObjBlurJitter( fc, j.c[0].z );
	const int n = max( j.i.y, 2 );
	for ( int i = 0; i < n; i++ ) {
		const float t = ( ( float( i ) + jit + 1.0f ) / float( n + 1 ) ) * 2.0f + -1.0f;	// GL mix( -1, 1, x )'s order
		const int2 Y = clamp( int2( floor( float2( X ) + 0.5f + vs * t ) ), int2( 0, 0 ), sz - 1 );
		const float d = length( float2( Y - X ) );
		const float ry = length( PfxT1.Load( int3( Y, 0 ) ).xy ) * 0.5f;
		const float zy = F4FX_ObjBlurDist( PfxT3.Load( int3( Y, 0 ) ).r, zNear, zFar );
#ifdef PFX_RED
		const float a = 1.0f;
#else
		const float a = F4FX_ObjBlurWeight( d, rx, ry, zx, zy, k );
#endif
		wsum += a;
		sum += a * PfxT0.Load( int3( Y, 0 ) );
	}
	PfxOut0[id.xy] = sum / wsum;	// alpha: wsum / wsum, 1 or 1 - 1 ulp by either side's divide (the gate's "gather" rule)
}
