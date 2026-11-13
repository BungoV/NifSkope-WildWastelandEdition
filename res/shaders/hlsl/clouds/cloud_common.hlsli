// HLSL source of features/clouds/cloud_common.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use
struct CloudBands {
	float3 baseKm;	// Low, Mid, High base, km above z = 0
	float3 topKm;
	float3 density;	// relative optical thickness, 1 = a plain cumulus; 0 = an empty band
	float3 coverage;	// outside the map square
	float sheet;	// the Low band's sheet share outside the map square
	float4 mapRect;	// world km: x0, y0, 1 / (2 halfExtent); w = 1 when the map texture is bound
	float3 windKm;	// the noise offset (wind x time), km
};
#elif defined(WW_LOOSE_UNIFORMS)
	CloudBands cvBands;
	float cvCal[34];	// coverage -> threshold coverage, 17 per band (cloudvolmath.h calibrate)
#if !(!defined(WW_CLOUD_VARIETY))
	/* lane CLOUD5: Cloud Variety (the *_v.prog twins define WW_CLOUD_VARIETY; Off draws the function above unchanged).
	 * The Low band takes a cloud TYPE 0..1 (stratus, stratocumulus, cumulus, cumulonimbus), each with its own depth and
	 * profile; a weather noise over tens of km swings the coverage (mean kept) and the type; the detail lookup is displaced
	 * by a curl field and eroded more, less at the base. Sources and the CPU twin: src/features/clouds/cloudvolmath.h varietyDensity. */
	float4 cvVarA;	// x the weather's Low type, y the type's swing, z the coverage's swing, w the Low band's own depth km
	float4 cvVarB;	// x a cumulonimbus' depth km, y the erosion, z the curl km (0 = red nocurl), w the anvil
	float cvCalT[221];	// the Low band's coverage tables at T = 1 - (1 - k / 12)^2, 13 x 17 (cloudvolmath.h kVarCal, varCalT)
#endif
#else
#ifndef WW_GLSL_MOD
#define WW_GLSL_MOD	// GLSL mod: x - y floor(x / y) (HLSL fmod truncates)
float mod(float x, float y) { return x - y * floor(x / y); }
float2 mod(float2 x, float2 y) { return x - y * floor(x / y); }
float3 mod(float3 x, float3 y) { return x - y * floor(x / y); }
float4 mod(float4 x, float4 y) { return x - y * floor(x / y); }
float2 mod(float2 x, float y) { return x - y * floor(x / y); }
float3 mod(float3 x, float y) { return x - y * floor(x / y); }
float4 mod(float4 x, float y) { return x - y * floor(x / y); }
#endif
// lane CLOUD2: the volume clouds' density, ONE function for every reader (the march, CLOUD3's shadows, CLOUD4).
// The CPU twin is src/features/clouds/cloudvolmath.h (bandDensity, gradient, calAt): keep the two in step. p = (world x km,
// world y km, height km above z = 0). The band heights live in the one struct below (src/features/clouds/cloudvol.cpp sets it).


Texture3D<float4> cvShape : register(t0, space0); SamplerState _cvShape_sampler : register(s0, space0);	// 128^3 R8 Perlin-Worley (src/features/clouds/cloudnoise.h), period kCvShapeKm
Texture3D<float4> cvDetail : register(t1, space0); SamplerState _cvDetail_sampler : register(s1, space0);	// 32^3 R8 Worley fBm, period kCvDetailKm
Texture2D<float4> cvMap : register(t2, space0); SamplerState _cvMap_sampler : register(s2, space0);	// CLOUD1's map: R Low, G Mid, B Low sheet, A High coverage

static const float kCvShapeKm = 6.0;
static const float kCvDetailKm = 0.6;
static const float kCvSigma = 40.0;	// extinction /km at density 1
static float cvFootKm = 0.0;	// a reader's pixel footprint at p, km (picks the noise mips; 0 = the finest, mip 0)

float cvCalAt( int b, float c )
{
	float t = clamp( c, 0.0, 1.0 ) * 16.0;
	int i = min( int( t ), 15 );
	return lerp( cvCal[b * 17 + i], cvCal[b * 17 + i + 1], t - float( i ) );
}

