// THE PATH-TO-WHITE DIAL rides in a fifth constant vector (Params[4].x), so
// this pass declares the longer block. The replay variant used for the
// capture proof defines F4FX_TONEMAP_PATH_TO_WHITE_OVERRIDE to a literal
// instead: it then reads no fifth vector and keeps the four-vector block the
// capture was recorded with, so the null control compares like with like.
#ifndef F4FX_TONEMAP_PATH_TO_WHITE_OVERRIDE
#define F4FX_POST_PARAMS_COUNT 5
#endif
#include "Common.hlsli"
#include "../HDR/DisplayMap.hlsli"

static const uint F4FX_TONEMAP_OUTPUT_SDR = 0;
static const uint F4FX_TONEMAP_OUTPUT_SCRGB_DISPLAY_MAP = 1;
static const uint F4FX_TONEMAP_CURVE_PHYSICAL = 3;

Texture2D<float4> SourceColor : register(t0);
RWTexture2D<float4> TargetColor : register(u0);

// Adapted from Polyphony Digital's MIT-licensed GT7 Tone Mapping sample.
// The full upstream notice and source URL are recorded in THIRD_PARTY.md.
float F4FX_PhysicalCurve(float x, float peak, float4 curveParams)
{
	const float alpha = curveParams.x;
	const float midPoint = curveParams.y;
	const float linearSection = curveParams.z;
	const float toeStrength = curveParams.w;
	const float k = (linearSection - 1.0f) / (alpha - 1.0f);
	const float kA = peak * (linearSection + k);
	const float kB = -peak * k * exp(linearSection / k);
	const float kC = -rcp(k * peak);
	x = max(x, 0.0f);
	if (x < linearSection * peak) {
		const float linearWeight = smoothstep(0.0f, midPoint, x);
		const float toe = midPoint * pow(max(x / midPoint, 0.0f), toeStrength);
		return lerp(toe, x, linearWeight);
	}
	return kA + kB * exp(x * kC);
}

float F4FX_PhysicalInversePq(float value, float referenceNits)
{
	const float m1 = 0.1593017578125f;
	const float m2 = 78.84375f;
	const float c1 = 0.8359375f;
	const float c2 = 18.8515625f;
	const float c3 = 18.6875f;
	float y = max(value, 0.0f) * referenceNits / 10000.0f;
	float ym = pow(abs(y), m1);
	return exp2(m2 * (log2(c1 + c2 * ym) - log2(1.0f + c3 * ym)));
}

float F4FX_PhysicalPqEotf(float value, float referenceNits)
{
	const float m1 = 0.1593017578125f;
	const float m2 = 78.84375f;
	const float c1 = 0.8359375f;
	const float c2 = 18.8515625f;
	const float c3 = 18.6875f;
	float np = pow(saturate(value), rcp(m2));
	float l = pow(max(np - c1, 0.0f) / max(c2 - c3 * np, F4FX_LUM_EPSILON), rcp(m1));
	return l * 10000.0f / referenceNits;
}

float3 F4FX_PhysicalRgbToICtCp(float3 rgb, float referenceNits)
{
	float l = dot(rgb, float3(1688.0f, 2146.0f, 262.0f)) / 4096.0f;
	float m = dot(rgb, float3(683.0f, 2951.0f, 462.0f)) / 4096.0f;
	float s = dot(rgb, float3(99.0f, 309.0f, 3688.0f)) / 4096.0f;
	float3 lmsPq = float3(
		F4FX_PhysicalInversePq(l, referenceNits),
		F4FX_PhysicalInversePq(m, referenceNits),
		F4FX_PhysicalInversePq(s, referenceNits));
	return float3(
		0.5f * (lmsPq.x + lmsPq.y),
		dot(lmsPq, float3(6610.0f, -13613.0f, 7003.0f)) / 4096.0f,
		dot(lmsPq, float3(17933.0f, -17390.0f, -543.0f)) / 4096.0f);
}

