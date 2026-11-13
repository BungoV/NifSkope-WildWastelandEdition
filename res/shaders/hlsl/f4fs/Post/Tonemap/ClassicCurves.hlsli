// The classic tone curves: one permutation (F4FS_TONEMAP_CURVE_VARIANT 8) with a
// runtime switch on Params[2].x, so none of them costs the shipped curves anything.
// Ported from open-shaders' ColorGrading/colorgrading.hlsl (Reinhard, Reinhard
// extended, Hejl Burgess-Dawson, Aldridge, Day/Insomniac, Uchimura, AgX Minimal by
// bwrensch and Troy Sobotka) and renodx's tonemap/frostbite.hlsl and
// tonemap/hermite_spline.hlsl (MIT, Carlos Lopez), with each curve's open-shaders
// default parameters. See THIRD_PARTY.md.
// Host mirror: f4fs::post::TonemapClassic (src/Post/Tonemap.cpp).

static const uint F4FS_CLASSIC_REINHARD = 0;
static const uint F4FS_CLASSIC_REINHARD_EXTENDED = 1;
static const uint F4FS_CLASSIC_HEJL_BURGESS_DAWSON = 2;
static const uint F4FS_CLASSIC_ALDRIDGE = 3;
static const uint F4FS_CLASSIC_INSOMNIAC = 4;
static const uint F4FS_CLASSIC_UCHIMURA = 5;
static const uint F4FS_CLASSIC_AGX = 6;
static const uint F4FS_CLASSIC_FROSTBITE = 7;

static const float3 F4FS_CLASSIC_LUMA = float3(0.2126390059, 0.7151686788, 0.0721923154);

float3 F4FS_ClassicLumaScale(float3 color, float lumaIn, float lumaOut)
{
	return lumaIn > 0.0 ? color * (lumaOut / lumaIn) : color;
}

float3 F4FS_ClassicHejl(float3 x)
{
	x = (x * (6.2 * x + 0.5)) / (x * (6.2 * x + 1.7) + 0.06);
	return pow(saturate(x), 2.2);
}

float F4FS_ClassicDay(float x)
{
	const float b = 0.0, w = 2.0, c = 0.3, s = 0.8, t = 0.7;
	const float k = (1.0 - t) * (c - b) / ((1.0 - s) * (w - c) + (1.0 - t) * (c - b));
	if (x < c) {
		return k * (1.0 - t) * (x - b) / (c - (1.0 - t) * b - t * x);
	}
	return (1.0 - k) * (x - c) / (s * x + (1.0 - s) * w - c) + k;
}

float3 F4FS_ClassicUchimura(float3 x, float peak)
{
	const float P = peak, a = 1.0, m = 0.22, l = 0.4, c = 1.33, b = 0.0;
	const float l0 = ((P - m) * l) / a;
	const float S0 = m + l0;
	const float S1 = m + a * l0;
	const float C2 = (a * P) / (P - S1);
	const float CP = -C2 / P;
	const float3 w0 = 1.0 - smoothstep(0.0, m, x);
	const float3 w2 = step(m + l0, x);
	const float3 w1 = 1.0 - w0 - w2;
	const float3 T = m * pow(x / m, c) + b;
	const float3 L = m + a * (x - m);
	const float3 S = P - (P - S1) * exp(CP * (x - S0));
	return clamp(T * w0 + L * w1 + S * w2, 0.0, P);
}

float3 F4FS_ClassicAgx(float3 x)
{
	// HLSL float3x3(...) is row-major; open-shaders transposes these, so the rows here are its columns.
	const float3x3 agxIn = float3x3(
		0.842479062253094, 0.0784335999999992, 0.0792237451477643,
		0.0423282422610123, 0.878468636469772, 0.0791661274605434,
		0.0423756549057051, 0.0784336, 0.879142973793104);
	const float3x3 agxOut = float3x3(
		1.19687900512017, -0.0980208811401368, -0.0990297440797205,
		-0.0528968517574562, 1.15190312990417, -0.0989611768448433,
		-0.0529716355144438, -0.0980434501171241, 1.15107367264116);
	const float minEv = -12.47393;
	const float maxEv = 4.026069;
	x = mul(agxIn, x);
	x = (clamp(log2(x), minEv, maxEv) - minEv) / (maxEv - minEv);
	const float3 x2 = x * x;
	const float3 x4 = x2 * x2;
	x = -17.86 * x4 * x2 * x + 78.01 * x4 * x2 - 126.7 * x4 * x + 92.06 * x4 - 28.72 * x2 * x + 4.361 * x2 -
	    0.1718 * x + 0.002857;
	return pow(saturate(mul(agxOut, x)), 2.2);
}

