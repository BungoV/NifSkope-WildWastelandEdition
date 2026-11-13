// HLSL source of features/water/water_glitter.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
/* lane WATERLOOK1 step 9 (WATERGLINT1, src/features/water/cellwater.h): the sun's glare as counted facets (fo4_waterlook.prog only,
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

static const float kWwGlitFacet = 1.4;	// u, a facet's width (2 cm: the capillary scale)
static const float kWwGlitSunR = 0.00466;	// rad, the sun's angular radius
static const float kWwGlitLamMin = 4.0;	// FIX1031B: the fewest lit facets a cell counts (shown mean kept within ~2-3%)

// the sun disc's share of the lobe (GGX alpha^2), none under red nodisc
float wwGlitSunA2()
{
	return ( waterGlintRed & 16 ) != 0 ? 0.0 : kWwGlitSunR * kWwGlitSunR / 8.0;
}

uint4 wwGlitHash( uint4 v )	// pcg4d (Jarzynski and Olano 2020)
{
	v = v * 1664525u + 1013904223u;
	v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
	v ^= v >> 16u;
	v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
	return v;
}

// k / lambda for one cell: k ~ Poisson( lambda ) from two uniforms (inversion below 32, normal above: its zero share is then under 1e-8)
float wwGlitCount( float lambda, float2 u )
{
	if ( lambda >= 32.0 ) {
		float z = sqrt( -2.0 * log( max( u.x, 1e-7 ) ) ) * cos( 6.2831853 * u.y );
		return max( 1.0 + z * rsqrt( lambda ), 0.0 );
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

float wwGlitLevel( float2 xy, float lev, float p, uint2 hb )
{
	float side = kWwGlitFacet * exp2( lev );	// u, a cell's side
	float2 c = floor( xy / side );
	int2 ic = int2( c );
	uint4 h = wwGlitHash( uint4( uint2( ic ), uint( lev ) * 0x9E37u ^ hb.x, hb.y ) );
	float2 u = float2( h.xy >> 8u ) / 16777216.0;
	float n = exp2( 2.0 * lev );	// facets in the cell
	float lam = n * p;
	if ( ( waterGlintRed & 128 ) == 0 )	// FIX1031B: no count the display cannot show (red sparse: the bare count)
		lam = max( lam, kWwGlitLamMin );
	return wwGlitCount( lam, u );
}

/* the factor on the glare (mean 1). xy world (u), foot the pixel's footprint area (u^2), h the half-vector's slope
 * relative to the shading normal, a2 the lobe's GGX alpha^2, cosH the half-vector's cosine to the shading normal */
float wwWaterGlitter( float2 xy, float foot, float2 h, float a2, float cosH )
{
	if ( ( waterGlintRed & 8 ) != 0 )
		return 1.0;
	float t = 1.0 + dot( h, h ) / a2;
	float pdf = 1.0 / ( 3.14159265 * a2 * t * t );	// GGX slope density
	float r = 0.5 * kWwGlitSunR;	// the half-vector's cone
	float cone = 3.14159265 * r * r / max( cosH * cosH * cosH, 1e-3 );
	float p = min( pdf * cone, 1.0 );
	uint2 hb = uint2( int2( floor( h / ( 2.0 * r ) ) ) );
	float lev = max( 0.5 * log2( max( foot, 1e-6 ) / ( kWwGlitFacet * kWwGlitFacet ) ), 0.0 );
	if ( ( waterGlintRed & 32 ) != 0 ) {	// red pixel: the screen's pixels are the cells
		xy = floor( gl_FragCoord.xy ) * kWwGlitFacet;
		lev = 0.0;
	}
	float l0 = floor( lev ), f = lev - l0;
	if ( ( waterGlintRed & 64 ) != 0 )
		f = 0.0;
	float g0 = wwGlitLevel( xy, l0, p, hb );
	return f > 0.0 ? lerp( g0, wwGlitLevel( xy, l0 + 1.0, p, hb ), f ) : g0;
}

/* lane FIX1031B: the glare (x the sun colour) of fo4_water.frag: the mean lobe with the sun's disc, its highlight
 * shoulder (red noshoulder: none), then the facets counted in this pixel. Red sparse: the count before the shoulder
 * (WATERGLINT1's order, which also bent the specks' mean down). reflA2 > 0, xy world (u), foot the footprint (u^2). */
