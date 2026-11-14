// HLSL source of features/exposure/cell_tonemap.glsl (include-only GLSL), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): the GLSL's comments,
// names and order kept. Its loose uniforms sit at the top under WW_LOOSE_UNIFORMS: the entry that includes this
// file includes it once more inside its LooseUniforms cbuffer with that defined (HLSL has one cbuffer for them).
#ifdef WW_HLSL_SRC
/* lane FIX1101A area T (LATE ITEM 26, bungo 11-01: "Fusion Shaders has the physical tonemapper ... add it and use it"):
 * the cell view's tone curve is Fusion Shaders' Physical operator, ported from FO4CS res/Post/TonemapCS.hlsl
 * (F4FX_PhysicalCurve, the ICtCp chroma fade, F4FX_TonemapPhysical in its SDR output mode). Adapted from Polyphony
 * Digital's MIT-licensed GT7 Tone Mapping sample (the notice is in LICENSE.md).
 * The constants are FO4CS's defaults: res/FO4CS.ini 184-199 (paper white 250 nits, blend 0.60, chroma fade
 * 0.98..1.16) and src/Post/Tonemap.cpp (the reference 100 nits, the curve's alpha 0.25, mid point 0.538, linear
 * section 0.444, toe strength 1.280). In:  the exposed scene-linear light (Rec.709, 1.0 = paper white).
 * Out: display-linear Rec.709 in 0..1, before the grade, the 1 / 2.2 and the LUT (cell_lights.glsl cellImageSpace).
 * WW_TONEMAP_RED=vanilla keeps the game's filmic curve (cellIsVanCurve). Twin: tests/tonemap1101a. */

static const float kWwTmPaper = 250.0;		// fTonemapPhysicalSdrPaperWhiteNits: the SDR target
static const float kWwTmRef = 100.0;		// kPhysicalReferenceNits: the curve's 1.0
static const float kWwTmBlend = 0.60;		// fTonemapPhysicalBlendRatio: per-channel skew vs the chroma-kept mapping
static const float kWwTmFade0 = 0.98;		// fTonemapPhysicalChromaFadeStart, of the peak's intensity
static const float kWwTmFade1 = 1.16;		// fTonemapPhysicalChromaFadeEnd
static const float kWwTmAlpha = 0.25;		// kPhysicalCurveAlpha
static const float kWwTmMid = 0.538;		// kPhysicalCurveMidPoint
static const float kWwTmLin = 0.444;		// kPhysicalCurveLinearSection
static const float kWwTmToe = 1.280;		// kPhysicalCurveToeStrength
static const float kWwTmPeak = kWwTmPaper / kWwTmRef;	// 2.5: the SDR peak in curve units

// F4FX_PhysicalCurve: a toe blended into a linear section, then an exponential shoulder toward the peak
float wwTmCurve( float x )
{
	const float k = ( kWwTmLin - 1.0 ) / ( kWwTmAlpha - 1.0 );
	const float kA = kWwTmPeak * ( kWwTmLin + k );
	float kB = -kWwTmPeak * k * exp( kWwTmLin / k );
	const float kC = -1.0 / ( k * kWwTmPeak );
	x = max( x, 0.0 );
	if ( x < kWwTmLin * kWwTmPeak ) {
		float linearWeight = smoothstep( 0.0, kWwTmMid, x );
		float toe = kWwTmMid * pow( max( x / kWwTmMid, 0.0 ), kWwTmToe );
		return lerp( toe, x, linearWeight );
	}
	return kA + kB * exp( x * kC );
}

float wwTmInvPq( float v )
{
	float y = max( v, 0.0 ) * kWwTmRef / 10000.0;
	float ym = pow( abs( y ), 0.1593017578125 );
	return exp2( 78.84375 * ( log2( 0.8359375 + 18.8515625 * ym ) - log2( 1.0 + 18.6875 * ym ) ) );
}

float wwTmPqEotf( float v )
{
	float np = pow( clamp( v, 0.0, 1.0 ), 1.0 / 78.84375 );
	float l = pow( max( np - 0.8359375, 0.0 ) / max( 18.8515625 - 18.6875 * np, 0.0001 ), 1.0 / 0.1593017578125 );
	return l * 10000.0 / kWwTmRef;
}

