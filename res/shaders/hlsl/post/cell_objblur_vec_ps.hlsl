// HLSL source of features/post/cell_objblur_vec.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* lane POSTFX1: per-object motion blur, pass 1 of 4 (src/features/post/cellpost.h). The blur vector of each pixel in PIXELS,
 * current minus previous, times the shutter, clamped to maxPx; the camera's own motion is taken out, so a still
 * world under a turning camera gets 0 (bungo's rule: objects blur, the camera never does).
 * velTex = TAAINPUTS1's velocity (previous minus current, texture units, the camera's reprojection where nothing
 * moved on its own), maskTex.b = 1 where that velocity is an object vector. The camera's vector at the pixel is
 * the game_taa_mv reprojection of the same depth. objRed = 1 (WW_CELL_OBJBLUR_RED=camera): the camera is kept
 * (the whole velocity), the gate's red. */

Texture2D<float4> velTex : register(t0, space0); SamplerState _velTex_sampler : register(s0, space0);
Texture2D<float4> maskTex : register(t1, space0); SamplerState _maskTex_sampler : register(s1, space0);
Texture2D<float4> depthTex : register(t2, space0); SamplerState _depthTex_sampler : register(s2, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	row_major float4x4 reproj;
	float2 jitterNdc;
	float2 invSize;
	float shutter;
	float maxPx;
	int objRed;
};


static float2 blur;

void frag_main()
{
	int2 p = int2( gl_FragCoord.xy );
	float2 uv = gl_FragCoord.xy * invSize;
	float2 vel = velTex.Load( int3(p, 0) ).xy;
	float2 v;
	if ( objRed != 0 ) {
		v = vel;
	} else {
		float d = depthTex.Load( int3(p, 0) ).r;
		float3 ndc = float3( uv * 2.0 - 1.0 - jitterNdc, d * 2.0 - 1.0 );
		float4 q = mul(float4( ndc, 1.0 ), reproj);
		float2 cam = ( q.xy / q.w - ndc.xy ) * 0.5;
		float own = maskTex.Load( int3(p, 0) ).b;
		v = own > 0.5 ? vel - cam : (float2)( 0.0 );
	}
	v = -v / invSize * shutter;	// texture units -> pixels, previous-minus-current -> the way it moved
	float l = length( v );
	if ( !( l < 1e6 ) )
		v = (float2)( 0.0 );
	else if ( l > maxPx )
		v *= maxPx / l;
	blur = v;
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float2 blur : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.blur = blur;
    return stage_output;
}
