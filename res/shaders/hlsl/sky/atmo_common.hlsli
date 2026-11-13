// HLSL source of features/sky/atmo_common.glsl (include-only GLSL), lane HLSLSRC3 (notes/hlsltools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
	float3 atRay;	// Rayleigh scattering /km (5.802, 13.558, 33.1)e-3
	float atMieS;	// Mie scattering /km (3.996e-3)
	float atMieE;	// Mie extinction /km (4.40e-3)
	float3 atHaze;	// the weather fog's haze layer: ground extinction /km, scale height km, plateau top km
	float3 atOzone;	// ozone absorption /km (0.650, 1.881, 0.085)e-3; 0 under the red "noozone"
	float atG;	// Mie Henyey-Greenstein g (lane WEATHERRECX1: the haze's, atmomath.h hazeOptics)
	float3 atHazeAlbedo;	// lane WEATHERRECX1: the haze's albedo at 680 / 550 / 440 nm (3.996 / 4.40 grey before 1.3)
	float3 atHazeSpec;	// lane WEATHERRECX1: the haze extinction's (lambda / 550)^-angstrom (1 = grey, before 1.3)
	/* the multiple-scattering LUT: x = cos sun zenith, y = height. lane FIX1101A (atmo): y = sqrt(height / 100 km), rows
	 * at 0.01, 0.10, 0.42, 0.94 .. km instead of 0.01, 3.2, 6.5 .. km, so the weather's haze layer (0.1-1 km) has its own
	 * rows; with the old linear rows the first row above the ground sat at 3.2 km and the 3-degree horizon read 20-37 %
	 * under a Monte Carlo of the same medium (notes/fix1101a/atmo/ATMO_TABLE.md). atMsOld 1 = the red WW_ATMO_RED=msold
	 * (linear rows, 20 uniform steps in atmo_ms.frag); every pass that reads or writes the LUT takes it from medium(). */
	float atMsOld;
#else
// lane ATMO1: the physical sky's medium and LUT mappings (Hillaire 2020; Bruneton 2017 for the transmittance
// mapping). km units, sun irradiance 1 at the top of the atmosphere. tests/features/sky/atmo_model.py is the same math.

static const float kPi = 3.14159265;
static const float kRb = 6360.0;
static const float kRt = 6460.0;

static const float kRayH = 8.0;
static const float kMieH = 1.2;
static const float3 kAlbedo = (float3)( 0.3 );

void atMedium( float h, out float3 rayS, out float3 mieS, out float3 ext )
{
	h = max( h, 0.0 );
	float dr = exp( -h / kRayH );
	float dm = exp( -h / kMieH );
	float dz = max( 0.0, 1.0 - abs( h - 25.0 ) / 15.0 );
	float dh = atHaze.x * exp( -max( h - atHaze.z, 0.0 ) / max( atHaze.y, 0.02 ) );
	rayS = atRay * dr;
	mieS = (float3)( atMieS * dm ) + dh * atHazeSpec * atHazeAlbedo;
	ext = rayS + (float3)( atMieE * dm ) + dh * atHazeSpec + atOzone * dz;
}

float atDistTop( float r, float mu )
{
	return max( 0.0, -r * mu + sqrt( max( 0.0, r * r * ( mu * mu - 1.0 ) + kRt * kRt ) ) );
}

float atDistGround( float r, float mu )
{
	return max( 0.0, -r * mu - sqrt( max( 0.0, r * r * ( mu * mu - 1.0 ) + kRb * kRb ) ) );
}

#ifndef WW_AT_HITSGROUND_OWN	// an entry may spell it itself (the HLSL sky twin: NVIDIA's operation order)
bool atHitsGround( float r, float mu )
{
	return mu < 0.0 && r * r * ( mu * mu - 1.0 ) + kRb * kRb >= 0.0;
}
#endif

// texel centers sit on the [0,1] ends of the parameter range
float atUnitToSub( float u, float n ) { return 0.5 / n + u * ( 1.0 - 1.0 / n ); }
float atSubToUnit( float u, float n ) { return ( u - 0.5 / n ) / ( 1.0 - 1.0 / n ); }

static const float2 kTransSize = float2( 256.0, 64.0 );

