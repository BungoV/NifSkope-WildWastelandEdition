// HLSL source of features/water/water_surf.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	bool waterSurfOn;	// the Surf row
	int waterSurfRed;	// 1 noshoal, 2 nobreak, 4 regular, 8 alias
	float4 waterSurfK;	// period (s), breaker index gamma, waves a group, foam opacity
#else
/* lane WATERLOOK1 step 11 (WATERSURF1, src/features/water/cellwater.h): shore waves on the sea. Only fo4_waterlook.frag compiles
 * this (WW_WATER_SURF, after water_shore.glsl); the Surf row. Normal and foam only: the plane stays flat.
 * War Thunder (CGDC 2015): shore waves appear in the shallows, parallel to the shore, the distance to the shore as
 * their phase, scaled by the depth, tops moved forward, groups at half the phase speed. Here with linear theory:
 *   phase      phi = omega ( t + tau( d ) ): tau the travel time in from the shore distance d over a plane beach of
 *              the local slope h / d, integrated on the dispersion relation (Fenton & McKee 1990's explicit k), so the
 *              crests run in at c( h ) and close up to the local wavelength 2 pi / k( h )
 *   height     the sea's Hs at the shore (WATERFOAMDETAIL1: wind sea by fetch + swell) shoaled by sqrt( cg0 / cg ),
 *              capped at gamma h once broken (McCowan 1894); the groups (waves a group) and an along-shore noise in
 *              the phase make the breaks irregular
 *   shape      Abreu et al. 2010's asymmetric wave, eta = H / 2 sqrt( 1 - r^2 ) cos( theta ) / ( 1 + r sin( theta ) ),
 *              theta = phi - asin( r ) (the crest at phi = 0, heights still +-H / 2): the shoreward face steepens as
 *              the wave nears breaking (r 0 deep, 0.6 at the break: that face ~4x the other)
 *   foam       from the break in: a sawtooth trail behind each crest (bright at the crest, decaying offshore), through
 *              WATERFOAMDETAIL1's foam texture
 * Seas only (class 0); faded in from half the deep wavelength in depth, out at the waterline, where the shore field
 * has no slope (past its 8160 u cap) and under 2 pixels a wavelength. Red WW_CELL_WATER_SURF_RED: noshoal (one deep
 * wavelength and height everywhere), nobreak (no steepening, no foam), regular (no groups, no phase noise), alias (no
 * footprint fade). */



static const float kSurfG = 686.6;	// 9.81 m/s^2 in game units

// wavenumber at depth h for the angular frequency om (Fenton & McKee 1990, within 1.5%)
float wwSurfK( float om, float h )
{
	float k0 = om * om / kSurfG;
	return k0 / pow( tanh( pow( k0 * max( h, 1e-2 ), 0.75 ) ), 0.6666667 );
}

// travel time from the waterline out to d over a plane beach of slope s: x = u^2 takes the 1 / sqrt( x ) out of 1 / c
float wwSurfTau( float om, float d, float s )
{
	float c0 = kSurfG / om;
	float dDeep = 3.1415927 * c0 / ( om * s );	// where h reaches half the deep wavelength: c = c0 within 0.4%
	float x = min( d, dDeep );
	float r = sqrt( x );
	const float gx[4] = { 0.0694318, 0.3300095, 0.6699905, 0.9305682 };	// Gauss-Legendre on [0, 1]
	const float gw[4] = { 0.1739274, 0.3260726, 0.3260726, 0.1739274 };
	float tau = 0.0;
	for ( int i = 0; i < 4; i++ ) {
		float u = r * gx[i];
		float h = s * u * u;
		tau += gw[i] * 2.0 * u * wwSurfK( om, h ) / om;	// 1 / c = k / om
	}
	return tau * r + max( d - dDeep, 0.0 ) / c0;
}

struct WwSurf
{
	float2 slope;	// -grad eta: adds to a normal's xy / z
	float foam;	// the foam's driving amount 0..1 (before the texture)
	float w;	// the weight the waves were drawn with
	float H;	// the local height (u), for the gates
};

/* the law at shore distance d (u), its unit gradient gd (pointing offshore), depth h (u), offshore height hs (u),
 * clock t (s), world P (the noise), footprint foot (u a pixel) */