float4 cvMapAt( float2 xy )
{
	float2 uv = ( xy - cvBands.mapRect.xy ) * cvBands.mapRect.z;
	if ( or( or( cvBands.mapRect.w < 0.5, any( (uv < (float2)( 0.0 )) ) ), any( (uv > (float2)( 1.0 )) ) ) )
		return float4( cvBands.coverage.x, cvBands.coverage.y, cvBands.coverage.x * cvBands.sheet, cvBands.coverage.z );
	return cvMap.SampleLevel( _cvMap_sampler, uv, 0.0 );
}

float cvSmooth( float a, float b, float x )
{
	float t = clamp( ( x - a ) / ( b - a ), 0.0, 1.0 );
	return t * t * ( 3.0 - 2.0 * t );
}

float cvGradient( int b, float hf, float sheet )
{
	if ( b == 0 ) {
		float cu = cvSmooth( 0.0, 0.07, hf ) * ( 1.0 - cvSmooth( 0.55, 1.0, hf ) );
		float st = cvSmooth( 0.0, 0.04, hf ) * ( 1.0 - cvSmooth( 0.12, 0.3, hf ) );
		return lerp( cu, st, clamp( sheet, 0.0, 1.0 ) );
	}
	return cvSmooth( 0.0, 0.15, hf ) * ( 1.0 - cvSmooth( 0.35, 0.9, hf ) );
}

#ifndef WW_CLOUD_VARIETY
// relative density (extinction = kCvSigma x this, /km); 0 outside the Low and Mid bands
float cloudDensity( float3 p )
{
	float4 m = cvMapAt( p.xy );
	float d = 0.0;
	for ( int b = 0; b < 2; b++ ) {
		float base = cvBands.baseKm[b], top = cvBands.topKm[b];
		float hf = ( p.z - base ) / max( top - base, 1e-3 );
		if ( or( or( hf <= 0.0, hf >= 1.0 ), cvBands.density[b] <= 0.0 ) )
			continue;
		float cov = b == 0 ? m.r : m.g;
		float sheet = b == 0 ? clamp( m.b / max( m.r, 1e-3 ), 0.0, 1.0 ) : 0.0;
		float c = cvCalAt( b, cov );
		if ( c <= 0.0 )
			continue;
		float3 q = p + cvBands.windKm;
		float s = cvShape.SampleLevel( _cvShape_sampler, q / kCvShapeKm, log2( max( cvFootKm * 128.0 / kCvShapeKm, 1.0 ) ) ).r;
		float t = 1.0 - c;
		float base_ = clamp( ( s * cvGradient( b, hf, sheet ) - t ) / max( 1.0 - t, 1e-4 ), 0.0, 1.0 ) * c;
		if ( base_ <= 0.0 )
			continue;
		float det = cvDetail.SampleLevel( _cvDetail_sampler, q / kCvDetailKm, log2( max( cvFootKm * 32.0 / kCvDetailKm, 1.0 ) ) ).r;
		float mod_ = lerp( det, 1.0 - det, clamp( hf * 4.0, 0.0, 1.0 ) ) * 0.2;
		d += clamp( ( base_ - mod_ ) / ( 1.0 - mod_ ), 0.0, 1.0 ) * cvBands.density[b];
	}
	return d;
}
#else


static const float kCvVarWrapKm = 384.0;	// cloudvolmath.h kVarWrapKm (the wind's wrap)
static const float kCvCurlPeriodKm = 24.0;

float cvVarHash( int ix, int iy, int oct )
{
	uint h = uint( ix ) * 0x8da6b343u ^ uint( iy ) * 0xd8163841u ^ uint( oct ) * 0xcb1ab31fu;
	h ^= h >> 13u;
	h *= 0x5bd1e995u;
	h ^= h >> 15u;
	return float( h >> 8u ) * ( 1.0 / 16777216.0 );
}

// antithetic: half a wrap east holds 1 - the value, so the noise's mean over the wrap is exactly 0 (cloudvolmath.h varLattice)
float cvVarLattice( int ix, int iy, int cells, int oct )
{
	int h = cells / 2;
	return ix >= h ? 1.0 - cvVarHash( ix - h, iy, oct ) : cvVarHash( ix, iy, oct );
}