float2 atTransUv( float r, float mu )
{
	float H = sqrt( kRt * kRt - kRb * kRb );
	float rho = sqrt( max( 0.0, r * r - kRb * kRb ) );
	float d = atDistTop( r, mu );
	float dmin = kRt - r, dmax = rho + H;
	return float2( atUnitToSub( ( d - dmin ) / max( dmax - dmin, 1e-6 ), kTransSize.x ),
		atUnitToSub( rho / H, kTransSize.y ) );
}

void atTransRmu( float2 uv, out float r, out float mu )
{
	float xm = atSubToUnit( uv.x, kTransSize.x ), xr = atSubToUnit( uv.y, kTransSize.y );
	float H = sqrt( kRt * kRt - kRb * kRb );
	float rho = H * xr;
	r = sqrt( rho * rho + kRb * kRb );
	float dmin = kRt - r, dmax = rho + H;
	float d = dmin + xm * ( dmax - dmin );
	mu = d == 0.0 ? 1.0 : clamp( ( H * H - rho * rho - d * d ) / ( 2.0 * r * d ), -1.0, 1.0 );
}

float3 atTrans( Texture2D<float4> T, SamplerState _T_sampler, float r, float mu )
{
	if ( atHitsGround( r, mu ) )
		return (float3)( 0.0 );
	return T.Sample( _T_sampler, atTransUv( r, mu ) ).rgb;
}
float atMsRowToH( float u )
{
	return atMsOld > 0.5 ? u : u * u;
}
float2 atMsUv( float r, float mus, float2 n )
{
	float hu = clamp( ( r - kRb ) / ( kRt - kRb ), 0.0, 1.0 );
	return float2( atUnitToSub( clamp( mus * 0.5 + 0.5, 0.0, 1.0 ), n.x ),
		atUnitToSub( atMsOld > 0.5 ? hu : sqrt( hu ), n.y ) );
}

float atPhaseR( float c ) { return 3.0 / ( 16.0 * kPi ) * ( 1.0 + c * c ); }
float atPhaseHg( float g, float c ) { return ( 1.0 - g * g ) / ( 4.0 * kPi * pow( max( 1e-4, 1.0 + g * g - 2.0 * g * c ), 1.5 ) ); }

// the sky-view LUT (Hillaire 2020 sec. 5.3): latitude squeezed toward the horizon, longitude = angle to the sun
void atSkyUvToDir( float2 uv, float2 n, float r, out float cosZ, out float cosAz )
{
	uv = float2( atSubToUnit( uv.x, n.x ), atSubToUnit( uv.y, n.y ) );
	float vh = sqrt( max( 0.0, r * r - kRb * kRb ) );
	float beta = acos( clamp( vh / r, -1.0, 1.0 ) );
	float zh = kPi - beta;
	float za;
	if ( uv.y < 0.5 ) {
		float c = 1.0 - 2.0 * uv.y;
		za = zh * ( 1.0 - c * c );
	} else {
		float c = uv.y * 2.0 - 1.0;
		za = zh + beta * c * c;
	}
	cosZ = cos( za );
	float c = uv.x * uv.x;
	cosAz = -( c * 2.0 - 1.0 );
}

float2 atSkyDirToUv( float r, float cosZ, float cosAz, float2 n )
{
	float vh = sqrt( max( 0.0, r * r - kRb * kRb ) );
	float beta = acos( clamp( vh / r, -1.0, 1.0 ) );
	float zh = kPi - beta;
	float za = acos( clamp( cosZ, -1.0, 1.0 ) );
	float2 uv;
	if ( za < zh ) {
		float c = 1.0 - za / zh;
		uv.y = ( 1.0 - sqrt( max( 0.0, c ) ) ) * 0.5;
	} else {
		float c = ( za - zh ) / max( beta, 1e-5 );
		uv.y = sqrt( clamp( c, 0.0, 1.0 ) ) * 0.5 + 0.5;
	}
	uv.x = sqrt( clamp( -cosAz * 0.5 + 0.5, 0.0, 1.0 ) );
	return float2( atUnitToSub( uv.x, n.x ), atUnitToSub( uv.y, n.y ) );
}