WwSurf wwSurfLaw( float d, float2 gd, float h, float hs, float t, float2 P, float foot )
{
	WwSurf o = { (float2)( 0.0 ), 0.0, 0.0, 0.0 };
	float om = 6.2831853 / max( waterSurfK.x, 0.5 );
	float k0 = om * om / kSurfG;
	float L0 = 6.2831853 / k0;
	bool shoal = ( waterSurfRed & 1 ) == 0;
	float hh = max( h, 1.0 );
	float k = shoal ? wwSurfK( om, hh ) : k0;
	float w = 1.0 - smoothstep( 0.35 * L0, 0.5 * L0, h );
	w *= smoothstep( 0.0, 64.0, d );
	float L = 6.2831853 / k;
	if ( ( waterSurfRed & 8 ) == 0 )
		w *= clamp( ( L / max( foot, 1e-3 ) - 2.0 ) * 0.5, 0.0, 1.0 );
	if ( w <= 0.0 )
		return o;
	float tau = shoal ? wwSurfTau( om, d, hh / max( d, 1.0 ) ) : d * k0 / om;
	float phi = om * ( t + tau );
	float env = 1.0;
	if ( ( waterSurfRed & 4 ) == 0 ) {
		float n1 = wwShoreNoise( P / ( 2.0 * L0 ) ), n2 = wwShoreNoise( P / ( 3.0 * L0 ) + 0.41 );
		phi += 3.0 * ( n1 - 0.5 );
		float psi = om / max( waterSurfK.z, 1.0 ) * ( t + 2.0 * tau ) + 6.2831853 * n2;
		env = 0.35 + 0.65 * ( 0.5 + 0.5 * cos( psi ) );
	}
	// shoaling: energy flux kept, cg0 = c0 / 2
	float kh = k * hh;
	float cg = shoal ? 0.5 * ( om / k ) * ( 1.0 + 2.0 * kh / sinh( min( 2.0 * kh, 40.0 ) ) ) : 0.5 * kSurfG / om;
	float Hfree = hs * env * ( shoal ? sqrt( 0.5 * ( kSurfG / om ) / max( cg, 1e-3 ) ) : 1.0 );
	float gam = waterSurfK.y;
	float b = shoal ? Hfree / max( gam * hh, 1e-3 ) : 0.0;	// over 1: broken
	float H = shoal ? min( Hfree, gam * hh ) : Hfree;
	bool brk = ( waterSurfRed & 2 ) == 0;
	float r = brk ? 0.6 * smoothstep( 0.5, 1.0, b ) : 0.0;
	float th = phi - asin( r );
	float den = 1.0 + r * sin( th );
	float dEta = -0.5 * H * sqrt( 1.0 - r * r ) * ( sin( th ) + r ) / ( den * den ) * k;	// d phi / d d = om / c = k
	dEta = clamp( dEta, -0.6, 0.6 );
	o.slope = -dEta * gd * w;
	o.w = w;
	o.H = H;
	if ( brk ) {
		float u = frac( phi / 6.2831853 );	// 0 at the crest, rising offshore
		o.foam = w * smoothstep( 0.8, 1.1, b ) * exp( -6.0 * u );
	}
	return o;
}

/* the surf at world P: the shore field (distance and its gradient), the baked depth and the sea's Hs at the shore;
 * foot = world units a pixel. Nothing off a baked sea */
WwSurf wwWaterSurf( float2 P, float t, float foot )
{
	WwSurf none = { (float2)( 0.0 ), 0.0, 0.0, 0.0 };
	if ( or( !waterSurfOn, !waterInfoOn ) )
		return none;
	if ( wwWaterInfo( P ).w != 0.0 )
		return none;
	float4 wv = wwShoreWave( P );
	if ( wv.w < 0.5 )
		return none;
	float e = 0.5 * waterInfoGrid.z;
	float d = wwShoreField( P ).x;
	float2 g = float2( wwShoreField( P + float2( e, 0.0 ) ).x - wwShoreField( P - float2( e, 0.0 ) ).x,
		wwShoreField( P + float2( 0.0, e ) ).x - wwShoreField( P - float2( 0.0, e ) ).x ) / ( 2.0 * e );
	float gl = length( g );
	WwSurf o = wwSurfLaw( d, g / max( gl, 1e-4 ), wv.z, wv.x, t, P, foot );
	float gw = smoothstep( 0.3, 0.7, gl );	// the field's slope is lost past its cap and on ridges
	o.slope *= gw;
	o.foam *= gw;
	o.w *= gw;
	return o;
}

// the surf foam's alpha through WATERFOAMDETAIL1's texture (as wwShoreFoamWave thresholds its own)
float wwSurfFoamAlpha( float D, float2 P, float foot, float t )
{
	if ( D < 0.002 )
		return 0.0;
	float3 det = wwFoamDetail( P, foot, t );
	float s = lerp( 0.5, waterShoreK0.w, det.z );
	float a = smoothstep( 1.0 - D - s, 1.0 - D + s, det.x );
	return a * lerp( det.y, 1.0, D * D ) * min( 1.0, 1.5 * D ) * waterSurfK.w;
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/water/water_surf.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl
// Not here (declarations, or no converted entry carried them): wwSurfTau, wwSurfLaw, wwWaterSurf
#ifdef WW_FN_ALL
static const float kSurfG = 686.6;
#endif

#if defined(WW_FN_wwSurfK) || defined(WW_FN_ALL)
float wwSurfK(float om, float h)
{
    float k0 = (om * om) / 686.5999755859375f;
    return k0 / pow(tanh(pow(k0 * max(h, 0.00999999977648258209228515625f), 0.75f)), 0.666666686534881591796875f);
}
#endif

#if defined(WW_FN_wwSurfFoamAlpha) || defined(WW_FN_ALL)
float wwSurfFoamAlpha(float D_1, float2 P, float foot, float t)
{
    if (D_1 < 0.00200000009499490261077880859375f)
    {
        return 0.0f;
    }
    float2 detPart3 = P;
    float detPart2 = foot;
    float detPart = t;
    float3 det = wwFoamDetail(detPart3, detPart2, detPart);
    float s = lerp(0.5f, waterShoreK0.w, det.z);
    float a = smoothstep((1.0f - D_1) - s, (1.0f - D_1) + s, det.x);
    return ((a * lerp(det.y, 1.0f, D_1 * D_1)) * min(1.0f, 1.5f * D_1)) * waterSurfK.w;
}
#endif
#endif	// WW_HLSL_SRC