float3 F4FX_PhysicalICtCpToRgb(float3 ictcp, float referenceNits)
{
	float3 lmsPq = float3(
		ictcp.x + 0.00860904f * ictcp.y + 0.11103f * ictcp.z,
		ictcp.x - 0.00860904f * ictcp.y - 0.11103f * ictcp.z,
		ictcp.x + 0.560031f * ictcp.y - 0.320627f * ictcp.z);
	float3 lms = float3(
		F4FX_PhysicalPqEotf(lmsPq.x, referenceNits),
		F4FX_PhysicalPqEotf(lmsPq.y, referenceNits),
		F4FX_PhysicalPqEotf(lmsPq.z, referenceNits));
	return max(float3(
		dot(lms, float3(3.43661f, -2.50645f, 0.0698454f)),
		dot(lms, float3(-0.79133f, 1.9836f, -0.192271f)),
		dot(lms, float3(-0.0259499f, -0.0989137f, 1.12486f))), 0.0f);
}

// THE SDR HIGHLIGHT SHOULDER (2026-09-02).
//
// The physical display mapping below is display = min(curve(x), peak) / peak,
// so it reaches white where the curve first equals peak. Every factor of peak
// cancels out of that equation, leaving a scene-referred clip point that is a
// pure function of the curve's alpha and linear-section constants:
//     k = (L - 1) / (a - 1),  x_clip = -k * ln( (L + k - 1) / (k * exp(L / k)) )
// With the shipped curve that is 1.471760 -- 0.5573 stops above paper white.
// Anything brighter is flat white, which is why the moon is a disc, the fire a
// core and the sight a blob. Returns 0 when no finite clip point exists.
float F4FX_PhysicalSdrClipScene(float4 curveParams)
{
	const float alpha = curveParams.x;
	const float linearSection = curveParams.z;
	const float k = (linearSection - 1.0f) / (alpha - 1.0f);
	const float denominator = k * exp(linearSection / k);
	if (!(denominator > 0.0f)) {
		return 0.0f;
	}
	const float q = (linearSection + k - 1.0f) / denominator;
	if (!(q > 0.0f) || !(q < 1.0f)) {
		return 0.0f;
	}
	return -k * log(q);
}

// Compress scene highlights above paper white into that headroom. Identity at
// and below white (mid-tones are byte-identical), monotonic, and it approaches
// the clip point only asymptotically, so nothing lands on it. The compression
// variable is the MAX CHANNEL, not the luminance: a luminance rolloff leaves a
// saturated highlight's own channel above the clip point and it still clips.
// Scaling the triple by one factor leaves the ratios -- the hue -- untouched.
// shoulderStops <= 0 returns the input, which is the bit-exact revert.
float3 F4FX_PhysicalSdrShoulder(float3 color709, float4 curveParams, float shoulderStops)
{
	if (shoulderStops <= 0.0f) {
		return color709;
	}
	const float headroom = F4FX_PhysicalSdrClipScene(curveParams) - 1.0f;
	if (!(headroom > 0.0f)) {
		return color709;
	}
	const float m = max(max(color709.r, color709.g), color709.b);
	if (!(m > 1.0f)) {
		return color709;
	}
	// Never steeper than 1:1 at white, and otherwise placed so that
	// shoulderStops above white lands on 95% of the headroom.
	const float reach = (exp2(shoulderStops) - 1.0f) * (0.05f / 0.95f);
	const float scale = max(headroom, reach);
	const float t = (m - 1.0f) / scale;
	const float mapped = 1.0f + headroom * (t / (1.0f + t));
	return color709 * (mapped / m);
}

// THE PATH TO WHITE (2026-09-02).
//
// The shoulder above keeps a saturated highlight's hue at every magnitude, and
// the display mapping then keeps that hue too: a fire core ten times brighter
// than paper white comes out as an orange whose red channel sits at peak and
// whose green and blue sit far below it -- a DIM saturated orange beside a
// white sky, when it should be the brightest thing in the frame. No SDR
// display can show "orange, brighter than white"; the honest rendering of an
// overbright colour is the one every per-channel curve produces by accident:
// it goes to white, and the brighter it is the whiter it goes.
//
// This does that on purpose, BEFORE the shoulder, driven by the scene
// luminance in multiples of paper white (the exposed, pre-shoulder value -- not
// the post-shoulder normalized luminance the ICtCp fade below reads, which the
// shoulder has already squeezed into the top few percent of the range, so
// that fade never engages on a saturated colour). Identity at and below white:
// mid-tones are byte-identical and no colour loses saturation it would have
// had. Above white the colour slides along a straight line toward its own
// luminance grey,
//
//     fade = smoothstep(0, stops, log2(luminance))
//     rgb  = lerp(rgb, luminance.xxx, fade)
//
// so luminance is preserved exactly, the chromaticity moves straight toward
// the white point (dominant wavelength unchanged), and the largest channel can
// only fall -- the shoulder afterwards has less to compress and the sun's
// clipped count cannot rise. At the default 2.5 stops a colour 1.3x white
// keeps 94% of its chroma, 2x keeps 65%, 4x keeps 10%, and 5.66x and beyond
// is white. stops <= 0 returns the input, which is the bit-exact revert.
float3 F4FX_PhysicalPathToWhite(float3 color709, float stops)
{
	if (stops <= 0.0f) {
		return color709;
	}
	const float luminance = dot(color709, float3(0.2126f, 0.7152f, 0.0722f));
	if (!(luminance > 1.0f)) {
		return color709;
	}
	const float fade = smoothstep(0.0f, stops, log2(luminance));
	return lerp(color709, luminance.xxx, fade);
}

