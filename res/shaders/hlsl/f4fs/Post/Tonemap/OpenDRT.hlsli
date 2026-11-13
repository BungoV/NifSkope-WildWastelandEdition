// Derived from open-shaders (https://github.com/alandtse/open-shaders) at 4470b8f,
// features/Post Processing/Shaders/PostProcessing/ColorGrading/Include/OpenDRT.hlsli
// (jiayev's port, with the b3983f94 colour fixes), itself an HLSL conversion of OpenDRT
// v1.1.0 by Jed Smith (https://github.com/jedypod/open-display-transform), licensed
// GPL-3.0. See THIRD_PARTY.md.
//
// THE F4FS FORM (2026-10-22). open-shaders feeds the transform from a 72-field cbuffer
// struct. Here the look modules (contrast, purity, brilliance, hue contrast and hue
// shifts, with their enables) ride in Params[5..19] (src/Post/OpenDrtLook.h; the
// look and tonescale presets are applied on the host), and the tonescale
// constraint maths that the DCTL had to run per pixel ("these should be
// pre-calculated but there is no way to do this in DCTL") is solved once on the
// host (f4fs::post::ResolveOpenDrtTonescale) and uploaded. Two more changes:
// every pow() whose base can reach zero goes through F4FS_OdrtSpow, so a
// zero base with a zero exponent cannot return NaN, and the display gamut is
// always Rec.709: F4FS's scene-HDR transport is scRGB clamped at zero, so a P3
// output would only be clipped later. Host mirror: f4fs::post::TonemapOpenDrt.
#ifndef F4FS_POST_TONEMAP_OPENDRT_HLSLI
#define F4FS_POST_TONEMAP_OPENDRT_HLSLI

static const float F4FS_ODRT_PI = 3.14159265358979323846f;

// Linear Rec.709/sRGB to XYZ D65.
static const float3x3 F4FS_ODRT_REC709_TO_XYZ = float3x3(
	0.412390799265959229f, 0.357584339383878125f, 0.180480788401834347f,
	0.212639005871510217f, 0.71516867876775625f, 0.0721923153607337414f,
	0.0193308187155918181f, 0.119194779794626018f, 0.950532152249661033f);
// XYZ D65 to linear Rec.709/sRGB.
static const float3x3 F4FS_ODRT_XYZ_TO_REC709 = float3x3(
	3.24096994190452348f, -1.53738317757009435f, -0.498610760293003552f,
	-0.969243636280879506f, 1.87596750150771996f, 0.0415550574071755843f,
	0.0556300796969936354f, -0.20397695888897649f, 1.05697151424287816f);
// P3-D65 to XYZ D65.
static const float3x3 F4FS_ODRT_P3D65_TO_XYZ = float3x3(
	0.486570948648216151f, 0.265667693169093f, 0.198217285234362467f,
	0.228974564069748754f, 0.691738521836506193f, 0.079286914093744984f,
	-4.00000000000000029e-17f, 0.0451133818589026167f, 1.04394436890097575f);
// XYZ D65 to P3-D65.
static const float3x3 F4FS_ODRT_XYZ_TO_P3D65 = float3x3(
	2.49349691194142542f, -0.93138361791912383f, -0.402710784450716841f,
	-0.829488969561574696f, 1.76266406031834655f, 0.0236246858419435941f,
	0.0358458302437844531f, -0.0761723892680418041f, 0.956884524007687309f);

// CAT02 chromatic adaptation from D65 to the creative whites.
static const float3x3 F4FS_ODRT_CAT_D65_TO_D93 = float3x3(
	0.95703423023223877f, -0.0247171502560377121f, 0.0624028593301773071f,
	-0.0179296955466270447f, 0.990019857883453369f, 0.0248119533061981201f,
	0.00127589143812656403f, 0.00427919067442417058f, 1.29345715045928955f);
static const float3x3 F4FS_ODRT_CAT_D65_TO_D75 = float3x3(
	0.981001079082489014f, -0.0116619253531098366f, 0.0265614092350006104f,
	-0.00843488052487373352f, 0.996506094932556152f, 0.0105696544051170349f,
	0.000552809564396739006f, 0.00179840810596942902f, 1.12374722957611084f);
