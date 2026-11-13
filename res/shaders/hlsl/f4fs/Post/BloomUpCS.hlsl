#include "Common.hlsli"

Texture2D<float4> SourceColor : register(t0);
Texture2D<float4> BloomMip : register(t1);
SamplerState LinearClamp : register(s0);
RWTexture2D<float4> OutputColor : register(u0);

// LENS FLARE (src/Post/LensFlare.h): the final combine only. The flare texture and the
// lens-dirt mask arrive on t2/t3 and Params[2] = { flare intensity, dirt intensity, 0, 0 }.
// The dirt multiplies the bloom and the flare together. Not defined = the shipped combine,
// instruction for instruction.
#ifndef F4FS_BLOOM_LENS
#define F4FS_BLOOM_LENS 0
#endif
#if F4FS_BLOOM_LENS
Texture2D<float4> FlareColor : register(t2);
Texture2D<float> LensDirt : register(t3);
#endif

#ifndef F4FS_BLOOM_PAIRED_TENT
#define F4FS_BLOOM_PAIRED_TENT 1
#endif

#ifndef F4FS_BLOOM_PREBLURRED
#define F4FS_BLOOM_PREBLURRED 0
#endif

// FP16 maximum; the bloom pyramid is forced to FP16 (PostChain.cpp:1285-1294).
static const float F4FS_BLOOM_MAX_FINITE = 65504.0;

float3 F4FS_SanitizeBloomSample(float3 color)
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
	float3 bounded = sign(color) * min(abs(color), F4FS_BLOOM_MAX_FINITE.xxx);
	return clamp(bounded, 0.0.xxx, F4FS_BLOOM_MAX_FINITE.xxx);
}

float3 F4FS_UpsampleTent3x3(float2 uv, float2 offset)
{
	float3 color = F4FS_SanitizeBloomSample(BloomMip.SampleLevel(LinearClamp, uv, 0).rgb) * 4.0;
	color += F4FS_SanitizeBloomSample(BloomMip.SampleLevel(LinearClamp, uv + float2(-offset.x, 0.0), 0).rgb) * 2.0;
	color += F4FS_SanitizeBloomSample(BloomMip.SampleLevel(LinearClamp, uv + float2(offset.x, 0.0), 0).rgb) * 2.0;
	color += F4FS_SanitizeBloomSample(BloomMip.SampleLevel(LinearClamp, uv + float2(0.0, -offset.y), 0).rgb) * 2.0;
	color += F4FS_SanitizeBloomSample(BloomMip.SampleLevel(LinearClamp, uv + float2(0.0, offset.y), 0).rgb) * 2.0;
	color += F4FS_SanitizeBloomSample(BloomMip.SampleLevel(LinearClamp, uv + float2(-offset.x, -offset.y), 0).rgb);
	color += F4FS_SanitizeBloomSample(BloomMip.SampleLevel(LinearClamp, uv + float2(offset.x, -offset.y), 0).rgb);
	color += F4FS_SanitizeBloomSample(BloomMip.SampleLevel(LinearClamp, uv + float2(-offset.x, offset.y), 0).rgb);
	color += F4FS_SanitizeBloomSample(BloomMip.SampleLevel(LinearClamp, uv + float2(offset.x, offset.y), 0).rgb);
	return color * (1.0 / 16.0);
}