float cvVarValue( float2 xy, float cellKm, int cells, int oct )
{
	float2 u = mod( xy, (float2)( kCvVarWrapKm ) ) / cellKm;
	int2 i = min( int2( floor( u ) ), (int2)( cells - 1 ) );
	float2 f = u - float2( i );
	int2 j = int2( i.x + 1 == cells ? 0 : i.x + 1, i.y + 1 == cells ? 0 : i.y + 1 );
	float2 w = f * f * ( 3.0 - 2.0 * f );
	float a = cvVarLattice( i.x, i.y, cells, oct ), b = cvVarLattice( j.x, i.y, cells, oct );
	float c = cvVarLattice( i.x, j.y, cells, oct ), e = cvVarLattice( j.x, j.y, cells, oct );
	return lerp( lerp( a, b, w.x ), lerp( c, e, w.x ), w.y );
}

float cvWeatherNoise( float2 xy )
{
	float v = 0.65 * cvVarValue( xy, 16.0, 24, 0 ) + 0.35 * cvVarValue( xy, 6.0, 64, 1 );
	return clamp( ( v - 0.5 ) * 4.0, -1.0, 1.0 );
}

float3 cvCurlRaw( float3 q )
{
	float3 u = mod( q, (float3)( kCvCurlPeriodKm ) ) / kCvCurlPeriodKm;
	float c1 = cos( 6.2831853 * frac( 13.0 * u.x + 5.0 * u.y + 0.11 ) );
	float c2 = cos( 6.2831853 * frac( -6.0 * u.x + 11.0 * u.y + 0.33 ) );
	float c3 = cos( 6.2831853 * frac( 10.0 * u.x + 8.0 * u.z + 0.64 ) );
	float c4 = cos( 6.2831853 * frac( -7.0 * u.x + 12.0 * u.z + 0.85 ) );
	float ax = c1 * 0.358979 + c2 * 0.877896, ay = c1 * -0.933346 + c2 * 0.478852;
	float bx = c3 * 0.624695 + c4 * 0.863779, bz = c3 * -0.780869 + c4 * 0.503871;
	return 0.5 * float3( ax + bx, ay, bz );
}

float cvTypeDepth( float T )
{
	float u = clamp( T, 0.0, 1.0 ) * 3.0, dc = cvVarA.w;
	if ( u < 1.0 )
		return dc * lerp( 0.3, 0.6, u );
	if ( u < 2.0 )
		return dc * lerp( 0.6, 1.0, u - 1.0 );
	return lerp( dc, max( cvVarB.x, dc ), u - 2.0 );
}

float cvTypeProfile( float T, float hf )
{
	const float rise[4] = { 0.10, 0.08, 0.07, 0.03 };
	const float fall[4] = { 0.60, 0.45, 0.55, 0.85 };
	float u = clamp( T, 0.0, 1.0 ) * 3.0;
	int k = min( int( u ), 2 );
	// the rise and fall heights blend, not the profiles' values (cloudvolmath.h typeProfile: no shoulder half way up)
	float r = lerp( rise[k], rise[k + 1], u - float( k ) ), f = lerp( fall[k], fall[k + 1], u - float( k ) );
	return cvSmooth( 0.0, r, hf ) * ( 1.0 - cvSmooth( f, 1.0, hf ) );
}

float cvCalAtT( float T, float c )
{
	float u = ( 1.0 - sqrt( 1.0 - clamp( T, 0.0, 1.0 ) ) ) * 12.0;	// cloudvolmath.h varCalT's inverse
	int k = min( int( u ), 11 );
	float t = clamp( c, 0.0, 1.0 ) * 16.0;
	int i = min( int( t ), 15 );
	float f = t - float( i );
	float a = lerp( cvCalT[k * 17 + i], cvCalT[k * 17 + i + 1], f );
	float b = lerp( cvCalT[k * 17 + 17 + i], cvCalT[k * 17 + 18 + i], f );
	return lerp( a, b, u - float( k ) );
}