// lane SKY2B: one view ray's march (Hillaire 2020 sec. 5.3: single scattering + the MS term, 30 quadratic steps),
// shared by the sky-view LUT and the per-pixel sky (red WW_ATMO_RED=perpixel). ground = 1 adds the lit ground
// (albedo kAlbedo, as the MS pass does) where the ray ends on the planet: without it high cameras saw a black earth.
float3 atSkyMarch( Texture2D<float4> T, SamplerState _T_sampler, Texture2D<float4> M, SamplerState _M_sampler, float2 msSize, float msOn, float ground, float r, float3 v, float3 s )
{
	bool gnd = atHitsGround( r, v.z );
	float d = gnd ? atDistGround( r, v.z ) : atDistTop( r, v.z );
	float cvs = dot( v, s );
	float pr = atPhaseR( cvs ), pm = atPhaseHg( atG, cvs );
	const int N = 30;
	float3 thr = (float3)( 1.0 ), L = (float3)( 0.0 );
	float t0 = 0.0;
	for ( int i = 0; i < N; i++ ) {
		float a = float( i + 1 ) / float( N );
		float t1 = d * a * a;
		float dt = t1 - t0;
		float t = t0 + dt * 0.5;
		t0 = t1;
		float3 p = float3( v.xy * t, r + v.z * t );
		float rp = length( p );
		float3 up = p / rp;
		float mus = dot( up, s );
		float3 rs, ms, e;
		atMedium( rp - kRb, rs, ms, e );
		float3 Ts = atTrans( T, _T_sampler, rp, mus );
		float3 S = Ts * ( rs * pr + ms * pm ) + msOn * M.Sample( _M_sampler, atMsUv( rp, mus, msSize ) ).rgb * ( rs + ms );
		float3 Tst = exp( -e * dt );
		L += thr * S * ( 1.0 - Tst ) / max( e, (float3)( 1e-9 ) );
		thr *= Tst;
	}
	if ( gnd && ground > 0.5 ) {
		float c = dot( normalize( float3( v.xy * d, r + v.z * d ) ), s );
		L += thr * atTrans( T, _T_sampler, kRb + 1e-4, c ) * max( c, 0.0 ) * kAlbedo / kPi;
	}
	return L;
}
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/sky/atmo_common.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: atmo_ap_ps.hlsl, atmo_apl_ps.hlsl, atmo_ms_ps.hlsl, atmo_sky_ps.hlsl, atmo_skyview_ps.hlsl, atmo_transmittance_ps.hlsl, cell_volsky_ps.hlsl, cloud_fly_a_ps.hlsl, cloud_fly_ps.hlsl, cloud_fly_v_ps.hlsl, cloud_march_a_ps.hlsl, cloud_march_ps.hlsl, cloud_march_v_ps.hlsl, lookdev_clouds_ps.hlsl, sky_moon_ps.hlsl
// Not here (declarations, or no converted entry carried them): atTransRmu, atMsRowToH, atSkyUvToDir
#ifdef WW_FN_ALL
static const float kPi = 3.14159265;
static const float kRb = 6360.0;
static const float kRt = 6460.0;
static const float kRayH = 8.0;
static const float kMieH = 1.2;
#endif

#if defined(WW_FN_atMedium) || defined(WW_FN_ALL)
void atMedium(inout float h, inout float3 rayS, out float3 mieS, out float3 ext)
{
    h = max(h, 0.0f);
    float dr = exp((-h) / 8.0f);
    float dm = exp((-h) / 1.2000000476837158203125f);
    float dz = max(0.0f, 1.0f - (abs(h - 25.0f) / 15.0f));
    float dh = atHaze.x * exp((-max(h - atHaze.z, 0.0f)) / max(atHaze.y, 0.0199999995529651641845703125f));
    rayS = atRay * dr;
    mieS = (atMieS * dm).xxx + ((atHazeSpec * dh) * atHazeAlbedo);
    ext = ((rayS + (atMieE * dm).xxx) + (atHazeSpec * dh)) + (atOzone * dz);
}
#endif

#if defined(WW_FN_atDistTop) || defined(WW_FN_ALL)
float atDistTop(float r, float mu)
{
    return max(0.0f, ((-r) * mu) + sqrt(max(0.0f, ((r * r) * ((mu * mu) - 1.0f)) + 41731600.0f)));
}
#endif

