// HLSL source of features/sky/atmo_ms.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "SkyBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
// lane ATMO1: the multiple-scattering LUT (Hillaire 2020 sec. 5.5): per (cos sun zenith, height), 64 directions of
// isotropic 2nd order + the ground, Psi = L2 / (1 - f_ms). "noms" red: the sky pass reads 0 instead.

#define WW_LOOSE_TYPES 1
#include "atmo_common.hlsli"
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "atmo_common.hlsli"
#undef WW_LOOSE_UNIFORMS
	float2 atMsSize;
};
#include "atmo_common.hlsli"

Texture2D<float4> atTransTex : register(t0, space0); SamplerState _atTransTex_sampler : register(s0, space0);

static float4 fragColor;

void frag_main()
{
	float2 uv = gl_FragCoord.xy / atMsSize;
	float mus = atSubToUnit( uv.x, atMsSize.x ) * 2.0 - 1.0;
	float r = kRb + max( 0.01, atMsRowToH( atSubToUnit( uv.y, atMsSize.y ) ) * ( kRt - kRb ) );
	float3 s = float3( sqrt( max( 0.0, 1.0 - mus * mus ) ), 0.0, mus );
	const int K = 8;
	/* lane FIX1101A (atmo): 40 steps packed toward the texel's point (t = d (i / N)^2): the haze layer sits in the first
	 * km of a ray up to 1000 km long, where 20 uniform steps put their first sample 25 km out. Red msold: 20 uniform. */
	bool old = atMsOld > 0.5;
	int N = old ? 20 : 40;
	float3 L2 = (float3)( 0.0 ), F = (float3)( 0.0 );
	for ( int a = 0; a < K; a++ ) {
		for ( int b = 0; b < K; b++ ) {
			float th = 2.0 * kPi * ( float( a ) + 0.5 ) / float( K );
			float ph = acos( 1.0 - 2.0 * ( float( b ) + 0.5 ) / float( K ) );
			float3 v = float3( sin( ph ) * cos( th ), sin( ph ) * sin( th ), cos( ph ) );
			bool gnd = atHitsGround( r, v.z );
			float d = gnd ? atDistGround( r, v.z ) : atDistTop( r, v.z );
			float dt = d / float( N );
			float3 thr = (float3)( 1.0 ), L = (float3)( 0.0 ), f = (float3)( 0.0 );
			for ( int i = 0; i < N; i++ ) {
				float t = ( float( i ) + 0.5 ) * dt;
				if ( !old ) {
					float a0 = float( i ) / float( N ), a1 = float( i + 1 ) / float( N );
					dt = d * ( a1 * a1 - a0 * a0 );
					t = d * 0.5 * ( a0 * a0 + a1 * a1 );
				}
				float3 p = float3( v.xy * t, r + v.z * t );
				float rp = length( p );
				float3 rs, ms, e;
				atMedium( rp - kRb, rs, ms, e );
				float3 Ts = atTrans( atTransTex, _atTransTex_sampler, rp, dot( p / rp, s ) );
				float3 sc = rs + ms;
				float3 Tst = exp( -e * dt );
				float3 ie = 1.0 / max( e, (float3)( 1e-9 ) );
				L += thr * ( Ts * sc / ( 4.0 * kPi ) ) * ( 1.0 - Tst ) * ie;
				f += thr * sc * ( 1.0 - Tst ) * ie;
				thr *= Tst;
			}
			if ( gnd ) {
				float3 pg = float3( v.xy * d, r + v.z * d );
				float3 n = normalize( pg );
				float c = dot( n, s );
				L += thr * atTrans( atTransTex, _atTransTex_sampler, kRb + 1e-4, c ) * max( c, 0.0 ) * kAlbedo / kPi;
			}
			L2 += L;
			F += f;
		}
	}
	L2 /= float( K * K );
	F /= float( K * K );
	fragColor = float4( L2 / max( (float3)( 1e-4 ), 1.0 - F ), 1.0 );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