float3 F4FX_TonemapPhysical(float3 color709, float4 outputParams, float4 curveParams, float4 chromaParams, float pathToWhiteStops, uint outputMode)
{
	const float targetNits = max(outputParams.x, 250.0f);
	const float peak = targetNits / max(chromaParams.z, 1.0f);
	float3 scene709 = max(color709, 0.0f);
	if (outputMode == F4FX_TONEMAP_OUTPUT_SDR) {
		// SDR ONLY. A real HDR transport (scRGB/HDR10 with a peak far above
		// paper white) already has the headroom this shoulder manufactures, so
		// its display mapping is left exactly as it was. The path to white runs
		// first: it reads the scene luminance the shoulder is about to compress.
		scene709 = F4FX_PhysicalPathToWhite(scene709, pathToWhiteStops);
		scene709 = F4FX_PhysicalSdrShoulder(scene709, curveParams, chromaParams.w);
	}
	float3 rgb = max(F4FX_Rec709ToRec2020(scene709), 0.0f);
	if (outputMode == F4FX_TONEMAP_OUTPUT_SDR) {
		// FO4's SDR scene buffer is reference-white relative: 1.0 is the
		// selected paper white, not a fixed 100-nit absolute sample. Convert it
		// into the curve's absolute 100-nit units before the reciprocal output
		// normalization below. Omitting this scale divided all normal scene
		// values by 2.5 at the default 250-nit target and pushed most shadows
		// into the toe even though the clipping diagnostic remained empty.
		rgb *= peak;
	}
	float3 originalUcs = F4FX_PhysicalRgbToICtCp(rgb, chromaParams.z);
	float3 skewedRgb = float3(
		F4FX_PhysicalCurve(rgb.r, peak, curveParams),
		F4FX_PhysicalCurve(rgb.g, peak, curveParams),
		F4FX_PhysicalCurve(rgb.b, peak, curveParams));
	float3 skewedUcs = F4FX_PhysicalRgbToICtCp(skewedRgb, chromaParams.z);
	float targetUcs = F4FX_PhysicalRgbToICtCp(peak.xxx, chromaParams.z).x;
	float normalizedLuminance = originalUcs.x / max(targetUcs, F4FX_LUM_EPSILON);
	float chromaScale = 1.0f - smoothstep(chromaParams.x, chromaParams.y, normalizedLuminance);
	float3 scaledRgb = F4FX_PhysicalICtCpToRgb(
		float3(skewedUcs.x, originalUcs.yz * chromaScale), chromaParams.z);
	float3 mapped2020 = min(lerp(skewedRgb, scaledRgb, saturate(outputParams.y)), peak.xxx) * outputParams.z;
	float3 mapped709 = max(F4FX_Rec2020ToRec709(mapped2020), 0.0f);
	if (outputMode == F4FX_TONEMAP_OUTPUT_SCRGB_DISPLAY_MAP) {
		// Physical framebuffer 1.0 is 100 nits; scRGB 1.0 is 80 nits.
		return mapped709 * (chromaParams.z / F4FX_SCRGB_REFERENCE_WHITE_NITS);
	}
	return saturate(mapped709);
}

float F4FX_HablePartial(float x, float4 hable0, float4 hable1)
{
	const float A = hable0.x;
	const float B = max(hable0.y, F4FX_LUM_EPSILON);
	const float C = hable0.z;
	const float D = hable0.w;
	const float E = hable1.x;
	const float F = max(hable1.y, F4FX_LUM_EPSILON);
	x = max(x, 0.0);
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - (E / F);
}

