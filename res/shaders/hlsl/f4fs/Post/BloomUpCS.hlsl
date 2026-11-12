#include "Common.hlsli"

Texture2D<float4> SourceColor : register(t0);
Texture2D<float4> BloomMip : register(t1);
SamplerState LinearClamp : register(s0);
RWTexture2D<float4> OutputColor : register(u0);

// FP16 maximum; the bloom pyramid is forced to FP16 (PostChain.cpp:1285-1294).
static const float F4FX_BLOOM_MAX_FINITE = 65504.0;

float3 F4FX_SanitizeBloomSample(float3 color)
{
	// Sanitize before weighting: one NaN otherwise poisons the whole tent and
	// grows from a single coarse-mip texel into a large black screen rectangle.
	// Bit inspection is intentional: FXC may optimize isnan/isinf away when the
	// runtime compiler does not enable its global IEEE-strict flag.
	uint3 bits = asuint(color);
	uint3 exponent = bits & 0x7F800000u;
	uint3 mantissa = bits & 0x007FFFFFu;
	bool nanX = exponent.x == 0x7F800000u && mantissa.x != 0u;
	bool nanY = exponent.y == 0x7F800000u && mantissa.y != 0u;
	bool nanZ = exponent.z == 0x7F800000u && mantissa.z != 0u;
	if (nanX || nanY || nanZ) {
		return 0.0.xxx;
	}
	// BLOOM FLICKER FIX F2 (2026-08-07). Infinity is clamped rather than zeroed,
	// for the reason spelled out in BloomDownCS: deleting an overflowing tap
	// removes the highlight's glow completely instead of saturating it. Identity
	// on every finite value.
	float3 bounded = sign(color) * min(abs(color), F4FX_BLOOM_MAX_FINITE.xxx);
	return clamp(bounded, 0.0.xxx, F4FX_BLOOM_MAX_FINITE.xxx);
}

float3 F4FX_UpsampleTent(float2 uv, float2 texel, float radiusPixels)
{
	float2 offset = texel * max(radiusPixels, 0.0);
	float3 color = F4FX_SanitizeBloomSample(BloomMip.SampleLevel(LinearClamp, uv, 0).rgb) * 4.0;
	color += F4FX_SanitizeBloomSample(BloomMip.SampleLevel(LinearClamp, uv + float2(-offset.x, 0.0), 0).rgb) * 2.0;
	color += F4FX_SanitizeBloomSample(BloomMip.SampleLevel(LinearClamp, uv + float2(offset.x, 0.0), 0).rgb) * 2.0;
	color += F4FX_SanitizeBloomSample(BloomMip.SampleLevel(LinearClamp, uv + float2(0.0, -offset.y), 0).rgb) * 2.0;
	color += F4FX_SanitizeBloomSample(BloomMip.SampleLevel(LinearClamp, uv + float2(0.0, offset.y), 0).rgb) * 2.0;
	color += F4FX_SanitizeBloomSample(BloomMip.SampleLevel(LinearClamp, uv + float2(-offset.x, -offset.y), 0).rgb);
	color += F4FX_SanitizeBloomSample(BloomMip.SampleLevel(LinearClamp, uv + float2(offset.x, -offset.y), 0).rgb);
	color += F4FX_SanitizeBloomSample(BloomMip.SampleLevel(LinearClamp, uv + float2(-offset.x, offset.y), 0).rgb);
	color += F4FX_SanitizeBloomSample(BloomMip.SampleLevel(LinearClamp, uv + float2(offset.x, offset.y), 0).rgb);
	return color * (1.0 / 16.0);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	uint outputWidth;
	uint outputHeight;
	OutputColor.GetDimensions(outputWidth, outputHeight);
	uint2 outputSize = max(uint2(outputWidth, outputHeight), uint2(1, 1));
	if (any(dispatchThreadId.xy >= outputSize)) {
		return;
	}

	uint bloomWidth;
	uint bloomHeight;
	BloomMip.GetDimensions(bloomWidth, bloomHeight);
	uint2 bloomSize = max(uint2(bloomWidth, bloomHeight), uint2(1, 1));
	float2 uv = ((float2)dispatchThreadId.xy + 0.5) / (float2)outputSize;
	float2 bloomTexel = 1.0 / (float2)bloomSize;

	float4 source = SourceColor.SampleLevel(LinearClamp, uv, 0);
	float3 sourceColor = F4FX_SanitizeBloomSample(source.rgb);
	uint sourceAlphaExponent = asuint(source.a) & 0x7F800000u;
	float sourceAlpha = sourceAlphaExponent == 0x7F800000u ? 1.0 : source.a;
	// Wavelength-dependent diffraction is approximated by a tiny radial
	// separation of the red/blue Gaussian bands. Green remains the luminance
	// anchor, avoiding the full-screen color shift of a post-process CA filter.
	float2 radial = uv - 0.5;
	float radialLength = length(radial);
	float2 dispersionOffset = radialLength > 1e-5 ?
		(radial / radialLength) * bloomTexel * max(Params[1].x, 0.0) :
		0.0.xx;
	float3 bloomGreen = F4FX_UpsampleTent(uv, bloomTexel, Params[0].x);
	float3 bloomRed = F4FX_UpsampleTent(uv + dispersionOffset, bloomTexel, Params[0].x);
	float3 bloomBlue = F4FX_UpsampleTent(uv - dispersionOffset, bloomTexel, Params[0].x);
	float3 bloom = float3(bloomRed.r, bloomGreen.g, bloomBlue.b);
	float3 combined = sourceColor * Params[0].y + bloom * Params[0].z;

	OutputColor[dispatchThreadId.xy] = float4(combined, sourceAlpha);
}
