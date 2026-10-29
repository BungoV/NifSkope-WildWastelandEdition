/* lane AIRZONE1 (bungo 10-29): the hand-placed volumes' read (src/airzonemath.h holds the law, the packing and a
 * double reference; src/airzone.h the .recx files). Included only under WW_CELL_AIRZONE (the loader's define while the
 * Air zones row / WW_CELL_AIRZONE is on), by cell_volair.frag (the fog: one read a froxel sub-step) and cell_volpart.glsl
 * (the lit particles: one read a puff pixel; not in the lit-decal programs, WW_FX_DECALLIT, which sit at the limit).
 * Written in a C++-compatible subset (no swizzles, no out parameters, struct returns, scalar maths) so
 * tests/airzone/airzone_math_test.cpp compiles THIS text through its shim and compares it with the reference.
 * Outside every zone each read is the identity exactly (k 1, add 0, tint 1): the pixels of a cell with no zones. */

const int AZ_MAX = 8;			// MIRROR airzonemath.h kMax
const int AZ_VEC4 = 9;			// MIRROR airzonemath.h kVec4
uniform int azCount;			// zones packed this frame (0: none)
uniform int azRed;			// 1 hardedge (WW_CELL_AIRZONE_RED)
uniform vec4 azZ[AZ_MAX * AZ_VEC4];	// v0 center, type | v1 half size, edge | v2..v4 world->local rows (w: zBottom, flags,
					// 1 / heightFalloff) | v5 fog scale, add per unit, noise, 1 / noiseScale | v6 fog tint, radius^2 |
					// v7 noise offset, particle light | v8 particle tint, particle density

float azPartD = 1.0;			// the last azPartLit's density for azPartAlpha (1: no zone read, the opacity untouched)

struct AzFog
{
	float k;
	float add;
	vec3 tint;
};

struct AzPart
{
	float density;
	float light;
	vec3 tint;
};

// MIRROR airzonemath.h refSd
float azSd( vec3 l, int type, vec3 h )
{
	if ( type == 4 ) {
		vec3 q = vec3( l.x / h.x, l.y / h.y, l.z / h.z );
		return ( length( q ) - 1.0 ) * min( h.x, min( h.y, h.z ) );
	}
	if ( type == 3 ) {
		float r = min( h.x, h.y );
		float hz = max( h.z - r, 0.0 );
		vec3 d = vec3( l.x, l.y, l.z - clamp( l.z, -hz, hz ) );
		return length( d ) - r;
	}
	float bx = abs( l.x ) - h.x;
	float by = abs( l.y ) - h.y;
	float bz = abs( l.z ) - h.z;
	if ( type == 1 ) {
		float r = min( h.x, h.y );
		float a = ( length( vec3( l.x / h.x, l.y / h.y, 0.0 ) ) - 1.0 ) * r;
		float oa = max( a, 0.0 );
		float ob = max( bz, 0.0 );
		return sqrt( oa * oa + ob * ob ) + min( max( a, bz ), 0.0 );
	}
	float ox = max( bx, 0.0 );
	float oy = max( by, 0.0 );
	float oz = max( bz, 0.0 );
	float sd = sqrt( ox * ox + oy * oy + oz * oz ) + min( max( bx, max( by, bz ) ), 0.0 );
	if ( type == 2 )
		sd = max( sd, ( l.y * h.z + l.z * h.y ) / sqrt( h.y * h.y + h.z * h.z ) );
	return sd;
}

// MIRROR airzonemath.h refWeight: 1 deep inside, 0 on the surface and outside (the edge is inside the shape)
float azWeight( int i, vec3 P )
{
	int b = i * AZ_VEC4;
	vec4 c = azZ[b];
	vec4 hs = azZ[b + 1];
	vec3 d = vec3( P.x - c.x, P.y - c.y, P.z - c.z );
	if ( dot( d, d ) > azZ[b + 6].w )
		return 0.0;
	vec4 r0 = azZ[b + 2];
	vec4 r1 = azZ[b + 3];
	vec4 r2 = azZ[b + 4];
	vec3 l = vec3( r0.x * d.x + r0.y * d.y + r0.z * d.z, r1.x * d.x + r1.y * d.y + r1.z * d.z, r2.x * d.x + r2.y * d.y + r2.z * d.z );
	float sd = azSd( l, int( c.w ), vec3( hs.x, hs.y, hs.z ) );
	if ( hs.w <= 0.0 || ( azRed & 1 ) != 0 )
		return sd <= 0.0 ? 1.0 : 0.0;
	return smoothstep( 0.0, hs.w, -sd );
}