float3 wwTmToICtCp( float3 rgb )
{
	float l = dot( rgb, float3( 1688.0, 2146.0, 262.0 ) ) / 4096.0;
	float m = dot( rgb, float3( 683.0, 2951.0, 462.0 ) ) / 4096.0;
	float s = dot( rgb, float3( 99.0, 309.0, 3688.0 ) ) / 4096.0;
	float3 lmsPq = float3( wwTmInvPq( l ), wwTmInvPq( m ), wwTmInvPq( s ) );
	return float3( 0.5 * ( lmsPq.x + lmsPq.y ), dot( lmsPq, float3( 6610.0, -13613.0, 7003.0 ) ) / 4096.0,
		dot( lmsPq, float3( 17933.0, -17390.0, -543.0 ) ) / 4096.0 );
}

float3 wwTmFromICtCp( float3 ictcp )
{
	float3 lmsPq = float3( ictcp.x + 0.00860904 * ictcp.y + 0.11103 * ictcp.z, ictcp.x - 0.00860904 * ictcp.y - 0.11103 * ictcp.z,
		ictcp.x + 0.560031 * ictcp.y - 0.320627 * ictcp.z );
	float3 lms = float3( wwTmPqEotf( lmsPq.x ), wwTmPqEotf( lmsPq.y ), wwTmPqEotf( lmsPq.z ) );
	return max( float3( dot( lms, float3( 3.43661, -2.50645, 0.0698454 ) ), dot( lms, float3( -0.79133, 1.9836, -0.192271 ) ),
		dot( lms, float3( -0.0259499, -0.0989137, 1.12486 ) ) ), (float3)( 0.0 ) );
}

// F4FX_TonemapPhysical, the SDR output mode (outputParams = 250 nits, the blend, 100 / 250; chromaParams = the fade, 100)
float3 wwTonemapPhysical( float3 c709 )
{
	c709 = max( c709, (float3)( 0.0 ) );
	float3 rgb = max( float3( dot( c709, float3( 0.6274040, 0.3292820, 0.0433136 ) ), dot( c709, float3( 0.0690970, 0.9195400, 0.0113612 ) ),
		dot( c709, float3( 0.0163916, 0.0880132, 0.8955950 ) ) ), (float3)( 0.0 ) ) * kWwTmPeak;	// paper white 1.0 -> curve units
	float3 originalUcs = wwTmToICtCp( rgb );
	float3 skewedRgb = float3( wwTmCurve( rgb.r ), wwTmCurve( rgb.g ), wwTmCurve( rgb.b ) );
	float3 skewedUcs = wwTmToICtCp( skewedRgb );
	float targetUcs = wwTmToICtCp( (float3)( kWwTmPeak ) ).x;
	float normalizedLuminance = originalUcs.x / max( targetUcs, 0.0001 );
	float chromaScale = 1.0 - smoothstep( kWwTmFade0, kWwTmFade1, normalizedLuminance );
	float3 scaledRgb = wwTmFromICtCp( float3( skewedUcs.x, originalUcs.yz * chromaScale ) );
	float3 m = min( lerp( skewedRgb, scaledRgb, kWwTmBlend ), (float3)( kWwTmPeak ) ) * ( kWwTmRef / kWwTmPaper );
	float3 o = float3( dot( m, float3( 1.6604910, -0.5876411, -0.0728499 ) ), dot( m, float3( -0.1245505, 1.1328999, -0.0083494 ) ),
		dot( m, float3( -0.0181508, -0.1005789, 1.1187297 ) ) );
	return clamp( o, 0.0, 1.0 );
}

// the grey axis of wwTonemapPhysical (no chroma to fade: the skewed curve, clipped at the peak) and its inverse, for the
// glow's place on the curve (cell_hdr.frag cellGlowShoulder). In and out as wwTonemapPhysical's, one channel.
float wwTmGrey( float e )
{
	return min( wwTmCurve( max( e, 0.0 ) * kWwTmPeak ), kWwTmPeak ) * ( kWwTmRef / kWwTmPaper );
}

