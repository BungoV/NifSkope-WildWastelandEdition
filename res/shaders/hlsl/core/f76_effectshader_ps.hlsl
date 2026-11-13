// HLSL source of f76_effectshader.frag (fragment shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static bool gl_FrontFacing;
#include "uniforms.hlsli"

Texture2D<float4> BaseMap : register(t0, space0); SamplerState _BaseMap_sampler : register(s0, space0);
Texture2D<float4> GreyscaleMap : register(t1, space0); SamplerState _GreyscaleMap_sampler : register(s1, space0);
TextureCube<float4> CubeMap : register(t2, space0); SamplerState _CubeMap_sampler : register(s2, space0);
Texture2D<float4> NormalMap : register(t3, space0); SamplerState _NormalMap_sampler : register(s3, space0);
Texture2D<float4> EnvironmentMap : register(t4, space0); SamplerState _EnvironmentMap_sampler : register(s4, space0);
Texture2D<float4> ReflMap : register(t5, space0); SamplerState _ReflMap_sampler : register(s5, space0);
Texture2D<float4> LightingMap : register(t6, space0); SamplerState _LightingMap_sampler : register(s6, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	bool hasSourceTexture;
	bool hasGreyscaleMap;
	bool hasCubeMap;
	bool hasNormalMap;
	bool hasEnvMask;
	bool hasSpecularMap;
	bool greyscaleAlpha;
	bool greyscaleColor;
	bool useFalloff;
	bool hasRGBFalloff;
	bool isGlass;
	bool hasWeaponBlood;
	float4 glowColor;
	float glowMult;
	int alphaFlags;			// bits 0 to 2: alpha test mode, bit 3: alpha blending enabled
	float alphaThreshold;
	float2 uvScale;
	float2 uvOffset;
	float4 falloffParams;
	float falloffDepth;
	float lightingInfluence;
	float envReflection;
	float fLumEmittance;
};


static float3 LightDir;
static float3 ViewDir;

static float2 texCoord;

static float4 C;

static float3x3 btnMatrix;
static float3x3 reflMatrix;

static float4 fragColor;

static float3 ViewDir_norm	/* = normalize( ViewDir ) : set in main() after the stage inputs */;
static float3x3 btnMatrix_norm	/* = float3x3( normalize( btnMatrix[0] ), normalize( btnMatrix[1] ), normalize( btnMatrix[2] ) ) : set in main() after the stage inputs */;

float3 fresnelSchlickRoughness(float NdotV, float3 F0, float roughness)
{
	return F0 + (max((float3)(1.0 - roughness), F0) - F0) * pow(1.0 - NdotV, 5.0);
}


