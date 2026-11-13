// HLSL source of features/clouds/cloud_flybake.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CloudsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#ifndef WW_GLSL_SIZE_Texture3D_float4_
#define WW_GLSL_SIZE_Texture3D_float4_
int3 textureSize(Texture3D<float4> t, int lod) { uint w, h, d, l; t.GetDimensions(uint(lod), w, h, d, l); return int3(w, h, d); }
#endif
#ifndef WW_GLSL_SIZE_Texture2D_float4_
#define WW_GLSL_SIZE_Texture2D_float4_
int2 textureSize(Texture2D<float4> t, int lod) { uint w, h, l; t.GetDimensions(uint(lod), w, h, l); return int2(w, h); }
#endif
static float4 gl_FragCoord;
/* lane CLOUD4: the fly-through grid's occupancy (src/features/clouds/cloudfly.cpp), one fragment = one cell of layer cfLayer.
 * 1 = cloudDensity (cloud_common.glsl) may be above 0 somewhere in the cell, 0 = it is 0 everywhere in it. The bound is
 * exact, not sampled: the shape noise's largest TEXEL over the cell (mips 0..2: a trilinear read never exceeds the texels
 * it mixes), the map's largest texel over the cell widened by cfSlackKm (the map's scroll between rebuilds), the height
 * profile's largest value over the cell's heights (cvGradient rises, holds, falls). The detail erosion only lowers the
 * density, so it is left out. cfProbe = 1 writes the densest of 6^3 points instead (the gate's census). */

#define WW_LOOSE_TYPES 1
#include "cloud_common.hlsli"
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "cloud_common.hlsli"
#undef WW_LOOSE_UNIFORMS
	float3 cfOrgKm;	// the box's corner, km (world x, y, height above z = 0)
	float cfCellKm;
	int cfLayer;
	float cfMargin;	// added to the shape bound (the filter's 8-bit weights)
	float cfSlackKm;
	int cfProbe;
};
#include "cloud_common.hlsli"



static float4 cfOut;

//! the largest shape texel a trilinear read at mip lod can mix anywhere in [q0, q1] (noise km, wind added); 1 = too wide
float cfShapeMip( float3 q0, float3 q1, int lod )
{
	int n = textureSize( cvShape, lod ).x;
	float3 u0 = q0 / kCvShapeKm * float( n ) - 0.5, u1 = q1 / kCvShapeKm * float( n ) - 0.5;
	int3 a = int3( floor( u0 ) ), b = int3( floor( u1 ) ) + 1;
	if ( any( ((b - a) > (int3)( 7 )) ) )
		return 1.0;
	float m = 0.0;
	for ( int z = a.z; z <= b.z; z++ )
		for ( int y = a.y; y <= b.y; y++ )
			for ( int x = a.x; x <= b.x; x++ )
				m = max( m, cvShape.Load( int4(int3( x, y, z ) & ( n - 1 ), lod) ).r );	// REPEAT, n a power of 2
	return m;
}

//! the largest Low, Mid coverage a bilinear map read can give anywhere in the world rect [xy0, xy1] (cvMapAt's rules)
float2 cfCoverMax( float2 xy0, float2 xy1 )
{
	if ( cvBands.mapRect.w < 0.5 )
		return cvBands.coverage.xy;
	float2 uv0 = ( xy0 - cvBands.mapRect.xy ) * cvBands.mapRect.z, uv1 = ( xy1 - cvBands.mapRect.xy ) * cvBands.mapRect.z;
	float2 r = (float2)( 0.0 );
	if ( or( any( (uv0 < (float2)( 0.0 )) ), any( (uv1 > (float2)( 1.0 )) ) ) )
		r = cvBands.coverage.xy;	// part of the rect is off the map
	if ( or( any( (uv0 > (float2)( 1.0 )) ), any( (uv1 < (float2)( 0.0 )) ) ) )
		return r;	// all of it
	int2 n = textureSize( cvMap, 0 );
	int2 a = clamp( int2( floor( clamp( uv0, 0.0, 1.0 ) * float2( n ) - 0.5 ) ), (int2)( 0 ), n - 1 );
	int2 b = clamp( int2( floor( clamp( uv1, 0.0, 1.0 ) * float2( n ) - 0.5 ) ) + 1, (int2)( 0 ), n - 1 );
	if ( any( ((b - a) > (int2)( 15 )) ) )
		return (float2)( 1.0 );
	for ( int y = a.y; y <= b.y; y++ )
		for ( int x = a.x; x <= b.x; x++ )
			r = max( r, cvMap.Load( int3(int2( x, y ), 0) ).rg );
	return r;
}

