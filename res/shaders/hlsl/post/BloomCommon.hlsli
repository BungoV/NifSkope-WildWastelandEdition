// Bloom pyramid: the shared math (lane VKFRAME1, 2026-11-12). No bindings and no constant block: each app's shim declares
// its resources and calls these (NifSkope: BloomBindingsVk.hlsli + bloom_down/hold/up.hlsl on Vulkan; Fusion Shaders: its
// BloomDownCS / BloomUpCS on its D3D slots). Originals: Fusion Shaders Post/BloomDownCS.hlsl and BloomUpCS.hlsl; the GL twins
// are res/shaders/features/post/cell_bloom.glsl, cell_bloom_down.frag, cell_bloom_hold.frag, cell_bloom_up.frag, and every
// expression below keeps the GL twin's order (the gate, tests/vkframe, compares the two on the GPU).
#ifndef F4FX_BLOOM_COMMON_HLSLI
#define F4FX_BLOOM_COMMON_HLSLI

// FP16 maximum: the pyramid is half float
static const float F4FX_BLOOM_MAX_FINITE = 65504.0;
// F4FX_LUM_EPSILON of Post/Common.hlsli, under its own name so both files can be included together
static const float F4FX_BLOOM_LUM_EPSILON = 0.0001;

// F4FX_Luminance (Post/Common.hlsli), the bloom's copy
float F4FX_BloomLuminance( float3 color )
{
	return max( dot( color, float3( 0.2126, 0.7152, 0.0722 ) ), F4FX_BLOOM_LUM_EPSILON );
}

// A NaN tap reads 0 (tested by its bits: a compiler may fold isnan away), anything else clamps to 0..65504; infinity
// clamps rather than vanishes, so an overflowing highlight keeps its glow
float3 F4FX_SanitizeBloomSample( float3 color )
{
	const uint3 bits = asuint( color );
	const uint3 exponent = bits & 0x7F800000u;
	const uint3 mantissa = bits & 0x007FFFFFu;
	if ( ( exponent.x == 0x7F800000u && mantissa.x != 0u ) || ( exponent.y == 0x7F800000u && mantissa.y != 0u )
		|| ( exponent.z == 0x7F800000u && mantissa.z != 0u ) )
		return float3( 0.0, 0.0, 0.0 );
	const float3 bounded = sign( color ) * min( abs( color ), F4FX_BLOOM_MAX_FINITE.xxx );
	return clamp( bounded, 0.0.xxx, F4FX_BLOOM_MAX_FINITE.xxx );
}

// The prefilter's threshold with a soft knee: the share of a texel's light that blooms
float F4FX_BloomThresholdWeight( float luminance, float threshold, float softKnee )
{
	luminance = max( luminance, 0.0 );
	if ( luminance <= F4FX_BLOOM_LUM_EPSILON )
		return 0.0;
	threshold = max( threshold, 0.0 );
	const float knee = threshold * max( softKnee, 0.0 );
	const float hard = luminance - threshold;
	float contribution = max( hard, 0.0 );
	if ( knee > F4FX_BLOOM_LUM_EPSILON ) {
		const float soft = clamp( hard + knee, 0.0, 2.0 * knee );
		contribution = max( contribution, ( soft * soft ) / ( 4.0 * knee ) );
	}
	return clamp( contribution / luminance, 0.0, 1.0 );
}

// The 2x2 box of one down pass at uv (the four taps half a source texel apart, bilinear); karis = 1 weights each tap by
// 1 / (1 + its luminance) (the prefilter level's firefly control), 0 = the plain box
float3 F4FX_DownsampleTent( Texture2D<float4> source, SamplerState linearClamp, float2 uv, float2 texel, float karis )
{
	float3 taps[4];
	taps[0] = F4FX_SanitizeBloomSample( source.SampleLevel( linearClamp, uv + texel * float2( -0.5, -0.5 ), 0.0 ).rgb );
	taps[1] = F4FX_SanitizeBloomSample( source.SampleLevel( linearClamp, uv + texel * float2( 0.5, -0.5 ), 0.0 ).rgb );
	taps[2] = F4FX_SanitizeBloomSample( source.SampleLevel( linearClamp, uv + texel * float2( -0.5, 0.5 ), 0.0 ).rgb );
	taps[3] = F4FX_SanitizeBloomSample( source.SampleLevel( linearClamp, uv + texel * float2( 0.5, 0.5 ), 0.0 ).rgb );
	float3 color = float3( 0.0, 0.0, 0.0 );
	float weightSum = 0.0;
	[unroll]
	for ( int t = 0; t < 4; t++ ) {
		const float w = lerp( 1.0, 1.0 / ( 1.0 + F4FX_BloomLuminance( taps[t] ) ), karis );
		color += taps[t] * w;
		weightSum += w;
	}
	return color / max( weightSum, F4FX_BLOOM_LUM_EPSILON );
}

