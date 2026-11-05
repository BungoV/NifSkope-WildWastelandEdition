/* lane FIX1101A area T (LATE ITEM 26, bungo 11-01: "Fusion Shaders has the physical tonemapper ... add it and use it"):
 * the cell view's tone curve is Fusion Shaders' Physical operator, ported from FO4CS res/Post/TonemapCS.hlsl
 * (F4FX_PhysicalCurve, the ICtCp chroma fade, F4FX_TonemapPhysical in its SDR output mode). Adapted from Polyphony
 * Digital's MIT-licensed GT7 Tone Mapping sample (the notice is in LICENSE.md).
 * The constants are FO4CS's defaults: res/FO4CS.ini 184-199 (paper white 250 nits, blend 0.60, chroma fade
 * 0.98..1.16) and src/Post/Tonemap.cpp (the reference 100 nits, the curve's alpha 0.25, mid point 0.538, linear
 * section 0.444, toe strength 1.280). In:  the exposed scene-linear light (Rec.709, 1.0 = paper white).
 * Out: display-linear Rec.709 in 0..1, before the grade, the 1 / 2.2 and the LUT (cell_lights.glsl cellImageSpace).
 * WW_TONEMAP_RED=vanilla keeps the game's filmic curve (cellIsVanCurve). Twin: tests/tonemap1101a. */

const float kWwTmPaper = 250.0;		// fTonemapPhysicalSdrPaperWhiteNits: the SDR target
const float kWwTmRef = 100.0;		// kPhysicalReferenceNits: the curve's 1.0
const float kWwTmBlend = 0.60;		// fTonemapPhysicalBlendRatio: per-channel skew vs the chroma-kept mapping
const float kWwTmFade0 = 0.98;		// fTonemapPhysicalChromaFadeStart, of the peak's intensity
const float kWwTmFade1 = 1.16;		// fTonemapPhysicalChromaFadeEnd
const float kWwTmAlpha = 0.25;		// kPhysicalCurveAlpha
const float kWwTmMid = 0.538;		// kPhysicalCurveMidPoint
const float kWwTmLin = 0.444;		// kPhysicalCurveLinearSection
const float kWwTmToe = 1.280;		// kPhysicalCurveToeStrength
const float kWwTmPeak = kWwTmPaper / kWwTmRef;	// 2.5: the SDR peak in curve units

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
		return mix( toe, x, linearWeight );
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

vec3 wwTmToICtCp( vec3 rgb )
{
	float l = dot( rgb, vec3( 1688.0, 2146.0, 262.0 ) ) / 4096.0;
	float m = dot( rgb, vec3( 683.0, 2951.0, 462.0 ) ) / 4096.0;
	float s = dot( rgb, vec3( 99.0, 309.0, 3688.0 ) ) / 4096.0;
	vec3 lmsPq = vec3( wwTmInvPq( l ), wwTmInvPq( m ), wwTmInvPq( s ) );
	return vec3( 0.5 * ( lmsPq.x + lmsPq.y ), dot( lmsPq, vec3( 6610.0, -13613.0, 7003.0 ) ) / 4096.0,
		dot( lmsPq, vec3( 17933.0, -17390.0, -543.0 ) ) / 4096.0 );
}

vec3 wwTmFromICtCp( vec3 ictcp )
{
	vec3 lmsPq = vec3( ictcp.x + 0.00860904 * ictcp.y + 0.11103 * ictcp.z, ictcp.x - 0.00860904 * ictcp.y - 0.11103 * ictcp.z,
		ictcp.x + 0.560031 * ictcp.y - 0.320627 * ictcp.z );
	vec3 lms = vec3( wwTmPqEotf( lmsPq.x ), wwTmPqEotf( lmsPq.y ), wwTmPqEotf( lmsPq.z ) );
	return max( vec3( dot( lms, vec3( 3.43661, -2.50645, 0.0698454 ) ), dot( lms, vec3( -0.79133, 1.9836, -0.192271 ) ),
		dot( lms, vec3( -0.0259499, -0.0989137, 1.12486 ) ) ), vec3( 0.0 ) );
}

// F4FX_TonemapPhysical, the SDR output mode (outputParams = 250 nits, the blend, 100 / 250; chromaParams = the fade, 100)
vec3 wwTonemapPhysical( vec3 c709 )
{
	c709 = max( c709, vec3( 0.0 ) );
	vec3 rgb = max( vec3( dot( c709, vec3( 0.6274040, 0.3292820, 0.0433136 ) ), dot( c709, vec3( 0.0690970, 0.9195400, 0.0113612 ) ),
		dot( c709, vec3( 0.0163916, 0.0880132, 0.8955950 ) ) ), vec3( 0.0 ) ) * kWwTmPeak;	// paper white 1.0 -> curve units
	vec3 originalUcs = wwTmToICtCp( rgb );
	vec3 skewedRgb = vec3( wwTmCurve( rgb.r ), wwTmCurve( rgb.g ), wwTmCurve( rgb.b ) );
	vec3 skewedUcs = wwTmToICtCp( skewedRgb );
	float targetUcs = wwTmToICtCp( vec3( kWwTmPeak ) ).x;
	float normalizedLuminance = originalUcs.x / max( targetUcs, 0.0001 );
	float chromaScale = 1.0 - smoothstep( kWwTmFade0, kWwTmFade1, normalizedLuminance );
	vec3 scaledRgb = wwTmFromICtCp( vec3( skewedUcs.x, originalUcs.yz * chromaScale ) );
	vec3 m = min( mix( skewedRgb, scaledRgb, kWwTmBlend ), vec3( kWwTmPeak ) ) * ( kWwTmRef / kWwTmPaper );
	vec3 o = vec3( dot( m, vec3( 1.6604910, -0.5876411, -0.0728499 ) ), dot( m, vec3( -0.1245505, 1.1328999, -0.0083494 ) ),
		dot( m, vec3( -0.0181508, -0.1005789, 1.1187297 ) ) );
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
