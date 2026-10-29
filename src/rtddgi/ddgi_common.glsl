// Lane RTXGI1 (2026-10-29): what the DDGI kernels share -- the bindings, the volume's parameters and the probe math.
// Every function marked MIRROR is src/rtddgimath.h's function of that name, line for line (the C++ is the twin the
// unit test checks; change both or neither). Compiled into trace/blend/border/reloc.comp by tools/rtddgi_spirv.sh.

#define DDGI_PI 3.14159265358979323846
#define DDGI_FIXED 32
#define DDGI_IRR_N 6
#define DDGI_IRR_S 8
#define DDGI_DIST_N 14
#define DDGI_DIST_S 16
#define DDGI_MISS 1e27
#define DDGI_TMIN 1e-4   // rtddgimath.h kTraceTmin: probebvh's t > 1e-4
#define DDGI_OFF 0
#define DDGI_ASLEEP 1
#define DDGI_VIGILANT 2
#define DDGI_FRESH 3     // M3: rtddgimath.h kFresh, a probe a scroll reset (relocated + classified alone, never read)
// red bits (params.flags.w): the volume's runtime refuters (src/rtddgi.h RtDdgiSpec::red)
#define DDGI_RED_NOBACK 1u   // backface hits shaded as front faces, distance kept
#define DDGI_RED_NOCHEB 2u   // the bounce sampler without the Chebyshev factor
#define DDGI_RED_NOBIAS 4u   // the bounce sampler without the self-shadow bias
#define DDGI_RED_HWHIT 8u    // GPU only: M1's trace, the hardware's hit on the grown BLAS (no true-triangle test, tmin 0, no tie pass)
#define DDGI_RED_NOTIE 16u   // no tie pass: the nearest face whichever the tree keeps (CPU and GPU)

struct Params {
	ivec4 dims;       // probes x y z, total
	vec4 origin;      // the grid's first probe (the soup's local frame), w = distClamp
	vec4 stepv;       // probe spacing per axis, w = the smallest
	vec4 sunTo;       // toward the sun, w = 1 on
	vec4 sun;         // its colour, w = the trace tmax
	vec4 dirTo;       // an interior's directional light, w = 1 on
	vec4 dirColor;    // its colour, w = fixtureClear
	vec4 sky[6];      // the sky radiance cube (+X -X +Y -Y +Z -Z), sky[0].w = 1 on
	ivec4 counts;     // rays per probe, lights, soup triangles, opaque triangles (the BLAS's geometry 0)
	ivec4 flags;      // oneBounce, skipAsleep, useMask, red bits
	vec4 blendp;      // base hysteresis, distance sharpness, self-shadow bias B, 0
};

layout( std430, set = 0, binding = 0 ) buffer Rays { vec4 rays[]; };            // per probe x ray: radiance rgb, signed distance
layout( std430, set = 0, binding = 1 ) buffer Irr { vec4 irr[]; };              // per probe: 8x8 encoded rgb, a = sky visibility (M1FIX)
layout( std430, set = 0, binding = 2 ) buffer Dist { vec2 dist[]; };            // per probe: 16x16 mean, mean^2
layout( std430, set = 0, binding = 3 ) buffer Probes { vec4 probes[]; };        // offset xyz, state
layout( std430, set = 0, binding = 4 ) buffer Age { uint age[]; };              // per probe: updates blended
layout( std430, set = 0, binding = 5 ) readonly buffer TriAttr { vec4 triAttr[]; };   // per soup triangle: albedo rgb + two-sided, Le rgb + 0
layout( set = 0, binding = 6 ) uniform accelerationStructureEXT tlas;
layout( std430, set = 0, binding = 7 ) readonly buffer Remap { int remap[]; };   // BLAS primitive order -> soup triangle
layout( std430, set = 0, binding = 8 ) readonly buffer Verts { float verts[]; }; // the true triangles, BLAS order, 9 floats
layout( std430, set = 0, binding = 9 ) readonly buffer TriOf { int triOf[]; };   // soup triangle -> masked record
layout( std430, set = 0, binding = 10 ) readonly buffer MaskTris { uint mt[]; };
layout( std430, set = 0, binding = 11 ) readonly buffer Maps { uvec4 maps[]; };
layout( std430, set = 0, binding = 12 ) readonly buffer Alpha { uint ab[]; };
layout( std430, set = 0, binding = 13 ) readonly buffer Lights { vec4 lights[]; };  // 8 vec4 a light (src/rtddgi.cpp packLights)
layout( std430, set = 0, binding = 14 ) readonly buffer ParamBuf { Params P; };

