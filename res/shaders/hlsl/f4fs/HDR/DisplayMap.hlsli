#ifndef F4FS_HDR_DISPLAY_MAP_HLSLI
#define F4FS_HDR_DISPLAY_MAP_HLSLI

static const float F4FS_SCRGB_REFERENCE_WHITE_NITS = 80.0f;
static const float F4FS_HDR10_REFERENCE_WHITE_NITS = 10000.0f;

float F4FS_NitsToScRgb(float nits)
{
	return max(nits, 0.0f) / F4FS_SCRGB_REFERENCE_WHITE_NITS;
}

float3 F4FS_SceneLinearToDisplayNits(float3 color, float paperWhiteNits, float peakNits)
{
	float safePaperWhite = max(paperWhiteNits, 0.0f);
	float safePeak = max(peakNits, 0.0f);
	float maxNits = safePeak > 0.0f ? safePeak : safePaperWhite;
	return min(max(color, 0.0f) * safePaperWhite, maxNits);
}

float3 F4FS_DisplayMappedLinearToScRgb(float3 color, float paperWhiteNits)
{
	return max(color, 0.0f) * F4FS_NitsToScRgb(paperWhiteNits);
}

float3 F4FS_Rec709ToRec2020(float3 c)
{
	return float3(
		0.6274040f * c.r + 0.3292820f * c.g + 0.0433136f * c.b,
		0.0690970f * c.r + 0.9195400f * c.g + 0.0113612f * c.b,
		0.0163916f * c.r + 0.0880132f * c.g + 0.8955950f * c.b);
}

float3 F4FS_Rec2020ToRec709(float3 c)
{
	return float3(
		1.6604910f * c.r - 0.5876411f * c.g - 0.0728499f * c.b,
		-0.1245505f * c.r + 1.1328999f * c.g - 0.0083494f * c.b,
		-0.0181508f * c.r - 0.1005789f * c.g + 1.1187297f * c.b);
}

float3 F4FS_LinearNitsToHdr10Pq(float3 nits)
{
	static const float m1 = 2610.0f / 16384.0f;
	static const float m2 = 2523.0f / 32.0f;
	static const float c1 = 3424.0f / 4096.0f;
	static const float c2 = 2413.0f / 128.0f;
	static const float c3 = 2392.0f / 128.0f;

	float3 normalized = saturate(max(nits, 0.0f) / F4FS_HDR10_REFERENCE_WHITE_NITS);
	float3 p = pow(normalized, m1);
	return pow((c1 + c2 * p) / (1.0f + c3 * p), m2);
}

float3 F4FS_DisplayMappedLinearToHdr10Pq(float3 color, float paperWhiteNits, float peakNits)
{
	float3 displayNits709 = F4FS_SceneLinearToDisplayNits(color, paperWhiteNits, peakNits);
	float3 displayNits2020 = max(F4FS_Rec709ToRec2020(displayNits709), 0.0f);
	return F4FS_LinearNitsToHdr10Pq(displayNits2020);
}

float3 F4FS_NativeUiLinearToDisplayNits(float3 color, float uiBrightnessNits)
{
	return saturate(color) * max(uiBrightnessNits, 0.0f);
}

float3 F4FS_NativeUiLinearToScRgb(float3 color, float uiBrightnessNits)
{
	return saturate(color) * F4FS_NitsToScRgb(uiBrightnessNits);
}

// The sRGB transfer pair, continued past 1 along its power segment so scene values
// brighter than HUD white survive the round trip.
float3 F4FS_SrgbEncodeExtended(float3 c)
{
	c = max(c, 0.0f);
	return lerp(c * 12.92f, 1.055f * pow(c, 1.0f / 2.4f) - 0.055f, step(0.0031308f, c));
}

float3 F4FS_SrgbDecodeExtended(float3 c)
{
	c = max(c, 0.0f);
	return lerp(c / 12.92f, pow((c + 0.055f) / 1.055f, 2.4f), step(0.04045f, c));
}

// HUD over scene, blended the way the HUD was authored: in sRGB gamma, relative to
// HUD white. A linear-light blend darkens and haloes anti-aliased HUD edges against
// the SDR look. Endpoints are exact: alpha 0 is the scene, alpha 1 the HUD.
float3 F4FS_CompositeNativeUiDisplayNits(float3 sceneDisplayNits, float3 uiLinearColor, float uiAlpha, float uiBrightnessNits)
{
	const float uiWhite = max(uiBrightnessNits, 1.0e-4f);
	const float3 sceneGamma = F4FS_SrgbEncodeExtended(sceneDisplayNits / uiWhite);
	const float3 uiGamma = F4FS_SrgbEncodeExtended(saturate(uiLinearColor));
	return F4FS_SrgbDecodeExtended(lerp(sceneGamma, uiGamma, saturate(uiAlpha))) * uiWhite;
}

float3 F4FS_CompositeNativeUiScRgb(float3 sceneScRgb, float3 uiLinearColor, float uiAlpha, float uiBrightnessNits)
{
	return F4FS_CompositeNativeUiDisplayNits(
		max(sceneScRgb, 0.0f) * F4FS_SCRGB_REFERENCE_WHITE_NITS, uiLinearColor, uiAlpha, uiBrightnessNits) /
		F4FS_SCRGB_REFERENCE_WHITE_NITS;
}

// PumboAutoHDR (https://github.com/Filoppi/PumboAutoHDR, as Open Shaders and
// Community Shaders use it): expands an SDR colour's highlights toward the display
// peak. Linear Rec.709 in, multiples of paper white out; black and mid-tones stay
// almost where they were, SDR white reaches peak / paper white. Host mirror:
// f4fs::hdr::PumboAutoHdr (src/HDR/HdrPipeline.cpp).
float3 F4FS_PumboAutoHdr(float3 sdrColor, float peakNits, float paperWhiteNits, float shoulderPow, float saturationExpansion)
{
	sdrColor = max(sdrColor, 0.0f);
	const float maxWhite = max(peakNits / max(paperWhiteNits, 1.0e-4f), 1.0f);
	const float ratio = (sdrColor.r + sdrColor.g + sdrColor.b) / 3.0f;
	const float total = ratio + pow(saturate(ratio), shoulderPow) * (maxWhite - 1.0f);
	const float singleScale = ratio > 0.0f ? total / ratio : 1.0f;
	const float3 total3 = sdrColor + pow(saturate(sdrColor), shoulderPow) * (maxWhite - 1.0f);
	const float3 perChannelScale = float3(
		sdrColor.r > 0.0f ? total3.r / sdrColor.r : 1.0f,
		sdrColor.g > 0.0f ? total3.g / sdrColor.g : 1.0f,
		sdrColor.b > 0.0f ? total3.b / sdrColor.b : 1.0f);
	return sdrColor * lerp(singleScale.xxx, perChannelScale, saturate(saturationExpansion));
}

#endif