// renodx ICtCp at a 100-nit PQ scale (Dolby LMS with 4% crosstalk).
float3 F4FS_ClassicPqEncode(float3 v)
{
	const float m1 = 0.1593017578125, m2 = 78.84375, c1 = 0.8359375, c2 = 18.8515625, c3 = 18.6875;
	const float3 y = pow(max(v, 0.0) * 0.01, m1);
	return pow((c1 + c2 * y) / (1.0 + c3 * y), m2);
}

float3 F4FS_ClassicPqDecode(float3 v)
{
	const float m1 = 0.1593017578125, m2 = 78.84375, c1 = 0.8359375, c2 = 18.8515625, c3 = 18.6875;
	const float3 e = pow(max(v, 0.0), 1.0 / m2);
	return pow(max(e - c1, 0.0) / (c2 - c3 * e), 1.0 / m1) * 100.0;
}

static const float3x3 F4FS_CLASSIC_BT709_TO_LMS = float3x3(
	0.302737735, 0.611749566, 0.0903553244,
	0.162085977, 0.713910935, 0.123707404,
	0.0392856878, 0.140089148, 0.81223651);
static const float3x3 F4FS_CLASSIC_LMS_TO_BT709 = float3x3(
	6.09774762, -5.24891804, 0.121105194,
	-1.37439925, 2.62696522, -0.247207255,
	-0.0578843585, -0.199205481, 1.26794762);
static const float3x3 F4FS_CLASSIC_PLMS_TO_ICTCP = float3x3(
	0.5, 0.5, 0.0,
	1.61369992, -3.32339606, 1.70969614,
	4.37806246, -4.24553983, -0.132522631);
static const float3x3 F4FS_CLASSIC_ICTCP_TO_PLMS = float3x3(
	1.0, 0.00860647417, 0.111033531,
	1.0, -0.00860647417, -0.111033531,
	1.0, 0.560046309, -0.320631946);

float3 F4FS_ClassicToIctcp(float3 rgb)
{
	return mul(F4FS_CLASSIC_PLMS_TO_ICTCP, F4FS_ClassicPqEncode(mul(F4FS_CLASSIC_BT709_TO_LMS, rgb)));
}

float3 F4FS_ClassicFromIctcp(float3 ictcp)
{
	return mul(F4FS_CLASSIC_LMS_TO_BT709, F4FS_ClassicPqDecode(mul(F4FS_CLASSIC_ICTCP_TO_PLMS, ictcp)));
}

float F4FS_ClassicRangeCompress(float v, float threshold, float maxValue)
{
	if (v < threshold) {
		return v;
	}
	if (maxValue <= threshold) {
		return threshold;
	}
	const float range = maxValue - threshold;
	return threshold + range * (1.0 - exp(-(v - threshold) / range));
}

