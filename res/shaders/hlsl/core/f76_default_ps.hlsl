// HLSL source of f76_default.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_PS 1
static bool gl_FrontFacing;

#include "uniforms.src.hlsli"

Texture2D<float4> BaseMap; SamplerState _BaseMap_sampler;
Texture2D<float4> NormalMap; SamplerState _NormalMap_sampler;
Texture2D<float4> GlowMap; SamplerState _GlowMap_sampler;
Texture2D<float4> ReflMap; SamplerState _ReflMap_sampler;
Texture2D<float4> LightingMap; SamplerState _LightingMap_sampler;
Texture2D<float4> GreyscaleMap; SamplerState _GreyscaleMap_sampler;
Texture2D<float4> EnvironmentMap; SamplerState _EnvironmentMap_sampler;	// environment BRDF LUT
TextureCube<float4> CubeMap; SamplerState _CubeMap_sampler;	// pre-filtered cube maps (specular, diffuse)
TextureCube<float4> CubeMap2; SamplerState _CubeMap2_sampler;

uniform float3 specColor;
uniform bool hasSpecular;
uniform float specStrength;
uniform float fresnelPower;

uniform float paletteScale;

uniform float3 glowColor;
uniform float glowMult;

uniform float alpha;
uniform int alphaFlags;			// bits 0 to 2: alpha test mode, bit 3: alpha blending enabled
uniform float alphaThreshold;

uniform float3 tintColor;

uniform float2 uvScale;
uniform float2 uvOffset;

uniform bool hasEmit;
uniform bool hasGlowMap;
uniform bool hasSoftlight;
uniform bool hasTintColor;
uniform bool hasCubeMap;
uniform bool hasSpecularMap;
uniform bool greyscaleColor;

// RGB = subsurface tint color, A = transmissive scale (> 0: translucency enabled)
uniform float4 translucencyColorAndScale;

uniform float envReflection;

static float3 LightDir;
static float3 ViewDir;

static float2 texCoord;

static float4 C;

static float3x3 btnMatrix;
static float3x3 reflMatrix;

static float4 fragColor;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
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


static float3 ViewDir_norm;
static float3x3 btnMatrix_norm;

#ifndef M_PI
	#define M_PI 3.1415926535897932384626433832795
#endif

#define FLT_EPSILON 1.192092896e-07F // smallest such that 1.0 + FLT_EPSILON != 1.0

float3 LightingFuncGGX_REF(float NdotL, float NdotH, float NdotV, float roughness)
{
	float alpha = roughness * roughness;
	// D (GGX normal distribution)
	float alphaSqr = alpha * alpha;
	float denom = NdotH * NdotH;
	denom = ( denom * alphaSqr ) + ( 1.0 - denom );
	float D = alphaSqr / ( denom * denom * 4.0 );
	// no pi because BRDF -> lighting
	// G (roughness is not remapped)
	float	k = alpha * 0.5;
	float	G = NdotL / (lerp(NdotL, 1.0, k) * lerp(NdotV, 1.0, k));

	return (float3)(D * G);
}

float3 tonemap(float3 x, float y)
{
	float a = 0.15;
	float b = 0.50;
	float c = 0.10;
	float d = 0.20;
	float e = 0.02;
	float f = 0.30;

	float3 z = x * (y * 4.22978723);
	z = (z * (a * z + b * c) + d * e) / (z * (a * z + b) + d * f) - e / f;
	return z / (y * 0.93333333);
}

float srgbCompress(float x)
{
	float	y = sqrt(max(x, 0.0) + 0.000858025);
	return (((y * -0.18732371 + 0.59302883) * y - 0.79095451) * y + 1.42598062) * y - 0.04110602;
}