#if defined(WW_FN_atDistGround) || defined(WW_FN_ALL)
float atDistGround(float r, float mu)
{
    return max(0.0f, ((-r) * mu) - sqrt(max(0.0f, ((r * r) * ((mu * mu) - 1.0f)) + 40449600.0f)));
}
#endif

#if defined(WW_FN_atHitsGround) || defined(WW_FN_ALL)
bool atHitsGround(float r, float mu)
{
    bool cond = mu < 0.0f;
    bool atHitsGroundRet;
    if (cond)
    {
        atHitsGroundRet = (((r * r) * ((mu * mu) - 1.0f)) + 40449600.0f) >= 0.0f;
    }
    else
    {
        atHitsGroundRet = cond;
    }
    return atHitsGroundRet;
}
#endif

#if defined(WW_FN_atUnitToSub) || defined(WW_FN_ALL)
float atUnitToSub(float u, float n)
{
    return (0.5f / n) + (u * (1.0f - (1.0f / n)));
}
#endif

#if defined(WW_FN_atSubToUnit) || defined(WW_FN_ALL)
float atSubToUnit(float u, float n)
{
    return (u - (0.5f / n)) / (1.0f - (1.0f / n));
}
#endif

#if defined(WW_FN_atTransUv) || defined(WW_FN_ALL)
float2 atTransUv(float r, float mu)
{
    float H = 1132.25439453125f;
    float rho = sqrt(max(0.0f, (r * r) - 40449600.0f));
    float cond = r;
    float atHitsGroundRet = mu;
    float d = atDistTop(cond, atHitsGroundRet);
    float dmin = 6460.0f - r;
    float dmax = rho + H;
    float atTransUvRet4 = (d - dmin) / max(dmax - dmin, 9.9999999747524270787835121154785e-07f);
    float atTransUvRet3 = 256.0f;
    float atTransUvRet2 = rho / H;
    float atTransUvRet = 64.0f;
    return float2(atUnitToSub(atTransUvRet4, atTransUvRet3), atUnitToSub(atTransUvRet2, atTransUvRet));
}
#endif

#if defined(WW_FN_atTrans) || defined(WW_FN_ALL)
float3 atTrans(Texture2D<float4> T, SamplerState _T_sampler, float r, float mu)
{
    float cond = r;
    float atHitsGroundRet = mu;
    if (atHitsGround(cond, atHitsGroundRet))
    {
        return 0.0f.xxx;
    }
    float atTransUvRet4 = r;
    float atTransUvRet3 = mu;
    return T.Sample(_T_sampler, atTransUv(atTransUvRet4, atTransUvRet3)).xyz;
}
#endif

#if defined(WW_FN_atMsUv) || defined(WW_FN_ALL)
float2 atMsUv(float r, float mus, float2 n)
{
    float hu = clamp((r - 6360.0f) / 100.0f, 0.0f, 1.0f);
    float cond = clamp((mus * 0.5f) + 0.5f, 0.0f, 1.0f);
    float atHitsGroundRet = n.x;
    float atTransUvRet4;
    if (atMsOld > 0.5f)
    {
        atTransUvRet4 = hu;
    }
    else
    {
        atTransUvRet4 = sqrt(hu);
    }
    float atTransUvRet3 = atTransUvRet4;
    float atTransUvRet2 = n.y;
    return float2(atUnitToSub(cond, atHitsGroundRet), atUnitToSub(atTransUvRet3, atTransUvRet2));
}
#endif

#if defined(WW_FN_atPhaseR) || defined(WW_FN_ALL)
float atPhaseR(float c)
{
    return 0.0596831031143665313720703125f * (1.0f + (c * c));
}
#endif

#if defined(WW_FN_atPhaseHg) || defined(WW_FN_ALL)
float atPhaseHg(float g, float c)
{
    return (1.0f - (g * g)) / (12.56637096405029296875f * pow(max(9.9999997473787516355514526367188e-05f, (1.0f + (g * g)) - ((2.0f * g) * c)), 1.5f));
}
#endif

