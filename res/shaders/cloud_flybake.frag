#version 410 core

/* lane CLOUD4: the fly-through grid's occupancy (src/gl/cloudfly.cpp), one fragment = one cell of layer cfLayer.
 * 1 = cloudDensity (cloud_common.glsl) may be above 0 somewhere in the cell, 0 = it is 0 everywhere in it. The bound is
 * exact, not sampled: the shape noise's largest TEXEL over the cell (mips 0..2: a trilinear read never exceeds the texels
 * it mixes), the map's largest texel over the cell widened by cfSlackKm (the map's scroll between rebuilds), the height
 * profile's largest value over the cell's heights (cvGradient rises, holds, falls). The detail erosion only lowers the
 * density, so it is left out. cfProbe = 1 writes the densest of 6^3 points instead (the gate's census). */

#include "cloud_common.glsl"

uniform vec3 cfOrgKm;	// the box's corner, km (world x, y, height above z = 0)
uniform float cfCellKm;
uniform int cfLayer;
uniform float cfMargin;	// added to the shape bound (the filter's 8-bit weights)
uniform float cfSlackKm;
uniform int cfProbe;

layout ( location = 0 ) out vec4 cfOut;

//! the largest shape texel a trilinear read at mip lod can mix anywhere in [q0, q1] (noise km, wind added); 1 = too wide
float cfShapeMip( vec3 q0, vec3 q1, int lod )
{
	int n = textureSize( cvShape, lod ).x;
	vec3 u0 = q0 / kCvShapeKm * float( n ) - 0.5, u1 = q1 / kCvShapeKm * float( n ) - 0.5;
	ivec3 a = ivec3( floor( u0 ) ), b = ivec3( floor( u1 ) ) + 1;
	if ( any( greaterThan( b - a, ivec3( 7 ) ) ) )
		return 1.0;
	float m = 0.0;
	for ( int z = a.z; z <= b.z; z++ )
		for ( int y = a.y; y <= b.y; y++ )
			for ( int x = a.x; x <= b.x; x++ )
				m = max( m, texelFetch( cvShape, ivec3( x, y, z ) & ( n - 1 ), lod ).r );	// REPEAT, n a power of 2
	return m;
}

//! the largest Low, Mid coverage a bilinear map read can give anywhere in the world rect [xy0, xy1] (cvMapAt's rules)
vec2 cfCoverMax( vec2 xy0, vec2 xy1 )
{
	if ( cvBands.mapRect.w < 0.5 )
		return cvBands.coverage.xy;
	vec2 uv0 = ( xy0 - cvBands.mapRect.xy ) * cvBands.mapRect.z, uv1 = ( xy1 - cvBands.mapRect.xy ) * cvBands.mapRect.z;
	vec2 r = vec2( 0.0 );
	if ( any( lessThan( uv0, vec2( 0.0 ) ) ) || any( greaterThan( uv1, vec2( 1.0 ) ) ) )
		r = cvBands.coverage.xy;	// part of the rect is off the map
	if ( any( greaterThan( uv0, vec2( 1.0 ) ) ) || any( lessThan( uv1, vec2( 0.0 ) ) ) )
		return r;	// all of it
	ivec2 n = textureSize( cvMap, 0 );
	ivec2 a = clamp( ivec2( floor( clamp( uv0, 0.0, 1.0 ) * vec2( n ) - 0.5 ) ), ivec2( 0 ), n - 1 );
	ivec2 b = clamp( ivec2( floor( clamp( uv1, 0.0, 1.0 ) * vec2( n ) - 0.5 ) ) + 1, ivec2( 0 ), n - 1 );
	if ( any( greaterThan( b - a, ivec2( 15 ) ) ) )
		return vec2( 1.0 );
	for ( int y = a.y; y <= b.y; y++ )
		for ( int x = a.x; x <= b.x; x++ )
			r = max( r, texelFetch( cvMap, ivec2( x, y ), 0 ).rg );
	return r;
}

//! cvGradient's largest value over [h0, h1]: each profile rises to a plateau starting at x and falls after it
float cfGradMax( int b, float h0, float h1 )
{
	if ( b == 0 )
		return max( cvGradient( 0, clamp( 0.07, h0, h1 ), 0.0 ), cvGradient( 0, clamp( 0.04, h0, h1 ), 1.0 ) );
	return cvGradient( b, clamp( 0.15, h0, h1 ), 0.0 );
}

