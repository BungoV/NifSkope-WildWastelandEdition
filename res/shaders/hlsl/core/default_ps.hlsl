// HLSL source of default.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_PS 1

struct Texture {
	float2 uvCenter;
	float2 uvScale;
	float2 uvOffset;
	float uvRotation;
	int coordSet;
	int textureUnit;
};

#include "uniforms.src.hlsli"

Texture2D<float4> textureUnits[10]; SamplerState _textureUnits_sampler[10];
uniform Texture textures[10];

// bits 0 to 2: color mode
// bit 3: lighting mode
// bits 4 to 5: vertex mode
uniform int vertexColorFlags;

uniform float alpha;
uniform int alphaFlags;			// bits 0 to 2: alpha test mode, bit 3: alpha blending enabled
uniform float alphaThreshold;

uniform float4 frontMaterialDiffuse;
uniform float4 frontMaterialSpecular;
uniform float4 frontMaterialAmbient;
uniform float4 frontMaterialEmission;
uniform float frontMaterialShininess;

static float3 LightDir;
static float3 ViewDir;

static float2 texCoords[9];

static float4 A;
static float4 C;
static float4 D;

static float3 N;

static float4 fragColor;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float3 LightDir : TEXCOORD0;
    float3 ViewDir : TEXCOORD1;
    float2 texCoords[9] : TEXCOORD2;
    nointerpolation float4 A : TEXCOORD11;
    float4 C : TEXCOORD12;
    nointerpolation float4 D : TEXCOORD13;
    float3 N : TEXCOORD14;
};
struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};



float4 getTexture( int n )
{
	float	r_c = cos( textures[n].uvRotation );
	float	r_s = sin( textures[n].uvRotation ) * -1.0;
	float2	offs = texCoords[textures[n].coordSet].xy - textures[n].uvCenter;
	offs = float2( offs.x * r_c - offs.y * r_s, offs.x * r_s + offs.y * r_c );
	offs = offs * textures[n].uvScale + textures[n].uvCenter + textures[n].uvOffset;

	return textureUnits[textures[n].textureUnit - 1].Sample(_textureUnits_sampler[textures[n].textureUnit - 1], offs);
}

void frag_main()
{
	float3 L = normalize( LightDir );
	float3 E = normalize( ViewDir );

	float4 color = (float4)(1.0);

	float3 normal = normalize( N );

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

	// Emissive
	float3 emissive = frontMaterialEmission.rgb * frontMaterialEmission.a;
	if ( ( vertexColorFlags & 0x10 ) != 0 )
		emissive = C.rgb * C.a;
	color.rgb += emissive * glowScaleSRGB;

	color.a *= alpha;

	// Texturing
	if ( textures[0].textureUnit > 0 ) {
		// base
		color *= getTexture( 0 );
	}
	if ( textures[1].textureUnit > 0 ) {
		// dark
		color *= getTexture( 1 );
	}
	if ( textures[2].textureUnit > 0 ) {
		// detail
		color *= getTexture( 2 );
		color.rgb *= 2.0;
	}
	if ( textures[6].textureUnit > 0 ) {
		// decal 0
		float4	tmp = getTexture( 6 );
		color.rgb = lerp( color.rgb, tmp.rgb, tmp.a );
	}
	if ( textures[7].textureUnit > 0 ) {
		// decal 1
		float4	tmp = getTexture( 7 );
		color.rgb = lerp( color.rgb, tmp.rgb, tmp.a );
	}
	if ( textures[8].textureUnit > 0 ) {
		// decal 2
		float4	tmp = getTexture( 8 );
		color.rgb = lerp( color.rgb, tmp.rgb, tmp.a );
	}
	if ( textures[9].textureUnit > 0 ) {
		// decal 3
		float4	tmp = getTexture( 9 );
		color.rgb = lerp( color.rgb, tmp.rgb, tmp.a );
	}
	if ( textures[4].textureUnit > 0 ) {
		// glow
		color.rgb += getTexture( 4 ).rgb * glowScaleSRGB;
	}

	// Specular
#if 0
	if ( NdotL > 0.0 ) {
		float3 H = normalize( L + E );
		float NdotH = dot( normal, H );
		if ( NdotH > 0.0 ) {
			float3 spec = frontMaterialSpecular.rgb * pow( NdotH, frontMaterialShininess );
			color.rgb += spec * frontMaterialSpecular.a;
		}
	}
#endif

	float	a = 1.0;
	if ( alphaFlags > 0 ) {
		// 0: always, 1: <, 2: ==, 3: <=, 4: >, 5: !=, 6: >=, 7: never
		int	m = ( color.a < alphaThreshold ? 0x2B2B : ( color.a > alphaThreshold ? 0x7171 : 0x4D4D ) );
		if ( ( m & ( 1 << alphaFlags ) ) == 0 )
			discard;
		if ( ( alphaFlags & 8 ) != 0 )
			a = color.a;
	}

	fragColor = float4( color.rgb * D.a, a );
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    texCoords = stage_input.texCoords;
    LightDir = stage_input.LightDir;
    ViewDir = stage_input.ViewDir;
    N = stage_input.N;
    C = stage_input.C;
    A = stage_input.A;
    D = stage_input.D;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
