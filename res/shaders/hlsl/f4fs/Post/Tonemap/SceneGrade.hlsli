// Scene-referred grading before the tone curve, after open-shaders' Color Grading
// (https://github.com/alandtse/open-shaders at 4470b8f: features/Post Processing/Shaders/
// PostProcessing/ColorGrading/colorgrading.hlsl, Include/Highlights.hlsli and
// PostProcessing/common.hlsli; GPL-3.0-or-later with the Modding and Linking exceptions).
// THE F4FS FORM (2026-10-22): its HDR half only (white balance, OKLCH, ASC CDL,
// shadows/midtones/highlights, contrast), in linear Rec.709 scene light. No log
// grading and no working-space choice; the white-balance matrix is solved on the host
// (f4fs::post::SceneGradeWhiteBalance). Host mirror: f4fs::post::ApplySceneGrade.
#ifndef F4FS_POST_TONEMAP_SCENE_GRADE_HLSLI
#define F4FS_POST_TONEMAP_SCENE_GRADE_HLSLI

// Params[F4FS_SG_BASE..] = kSceneGradeFirstVector (src/Post/Tonemap.h).
#define F4FS_SG_BASE 20
#define F4FS_SG(i) Params[F4FS_SG_BASE + (i) / 4][(i) % 4]
#define F4FS_SG_TEMPERATURE 0
#define F4FS_SG_TINT 1
#define F4FS_SG_SATURATION 2
#define F4FS_SG_VIBRANCE 3
#define F4FS_SG_HUE_SHIFT 4
#define F4FS_SG_MIXER_REDS_HUE_SHIFT 5
#define F4FS_SG_MIXER_REDS_VIBRANCE 6
#define F4FS_SG_MIXER_REDS_BRIGHTNESS 7
#define F4FS_SG_MIXER_ORANGES_HUE_SHIFT 8
#define F4FS_SG_MIXER_ORANGES_VIBRANCE 9
#define F4FS_SG_MIXER_ORANGES_BRIGHTNESS 10
#define F4FS_SG_MIXER_GREENS_HUE_SHIFT 11
#define F4FS_SG_MIXER_GREENS_VIBRANCE 12
#define F4FS_SG_MIXER_GREENS_BRIGHTNESS 13
#define F4FS_SG_MIXER_CYANS_HUE_SHIFT 14
#define F4FS_SG_MIXER_CYANS_VIBRANCE 15
#define F4FS_SG_MIXER_CYANS_BRIGHTNESS 16
#define F4FS_SG_MIXER_BLUES_HUE_SHIFT 17
#define F4FS_SG_MIXER_BLUES_VIBRANCE 18
#define F4FS_SG_MIXER_BLUES_BRIGHTNESS 19
#define F4FS_SG_MIXER_PURPLES_HUE_SHIFT 20
#define F4FS_SG_MIXER_PURPLES_VIBRANCE 21
#define F4FS_SG_MIXER_PURPLES_BRIGHTNESS 22
#define F4FS_SG_MIXER_MAGENTAS_HUE_SHIFT 23
#define F4FS_SG_MIXER_MAGENTAS_VIBRANCE 24
#define F4FS_SG_MIXER_MAGENTAS_BRIGHTNESS 25
#define F4FS_SG_CDL_SLOPE_R 26
#define F4FS_SG_CDL_SLOPE_G 27
#define F4FS_SG_CDL_SLOPE_B 28
#define F4FS_SG_CDL_POWER_R 29
#define F4FS_SG_CDL_POWER_G 30
#define F4FS_SG_CDL_POWER_B 31
#define F4FS_SG_CDL_OFFSET_R 32
#define F4FS_SG_CDL_OFFSET_G 33
#define F4FS_SG_CDL_OFFSET_B 34
#define F4FS_SG_SHADOWS_GAIN_R 35
#define F4FS_SG_SHADOWS_GAIN_G 36
#define F4FS_SG_SHADOWS_GAIN_B 37
#define F4FS_SG_SHADOWS_OFFSET_R 38
#define F4FS_SG_SHADOWS_OFFSET_G 39
#define F4FS_SG_SHADOWS_OFFSET_B 40
#define F4FS_SG_MIDTONES_GAIN_R 41
#define F4FS_SG_MIDTONES_GAIN_G 42
#define F4FS_SG_MIDTONES_GAIN_B 43
#define F4FS_SG_MIDTONES_OFFSET_R 44
#define F4FS_SG_MIDTONES_OFFSET_G 45
#define F4FS_SG_MIDTONES_OFFSET_B 46
#define F4FS_SG_HIGHLIGHTS_GAIN_R 47
#define F4FS_SG_HIGHLIGHTS_GAIN_G 48
#define F4FS_SG_HIGHLIGHTS_GAIN_B 49
#define F4FS_SG_HIGHLIGHTS_OFFSET_R 50
#define F4FS_SG_HIGHLIGHTS_OFFSET_G 51
#define F4FS_SG_HIGHLIGHTS_OFFSET_B 52
#define F4FS_SG_SHADOWS_START 53
#define F4FS_SG_SHADOWS_END 54
#define F4FS_SG_HIGHLIGHTS_START 55
#define F4FS_SG_HIGHLIGHTS_END 56
#define F4FS_SG_CONTRAST_R 57
#define F4FS_SG_CONTRAST_G 58
#define F4FS_SG_CONTRAST_B 59
#define F4FS_SG_PIVOT_R 60
#define F4FS_SG_PIVOT_G 61
#define F4FS_SG_PIVOT_B 62
#define F4FS_SG_WB_ROW 16

