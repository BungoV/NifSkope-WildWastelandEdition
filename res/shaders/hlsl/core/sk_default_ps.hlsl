// HLSL source of sk_default.frag (fragment shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#include "uniforms.hlsli"

Texture2D<float4> BaseMap : register(t0, space0); SamplerState _BaseMap_sampler : register(s0, space0);
Texture2D<float4> NormalMap : register(t1, space0); SamplerState _NormalMap_sampler : register(s1, space0);
Texture2D<float4> GlowMap : register(t2, space0); SamplerState _GlowMap_sampler : register(s2, space0);
Texture2D<float4> HeightMap : register(t3, space0); SamplerState _HeightMap_sampler : register(s3, space0);
Texture2D<float4> LightMask : register(t4, space0); SamplerState _LightMask_sampler : register(s4, space0);
Texture2D<float4> BacklightMap : register(t5, space0); SamplerState _BacklightMap_sampler : register(s5, space0);
Texture2D<float4> EnvironmentMap : register(t6, space0); SamplerState _EnvironmentMap_sampler : register(s6, space0);
TextureCube<float4> CubeMap : register(t7, space0); SamplerState _CubeMap_sampler : register(s7, space0);
cbuffer LooseUniforms : register(b31, space0)
{
	float3 specColor;
	float specStrength;
	float specGlossiness;
	bool useGloss;
	bool hasGlowMap;
	float3 glowColor;
	float glowMult;
	float alpha;
	int alphaFlags;			// bits 0 to 2: alpha test mode, bit 3: alpha blending enabled
	float alphaThreshold;
	float3 tintColor;
	bool hasHeightMap;
	float2 uvScale;
	float2 uvOffset;
	bool hasEmit;
	bool hasSoftlight;
	bool hasBacklight;
	bool hasRimlight;
	bool hasTintColor;
	bool hasCubeMap;
	bool hasEnvMask;
	float lightingEffect1;
	float lightingEffect2;
	float envReflection;
};


static float3 LightDir;
static float3 ViewDir;

static float2 texCoord;

static float4 A;
static float4 C;
static float4 D;

static float3x3 btnMatrix;

static float4 fragColor;

static float3x3 btnMatrix_norm	/* = float3x3(normalize(btnMatrix[0]), normalize(btnMatrix[1]), normalize(btnMatrix[2])) : set in main() after the stage inputs */;


float3 tonemap(float3 x)
{
	float a = 0.15;
	float b = 0.50;
	float c = 0.10;
	float d = 0.20;
	float e = 0.02;
	float f = 0.30;

	float3 z = x * x * D.a * (A.a * 4.22978723);
	z = (z * (a * z + b * c) + d * e) / (z * (a * z + b) + d * f) - e / f;
	return sqrt(z / (A.a * 0.93333333));
}

float3 toGrayscale(float3 color)
{
	return (float3)(dot(float3(0.3, 0.59, 0.11), color));
}

