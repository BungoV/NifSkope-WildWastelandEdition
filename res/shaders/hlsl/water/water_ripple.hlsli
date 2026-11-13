// HLSL source of features/water/water_ripple.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	// The wind's waves reflect off whatever pierces the surface; next to it the water carries ring trains running
	// outward, as strong as the incident slopes there (a wall reflects nearly all of a short wave), fading over a few
	// wavelengths. Still water makes none. The distance to the contact comes from the depth difference to the opaque scene
	// behind the pixel (the soft edge's own term): vertical depth / its horizontal gradient, exact for a plane-faced
	// contact (a post seen at any angle, a sloping rock face), so it needs no object list and covers everything drawn.
	// Two trains, 0.3 m and 0.8 m (engineering choices: the short capillary-gravity band the eye reads as ripples), each at
	// its own phase speed (w^2 = g k + sigma / rho k^3, the phase from the C++ in double). A train finer than the pixel
	// fades out and its slope variance goes to the glare's roughness (LEAN).
	float4 waterOceanRip;	// k of the two trains (rad / u), their phases now (rad, wrapped)
#else
// lane WATERLOOK1 step 3 (src/features/water/cellocean.h): ripples where anything stands in the water (rocks, posts, piers, actors).


static const float kRipDecay = 3.0;	// the rings fade over 3 wavelengths (e-fold)
static const float kRipGain = 0.7;	// each train's slope amplitude / the incident slope rms

// d: the distance to the contact (u); dir: away from it (unit, world xy); foot: the pixel's step (u); s0: the
// incident slope rms. Returns ( slope x, slope y, the slope variance x + y the pixel cannot show ).
float3 wwContactRipple( float d, float2 dir, float foot, float s0 )
{
	float3 o = (float3)( 0.0 );
	for ( int i = 0; i < 2; i++ ) {
		float k = waterOceanRip[i];
		float lam = 6.2831853 / k;
		float a = kRipGain * s0 * exp( -d / ( kRipDecay * lam ) );
		float keep = 1.0 - smoothstep( 0.25, 0.5, foot / lam );	// full under a quarter wavelength a pixel, none from half
		// h = a / k sin( k d - w t ): its slope along dir (the envelope's own slope, 1 / ( 3 k lam ) of it, left out)
		o.xy += keep * a * cos( k * d - waterOceanRip[2 + i] ) * dir;
		o.z += ( 1.0 - keep * keep ) * 0.5 * a * a;
	}
	return o;
}

// The contact from the scene behind: the face the opaque point S lies on (its tangent plane from the pixel quad) meets
// the water's plane along a line; the distance from the water point P to that line, along the water. Exact for any flat
// face (a pier, a wall, a sloping bank seen at any angle), a little short beside a thin round object (its tangent
// plane). vert = the water's height over S (u, 1e9 none). Returns the distance (u, 1e9 none); dir away from the
// contact (unit, world xy), foot the pixel's step on the water (u), trust (1 a face up to 73 degrees off the ray; 0 a
// face seen edge-on, or a jump in the depth: the quad spans an object's flank and what lies behind). Every pixel of a quad must call it.
float wwContactDist( float vert, float3 P, float3 S, float3 eye, out float2 dir, out float foot, out float trust )
{
	float3 sx = ddx( S ), sy = ddy( S );
	float3 Vs = normalize( S - eye );
	float ang = max( length( ddx( Vs ) ), length( ddy( Vs ) ) );
	foot = max( length( ddx( P.xy ) ), length( ddy( P.xy ) ) );
	// a smooth face moves |S - eye| * angle / cos( incidence ) a pixel; a jump far more
	trust = 1.0 - smoothstep( 4.0, 8.0, max( length( sx ), length( sy ) ) / max( length( S - eye ) * ang, 1e-6 ) );
	float3 n = cross( sx, sy );
	float nl = length( n );
	n /= max( nl, 1e-12 );
	float e = dot( P - S, n ), h = length( n.xy );
	// a face seen edge-on holds the ray itself, so the line it gives runs past P: an object's flank, not a contact
	trust *= smoothstep( 0.15, 0.3, abs( dot( n, Vs ) ) );
	dir = sign( e ) * n.xy / max( h, 1e-6 );
	return or( or( vert > 1e8, nl < 1e-12 ), h < 1e-3 ) ? 1e9 : abs( e ) / h;
}