float wwTmGreyInv( float y )
{
	float v = clamp( y, 0.0, 1.0 ) * kWwTmPeak;
	const float k = ( kWwTmLin - 1.0 ) / ( kWwTmAlpha - 1.0 );
	if ( v >= kWwTmLin * kWwTmPeak ) {	// the shoulder: v = kA + kB exp( x kC ), kA > the peak
		const float kA = kWwTmPeak * ( kWwTmLin + k );
		float kB = -kWwTmPeak * k * exp( kWwTmLin / k );
		const float kC = -1.0 / ( k * kWwTmPeak );
		return log( ( v - kA ) / kB ) / kC / kWwTmPeak;
	}
	if ( v >= kWwTmMid )	// the linear section past the toe's blend
		return v / kWwTmPeak;
	float lo = 0.0, hi = kWwTmMid;	// the toe's blend has no closed inverse: bisect (monotonic, 16 steps: 1e-5)
	for ( int i = 0; i < 16; i++ ) {
		float mid = 0.5 * ( lo + hi );
		if ( wwTmCurve( mid ) < v )
			lo = mid;
		else
			hi = mid;
	}
	return 0.5 * ( lo + hi ) / kWwTmPeak;
}
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1, WW_FN_<name> picks functions); it goes with the last of them
// Shared HLSL for res/shaders/features/exposure/cell_tonemap.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// Included by: cell_hdr_fd_ps.hlsl, cell_hdr_ps.hlsl, fo4_cell_ps.hlsl, fo4_cellcsm_ps.hlsl, fo4_cellcsmfgc_ps.hlsl, fo4_cellcsmfgl_ps.hlsl, fo4_cellcsmfgp_ps.hlsl, fo4_cellcsmfs_ps.hlsl, fo4_cellfgc_ps.hlsl, fo4_cellfgl_ps.hlsl, fo4_cellfgp_ps.hlsl, fo4_cellfs_ps.hlsl, fo4_effectcell_ps.hlsl, fo4_effectcellfx_ps.hlsl, fo4_effectdecal_ps.hlsl, fo4_effectdecalcsm_ps.hlsl, fo4_water_ps.hlsl, fo4_waterlook_ps.hlsl, fo4_waterlookd_ps.hlsl, fo4_waterlookdp_ps.hlsl, fo4_waterlookm_ps.hlsl, fo4_waterlookp_ps.hlsl, impostor_cell_ps.hlsl, impostor_cellinst_ps.hlsl, impostor_cellinstez_ps.hlsl, particles_cell_ps.hlsl, pbrm_cell_ps.hlsl, pbrm_cellcsm_ps.hlsl
// Not here (declarations, or no converted entry carried them): wwTmGrey, wwTmGreyInv
#ifdef WW_FN_ALL
static const float kWwTmPaper = 250.0;
static const float kWwTmRef = 100.0;
static const float kWwTmBlend = 0.60;
static const float kWwTmFade0 = 0.98;
static const float kWwTmFade1 = 1.16;
static const float kWwTmAlpha = 0.25;
static const float kWwTmMid = 0.538;
static const float kWwTmLin = 0.444;
static const float kWwTmToe = 1.280;
#endif

#if defined(WW_FN_wwTmCurve) || defined(WW_FN_ALL)
float wwTmCurve(inout float x)
{
    float kB = -3.3733513355255126953125f;
    x = max(x, 0.0f);
    if (x < 1.11000001430511474609375f)
    {
        float linearWeight = smoothstep(0.0f, 0.53799998760223388671875f, x);
        float toe = 0.53799998760223388671875f * pow(max(x / 0.53799998760223388671875f, 0.0f), 1.2799999713897705078125f);
        return lerp(toe, x, linearWeight);
    }
    return 2.9633333683013916015625f + (kB * exp(x * (-0.539568364620208740234375f)));
}
#endif

#if defined(WW_FN_wwTmInvPq) || defined(WW_FN_ALL)
float wwTmInvPq(float v)
{
    float y = (max(v, 0.0f) * 100.0f) / 10000.0f;
    float ym = pow(abs(y), 0.1593017578125f);
    return exp2(78.84375f * (log2(0.8359375f + (18.8515625f * ym)) - log2(1.0f + (18.6875f * ym))));
}
#endif

#if defined(WW_FN_wwTmPqEotf) || defined(WW_FN_ALL)
float wwTmPqEotf(float v)
{
    float np = pow(clamp(v, 0.0f, 1.0f), 0.0126833133399486541748046875f);
    float l = pow(max(np - 0.8359375f, 0.0f) / max(18.8515625f - (18.6875f * np), 9.9999997473787516355514526367188e-05f), 6.277394771575927734375f);
    return (l * 10000.0f) / 100.0f;
}
#endif

#if defined(WW_FN_wwTmToICtCp) || defined(WW_FN_ALL)
float3 wwTmToICtCp(float3 rgb)
{
    float l = dot(rgb, float3(1688.0f, 2146.0f, 262.0f)) / 4096.0f;
    float m = dot(rgb, float3(683.0f, 2951.0f, 462.0f)) / 4096.0f;
    float s = dot(rgb, float3(99.0f, 309.0f, 3688.0f)) / 4096.0f;
    float lmsPqPart3 = l;
    float lmsPqPart2 = m;
    float lmsPqPart = s;
    float3 lmsPq = float3(wwTmInvPq(lmsPqPart3), wwTmInvPq(lmsPqPart2), wwTmInvPq(lmsPqPart));
    return float3(0.5f * (lmsPq.x + lmsPq.y), dot(lmsPq, float3(6610.0f, -13613.0f, 7003.0f)) / 4096.0f, dot(lmsPq, float3(17933.0f, -17390.0f, -543.0f)) / 4096.0f);
}
#endif

