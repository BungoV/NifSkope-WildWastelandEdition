#pragma once
// lane SKINSSS1: screen-space skin subsurface scattering (row "Skin subsurface", WW_CELL_SKINSSS), the C++ twin of
// res/shaders/cell_skinsss.frag. The law is FO4CS's separable SSS (Extract, BlurX, BlurY, composite) with the
// skin profile's constants and the Jimenez / d'Eon 5-Gaussian kernel. No Qt: tests/skinsss reads it.
// WWSKINSSS_RED: the old path (no scattering: the composite keeps the frame).
#include <algorithm>
#include <cmath>

namespace WwSkinSss {

struct V3
{
	float r = 0.0f, g = 0.0f, b = 0.0f;
};

constexpr int kMaxTaps = 25;	// blur radius 24 plus the centre
// FO4CS kSkinProfile
constexpr float kAmount = 0.65f;
constexpr float kBlurRadius = 3.0f;	// in FO4CS work pixels (half resolution)
constexpr float kSaturation = 1.08f;
constexpr float kDepthThreshold = 0.003f;	// in device depth
constexpr float kDepthFalloff = 1.0f;
constexpr float kProfileRange = 2.0f;
constexpr float kMinCoverage = 1e-5f;
// FO4CS works at half resolution by default; the NifSkope pass runs at full resolution, so the kernel covers the
// same screen span with twice the taps
constexpr float kWorkScale = 2.0f;

inline float clamp01( float x )
{
	return std::min( std::max( x, 0.0f ), 1.0f );
}

inline V3 profileWeight( float normalizedRadius )
{
	static const float falloff[3] = { 1.0f, 0.37f, 0.30f };
	static const float gw[5] = { 0.100f, 0.118f, 0.113f, 0.358f, 0.078f };
	static const float gv[5] = { 0.0484f, 0.187f, 0.567f, 1.99f, 7.41f };
	const float r = clamp01( normalizedRadius ) * kProfileRange;
	float out[3];
	for ( int c = 0; c < 3; c++ ) {
		const float rr = r / ( 0.001f + falloff[c] );
		float s = 0.0f;
		for ( int k = 0; k < 5; k++ )
			s += gw[k] * std::exp( -( rr * rr ) / ( 2.0f * gv[k] ) ) / ( 6.28318530718f * gv[k] );
		out[c] = s;
	}
	return { out[0], out[1], out[2] };
}

inline int kernelRadius( float workScale = kWorkScale )
{
	return std::min( std::max( int( std::ceil( kBlurRadius * workScale ) ), 1 ), kMaxTaps - 1 );
}

inline void kernel( V3 out[kMaxTaps], int radius )
{
	for ( int i = 0; i < kMaxTaps; i++ )
		out[i] = i <= radius ? profileWeight( float( i ) / float( radius ) ) : V3();
}

// GL window depth of a standard projection = f / ( f - n ) * ( 1 - n / z ), the same function of view depth as the
// game's D3D device depth: the threshold is read on the frame's own depth buffer
inline float depthWeight( float centreDepth, float sampleDepth )
{
	return clamp01( 1.0f - std::fabs( sampleDepth - centreDepth ) / kDepthThreshold * kDepthFalloff );
}

inline V3 saturate( V3 c, float s )
{
	const float l = 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
	return { std::max( l + ( c.r - l ) * s, 0.0f ), std::max( l + ( c.g - l ) * s, 0.0f ), std::max( l + ( c.b - l ) * s, 0.0f ) };
}

struct Px
{
	V3 rgb;
	float a = 0.0f;
};

// extract: the skin tag (0 or 1) x amount is the coverage; the light is saturated
inline Px extract( V3 light, float skinTag )
{
	const float w = clamp01( skinTag ) * clamp01( kAmount );
	if ( w <= 0.0f )
		return Px();
	return { saturate( light, kSaturation ), w };
}

// one separable pass at the centre i of a line of n pixels (depth = the frame's window depth)
inline Px blur( const Px * line, const float * depth, int n, int i, const V3 * k, int radius )
{
	const Px & c = line[i];
	if ( c.a <= 0.0f )
		return Px();
	float sum[3] = { 0, 0, 0 }, ws[3] = { 0, 0, 0 };
	for ( int t = -radius; t <= radius; t++ ) {
		const int j = std::min( std::max( i + t, 0 ), n - 1 );
		const Px & s = line[j];
		const V3 & kw = k[std::abs( t )];
		const float dw = depthWeight( depth[i], depth[j] ) * s.a;
		const float kk[3] = { kw.r, kw.g, kw.b }, sv[3] = { s.rgb.r, s.rgb.g, s.rgb.b };
		for ( int ch = 0; ch < 3; ch++ ) {
			sum[ch] += sv[ch] * kk[ch] * dw;
			ws[ch] += kk[ch] * dw;
		}
	}
	const float cv[3] = { c.rgb.r, c.rgb.g, c.rgb.b };
	float o[3];
	for ( int ch = 0; ch < 3; ch++ )
		o[ch] = ws[ch] > kMinCoverage ? sum[ch] / ws[ch] : cv[ch];
	return { { o[0], o[1], o[2] }, c.a };
}

/* composite: FO4CS's baseLit + albedo x ( scattered - direct ) x w on a forward frame. The frame holds the lit
 * radiance (albedo x irradiance + specular), not the irradiance, so the light the extract took is that radiance and
 * the albedo is already in it: lit + ( scattered - lit ) x w */
inline V3 composite( V3 lit, Px scattered )
{
#ifdef WWSKINSSS_RED
	(void)scattered;
	return lit;
#else
	const float w = clamp01( scattered.a );
	return { lit.r + ( scattered.rgb.r - lit.r ) * w, lit.g + ( scattered.rgb.g - lit.g ) * w, lit.b + ( scattered.rgb.b - lit.b ) * w };
#endif
}

}	// namespace WwSkinSss
