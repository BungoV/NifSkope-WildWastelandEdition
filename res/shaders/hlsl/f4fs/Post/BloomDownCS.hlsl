#include "Common.hlsli"

Texture2D<float4> SourceColor : register(t0);
// EMISSIVE BLOOM BOOST (lane BE, 2026-09-02). The two masks the prefilter's
// one added multiply reads: RT23 (kGbufferEmissive, the engine's own emissive
// G-buffer, .bgsm/.pbrm glow) and the effect mask (this module's own target,
// written by re-issuing graded .bgem draws). BOUND ONLY when Params[1].z is 1,
// i.e. when either boost is above zero; at both boosts = 0 neither is bound and
// the branch below is not taken, so the prefilter is bit-identical to before.
Texture2D<float4> EmissiveMaterialMask : register(t1);
Texture2D<float4> EmissiveEffectMask : register(t2);
// RT24 (kGbufferMaterial): the material code rides .w (5 = skin, 84 = PBR).
// RT23 is DUAL-USE -- on code-84 pixels it holds the OpenPBR F0 and on code-5
// it is skin-composited, never emission (Fire.rdc 2026-09-02: the power armor's
// plates fill RT23 with grey F0). The gate below is the one base-combine and
// the SSGI radiance strip already apply when they read RT23 as emissive.
Texture2D<float4> EmissiveMaterialCode : register(t3);
// LANE SKY2 (2026-09-06). THE SKY GATE. RT23 and RT24 are G-BUFFER targets:
// the engine writes them only where geometry was rasterized this frame and
// never clears them over the sky, because every consumer it owns depth- or
// stencil-tests the sky away. This prefilter did not, so over the sky it read
// whatever the two targets last held -- the vault suit's gold trim and "111"
// from frames ago, wherever the camera had since moved them onto sky
// (SkylightingOn.rdc 17:41: 443,904 sky pixels boosted, 39,517 of them by more
// than 2x, mean boost 1.38, max 4.18, shaped exactly like the suit's brightest
// PBR parts). The scene depth is the only resource that says "this pixel was
// drawn this frame": depth == 1 is the cleared far plane, i.e. sky, and the
// boost is refused there. Params[1].w = 1 only when this SRV is bound; a host
// that cannot bind it prints `skyGate=absent` and keeps today's behaviour.
Texture2D<float> SceneDepth : register(t4);
SamplerState LinearClamp : register(s0);
RWTexture2D<float4> BloomTarget : register(u0);

// FP16 maximum. The bloom pyramid is forced to FP16 (PostChain.cpp:1285-1294),
// so this is the largest value a mip can carry.
static const float F4FS_BLOOM_MAX_FINITE = 65504.0;
// The sky's depth: the far plane the engine clears its 24-bit depth to. One
// 24-bit step below it, so that a cleared texel (exactly 1.0) is sky and the
// farthest drawn texel is not. Host twin: kBloomSkyDepth (BloomEmissiveMask.h).
static const float F4FS_BLOOM_SKY_DEPTH = 0.99999994;

float3 F4FS_SanitizeBloomSample(float3 color)
{
	// A single non-finite HDR texel can survive every downsample and then expand
	// into a large black rectangle through bilinear upsampling. Reject each
	// filtered sample before it enters the tent sum so NaNs cannot contaminate it.
	// Inspect IEEE-754 exponent bits because FXC may optimize isnan/isinf away
	// unless the whole shader is compiled with IEEE-strict mode.
	uint3 bits = asuint(color);
	uint3 exponent = bits & 0x7F800000u;
	uint3 mantissa = bits & 0x007FFFFFu;
	// NaN keeps the original hard reject: it has no magnitude to clamp toward,
	// and it is the black-rectangle bug this sanitizer was written for.
	bool nanX = exponent.x == 0x7F800000u && mantissa.x != 0u;
	bool nanY = exponent.y == 0x7F800000u && mantissa.y != 0u;
	bool nanZ = exponent.z == 0x7F800000u && mantissa.z != 0u;
	if (nanX || nanY || nanZ) {
		return 0.0.xxx;
	}
	// BLOOM FLICKER FIX F2 (2026-08-07, bloom-flicker-investigation.md M-D).
	// Infinity is now CLAMPED, not zeroed. Zeroing meant an overflowing
	// highlight lost its glow ENTIRELY on the frames it crossed the ceiling and
	// got it back on the frames it did not — a hard on/off pop on exactly the
	// brightest surfaces (sun glint, lamp cores), which is the opposite of what
	// a sanitizer should do. `sign(x) * min(abs(x), M)` is the identity on every
	// finite value, including signed zero, so nothing else moves.
	float3 bounded = sign(color) * min(abs(color), F4FS_BLOOM_MAX_FINITE.xxx);
	return clamp(bounded, 0.0.xxx, F4FS_BLOOM_MAX_FINITE.xxx);
}