// MIRROR airzonemath.h refHash
float azHash( int x, int y, int z )
{
	uint h = ( uint( x ) * 73856093u ) ^ ( uint( y ) * 19349663u ) ^ ( uint( z ) * 83492791u );
	h = ( h ^ ( h >> 13u ) ) * 1274126177u;
	h = h ^ ( h >> 16u );
	return float( h & 16777215u ) * ( 1.0 / 16777216.0 );
}

// MIRROR airzonemath.h refNoise: one loop over the 8 corners (one copy of the hash)
float azNoise( vec3 p )
{
	float fx = floor( p.x );
	float fy = floor( p.y );
	float fz = floor( p.z );
	int ix = int( fx );
	int iy = int( fy );
	int iz = int( fz );
	float tx = p.x - fx;
	float ty = p.y - fy;
	float tz = p.z - fz;
	float ux = tx * tx * ( 3.0 - 2.0 * tx );
	float uy = ty * ty * ( 3.0 - 2.0 * ty );
	float uz = tz * tz * ( 3.0 - 2.0 * tz );
	float n = 0.0;
	for ( int k = 0; k < 8; k++ ) {
		int dx = k & 1;
		int dy = ( k >> 1 ) & 1;
		int dz = ( k >> 2 ) & 1;
		float w = ( dx == 1 ? ux : 1.0 - ux ) * ( dy == 1 ? uy : 1.0 - uy ) * ( dz == 1 ? uz : 1.0 - uz );
		n += w * azHash( ix + dx, iy + dy, iz + dz );
	}
	return n;
}

// MIRROR airzonemath.h refFog: P world; tau' = tau k + add len, the scattered light x tint
AzFog azFogAt( vec3 P )
{
	AzFog o;
	o.k = 0.0;
	o.add = 0.0;
	o.tint = vec3( 0.0 );
	float r = 1.0;
	for ( int i = 0; i < azCount; i++ ) {
		int b = i * AZ_VEC4;
		if ( ( int( azZ[b + 3].w ) & 1 ) == 0 )
			continue;
		float w = azWeight( i, P );
		if ( w <= 0.0 )
			continue;
		float s = r * w;
		vec4 f = azZ[b + 5];
		float hf = 1.0;
		if ( azZ[b + 4].w > 0.0 )
			hf = exp( -max( P.z - azZ[b + 2].w, 0.0 ) * azZ[b + 4].w );
		float n = 1.0;
		if ( f.z > 0.0 ) {
			vec4 o7 = azZ[b + 7];
			n = max( 1.0 + f.z * ( 2.0 * azNoise( vec3( P.x * f.w + o7.x, P.y * f.w + o7.y, P.z * f.w + o7.z ) ) - 1.0 ), 0.0 );
		}
		vec4 t = azZ[b + 6];
		o.k += s * f.x;
		o.add += s * f.y * hf * n;
		o.tint = o.tint + vec3( t.x, t.y, t.z ) * s;
		r *= 1.0 - w;
		if ( r <= 0.0 )
			break;
	}
	o.k += r;
	o.tint = o.tint + vec3( r );
	return o;
}

// MIRROR airzonemath.h refPart
AzPart azPartAt( vec3 P )
{
	AzPart o;
	o.density = 0.0;
	o.light = 0.0;
	o.tint = vec3( 0.0 );
	float r = 1.0;
	for ( int i = 0; i < azCount; i++ ) {
		int b = i * AZ_VEC4;
		if ( ( int( azZ[b + 3].w ) & 2 ) == 0 )
			continue;
		float w = azWeight( i, P );
		if ( w <= 0.0 )
			continue;
		float s = r * w;
		vec4 t = azZ[b + 8];
		o.density += s * t.w;
		o.light += s * azZ[b + 7].w;
		o.tint = o.tint + vec3( t.x, t.y, t.z ) * s;
		r *= 1.0 - w;
		if ( r <= 0.0 )
			break;
	}
	o.density += r;
	o.light += r;
	o.tint = o.tint + vec3( r );
	return o;
}

// a lit puff's light at world P (one read; keeps the density for azPartAlpha)
vec3 azPartLit( vec3 P, vec3 lit )
{
	AzPart p = azPartAt( P );
	azPartD = p.density;
	return lit * p.tint * p.light;
}

// MIRROR airzonemath.h refPartAlpha
float azPartAlpha( float a )
{
	if ( azPartD == 1.0 )
		return a;
	if ( azPartD <= 0.0 )
		return 0.0;
	return 1.0 - pow( max( 1.0 - a, 0.0 ), azPartD );
}