// One down level: the box, then (prefilter = 1, level 0 only) the threshold, times the scale
float3 F4FX_BloomDown( Texture2D<float4> source, SamplerState linearClamp, float2 uv, float2 texel, float threshold,
	float softKnee, float prefilter, float karis, float scale )
{
	const float3 color = F4FX_DownsampleTent( source, linearClamp, uv, texel, karis );
	const float extract = lerp( 1.0, F4FX_BloomThresholdWeight( F4FX_BloomLuminance( color ), threshold, softKnee ), prefilter );
	return color * extract * max( scale, 0.0 );
}

// The 9-tap tent (4 / 2 2 2 2 / 1 1 1 1 over 16) at the bloom texel times the radius
float3 F4FX_UpsampleTent( Texture2D<float4> bloomMip, SamplerState linearClamp, float2 uv, float2 texel, float radiusPixels )
{
	const float2 o = texel * max( radiusPixels, 0.0 );
	float3 c = F4FX_SanitizeBloomSample( bloomMip.SampleLevel( linearClamp, uv, 0.0 ).rgb ) * 4.0;
	c += F4FX_SanitizeBloomSample( bloomMip.SampleLevel( linearClamp, uv + float2( -o.x, 0.0 ), 0.0 ).rgb ) * 2.0;
	c += F4FX_SanitizeBloomSample( bloomMip.SampleLevel( linearClamp, uv + float2( o.x, 0.0 ), 0.0 ).rgb ) * 2.0;
	c += F4FX_SanitizeBloomSample( bloomMip.SampleLevel( linearClamp, uv + float2( 0.0, -o.y ), 0.0 ).rgb ) * 2.0;
	c += F4FX_SanitizeBloomSample( bloomMip.SampleLevel( linearClamp, uv + float2( 0.0, o.y ), 0.0 ).rgb ) * 2.0;
	c += F4FX_SanitizeBloomSample( bloomMip.SampleLevel( linearClamp, uv + float2( -o.x, -o.y ), 0.0 ).rgb );
	c += F4FX_SanitizeBloomSample( bloomMip.SampleLevel( linearClamp, uv + float2( o.x, -o.y ), 0.0 ).rgb );
	c += F4FX_SanitizeBloomSample( bloomMip.SampleLevel( linearClamp, uv + float2( -o.x, o.y ), 0.0 ).rgb );
	c += F4FX_SanitizeBloomSample( bloomMip.SampleLevel( linearClamp, uv + float2( o.x, o.y ), 0.0 ).rgb );
	return c * ( 1.0 / 16.0 );
}

// The glare's dispersion: red and blue tents pushed apart along the radial from the frame's centre, green the anchor
// (no push at the centre: one tent)
float3 F4FX_UpsampleTentDispersed( Texture2D<float4> bloomMip, SamplerState linearClamp, float2 uv, float2 texel,
	float radiusPixels, float dispersion )
{
	const float2 radial = uv - 0.5;
	const float len = length( radial );
	const float2 d = len > 1e-5 ? ( radial / len ) * texel * max( dispersion, 0.0 ) : float2( 0.0, 0.0 );
	const float3 g = F4FX_UpsampleTent( bloomMip, linearClamp, uv, texel, radiusPixels );
	if ( all( d == float2( 0.0, 0.0 ) ) )
		return g;
	const float3 r = F4FX_UpsampleTent( bloomMip, linearClamp, uv + d, texel, radiusPixels );
	const float3 b = F4FX_UpsampleTent( bloomMip, linearClamp, uv - d, texel, radiusPixels );
	return float3( r.r, g.g, b.b );
}