float F4FS_BloomThresholdWeight(float luminance, float threshold, float softKnee)
{
	luminance = max(luminance, 0.0);
	if (luminance <= F4FS_LUM_EPSILON) {
		return 0.0;
	}

	threshold = max(threshold, 0.0);
	float knee = threshold * max(softKnee, 0.0);
	float hardContribution = luminance - threshold;
	float contribution = max(hardContribution, 0.0);

	if (knee > F4FS_LUM_EPSILON) {
		float softContribution = clamp(hardContribution + knee, 0.0, 2.0 * knee);
		float curvedContribution = (softContribution * softContribution) / (4.0 * knee);
		contribution = max(contribution, curvedContribution);
	}

	return saturate(contribution / luminance);
}

// BLOOM DOWNSAMPLE (13-tap). Ported from Open Shaders (open-shaders@4470b8f,
// features/Post Processing/Shaders/PostProcessing/common.hlsli: KarisAverage,
// DownsampleCOD, DownsampleCODFirstMip; GPL-3.0), after the Call of Duty:
// Advanced Warfare bloom (Jimenez, "Next Generation Post Processing"). Tap
// spacing here is in SOURCE texels for every level: four taps at +-1 and a 3x3
// grid at {-2, 0, +2}, so each output texel reads a 4x4-texel footprint and the
// weights sum to 1 (host twin: BloomDownsample13Weights, Bloom.h).
//
// The prefilter level weights each overlapping 2x2 group of taps by 1/(1 + luma)
// before averaging (Karis), which caps how much one hot texel can own of the
// output (BLOOM FLICKER FIX F1, bloom-flicker-investigation.md M-B). Coarser
// levels average plainly: they already hold averages of averages.
float3 F4FS_BloomTap(float2 uv, float2 offset, float2 uvMax)
{
	return F4FS_SanitizeBloomSample(SourceColor.SampleLevel(LinearClamp, min(uv + offset, uvMax), 0).rgb);
}

float3 F4FS_BloomGroupAverage(float3 a, float3 b, float3 c, float3 d, float karisAverage)
{
	float4 weights = lerp(1.0.xxxx,
		rcp(1.0 + float4(F4FS_Luminance(a), F4FS_Luminance(b), F4FS_Luminance(c), F4FS_Luminance(d))),
		saturate(karisAverage));
	return (a * weights.x + b * weights.y + c * weights.z + d * weights.w) / max(dot(weights, 1.0.xxxx), F4FS_LUM_EPSILON);
}

