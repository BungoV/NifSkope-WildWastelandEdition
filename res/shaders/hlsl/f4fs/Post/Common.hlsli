#ifndef F4FX_POST_COMMON_HLSLI
#define F4FX_POST_COMMON_HLSLI

static const uint F4FX_ADAPT_BUCKETS = 128;
static const float F4FX_LOG_LUM_MIN = -12.0;
static const float F4FX_LOG_LUM_MAX = 4.0;
static const float F4FX_LUM_EPSILON = 0.0001;

// ONE PASS CARRIES A LONGER BLOCK (2026-09-02). The tonemap pass uploads a
// fifth parameter vector (its path-to-white dial rides in Params[4].x) and
// declares it by defining F4FX_POST_PARAMS_COUNT before this include. Every
// other pass keeps the four-vector block its runtime actually uploads, so no
// shader ever declares a constant block larger than the buffer bound to it.
#ifndef F4FX_POST_PARAMS_COUNT
#define F4FX_POST_PARAMS_COUNT 4
#endif

cbuffer F4FXPostConstants : register(b0)
{
	float4 Timer;
	float4 ScreenSize;
	float4 SceneBlend;
	float4 Params[F4FX_POST_PARAMS_COUNT];
};

float F4FX_Luminance(float3 color)
{
	return max(dot(color, float3(0.2126, 0.7152, 0.0722)), F4FX_LUM_EPSILON);
}

uint F4FX_LogLuminanceBucket(float luminance)
{
	float logLum = clamp(log2(max(luminance, F4FX_LUM_EPSILON)), F4FX_LOG_LUM_MIN, F4FX_LOG_LUM_MAX);
	float normalized = (logLum - F4FX_LOG_LUM_MIN) / (F4FX_LOG_LUM_MAX - F4FX_LOG_LUM_MIN);
	return min((uint)(normalized * (F4FX_ADAPT_BUCKETS - 1)), F4FX_ADAPT_BUCKETS - 1);
}

float F4FX_BucketCenterLogLuminance(uint bucket)
{
	float normalized = ((float)bucket + 0.5) / (float)F4FX_ADAPT_BUCKETS;
	return lerp(F4FX_LOG_LUM_MIN, F4FX_LOG_LUM_MAX, normalized);
}

#endif
