/* lane PERFFIX3 D (2026-11-01): the precipitation field on the GPU. particleAt of src/features/weather/precipmath.h, evaluated per
 * particle from gl_VertexID (no buffer, no upload). The CPU twin stays the reference: tests/features/weather/precipgpu reads these
 * outputs back by transform feedback and compares them with precipmath.h particleAt.
 *
 * Exactness: the hashes are the same uint32 ops (u01 is exact in float: 24 bits). Positions are camera-relative
 * (the eye sits at the view origin), the wrap offsets per variant come from the CPU in double (precipmath.h gpuRun):
 *   rel = -L/2 + wrap( u L + off + sway, L ),  off = (drift without the flutter - (cam - L/2)) mod L
 * which is the CPU's  lo + wrap( base + drift - lo, L ) - cam  modulo L. */

uniform uint pcSeedS;		// hash32( seed x 0x9E3779B9 + (far ? 0x68E31DA4 : 0) )
uniform float pcL;			// the box side, game units
uniform float pcInner;		// far tier: the near box it skips (0: near tier)
uniform vec2 pcOffXY;		// the wind's wrap offset (x, y)
uniform float pcOffZ[16];	// the fall's wrap offset per variant
uniform int pcNCand;		// variants within 1.6 x the layer's size (0: pcBest alone)
uniform int pcBest;
uniform int pcCand[16];
uniform float pcFallU[16];	// fall speed per variant, units/s
uniform float pcHalfU[16];	// the quad's half size per variant, units
uniform vec2 pcWind;		// the wind x the layer's share, units/s
uniform float pcSway;		// the flutter's amplitude, units
uniform float pcSpin;		// the largest spin, rad/s
uniform float pcT;			// seconds
uniform int pcCols;			// the atlas grid
uniform int pcCells;
uniform vec2 pcCellUv;		// one cell in uv
// lane BUGS1102B (PRECIPROOF-GPU2): a sunlit interior's roof map (src/features/weather/preciproof.h WwRoofMap, R32F: the highest occluder
// z a texel, -1e30 open), so a particle under a roof is dropped here as the CPU field drops it. pcRoofOn false: no map
// (an exterior) or red WW_CELL_PRECIP_RED=gpuroof
uniform bool pcRoofOn;
uniform sampler2D pcRoof;
uniform vec3 pcRoofCam;		// the eye in the map: ( ( eye.xy - map corner ) / texel, eye.z ), from the CPU in double
uniform float pcRoofInv;	// 1 / the texel side, game units

//! roofAt's rule: the texel holding the particle; off the map is open sky
bool pcRoofOpen( vec3 rel )
{
	ivec2 t = ivec2( floor( pcRoofCam.xy + rel.xy * pcRoofInv ) );
	ivec2 n = textureSize( pcRoof, 0 );
#ifdef PC_ROOF_RED
	t = t.yx;	// tests/features/weather/precipgpuroof red: the map read across its axes
#endif
	if ( any( lessThan( t, ivec2( 0 ) ) ) || any( greaterThanEqual( t, n ) ) )
		return true;
	return texelFetch( pcRoof, t, 0 ).r - pcRoofCam.z < rel.z;
}

uint pcHash( uint x )
{
	x ^= x >> 16;
	x *= 0x7feb352du;
	x ^= x >> 15;
	x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}

float pcU01( uint h )
{
	return float( h >> 8 ) * ( 1.0 / 16777216.0 );
}

float pcWrap( float x, float L )
{
	float r = x - L * floor( x / L );
	return r >= L ? r - L : max( r, 0.0 );
}

/* particle i: camera-relative position, velocity (world units/s), half size, angle, atlas cell corner (uv); false
 * when it sits inside the near box (far tier: skipped) or under the roof map (pcRoofOn) */
bool pcParticle( uint i, out vec3 rel, out vec3 vel, out float hs, out float angle, out vec2 cellUv )
{
	uint h = pcHash( i ^ pcSeedS );
	float u0 = pcU01( h );
	h = pcHash( h + 0x1B873593u );
	float u1 = pcU01( h );
	h = pcHash( h + 0x1B873593u );
	float u2 = pcU01( h );
	h = pcHash( h + 0x1B873593u );
	int var = pcNCand > 0 ? pcCand[int( h % uint( pcNCand ) )] : pcBest;
#ifdef PC_FIELD_RED
	var = pcBest;	// tests/features/weather/precipgpu red: the variant pick dropped
#endif
	h = pcHash( h + 0x1B873593u );
	float a0 = pcU01( h ) * 6.283185307179586;
	h = pcHash( h + 0x1B873593u );
	float spin = pcSpin * ( 2.0 * pcU01( h ) - 1.0 );
	h = pcHash( h + 0x1B873593u );
	float ph = pcU01( h ) * 6.283185307179586, om = 0.8 + 0.8 * pcU01( pcHash( h ^ 0x85EBCA6Bu ) );

	float fall = pcFallU[var];
	float sx = pcSway * sin( om * pcT + ph ), sy = pcSway * cos( 0.7 * om * pcT + 1.3 * ph );
	vel = vec3( pcWind.x + pcSway * om * cos( om * pcT + ph ), pcWind.y - pcSway * 0.7 * om * sin( 0.7 * om * pcT + 1.3 * ph ),
		-fall );
	float L = pcL;
	rel = vec3( pcWrap( u0 * L + pcOffXY.x + sx, L ), pcWrap( u1 * L + pcOffXY.y + sy, L ), pcWrap( u2 * L + pcOffZ[var], L ) )
		- 0.5 * L;
	hs = pcHalfU[var];
	angle = a0 + spin * pcT;
	int cell = var % max( pcCells, 1 );
	cellUv = vec2( float( cell % max( pcCols, 1 ) ), float( cell / max( pcCols, 1 ) ) ) * pcCellUv;
	if ( pcInner > 0.0 && all( lessThan( abs( rel ), vec3( 0.5 * pcInner ) ) ) )
		return false;
	return !pcRoofOn || pcRoofOpen( rel );
}