float cloudDensity( float3 p )
{
	float4 m = cvMapAt( p.xy );
	float3 q = p + cvBands.windKm;
	float n = cvWeatherNoise( q.xy );
	float d = 0.0, s = -1.0;
	float3 curl = (float3)( 0.0 );
	bool curlOk = false;
	for ( int b = 0; b < 2; b++ ) {
		if ( cvBands.density[b] <= 0.0 )
			continue;
		float cov = b == 0 ? m.r : m.g;
		cov += cvVarA.z * n * min( cov, 1.0 - cov );
		float hf, prof, c;
		if ( b == 0 ) {
			float sheet = clamp( m.b / max( m.r, 1e-3 ), 0.0, 1.0 );
			float T = clamp( cvVarA.x + cvVarA.y * n, 0.0, 1.0 ) * ( 1.0 - sheet );
			hf = ( p.z - cvBands.baseKm.x ) / max( cvTypeDepth( T ), 1e-3 );
			if ( or( hf <= 0.0, hf >= 1.0 ) )
				continue;
			c = cvCalAtT( T, cov );
			float e = lerp( 1.0, 0.5, cvVarB.w * cvSmooth( 0.8, 1.0, T ) * cvSmooth( 0.65, 0.85, hf ) );
			c = c > 0.0 ? pow( c, e ) : 0.0;
			prof = cvTypeProfile( T, hf );
		} else {
			hf = ( p.z - cvBands.baseKm.y ) / max( cvBands.topKm.y - cvBands.baseKm.y, 1e-3 );
			if ( or( hf <= 0.0, hf >= 1.0 ) )
				continue;
			c = cvCalAt( 1, cov );
			prof = cvGradient( 1, hf, 0.0 );
		}
		if ( c <= 0.0 )
			continue;
		if ( s < 0.0 )
			s = cvShape.SampleLevel( _cvShape_sampler, q / kCvShapeKm, log2( max( cvFootKm * 128.0 / kCvShapeKm, 1.0 ) ) ).r;
		float t = 1.0 - c;
		float base_ = clamp( ( s * prof - t ) / max( 1.0 - t, 1e-4 ), 0.0, 1.0 ) * c;
		if ( base_ <= 0.0 )
			continue;
		if ( !curlOk ) {
			curl = cvCurlRaw( q );
			curlOk = true;
		}
		float3 qd = q + curl * ( cvVarB.z * ( 1.0 - 0.5 * hf ) );
		float det = cvDetail.SampleLevel( _cvDetail_sampler, qd / kCvDetailKm, log2( max( cvFootKm * 32.0 / kCvDetailKm, 1.0 ) ) ).r;
		float er = cvVarB.y * lerp( 0.55, 1.0, cvSmooth( 0.0, 0.35, hf ) );
		float mod_ = lerp( det, 1.0 - det, clamp( hf * 4.0, 0.0, 1.0 ) ) * er;
		d += clamp( ( base_ - mod_ ) / ( 1.0 - mod_ ), 0.0, 1.0 ) * cvBands.density[b];
	}
	return d;
}
#endif
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/clouds/cloud_common.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cloud_fly_a_ps.hlsl, cloud_fly_ps.hlsl, cloud_fly_v_ps.hlsl, cloud_flybake_ps.hlsl, cloud_flybake_v_ps.hlsl, cloud_march_a_ps.hlsl, cloud_march_ps.hlsl, cloud_march_v_ps.hlsl, cloud_shadowmap_ps.hlsl, cloud_shadowmap_v_ps.hlsl
// Not here (declarations, or no converted entry carried them): cvTypeProfile
#ifdef WW_FN_ALL
static const float kCvShapeKm = 6.0;
static const float kCvDetailKm = 0.6;
static const float kCvSigma = 40.0;
static const float kCvVarWrapKm = 384.0;
static const float kCvCurlPeriodKm = 24.0;
#endif

// GLSL mix(x, y, a) as NV GL lowers it: fma(y - x, a, x) (fitted on cvCalAt and mod_, HLSLBULK1 r10)
#ifndef WW_CVMIX
#define WW_CVMIX
float cvMix(float x, float y, float a)
{
    return mad(y - x, a, x);
}
#endif

