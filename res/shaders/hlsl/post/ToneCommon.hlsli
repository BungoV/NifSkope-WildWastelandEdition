// Cell tone map: the shared math (lane VKFRAME1 P6, 2026-11-12). No bindings and no constant block: each app's shim declares
// its resources and calls these (NifSkope: ToneBindingsVk.hlsli + tonemap.hlsl on Vulkan; Fusion Shaders: its TonemapCS on its
// D3D slots, which F4FX_TonemapPhysicalSdr came from). The GL twins are res/shaders/features/exposure/cell_tonemap.glsl (the
// Physical curve), cell_hdr.frag (the modes, the glow's shoulder), features/lights/cell_lights.glsl cellImageSpace (the
// game's imagespace around the curve) and ww_dither.glsl. Every expression keeps the GL twin's order, and the literals carry
// an f so constant folding stays float (the gate, tests/vkframe/tonemap.sh, compares the two on the GPU).
// The Physical curve is adapted from Polyphony Digital's MIT-licensed GT7 Tone Mapping sample (the notice is in LICENSE.md).
#ifndef F4FX_TONE_COMMON_HLSLI
#define F4FX_TONE_COMMON_HLSLI

static const float F4FX_TM_PAPER = 250.0f;	// SDR paper white, nits
static const float F4FX_TM_REF = 100.0f;	// the curve's 1.0, nits
static const float F4FX_TM_BLEND = 0.60f;	// per-channel skew vs the chroma-kept mapping
static const float F4FX_TM_FADE0 = 0.98f;	// chroma fade start, of the peak's intensity
static const float F4FX_TM_FADE1 = 1.16f;	// chroma fade end
static const float F4FX_TM_ALPHA = 0.25f;
static const float F4FX_TM_MID = 0.538f;
static const float F4FX_TM_LIN = 0.444f;
static const float F4FX_TM_TOE = 1.280f;
static const float F4FX_TM_PEAK = F4FX_TM_PAPER / F4FX_TM_REF;	// 2.5

// a toe blended into a linear section, then an exponential shoulder toward the peak
float F4FX_TmCurve( float x )
{
	const float k = ( F4FX_TM_LIN - 1.0f ) / ( F4FX_TM_ALPHA - 1.0f );
	const float kA = F4FX_TM_PEAK * ( F4FX_TM_LIN + k );
	float kB = -F4FX_TM_PEAK * k * exp( F4FX_TM_LIN / k );
	const float kC = -1.0f / ( k * F4FX_TM_PEAK );
	x = max( x, 0.0f );
	if ( x < F4FX_TM_LIN * F4FX_TM_PEAK ) {
		float linearWeight = smoothstep( 0.0f, F4FX_TM_MID, x );
		float toe = F4FX_TM_MID * pow( max( x / F4FX_TM_MID, 0.0f ), F4FX_TM_TOE );
		return lerp( toe, x, linearWeight );
	}
	return kA + kB * exp( x * kC );
}

float F4FX_TmInvPq( float v )
{
	float y = max( v, 0.0f ) * F4FX_TM_REF / 10000.0f;
	float ym = pow( abs( y ), 0.1593017578125f );
	return exp2( 78.84375f * ( log2( 0.8359375f + 18.8515625f * ym ) - log2( 1.0f + 18.6875f * ym ) ) );
}

float F4FX_TmPqEotf( float v )
{
	float np = pow( clamp( v, 0.0f, 1.0f ), 1.0f / 78.84375f );
	float l = pow( max( np - 0.8359375f, 0.0f ) / max( 18.8515625f - 18.6875f * np, 0.0001f ), 1.0f / 0.1593017578125f );
	return l * 10000.0f / F4FX_TM_REF;
}