// mode 0 trace an update, 1 trace the fixed rays (relocation / classification), 2 blend, 3 border, 4 relocate, 5 classify
// M3: modes 1, 4 and 5 with job.y = 1 take only DDGI_FRESH probes (a scroll's new planes); 0 = every probe (M1's)
layout( push_constant ) uniform Pc { vec4 rot0; vec4 rot1; vec4 rot2; uvec4 job; } pc;   // job: mode, frame, ray first, ray count

uint ddgiLinearId() { return gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * ( gl_NumWorkGroups.x * gl_WorkGroupSize.x ); }
uint ddgiGroupId() { return gl_WorkGroupID.x + gl_WorkGroupID.y * gl_NumWorkGroups.x; }

float signNotZero( float v ) { return v >= 0.0 ? 1.0 : -1.0; }

// MIRROR rtddgimath.h octEncode
vec2 ddgiOctEncode( vec3 d )
{
	const float l1 = abs( d.x ) + abs( d.y ) + abs( d.z );
	vec2 uv = d.xy / l1;
	if ( d.z < 0.0 ) {
		const float ou = uv.x;
		uv.x = ( 1.0 - abs( uv.y ) ) * signNotZero( ou );
		uv.y = ( 1.0 - abs( ou ) ) * signNotZero( uv.y );
	}
	return uv;
}
// MIRROR rtddgimath.h octDecode
vec3 ddgiOctDecode( vec2 uv )
{
	vec3 d = vec3( uv, 1.0 - abs( uv.x ) - abs( uv.y ) );
	if ( d.z < 0.0 ) {
		const float ox = d.x;
		d.x = ( 1.0 - abs( d.y ) ) * signNotZero( ox );
		d.y = ( 1.0 - abs( ox ) ) * signNotZero( d.y );
	}
	return normalize( d );
}
// MIRROR rtddgimath.h texelDir
vec3 ddgiTexelDir( int tx, int ty, int n )
{
	return ddgiOctDecode( vec2( ( float( tx ) + 0.5 ) / float( n ) * 2.0 - 1.0, ( float( ty ) + 0.5 ) / float( n ) * 2.0 - 1.0 ) );
}
// MIRROR rtddgimath.h borderSource
ivec2 ddgiBorderSource( int x, int y, int S )
{
	const int n = S - 2;
	ivec2 s = ivec2( x, y );
	const bool l = x == 0, r = x == S - 1, t = y == 0, b = y == S - 1;
	if ( ( l || r ) && ( t || b ) )
		return ivec2( l ? n : 1, t ? n : 1 );
	if ( t ) s = ivec2( S - 1 - x, 1 );
	if ( b ) s = ivec2( S - 1 - x, n );
	if ( l ) s = ivec2( 1, S - 1 - y );
	if ( r ) s = ivec2( n, S - 1 - y );
	return s;
}
// MIRROR rtddgimath.h borderXY
ivec2 ddgiBorderXY( int k, int S )
{
	if ( k < S ) return ivec2( k, 0 );
	k -= S;
	if ( k < S ) return ivec2( k, S - 1 );
	k -= S;
	if ( k < S - 2 ) return ivec2( 0, 1 + k );
	k -= S - 2;
	return ivec2( S - 1, 1 + k );
}
// MIRROR rtddgimath.h sampleTile (the bilinear corner and weights; one reader per buffer below)
void ddgiTileTaps( vec3 d, int S, out ivec2 x0y0, out vec2 a )
{
	const vec2 uv = ddgiOctEncode( d );
	const int n = S - 2;
	const vec2 f = vec2( 1.0 ) + ( uv * 0.5 + vec2( 0.5 ) ) * float( n ) - vec2( 0.5 );
	x0y0 = clamp( ivec2( floor( f ) ), ivec2( 0 ), ivec2( S - 2 ) );
	a = clamp( f - vec2( x0y0 ), vec2( 0.0 ), vec2( 1.0 ) );
}
vec3 ddgiSampleIrr( uint p, vec3 d )
{
	ivec2 c;
	vec2 a;
	ddgiTileTaps( d, DDGI_IRR_S, c, a );
	const uint b = p * uint( DDGI_IRR_S * DDGI_IRR_S );
	const vec3 t00 = irr[b + uint( c.y * DDGI_IRR_S + c.x )].rgb, t10 = irr[b + uint( c.y * DDGI_IRR_S + c.x + 1 )].rgb;
	const vec3 t01 = irr[b + uint( ( c.y + 1 ) * DDGI_IRR_S + c.x )].rgb, t11 = irr[b + uint( ( c.y + 1 ) * DDGI_IRR_S + c.x + 1 )].rgb;
	return mix( mix( t00, t10, a.x ), mix( t01, t11, a.x ), a.y );
}
vec2 ddgiSampleDist( uint p, vec3 d )
{
	ivec2 c;
	vec2 a;
	ddgiTileTaps( d, DDGI_DIST_S, c, a );
	const uint b = p * uint( DDGI_DIST_S * DDGI_DIST_S );
	const vec2 t00 = dist[b + uint( c.y * DDGI_DIST_S + c.x )], t10 = dist[b + uint( c.y * DDGI_DIST_S + c.x + 1 )];
	const vec2 t01 = dist[b + uint( ( c.y + 1 ) * DDGI_DIST_S + c.x )], t11 = dist[b + uint( ( c.y + 1 ) * DDGI_DIST_S + c.x + 1 )];
	return mix( mix( t00, t10, a.x ), mix( t01, t11, a.x ), a.y );
}
// MIRROR rtddgimath.h sphericalFibonacci
vec3 ddgiFibonacci( int i, int n )
{
	const float kPhiMinus1 = 0.61803398874989484820;
	const float phi = 2.0 * DDGI_PI * fract( float( i ) * kPhiMinus1 );
	const float cosT = 1.0 - ( 2.0 * float( i ) + 1.0 ) / float( n );
	const float sinT = sqrt( clamp( 1.0 - cosT * cosT, 0.0, 1.0 ) );
	return vec3( cos( phi ) * sinT, sin( phi ) * sinT, cosT );
}
// MIRROR rtddgimath.h rayDir (the rotation's rows in pc.rot0..2)
vec3 ddgiRayDir( int i, int n )
{
	if ( i < DDGI_FIXED )
		return ddgiFibonacci( i, DDGI_FIXED );
	const vec3 f = ddgiFibonacci( i - DDGI_FIXED, n - DDGI_FIXED );
	return normalize( vec3( dot( pc.rot0.xyz, f ), dot( pc.rot1.xyz, f ), dot( pc.rot2.xyz, f ) ) );
}
// MIRROR rtddgimath.h tieWindow
float ddgiTieWindow( vec3 o, float t ) { return 1e-4 + 2e-6 * ( abs( o.x ) + abs( o.y ) + abs( o.z ) + t ); }
// MIRROR rtddgimath.h tieWins
bool ddgiTieWins( bool backA, int triA, bool backB, int triB )
{
	if ( backA != backB )
		return !backA;
	return triA < triB;
}
// MIRROR rtddgimath.h gammaEncode / gammaDecode
float ddgiGammaEncode( float x ) { return pow( max( x, 0.0 ), 1.0 / 5.0 ); }
float ddgiGammaDecode( float e ) { return pow( max( e, 0.0 ), 5.0 ); }
// MIRROR rtddgimath.h texelHysteresis
float ddgiTexelHysteresis( float h, vec3 oldEnc, vec3 newEnc )
{
	const vec3 dv = abs( newEnc - oldEnc );
	const float ch = max( dv.x, max( dv.y, dv.z ) );
	if ( ch > 0.25 )
		h = max( 0.0, h - 0.15 );
	if ( ch > 0.8 )
		h = 0.0;
	return h;
}
// MIRROR rtddgimath.h ageHysteresis
float ddgiAgeHysteresis( uint a, float h )
{
	return a == 0u ? 0.0 : min( h, float( a ) / float( a + 1u ) );
}
// MIRROR rtddgimath.h biasVector
vec3 ddgiBiasVector( vec3 n, vec3 o, float D, float B ) { return ( n * 0.2 + o * 0.8 ) * ( 0.75 * D * B ); }
// MIRROR rtddgimath.h backfaceWeight
float ddgiBackfaceWeight( vec3 dirToProbe, vec3 n )
{
	const float c = ( dot( dirToProbe, n ) + 1.0 ) * 0.5;
	return c * c + 0.2;
}
// MIRROR rtddgimath.h chebyshevWeight
float ddgiChebyshev( float r, float mean, float mean2 )
{
	if ( r <= mean )
		return 1.0;
	const float var = abs( mean * mean - mean2 );
	const float d = r - mean;
	float c = var / max( var + d * d, 1e-12 );
	c = c * c * c;
	return max( 0.05, max( c, 0.0 ) );
}
// MIRROR rtddgimath.h crushWeight
float ddgiCrush( float w ) { return w < 0.2 ? w * w * w / ( 0.2 * 0.2 ) : w; }
// MIRROR rtddgimath.h lightRadial
float ddgiRadial( float d, float r, float bias, float scale, float expo )
{
	const float x = clamp( d / max( r, 0.001 ), 0.0, 1.0 );
	const float xe = expo > 0.0 ? pow( x, expo ) : 1.0;
	const float k = 1.0 - clamp( scale * xe + bias, 0.0, 1.0 );
	return pow( k, 2.2 );
}
// MIRROR rtddgimath.h spotCone
float ddgiSpot( float dl, float cosOuter, float cone )
{
	const float base = clamp( 1.0 - ( 1.0 - dl ) / max( 1.0 - cosOuter, 1e-4 ), 0.0, 1.0 );
	return min( pow( base, max( cone, 1e-3 ) ), 1.0 );
}
// MIRROR rtddgimath.h skyRadiance
vec3 ddgiSky( vec3 d )
{
	const vec3 rx = d.x >= 0.0 ? P.sky[0].rgb : P.sky[1].rgb;
	const vec3 ry = d.y >= 0.0 ? P.sky[2].rgb : P.sky[3].rgb;
	const vec3 rz = d.z >= 0.0 ? P.sky[4].rgb : P.sky[5].rgb;
	return d.x * d.x * rx + d.y * d.y * ry + d.z * d.z * rz;
}

