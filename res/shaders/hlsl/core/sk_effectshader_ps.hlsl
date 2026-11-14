// HLSL source of sk_effectshader.frag (fragment shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#include "uniforms.hlsli"

Texture2D<float4> BaseMap : register(t0, space0); SamplerState _BaseMap_sampler : register(s0, space0);
Texture2D<float4> GreyscaleMap : register(t1, space0); SamplerState _GreyscaleMap_sampler : register(s1, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	bool hasSourceTexture;
	bool hasGreyscaleMap;
	bool greyscaleAlpha;
	bool greyscaleColor;
	bool useFalloff;
	bool vertexColors;
	bool vertexAlpha;
	bool hasWeaponBlood;
	float4 glowColor;
	float glowMult;
	int alphaFlags;			// bits 0 to 2: alpha test mode, bit 3: alpha blending enabled
	float alphaThreshold;
	float2 uvScale;
	float2 uvOffset;
	float4 falloffParams;
	float falloffDepth;
};


static float3 LightDir;
static float3 ViewDir;

static float2 texCoord;

static float4 C;

static float3 N;

static float4 fragColor;

float4 colorLookup( float x, float y ) {

	return GreyscaleMap.Sample( _GreyscaleMap_sampler, float2( clamp(x, 0.0, 1.0), clamp(y, 0.0, 1.0)) );
}

void frag_main()
{
	float4 baseMap = BaseMap.Sample( _BaseMap_sampler, texCoord.xy * uvScale + uvOffset );

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

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    LightDir = stage_input.LightDir;
    ViewDir = stage_input.ViewDir;
    texCoord = stage_input.texCoord;
    C = stage_input.C;
    N = stage_input.N;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
