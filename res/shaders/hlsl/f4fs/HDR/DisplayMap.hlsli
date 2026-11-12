#ifndef F4FX_HDR_DISPLAY_MAP_HLSLI
#define F4FX_HDR_DISPLAY_MAP_HLSLI

static const float F4FX_SCRGB_REFERENCE_WHITE_NITS = 80.0f;
static const float F4FX_HDR10_REFERENCE_WHITE_NITS = 10000.0f;

float F4FX_NitsToScRgb(float nits)
{
	return max(nits, 0.0f) / F4FX_SCRGB_REFERENCE_WHITE_NITS;
}

float3 F4FX_SceneLinearToDisplayNits(float3 color, float paperWhiteNits, float peakNits)
{
	float safePaperWhite = max(paperWhiteNits, 0.0f);
	float safePeak = max(peakNits, 0.0f);
	float maxNits = safePeak > 0.0f ? safePeak : safePaperWhite;
	return min(max(color, 0.0f) * safePaperWhite, maxNits);
}

float3 F4FX_DisplayMappedLinearToScRgb(float3 color, float paperWhiteNits)
{
	return max(color, 0.0f) * F4FX_NitsToScRgb(paperWhiteNits);
}

float3 F4FX_Rec709ToRec2020(float3 c)
{
	return float3(
		0.6274040f * c.r + 0.3292820f * c.g + 0.0433136f * c.b,
		0.0690970f * c.r + 0.9195400f * c.g + 0.0113612f * c.b,
		0.0163916f * c.r + 0.0880132f * c.g + 0.8955950f * c.b);
}

float3 F4FX_Rec2020ToRec709(float3 c)
{
	return float3(
		1.6604910f * c.r - 0.5876411f * c.g - 0.0728499f * c.b,
		-0.1245505f * c.r + 1.1328999f * c.g - 0.0083494f * c.b,
		-0.0181508f * c.r - 0.1005789f * c.g + 1.1187297f * c.b);
}

float3 F4FX_LinearNitsToHdr10Pq(float3 nits)
{
	static const float m1 = 2610.0f / 16384.0f;
	static const float m2 = 2523.0f / 32.0f;
	static const float c1 = 3424.0f / 4096.0f;
	static const float c2 = 2413.0f / 128.0f;
	static const float c3 = 2392.0f / 128.0f;

	float3 normalized = saturate(max(nits, 0.0f) / F4FX_HDR10_REFERENCE_WHITE_NITS);
	float3 p = pow(normalized, m1);
	return pow((c1 + c2 * p) / (1.0f + c3 * p), m2);
}

float3 F4FX_DisplayMappedLinearToHdr10Pq(float3 color, float paperWhiteNits, float peakNits)
{
	float3 displayNits709 = F4FX_SceneLinearToDisplayNits(color, paperWhiteNits, peakNits);
	float3 displayNits2020 = max(F4FX_Rec709ToRec2020(displayNits709), 0.0f);
	return F4FX_LinearNitsToHdr10Pq(displayNits2020);
}

float3 F4FX_NativeUiLinearToDisplayNits(float3 color, float uiBrightnessNits)
{
	return saturate(color) * max(uiBrightnessNits, 0.0f);
}

float3 F4FX_NativeUiLinearToScRgb(float3 color, float uiBrightnessNits)
{
	return saturate(color) * F4FX_NitsToScRgb(uiBrightnessNits);
}

float3 F4FX_CompositeNativeUiDisplayNits(float3 sceneDisplayNits, float3 uiLinearColor, float uiAlpha, float uiBrightnessNits)
{
	return lerp(sceneDisplayNits, F4FX_NativeUiLinearToDisplayNits(uiLinearColor, uiBrightnessNits), saturate(uiAlpha));
}

float3 F4FX_CompositeNativeUiScRgb(float3 sceneScRgb, float3 uiLinearColor, float uiAlpha, float uiBrightnessNits)
{
	return lerp(sceneScRgb, F4FX_NativeUiLinearToScRgb(uiLinearColor, uiBrightnessNits), saturate(uiAlpha));
}

#endif