void frag_main()
{
	float2 offset = texCoord.xy * uvScale + uvOffset;

	float3 E = normalize(ViewDir);
	
	if ( hasHeightMap ) {
		float height = HeightMap.Sample( _HeightMap_sampler, offset ).r;
		offset += normalize(mul(btnMatrix_norm, ViewDir)).xy * (height * 0.08 - 0.04); 
	}

	float4 baseMap = BaseMap.Sample( _BaseMap_sampler, offset );

	float4 color = float4( baseMap.rgb, 1.0 );
	if ( alphaFlags > 0 ) {
		float	a = C.a * baseMap.a * alpha;
		// 0: always, 1: <, 2: ==, 3: <=, 4: >, 5: !=, 6: >=, 7: never
		int	m = ( a < alphaThreshold ? 0x2B2B : ( a > alphaThreshold ? 0x7171 : 0x4D4D ) );
		if ( ( m & ( 1 << alphaFlags ) ) == 0 )
			discard;
		if ( ( alphaFlags & 8 ) != 0 )
			color.a = a;
	}

	float4 normalMap = NormalMap.Sample( _NormalMap_sampler, offset );
	float4 glowMap = GlowMap.Sample( _GlowMap_sampler, offset );
	
	float3 normal = normalize(mul((normalMap.rgb * 2.0 - 1.0), btnMatrix_norm));

	float3 L = normalize(LightDir);
	float3 R = reflect(-L, normal);
	float3 H = normalize( L + E );
	
	float NdotL = max( dot(normal, L), 0.0 );
	float NdotH = max( dot(normal, H), 0.0 );
	float EdotN = max( dot(normal, E), 0.0 );
	float NdotNegL = max( dot(normal, -L), 0.0 );

	float3 reflected = reflect( -E, normal );
	float3 reflectedWS = mul(reflected, envMapRotation);


	float3 albedo = baseMap.rgb * C.rgb;
	float3 diffuse = A.rgb + (D.rgb * NdotL);


	// Environment
	if ( hasCubeMap ) {
		float4 cube = CubeMap.Sample( _CubeMap_sampler, reflectedWS );
		cube.rgb *= envReflection;
		
		if ( hasEnvMask ) {
			float4 env = EnvironmentMap.Sample( _EnvironmentMap_sampler, offset );
			cube.rgb *= env.r;
		} else {
			cube.rgb *= normalMap.a;
		}
		

		albedo += cube.rgb;
	}

	// Emissive & Glow
	float3 emissive = (float3)(0.0);
	if ( hasEmit ) {
		emissive += glowColor * glowMult;
		
		if ( hasGlowMap ) {
			emissive *= glowMap.rgb;
		}
	}

	// Specular
	float3 spec = clamp( specColor * specStrength * normalMap.a * pow(NdotH, useGloss ? specGlossiness : 1.0), 0.0, 1.0 );
	spec *= D.rgb;

	float3 backlight = (float3)(0.0);
	if ( hasBacklight ) {
		backlight = BacklightMap.Sample( _BacklightMap_sampler, offset ).rgb;
		backlight *= NdotNegL;
		
		emissive += backlight * D.rgb;
	}

	float4 mask = (float4)(0.0);
	if ( or( hasRimlight, hasSoftlight ) ) {
		mask = LightMask.Sample( _LightMask_sampler, offset );
	}

	float3 rim = (float3)(0.0);
	if ( hasRimlight ) {
		rim = mask.rgb * pow((float3)((1.0 - EdotN)), (float3)(lightingEffect2));
		rim *= smoothstep( -0.2, 1.0, dot(-L, E) );
		
		emissive += rim * D.rgb;
	}
	
	float3 soft = (float3)(0.0);
	if ( hasSoftlight ) {
		float wrap = (dot(normal, L) + lightingEffect1) / (1.0 + lightingEffect1);

		soft = max( wrap, 0.0 ) * mask.rgb * smoothstep( 1.0, 0.0, NdotL );
		soft *= sqrt( clamp( lightingEffect1, 0.0, 1.0 ) );
		
		emissive += soft * D.rgb;
	}
	
	if ( hasTintColor ) {
		albedo *= tintColor;
	}

	color.rgb = albedo * (diffuse + emissive * glowScaleSRGB) + spec;
	color.rgb = tonemap( color.rgb );

	fragColor = color;
}

struct SPIRV_Cross_Input
{
    WW_LOC(3) float3 LightDir : TEXCOORD3;
    WW_LOC(4) float3 ViewDir : TEXCOORD4;
    WW_LOC(8) float2 texCoord : TEXCOORD8;
    WW_LOC(0) nointerpolation float4 A : TEXCOORD0;
    WW_LOC(1) float4 C : TEXCOORD1;
    WW_LOC(2) nointerpolation float4 D : TEXCOORD2;
    WW_LOC(5) float3x3 btnMatrix : TEXCOORD5;
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
    A = stage_input.A;
    C = stage_input.C;
    D = stage_input.D;
    btnMatrix = stage_input.btnMatrix;
    btnMatrix_norm = float3x3(normalize(btnMatrix[0]), normalize(btnMatrix[1]), normalize(btnMatrix[2]));
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