#if defined(WW_FN_wwTmFromICtCp) || defined(WW_FN_ALL)
float3 wwTmFromICtCp(float3 ictcp)
{
    float3 lmsPq = float3((ictcp.x + (0.00860903970897197723388671875f * ictcp.y)) + (0.11102999746799468994140625f * ictcp.z), (ictcp.x - (0.00860903970897197723388671875f * ictcp.y)) - (0.11102999746799468994140625f * ictcp.z), (ictcp.x + (0.560030996799468994140625f * ictcp.y)) - (0.3206270039081573486328125f * ictcp.z));
    float lmsPqPart3 = lmsPq.x;
    float lmsPqPart2 = lmsPq.y;
    float lmsPqPart = lmsPq.z;
    float3 lms = float3(wwTmPqEotf(lmsPqPart3), wwTmPqEotf(lmsPqPart2), wwTmPqEotf(lmsPqPart));
    return max(float3(dot(lms, float3(3.4366099834442138671875f, -2.5064499378204345703125f, 0.069845400750637054443359375f)), dot(lms, float3(-0.79132997989654541015625f, 1.98360002040863037109375f, -0.1922709941864013671875f)), dot(lms, float3(-0.0259498991072177886962890625f, -0.0989136993885040283203125f, 1.1248600482940673828125f))), 0.0f.xxx);
}
#endif

#if defined(WW_FN_wwTonemapPhysical) || defined(WW_FN_ALL)
float3 wwTonemapPhysical(inout float3 c709)
{
    c709 = max(c709, 0.0f.xxx);
    // paper white 1.0 -> curve units
    float3 rgb = max(float3(dot(c709, float3(0.6274039745330810546875f, 0.329281985759735107421875f, 0.043313600122928619384765625f)), dot(c709, float3(0.06909699738025665283203125f, 0.919539988040924072265625f, 0.0113612003624439239501953125f)), dot(c709, float3(0.01639159955084323883056640625f, 0.0880132019519805908203125f, 0.895595014095306396484375f))), 0.0f.xxx) * 2.5f;
    float3 lmsPqPart3 = rgb;
    float3 originalUcs = wwTmToICtCp(lmsPqPart3);
    float lmsPqPart2 = rgb.x;
    float lmsPqPart = wwTmCurve(lmsPqPart2);
    float wwTmCurvePart2 = rgb.y;
    float wwTmCurveRet2 = wwTmCurve(wwTmCurvePart2);
    float wwTmCurvePart = rgb.z;
    float wwTmCurveRet = wwTmCurve(wwTmCurvePart);
    float3 skewedRgb = float3(lmsPqPart, wwTmCurveRet2, wwTmCurveRet);
    float3 skewedUcsPart = skewedRgb;
    float3 skewedUcs = wwTmToICtCp(skewedUcsPart);
    float3 targetUcsPart = 2.5f.xxx;
    float targetUcs = wwTmToICtCp(targetUcsPart).x;
    float normalizedLuminance = originalUcs.x / max(targetUcs, 9.9999997473787516355514526367188e-05f);
    float chromaScale = 1.0f - smoothstep(0.980000019073486328125f, 1.15999996662139892578125f, normalizedLuminance);
    float3 scaledRgbPart = float3(skewedUcs.x, originalUcs.yz * chromaScale);
    float3 scaledRgb = wwTmFromICtCp(scaledRgbPart);
    float3 m = min(lerp(skewedRgb, scaledRgb, 0.60000002384185791015625f.xxx), 2.5f.xxx) * 0.4000000059604644775390625f;
    float3 o = float3(dot(m, float3(1.66049098968505859375f, -0.58764111995697021484375f, -0.0728498995304107666015625f)), dot(m, float3(-0.124550499022006988525390625f, 1.13289988040924072265625f, -0.0083494000136852264404296875f)), dot(m, float3(-0.01815080083906650543212890625f, -0.100578896701335906982421875f, 1.11872971057891845703125f)));
    return clamp(o, 0.0f.xxx, 1.0f.xxx);
}
#endif
#endif	// WW_HLSL_SRC
