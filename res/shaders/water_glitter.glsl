/* lane WATERLOOK1 step 9 (WATERGLINT1, src/gl/cellwater.h): the sun's glare as counted facets (fo4_waterlook.prog only,
 * after water_refl.glsl; the Water sun glare row).
 * The glare (water_refl.glsl wwWaterGlare, GGX over the pixel's slope variance) is the MEAN over the facets a pixel
 * covers. Water is made of capillary facets about 2 cm across, each a flat mirror; a pixel covering n of them sees
 * the sun in k ~ Poisson( n p ) of them, p the share of facets whose normal points the sun into the eye: the GGX slope
 * density at the half-vector x the sun's cone in slope space (Zirr and Kaplanyan 2016, "Real-time rendering of
 * procedural multiscale materials", the counting form). The glare is scaled by k / (n p): its mean is unchanged, and
 * so is the far glare (n p large: the count's spread 1 / sqrt( n p ) goes to 0), while near water, where a pixel holds
 * few facets, glitters in specks only inside the glare's lobe.
 *   cells      world-anchored squares of 2^l facets a side matching the pixel's footprint, two levels blended by the
 *              footprint's fraction (no pop as the distance changes); a cell's draw also depends on the half-vector's
 *              bin (the sun cone's width), so the specks follow the sun, the view and the waves
 *   sun disc   the sun's radius (0.267 deg) widens the lobe: alpha^2 += r^2 / 8 (the half-vector's disc, per axis)
 * Reds (WW_CELL_WATER_GLINT_RED): noglitter (the mean alone), nodisc (a point sun), pixel (the cells in screen pixels,
 * not the world: specks swim with the camera), nolodblend (one level: specks pop where the footprint crosses 2^l).
 * lane FIX1031B (WATERGLINT2): near water a cell holds about 1e-4 of a lit facet (the sun's cone is 0.27 deg), so the
 * count was 0 in nearly every pixel and the rare lit one carries 1e4 x the mean: the tone map clips it, and the sun path
 * lost 30% (river) to 53% (sea) of its shown brightness (bungo's 10-31 frames). A cell now counts at least
 * kWwGlitLamMin lit facets (the spread a display can show; the mean is unchanged), and the shoulder takes the mean
 * glare BEFORE the count (the count's mean then stays 1 after it). Red sparse (128): the count and order before. */

const float kWwGlitFacet = 1.4;	// u, a facet's width (2 cm: the capillary scale)
const float kWwGlitSunR = 0.00466;	// rad, the sun's angular radius
const float kWwGlitLamMin = 4.0;	// FIX1031B: the fewest lit facets a cell counts (shown mean kept within ~2-3%)

// the sun disc's share of the lobe (GGX alpha^2), none under red nodisc
float wwGlitSunA2()
{
	return ( waterGlintRed & 16 ) != 0 ? 0.0 : kWwGlitSunR * kWwGlitSunR / 8.0;
}

uvec4 wwGlitHash( uvec4 v )	// pcg4d (Jarzynski and Olano 2020)
{
	v = v * 1664525u + 1013904223u;
	v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
	v ^= v >> 16u;
	v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
	return v;
}

// k / lambda for one cell: k ~ Poisson( lambda ) from two uniforms (inversion below 32, normal above: its zero share is then under 1e-8)
float wwGlitCount( float lambda, vec2 u )
{
	if ( lambda >= 32.0 ) {
		float z = sqrt( -2.0 * log( max( u.x, 1e-7 ) ) ) * cos( 6.2831853 * u.y );
		return max( 1.0 + z * inversesqrt( lambda ), 0.0 );
	}
	float pk = exp( -lambda ), cdf = pk;
	float k = 0.0;
	for ( int i = 0; i < 80 && u.x > cdf; i++ ) {
		k += 1.0;
		pk *= lambda / k;
		cdf += pk;
	}
	return k / max( lambda, 1e-12 );
}

float wwGlitLevel( vec2 xy, float lev, float p, uvec2 hb )
{
	float side = kWwGlitFacet * exp2( lev );	// u, a cell's side
	vec2 c = floor( xy / side );
	ivec2 ic = ivec2( c );
	uvec4 h = wwGlitHash( uvec4( uvec2( ic ), uint( lev ) * 0x9E37u ^ hb.x, hb.y ) );
	vec2 u = vec2( h.xy >> 8u ) / 16777216.0;
	float n = exp2( 2.0 * lev );	// facets in the cell
	float lam = n * p;
	if ( ( waterGlintRed & 128 ) == 0 )	// FIX1031B: no count the display cannot show (red sparse: the bare count)
		lam = max( lam, kWwGlitLamMin );
	return wwGlitCount( lam, u );
}

/* the factor on the glare (mean 1). xy world (u), foot the pixel's footprint area (u^2), h the half-vector's slope
 * relative to the shading normal, a2 the lobe's GGX alpha^2, cosH the half-vector's cosine to the shading normal */
float wwWaterGlitter( vec2 xy, float foot, vec2 h, float a2, float cosH )
{
	if ( ( waterGlintRed & 8 ) != 0 )
		return 1.0;
	float t = 1.0 + dot( h, h ) / a2;
	float pdf = 1.0 / ( 3.14159265 * a2 * t * t );	// GGX slope density
	float r = 0.5 * kWwGlitSunR;	// the half-vector's cone
	float cone = 3.14159265 * r * r / max( cosH * cosH * cosH, 1e-3 );
	float p = min( pdf * cone, 1.0 );
	uvec2 hb = uvec2( ivec2( floor( h / ( 2.0 * r ) ) ) );
	float lev = max( 0.5 * log2( max( foot, 1e-6 ) / ( kWwGlitFacet * kWwGlitFacet ) ), 0.0 );
	if ( ( waterGlintRed & 32 ) != 0 ) {	// red pixel: the screen's pixels are the cells
		xy = floor( gl_FragCoord.xy ) * kWwGlitFacet;
		lev = 0.0;
	}
	float l0 = floor( lev ), f = lev - l0;
	if ( ( waterGlintRed & 64 ) != 0 )
		f = 0.0;
	float g0 = wwGlitLevel( xy, l0, p, hb );
	return f > 0.0 ? mix( g0, wwGlitLevel( xy, l0 + 1.0, p, hb ), f ) : g0;
}

/* lane FIX1031B: the glare (x the sun colour) of fo4_water.frag: the mean lobe with the sun's disc, its highlight
 * shoulder (red noshoulder: none), then the facets counted in this pixel. Red sparse: the count before the shoulder
 * (WATERGLINT1's order, which also bent the specks' mean down). reflA2 > 0, xy world (u), foot the footprint (u^2). */
float wwWaterGlareGlit( vec3 Ns, vec3 V, vec3 Ldir, float reflA2, vec2 xy, float foot )
{
	float glA2 = reflA2 + wwGlitSunA2();
	vec3 glH = normalize( Ldir - V );
	float g = wwWaterGlare( Ns, V, Ldir, vec3( 1.0 ), glA2, waterGlintK.y, false ).x;
	float k = wwWaterGlitter( xy, foot, glH.xy / max( glH.z, 1e-3 ) - Ns.xy / max( Ns.z, 1e-3 ), glA2, dot( glH, Ns ) );
	bool shoulder = ( waterGlintRed & 4 ) == 0;
	if ( ( waterGlintRed & 128 ) != 0 ) {
		g *= k;
		return shoulder ? g / ( 1.0 + g / waterGlintK.x ) : g;
	}
	if ( shoulder )
		g = g / ( 1.0 + g / waterGlintK.x );
	return g * k;
}