static const float3x3 F4FS_ODRT_CAT_D65_TO_D60 = float3x3(
	1.01182246208190918f, 0.00778879318386316299f, -0.0157783031463623047f,
	0.00561682833358645439f, 1.00150644779205322f, -0.00628517568111419678f,
	-0.000335735734552145004f, -0.0010509500280022619f, 0.927366673946380615f);
static const float3x3 F4FS_ODRT_CAT_D65_TO_D55 = float3x3(
	1.02585089206695557f, 0.0179439820349216461f, -0.0332137793302536011f,
	0.0129133854061365128f, 1.00214779376983643f, -0.0132421031594276428f,
	-0.000719940289855003032f, -0.00218106806278228803f, 0.84868013858795166f);
static const float3x3 F4FS_ODRT_CAT_D65_TO_D50 = float3x3(
	1.04257404804229736f, 0.03089117631316185f, -0.052812620997428894f,
	0.0221935361623764038f, 1.00185668468475342f, -0.0210737623274326324f,
	-0.00116488314233720303f, -0.00342052709311246915f, 0.761789083480834961f);

// The creative whites, in the order of [Post] sTonemapOpenDrtCreativeWhite.
static const int F4FS_ODRT_CWP_D93 = 0;
static const int F4FS_ODRT_CWP_D75 = 1;
static const int F4FS_ODRT_CWP_D65 = 2;
static const int F4FS_ODRT_CWP_D60 = 3;
static const int F4FS_ODRT_CWP_D55 = 4;
static const int F4FS_ODRT_CWP_D50 = 5;

