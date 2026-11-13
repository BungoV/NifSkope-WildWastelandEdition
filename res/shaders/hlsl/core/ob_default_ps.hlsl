// HLSL source of ob_default.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
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
Texture2D<float4> GlowMap; SamplerState _GlowMap_sampler;
Texture2D<float4> HeightMap; SamplerState _HeightMap_sampler;
TextureCube<float4> CubeMap; SamplerState _CubeMap_sampler;
Texture2D<float4> EnvironmentMap; SamplerState _EnvironmentMap_sampler;

// bits 0 to 2: color mode
// bit 3: lighting mode
// bits 4 to 5: vertex mode
uniform int vertexColorFlags;

uniform bool isEffect;
uniform bool hasSpecular;
uniform bool useGloss;
uniform bool hasEmit;
uniform bool hasGlowMap;
uniform bool hasCubeMap;
uniform bool hasCubeMask;
uniform float cubeMapScale;
// <= 0: disabled, 1 or 2: simple parallax mapping using BaseMap.a or HeightMap.r, >= 3: POM using HeightMap.r
uniform int parallaxMaxSteps;
uniform float parallaxScale;
uniform float glowMult;

uniform float alpha;
uniform int alphaFlags;			// bits 0 to 2: alpha test mode, bit 3: alpha blending enabled
uniform float alphaThreshold;

uniform float2 uvCenter;
uniform float2 uvScale;
uniform float2 uvOffset;
uniform float uvRotation;

uniform float4 falloffParams;

uniform float4 frontMaterialDiffuse;
uniform float4 frontMaterialSpecular;
uniform float4 frontMaterialAmbient;
uniform float4 frontMaterialEmission;
uniform float frontMaterialShininess;

static float3x3 reflMatrix;

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
    WW_LOC(9) float3x3 reflMatrix : TEXCOORD9;
    WW_LOC(3) float3 LightDir : TEXCOORD3;
    WW_LOC(4) float3 ViewDir : TEXCOORD4;
    WW_LOC(8) float2 texCoord : TEXCOORD8;
    WW_LOC(0) nointerpolation float4 A : TEXCOORD0;
    WW_LOC(1) float4 C : TEXCOORD1;
    WW_LOC(2) nointerpolation float4 D : TEXCOORD2;
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

// parallax occlusion mapping based on code from
// https://web.archive.org/web/20150419215321/http://sunandblackcat.com/tipFullView.php?l=eng&topicid=28

float2 parallaxMapping( float3 V, float2 offset )
{
	if ( parallaxMaxSteps < 3 ) {
		float	h;
		if ( parallaxMaxSteps < 2 )
			h = BaseMap.Sample(_BaseMap_sampler, offset).a;
		else
			h = HeightMap.Sample(_HeightMap_sampler, offset).r;
		return offset + V.xy * ( ( h - 0.5 ) * parallaxScale );
	}

	// determine optimal height of each layer
	float	layerHeight = 1.0 / lerp( max( (float)(parallaxMaxSteps), 4.0 ), 4.0, abs(V.z) );

	// current height of the layer
	float	curLayerHeight = 1.0;
	float2	dtex = parallaxScale * V.xy / max( abs(V.z), 0.02 );
	// current texture coordinates
	float2	currentTextureCoords = offset + ( dtex * 0.5 );
	// shift of texture coordinates for each layer
	dtex *= layerHeight;

	// height from heightmap
	float	heightFromTexture = HeightMap.Sample(_HeightMap_sampler, currentTextureCoords).r;

	// while point is above the surface
	while ( curLayerHeight > heightFromTexture ) {
		// to the next layer
		curLayerHeight -= layerHeight;
		// shift of texture coordinates
		currentTextureCoords -= dtex;
		// new height from heightmap
		heightFromTexture = HeightMap.Sample(_HeightMap_sampler, currentTextureCoords).r;
	}

	// previous texture coordinates
	float2	prevTCoords = currentTextureCoords + dtex;

	// heights for linear interpolation
	float	nextH = curLayerHeight - heightFromTexture;
	float	prevH = curLayerHeight + layerHeight - HeightMap.Sample(_HeightMap_sampler, prevTCoords).r;

	// proportions for linear interpolation
	float	weight = nextH / ( nextH - prevH );

	// return interpolation of texture coordinates
	return lerp( currentTextureCoords, prevTCoords, weight );
}