// At radius <= 1 this is the shipped 3x3 tent, instruction for instruction.
// Wider radii sample the SAME continuous tent, w = 1 - |x| / (2r), at a step
// of at most two coarse texels, so the bilinear footprints touch: a 3x3
// spread to radius 2-4 left gaps between its taps and stamped every small
// highlight as a 3x3 lattice of glows, once per mip. New taps enter at zero
// weight, so the glow's width is continuous as the radius slider moves.
// Host law: BloomUpsampleTentStepTexels / StepsPerSide / Weight (Bloom.h).
float3 F4FS_UpsampleTent(float2 uv, float2 texel, float radiusPixels)
{
#if F4FS_BLOOM_PREBLURRED
	// Padding retains the original clamp-before-filter boundary through the two-texel dispersion range.
	float2 size = 1.0 / texel;
	return F4FS_SanitizeBloomSample(BloomMip.SampleLevel(LinearClamp, (uv * size + 3.0) / (size + 6.0), 0).rgb);
#else
	float radius = max(radiusPixels, 0.0);
	float3 color;
	[branch]
	if (radius <= 1.0) {
		color = F4FS_UpsampleTent3x3(uv, texel * radius);
	} else if (F4FS_BLOOM_PAIRED_TENT && radius == 2.0) {
		// shortcut: pair radius 2; extend pairing if another radius profiles hot.
		// Folding in the bilinear phase first avoids shifting the blur.
		float2 pixel = uv / texel - 0.5;
		float2 phase = frac(pixel);
		float2 origin = floor(pixel) + 0.5;
		float4 pairBase = float4(-3.0, -1.0, 1.0, 3.0);
		float4 weightBase = float4(0.75, 1.75, 1.25, 0.25);
		float4 weightSlope = float4(-0.5, -0.5, 0.5, 0.5);
		float4 secondBase = float4(0.5, 1.0, 0.5, 0.0);
		float4 secondSlope = float4(-0.25, -0.25, 0.25, 0.25);
		float4 weightsX = weightBase + phase.x * weightSlope;
		float4 weightsY = weightBase + phase.y * weightSlope;
		float4 offsetsX = pairBase + (secondBase + phase.x * secondSlope) / weightsX;
		float4 offsetsY = pairBase + (secondBase + phase.y * secondSlope) / weightsY;
		color = 0.0;
		[unroll]
		for (int y = 0; y < 4; ++y) {
			[unroll]
			for (int x = 0; x < 4; ++x) {
				float2 sampleUv = (origin + float2(offsetsX[x], offsetsY[y])) * texel;
				color += F4FS_SanitizeBloomSample(BloomMip.SampleLevel(LinearClamp, sampleUv, 0).rgb) *
					(weightsX[x] * weightsY[y]);
			}
		}
		color *= 1.0 / 16.0;
	} else {
		float stepTexels = radius <= 2.0 ? 1.0 : 0.5 * radius;
		int stepsPerSide = (int)ceil(2.0 * radius / stepTexels) - 1;
		float weightSlope = stepTexels / (2.0 * radius);
		float2 step = texel * stepTexels;
		float3 sum = 0.0;
		float weightSum = 0.0;
		[loop]
		for (int y = -stepsPerSide; y <= stepsPerSide; ++y) {
			float weightY = max(1.0 - (float)abs(y) * weightSlope, 0.0);
			[loop]
			for (int x = -stepsPerSide; x <= stepsPerSide; ++x) {
				float weight = weightY * max(1.0 - (float)abs(x) * weightSlope, 0.0);
				sum += F4FS_SanitizeBloomSample(BloomMip.SampleLevel(LinearClamp, uv + step * float2(x, y), 0).rgb) * weight;
				weightSum += weight;
			}
		}
		color = sum / weightSum;
	}
	return color;
#endif
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
#if F4FS_BLOOM_PREBLURRED
	bloomWidth -= 6;
	bloomHeight -= 6;
#endif
	uint2 bloomSize = max(uint2(bloomWidth, bloomHeight), uint2(1, 1));
	float2 uv = ((float2)dispatchThreadId.xy + 0.5) / (float2)outputSize;
	float2 bloomTexel = 1.0 / (float2)bloomSize;
	// PYRAMID MAPPING. Params[3].xy is the render subrect as a fraction of this
	// output, uploaded on the final combine only (Bloom.cpp). Under an upscaler
	// the prefilter mapped the pyramid over that subrect, so the scene-space
	// bloom is read at uv / subrect while the source stays allocation-space; the
	// runtime dispatches the subrect alone. The zero row (every intermediate
	// step, whose output IS a pyramid level) and (1, 1) are the identity.
	float2 bloomScale = Params[3].xy;
	bloomScale = all(bloomScale > 0.0) ? bloomScale : 1.0.xx;
	float2 bloomUv = uv / bloomScale;

	float4 source = SourceColor.SampleLevel(LinearClamp, uv, 0);
	float3 sourceColor = F4FS_SanitizeBloomSample(source.rgb);
	uint sourceAlphaExponent = asuint(source.a) & 0x7F800000u;
	float sourceAlpha = sourceAlphaExponent == 0x7F800000u ? 1.0 : source.a;
	// Wavelength-dependent diffraction is approximated by a tiny radial
	// separation of the red/blue Gaussian bands. Green remains the luminance
	// anchor, avoiding the full-screen color shift of a post-process CA filter.
	// Centred on Params[1].yz, the scene's extent as a fraction of the bloom's
	// uv. The runtime maps the pyramid over the scene and uploads (1, 1); a zero
	// row keeps the old 0.5 centre as well.
	// At zero dispersion all three tents sample the same uv, so one is taken.
	float3 bloom;
	[branch]
	if (Params[1].x > 0.0) {
		float2 subrect = Params[1].yz;
		subrect = all(subrect > 0.0) ? subrect : 1.0.xx;
		float2 radial = bloomUv - 0.5 * subrect;
		float radialLength = length(radial);
		float2 dispersionOffset = radialLength > 1e-5 ?
			(radial / radialLength) * bloomTexel * max(Params[1].x, 0.0) :
			0.0.xx;
		float3 bloomGreen = F4FS_UpsampleTent(bloomUv, bloomTexel, Params[0].x);
		float3 bloomRed = F4FS_UpsampleTent(bloomUv + dispersionOffset, bloomTexel, Params[0].x);
		float3 bloomBlue = F4FS_UpsampleTent(bloomUv - dispersionOffset, bloomTexel, Params[0].x);
		bloom = float3(bloomRed.r, bloomGreen.g, bloomBlue.b);
	} else {
		bloom = F4FS_UpsampleTent(bloomUv, bloomTexel, Params[0].x);
	}
#if F4FS_BLOOM_LENS
	// The flare target and the dirt mask are scene-space, like the pyramid.
	float2 dirtSubrect = Params[1].yz;
	dirtSubrect = all(dirtSubrect > 0.0) ? dirtSubrect : 1.0.xx;
	float dirtGain = 1.0 + Params[2].y * LensDirt.SampleLevel(LinearClamp, bloomUv / dirtSubrect, 0);
	float3 flare = F4FS_SanitizeBloomSample(FlareColor.SampleLevel(LinearClamp, bloomUv, 0).rgb) * Params[2].x;
	float3 combined = sourceColor * Params[0].y + (bloom * Params[0].z + flare) * dirtGain;
#else
	float3 combined = sourceColor * Params[0].y + bloom * Params[0].z;
#endif

	OutputColor[dispatchThreadId.xy] = float4(combined, sourceAlpha);
}
