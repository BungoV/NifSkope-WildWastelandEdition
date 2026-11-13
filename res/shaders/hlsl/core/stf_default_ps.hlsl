// HLSL source of stf_default.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "CoreBindingsVk.hlsli"
#include "GlslCompat.hlsli"
#define WW_STAGE_PS 1
#ifndef NUM_TEXTURE_UNITS
#define NUM_TEXTURE_UNITS 30
#endif
static bool gl_FrontFacing;

struct UVStream {
	float4	scaleAndOffset;
	bool	useChannelTwo;
};

struct TextureSet {
	// >= 1: textureUnits index, -1: use replacement, 0: disabled
	int	textures[9];
	float4	textureReplacements[9];
	float	floatParam;
};

struct Material {
	float4	color;
	// bit 0 = color override mode (0: multiply, 1: lerp)
	// bit 1 = use vertex color as tint (1: yes)
	// bit 2 = is a flipbook
	int	flags;
	TextureSet	textureSet;
};

struct Layer {
	Material	material;
	UVStream	uvStream;
};

struct Blender {
	UVStream	uvStream;
	int	maskTexture;
	float4	maskTextureReplacement;
	// 0 = "Linear" (default), 1 = "Additive", 2 = "PositionContrast",
	// 3 = "None", 4 = "CharacterCombine", 5 = "Skin"
	int	blendMode;
	// 0 = "Red" (default), 1 = "Green", 2 = "Blue", 3 = "Alpha"
	int	colorChannel;
	float	floatParams[5];
	bool	boolParams[8];
};

struct LayeredEmissivityComponent {
	bool	isEnabled;
	int	firstLayerIndex;
	float4	firstLayerTint;
	int	firstLayerMaskIndex;
	int	secondLayerIndex;	// -1 if the layer is not active
	float4	secondLayerTint;
	int	secondLayerMaskIndex;
	int	firstBlenderIndex;
	int	firstBlenderMode;
	int	thirdLayerIndex;	// -1 if the layer is not active
	float4	thirdLayerTint;
	int	thirdLayerMaskIndex;
	int	secondBlenderIndex;
	int	secondBlenderMode;
	float	emissiveClipThreshold;
	bool	adaptiveEmittance;
	float	luminousEmittance;
	float	exposureOffset;
	bool	enableAdaptiveLimits;
	float	maxOffsetEmittance;
	float	minOffsetEmittance;
};

struct EmissiveSettingsComponent {
	bool	isEnabled;
	int	emissiveSourceLayer;
	float4	emissiveTint;
	int	emissiveMaskSourceBlender;
	float	emissiveClipThreshold;
	bool	adaptiveEmittance;
	float	luminousEmittance;
	float	exposureOffset;
	bool	enableAdaptiveLimits;
	float	maxOffsetEmittance;
	float	minOffsetEmittance;
};

struct DecalSettingsComponent {
	bool	isDecal;
	float	materialOverallAlpha;
	int	writeMask;
	bool	isPlanet;
	bool	isProjected;
	bool	useParallaxOcclusionMapping;
	// >= 1: textureUnits index, <= 0: disabled
	int	surfaceHeightMap;
	float	parallaxOcclusionScale;
	bool	parallaxOcclusionShadows;
	int	maxParralaxOcclusionSteps;
	int	renderLayer;
	bool	useGBufferNormals;
	int	blendMode;
	bool	animatedDecalIgnoresTAA;
};

struct EffectSettingsComponent {
	// NOTE: the falloff settings here are deprecated and have been replaced by LayeredEdgeFalloffComponent
	bool	vertexColorBlend;
	bool	noHalfResOptimization;
	bool	softEffect;
	float	softFalloffDepth;
	bool	emissiveOnlyEffect;
	bool	emissiveOnlyAutomaticallyApplied;
	bool	receiveDirectionalShadows;
	bool	receiveNonDirectionalShadows;
	bool	isGlass;
	bool	frosting;
	float	frostingUnblurredBackgroundAlphaBlend;
	float	frostingBlurBias;
	float	materialOverallAlpha;
	bool	zTest;
	bool	zWrite;
	int	blendingMode;
	bool	backLightingEnable;
	float	backlightingScale;
	float	backlightingSharpness;
	float	backlightingTransparencyFactor;
	float4	backLightingTintColor;
	bool	depthMVFixup;
	bool	depthMVFixupEdgesOnly;
	bool	forceRenderBeforeOIT;
	int	depthBiasInUlp;
};