float wwWaterGlareGlit( float3 Ns, float3 V, float3 Ldir, float reflA2, float2 xy, float foot )
{
	float glA2 = reflA2 + wwGlitSunA2();
	float3 glH = normalize( Ldir - V );
	float g = wwWaterGlare( Ns, V, Ldir, (float3)( 1.0 ), glA2, waterGlintK.y, false ).x;
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
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/water/water_glitter.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl
#ifdef WW_FN_ALL
static const float kWwGlitFacet = 1.4;
static const float kWwGlitSunR = 0.00466;
static const float kWwGlitLamMin = 4.0;
#endif

#if defined(WW_FN_wwGlitSunA2) || defined(WW_FN_ALL)
float wwGlitSunA2()
{
    return ((waterGlintRed & 16) != 0) ? 0.0f : 2.7144499199494021013379096984863e-06f;
}
#endif

#if defined(WW_FN_wwGlitHash) || defined(WW_FN_ALL)
uint4 wwGlitHash(inout uint4 v)
{
    v = (v * uint4(1664525u, 1664525u, 1664525u, 1664525u)) + uint4(1013904223u, 1013904223u, 1013904223u, 1013904223u);
    v.x += (v.y * v.w);
    v.y += (v.z * v.x);
    v.z += (v.x * v.y);
    v.w += (v.y * v.z);
    v ^= (v >> uint4(16u, 16u, 16u, 16u));
    v.x += (v.y * v.w);
    v.y += (v.z * v.x);
    v.z += (v.x * v.y);
    v.w += (v.y * v.z);
    return v;
}
#endif

#if defined(WW_FN_wwGlitCount) || defined(WW_FN_ALL)
float wwGlitCount(float lambda, float2 u)
{
    if (lambda >= 32.0f)
    {
        float z = sqrt((-2.0f) * log(max(u.x, 1.0000000116860974230803549289703e-07f))) * cos(6.283185482025146484375f * u.y);
        return max(1.0f + (z * rsqrt(lambda)), 0.0f);
    }
    float pk = exp(-lambda);
    float cdf = pk;
    float k = 0.0f;
    int i = 0;
    for (;;)
    {
        bool cond2 = i < 80;
        bool cond;
        if (cond2)
        {
            cond = u.x > cdf;
        }
        else
        {
            cond = cond2;
        }
        if (cond)
        {
            k += 1.0f;
            pk *= (lambda / k);
            cdf += pk;
            i++;
            continue;
        }
        else
        {
            break;
        }
    }
    return k / max(lambda, 9.9999999600419720025001879548654e-13f);
}
#endif

#if defined(WW_FN_wwGlitLevel) || defined(WW_FN_ALL)
float wwGlitLevel(float2 xy, float lev, float p, uint2 hb)
{
    // u, a cell's side
    float side = 1.39999997615814208984375f * exp2(lev);
    float2 c = floor(xy / side.xx);
    int2 ic = int2(c);
    uint4 cond2 = uint4(uint2(ic), (uint(lev) * 40503u) ^ hb.x, hb.y);
    uint4 cond = wwGlitHash(cond2);
    uint4 h = cond;
    float2 u = float2(h.xy >> uint2(8u, 8u)) / 16777216.0f.xx;
    // facets in the cell
    float n = exp2(2.0f * lev);
    float lam = n * p;
    // FIX1031B: no count the display cannot show (red sparse: the bare count)
    if ((waterGlintRed & 128) == 0)
    {
        lam = max(lam, 4.0f);
    }
    float wwGlitLevelRet2 = lam;
    float2 wwGlitLevelRet = u;
    return wwGlitCount(wwGlitLevelRet2, wwGlitLevelRet);
}
#endif

#if defined(WW_FN_wwWaterGlitter) || defined(WW_FN_ALL)
float wwWaterGlitter(inout float2 xy, float foot, float2 h, float a2, float cosH)
{
    if ((waterGlintRed & 8) != 0)
    {
        return 1.0f;
    }
    float t = 1.0f + (dot(h, h) / a2);
    // GGX slope density
    float pdf = 1.0f / (((3.1415927410125732421875f * a2) * t) * t);
    // the half-vector's cone
    float r = 0.0023300000466406345367431640625f;
    float cone = ((3.1415927410125732421875f * r) * r) / max((cosH * cosH) * cosH, 0.001000000047497451305389404296875f);
    float p = min(pdf * cone, 1.0f);
    uint2 hb = uint2(int2(floor(h / (2.0f * r).xx)));
    float lev = max(0.5f * log2(max(foot, 9.9999999747524270787835121154785e-07f) / 1.96000003814697265625f), 0.0f);
    // red pixel: the screen's pixels are the cells
    if ((waterGlintRed & 32) != 0)
    {
        xy = floor(gl_FragCoord.xy) * 1.39999997615814208984375f;
        lev = 0.0f;
    }
    float l0 = floor(lev);
    float f = lev - l0;
    if ((waterGlintRed & 64) != 0)
    {
        f = 0.0f;
    }
    float2 cond2 = xy;
    float cond = l0;
    float wwGlitLevelRet2 = p;
    uint2 wwGlitLevelRet = hb;
    float g0 = wwGlitLevel(cond2, cond, wwGlitLevelRet2, wwGlitLevelRet);
    float wwWaterGlitterRet;
    if (f > 0.0f)
    {
        float2 wwWaterGlitterPart4 = xy;
        float wwWaterGlitterPart3 = l0 + 1.0f;
        float wwWaterGlitterPart2 = p;
        uint2 wwWaterGlitterPart = hb;
        wwWaterGlitterRet = lerp(g0, wwGlitLevel(wwWaterGlitterPart4, wwWaterGlitterPart3, wwWaterGlitterPart2, wwWaterGlitterPart), f);
    }
    else
    {
        wwWaterGlitterRet = g0;
    }
    return wwWaterGlitterRet;
}
#endif

#if defined(WW_FN_wwWaterGlareGlit) || defined(WW_FN_ALL)
float wwWaterGlareGlit(float3 Ns, float3 V, float3 Ldir, float reflA2, float2 xy, float foot)
{
    float glA2 = reflA2 + wwGlitSunA2();
    float3 glH = normalize(Ldir - V);
    float3 cond2 = Ns;
    float3 cond = V;
    float3 wwGlitLevelRet2 = Ldir;
    float3 wwGlitLevelRet = 1.0f.xxx;
    float wwWaterGlitterRet = glA2;
    float wwWaterGlitterPart4 = waterGlintK.y;
    bool wwWaterGlitterPart3 = false;
    float g = wwWaterGlare(cond2, cond, wwGlitLevelRet2, wwGlitLevelRet, wwWaterGlitterRet, wwWaterGlitterPart4, wwWaterGlitterPart3).x;
    float2 wwWaterGlitterPart2 = xy;
    float wwWaterGlitterPart = foot;
    float2 wwWaterGlitterPart7 = (glH.xy / max(glH.z, 0.001000000047497451305389404296875f).xx) - (Ns.xy / max(Ns.z, 0.001000000047497451305389404296875f).xx);
    float wwWaterGlitterPart6 = glA2;
    float wwWaterGlitterPart5 = dot(glH, Ns);
    float wwWaterGlitterRet2 = wwWaterGlitter(wwWaterGlitterPart2, wwWaterGlitterPart, wwWaterGlitterPart7, wwWaterGlitterPart6, wwWaterGlitterPart5);
    float k = wwWaterGlitterRet2;
    bool shoulder = (waterGlintRed & 4) == 0;
    if ((waterGlintRed & 128) != 0)
    {
        g *= k;
        float wwWaterGlareGlitRet;
        if (shoulder)
        {
            wwWaterGlareGlitRet = g / (1.0f + (g / waterGlintK.x));
        }
        else
        {
            wwWaterGlareGlitRet = g;
        }
        return wwWaterGlareGlitRet;
    }
    if (shoulder)
    {
        g /= (1.0f + (g / waterGlintK.x));
    }
    return g * k;
}
#endif
#endif	// WW_HLSL_SRC