#if defined(WW_FN_cvCalAt) || defined(WW_FN_ALL)
float cvCalAt(int b, float c)
{
    float t = clamp(c, 0.0f, 1.0f) * 16.0f;
    int i = min(int(t), 15);
    return cvMix(cvCal[(b * 17) + i], cvCal[((b * 17) + i) + 1], t - float(i));
}
#endif

#if defined(WW_FN_cvMapAt) || defined(WW_FN_ALL)
float4 cvMapAt(float2 xy)
{
    float2 uv = (xy - cvBands.mapRect.xy) * cvBands.mapRect.z;
    bool cond3 = cvBands.mapRect.w < 0.5f;
    bool cond2;
    if (!cond3)
    {
        cond2 = any(bool2(uv.x < 0.0f.xx.x, uv.y < 0.0f.xx.y));
    }
    else
    {
        cond2 = cond3;
    }
    bool cond;
    if (!cond2)
    {
        cond = any(bool2(uv.x > 1.0f.xx.x, uv.y > 1.0f.xx.y));
    }
    else
    {
        cond = cond2;
    }
    if (cond)
    {
        return float4(cvBands.coverage.x, cvBands.coverage.y, cvBands.coverage.x * cvBands.sheet, cvBands.coverage.z);
    }
    return cvMap.SampleLevel(_cvMap_sampler, uv, 0.0f);
}
#endif

#if defined(WW_FN_cvSmooth) || defined(WW_FN_ALL)
float cvSmooth(float a, float b, float x)
{
    float t = clamp((x - a) / (b - a), 0.0f, 1.0f);
    return (t * t) * (3.0f - (2.0f * t));
}
#endif

#if defined(WW_FN_cvGradient) || defined(WW_FN_ALL)
float cvGradient(int b, float hf, float sheet)
{
    if (b == 0)
    {
        float cond3 = 0.0f;
        float cond2 = 0.070000000298023223876953125f;
        float cond = hf;
        float cuPart3 = 0.550000011920928955078125f;
        float cuPart2 = 1.0f;
        float cuPart = hf;
        float cu = cvSmooth(cond3, cond2, cond) * (1.0f - cvSmooth(cuPart3, cuPart2, cuPart));
        float stPart6 = 0.0f;
        float stPart5 = 0.039999999105930328369140625f;
        float stPart4 = hf;
        float stPart3 = 0.119999997317790985107421875f;
        float stPart2 = 0.300000011920928955078125f;
        float stPart = hf;
        float st = cvSmooth(stPart6, stPart5, stPart4) * (1.0f - cvSmooth(stPart3, stPart2, stPart));
        return cvMix(cu, st, clamp(sheet, 0.0f, 1.0f));
    }
    float cvGradientRet6 = 0.0f;
    float cvGradientRet5 = 0.1500000059604644775390625f;
    float cvGradientRet4 = hf;
    float cvGradientRet3 = 0.3499999940395355224609375f;
    float cvGradientRet2 = 0.89999997615814208984375f;
    float cvGradientRet = hf;
    return cvSmooth(cvGradientRet6, cvGradientRet5, cvGradientRet4) * (1.0f - cvSmooth(cvGradientRet3, cvGradientRet2, cvGradientRet));
}
#endif