#ifdef WW_CLOUD_SHAPE
/* lane CLOUDSHAPE1: the largest body-field texel a read at mips 0..2 can mix anywhere in [q0, q1] (km, wind and shear
 * added, the band's offset off): the rect widened by 4 mip-0 texels; 1 = too wide to bound */
float cfCellMax( vec2 q0, vec2 q1, float off )
{
	int n = textureSize( cvCell, 0 ).x;
	vec2 u0 = ( q0 / kShpCellKm + off ) * float( n ) - 0.5, u1 = ( q1 / kShpCellKm + off ) * float( n ) - 0.5;
	ivec2 a = ivec2( floor( u0 ) ) - 4, b = ivec2( floor( u1 ) ) + 5;
	if ( any( greaterThan( b - a, ivec2( 40 ) ) ) )
		return 1.0;
	float m = 0.0;
	for ( int y = a.y; y <= b.y; y++ )
		for ( int x = a.x; x <= b.x; x++ )
			m = max( m, texelFetch( cvCell, ivec2( ( x % n + n ) % n, ( y % n + n ) % n ), 0 ).r );
	return m;
}
#endif

void main()
{
	ivec2 ij = ivec2( gl_FragCoord.xy );
	vec3 lo = cfOrgKm + vec3( ij, cfLayer ) * cfCellKm, hi = lo + vec3( cfCellKm );
	if ( cfProbe == 1 ) {
		cvFootKm = 0.0;
		float m = 0.0;
		for ( int k = 0; k < 216; k++ ) {
			vec3 f = ( vec3( k % 6, ( k / 6 ) % 6, k / 36 ) + 0.5 ) / 6.0;
			m = max( m, cloudDensity( mix( lo, hi, f ) ) );
		}
		cfOut = vec4( m );
		return;
	}
	vec2 cov = cfCoverMax( lo.xy - cfSlackKm, hi.xy + cfSlackKm );
	float s = -1.0;	// the shape bound, read once a band needs it
	float occ = 0.0;
	for ( int b = 0; b < 2; b++ ) {
		float base = cvBands.baseKm[b], top = cvBands.topKm[b];
		float h0 = ( lo.z - base ) / max( top - base, 1e-3 ), h1 = ( hi.z - base ) / max( top - base, 1e-3 );
		if ( h1 <= 0.0 || h0 >= 1.0 || cvBands.density[b] <= 0.0 )
			continue;
#ifdef WW_CLOUD_SHAPE
		{	/* lane CLOUDSHAPE1: f <= the body's largest strength - the reach at the cell's bottom (the reach rises with
			 * height: a dome or the sheet's hf x 4, a blend never below the smaller) - the base cut at its top + the
			 * largest lump; the erosion only lowers f */
			float c = cvCalAt( b, b == 0 ? cov.x : cov.y );
			if ( c <= 0.0 )
				continue;
			vec4 st = b == 0 ? cvShpT0 : cvShpT1;
			float hz0 = clamp( h0, 0.0, 1.0 ), hz1 = clamp( h1, 0.0, 1.0 );
			vec2 sa = kShpDir * ( kShpShearKm * ( lo.z - base ) ), sb = kShpDir * ( kShpShearKm * ( hi.z - base ) );
			vec2 q0 = lo.xy + min( sa, sb ) + cvBands.windKm.xy, q1 = hi.xy + max( sa, sb ) + cvBands.windKm.xy;
			float w = ( cfCellMax( q0, q1, 0.37 * float( b ) ) - ( 1.0 - c ) ) / max( c, 1e-3 );
			float r0 = min( pow( hz0 / max( st.x, 1e-3 ), st.y ), hz0 * 4.0 );
			if ( st.w > 0.0 ) {	// the towers' field (a blend of f and max(f, f_t) never exceeds max(f, f_t))
				float s3 = 1.0 / kShpTowerScale;
				float wt = ( cfCellMax( q0 * s3, q1 * s3, 0.21 ) - ( 1.0 - c ) ) / max( c, 1e-3 );
				w = max( w - r0, wt - 0.5 - 0.5 * pow( hz0, 1.2 ) ) + r0;
			}
			if ( w - r0 - ( 1.0 - cvSmooth( 0.0, kShpRise, hz1 ) ) + st.w + kShpLump * ( 1.0 - kShpMean ) / kShpSd > 0.0 )
				occ = 1.0;
			continue;
		}
#endif
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
			vec3 q0 = lo + cvBands.windKm, q1 = hi + cvBands.windKm;
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
	cfOut = vec4( occ );
}