// probe p's grid index, position (grid + offset) and state
ivec3 ddgiProbeCoord( uint p ) { return ivec3( int( p ) % P.dims.x, ( int( p ) / P.dims.x ) % P.dims.y, int( p ) / ( P.dims.x * P.dims.y ) ); }
vec3 ddgiProbePos( uint p ) { return P.origin.xyz + vec3( ddgiProbeCoord( p ) ) * P.stepv.xyz + probes[p].xyz; }
int ddgiProbeState( uint p ) { return int( probes[p].w + 0.5 ); }

// the alpha-test hole at barycentrics (b1, b2) of soup triangle tri (src/bakert/common.glsl's hole, this set's bindings)
bool ddgiHole( int tri, float b1, float b2 )
{
	if ( P.flags.z == 0 || tri < 0 || tri >= P.counts.z )
		return false;
	const int m = triOf[tri];
	if ( m < 0 )
		return false;
	const uint b = uint( m ) * 12u;
	const float b0 = 1.0 - b1 - b2;
	float u = b0 * uintBitsToFloat( mt[b + 2u] ) + b1 * uintBitsToFloat( mt[b + 4u] ) + b2 * uintBitsToFloat( mt[b + 6u] );
	float v = b0 * uintBitsToFloat( mt[b + 3u] ) + b1 * uintBitsToFloat( mt[b + 5u] ) + b2 * uintBitsToFloat( mt[b + 7u] );
	const uvec4 mp = maps[mt[b]];
	u -= floor( u );
	v -= floor( v );
	const int x = clamp( int( u * float( mp.y ) ), 0, int( mp.y ) - 1 );
	const int y = clamp( int( v * float( mp.z ) ), 0, int( mp.z ) - 1 );
	const uint off = mp.x + uint( y ) * mp.y + uint( x );
	const uint a = ( ab[off >> 2u] >> ( ( off & 3u ) * 8u ) ) & 255u;
	const uint thr = mt[b + 1u];
	if ( mt[b + 11u] == 0u )
		return a < thr;
	const float s = b0 * uintBitsToFloat( mt[b + 8u] ) + b1 * uintBitsToFloat( mt[b + 9u] ) + b2 * uintBitsToFloat( mt[b + 10u] );
	return float( a ) * s < float( thr );
}