float3 F4FS_ClassicFrostbite(float3 color, float maxValue)
{
	const float rolloffStart = min(0.25, maxValue);
	const float saturationBoost = 0.3;
	const float hueCorrect = 0.6;
	if (max(color.r, max(color.g, color.b)) <= 0.0) {
		return 0.0;
	}
	const float3 ictcp = F4FS_ClassicToIctcp(color);
	const float saturationAmount = pow(smoothstep(1.0, 0.3, ictcp.x), 1.3);
	color = F4FS_ClassicFromIctcp(ictcp * float3(1.0, saturationAmount, saturationAmount));
	const float maxCol = max(color.r, max(color.g, color.b));
	const float3 huePreserving = color * (F4FS_ClassicRangeCompress(maxCol, rolloffStart, maxValue) / maxCol);
	const float3 perChannel = float3(F4FS_ClassicRangeCompress(color.r, rolloffStart, maxValue),
		F4FS_ClassicRangeCompress(color.g, rolloffStart, maxValue),
		F4FS_ClassicRangeCompress(color.b, rolloffStart, maxValue));
	color = lerp(perChannel, huePreserving, hueCorrect);
	float3 mapped = F4FS_ClassicToIctcp(color);
	const float boost = saturationBoost * smoothstep(1.0, 0.5, ictcp.x);
	mapped.yz = lerp(mapped.yz, ictcp.yz * mapped.x / max(1e-3, ictcp.x), boost);
	return F4FS_ClassicFromIctcp(mapped);
}

// renodx HermiteSplineLuminanceRolloff(color, peak, whiteClip) in log2 space (ITU-R BT.2408 knee).
float3 F4FS_ClassicHermite(float3 color, float peak, float whiteClip)
{
	const float y = dot(color, F4FS_CLASSIC_LUMA);
	if (y <= 0.0) {
		return 0.0;
	}
	const float input = log2(y);
	const float lw = log2(whiteClip);
	const float targetWhite = log2(peak);
	const float e1 = input / lw;
	const float maxLum = targetWhite / lw;
	const float knee = 1.5 * maxLum - 0.5;
	const float t = (e1 - knee) / (1.0 - knee);
	const float t2 = t * t;
	const float t3 = t2 * t;
	const float p = (2.0 * t3 - 3.0 * t2 + 1.0) * knee + (t3 - 2.0 * t2 + t) * (1.0 - knee) + (-2.0 * t3 + 3.0 * t2) * maxLum;
	const float e2 = e1 < knee ? e1 : p;
	const float yOut = exp2(min(lw * e2, targetWhite));
	return F4FS_ClassicLumaScale(color, y, yOut);
}

// Scene-linear Rec.709 in, display-linear out in multiples of paper white. Reinhard
// through AgX are SDR curves (they top out at 1); Uchimura, Frostbite and the Hermite
// spline reach the output peak.
float3 F4FS_TonemapClassic(float3 color, uint op, float whitePoint, float outputPeak)
{
	[branch] if (op == F4FS_CLASSIC_REINHARD) {
		const float luma = dot(color, F4FS_CLASSIC_LUMA);
		return saturate(F4FS_ClassicLumaScale(color, luma, luma / (1.0 + luma)));
	} else if (op == F4FS_CLASSIC_REINHARD_EXTENDED) {
		const float luma = dot(color, F4FS_CLASSIC_LUMA);
		return saturate(F4FS_ClassicLumaScale(color, luma, luma * (1.0 + luma / (whitePoint * whitePoint)) / (1.0 + luma)));
	} else if (op == F4FS_CLASSIC_HEJL_BURGESS_DAWSON) {
		return F4FS_ClassicHejl(max(0.0, color - 0.004));
	} else if (op == F4FS_CLASSIC_ALDRIDGE) {
		const float cutoff = 0.19;
		const float3 tmp = 2.0 * cutoff;
		return F4FS_ClassicHejl(color + (tmp - color) * clamp(tmp - color, 0.0, 1.0) * (0.25 / cutoff) - cutoff);
	} else if (op == F4FS_CLASSIC_INSOMNIAC) {
		return saturate(float3(F4FS_ClassicDay(color.r), F4FS_ClassicDay(color.g), F4FS_ClassicDay(color.b)));
	} else if (op == F4FS_CLASSIC_UCHIMURA) {
		return F4FS_ClassicUchimura(color, outputPeak);
	} else if (op == F4FS_CLASSIC_AGX) {
		return F4FS_ClassicAgx(color);
	} else if (op == F4FS_CLASSIC_FROSTBITE) {
		return F4FS_ClassicFrostbite(color, outputPeak);
	}
	// The clip stays above the peak: at the peak itself the log2 knee divides by zero.
	return F4FS_ClassicHermite(color, outputPeak, max(whitePoint, outputPeak + 0.01));
}