struct LayeredEdgeFalloffComponent {
	float	falloffStartAngles[3];
	float	falloffStopAngles[3];
	float	falloffStartOpacities[3];
	float	falloffStopOpacities[3];
	// bits 0 to 2: active layers mask, bit 7: use RGB falloff
	int	flags;
};

struct OpacityComponent {	// for shader route = Effect
	int	firstLayerIndex;
	bool	secondLayerActive;
	int	secondLayerIndex;
	int	firstBlenderIndex;
	// 0 = "Lerp", 1 = "Additive", 2 = "Subtractive", 3 = "Multiplicative"
	int	firstBlenderMode;
	bool	thirdLayerActive;
	int	thirdLayerIndex;
	int	secondBlenderIndex;
	int	secondBlenderMode;
	float	specularOpacityOverride;
};

struct AlphaSettingsComponent {	// for shader route = Deferred
	bool	hasOpacity;
	float	alphaTestThreshold;
	int	opacitySourceLayer;
	// 0 = "Linear" (default), 1 = "Additive", 2 = "PositionContrast", 3 = "None"
	int	alphaBlenderMode;
	bool	useDetailBlendMask;
	bool	useVertexColor;
	int	vertexColorChannel;
	UVStream	opacityUVstream;
	float	heightBlendThreshold;
	float	heightBlendFactor;
	float	position;
	float	contrast;
	bool	useDitheredTransparency;
};

struct TranslucencySettingsComponent {
	bool	isEnabled;
	bool	isThin;
	bool	flipBackFaceNormalsInViewSpace;
	bool	useSSS;
	float	sssWidth;
	float	sssStrength;
	float	transmissiveScale;
	float	transmittanceWidth;
	float	specLobe0RoughnessScale;
	float	specLobe1RoughnessScale;
	int	transmittanceSourceLayer;
};

struct DetailBlenderSettings {
	bool	detailBlendMaskSupported;
	int	maskTexture;
	float4	maskTextureReplacement;
	UVStream	uvStream;
};

struct LayeredMaterial {
	// shader model IDs are defined in lib/lib[reference game]utils/src/mat_dump.cpp
	int	shaderModel;
	bool	isEffect;
	bool	hasOpacityComponent;
	int	numLayers;
	Layer	layers[6];
	Blender	blenders[5];
	LayeredEmissivityComponent	layeredEmissivity;
	EmissiveSettingsComponent	emissiveSettings;
	DecalSettingsComponent	decalSettings;
	EffectSettingsComponent	effectSettings;
	LayeredEdgeFalloffComponent	layeredEdgeFalloff;
	OpacityComponent	opacity;
	AlphaSettingsComponent	alphaSettings;
	TranslucencySettingsComponent	translucencySettings;
	DetailBlenderSettings	detailBlender;
};

#include "uniforms.src.hlsli"

TextureCube<float4>	CubeMap; SamplerState _CubeMap_sampler;
TextureCube<float4>	CubeMap2; SamplerState _CubeMap2_sampler;
uniform bool	hasCubeMap;
uniform bool	hasSpecular;

Texture2D<float4>	textureUnits[NUM_TEXTURE_UNITS]; SamplerState _textureUnits_sampler[NUM_TEXTURE_UNITS];

// bit 0: alpha testing, bit 1: alpha blending
uniform int alphaFlags;

uniform	LayeredMaterial	lm;

static float3 LightDir;
static float3 ViewDir;

static float4 texCoord;

static float4 C;

static float3x3 btnMatrix;

static float4 fragColor;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    WW_LOC(3) float3 LightDir : TEXCOORD3;
    WW_LOC(4) float3 ViewDir : TEXCOORD4;
    WW_LOC(8) float4 texCoord : TEXCOORD8;
    WW_LOC(1) float4 C : TEXCOORD1;
    WW_LOC(5) float3x3 btnMatrix : TEXCOORD5;
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

float emissiveIntensity( bool useAdaptive, bool adaptiveLimits, float4 luminanceParams )
{
	float	l = luminanceParams[0];	// luminousEmittance

	if ( useAdaptive ) {
		l = dot( lightSourceAmbient.rgb * 20.0 + lightSourceDiffuse[0].rgb * 80.0, float3(0.2126, 0.7152, 0.0722) );
		l = l * exp2( luminanceParams[1] );	// exposureOffset
		if ( adaptiveLimits )	// minOffsetEmittance, maxOffsetEmittance
			l = clamp( l, luminanceParams[3], luminanceParams[2] );
	}

	return sqrt( l * 0.01 ) * glowScale;
}