static const float F4FS_SG_PI = 3.14159265358979f;
static const float3 F4FS_SG_LUMA = float3(0.2126390f, 0.7151687f, 0.0721923f);

float3 F4FS_SgVec3(uint i)
{
	return float3(F4FS_SG(i), F4FS_SG(i + 1), F4FS_SG(i + 2));
}

float3 F4FS_SgRgbToOklab(float3 c)
{
	const float l = 0.4121656120f * c.r + 0.5362752080f * c.g + 0.0514575653f * c.b;
	const float m = 0.2118591070f * c.r + 0.6807189584f * c.g + 0.1074065790f * c.b;
	const float s = 0.0883097947f * c.r + 0.2818474174f * c.g + 0.6302613616f * c.b;
	const float3 lms = sign(float3(l, m, s)) * pow(abs(float3(l, m, s)), 1.0f / 3.0f);
	return float3(
		0.2104542553f * lms.x + 0.7936177850f * lms.y - 0.0040720468f * lms.z,
		1.9779984951f * lms.x - 2.4285922050f * lms.y + 0.4505937099f * lms.z,
		0.0259040371f * lms.x + 0.7827717662f * lms.y - 0.8086757660f * lms.z);
}

float3 F4FS_SgOklabToRgb(float3 c)
{
	float3 lms = float3(
		c.x + 0.3963377774f * c.y + 0.2158037573f * c.z,
		c.x - 0.1055613458f * c.y - 0.0638541728f * c.z,
		c.x - 0.0894841775f * c.y - 1.2914855480f * c.z);
	lms = lms * lms * lms;
	return float3(
		4.0767245293f * lms.x - 3.3072168827f * lms.y + 0.2307590544f * lms.z,
		-1.2681437731f * lms.x + 2.6093323231f * lms.y - 0.3411344290f * lms.z,
		-0.0041119885f * lms.x - 0.7034763098f * lms.y + 1.7068625689f * lms.z);
}

// Sign-preserving power (open-shaders' Gamma() with pivots 0 and 1); 0 stays 0.
float3 F4FS_SgSignedPow(float3 x, float3 p)
{
	return x == 0.0f ? 0.0f : sign(x) * pow(abs(x), p);
}

float3 F4FS_SgFinite(float3 c)
{
	return float3(isfinite(c.r) ? clamp(c.r, 0.0, 65504.0) : 0.0,
		isfinite(c.g) ? clamp(c.g, 0.0, 65504.0) : 0.0,
		isfinite(c.b) ? clamp(c.b, 0.0, 65504.0) : 0.0);
}