float3 F4FX_TmToICtCp( float3 rgb )
{
	float l = dot( rgb, float3( 1688.0f, 2146.0f, 262.0f ) ) / 4096.0f;
	float m = dot( rgb, float3( 683.0f, 2951.0f, 462.0f ) ) / 4096.0f;
	float s = dot( rgb, float3( 99.0f, 309.0f, 3688.0f ) ) / 4096.0f;
	float3 lmsPq = float3( F4FX_TmInvPq( l ), F4FX_TmInvPq( m ), F4FX_TmInvPq( s ) );
	return float3( 0.5f * ( lmsPq.x + lmsPq.y ), dot( lmsPq, float3( 6610.0f, -13613.0f, 7003.0f ) ) / 4096.0f,
		dot( lmsPq, float3( 17933.0f, -17390.0f, -543.0f ) ) / 4096.0f );
}

float3 F4FX_TmFromICtCp( float3 ictcp )
{
	float3 lmsPq = float3( ictcp.x + 0.00860904f * ictcp.y + 0.11103f * ictcp.z, ictcp.x - 0.00860904f * ictcp.y - 0.11103f * ictcp.z,
		ictcp.x + 0.560031f * ictcp.y - 0.320627f * ictcp.z );
	float3 lms = float3( F4FX_TmPqEotf( lmsPq.x ), F4FX_TmPqEotf( lmsPq.y ), F4FX_TmPqEotf( lmsPq.z ) );
	return max( float3( dot( lms, float3( 3.43661f, -2.50645f, 0.0698454f ) ), dot( lms, float3( -0.79133f, 1.9836f, -0.192271f ) ),
		dot( lms, float3( -0.0259499f, -0.0989137f, 1.12486f ) ) ), float3( 0.0f, 0.0f, 0.0f ) );
}

// the Physical operator, SDR output: in the exposed scene-linear light (Rec.709, 1.0 = paper white), out display-linear 0..1
float3 F4FX_TonemapPhysicalSdr( float3 c709 )
{
	c709 = max( c709, float3( 0.0f, 0.0f, 0.0f ) );
	float3 rgb = max( float3( dot( c709, float3( 0.6274040f, 0.3292820f, 0.0433136f ) ), dot( c709, float3( 0.0690970f, 0.9195400f, 0.0113612f ) ),
		dot( c709, float3( 0.0163916f, 0.0880132f, 0.8955950f ) ) ), float3( 0.0f, 0.0f, 0.0f ) ) * F4FX_TM_PEAK;
	float3 originalUcs = F4FX_TmToICtCp( rgb );
	float3 skewedRgb = float3( F4FX_TmCurve( rgb.r ), F4FX_TmCurve( rgb.g ), F4FX_TmCurve( rgb.b ) );
	float3 skewedUcs = F4FX_TmToICtCp( skewedRgb );
	float targetUcs = F4FX_TmToICtCp( float3( F4FX_TM_PEAK, F4FX_TM_PEAK, F4FX_TM_PEAK ) ).x;
	float normalizedLuminance = originalUcs.x / max( targetUcs, 0.0001f );
	float chromaScale = 1.0f - smoothstep( F4FX_TM_FADE0, F4FX_TM_FADE1, normalizedLuminance );
	float3 scaledRgb = F4FX_TmFromICtCp( float3( skewedUcs.x, originalUcs.yz * chromaScale ) );
	float3 m = min( lerp( skewedRgb, scaledRgb, F4FX_TM_BLEND ), float3( F4FX_TM_PEAK, F4FX_TM_PEAK, F4FX_TM_PEAK ) ) * ( F4FX_TM_REF / F4FX_TM_PAPER );
	float3 o = float3( dot( m, float3( 1.6604910f, -0.5876411f, -0.0728499f ) ), dot( m, float3( -0.1245505f, 1.1328999f, -0.0083494f ) ),
		dot( m, float3( -0.0181508f, -0.1005789f, 1.1187297f ) ) );
	return clamp( o, 0.0f, 1.0f );
}

// the Physical operator's grey axis (no chroma to fade) and its inverse, one channel
float F4FX_TmGrey( float e )
{
	return min( F4FX_TmCurve( max( e, 0.0f ) * F4FX_TM_PEAK ), F4FX_TM_PEAK ) * ( F4FX_TM_REF / F4FX_TM_PAPER );
}