// The tent down the top's mips (GL twin cell_bloom.glsl bloomTentLod): the taps at mip lod, spread by 2^lod level-0 texels
float3 F4FX_UpsampleTentLod( Texture2D<float4> bloomMip, SamplerState linearClamp, float2 uv, float2 texel, float radiusPixels,
	float lod )
{
	const float2 o = texel * exp2( lod ) * max( radiusPixels, 0.0 );
	float3 c = F4FX_SanitizeBloomSample( bloomMip.SampleLevel( linearClamp, uv, lod ).rgb ) * 4.0;
	c += F4FX_SanitizeBloomSample( bloomMip.SampleLevel( linearClamp, uv + float2( -o.x, 0.0 ), lod ).rgb ) * 2.0;
	c += F4FX_SanitizeBloomSample( bloomMip.SampleLevel( linearClamp, uv + float2( o.x, 0.0 ), lod ).rgb ) * 2.0;
	c += F4FX_SanitizeBloomSample( bloomMip.SampleLevel( linearClamp, uv + float2( 0.0, -o.y ), lod ).rgb ) * 2.0;
	c += F4FX_SanitizeBloomSample( bloomMip.SampleLevel( linearClamp, uv + float2( 0.0, o.y ), lod ).rgb ) * 2.0;
	c += F4FX_SanitizeBloomSample( bloomMip.SampleLevel( linearClamp, uv + float2( -o.x, -o.y ), lod ).rgb );
	c += F4FX_SanitizeBloomSample( bloomMip.SampleLevel( linearClamp, uv + float2( o.x, -o.y ), lod ).rgb );
	c += F4FX_SanitizeBloomSample( bloomMip.SampleLevel( linearClamp, uv + float2( -o.x, o.y ), lod ).rgb );
	c += F4FX_SanitizeBloomSample( bloomMip.SampleLevel( linearClamp, uv + float2( o.x, o.y ), lod ).rgb );
	return c * ( 1.0 / 16.0 );
}

// The dispersed tent at mip lod (GL twin bloomTentDispersedLod): the push stays in level-0 texels
float3 F4FX_UpsampleTentDispersedLod( Texture2D<float4> bloomMip, SamplerState linearClamp, float2 uv, float2 texel,
	float radiusPixels, float dispersion, float lod )
{
	const float2 radial = uv - 0.5;
	const float len = length( radial );
	const float2 d = len > 1e-5 ? ( radial / len ) * texel * max( dispersion, 0.0 ) : float2( 0.0, 0.0 );
	const float3 g = F4FX_UpsampleTentLod( bloomMip, linearClamp, uv, texel, radiusPixels, lod );
	if ( all( d == float2( 0.0, 0.0 ) ) )
		return g;
	const float3 r = F4FX_UpsampleTentLod( bloomMip, linearClamp, uv + d, texel, radiusPixels, lod );
	const float3 b = F4FX_UpsampleTentLod( bloomMip, linearClamp, uv - d, texel, radiusPixels, lod );
	return float3( r.r, g.g, b.b );
}

// One up level: this level's band x its weight + the tent of the accumulated chain below x its weight
float3 F4FX_BloomUp( Texture2D<float4> source, Texture2D<float4> bloomMip, SamplerState linearClamp, float2 uv, float2 bloomTexel,
	float radiusPixels, float sourceWeight, float bloomWeight, float dispersion )
{
	const float3 band = F4FX_SanitizeBloomSample( source.SampleLevel( linearClamp, uv, 0.0 ).rgb );
	const float3 bloom = F4FX_UpsampleTentDispersed( bloomMip, linearClamp, uv, bloomTexel, radiusPixels, dispersion );
	return band * sourceWeight + bloom * bloomWeight;
}

// The level-0 hold over the temporal jitter's period: the mean of the ring's n newest layers (k = the newest, 8 layers)
float3 F4FX_BloomHoldMean( Texture2DArray<float4> ring, int2 p, int k, int n )
{
	n = clamp( n, 1, 8 );
	float3 c = float3( 0.0, 0.0, 0.0 );
	for ( int i = 0; i < n; i++ )
		c += ring.Load( int4( p, ( k - i + 8 ) & 7, 0 ) ).rgb;
	return c / float( n );
}

// The bloom's source, one sample (lane VKFRAME1 P3; GL twin cell_bloom_src.frag): a lit sample's light (never negative)
// times the exposure (1 under the exposure's red), as the half-float target stores it. lit: the sample's stencil said
// "a cell program or a cell effect wrote it last" (stencil & 3 == 1), on: the cell's lights and its imagespace are on.
// Alpha 1 for a lit sample, 0 else: the resolve's mean is the lit share.
float4 F4FX_BloomSourceSample( float3 hdr, float exposure, bool lit, bool on )
{
	if ( !lit )
		return float4( 0.0, 0.0, 0.0, 0.0 );
	if ( !on )
		return float4( 0.0, 0.0, 0.0, 0.0 );
	const float3 c = max( hdr, 0.0.xxx ) * exposure;
	return float4( f16tof32( f32tof16( c ) ), 1.0 );
}

// The bloom's test probe (cell_bloom_src.frag bloomImpulse): value v on the 2x2 block holding (x, y), 0 elsewhere
float4 F4FX_BloomImpulse( int2 p, float2 at, float v )
{
	return float4( all( p / 2 == int2( at ) / 2 ) ? v.xxx : 0.0.xxx, 1.0 );
}

#endif