float3 F4FX_TonemapHable(float3 color, float whitePoint, float4 hable0, float4 hable1)
{
	float whiteScale = rcp(max(F4FX_HablePartial(max(whitePoint, F4FX_LUM_EPSILON), hable0, hable1), F4FX_LUM_EPSILON));
	return saturate(float3(
		F4FX_HablePartial(color.r, hable0, hable1) * whiteScale,
		F4FX_HablePartial(color.g, hable0, hable1) * whiteScale,
		F4FX_HablePartial(color.b, hable0, hable1) * whiteScale));
}

float F4FX_TonemapLottesChannel(float x)
{
	const float a = 1.6;
	const float d = 0.977;
	const float hdrMax = 8.0;
	const float midIn = 0.18;
	const float midOut = 0.267;
	x = max(x, 0.0);

	float hdrMaxA = pow(hdrMax, a);
	float hdrMaxAD = pow(hdrMax, a * d);
	float midInA = pow(midIn, a);
	float midInAD = pow(midIn, a * d);
	float denominator = (hdrMaxAD - midInAD) * midOut;
	float b = (-midInA + hdrMaxA * midOut) / denominator;
	float c = (hdrMaxAD * midInA - hdrMaxA * midInAD * midOut) / denominator;
	return saturate(pow(x, a) / (pow(x, a * d) * b + c));
}

float3 F4FX_TonemapLottes(float3 color)
{
	return float3(
		F4FX_TonemapLottesChannel(color.r),
		F4FX_TonemapLottesChannel(color.g),
		F4FX_TonemapLottesChannel(color.b));
}

float3 F4FX_TonemapAcesFitted(float3 color)
{
	color = max(color, 0.0);
	return saturate((color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14));
}

float3 F4FX_ApplyTonemap(float3 color, uint curve, float whitePoint, float4 hable0, float4 hable1)
{
	if (curve == 0) {
		return F4FX_TonemapHable(color, whitePoint, hable0, hable1);
	}
	if (curve == 1) {
		return F4FX_TonemapLottes(color);
	}
	return F4FX_TonemapAcesFitted(color);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	uint targetWidth;
	uint targetHeight;
	TargetColor.GetDimensions(targetWidth, targetHeight);
	uint2 targetSize = max(uint2(targetWidth, targetHeight), uint2(1, 1));
	if (any(dispatchThreadId.xy >= targetSize)) {
		return;
	}

	uint sourceWidth;
	uint sourceHeight;
	SourceColor.GetDimensions(sourceWidth, sourceHeight);
	uint2 sourceSize = max(uint2(sourceWidth, sourceHeight), uint2(1, 1));
	uint2 sourceCoord = min(dispatchThreadId.xy, sourceSize - 1);

	float4 source = SourceColor.Load(int3(sourceCoord, 0));
	float exposureEv = Params[0].y;
	float whitePoint = max(Params[0].z, F4FX_LUM_EPSILON);
	uint outputMode = (uint)Params[0].w;
	float paperWhiteNits = Params[1].x;
	float4 hable0 = Params[2];
	float4 hable1 = Params[3];
	float3 exposed = max(source.rgb, 0.0) * exp2(exposureEv);
	uint curve = (uint)Params[0].x;
	float3 mapped = 0.0f;
	if (curve == F4FX_TONEMAP_CURVE_PHYSICAL) {
#ifdef F4FX_TONEMAP_PATH_TO_WHITE_OVERRIDE
		const float pathToWhiteStops = F4FX_TONEMAP_PATH_TO_WHITE_OVERRIDE;
#else
		const float pathToWhiteStops = Params[4].x;
#endif
		mapped = F4FX_TonemapPhysical(exposed, Params[1].yzwx, Params[2], Params[3], pathToWhiteStops, outputMode);
	} else {
		// Preserve the established HDR behavior for legacy display curves; only
		// the physical operator performs target-aware HDR highlight compression.
		mapped = outputMode == F4FX_TONEMAP_OUTPUT_SCRGB_DISPLAY_MAP ?
			F4FX_DisplayMappedLinearToScRgb(exposed, paperWhiteNits) :
			F4FX_ApplyTonemap(exposed, curve, whitePoint, hable0, hable1);
	}
	TargetColor[dispatchThreadId.xy] = float4(mapped, source.a);
}
