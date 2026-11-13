#ifndef F4FS_POST_COMMON_HLSLI
#define F4FS_POST_COMMON_HLSLI

static const uint F4FS_ADAPT_BUCKETS = 256;
static const float F4FS_LOG_LUM_MIN = -12.0;
// The local-exposure grid's log range (AdaptLocalGridCS, AdaptApplyCS).
static const float F4FS_LOG_LUM_MAX = 4.0;
// The meter's (histogram) ceiling, host twin kAdaptationLogLuminanceMax: the sky and the sun count at their own
// brightness instead of piling into a top bucket at 16.
static const float F4FS_METER_LOG_LUM_MAX = 18.0;
static const float F4FS_LUM_EPSILON = 0.0001;

// ONE PASS CARRIES A LONGER BLOCK (2026-09-02). The tonemap pass uploads a
// fifth parameter vector (its path-to-white dial rides in Params[4].x) and
// declares it by defining F4FS_POST_PARAMS_COUNT before this include. Every
// other pass keeps the four-vector block its runtime actually uploads, so no
// shader ever declares a constant block larger than the buffer bound to it.
#ifndef F4FS_POST_PARAMS_COUNT
#define F4FS_POST_PARAMS_COUNT 4
#endif

cbuffer F4FSPostConstants : register(b0)
{
	float4 Timer;
	float4 ScreenSize;
	float4 SceneBlend;
	float4 Params[F4FS_POST_PARAMS_COUNT];
};

float F4FS_Luminance(float3 color)
{
	return max(dot(color, float3(0.2126, 0.7152, 0.0722)), F4FS_LUM_EPSILON);
}

uint F4FS_LogLuminanceBucket(float luminance)
{
	float logLum = clamp(log2(max(luminance, F4FS_LUM_EPSILON)), F4FS_LOG_LUM_MIN, F4FS_METER_LOG_LUM_MAX);
	float normalized = (logLum - F4FS_LOG_LUM_MIN) / (F4FS_METER_LOG_LUM_MAX - F4FS_LOG_LUM_MIN);
	// Bucket b spans [b, b + 1) / BUCKETS of the range, so its centre below is (b + 0.5) / BUCKETS.
	return min((uint)(normalized * F4FS_ADAPT_BUCKETS), F4FS_ADAPT_BUCKETS - 1);
}

float F4FS_BucketCenterLogLuminance(uint bucket)
{
	float normalized = ((float)bucket + 0.5) / (float)F4FS_ADAPT_BUCKETS;
	return lerp(F4FS_LOG_LUM_MIN, F4FS_METER_LOG_LUM_MAX, normalized);
}

#endif