// The look ([Post.OpenDRT], src/Post/OpenDrtLook.h): field i rides in Params[5 + i / 4][i % 4].
#define F4FS_ODRT_LOOK(i) Params[5 + (i) / 4][(i) % 4]
#define F4FS_ODRT_TN_HCON_ENABLE F4FS_ODRT_LOOK(0)
#define F4FS_ODRT_TN_HCON F4FS_ODRT_LOOK(1)
#define F4FS_ODRT_TN_HCON_PV F4FS_ODRT_LOOK(2)
#define F4FS_ODRT_TN_HCON_ST F4FS_ODRT_LOOK(3)
#define F4FS_ODRT_TN_LCON_ENABLE F4FS_ODRT_LOOK(4)
#define F4FS_ODRT_TN_LCON F4FS_ODRT_LOOK(5)
#define F4FS_ODRT_TN_LCON_W F4FS_ODRT_LOOK(6)
#define F4FS_ODRT_RS_SA F4FS_ODRT_LOOK(7)
#define F4FS_ODRT_RS_RW F4FS_ODRT_LOOK(8)
#define F4FS_ODRT_RS_BW F4FS_ODRT_LOOK(9)
#define F4FS_ODRT_PT_ENABLE F4FS_ODRT_LOOK(10)
#define F4FS_ODRT_PT_LML F4FS_ODRT_LOOK(11)
#define F4FS_ODRT_PT_LML_R F4FS_ODRT_LOOK(12)
#define F4FS_ODRT_PT_LML_G F4FS_ODRT_LOOK(13)
#define F4FS_ODRT_PT_LML_B F4FS_ODRT_LOOK(14)
#define F4FS_ODRT_PT_LMH F4FS_ODRT_LOOK(15)
#define F4FS_ODRT_PT_LMH_R F4FS_ODRT_LOOK(16)
#define F4FS_ODRT_PT_LMH_B F4FS_ODRT_LOOK(17)
#define F4FS_ODRT_PTL_ENABLE F4FS_ODRT_LOOK(18)
#define F4FS_ODRT_PTL_C F4FS_ODRT_LOOK(19)
#define F4FS_ODRT_PTL_M F4FS_ODRT_LOOK(20)
#define F4FS_ODRT_PTL_Y F4FS_ODRT_LOOK(21)
#define F4FS_ODRT_PTM_ENABLE F4FS_ODRT_LOOK(22)
#define F4FS_ODRT_PTM_LOW F4FS_ODRT_LOOK(23)
#define F4FS_ODRT_PTM_LOW_RNG F4FS_ODRT_LOOK(24)
#define F4FS_ODRT_PTM_LOW_ST F4FS_ODRT_LOOK(25)
#define F4FS_ODRT_PTM_HIGH F4FS_ODRT_LOOK(26)
#define F4FS_ODRT_PTM_HIGH_RNG F4FS_ODRT_LOOK(27)
#define F4FS_ODRT_PTM_HIGH_ST F4FS_ODRT_LOOK(28)
#define F4FS_ODRT_BRL_ENABLE F4FS_ODRT_LOOK(29)
#define F4FS_ODRT_BRL F4FS_ODRT_LOOK(30)
#define F4FS_ODRT_BRL_R F4FS_ODRT_LOOK(31)
#define F4FS_ODRT_BRL_G F4FS_ODRT_LOOK(32)
#define F4FS_ODRT_BRL_B F4FS_ODRT_LOOK(33)
#define F4FS_ODRT_BRL_RNG F4FS_ODRT_LOOK(34)
#define F4FS_ODRT_BRL_ST F4FS_ODRT_LOOK(35)
#define F4FS_ODRT_BRLP_ENABLE F4FS_ODRT_LOOK(36)
#define F4FS_ODRT_BRLP F4FS_ODRT_LOOK(37)
#define F4FS_ODRT_BRLP_R F4FS_ODRT_LOOK(38)
#define F4FS_ODRT_BRLP_G F4FS_ODRT_LOOK(39)
#define F4FS_ODRT_BRLP_B F4FS_ODRT_LOOK(40)
#define F4FS_ODRT_HC_ENABLE F4FS_ODRT_LOOK(41)
#define F4FS_ODRT_HC_R F4FS_ODRT_LOOK(42)
#define F4FS_ODRT_HC_R_RNG F4FS_ODRT_LOOK(43)
#define F4FS_ODRT_HS_RGB_ENABLE F4FS_ODRT_LOOK(44)
#define F4FS_ODRT_HS_R F4FS_ODRT_LOOK(45)
#define F4FS_ODRT_HS_R_RNG F4FS_ODRT_LOOK(46)
#define F4FS_ODRT_HS_G F4FS_ODRT_LOOK(47)
#define F4FS_ODRT_HS_G_RNG F4FS_ODRT_LOOK(48)
#define F4FS_ODRT_HS_B F4FS_ODRT_LOOK(49)
#define F4FS_ODRT_HS_B_RNG F4FS_ODRT_LOOK(50)
#define F4FS_ODRT_HS_CMY_ENABLE F4FS_ODRT_LOOK(51)
#define F4FS_ODRT_HS_C F4FS_ODRT_LOOK(52)
#define F4FS_ODRT_HS_C_RNG F4FS_ODRT_LOOK(53)
#define F4FS_ODRT_HS_M F4FS_ODRT_LOOK(54)
#define F4FS_ODRT_HS_M_RNG F4FS_ODRT_LOOK(55)
#define F4FS_ODRT_HS_Y F4FS_ODRT_LOOK(56)
#define F4FS_ODRT_HS_Y_RNG F4FS_ODRT_LOOK(57)

// The run-time half: the host-solved tonescale and the creative white.
struct F4FS_OdrtParams
{
	float s;          // ts_s: hyperbolic compression scale
	float p;          // ts_p: contrast after surround compensation
	float m2;         // ts_m2: scale for the inverse toe
	float dsc;        // ts_dsc: 100 / peak nits (display encoding scale)
	float s1;         // ts_s1: purity-compression scale (lerped toward HDR by HDR purity)
	float sLp100;     // s_Lp100: the 100-nit scale
	float toe;        // tn_toe
	float offset;     // tn_off
	int creativeWhite;
	float creativeWhiteLimit;
};

float F4FS_OdrtSdiv(float a, float b)
{
	return b == 0.0f ? 0.0f : a / b;
}

float3 F4FS_OdrtSdiv3(float3 a, float b)
{
	return float3(F4FS_OdrtSdiv(a.x, b), F4FS_OdrtSdiv(a.y, b), F4FS_OdrtSdiv(a.z, b));
}

// Safe power: a non-positive base is returned as is (OpenDRT's spowf).
float F4FS_OdrtSpow(float a, float b)
{
	return a <= 0.0f ? a : pow(a, b);
}

