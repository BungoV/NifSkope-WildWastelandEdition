// Lane RTBAKE1: what both GPU tracers share -- the ray and hit records and the alpha-test hole (src/probemask.h's
// AlphaMask::hole, in float). Compiled into rq.comp and bvh.comp by tools/bakert_spirv.sh.

struct Ray { vec4 oT; vec4 dT; };   // origin + tmin, direction + tmax (local frame of the tree)

layout( std430, set = 0, binding = 0 ) readonly buffer Rays { Ray rays[]; };
layout( std430, set = 0, binding = 1 ) writeonly buffer Hits { uvec2 hits[]; };   // t bits, tri (0xFFFFFFFF = miss)
layout( std430, set = 0, binding = 2 ) readonly buffer TriOf { int triOf[]; };    // soup triangle -> masked record
layout( std430, set = 0, binding = 3 ) readonly buffer MaskTris { uint mt[]; };   // 12 words a masked triangle
layout( std430, set = 0, binding = 4 ) readonly buffer Maps { uvec4 maps[]; };    // byte offset, w, h, 0
layout( std430, set = 0, binding = 5 ) readonly buffer Alpha { uint ab[]; };      // the maps' alpha bytes, 4 a word

// flags bit 0: any hit (an occlusion ray stops at its first accepted hit, which need not be the nearest)
layout( push_constant ) uniform Pc { uint first; uint count; uint useMask; uint nTri; uint nOpaque; uint flags; } pc;

float mf( uint i ) { return uintBitsToFloat( mt[i] ); }

// the hit at barycentrics (b1, b2) of soup triangle `tri` lands on a hole (AlphaMask::hole)
bool hole( int tri, float b1, float b2 )
{
	if ( pc.useMask == 0u || tri < 0 || uint( tri ) >= pc.nTri )
		return false;
	const int m = triOf[tri];
	if ( m < 0 )
		return false;
	const uint b = uint( m ) * 12u;
	const float b0 = 1.0 - b1 - b2;
	float u = b0 * mf( b + 2u ) + b1 * mf( b + 4u ) + b2 * mf( b + 6u );
	float v = b0 * mf( b + 3u ) + b1 * mf( b + 5u ) + b2 * mf( b + 7u );
	const uvec4 mp = maps[mt[b]];
	u -= floor( u );
	v -= floor( v );
	const int x = clamp( int( u * float( mp.y ) ), 0, int( mp.y ) - 1 );
	const int y = clamp( int( v * float( mp.z ) ), 0, int( mp.z ) - 1 );
	const uint off = mp.x + uint( y ) * mp.y + uint( x );
	const uint a = ( ab[off >> 2u] >> ( ( off & 3u ) * 8u ) ) & 255u;
	const uint thr = mt[b + 1u];
	if ( mt[b + 11u] == 0u )   // no corner scale: the integer test
		return a < thr;
	const float s = b0 * mf( b + 8u ) + b1 * mf( b + 9u ) + b2 * mf( b + 10u );
	return float( a ) * s < float( thr );
}
