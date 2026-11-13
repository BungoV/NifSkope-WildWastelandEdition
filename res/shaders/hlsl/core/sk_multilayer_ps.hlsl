// HLSL source of sk_multilayer.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
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
Texture2D<float4> LightMask; SamplerState _LightMask_sampler;
Texture2D<float4> BacklightMap; SamplerState _BacklightMap_sampler;
Texture2D<float4> InnerMap; SamplerState _InnerMap_sampler;
Texture2D<float4> EnvironmentMap; SamplerState _EnvironmentMap_sampler;
TextureCube<float4> CubeMap; SamplerState _CubeMap_sampler;

uniform float3 specColor;
uniform float specStrength;
uniform float specGlossiness;
uniform bool useGloss;

uniform float3 glowColor;
uniform float glowMult;

uniform float alpha;
uniform int alphaFlags;			// bits 0 to 2: alpha test mode, bit 3: alpha blending enabled
uniform float alphaThreshold;

uniform float2 uvScale;
uniform float2 uvOffset;

uniform bool hasEmit;
uniform bool hasSoftlight;
uniform bool hasBacklight;
uniform bool hasRimlight;
uniform bool hasCubeMap;
uniform bool hasEnvMask;

uniform float lightingEffect1;
uniform float lightingEffect2;

uniform float2 innerScale;
uniform float innerThickness;
uniform float outerRefraction;
uniform float outerReflection;

static float3 LightDir;
static float3 ViewDir;

static float2 texCoord;

static float4 A;
static float4 C;
static float4 D;

static float3x3 btnMatrix;

static float4 fragColor;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
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

// Compute inner layer's texture coordinate and transmission depth
// vTexCoord: Outer layer's texture coordinate
// vInnerScale: Tiling of inner texture
// vViewTS: View vector in tangent space
// vNormalTS: Normal in tangent space (sampled normal map)
// fLayerThickness: Distance from outer layer to inner layer
float3 ParallaxOffsetAndDepth( float2 vTexCoord, float2 vInnerScale, float3 vViewTS, float3 vNormalTS, float fLayerThickness )
{
	// Tangent space reflection vector
	float3 vReflectionTS = reflect( -vViewTS, vNormalTS );
	// Tangent space transmission vector (reflect about surface plane)
	float3 vTransTS = float3( vReflectionTS.xy, -vReflectionTS.z );

	// Distance along transmission vector to intersect inner layer
	float fTransDist = fLayerThickness / abs(vTransTS.z);

	// Texel size
	// 	Bethesda's version does indeed seem to assume 1024, which is why they
	//	introduced the additional parameter.
	float2 vTexelSize = float2( 1.0/(1024.0 * vInnerScale.x), 1.0/(1024.0 * vInnerScale.y) );

	// Inner layer's texture coordinate due to parallax
	float2 vOffset = vTexelSize * fTransDist * vTransTS.xy;
	float2 vOffsetTexCoord = vTexCoord + vOffset;

	// Return offset texture coordinate in xy and transmission dist in z
	return float3( vOffsetTexCoord, fTransDist );
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

	float3 normalTS = normalize(normalMap.rgb * 2.0 - 1.0);
	float3 normal = normalize(mul(normalTS, btnMatrix));

	// Sample the non-parallax offset alpha channel of the inner map
	//	Used to modulate the innerThickness
	float innerMapAlpha = InnerMap.Sample(_InnerMap_sampler, offset).a;


	float3 L = normalize(LightDir);
	float3 E = normalize(ViewDir);
	float3 R = reflect(-L, normal);
	float3 H = normalize( L + E );

	float NdotL = max( dot(normal, L), 0.0 );
	float NdotH = max( dot(normal, H), 0.0 );
	float EdotN = max( dot(normal, E), 0.0 );
	float NdotNegL = max( dot(normal, -L), 0.0 );


	// Mix between the face normal and the normal map based on the refraction scale
	float3 mixedNormal = lerp( float3(0.0, 0.0, 1.0), normalTS, clamp( outerRefraction, 0.0, 1.0 ) );
	float3 parallax = ParallaxOffsetAndDepth( offset, innerScale, normalize(mul(btnMatrix, E)), mixedNormal, innerThickness * innerMapAlpha );

	// Sample the inner map at the offset coords
	float4 innerMap = InnerMap.Sample(_InnerMap_sampler, parallax.xy * innerScale);

	float3 reflected = reflect( -E, normal );
	float3 reflectedWS = mul(reflected, envMapRotation);


	float3 albedo;
	float3 diffuse = A.rgb + (D.rgb * NdotL);
	float3 inner = innerMap.rgb * C.rgb;
	float3 outer = baseMap.rgb * C.rgb;


	// Mix inner/outer layer based on fresnel
	float outerMix = max( 1.0 - EdotN, baseMap.a );
	albedo = lerp( inner, outer, outerMix );


	// Environment
	if ( hasCubeMap ) {
		float4 cube = CubeMap.Sample(_CubeMap_sampler, reflectedWS);
		cube.rgb *= outerReflection;

		if ( hasEnvMask ) {
			float4 env = EnvironmentMap.Sample(_EnvironmentMap_sampler, offset);
			cube.rgb *= env.r;
		} else {
			cube.rgb *= normalMap.a;
		}

		albedo += cube.rgb;
	}

	// Specular
	float3 spec = clamp( specColor * specStrength * normalMap.a * pow(NdotH, useGloss ? specGlossiness : 1.0), 0.0, 1.0 );
	spec *= D.rgb;

	// Emissive
	//	Mixed with outer map
	float3 emissive = (float3)(0.0);
	if ( hasEmit ) {
		emissive += glowColor * glowMult;
	}

	// Backlight
	// 	Mixed with inner and outer map
	float3 backlight = (float3)(0.0);
	if ( hasBacklight ) {
		backlight = BacklightMap.Sample(_BacklightMap_sampler, offset).rgb;
		backlight *= NdotNegL;

		emissive += backlight * D.rgb;
	}

	// TODO: Test rim and soft light mixing with inner/outer layer

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
    btnMatrix = stage_input.btnMatrix;
    LightDir = stage_input.LightDir;
    ViewDir = stage_input.ViewDir;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