void frag_main()
{
	float3 L = normalize( LightDir );
	float3 E = normalize( ViewDir );

	float2 offset = texCoord.xy - uvCenter;
	float r_c = cos( uvRotation );
	float r_s = sin( uvRotation ) * -1.0;
	offset = float2( offset.x * r_c - offset.y * r_s, offset.x * r_s + offset.y * r_c ) * uvScale + uvCenter + uvOffset;
	if ( parallaxMaxSteps >= 1 && parallaxScale >= 0.0005 )
		offset = parallaxMapping( E, offset );

	float4 baseMap = BaseMap.Sample(_BaseMap_sampler, offset);
	float4 color = baseMap;

	if ( isEffect ) {
		float alphaMult = 1.0;

		if ( falloffParams.y != falloffParams.x ) {
			// TODO: When X and Y are both 0.0 or both 1.0 the effect is reversed.
			float falloff = smoothstep( falloffParams.y, falloffParams.x, abs(E.z) );
			falloff = lerp( max(falloffParams.w, 0.0), min(falloffParams.z, 1.0), falloff );
			alphaMult = falloff;
		}

		color *= C;
		if ( dot( frontMaterialEmission.rgb, (float3)(1.0) ) > 0.003 )
			color.rgb = color.rgb * frontMaterialEmission.rgb;
		color.rgb = color.rgb * glowMult;
		color.a = color.a * alphaMult;
	} else {
		float4 normalMap = NormalMap.Sample(_NormalMap_sampler, offset);

		float3 normal = normalize( normalMap.rgb * 2.0 - 1.0 );

		float NdotL = max( dot(normal, L), 0.0 );

		if ( ( vertexColorFlags & 0x28 ) == 0x28 ) {
			color *= C;
			color.rgb *= A.rgb + ( D.rgb * NdotL );
		} else if ( ( vertexColorFlags & 0x08 ) != 0 ) {
			color.rgb *= ( A.rgb * frontMaterialAmbient.rgb ) + ( D.rgb * frontMaterialDiffuse.rgb * NdotL );
			color.a *= min( frontMaterialAmbient.a + frontMaterialDiffuse.a, 1.0 );
		} else {
			color.rgb *= A.rgb + ( D.rgb * NdotL );
		}


		// Environment
		if ( hasCubeMap && cubeMapScale > 0.0 ) {
			float3 R = reflect( -E, normal );
			float3 reflectedWS = mul(R, reflMatrix);
			float3 cube = CubeMap.Sample(_CubeMap_sampler, reflectedWS).rgb;
			if ( hasCubeMask )
				cube *= EnvironmentMap.Sample(_EnvironmentMap_sampler, offset).r * cubeMapScale;
			else
				cube *= normalMap.a * cubeMapScale;
			color.rgb += cube * A.rgb * ( 1.0 / 0.375 );
		}

		// Emissive
		float3 emissive = (float3)(glowMult * glowScaleSRGB);
		if ( ( vertexColorFlags & 0x10 ) == 0 )
			emissive *= frontMaterialEmission.rgb * frontMaterialEmission.a;
		else
			emissive *= C.rgb;
		if ( hasGlowMap )
			color.rgb += emissive * baseMap.rgb * GlowMap.Sample(_GlowMap_sampler, offset).rgb;
		else if ( hasEmit )
			color.rgb += emissive * baseMap.rgb;

		// Specular
		if ( hasSpecular && NdotL > 0.0 ) {
			float3 H = normalize( L + E );
			float NdotH = dot( normal, H );
			if ( NdotH > 0.0 ) {
				float3 spec = frontMaterialSpecular.rgb * pow( NdotH, useGloss ? frontMaterialShininess : 1.0 );
				color.rgb += spec * frontMaterialSpecular.a * normalMap.a;
			}
		}
	}

	color.a *= alpha;

	float	a = 1.0;
	if ( alphaFlags > 0 ) {
		// 0: always, 1: <, 2: ==, 3: <=, 4: >, 5: !=, 6: >=, 7: never
		int	m = ( color.a < alphaThreshold ? 0x2B2B : ( color.a > alphaThreshold ? 0x7171 : 0x4D4D ) );
		if ( ( m & ( 1 << alphaFlags ) ) == 0 )
			discard;
		if ( ( alphaFlags & 8 ) != 0 )
			a = color.a;
	}

	fragColor = float4( tonemap( color.rgb ), a );
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    D = stage_input.D;
    A = stage_input.A;
    LightDir = stage_input.LightDir;
    ViewDir = stage_input.ViewDir;
    texCoord = stage_input.texCoord;
    C = stage_input.C;
    reflMatrix = stage_input.reflMatrix;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
