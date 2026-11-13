// HLSL source of sk_msn.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_PS 1

#include "uniforms.src.hlsli"

Texture2D<float4> BaseMap; SamplerState _BaseMap_sampler;
Texture2D<float4> NormalMap; SamplerState _NormalMap_sampler;
Texture2D<float4> SpecularMap; SamplerState _SpecularMap_sampler;
Texture2D<float4> LightMask; SamplerState _LightMask_sampler;
Texture2D<float4> TintMask; SamplerState _TintMask_sampler;
Texture2D<float4> DetailMask; SamplerState _DetailMask_sampler;
Texture2D<float4> BacklightMap; SamplerState _BacklightMap_sampler;

uniform float3 specColor;
uniform float specStrength;
uniform float specGlossiness;
uniform bool useGloss;

uniform float3 glowColor;
uniform float glowMult;

uniform float alpha;
uniform int alphaFlags;			// bits 0 to 2: alpha test mode, bit 3: alpha blending enabled
uniform float alphaThreshold;

uniform float3 tintColor;

uniform float2 uvScale;
uniform float2 uvOffset;

uniform bool hasEmit;
uniform bool hasSoftlight;
uniform bool hasBacklight;
uniform bool hasRimlight;
uniform bool hasModelSpaceNormals;
uniform bool hasSpecularMap;
uniform bool hasDetailMask;
uniform bool hasTintMask;
uniform bool hasTintColor;

uniform float lightingEffect1;
uniform float lightingEffect2;

uniform row_major float4x4 modelViewMatrix;

static float3 LightDir;
static float3 ViewDir;

static float2 texCoord;

static float4 A;
static float4 C;
static float4 D;

static float4 fragColor;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float3 LightDir : TEXCOORD0;
    float3 ViewDir : TEXCOORD1;
    float2 texCoord : TEXCOORD2;
    nointerpolation float4 A : TEXCOORD3;
    float4 C : TEXCOORD4;
    nointerpolation float4 D : TEXCOORD5;
};
struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};



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

float overlay( float base, float blend )
{
	float result;
	if ( base < 0.5 ) {
		result = 2.0 * base * blend;
	} else {
		result = 1.0 - 2.0 * (1.0 - blend) * (1.0 - base);
	}
	return result;
}

float3 overlay( float3 ba, float3 bl )
{
	return float3( overlay(ba.r, bl.r), overlay(ba.g, bl.g), overlay( ba.b, bl.b ) );
}

void frag_main()
{
	float2 offset = texCoord.xy * uvScale + uvOffset;

	float4 baseMap = BaseMap.Sample(_BaseMap_sampler, offset);

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

	float4 normalMap = NormalMap.Sample(_NormalMap_sampler, offset);

	float3 normal = normalMap.rgb * 2.0 - 1.0;

	// Convert model space to view space
	//	Swizzled G/B values!
	normal = normalize( mul(normal.rbg, (float3x3)(modelViewMatrix)) );

	// Face Normals
	//vec3 X = dFdx(v);
	//vec3 Y = dFdy(v);
	//vec3 constructedNormal = normalize(cross(X,Y));


	float3 L = normalize(LightDir);
	float3 E = normalize(ViewDir);
	float3 R = reflect(-L, normal);
	float3 H = normalize( L + E );

	float NdotL = max( dot(normal, L), 0.0 );
	float NdotH = max( dot(normal, H), 0.0 );
	float EdotN = max( dot(normal, E), 0.0 );
	float NdotNegL = max( dot(normal, -L), 0.0 );


	float3 albedo = baseMap.rgb * C.rgb;
	float3 diffuse = A.rgb + (D.rgb * NdotL);


	// Emissive
	float3 emissive = (float3)(0.0);
	if ( hasEmit ) {
		emissive += glowColor * glowMult;
	}

	// Specular

	float s = SpecularMap.Sample(_SpecularMap_sampler, offset).r;
	if ( or(!hasSpecularMap, hasBacklight) ) {
		s = normalMap.a;
	}

	float3 spec = clamp( specColor * specStrength * s * pow(NdotH, useGloss ? specGlossiness : 1.0), 0.0, 1.0 );
	spec *= D.rgb;


	float3 backlight = (float3)(0.0);
	if ( hasBacklight ) {
		backlight = BacklightMap.Sample(_BacklightMap_sampler, offset).rgb;
		backlight *= NdotNegL;

		emissive += backlight * D.rgb;
	}

	float4 mask = (float4)(0.0);
	if ( or(hasRimlight, hasSoftlight) ) {
		mask = LightMask.Sample(_LightMask_sampler, offset);
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

	float3 detail = (float3)(0.0);
	if ( hasDetailMask ) {
		detail = DetailMask.Sample(_DetailMask_sampler, offset).rgb;

		albedo = overlay( albedo, detail );
	}

	float3 tint = (float3)(0.0);
	if ( hasTintMask ) {
		tint = TintMask.Sample(_TintMask_sampler, offset).rgb;

		albedo = overlay( albedo, tint );
	}

	if ( hasDetailMask ) {
		albedo += albedo;
	}

	if ( hasTintColor ) {
		albedo *= tintColor;
	}

	color.rgb = albedo * (diffuse + emissive * glowScaleSRGB) + spec;
	color.rgb = tonemap( color.rgb );

	fragColor = color;
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    D = stage_input.D;
    A = stage_input.A;
    texCoord = stage_input.texCoord;
    C = stage_input.C;
    LightDir = stage_input.LightDir;
    ViewDir = stage_input.ViewDir;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