#if defined(WW_FN_cloudDensity) || defined(WW_FN_ALL)
float cloudDensity(float3 p)
{
    float2 cond3 = p.xy;
    float4 m = cvMapAt(cond3);
    float d = 0.0f;
    float cond2;
    float cond;
    for (int b = 0; b < 2; b++)
    {
        float base = cvBands.baseKm[b];
        float top = cvBands.topKm[b];
        float hf = (p.z - base) / max(top - base, 0.001000000047497451305389404296875f);
        bool cuPart3 = (hf <= 0.0f) || (hf >= 1.0f);
        bool cuPart2;
        if (!cuPart3)
        {
            cuPart2 = cvBands.density[b] <= 0.0f;
        }
        else
        {
            cuPart2 = cuPart3;
        }
        if (cuPart2)
        {
            continue;
        }
        if (b == 0)
        {
            cond2 = m.x;
        }
        else
        {
            cond2 = m.y;
        }
        float cov = cond2;
        if (b == 0)
        {
            cond = clamp(m.z / max(m.x, 0.001000000047497451305389404296875f), 0.0f, 1.0f);
        }
        else
        {
            cond = 0.0f;
        }
        float sheet = cond;
        int cuPart = b;
        float stPart6 = cov;
        float c = cvCalAt(cuPart, stPart6);
        if (c <= 0.0f)
        {
            continue;
        }
        float3 q = p + cvBands.windKm;
        float s = cvShape.SampleLevel(_cvShape_sampler, q / 6.0f.xxx, log2(max((cvFootKm * 128.0f) / 6.0f, 1.0f))).x;
        float t = 1.0f - c;
        int stPart5 = b;
        float stPart4 = hf;
        float stPart3 = sheet;
        float base_ = clamp(mad(s, cvGradient(stPart5, stPart4, stPart3), -t) / max(1.0f - t, 9.9999997473787516355514526367188e-05f), 0.0f, 1.0f) * c;
        if (base_ <= 0.0f)
        {
            continue;
        }
        float det = cvDetail.SampleLevel(_cvDetail_sampler, q / 0.60000002384185791015625f.xxx, log2(max((cvFootKm * 32.0f) / 0.60000002384185791015625f, 1.0f))).x;
        float mod_ = cvMix(det, 1.0f - det, clamp(hf * 4.0f, 0.0f, 1.0f)) * 0.20000000298023223876953125f;
        d = mad(clamp((base_ - mod_) / (1.0f - mod_), 0.0f, 1.0f), cvBands.density[b], d);
    }
    return d;
}
#endif

#if defined(WW_FN_cvVarHash) || defined(WW_FN_ALL)
float cvVarHash(int ix, int iy, int oct)
{
    uint h = ((uint(ix) * 2376512323u) ^ (uint(iy) * 3625334849u)) ^ (uint(oct) * 3407524639u);
    h ^= (h >> 13u);
    h *= 1540483477u;
    h ^= (h >> 15u);
    return float(h >> 8u) * 5.9604644775390625e-08f;
}
#endif

#if defined(WW_FN_cvVarLattice) || defined(WW_FN_ALL)
float cvVarLattice(int ix, int iy, int cells, int oct)
{
    int h = cells / 2;
    float cond3;
    if (ix >= h)
    {
        int cond2 = ix - h;
        int cond = iy;
        int cuPart3 = oct;
        cond3 = 1.0f - cvVarHash(cond2, cond, cuPart3);
    }
    else
    {
        int cuPart2 = ix;
        int cuPart = iy;
        int stPart6 = oct;
        cond3 = cvVarHash(cuPart2, cuPart, stPart6);
    }
    return cond3;
}
#endif

#if defined(WW_FN_cvVarValue) || defined(WW_FN_ALL)
float cvVarValue(float2 xy, float cellKm, int cells, int oct)
{
    float2 u = mod(xy, 384.0f.xx) / cellKm.xx;
    int2 i = min(int2(floor(u)), (cells - 1).xx);
    float2 f = u - float2(i);
    int cond3;
    if ((i.x + 1) == cells)
    {
        cond3 = 0;
    }
    else
    {
        cond3 = i.x + 1;
    }
    int cond2;
    if ((i.y + 1) == cells)
    {
        cond2 = 0;
    }
    else
    {
        cond2 = i.y + 1;
    }
    int2 j = int2(cond3, cond2);
    float2 w = (f * f) * (3.0f.xx - (f * 2.0f));
    int cond = i.x;
    int cuPart3 = i.y;
    int cuPart2 = cells;
    int cuPart = oct;
    float a = cvVarLattice(cond, cuPart3, cuPart2, cuPart);
    int stPart6 = j.x;
    int stPart5 = i.y;
    int stPart4 = cells;
    int stPart3 = oct;
    float b = cvVarLattice(stPart6, stPart5, stPart4, stPart3);
    int stPart2 = i.x;
    int stPart = j.y;
    int cvGradientRet6 = cells;
    int cvGradientRet5 = oct;
    float c = cvVarLattice(stPart2, stPart, cvGradientRet6, cvGradientRet5);
    int cvGradientRet4 = j.x;
    int cvGradientRet3 = j.y;
    int cvGradientRet2 = cells;
    int cvGradientRet = oct;
    float e = cvVarLattice(cvGradientRet4, cvGradientRet3, cvGradientRet2, cvGradientRet);
    return lerp(lerp(a, b, w.x), lerp(c, e, w.x), w.y);
}
#endif