float F4FS_OdrtHypot2(float2 v)
{
	return sqrt(max(0.0f, v.x * v.x + v.y * v.y));
}

float F4FS_OdrtHypot3(float3 v)
{
	return sqrt(max(0.0f, v.x * v.x + v.y * v.y + v.z * v.z));
}

float F4FS_OdrtCompressHyperbolicPower(float x, float s, float p)
{
	return F4FS_OdrtSpow(x / (x + s), p);
}

float F4FS_OdrtCompressToeQuadratic(float x, float toe)
{
	if (toe == 0.0f) {
		return x;
	}
	return F4FS_OdrtSpow(x, 2.0f) / (x + toe);
}

// OpenDRT's cubic toe (Contrast Low); inv solves for the input.
float F4FS_OdrtCompressToeCubic(float x, float m, float w, bool inv)
{
	if (m == 1.0f) {
		return x;
	}
	const float x2 = x * x;
	if (!inv) {
		return x * (x2 + m * w) / (x2 + w);
	}
	const float p0 = x2 - 3.0f * m * w;
	const float p1 = 2.0f * x2 + 27.0f * w - 9.0f * m * w;
	const float p2 = pow(sqrt(x2 * p1 * p1 - 4.0f * p0 * p0 * p0) / 2.0f + x * p1 / 2.0f, 1.0f / 3.0f);
	return p0 / (3.0f * p2) + p2 / 3.0f + x / 3.0f;
}

// OpenDRT's Contrast High: a power above the pivot with a linear extension.
float F4FS_OdrtContrastHigh(float x, float p, float pv, float pvLx)
{
	const float x0 = 0.18f * pow(2.0f, pv);
	if (x < x0 || p == 1.0f) {
		return x;
	}
	const float o = x0 - x0 / p;
	const float s0 = pow(x0, 1.0f - p) / p;
	const float x1 = x0 * pow(2.0f, pvLx);
	const float k1 = p * s0 * pow(x1, p) / x1;
	const float y1 = s0 * pow(x1, p) + o;
	return x > x1 ? k1 * (x - x1) + y1 : s0 * pow(x, p) + o;
}

float F4FS_OdrtSoftplus(float x, float s)
{
	if (x > 10.0f * s || s < 1e-4f) {
		return x;
	}
	return s * log(max(0.0f, 1.0f + exp(x / s)));
}

float F4FS_OdrtGaussWindow(float x, float w)
{
	return exp(-x * x / w);
}

float2 F4FS_OdrtOpponent(float3 rgb)
{
	return float2(rgb.x - rgb.z, rgb.y - (rgb.x + rgb.z) / 2.0f);
}

float F4FS_OdrtHueOffset(float h, float o)
{
	return fmod(h - o + F4FS_ODRT_PI, 2.0f * F4FS_ODRT_PI) - F4FS_ODRT_PI;
}

// display_gamut_whitepoint with display_gamut = 0 (Rec.709). D65 is the identity,
// so it is skipped outright rather than blended with itself.
float3 F4FS_OdrtDisplayWhitepoint(float3 rgb, float tsn, float limit, int creativeWhite)
{
	rgb = mul(F4FS_ODRT_P3D65_TO_XYZ, rgb);
	float scale = 1.0f;
	if (creativeWhite != F4FS_ODRT_CWP_D65) {
		const float3 neutral = rgb;
		const float cwpF = F4FS_OdrtSpow(tsn, 2.0f * limit);
		float cwpNorm = 1.0f;
		if (creativeWhite == F4FS_ODRT_CWP_D93) {
			rgb = mul(F4FS_ODRT_CAT_D65_TO_D93, rgb);
			cwpNorm = 0.744192699063f;
		} else if (creativeWhite == F4FS_ODRT_CWP_D75) {
			rgb = mul(F4FS_ODRT_CAT_D65_TO_D75, rgb);
			cwpNorm = 0.873470832146f;
		} else if (creativeWhite == F4FS_ODRT_CWP_D60) {
			rgb = mul(F4FS_ODRT_CAT_D65_TO_D60, rgb);
			cwpNorm = 0.955936992163f;
		} else if (creativeWhite == F4FS_ODRT_CWP_D55) {
			rgb = mul(F4FS_ODRT_CAT_D65_TO_D55, rgb);
			cwpNorm = 0.905671332781f;
		} else {
			rgb = mul(F4FS_ODRT_CAT_D65_TO_D50, rgb);
			cwpNorm = 0.850004385027f;
		}
		rgb = rgb * cwpF + neutral * (1.0f - cwpF);
		scale = cwpNorm * cwpF + 1.0f - cwpF;
	}
	return mul(F4FS_ODRT_XYZ_TO_REC709, rgb) * scale;
}