float3 F4FS_SceneGrade(float3 color)
{
	color = F4FS_SgFinite(color);

	// White balance (host-solved Bradford adaptation to D65).
	const uint wb = F4FS_SG_BASE + F4FS_SG_WB_ROW;
	color = float3(dot(Params[wb].xyz, color), dot(Params[wb + 1].xyz, color), dot(Params[wb + 2].xyz, color));

	// OKLCH saturation, vibrance and hue, then the 7-hue mixer (one OKLab round trip).
	{
		const float3 lab = F4FS_SgRgbToOklab(color);
		float l = lab.x;
		float c = sqrt(lab.y * lab.y + lab.z * lab.z);
		float h = c > 0.0f ? atan2(lab.z, lab.y) : 0.0f;
		c *= F4FS_SG(F4FS_SG_SATURATION);
		c *= max(0.0f, 1.0f + (F4FS_SG(F4FS_SG_VIBRANCE) - 1.0f) * (1.0f - smoothstep(0.0f, 0.5f, c)));
		h += F4FS_SG(F4FS_SG_HUE_SHIFT) * F4FS_SG_PI;

		static const float redHue = 0.08120523664f;
		const float turns = h / (2.0f * F4FS_SG_PI) - redHue;
		float lerpFactor = (turns - floor(turns)) * 7.0f;
		const float leftFloor = floor(lerpFactor);
		const uint left = (uint)leftFloor % 7u;
		lerpFactor -= leftFloor;
		const uint right = (left + 1u) % 7u;
		const float effect = saturate(c / 0.37f);
		const uint l3 = F4FS_SG_MIXER_REDS_HUE_SHIFT + left * 3u;
		const uint r3 = F4FS_SG_MIXER_REDS_HUE_SHIFT + right * 3u;
		h += lerp(F4FS_SG(l3), F4FS_SG(r3), lerpFactor) * F4FS_SG_PI / 4.0f;
		const float hueVibrance = lerp(F4FS_SG(l3 + 1u), F4FS_SG(r3 + 1u), lerpFactor);
		c *= max(0.0f, 1.0f + (hueVibrance - 1.0f) * (1.0f - smoothstep(0.0f, 0.5f, c)));
		l += lerp(F4FS_SG(l3 + 2u), F4FS_SG(r3 + 2u), lerpFactor) * effect;
		color = F4FS_SgOklabToRgb(float3(l, c * cos(h), c * sin(h)));
	}

	// ASC CDL.
	color = F4FS_SgSignedPow(color * F4FS_SgVec3(F4FS_SG_CDL_SLOPE_R) + F4FS_SgVec3(F4FS_SG_CDL_OFFSET_R),
		F4FS_SgVec3(F4FS_SG_CDL_POWER_R));

	// Shadows / midtones / highlights gain and offset.
	{
		const float3 sg = F4FS_SgVec3(F4FS_SG_SHADOWS_GAIN_R);
		const float3 so = F4FS_SgVec3(F4FS_SG_SHADOWS_OFFSET_R);
		const float3 mg = F4FS_SgVec3(F4FS_SG_MIDTONES_GAIN_R);
		const float3 mo = F4FS_SgVec3(F4FS_SG_MIDTONES_OFFSET_R);
		const float3 hg = F4FS_SgVec3(F4FS_SG_HIGHLIGHTS_GAIN_R);
		const float3 ho = F4FS_SgVec3(F4FS_SG_HIGHLIGHTS_OFFSET_R);
		const float luma = dot(color, F4FS_SG_LUMA);
		const float shadowWeight = 1.0f - smoothstep(F4FS_SG(F4FS_SG_SHADOWS_START), F4FS_SG(F4FS_SG_SHADOWS_END), luma);
		[branch] if (any(hg < mg) || any(ho < mo)) {
			// Highlights.hlsli Apply: an integrated mask keeps a reduced gain monotonic.
			const float eps = 1e-6f;
			const float begin = max(F4FS_SG(F4FS_SG_HIGHLIGHTS_START), 0.0f);
			const float width = max(F4FS_SG(F4FS_SG_HIGHLIGHTS_END) - begin, eps);
			const float t = saturate((luma - begin) / width);
			const float weight = t * t * (3.0f - 2.0f * t);
			const float integral = width * t * t * t * (1.0f - 0.5f * t) + max(luma - (begin + width), 0.0f);
			const float integratedWeight = integral / max(luma, eps);
			const float3 gainWeight = hg < mg ? integratedWeight : weight;
			const float3 gain = mg + (hg - mg) * gainWeight;
			const float3 offsetDelta = ho - mo;
			float3 offset = mo + max(offsetDelta, 0.0f) * weight;
			[branch] if (any(offsetDelta < 0.0f)) {
				const float transitionIntegral = width * t * t * t *
					(1.0f - 0.5f * t - t * t * (9.0f / 5.0f - 2.0f * t + 4.0f / 7.0f * t * t));
				const float3 available = max(color / max(luma, eps), 0.0f) *
					(min(mg, hg) * integral + max(mg - hg, 0.0f) * transitionIntegral);
				const float3 reduction = max(-offsetDelta, 0.0f);
				offset -= reduction * available / max(reduction + available, eps);
			}
			color = color * gain + offset + shadowWeight * (color * (sg - mg) + so - mo);
		} else {
			const float highlightWeight = smoothstep(F4FS_SG(F4FS_SG_HIGHLIGHTS_START), F4FS_SG(F4FS_SG_HIGHLIGHTS_END), luma);
			const float midWeight = 1.0f - shadowWeight - highlightWeight;
			color = color * (sg * shadowWeight + mg * midWeight + hg * highlightWeight) +
				(so * shadowWeight + mo * midWeight + ho * highlightWeight);
		}
	}

	// Per-channel contrast around the pivot.
	const float3 pivot = F4FS_SgVec3(F4FS_SG_PIVOT_R);
	color = F4FS_SgSignedPow(color / pivot, F4FS_SgVec3(F4FS_SG_CONTRAST_R)) * pivot;

	return F4FS_SgFinite(color);
}

#endif