#if defined(WW_FN_cvWeatherNoise) || defined(WW_FN_ALL)
float cvWeatherNoise(float2 xy)
{
    float2 cond3 = xy;
    float cond2 = 16.0f;
    int cond = 24;
    int cuPart3 = 0;
    float2 cuPart2 = xy;
    float cuPart = 6.0f;
    int stPart6 = 64;
    int stPart5 = 1;
    float v = (0.64999997615814208984375f * cvVarValue(cond3, cond2, cond, cuPart3)) + (0.3499999940395355224609375f * cvVarValue(cuPart2, cuPart, stPart6, stPart5));
    return clamp((v - 0.5f) * 4.0f, -1.0f, 1.0f);
}
#endif

#if defined(WW_FN_cvCurlRaw) || defined(WW_FN_ALL)
float3 cvCurlRaw(float3 q)
{
    float3 u = mod(q, 24.0f.xxx) / 24.0f.xxx;
    float c1 = cos(6.283185482025146484375f * frac(((13.0f * u.x) + (5.0f * u.y)) + 0.10999999940395355224609375f));
    float c2 = cos(6.283185482025146484375f * frac((((-6.0f) * u.x) + (11.0f * u.y)) + 0.3300000131130218505859375f));
    float c3 = cos(6.283185482025146484375f * frac(((10.0f * u.x) + (8.0f * u.z)) + 0.63999998569488525390625f));
    float c4 = cos(6.283185482025146484375f * frac((((-7.0f) * u.x) + (12.0f * u.z)) + 0.85000002384185791015625f));
    float ax = (c1 * 0.3589789867401123046875f) + (c2 * 0.877896010875701904296875f);
    float ay = (c1 * (-0.933345973491668701171875f)) + (c2 * 0.4788520038127899169921875f);
    float bx = (c3 * 0.624695003032684326171875f) + (c4 * 0.863779008388519287109375f);
    float bz = (c3 * (-0.780869007110595703125f)) + (c4 * 0.503871023654937744140625f);
    return float3(ax + bx, ay, bz) * 0.5f;
}
#endif

#if defined(WW_FN_cvTypeDepth) || defined(WW_FN_ALL)
float cvTypeDepth(float T)
{
    float u = clamp(T, 0.0f, 1.0f) * 3.0f;
    float dc = cvVarA.w;
    if (u < 1.0f)
    {
        return dc * lerp(0.300000011920928955078125f, 0.60000002384185791015625f, u);
    }
    if (u < 2.0f)
    {
        return dc * lerp(0.60000002384185791015625f, 1.0f, u - 1.0f);
    }
    return lerp(dc, max(cvVarB.x, dc), u - 2.0f);
}
#endif

#if defined(WW_FN_cvCalAtT) || defined(WW_FN_ALL)
float cvCalAtT(float T, float c)
{
    float u = (1.0f - sqrt(1.0f - clamp(T, 0.0f, 1.0f))) * 12.0f;
    int k = min(int(u), 11);
    float t = clamp(c, 0.0f, 1.0f) * 16.0f;
    int i = min(int(t), 15);
    float f = t - float(i);
    float a = lerp(cvCalT[(k * 17) + i], cvCalT[((k * 17) + i) + 1], f);
    float b = lerp(cvCalT[((k * 17) + 17) + i], cvCalT[((k * 17) + 18) + i], f);
    return lerp(a, b, u - float(k));
}
#endif

