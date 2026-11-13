// HLSL source of sk_effectshader.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_PS 1

#include "uniforms.src.hlsli"

Texture2D<float4> BaseMap; SamplerState _BaseMap_sampler;
Texture2D<float4> GreyscaleMap; SamplerState _GreyscaleMap_sampler;

uniform bool hasSourceTexture;
uniform bool hasGreyscaleMap;
uniform bool greyscaleAlpha;
uniform bool greyscaleColor;

uniform bool useFalloff;
uniform bool vertexColors;
uniform bool vertexAlpha;

uniform bool hasWeaponBlood;

uniform float4 glowColor;
uniform float glowMult;

uniform int alphaFlags;			// bits 0 to 2: alpha test mode, bit 3: alpha blending enabled
uniform float alphaThreshold;

uniform float2 uvScale;
uniform float2 uvOffset;

uniform float4 falloffParams;
uniform float falloffDepth;

static float3 LightDir;
static float3 ViewDir;

static float2 texCoord;

static float4 C;

static float3 N;

static float4 fragColor;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    WW_LOC(3) float3 LightDir : TEXCOORD3;
    WW_LOC(4) float3 ViewDir : TEXCOORD4;
    WW_LOC(8) float2 texCoord : TEXCOORD8;
    WW_LOC(1) float4 C : TEXCOORD1;
    WW_LOC(5) float3 N : TEXCOORD5;
};
struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};


float4 colorLookup( float x, float y ) {

	return GreyscaleMap.Sample(_GreyscaleMap_sampler, float2( clamp(x, 0.0, 1.0), clamp(y, 0.0, 1.0)));
}

void frag_main()
{
	float4 baseMap = BaseMap.Sample(_BaseMap_sampler, texCoord.xy * uvScale + uvOffset);

	float4 color;

	float3 normal = normalize( N );

	// Reconstructed normal
	//normal = normalize(cross(dFdy(v.xyz), dFdx(v.xyz)));

	float3 E = normalize(ViewDir);

	float tmp2 = falloffDepth; // Unused right now

	// Falloff
	float falloff = 1.0;
	if ( useFalloff ) {
		float startO = min(falloffParams.z, 1.0);
		float stopO = max(falloffParams.w, 0.0);

		// TODO: When X and Y are both 0.0 or both 1.0 the effect is reversed.
		falloff = smoothstep( falloffParams.y, falloffParams.x, abs(E.b));

		falloff = lerp( max(falloffParams.w, 0.0), min(falloffParams.z, 1.0), falloff );
	}

	float alphaMult = glowColor.a * glowColor.a;

	color = baseMap;

	if ( hasWeaponBlood ) {
		color.rgb = float3( 1.0, 0.0, 0.0 ) * baseMap.r;
		color.a = baseMap.a * baseMap.g;
	}

	color *= C;
	color.rgb *= glowColor.rgb;
	color.a *= falloff * alphaMult;

	if ( greyscaleColor ) {
		// Only Red emissive channel is used
		float emRGB = glowColor.r;

		float4 luG = colorLookup( baseMap.g, C.g * falloff * emRGB );

		color.rgb = luG.rgb;
	}

	if ( greyscaleAlpha ) {
		float4 luA = colorLookup( baseMap.a, C.a * falloff * alphaMult );

		color.a = luA.a;
	}

	if ( alphaFlags > 0 ) {
		// 0: always, 1: <, 2: ==, 3: <=, 4: >, 5: !=, 6: >=, 7: never
		int	m = ( color.a < alphaThreshold ? 0x2B2B : ( color.a > alphaThreshold ? 0x7171 : 0x4D4D ) );
		if ( ( m & ( 1 << alphaFlags ) ) == 0 )
			discard;
	}
	if ( alphaFlags < 8 )
		color.a = 1.0;

	fragColor = float4( color.rgb * ( glowMult * sqrt(brightnessScale) ), color.a );
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    texCoord = stage_input.texCoord;
    N = stage_input.N;
    ViewDir = stage_input.ViewDir;
    C = stage_input.C;
    LightDir = stage_input.LightDir;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
