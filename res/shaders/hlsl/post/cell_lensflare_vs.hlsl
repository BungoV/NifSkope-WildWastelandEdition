// HLSL source of features/post/cell_lensflare.vert (vertex shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
static float4 gl_Position;
/* lane LIGHTS3 (LENSFLARE1): one lens-flare sprite corner. The CPU places the quad (centre + corner offset, NDC);
 * here the light's own screen point is tested against the opaque depth (a 5 x 5 kernel, 2 px apart: inferred,
 * the game averages its own visibility test over 4 frames) and the share seen scales the colour, and the size too
 * when the sprite Shrinks When Occluded. */

static float4 vertexPosition;	// centre xy, corner offset xy (NDC)
static float4 vertexColor;	// colour rgb, shrinks (0 / 1)
static float4 vertexUV;	// u, v, 0, 0
static float4 vertexLight;	// the light's NDC x, y and its view depth (game units, positive)

Texture2D<float4> flareDepth : register(t0, space0); SamplerState _flareDepth_sampler : register(s0, space0);	// the opaque pass's depth, the viewport's corner at texel 0
cbuffer LooseUniforms : register(b31, space0)
{
	row_major float4x4 flareProj;
	float2 flareViewport;	// pixels
	float flareSlack;	// view units a surface may sit in front of the light and still let it show
	int flareRed;		// 1 noocclude
};


static float2 flareUV;
static float3 flareColor;

void vert_main()
{
	float seen = 1.0;
	if ( ( flareRed & 1 ) == 0 ) {
		int2 c = int2( ( vertexLight.xy * 0.5 + 0.5 ) * flareViewport );
		int2 hi = int2( flareViewport ) - 1;
		float n = 0.0;
		for ( int y = -2; y <= 2; y++ ) {
			for ( int x = -2; x <= 2; x++ ) {
				int2 p = clamp( c + int2( x, y ) * 2, (int2)( 0 ), hi );
				float d = flareDepth.Load( int3(p, 0) ).r;
				float sceneW = flareProj[3][2] / ( flareProj[2][2] + ( 2.0 * d - 1.0 ) );
				n += ( or( d >= 1.0, sceneW + flareSlack >= vertexLight.z ) ) ? 1.0 : 0.0;
			}
		}
		seen = n / 25.0;
	}
	float size = vertexColor.a > 0.5 ? seen : 1.0;
	gl_Position = float4( vertexPosition.xy + vertexPosition.zw * size, 0.0, 1.0 );
	flareUV = vertexUV.xy;
	flareColor = vertexColor.rgb * seen;
}

struct SPIRV_Cross_Input
{
    float4 vertexPosition : TEXCOORD0;
    float4 vertexColor : TEXCOORD1;
    float4 vertexUV : TEXCOORD2;
    float4 vertexLight : TEXCOORD3;
};

struct SPIRV_Cross_Output
{
    float2 flareUV : TEXCOORD0;
    float3 flareColor : TEXCOORD1;
    float4 gl_Position : SV_Position;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vertexPosition = stage_input.vertexPosition;
    vertexColor = stage_input.vertexColor;
    vertexUV = stage_input.vertexUV;
    vertexLight = stage_input.vertexLight;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.flareUV = flareUV;
    stage_output.flareColor = flareColor;
    return stage_output;
}