float LightingFuncGGX_REF( float NdotH, float NdotL, float NdotV, float roughness )
{
	float alpha = roughness * roughness;
	// D (GGX normal distribution)
	float alphaSqr = alpha * alpha;
	float denom = NdotH * NdotH;
	denom = ( denom * alphaSqr ) + ( 1.0 - denom );
	float D = alphaSqr / ( denom * denom * 4.0 );
	// no pi because BRDF -> lighting
	// G
	float	k = alpha * 0.5;
	float	G = NdotL / ( lerp(NdotL, 1.0, k) * lerp(NdotV, 1.0, k) );

	return D * G;
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

float2 getTexCoord(in UVStream uvStream)
{
	float2	offset = ( !uvStream.useChannelTwo ? texCoord.xy : texCoord.zw );
	return offset * uvStream.scaleAndOffset.xy + uvStream.scaleAndOffset.zw;
}

float4 getLayerTexture(int layerNum, int textureNum, float2 offset)
{
	int	n = lm.layers[layerNum].material.textureSet.textures[textureNum];
	if ( n < 0 )
		return lm.layers[layerNum].material.textureSet.textureReplacements[textureNum];
	return textureUnits[n].Sample(_textureUnits_sampler[n], offset);
}

float getDetailBlendMask()
{
	if ( !( lm.detailBlender.detailBlendMaskSupported && lm.detailBlender.maskTexture != 0 ) )
		return 1.0;
	if ( lm.detailBlender.maskTexture < 0 )
		return lm.detailBlender.maskTextureReplacement.r;
	return textureUnits[lm.detailBlender.maskTexture].Sample(_textureUnits_sampler[lm.detailBlender.maskTexture], getTexCoord( lm.detailBlender.uvStream )).r;
}

float getBlenderMask(int n)
{
	float	r = 1.0;
	if ( lm.blenders[n].maskTexture != 0 ) {
		if ( lm.blenders[n].maskTexture < 0 ) {
			r = lm.blenders[n].maskTextureReplacement.r;
		} else {
			float2	offset = getTexCoord(lm.blenders[n].uvStream);
			r = textureUnits[lm.blenders[n].maskTexture].Sample(_textureUnits_sampler[lm.blenders[n].maskTexture], offset).r;
		}
	}
	if ( lm.blenders[n].boolParams[5] )
		r *= C[lm.blenders[n].colorChannel];
	if ( lm.blenders[n].boolParams[7] )
		r *= getDetailBlendMask();
	return r * lm.blenders[n].floatParams[4];	// mask intensity
}

// parallax occlusion mapping based on code from
// https://web.archive.org/web/20150419215321/http://sunandblackcat.com/tipFullView.php?l=eng&topicid=28

float2 parallaxMapping( int n, float3 V, float2 offset )
{
	if ( sfParallaxScale < 0.0005 )
		return offset;	// disabled

	// determine optimal height of each layer
	float	layerHeight = 1.0 / lerp( (float)(sfParallaxMaxSteps), 8.0, abs(V.z) );

	// current height of the layer
	float	curLayerHeight = 1.0;
	float2	dtex = sfParallaxScale * V.xy / max( abs(V.z), 0.02 );
	// current texture coordinates
	float2	currentTextureCoords = offset + ( dtex * sfParallaxOffset );
	// shift of texture coordinates for each layer
	dtex *= layerHeight;

	// height from heightmap
	float	heightFromTexture = textureUnits[n].Sample(_textureUnits_sampler[n], currentTextureCoords).r;

	// while point is above the surface
	while ( curLayerHeight > heightFromTexture ) {
		// to the next layer
		curLayerHeight -= layerHeight;
		// shift of texture coordinates
		currentTextureCoords -= dtex;
		// new height from heightmap
		heightFromTexture = textureUnits[n].Sample(_textureUnits_sampler[n], currentTextureCoords).r;
	}

	// previous texture coordinates
	float2	prevTCoords = currentTextureCoords + dtex;

	// heights for linear interpolation
	float	nextH = curLayerHeight - heightFromTexture;
	float	prevH = curLayerHeight + layerHeight - textureUnits[n].Sample(_textureUnits_sampler[n], prevTCoords).r;

	// proportions for linear interpolation
	float	weight = nextH / ( nextH - prevH );

	// return interpolation of texture coordinates
	return lerp( currentTextureCoords, prevTCoords, weight );
}


void frag_main()
{
	if ( lm.shaderModel == 45 )	// "Invisible"
		discard;

	float3	baseMap = C.rgb;
	float3	normal = float3(0.0, 0.0, 1.0);
	float3	pbrMap = float3(0.0, 0.0, 1.0);	// roughness, metalness, AO
	float	baseAlpha = C.a;
	float	alpha = 1.0;
	float3	emissive = (float3)(0.0);
	float3	transmissive = (float3)(0.0);
	int	numLayers = min( lm.numLayers, 6 );

	for ( int i = 0; i < numLayers; i++ ) {
		float3	layerBaseMap = (float3)(0.0);
		float3	layerNormal = float3(0.0, 0.0, 1.0);
		float3	layerPBRMap = float3(0.0, 0.0, 1.0);

		int	blendMode = 3;	// "None"
		if ( i > 0 )
			blendMode = lm.blenders[i - 1].blendMode;

		float2	offset = getTexCoord( lm.layers[i].uvStream );
		// _height.dds
		if ( lm.layers[i].material.textureSet.textures[6] >= 1 )
			offset = parallaxMapping( lm.layers[i].material.textureSet.textures[6], normalize( mul(btnMatrix_norm, ViewDir_norm) ), offset );
		// _color.dds
		if ( lm.layers[i].material.textureSet.textures[0] != 0 )
			layerBaseMap = getLayerTexture(i, 0, offset).rgb;
		{
			float4	tintColor = ( (lm.layers[i].material.flags & 2) == 0 ? lm.layers[i].material.color : C );
			if ( (lm.layers[i].material.flags & 1) == 0 )
				layerBaseMap *= tintColor.rgb;
			else
				layerBaseMap = lerp( layerBaseMap, tintColor.rgb, tintColor.a );
		}
		// _normal.dds
		if ( lm.layers[i].material.textureSet.textures[1] != 0 ) {
			layerNormal.rg = getLayerTexture(i, 1, offset).rg * lm.layers[i].material.textureSet.floatParam;
			// Calculate missing blue channel
			layerNormal.b = sqrt(max(1.0 - dot(layerNormal.rg, layerNormal.rg), 0.0));
		}
		// _rough.dds
		if ( lm.layers[i].material.textureSet.textures[3] != 0 )
			layerPBRMap.r = getLayerTexture(i, 3, offset).r;
		// _metal.dds
		if ( lm.layers[i].material.textureSet.textures[4] != 0 )
			layerPBRMap.g = getLayerTexture(i, 4, offset).r;
		// _ao.dds
		if ( lm.layers[i].material.textureSet.textures[5] != 0 )
			layerPBRMap.b = getLayerTexture(i, 5, offset).r;

		// falloff
		float	f = 1.0;
		if ( ( lm.layeredEdgeFalloff.flags & ( 1 << i ) ) != 0 ) {
			float	startAngle = cos( radians(lm.layeredEdgeFalloff.falloffStartAngles[i]) );
			float	stopAngle = cos( radians(lm.layeredEdgeFalloff.falloffStopAngles[i]) );
			float	startOpacity = lm.layeredEdgeFalloff.falloffStartOpacities[i];
			float	stopOpacity = lm.layeredEdgeFalloff.falloffStopOpacities[i];
			float	NdotV = abs( dot(btnMatrix_norm[2], ViewDir_norm) );
			f = 0.5;
			if ( stopAngle > (startAngle + 0.000001) )
				f = smoothstep( startAngle, stopAngle, NdotV );
			else if ( startAngle > (stopAngle + 0.000001) )
				f = 1.0 - smoothstep( stopAngle, startAngle, NdotV );
			f = clamp( lerp(startOpacity, stopOpacity, f), 0.0, 1.0 );
			if ( (lm.layeredEdgeFalloff.flags & 0x80) != 0 )
				layerBaseMap *= f;
		}

		// material layering
		float	layerMask = 1.0;
		if ( i == 0 ) {
			if ( lm.decalSettings.isDecal && lm.layers[0].material.textureSet.textures[0] == 0 )
				discard;
			baseMap = layerBaseMap;
			normal = layerNormal;
			pbrMap = layerPBRMap;
			baseAlpha = 1.0;
			alpha = f;
		} else {
			layerMask = getBlenderMask( i - 1 );
			if ( blendMode != 3 && !( lm.isEffect && lm.effectSettings.isGlass ) ) {
				// TODO: correctly implement Skin, instead of interpreting it as Linear
				float	srcMask = layerMask;
				if ( blendMode == 2 ) {
					// PositionContrast
					float	blendPosition = lm.blenders[i - 1].floatParams[2];
					float	blendContrast = lm.blenders[i - 1].floatParams[3];
					blendContrast = max( blendContrast * min(blendPosition, 1.0 - blendPosition), 0.001 );
					blendPosition = ( blendPosition - 0.5 ) * 3.17;
					blendPosition = ( blendPosition * blendPosition + 1.0 ) * blendPosition + 0.5;
					float	maskMin = blendPosition - blendContrast;
					float	maskMax = blendPosition + blendContrast;
					srcMask = ( srcMask - maskMin ) / ( maskMax - maskMin );
				} else if ( blendMode == 4 ) {
					// CharacterCombine: blend color, roughness and metalness multiplicatively
					layerBaseMap = layerBaseMap * baseMap * 2.0;
					layerPBRMap.r = layerPBRMap.r * pbrMap.r * 2.0;
					if ( layerPBRMap.g < 0.5 )
						layerPBRMap.g = layerPBRMap.g * pbrMap.g;
					else
						layerPBRMap.g = layerPBRMap.g + pbrMap.g - layerPBRMap.g * pbrMap.g;
				} else if ( blendMode == 1 ) {
					// Additive
					layerBaseMap += baseMap;
					layerPBRMap += pbrMap;
					if ( !lm.blenders[i - 1].boolParams[4] )
						layerNormal += normal;
				}
				srcMask = clamp( srcMask, 0.0, 1.0 ) * f;
				if ( lm.blenders[i - 1].boolParams[0] )
					baseMap = min( lerp( baseMap, layerBaseMap, srcMask ), (float3)(1.0) );	// blend color
				layerPBRMap = min( lerp( pbrMap, layerPBRMap, srcMask ), (float3)(1.0) );
				if ( lm.blenders[i - 1].boolParams[1] )
					pbrMap.g = layerPBRMap.g;	// blend metalness
				if ( lm.blenders[i - 1].boolParams[2] )
					pbrMap.r = layerPBRMap.r;	// blend roughness
				if ( lm.blenders[i - 1].boolParams[3] ) {
					if ( lm.blenders[i - 1].boolParams[4] ) {
						normal.rg = normal.rg + ( layerNormal.rg * srcMask );	// blend normals additively
						normal.b = sqrt( max( 1.0 - dot(normal.rg, normal.rg), 0.0 ) );
					} else {
						normal = normalize( lerp( normal, layerNormal, srcMask ) );
					}
				}
				if ( lm.blenders[i - 1].boolParams[6] )
					pbrMap.b = layerPBRMap.b;	// blend ambient occlusion
			}
		}

		if ( lm.layers[i].material.textureSet.textures[2] != 0 ) {
			// _opacity.dds
			if ( lm.isEffect ) {
				float	a = getLayerTexture( i, 2, offset ).r;
				if ( lm.hasOpacityComponent ) {
					int	opacityBlendMode = -1;
					if ( i == lm.opacity.firstLayerIndex ) {
						baseAlpha = a;
					} else if ( !lm.effectSettings.isGlass ) {
						// FIXME: this assumes blender index = layer index - 1
						if ( lm.opacity.secondLayerActive && i == lm.opacity.secondLayerIndex )
							opacityBlendMode = lm.opacity.firstBlenderMode;
						else if ( lm.opacity.thirdLayerActive && i == lm.opacity.thirdLayerIndex )
							opacityBlendMode = lm.opacity.secondBlenderMode;
					}
					switch ( opacityBlendMode ) {
					case 0:
						baseAlpha = lerp( baseAlpha, a, layerMask );
						break;
					case 1:
						baseAlpha = min( baseAlpha + a * layerMask, 1.0 );
						break;
					case 2:
						baseAlpha = max( baseAlpha - a * layerMask, 0.0 );
						break;
					case 3:
						baseAlpha *= a * layerMask;
						break;
					}
				} else if ( i == 0 ) {
					baseAlpha = a;
				}
			} else if ( lm.alphaSettings.hasOpacity && i == lm.alphaSettings.opacitySourceLayer ) {
				if ( (lm.layers[i].material.flags & 4) == 0 )
					baseAlpha = getLayerTexture( i, 2, getTexCoord(lm.alphaSettings.opacityUVstream) ).r;
				else
					baseAlpha = getLayerTexture( i, 2, offset ).r;
			}
		}

		if ( lm.layers[i].material.textureSet.textures[7] != 0 ) {
			// _emissive.dds
			float4	tmp = (float4)(0.0);
			int	maskBlender = 0;
			if ( lm.emissiveSettings.isEnabled && i == lm.emissiveSettings.emissiveSourceLayer ) {
				tmp = lm.emissiveSettings.emissiveTint;
				maskBlender = lm.emissiveSettings.emissiveMaskSourceBlender;
			} else if ( lm.layeredEmissivity.isEnabled && i == lm.layeredEmissivity.firstLayerIndex ) {
				tmp = lm.layeredEmissivity.firstLayerTint;
				maskBlender = lm.layeredEmissivity.firstLayerMaskIndex;
			} else if ( lm.layeredEmissivity.isEnabled && i == lm.layeredEmissivity.secondLayerIndex ) {
				tmp = lm.layeredEmissivity.secondLayerTint;
				maskBlender = lm.layeredEmissivity.secondLayerMaskIndex;
			} else if ( lm.layeredEmissivity.isEnabled && i == lm.layeredEmissivity.thirdLayerIndex ) {
				tmp = lm.layeredEmissivity.thirdLayerTint;
				maskBlender = lm.layeredEmissivity.thirdLayerMaskIndex;
			} else {
				continue;
			}
			if ( maskBlender > 0 && maskBlender < numLayers )
				tmp.a *= getBlenderMask( maskBlender - 1 );
			emissive += getLayerTexture( i, 7, offset ).rgb * tmp.rgb * tmp.a;
		}

		if ( lm.layers[i].material.textureSet.textures[8] != 0 ) {
			// _transmissive.dds
			if ( lm.translucencySettings.isEnabled && i == lm.translucencySettings.transmittanceSourceLayer )
				transmissive = (float3)(getLayerTexture( i, 8, offset ).r * lm.translucencySettings.transmissiveScale);
		}
	}

	float4	color = (float4)(1.0);

	if ( alphaFlags != 0 ) {
		if ( lm.isEffect ) {
			if ( lm.effectSettings.vertexColorBlend ) {
				baseMap *= C.rgb;
				baseAlpha *= C.a;
			}
			alpha = alpha * lm.effectSettings.materialOverallAlpha * baseAlpha;
			// alpha test settings seem to be ignored for effects, and a fixed threshold of 1/128 is used instead
			if ( !( alpha > 0.0078 ) )
				discard;
			if ( lm.effectSettings.blendingMode == 2 )	// SourceSoftAdditive
				baseMap *= alpha;
		} else {
			if ( lm.decalSettings.isDecal )
				alpha = lm.decalSettings.materialOverallAlpha;
			if ( lm.alphaSettings.hasOpacity ) {
				if ( lm.alphaSettings.useDetailBlendMask )
					alpha *= getDetailBlendMask();
				if ( lm.alphaSettings.useVertexColor )
					alpha *= C[lm.alphaSettings.vertexColorChannel];
			}
			alpha = alpha * baseAlpha;
			if ( ( alphaFlags & 1 ) != 0 && !( alpha > lm.alphaSettings.alphaTestThreshold ) )
				discard;
		}
		if ( ( alphaFlags & 2 ) != 0 )
			color.a = alpha;
	}

	if ( !gl_FrontFacing )
		normal.z *= -1.0;
	normal = normalize( mul(normal, btnMatrix_norm) );

	float3	L = normalize(LightDir);
	float3	V = ViewDir_norm;
	float3	R = reflect(-V, normal);
	float3	H = normalize(L + V);

	float	NdotL = dot(normal, L);
	float	NdotL0 = max(NdotL, 0.0);
	float	NdotH = clamp(dot(normal, H), 0.0, 1.0);
	float	NdotV = abs(dot(normal, V));
	float	LdotH = dot(L, H);

	float3	reflectedWS = mul(R, envMapRotation);
	float3	normalWS = mul(normal, envMapRotation);

	float3	f0 = lerp((float3)(0.04), baseMap, pbrMap.g);
	float3	albedo = baseMap * (1.0 - pbrMap.g);

	// Specular
	float	roughness = pbrMap.r;
	float3	spec = lightSourceDiffuse[0].rgb;
	spec *= LightingFuncGGX_REF( NdotH, NdotL0, NdotV, clamp(roughness, 0.045, 0.95) );

	// Diffuse
	float3	diffuse = (float3)(NdotL0);
	// Fresnel
	float2	fDirect = textureUnits[0].SampleLevel(_textureUnits_sampler[0], float2(LdotH, NdotL0), 0.0).ba;
	spec *= lerp(f0, (float3)(1.0), fDirect.x);
	float4	envLUT = textureUnits[0].SampleLevel(_textureUnits_sampler[0], float2(NdotV, roughness), 0.0);
	float2	fDiff = float2(fDirect.y, envLUT.b);
	fDiff = fDiff * (LdotH * LdotH * roughness * 2.0 - 0.5) + 1.0;
	diffuse *= ((float3)(1.0) - f0) * fDiff.x * fDiff.y;

	// Environment
	float3	refl = (float3)(0.0);
	float3	ambient = lightSourceAmbient.rgb;
	if ( hasCubeMap ) {
		float	m = roughness * (roughness * -4.0 + 10.0);
		refl = CubeMap.SampleLevel(_CubeMap_sampler, reflectedWS, max(m, 0.0)).rgb;
		refl *= ambient;
		ambient *= CubeMap2.SampleLevel(_CubeMap2_sampler, normalWS, 0.0).rgb;
	} else {
		ambient *= 0.08;
		refl = ambient;
	}
	float3	f = lerp(f0, (float3)(1.0), envLUT.r);
	if (!hasSpecular) {
		albedo = baseMap;
		diffuse = (float3)(NdotL0);
		spec = (float3)(0.0);
		f = (float3)(0.0);
	} else {
		float	fDiffEnv = envLUT.b * ((NdotV + 1.0) * roughness - 0.5) + 1.0;
		ambient *= ((float3)(1.0) - f0) * fDiffEnv;
	}
	float	ao = pbrMap.b;
	float	specOcc = max( ( ao - 1.0 ) * ( ( NdotV * 1.125 - 2.625 ) * NdotV + 2.5 ) + 1.0, 0.0 );
	refl *= f * envLUT.g;

	// Diffuse
	color.rgb = ( diffuse * lightSourceDiffuse[0].rgb + ambient ) * albedo * ao;
	// Specular
	color.rgb += ( spec + refl ) * specOcc;

	// Emissive
	if ( lm.emissiveSettings.isEnabled ) {
		emissive *= emissiveIntensity( lm.emissiveSettings.adaptiveEmittance, lm.emissiveSettings.enableAdaptiveLimits, float4(lm.emissiveSettings.luminousEmittance, lm.emissiveSettings.exposureOffset, lm.emissiveSettings.maxOffsetEmittance, lm.emissiveSettings.minOffsetEmittance) );
	} else if ( lm.layeredEmissivity.isEnabled ) {
		emissive *= emissiveIntensity( lm.layeredEmissivity.adaptiveEmittance, lm.layeredEmissivity.enableAdaptiveLimits, float4(lm.layeredEmissivity.luminousEmittance, lm.layeredEmissivity.exposureOffset, lm.layeredEmissivity.maxOffsetEmittance, lm.layeredEmissivity.minOffsetEmittance) );
	}
	color.rgb += emissive;

	// Transmissive
	if ( lm.translucencySettings.isEnabled && lm.translucencySettings.isThin ) {
		transmissive *= albedo * ( (float3)(1.0) - f ) * ao;
		// TODO: implement flipBackFaceNormalsInViewSpace
		color.rgb += transmissive * lightSourceDiffuse[0].rgb * max( -NdotL, 0.0 );
		if ( hasCubeMap )
			color.rgb += CubeMap2.SampleLevel(_CubeMap2_sampler, -normalWS, 0.0).rgb * transmissive * lightSourceAmbient.rgb;
		else
			color.rgb += transmissive * lightSourceAmbient.rgb * 0.08;
	}

	color.rgb = tonemap( color.rgb * brightnessScale, toneMapScale );

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
    // the GLSL global initializers, which GLSL runs here (they read inputs, uniforms or globals)
    ViewDir_norm = normalize( ViewDir );
    btnMatrix_norm = float3x3( normalize( btnMatrix[0] ), normalize( btnMatrix[1] ), normalize( btnMatrix[2] ) );
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