#if defined(WW_FN_atSkyDirToUv) || defined(WW_FN_ALL)
float2 atSkyDirToUv(float r, float cosZ, float cosAz, float2 n)
{
    float vh = sqrt(max(0.0f, (r * r) - 40449600.0f));
    float beta = acos(clamp(vh / r, -1.0f, 1.0f));
    float zh = 3.1415927410125732421875f - beta;
    float za = acos(clamp(cosZ, -1.0f, 1.0f));
    float2 uv;
    if (za < zh)
    {
        float c = 1.0f - (za / zh);
        uv.y = (1.0f - sqrt(max(0.0f, c))) * 0.5f;
    }
    else
    {
        float c_1 = (za - zh) / max(beta, 9.9999997473787516355514526367188e-06f);
        uv.y = (sqrt(clamp(c_1, 0.0f, 1.0f)) * 0.5f) + 0.5f;
    }
    uv.x = sqrt(clamp(((-cosAz) * 0.5f) + 0.5f, 0.0f, 1.0f));
    float cond = uv.x;
    float atHitsGroundRet = n.x;
    float atTransUvRet4 = uv.y;
    float atTransUvRet3 = n.y;
    return float2(atUnitToSub(cond, atHitsGroundRet), atUnitToSub(atTransUvRet4, atTransUvRet3));
}
#endif

#if defined(WW_FN_atSkyMarch) || defined(WW_FN_ALL)
float3 atSkyMarch(Texture2D<float4> T, SamplerState _T_sampler, Texture2D<float4> M, SamplerState _M_sampler, float2 msSize, float msOn, float ground, float r, float3 v, float3 s)
{
    float cond = r;
    float atHitsGroundRet = v.z;
    bool gnd = atHitsGround(cond, atHitsGroundRet);
    float atTransUvRet4;
    if (gnd)
    {
        float atTransUvRet3 = r;
        float atTransUvRet2 = v.z;
        atTransUvRet4 = atDistGround(atTransUvRet3, atTransUvRet2);
    }
    else
    {
        float atTransUvRet = r;
        float atTransUvPart = v.z;
        atTransUvRet4 = atDistTop(atTransUvRet, atTransUvPart);
    }
    float d = atTransUvRet4;
    float cvs = dot(v, s);
    float prPart = cvs;
    float pr = atPhaseR(prPart);
    float pmPart2 = atG;
    float pmPart = cvs;
    float pm = atPhaseHg(pmPart2, pmPart);
    float3 thr = 1.0f.xxx;
    float3 L = 0.0f.xxx;
    float t0 = 0.0f;
    float3 part4;
    float3 part3;
    float3 part2;
    for (int i = 0; i < 30; i++)
    {
        float a = float(i + 1) / 30.0f;
        float t1 = (d * a) * a;
        float dt = t1 - t0;
        float t = t0 + (dt * 0.5f);
        t0 = t1;
        float3 p = float3(v.xy * t, r + (v.z * t));
        float rp = length(p);
        float3 up = p / rp.xxx;
        float mus = dot(up, s);
        float part = rp - 6360.0f;
        atMedium(part, part4, part3, part2);
        float3 rs = part4;
        float3 ms = part3;
        float3 e = part2;
        float TsPart2 = rp;
        float TsPart = mus;
        float3 Ts = atTrans(T, _T_sampler, TsPart2, TsPart);
        float SPart3 = rp;
        float SPart2 = mus;
        float2 SPart = msSize;
        float3 S = (Ts * ((rs * pr) + (ms * pm))) + ((M.Sample(_M_sampler, atMsUv(SPart3, SPart2, SPart)).xyz * msOn) * (rs + ms));
        float3 Tst = exp((-e) * dt);
        L += (((thr * S) * (1.0f.xxx - Tst)) / max(e, 9.999999717180685365747194737196e-10f.xxx));
        thr *= Tst;
    }
    if (gnd && (ground > 0.5f))
    {
        float c = dot(normalize(float3(v.xy * d, r + (v.z * d))), s);
        float LPart2 = 6360.0f;
        float LPart = c;
        L += ((((thr * atTrans(T, _T_sampler, LPart2, LPart)) * max(c, 0.0f)) * 0.300000011920928955078125f.xxx) / 3.1415927410125732421875f.xxx);
    }
    return L;
}
#endif
#endif	// WW_HLSL_SRC