float F4FX_TmGreyInv( float y )
{
	float v = clamp( y, 0.0f, 1.0f ) * F4FX_TM_PEAK;
	const float k = ( F4FX_TM_LIN - 1.0f ) / ( F4FX_TM_ALPHA - 1.0f );
	if ( v >= F4FX_TM_LIN * F4FX_TM_PEAK ) {
		const float kA = F4FX_TM_PEAK * ( F4FX_TM_LIN + k );
		float kB = -F4FX_TM_PEAK * k * exp( F4FX_TM_LIN / k );
		const float kC = -1.0f / ( k * F4FX_TM_PEAK );
		return log( ( v - kA ) / kB ) / kC / F4FX_TM_PEAK;
	}
	if ( v >= F4FX_TM_MID )
		return v / F4FX_TM_PEAK;
	float lo = 0.0f, hi = F4FX_TM_MID;	// the toe's blend has no closed inverse: bisect (monotonic, 16 steps)
	for ( int i = 0; i < 16; i++ ) {
		float mid = 0.5f * ( lo + hi );
		if ( F4FX_TmCurve( mid ) < v )
			lo = mid;
		else
			hi = mid;
	}
	return 0.5f * ( lo + hi ) / F4FX_TM_PEAK;
}

// the game's imagespace around the curve, the CPU's values (NifSkope: cellhdr.cpp / celllights.cpp uniforms)
struct F4FX_ImageSpace
{
	float exposure;		// middle grey / adapted, clamped
	float E;			// the vanilla filmic curve's toe numerator
	float adapted;		// the contrast pivot
	float3 cine;		// saturation, brightness, contrast
	float4 tint;		// amount, r, g, b
	float3 wbD;			// white balance gain - 1
	int red;			// 1 no LUT, 2 no exposure, 4 no grade
	bool vanCurve;		// the game's filmic curve instead of Physical
	bool lutOn;
	bool foOn;			// the Fusion Shaders bloom adds before the curve
	float foEc;			// > 0: the energy-conserving glare's k
	float foFogCap;
};

// the curve alone (the glow's shoulder): its input is 2 x the exposed light, out display-linear before the 1 / 2.2
float F4FX_CellCurveW( float x, F4FX_ImageSpace p )
{
	if ( !p.vanCurve )
		return F4FX_TmGrey( 0.5f * x );
	float E = p.E;
	float c = ( x * ( 0.15f * x + 0.05f ) + 0.2f * E ) / ( x * ( 0.15f * x + 0.5f ) + 0.06f ) - E / 0.3f;
	return c / ( ( 0.2f * E + 19.376f ) * 0.040856f - E / 0.3f );
}

float F4FX_CellCurveInv( float y, F4FX_ImageSpace p )
{
	if ( !p.vanCurve )
		return 2.0f * F4FX_TmGreyInv( y );
	float E = p.E;
	float yp = y * ( ( 0.2f * E + 19.376f ) * 0.040856f - E / 0.3f ) + E / 0.3f;
	float a = 0.15f * ( yp - 1.0f ), b = 0.5f * yp - 0.05f, c = 0.06f * yp - 0.2f * E;
	if ( abs( a ) < 1e-7f )
		return max( -c / b, 0.0f );
	return max( ( -b - sqrt( max( b * b - 4.0f * a * c, 0.0f ) ) ) / ( 2.0f * a ), 0.0f );
}