//! cvGradient's largest value over [h0, h1]: each profile rises to a plateau starting at x and falls after it
float cfGradMax( int b, float h0, float h1 )
{
	if ( b == 0 )
		return max( cvGradient( 0, clamp( 0.07, h0, h1 ), 0.0 ), cvGradient( 0, clamp( 0.04, h0, h1 ), 1.0 ) );
	return cvGradient( b, clamp( 0.15, h0, h1 ), 0.0 );
}

void frag_main()
{
	int2 ij = int2( gl_FragCoord.xy );
	float3 lo = cfOrgKm + float3( ij, cfLayer ) * cfCellKm, hi = lo + (float3)( cfCellKm );
	if ( cfProbe == 1 ) {
		cvFootKm = 0.0;
		float m = 0.0;
		for ( int k = 0; k < 216; k++ ) {
			float3 f = ( float3( k % 6, ( k / 6 ) % 6, k / 36 ) + 0.5 ) / 6.0;
			m = max( m, cloudDensity( lerp( lo, hi, f ) ) );
		}
		cfOut = (float4)( m );
		return;
	}
	float2 cov = cfCoverMax( lo.xy - cfSlackKm, hi.xy + cfSlackKm );
	float s = -1.0;	// the shape bound, read once a band needs it
	float occ = 0.0;
	for ( int b = 0; b < 2; b++ ) {
		float base = cvBands.baseKm[b], top = cvBands.topKm[b];
		float h0 = ( lo.z - base ) / max( top - base, 1e-3 ), h1 = ( hi.z - base ) / max( top - base, 1e-3 );
		if ( or( or( h1 <= 0.0, h0 >= 1.0 ), cvBands.density[b] <= 0.0 ) )
			continue;
#ifdef WW_CLOUD_VARIETY
		/* lane CLOUD5: the weather noise's swing raises the coverage by at most swing x min(c, 1 - c) (rising in c); the
		 * type tables are bounded by their largest anchor (a blend never exceeds both), the anvil by sqrt (its power >=
		 * 0.5); every type profile is <= 1 (gm below); the curl moves only the detail read, which only erodes */
		float cv = b == 0 ? cov.x : cov.y;
		cv += cvVarA.z * min( cv, 1.0 - cv );
		float c = b == 0 ? sqrt( max( max( cvCalAtT( 0.0, cv ), cvCalAtT( 1.0 / 3.0, cv ) ),
								 max( cvCalAtT( 2.0 / 3.0, cv ), cvCalAtT( 1.0, cv ) ) ) )
						 : cvCalAt( 1, cv );
#else
		float c = cvCalAt( b, b == 0 ? cov.x : cov.y );	// cvCalAt rises with the coverage (the gate checks it)
#endif
		if ( c <= 0.0 )
			continue;
		if ( s < 0.0 ) {
			float3 q0 = lo + cvBands.windKm, q1 = hi + cvBands.windKm;
			s = max( cfShapeMip( q0, q1, 0 ), max( cfShapeMip( q0, q1, 1 ), cfShapeMip( q0, q1, 2 ) ) );
			s = min( s + cfMargin, 1.0 );
		}
#ifdef WW_CLOUD_VARIETY
		float gm = b == 0 ? 1.0 : cfGradMax( b, clamp( h0, 0.0, 1.0 ), clamp( h1, 0.0, 1.0 ) );
		if ( s * gm > 1.0 - c )
#else
		if ( s * cfGradMax( b, clamp( h0, 0.0, 1.0 ), clamp( h1, 0.0, 1.0 ) ) > 1.0 - c )
#endif
			occ = 1.0;
	}
	cfOut = (float4)( occ );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 cfOut : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.cfOut = cfOut;
    return stage_output;
}