// The rings at this pixel (s0 as above); zero away from any contact or across a silhouette.
float3 wwContactRippleAt( float vert, float3 P, float3 S, float3 eye, float s0 )
{
	float2 dir;
	float foot, trust;
	float d = wwContactDist( vert, P, S, eye, dir, foot, trust );
	float lamMax = 6.2831853 / min( waterOceanRip.x, waterOceanRip.y );
	if ( or( or( s0 <= 0.0, trust <= 0.0 ), d > 4.0 * kRipDecay * lamMax ) )
		return (float3)( 0.0 );
	float3 o = wwContactRipple( d, dir, foot, s0 );
	return float3( trust * o.xy, o.z );
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/water/water_ripple.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookp_ps.hlsl
#ifdef WW_FN_ALL
static const float kRipDecay = 3.0;
static const float kRipGain = 0.7;
#endif

#if defined(WW_FN_wwContactRipple) || defined(WW_FN_ALL)
float3 wwContactRipple(float d, float2 dir, float foot, float s0)
{
    float3 o = 0.0f.xxx;
    for (int i = 0; i < 2; i++)
    {
        float k = waterOceanRip[i];
        float lam = 6.283185482025146484375f / k;
        float a = (0.699999988079071044921875f * s0) * exp((-d) / (3.0f * lam));
        // full under a quarter wavelength a pixel, none from half
        float keep = 1.0f - smoothstep(0.25f, 0.5f, foot / lam);
        float3 oPart2 = o;
        float2 oPart = oPart2.xy + (dir * ((keep * a) * cos((k * d) - waterOceanRip[2 + i])));
        o.x = oPart.x;
        o.y = oPart.y;
        o.z += ((((1.0f - (keep * keep)) * 0.5f) * a) * a);
    }
    return o;
}
#endif

#if defined(WW_FN_wwContactDist) || defined(WW_FN_ALL)
float wwContactDist(float vert, float3 P, float3 S, float3 eye, out float2 dir, out float foot, inout float trust)
{
    float3 sx = ddx(S);
    float3 sy = ddy(S);
    float3 Vs = normalize(S - eye);
    float ang = max(length(ddx(Vs)), length(ddy(Vs)));
    foot = max(length(ddx(P.xy)), length(ddy(P.xy)));
    // a smooth face moves |S - eye| * angle / cos( incidence ) a pixel; a jump far more
    trust = 1.0f - smoothstep(4.0f, 8.0f, max(length(sx), length(sy)) / max(length(S - eye) * ang, 9.9999999747524270787835121154785e-07f));
    float3 n = cross(sx, sy);
    float nl = length(n);
    n /= max(nl, 9.9999999600419720025001879548654e-13f).xxx;
    float e = dot(P - S, n);
    float h = length(n.xy);
    // a face seen edge-on holds the ray itself, so the line it gives runs past P: an object's flank, not a contact
    trust *= smoothstep(0.1500000059604644775390625f, 0.300000011920928955078125f, abs(dot(n, Vs)));
    dir = (n.xy * sign(e)) / max(h, 9.9999999747524270787835121154785e-07f).xx;
    float oPart2;
    bool wwh0 = ((vert > 100000000.0f) || (nl < 9.9999999600419720025001879548654e-13f));
    bool wwh1 = (h < 0.001000000047497451305389404296875f);
    if (or(wwh0, wwh1))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
    {
        oPart2 = 1000000000.0f;
    }
    else
    {
        oPart2 = abs(e) / h;
    }
    return oPart2;
}
#endif

#if defined(WW_FN_wwContactRippleAt) || defined(WW_FN_ALL)
float3 wwContactRippleAt(float vert, float3 P, float3 S, float3 eye, float s0)
{
    float oPart2 = vert;
    float3 oPart = P;
    float3 wwContactDistPart2 = S;
    float3 wwContactDistPart = eye;
    float2 wwContactDistPick3;
    float wwContactDistPick2;
    float wwContactDistPick;
    float wwContactDistRet = wwContactDist(oPart2, oPart, wwContactDistPart2, wwContactDistPart, wwContactDistPick3, wwContactDistPick2, wwContactDistPick);
    float2 dir = wwContactDistPick3;
    float foot = wwContactDistPick2;
    float trust = wwContactDistPick;
    float d = wwContactDistRet;
    float lamMax = 6.283185482025146484375f / min(waterOceanRip.x, waterOceanRip.y);
    bool cond2 = (s0 <= 0.0f) || (trust <= 0.0f);
    bool cond;
    if (!cond2)
    {
        cond = d > (12.0f * lamMax);
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        return 0.0f.xxx;
    }
    float oPart6 = d;
    float2 oPart5 = dir;
    float oPart4 = foot;
    float oPart3 = s0;
    float3 o = wwContactRipple(oPart6, oPart5, oPart4, oPart3);
    return float3(o.xy * trust, o.z);
}
#endif
#endif	// WW_HLSL_SRC