void frag_main()
{
	float2 offset = texCoord.xy * uvScale + uvOffset;

	float4	baseMap = BaseMap.Sample(_BaseMap_sampler, offset);

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

	float4	normalMap = NormalMap.Sample(_NormalMap_sampler, offset);
	float4	lightingMap = float4(0.25, 1.0, 0.0, 1.0);
	if ( hasSpecularMap )
		lightingMap = LightingMap.Sample(_LightingMap_sampler, offset);
	float4	reflMap = ReflMap.Sample(_ReflMap_sampler, offset);
	float4	glowMap = GlowMap.Sample(_GlowMap_sampler, offset);

	float3 normal = normalMap.rgb;
	// Calculate missing blue channel
	normal.b = sqrt(max(1.0 - dot(normal.rg, normal.rg), 0.0));
	normal = normalize( mul(normal, btnMatrix_norm) );
	if ( !gl_FrontFacing )
		normal *= -1.0;

	float3 L = normalize(LightDir);
	float3 V = ViewDir_norm;
	float3 R = reflect(-V, normal);
	float3 H = normalize(L + V);

	float NdotL = dot(normal, L);
	float NdotL0 = max(NdotL, 0.0);
	float NdotH = clamp(dot(normal, H), 0.0, 1.0);
	float NdotV = abs(dot(normal, V));
	float LdotH = dot(L, H);

	float3	reflectedWS = mul(R, reflMatrix);
	float3	normalWS = mul(normal, reflMatrix);

	float3 albedo = baseMap.rgb * C.rgb;
	if ( greyscaleColor ) {
		// work around incorrect format used by [reference game] grayscale textures
		albedo = GreyscaleMap.SampleLevel(_GreyscaleMap_sampler, float2(srgbCompress(baseMap.g), paletteScale * C.r), 0.0).rgb;
	}

	// Emissive
	float3 emissive = (float3)(0.0);
	if ( hasEmit ) {
		emissive += glowColor * glowMult;
		if ( hasGlowMap ) {
			emissive *= glowMap.rgb;
		}
	} else if ( hasGlowMap ) {
		emissive += glowMap.rgb * glowMult;
	}
	emissive *= lightingMap.a * glowScale;

	float3	f0 = max(reflMap.rgb, (float3)(0.02));

	// Specular
	float	roughness = 1.0 - lightingMap.r;
	float3	spec = LightingFuncGGX_REF(NdotL0, NdotH, NdotV, max(roughness, 0.02)) * lightSourceDiffuse[0].rgb;

	// Diffuse
	float3	diffuse = (float3)(NdotL0);
	// Fresnel
	float2	fDirect = EnvironmentMap.SampleLevel(_EnvironmentMap_sampler, float2(LdotH, NdotL0), 0.0).ba;
	spec *= lerp(f0, (float3)(1.0), fDirect.x);
	float4	envLUT = EnvironmentMap.SampleLevel(_EnvironmentMap_sampler, float2(NdotV, roughness), 0.0);
	float2	fDiff = float2(fDirect.y, envLUT.b);
	fDiff = fDiff * (LdotH * LdotH * roughness * 2.0 - 0.5) + 1.0;
	diffuse *= ((float3)(1.0) - f0) * fDiff.x * fDiff.y;

	// Environment
	float3	refl = (float3)(0.0);
	float3	ambient = lightSourceAmbient.rgb;
	if ( hasCubeMap ) {
		float	m = roughness * (roughness * -4.0 + 10.0);
		refl = CubeMap.SampleLevel(_CubeMap_sampler, reflectedWS, max(m, 0.0)).rgb;
		refl *= envReflection * specStrength;
		refl *= ambient;
		ambient *= CubeMap2.SampleLevel(_CubeMap2_sampler, normalWS, 0.0).rgb;
	} else {
		ambient *= 0.06666667;
		refl = ambient;
	}
	float3	f = lerp(f0, (float3)(1.0), envLUT.r);
	if (!hasSpecular) {
		albedo = max(albedo, reflMap.rgb);
		diffuse = (float3)(NdotL0);
		spec = (float3)(0.0);
		f = (float3)(0.0);
	} else {
		float	fDiffEnv = envLUT.b * ((NdotV + 1.0) * roughness - 0.5) + 1.0;
		ambient *= ((float3)(1.0) - f0) * fDiffEnv;
	}
	float	ao = lightingMap.g;
	float	specOcc = max( ( ao - 1.0 ) * ( ( NdotV * 1.125 - 2.625 ) * NdotV + 2.5 ) + 1.0, 0.0 );
	// apply AO to all diffuse lighting, and specular occlusion to indirect specular only
	albedo *= ao;
	refl *= f * envLUT.g * specOcc;

	//if ( hasTintColor ) {
	//	albedo *= tintColor;
	//}

	// Diffuse
	color.rgb = diffuse * albedo * lightSourceDiffuse[0].rgb;
	// Ambient
	color.rgb += ambient * albedo;
	// Specular
	color.rgb += spec;
	color.rgb += refl;

	// Emissive
	color.rgb += emissive;

	// Transmissive
	if ( translucencyColorAndScale.a > 0.0 ) {
		float3	transmissive = albedo * translucencyColorAndScale.rgb * ( (float3)(1.0) - f );
		transmissive *= translucencyColorAndScale.a * lightingMap.b;
		color.rgb += transmissive * lightSourceDiffuse[0].rgb * max( -NdotL, 0.0 );
		if ( hasCubeMap )
			color.rgb += CubeMap2.SampleLevel(_CubeMap2_sampler, -normalWS, 0.0).rgb * transmissive * lightSourceAmbient.rgb;
		else
			color.rgb += transmissive * lightSourceAmbient.rgb * 0.06666667;
	}

	color.rgb = tonemap(color.rgb * brightnessScale, toneMapScale);

	fragColor = color;
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FrontFacing = stage_input.gl_FrontFacing;
    ViewDir = stage_input.ViewDir;
    btnMatrix = stage_input.btnMatrix;
    texCoord = stage_input.texCoord;
    C = stage_input.C;
    LightDir = stage_input.LightDir;
    reflMatrix = stage_input.reflMatrix;
    // the GLSL global initializers, which GLSL runs here (they read inputs, uniforms or globals)
    ViewDir_norm = normalize( ViewDir );
    btnMatrix_norm = float3x3( normalize( btnMatrix[0] ), normalize( btnMatrix[1] ), normalize( btnMatrix[2] ) );
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