// XYZ D65 in, display-linear Rec.709 out, normalized so 1.0 is the peak the
// tonescale was solved for (unclamped).
float3 F4FS_OpenDrtTransform(float3 xyz, F4FS_OdrtParams params)
{
	float3 rgb = mul(F4FS_ODRT_XYZ_TO_P3D65, xyz);

	// Rendering space: "desaturate" to control the scale of the colour volume in the rgb ratios.
	const float3 rsW = float3(F4FS_ODRT_RS_RW, 1.0f - F4FS_ODRT_RS_RW - F4FS_ODRT_RS_BW, F4FS_ODRT_RS_BW);
	float satL = rgb.x * rsW.x + rgb.y * rsW.y + rgb.z * rsW.z;
	rgb = satL * F4FS_ODRT_RS_SA + rgb * (1.0f - F4FS_ODRT_RS_SA);

	rgb += params.offset;

	// Tonescale norm and RGB ratios.
	float tsn = F4FS_OdrtHypot3(rgb) / 1.73205080756887729353f;
	rgb = F4FS_OdrtSdiv3(rgb, tsn);

	const float2 opp = F4FS_OdrtOpponent(rgb);
	float achD = F4FS_OdrtHypot2(opp) / 2.0f;
	achD = 1.25f * F4FS_OdrtCompressToeQuadratic(achD, 0.25f);

	// Hue angle, rotated so that red = 0. An achromatic pixel has no angle; the
	// fallback keeps atan2(0, 0) from returning NaN (b3983f94).
	const float2 hueOpp = any(opp != 0.0f) ? opp : float2(0.0f, 1.0f);
	const float hue = fmod(atan2(hueOpp.x, hueOpp.y) + F4FS_ODRT_PI + 1.10714931f, 2.0f * F4FS_ODRT_PI);

	const float3 haRgb = float3(
		F4FS_OdrtGaussWindow(F4FS_OdrtHueOffset(hue, 0.1f), 0.66f),
		F4FS_OdrtGaussWindow(F4FS_OdrtHueOffset(hue, 4.3f), 0.66f),
		F4FS_OdrtGaussWindow(F4FS_OdrtHueOffset(hue, 2.3f), 0.66f));
	const float3 haRgbHs = float3(
		F4FS_OdrtGaussWindow(F4FS_OdrtHueOffset(hue, -0.4f), 0.66f),
		haRgb.y,
		F4FS_OdrtGaussWindow(F4FS_OdrtHueOffset(hue, 2.5f), 0.66f));
	const float3 haCmy = float3(
		F4FS_OdrtGaussWindow(F4FS_OdrtHueOffset(hue, 3.3f), 0.5f),
		F4FS_OdrtGaussWindow(F4FS_OdrtHueOffset(hue, 1.3f), 0.5f),
		F4FS_OdrtGaussWindow(F4FS_OdrtHueOffset(hue, -1.15f), 0.5f));

	// Brilliance.
	[branch] if (F4FS_ODRT_BRL_ENABLE > 0.5f) {
		const float brlTsf = F4FS_OdrtSpow(tsn / (tsn + 1.0f), 1.0f - F4FS_ODRT_BRL_RNG);
		const float brlExf = (F4FS_ODRT_BRL + F4FS_ODRT_BRL_R * haRgb.x + F4FS_ODRT_BRL_G * haRgb.y + F4FS_ODRT_BRL_B * haRgb.z) *
			F4FS_OdrtSpow(achD, 1.0f / F4FS_ODRT_BRL_ST);
		const float brlEx = pow(2.0f, brlExf * (brlExf < 0.0f ? brlTsf : 1.0f - brlTsf));
		tsn *= brlEx;
	}

	// Contrast Low, then Contrast High.
	[branch] if (F4FS_ODRT_TN_LCON_ENABLE > 0.5f) {
		const float lconM = pow(2.0f, -F4FS_ODRT_TN_LCON);
		float lconW = F4FS_ODRT_TN_LCON_W / 4.0f;
		lconW *= lconW;
		const float x0 = 0.18f + params.offset;
		tsn *= F4FS_OdrtCompressToeCubic(x0, lconM, lconW, true) / x0;
		tsn = F4FS_OdrtCompressToeCubic(tsn, lconM, lconW, false);
	}
	[branch] if (F4FS_ODRT_TN_HCON_ENABLE > 0.5f) {
		tsn = F4FS_OdrtContrastHigh(tsn, pow(2.0f, F4FS_ODRT_TN_HCON), F4FS_ODRT_TN_HCON_PV, F4FS_ODRT_TN_HCON_ST);
	}

	// Hyperbolic compression.
	const float tsnPt = F4FS_OdrtCompressHyperbolicPower(tsn, params.s1, params.p);
	const float tsnConst = F4FS_OdrtCompressHyperbolicPower(tsn, params.sLp100, params.p);
	tsn = F4FS_OdrtCompressHyperbolicPower(tsn, params.s, params.p);

	// Hue contrast R.
	[branch] if (F4FS_ODRT_HC_ENABLE > 0.5f) {
		float hcTs = 1.0f - tsnConst;
		float hcC = hcTs * (1.0f - achD) + achD * (1.0f - hcTs);
		hcC *= achD * haRgb.x;
		hcTs = F4FS_OdrtSpow(hcTs, 1.0f / F4FS_ODRT_HC_R_RNG);
		const float hcF = F4FS_ODRT_HC_R * (hcC - 2.0f * hcC * hcTs) + 1.0f;
		rgb = float3(rgb.x, rgb.y * hcF, rgb.z * hcF);
	}

	// Hue shift RGB by the purity-compress tonescale, more as intensity rises.
	[branch] if (F4FS_ODRT_HS_RGB_ENABLE > 0.5f) {
		const float3 hsRgb = float3(
			haRgbHs.x * achD * F4FS_OdrtSpow(tsnPt, 1.0f / F4FS_ODRT_HS_R_RNG),
			haRgbHs.y * achD * F4FS_OdrtSpow(tsnPt, 1.0f / F4FS_ODRT_HS_G_RNG),
			haRgbHs.z * achD * F4FS_OdrtSpow(tsnPt, 1.0f / F4FS_ODRT_HS_B_RNG));
		float3 hsf = float3(hsRgb.x * F4FS_ODRT_HS_R, hsRgb.y * -F4FS_ODRT_HS_G, hsRgb.z * -F4FS_ODRT_HS_B);
		hsf = float3(hsf.z - hsf.y, hsf.x - hsf.z, hsf.y - hsf.x);
		rgb += hsf;
	}

	// Hue shift CMY by the tonescale, less as intensity rises.
	[branch] if (F4FS_ODRT_HS_CMY_ENABLE > 0.5f) {
		const float tsnPtCompl = 1.0f - tsnPt;
		const float3 hsCmy = float3(
			haCmy.x * achD * F4FS_OdrtSpow(tsnPtCompl, 1.0f / F4FS_ODRT_HS_C_RNG),
			haCmy.y * achD * F4FS_OdrtSpow(tsnPtCompl, 1.0f / F4FS_ODRT_HS_M_RNG),
			haCmy.z * achD * F4FS_OdrtSpow(tsnPtCompl, 1.0f / F4FS_ODRT_HS_Y_RNG));
		float3 hsf = float3(hsCmy.x * -F4FS_ODRT_HS_C, hsCmy.y * F4FS_ODRT_HS_M, hsCmy.z * F4FS_ODRT_HS_Y);
		hsf = float3(hsf.z - hsf.y, hsf.x - hsf.z, hsf.y - hsf.x);
		rgb += hsf;
	}

	// Purity compression.
	float ptf = 1.0f;
	[branch] if (F4FS_ODRT_PT_ENABLE > 0.5f) {
		const float ptLmlP = 1.0f + 4.0f * (1.0f - tsnPt) *
			(F4FS_ODRT_PT_LML + F4FS_ODRT_PT_LML_R * haRgbHs.x + F4FS_ODRT_PT_LML_G * haRgbHs.y + F4FS_ODRT_PT_LML_B * haRgbHs.z);
		ptf = 1.0f - F4FS_OdrtSpow(tsnPt, ptLmlP);
		const float ptLmhP = (1.0f - achD * (F4FS_ODRT_PT_LMH_R * haRgbHs.x + F4FS_ODRT_PT_LMH_B * haRgbHs.z)) *
			(1.0f - F4FS_ODRT_PT_LMH * achD);
		ptf = F4FS_OdrtSpow(ptf, ptLmhP);
	}

	// Mid-range purity: boost on the low end, reduce on the high end. A zero strength
	// or range leaves that end alone.
	[branch] if (F4FS_ODRT_PTM_ENABLE > 0.5f) {
		const float ptmLowF = F4FS_ODRT_PTM_LOW_ST == 0.0f || F4FS_ODRT_PTM_LOW_RNG == 0.0f ? 1.0f :
			1.0f + F4FS_ODRT_PTM_LOW * exp(-2.0f * achD * achD / F4FS_ODRT_PTM_LOW_ST) *
			F4FS_OdrtSpow(1.0f - tsnConst, 1.0f / F4FS_ODRT_PTM_LOW_RNG);
		const float ptmHighF = F4FS_ODRT_PTM_HIGH_ST == 0.0f || F4FS_ODRT_PTM_HIGH_RNG == 0.0f ? 1.0f :
			1.0f + F4FS_ODRT_PTM_HIGH * exp(-2.0f * achD * achD / F4FS_ODRT_PTM_HIGH_ST) *
			F4FS_OdrtSpow(tsnPt, 1.0f / (4.0f * F4FS_ODRT_PTM_HIGH_RNG));
		ptf *= ptmLowF * ptmHighF;
	}

	// Lerp to peak achromatic by ptf in rgb ratios.
	rgb = rgb * ptf + 1.0f - ptf;

	// Inverse rendering space.
	satL = rgb.x * rsW.x + rgb.y * rsW.y + rgb.z * rsW.z;
	rgb = (satL * F4FS_ODRT_RS_SA - rgb) / (F4FS_ODRT_RS_SA - 1.0f);

	rgb = F4FS_OdrtDisplayWhitepoint(rgb, tsnConst, params.creativeWhiteLimit, params.creativeWhite);

	// Post brilliance.
	[branch] if (F4FS_ODRT_BRLP_ENABLE > 0.5f) {
		const float2 brlpOpp = F4FS_OdrtOpponent(rgb);
		float brlpAchD = F4FS_OdrtHypot2(brlpOpp) / 4.0f;
		brlpAchD = 1.1f * (brlpAchD * brlpAchD / (brlpAchD + 0.1f));
		const float3 brlpHaRgb = achD * haRgb;
		const float brlpM = F4FS_ODRT_BRLP + F4FS_ODRT_BRLP_R * brlpHaRgb.x + F4FS_ODRT_BRLP_G * brlpHaRgb.y + F4FS_ODRT_BRLP_B * brlpHaRgb.z;
		rgb *= pow(2.0f, brlpM * brlpAchD * tsn);
	}

	// Purity compress low.
	[branch] if (F4FS_ODRT_PTL_ENABLE > 0.5f) {
		rgb = float3(
			F4FS_OdrtSoftplus(rgb.x, F4FS_ODRT_PTL_C),
			F4FS_OdrtSoftplus(rgb.y, F4FS_ODRT_PTL_M),
			F4FS_OdrtSoftplus(rgb.z, F4FS_ODRT_PTL_Y));
	}

	// Final tonescale adjustments: inverse toe scale, toe, display encoding.
	tsn *= params.m2;
	tsn = F4FS_OdrtCompressToeQuadratic(tsn, params.toe);
	tsn *= params.dsc;

	return rgb * tsn;
}

#endif