#if defined(WW_FN_cloudDensity_V2) || defined(WW_FN_ALL)
float cloudDensity(float3 p)
{
    float2 cond3 = p.xy;
    float4 m = cvMapAt(cond3);
    float3 q = p + cvBands.windKm;
    float2 cond2 = q.xy;
    float n = cvWeatherNoise(cond2);
    float d = 0.0f;
    float s = -1.0f;
    float3 curl = 0.0f.xxx;
    bool curlOk = false;
    float cond;
    float hf;
    float c;
    float cuPart3;
    float prof;
    for (int b = 0; b < 2; b++)
    {
        if (cvBands.density[b] <= 0.0f)
        {
            continue;
        }
        if (b == 0)
        {
            cond = m.x;
        }
        else
        {
            cond = m.y;
        }
        float cov = cond;
        cov += ((cvVarA.z * n) * min(cov, 1.0f - cov));
        if (b == 0)
        {
            float sheet = clamp(m.z / max(m.x, 0.001000000047497451305389404296875f), 0.0f, 1.0f);
            float T = clamp(cvVarA.x + (cvVarA.y * n), 0.0f, 1.0f) * (1.0f - sheet);
            float cuPart2 = T;
            hf = (p.z - cvBands.baseKm.x) / max(cvTypeDepth(cuPart2), 0.001000000047497451305389404296875f);
            bool wwh0 = (hf <= 0.0f);
            bool wwh1 = (hf >= 1.0f);
            if (or(wwh0, wwh1))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
            {
                continue;
            }
            float cuPart = T;
            float stPart6 = cov;
            c = cvCalAtT(cuPart, stPart6);
            float stPart5 = 0.800000011920928955078125f;
            float stPart4 = 1.0f;
            float stPart3 = T;
            float stPart2 = 0.64999997615814208984375f;
            float stPart = 0.85000002384185791015625f;
            float cvGradientRet6 = hf;
            float e = lerp(1.0f, 0.5f, (cvVarB.w * cvSmooth(stPart5, stPart4, stPart3)) * cvSmooth(stPart2, stPart, cvGradientRet6));
            if (c > 0.0f)
            {
                cuPart3 = pow(c, e);
            }
            else
            {
                cuPart3 = 0.0f;
            }
            c = cuPart3;
            float cvGradientRet5 = T;
            float cvGradientRet4 = hf;
            prof = cvTypeProfile(cvGradientRet5, cvGradientRet4);
        }
        else
        {
            hf = (p.z - cvBands.baseKm.y) / max(cvBands.topKm.y - cvBands.baseKm.y, 0.001000000047497451305389404296875f);
            bool wwh2 = (hf <= 0.0f);
            bool wwh3 = (hf >= 1.0f);
            if (or(wwh2, wwh3))   // HLSLFIX2 orfix.py: no short-circuit branch (NV took the wrong side)
            {
                continue;
            }
            int cvGradientRet3 = 1;
            float cvGradientRet2 = cov;
            c = cvCalAt(cvGradientRet3, cvGradientRet2);
            int cvGradientRet = 1;
            float profPart2 = hf;
            float profPart = 0.0f;
            prof = cvGradient(cvGradientRet, profPart2, profPart);
        }
        if (c <= 0.0f)
        {
            continue;
        }
        if (s < 0.0f)
        {
            s = cvShape.SampleLevel(_cvShape_sampler, q / 6.0f.xxx, log2(max((cvFootKm * 128.0f) / 6.0f, 1.0f))).x;
        }
        float t = 1.0f - c;
        float base_ = clamp(((s * prof) - t) / max(1.0f - t, 9.9999997473787516355514526367188e-05f), 0.0f, 1.0f) * c;
        if (base_ <= 0.0f)
        {
            continue;
        }
        if (!curlOk)
        {
            float3 curlPart = q;
            curl = cvCurlRaw(curlPart);
            curlOk = true;
        }
        float3 qd = q + (curl * (cvVarB.z * (1.0f - (0.5f * hf))));
        float det = cvDetail.SampleLevel(_cvDetail_sampler, qd / 0.60000002384185791015625f.xxx, log2(max((cvFootKm * 32.0f) / 0.60000002384185791015625f, 1.0f))).x;
        float erPart3 = 0.0f;
        float erPart2 = 0.3499999940395355224609375f;
        float erPart = hf;
        float er = cvVarB.y * lerp(0.550000011920928955078125f, 1.0f, cvSmooth(erPart3, erPart2, erPart));
        float mod_ = lerp(det, 1.0f - det, clamp(hf * 4.0f, 0.0f, 1.0f)) * er;
        d += (clamp((base_ - mod_) / (1.0f - mod_), 0.0f, 1.0f) * cvBands.density[b]);
    }
    return d;
}
#endif
#endif	// WW_HLSL_SRC