// the share of the glow's response from black the curve gives at the backdrop d's place on it, per channel
float3 F4FX_CellGlowShoulder( float3 d, float3 du, F4FX_ImageSpace p )
{
	float3 k = float3( 1.0f, 1.0f, 1.0f );
	[unroll] for ( int i = 0; i < 3; i++ ) {
		float u = F4FX_CellCurveInv( pow( clamp( d[i], 0.0f, 1.0f ), 2.2f ), p );
		float from0 = pow( max( F4FX_CellCurveW( du[i], p ), 0.0f ), 1.0f / 2.2f );
		float here = pow( max( F4FX_CellCurveW( u + du[i], p ), 0.0f ), 1.0f / 2.2f ) - pow( max( F4FX_CellCurveW( u, p ), 0.0f ), 1.0f / 2.2f );
		k[i] = from0 > 1e-6f ? clamp( here / from0, 0.0f, 1.0f ) : 1.0f;
	}
	return k;
}

// sqrt-of-linear in, display out. measureBloom: the measure's blurred bright pass at this sample, already read (added only
// when measureOn); gain: the fog's bloom gain (1 in clear air); foHere: the Fusion Shaders bloom times its weight (or 0).
float3 F4FX_CellImageSpace( float3 sqrtColor, bool measureOn, float3 measureBloom, float gain, float3 foHere, F4FX_ImageSpace p,
	Texture3D<float4> lut, SamplerState lutSampler )
{
	float3 x = max( sqrtColor, float3( 0.0f, 0.0f, 0.0f ) );
	x = x * x;
	if ( measureOn )
		x += gain * measureBloom;
	x = x * ( ( p.red & 2 ) != 0 ? 1.0f : p.exposure ) * 2.0f;
	x *= float3( 1.0f, 1.0f, 1.0f ) + p.wbD;
	if ( p.foOn && p.foEc > 0.0f ) {
		float kg = min( p.foEc * ( p.foFogCap > 0.0f ? min( gain, p.foFogCap ) : gain ), 1.0f );
		x = x * ( 1.0f - kg ) + 2.0f * ( kg / p.foEc ) * foHere;
	} else if ( p.foOn )
		x += 2.0f * gain * foHere;
	float3 c;
	if ( p.vanCurve ) {
		float E = p.E;
		c = ( x * ( 0.15f * x + 0.05f ) + 0.2f * E ) / ( x * ( 0.15f * x + 0.5f ) + 0.06f ) - E / 0.3f;
		c /= ( 0.2f * E + 19.376f ) * 0.040856f - E / 0.3f;
	} else
		c = F4FX_TonemapPhysicalSdr( 0.5f * x );
	if ( ( p.red & 4 ) == 0 ) {
		float luma = dot( c, float3( 0.2125f, 0.7154f, 0.0721f ) );
		c = lerp( float3( luma, luma, luma ), c, p.cine.x );
		c = lerp( c, luma * p.tint.yzw, p.tint.x );
		c = p.cine.z * ( p.cine.y * c - p.adapted ) + p.adapted;
	}
	c = pow( max( c, float3( 0.0f, 0.0f, 0.0f ) ), float3( 1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f ) );
	if ( p.lutOn && ( p.red & 1 ) == 0 )
		c = lut.SampleLevel( lutSampler, c * 0.9375f + 0.03125f, 0.0f ).rgb;
	return c;
}

// a uniform [0, 1) from a pixel and a seed (an integer hash)
float F4FX_DitherHash( uint2 p, uint s )
{
	uint h = p.x * 1664525u + p.y * 1013904223u + s * 2654435769u;
	h ^= h >> 16;
	h *= 2246822519u;
	h ^= h >> 13;
	h *= 3266489917u;
	h ^= h >> 16;
	return float( h >> 8 ) * ( 1.0f / 16777216.0f );
}

// a triangular +-1 level noise before an 8-bit write, faded to 0 at exactly 0 and 1
float3 F4FX_Dither8( float3 c, uint2 p, int on, int seed )
{
	if ( on == 0 )
		return c;
	uint s = uint( seed );
	float n = F4FX_DitherHash( p, 2u * s ) + F4FX_DitherHash( p, 2u * s + 1u ) - 1.0f;
	float3 w = clamp( min( c, 1.0f - c ) * 255.0f, 0.0f, 1.0f );
	return c + n * w * ( 1.0f / 255.0f );
}

#endif