float3 F4FS_Downsample13(float2 uv, float2 texel, float karisAverage, float2 uvMax)
{
	float3 outer[9];
	[unroll]
	for (uint i = 0u; i < 9u; ++i) {
		outer[i] = F4FS_BloomTap(uv, texel * 2.0 * float2((float)(i % 3u) - 1.0, (float)(i / 3u) - 1.0), uvMax);
	}
	float3 center = F4FS_BloomGroupAverage(
		F4FS_BloomTap(uv, texel * float2(-1.0, -1.0), uvMax),
		F4FS_BloomTap(uv, texel * float2(1.0, -1.0), uvMax),
		F4FS_BloomTap(uv, texel * float2(-1.0, 1.0), uvMax),
		F4FS_BloomTap(uv, texel * float2(1.0, 1.0), uvMax),
		karisAverage);
	return 0.5 * center + 0.125 * (
		F4FS_BloomGroupAverage(outer[0], outer[1], outer[3], outer[4], karisAverage) +
		F4FS_BloomGroupAverage(outer[1], outer[2], outer[4], outer[5], karisAverage) +
		F4FS_BloomGroupAverage(outer[3], outer[4], outer[6], outer[7], karisAverage) +
		F4FS_BloomGroupAverage(outer[4], outer[5], outer[7], outer[8], karisAverage));
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	uint targetWidth;
	uint targetHeight;
	BloomTarget.GetDimensions(targetWidth, targetHeight);
	uint2 targetSize = max(uint2(targetWidth, targetHeight), uint2(1, 1));
	if (any(dispatchThreadId.xy >= targetSize)) {
		return;
	}

	uint sourceWidth;
	uint sourceHeight;
	SourceColor.GetDimensions(sourceWidth, sourceHeight);
	uint2 sourceSize = max(uint2(sourceWidth, sourceHeight), uint2(1, 1));
	float2 uv = ((float2)dispatchThreadId.xy + 0.5) / (float2)targetSize;
	float2 sourceTexel = 1.0 / (float2)sourceSize;

	// Params[0].z is the PREFILTER flag (Bloom.cpp:637, `a_level == 0`). It
	// already gates the threshold; F1 gates the Karis average on the same flag,
	// so the extraction level is the only one whose weighting changes.
	float prefilter = saturate(Params[0].z);
	// RENDER SUBRECT MAPPING. Params[3].xy is the render subrect as a fraction
	// of the source allocation, uploaded on the prefilter only (Bloom.cpp).
	// Under an upscaler the scene fills only that top-left subrect of the
	// display-sized source. The pyramid keeps its display-sized plan and maps
	// its texels over the subrect: mip 0 then spans the scene exactly as it
	// spans a native frame, so the glow keeps its screen size at every render
	// scale and no mip holds the stale border. The 13 taps step half a mip-0
	// texel, mapped into source uv, which at native resolution is one source
	// texel. A zero row (coarser levels) and (1, 1) are the native mapping, op
	// for op.
	bool subrectBound = prefilter > 0.0 && Params[3].x > 0.0 && Params[3].y > 0.0;
	float2 subrect = subrectBound ? min(Params[3].xy, 1.0.xx) : 1.0.xx;
	bool mapped = any(subrect < 1.0);
	float2 tapTexel = mapped ? 0.5 * subrect / (float2)targetSize : sourceTexel;
	if (mapped) {
		uv *= subrect;
	}

	// SOURCE DE-JITTER (lane BLOOMSHIM, 2026-09-03). Params[2].xy is the UV
	// offset that puts this texel back where the scene would be WITHOUT the
	// engine's TAA jitter; .z is 1 only when the resolver served. The prefilter
	// reads a copy of kMainTemp taken ~460 events before the engine's TAA
	// resolve (Fire.rdc: bloom at EID 140726, the resolve at 141187), so its
	// content walks an 8-phase Halton sub-pixel cycle under a motionless
	// camera, and the extraction is NOT invariant to where its 2x2 grid lands:
	// one whole source texel of grid shift moves the extracted energy 7.4%
	// peak-to-peak around a fire at the live threshold. Anchoring the sample
	// to the scene removes that driver without touching the extraction.
	//
	// Params[2] was an all-zero row on every level before this key existed, and
	// the runtime still uploads exact zeros whenever it refuses, so this line
	// is `uv + 0.0` -- the identity on every finite uv -- at the shipped value.
	// The masks below take the SAME offset: RT23/RT24 are rasterized with the
	// same jittered projection as the colour, and un-jittering one without the
	// other would read a neighbour's material code. The C++ resolver refuses
	// anything beyond half a source texel, and half a source texel is smaller
	// than the smallest uv this shader ever forms (0.5 / targetSize), so the
	// offset uv stays strictly positive and the Load below cannot underflow.
	uv += Params[2].xy;

	// RENDER SUBRECT CLAMP. Everything right of / below the subrect is stale
	// full-size content (the last loading screen); clamping every tap to the
	// subrect's last texel centre repeats the scene's edge there instead,
	// exactly what LinearClamp does at a native screen edge. The lower bound
	// keeps a de-jittered uv that the mapping made smaller than half a source
	// texel (Ultra Performance) from turning the masks' Load index negative.
	float2 uvMax = subrectBound ? Params[3].xy - 0.5 * sourceTexel : 1.0.xx;
	uv = clamp(uv, 0.0.xx, uvMax);
	float3 color = F4FS_Downsample13(uv, tapTexel, prefilter, uvMax);
	// EMISSIVE BLOOM BOOST: input = frameColour * (1 + materialBoost * maskMaterial
	// + effectBoost * maskEffect), BEFORE the threshold and knee, on the
	// prefilter level only. A scalar on the frame's own colour, so tint and
	// shading are preserved; the surface itself is never changed. Params[1].x =
	// fBloomEmissiveBoost, .y = fBloomEffectBoost, .z = 1 when either is above
	// zero (the only time t1/t2 are bound). The material mask is RT23's
	// luminance (0..1); the effect mask accumulates additively and is unbounded.
	[branch]
	if (Params[1].z > 0.0 && prefilter > 0.0) {
		// Material code at the full-resolution pixel under this half-resolution
		// texel: a code is a label and must be LOADED, never filtered.
		uint codeWidth;
		uint codeHeight;
		EmissiveMaterialCode.GetDimensions(codeWidth, codeHeight);
		uint2 codeSize = max(uint2(codeWidth, codeHeight), uint2(1, 1));
		uint2 codePixel = min(uint2(uv * (float2)codeSize), codeSize - uint2(1, 1));
		float matCode = EmissiveMaterialCode.Load(int3(codePixel, 0)).w * 255.0;
		// The native Pip-Boy screen writes code 4 and already carries its CRT glow.
		// Boosting that emission again washes out the UI's text and scan lines.
		// 86 is F4FS_BGSM22_PBR_CODE: like 84, RT23 holds F0 there, never emission.
		bool legacyEmissive = abs(matCode - 84.0) >= 0.25 && abs(matCode - 86.0) >= 0.25 &&
			abs(matCode - 5.0) >= 0.25 &&
			abs(matCode - 4.0) >= 0.25;
		// THE SKY GATE (lane SKY2): a pixel the G-buffer did not write this
		// frame has no material to boost, whatever RT23 still holds there.
		// Same full-resolution pixel as the code, LOADed like the code. The
		// host mirror is BloomPrefilterMaterialMaskAdmits (BloomEmissiveMask.h).
		bool drawnThisFrame = Params[1].w > 0.0
			? SceneDepth.Load(int3(codePixel, 0)) < F4FS_BLOOM_SKY_DEPTH
			: true;
		float3 maskWeights = float3(0.2126, 0.7152, 0.0722);
		float maskMaterial = (legacyEmissive && drawnThisFrame) ?
			max(dot(EmissiveMaterialMask.SampleLevel(LinearClamp, uv, 0).rgb, maskWeights), 0.0) : 0.0;
		float maskEffect = max(dot(EmissiveEffectMask.SampleLevel(LinearClamp, uv, 0).rgb, maskWeights), 0.0);
		color *= 1.0 + Params[1].x * maskMaterial + Params[1].y * maskEffect;
	}
	float luminance = F4FS_Luminance(color);
	float thresholdWeight = F4FS_BloomThresholdWeight(luminance, Params[0].x, Params[0].y);
	float extractWeight = lerp(1.0, thresholdWeight, prefilter);
	float prefilterScale = max(Params[0].w, 0.0);

	BloomTarget[dispatchThreadId.xy] = float4(color * extractWeight * prefilterScale, 1.0);
}
