// HLSL source of features/sky/atmo_ap.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "SkyBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
// lane SKYA (ATMO2): the aerial-perspective froxel volume (Hillaire 2020 sec. 5.4), one slice a draw. Per froxel the
// light the air sends toward the eye from the camera to the slice's distance (rgb, scene units: pi k L as the sky)
// and the transmittance over the same stretch (a, the mean of the three channels as Hillaire stores it). Slices are
// quadratic in distance: slice k's centre sits at maxKm ((k + 0.5) / n)^2. The same medium, the same transmittance
// and multiple-scattering LUTs as the sky-view: distant geometry takes exactly the sky's haze and in-scatter.

#define WW_LOOSE_TYPES 1
#include "atmo_common.hlsli"
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "atmo_common.hlsli"
#undef WW_LOOSE_UNIFORMS
	float2 atMsSize;
	float3 atApDims;	// width, height, slices
#if !(defined(WW_AP_LAYERED))
	float atApSlice;	// this draw's slice index
#endif
	float atApMaxKm;
	float3 atFrustum0;	// world ray (not unit) at the viewport's lower-left corner
	float3 atFrustumX;	// its change across the viewport's width
	float3 atFrustumY;	// and height
	float atCamR;	// km from the planet centre
	float3 atSunW;	// world, TO the sun
	float3 atMoonW;
	float atSunK;	// pi k for the sun (atmosphere.cpp)
	float atMoonK;	// 0 = no moon term
	float atMsOn;
	float atApNoSunT;	// red "nosunt": the in-scatter ignores the sun's transmittance (must fail the AP gate)
	float atTintOn;	// SKY2B (SKYTINT2): 1 = the Sky Color row's Weather Tint (0 under red WW_ATMO_TINT_RED=physical)
	float3 atTint[17];	// the weather dome's linear color at 90 (i / 16)^2 degrees (gl/atmomath.h tintTable)
};
#include "atmo_common.hlsli"

Texture2D<float4> atTransTex : register(t0, space0); SamplerState _atTransTex_sampler : register(s0, space0);
Texture2D<float4> atMsTex : register(t1, space0); SamplerState _atMsTex_sampler : register(s1, space0);
#ifdef WW_AP_LAYERED
static float atApSliceIn;	// lane PERFFIX3: the slice from atmo_apl.geom (one layered draw, gl_Layer)
#define atApSlice atApSliceIn
#else

#endif


static float4 fragColor;

float3 atLightS( float3 up, float rp, float3 rs, float3 ms, float3 v, float3 l, float k )
{
	float mus = dot( up, l );
	float c = dot( v, l );
	float3 Ts = atApNoSunT > 0.5 ? (float3)( 1.0 ) : atTrans( atTransTex, _atTransTex_sampler, rp, mus );
	return k * ( Ts * ( rs * atPhaseR( c ) + ms * atPhaseHg( atG, c ) )
		+ atMsOn * atMsTex.Sample( _atMsTex_sampler, atMsUv( rp, mus, atMsSize ) ).rgb * ( rs + ms ) );
}

// atmo_sky.frag atWeatherTint's twin (gl/atmomath.h weatherTint): the luminance kept, the dome's color at this elevation
float3 atWeatherTint( float3 c, float elevDeg )
{
	float t = sqrt( clamp( elevDeg, 0.0, 90.0 ) / 90.0 ) * 16.0;
	int i = min( int( t ), 15 );
	float3 w = lerp( atTint[i], atTint[i + 1], t - float( i ) );
	const float3 y = float3( 0.2126, 0.7152, 0.0722 );
	float lw = dot( w, y );
	return lw > 1e-8 ? w * ( dot( c, y ) / lw ) : c;
}

void frag_main()
{
	float2 uv = gl_FragCoord.xy / atApDims.xy;
	float3 v = normalize( atFrustum0 + uv.x * atFrustumX + uv.y * atFrustumY );
	float z = ( atApSlice + 0.5 ) / atApDims.z;
	float D = atApMaxKm * z * z;
	const int N = 16;
	float3 thr = (float3)( 1.0 ), L = (float3)( 0.0 );
	float t0 = 0.0;
	for ( int i = 0; i < N; i++ ) {
		float a = float( i + 1 ) / float( N );
		float t1 = D * a * a;	// packed toward the camera: the haze layer can be ~100 m thick
		float dt = t1 - t0;
		float t = t0 + dt * 0.5;
		t0 = t1;
		float3 p = float3( v.xy * t, atCamR + v.z * t );
		float rp = length( p );
		float3 up = p / rp;
		float3 rs, ms, e;
		atMedium( rp - kRb, rs, ms, e );
		float3 S = atLightS( up, rp, rs, ms, v, atSunW, atSunK );
		if ( atMoonK > 0.0 )
			S += atLightS( up, rp, rs, ms, v, atMoonW, atMoonK );
		float3 Tst = exp( -e * dt );
		L += thr * S * ( 1.0 - Tst ) / max( e, (float3)( 1e-9 ) );
		thr *= Tst;
	}
	if ( atTintOn > 0.5 )
		L = atWeatherTint( L, degrees( asin( clamp( v.z, -1.0, 1.0 ) ) ) );
	fragColor = float4( L, dot( thr, (float3)( 1.0 / 3.0 ) ) );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
#if defined(WW_AP_LAYERED)
    nointerpolation float atApSliceIn : TEXCOORD0;
#endif
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
#if defined(WW_AP_LAYERED)
    atApSliceIn = stage_input.atApSliceIn;
#endif
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