void frag_main()
{
	float2 offset = texCoord.xy * uvScale + uvOffset;

	float4 baseMap = BaseMap.Sample( _BaseMap_sampler, offset );
	float4 normalMap = NormalMap.Sample( _NormalMap_sampler, offset );
	float4 reflMap = ReflMap.Sample(_ReflMap_sampler, offset);

	float3 normal = normalMap.rgb;
	// Calculate missing blue channel
	normal.b = sqrt(max(1.0 - dot(normal.rg, normal.rg), 0.0));
	normal = normalize( mul(normal, btnMatrix_norm) );
	if ( !gl_FrontFacing )
		normal *= -1.0;

	float3 f0 = reflMap.rgb;
	float3 L = normalize(LightDir);
	float3 V = ViewDir_norm;
	float3 R = reflect(-V, normal);
	float3 H = normalize( L + V );

	float NdotL = max( dot(normal, L), 0.000001 );
	float NdotH = max( dot(normal, H), 0.000001 );
	float NdotV = max( abs(dot(normal, V)), 0.000001 );
	float LdotH = max( dot(L, H), 0.000001 );
	float NdotNegL = max( dot(normal, -L), 0.000001 );

	float3 reflectedWS = mul(R, reflMatrix);

	if ( greyscaleAlpha )
		baseMap.a = 1.0;

	float4 baseColor = glowColor;
	if ( !greyscaleColor )
		baseColor.rgb *= glowMult;

	// Falloff
	float falloff = 1.0;
	if ( or( useFalloff, hasRGBFalloff ) ) {
		if ( falloffParams.y > (falloffParams.x + 0.000001) )
			falloff = smoothstep(falloffParams.x, falloffParams.y, NdotV);
		else if ( falloffParams.x > (falloffParams.y + 0.000001) )
			falloff = 1.0 - smoothstep(falloffParams.y, falloffParams.x, NdotV);
		else
			falloff = 0.5;
		falloff = clamp(lerp(falloffParams.z, falloffParams.w, falloff), 0.0, 1.0);

		if ( useFalloff )
			baseMap.a *= falloff;

		if ( hasRGBFalloff )
			baseMap.rgb *= falloff;
	}

	float alphaMult = baseColor.a * baseColor.a;

	float4 color = baseMap * C;
	color.rgb *= baseColor.rgb;
	color.a *= alphaMult;

	if ( greyscaleColor )
		color.rgb = GreyscaleMap.SampleLevel( _GreyscaleMap_sampler, float2( BaseMap.Sample( _BaseMap_sampler, offset ).g, baseColor.r * C.r * falloff ), 0.0 ).rgb;

	if ( greyscaleAlpha )
		color.a = GreyscaleMap.SampleLevel( _GreyscaleMap_sampler, float2( BaseMap.Sample( _BaseMap_sampler, offset ).a, color.a ), 0.0 ).a;

	if ( alphaFlags > 0 ) {
		// 0: always, 1: <, 2: ==, 3: <=, 4: >, 5: !=, 6: >=, 7: never
		int	m = ( color.a < alphaThreshold ? 0x2B2B : ( color.a > alphaThreshold ? 0x7171 : 0x4D4D ) );
		if ( ( m & ( 1 << alphaFlags ) ) == 0 )
			discard;
	}
	if ( alphaFlags < 8 )
		color.a = 1.0;

	float3 diffuse = lightSourceAmbient.rgb + (lightSourceDiffuse[0].rgb * NdotL);
	color.rgb = lerp( color.rgb, color.rgb * lightSourceDiffuse[0].rgb, lightingInfluence );

	// Specular
	float g = 1.0;
	float s = 1.0;
	if ( hasSpecularMap && !isGlass ) {
		float4 lightingMap = LightingMap.Sample(_LightingMap_sampler, offset);
		s = lightingMap.r;
		g = lightingMap.g;
	}
	float roughness = 1.0 - s;

	// Environment
	if ( hasCubeMap ) {
		float	m = roughness * (roughness * -4.0 + 10.0);
		float3	cube = CubeMap.SampleLevel( _CubeMap_sampler, reflectedWS, max(m, 0.0) ).rgb;
		cube *= envReflection * g;
		cube = lerp( cube, cube * lightSourceDiffuse[0].rgb, lightingInfluence );
		if ( hasEnvMask )
			cube *= EnvironmentMap.Sample( _EnvironmentMap_sampler, offset ).rgb;
		color.rgb += cube * falloff;
	}

	fragColor = float4( color.rgb * brightnessScale, color.a );
}

struct SPIRV_Cross_Input
{
    WW_LOC(3) float3 LightDir : TEXCOORD3;
    WW_LOC(4) float3 ViewDir : TEXCOORD4;
    WW_LOC(8) float2 texCoord : TEXCOORD8;
    WW_LOC(1) float4 C : TEXCOORD1;
    WW_LOC(5) float3x3 btnMatrix : TEXCOORD5;
    WW_LOC(9) nointerpolation float3x3 reflMatrix : TEXCOORD9;
    bool gl_FrontFacing : SV_IsFrontFace;
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
    btnMatrix = stage_input.btnMatrix;
    reflMatrix = stage_input.reflMatrix;
    gl_FrontFacing = stage_input.gl_FrontFacing;
    ViewDir_norm = normalize( ViewDir );
    btnMatrix_norm = float3x3( normalize( btnMatrix[0] ), normalize( btnMatrix[1] ), normalize( btnMatrix[2] ) );
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
