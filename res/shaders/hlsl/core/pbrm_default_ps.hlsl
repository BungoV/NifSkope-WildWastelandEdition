// HLSL source of pbrm_default.frag (fragment shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "CoreBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
static bool gl_FrontFacing;
#include "uniforms.hlsli"
#define WW_LOOSE_TYPES 1
#include "../air/lookdev_fog.hlsli"
#if defined(WW_SUNSHADOW)
#include "../shadows/ww_sunshadow.hlsli"
#endif
#if defined(WW_CELLLIGHTS)
#if defined(WW_CELLLIGHTS)
#ifndef WW_CELL_PBR
#define WW_CELL_PBR 1	// as below, before this file
#endif
#endif
#if defined(WW_CELLLIGHTS) && !defined(WW_CELL_GI_MARCH_COPIES)
#ifndef WW_CELL_GI_MARCH_ONCE
#define WW_CELL_GI_MARCH_ONCE 1	// as below, before this file
#endif
#endif
#if defined(WW_CELLLIGHTS)
#ifndef CELL_GI_ONEREAD
#define CELL_GI_ONEREAD 1	// as below, before this file
#endif
#endif
#if defined(WW_CELLLIGHTS)
#ifndef CELL_RESTIR_MISS_PIXEL
#define CELL_RESTIR_MISS_PIXEL 1	// as below, before this file
#endif
#endif
#include "../lights/cell_lights.hlsli"
#include "../lights/cell_cluster.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && defined(WW_DEBUG_VIEWS)
#include "../overlays/debug_view.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH))
#include "../ao/cell_gihalfat.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && !(defined(WW_CELL_PRT)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#include "../ao/cell_gihalfat.hlsli"
#endif
#if defined(WW_CELLLIGHTS)
#include "../ao/cell_ao.hlsli"
#include "../reflections/cell_ssr.hlsli"
#include "cell_grass.hlsli"
#include "land_shore.hlsli"
#include "land_wet.hlsli"
#include "../water/water_info.hlsli"
#include "../water/water_shore.hlsli"
#include "land_swash.hlsli"
#include "../weather/rain_wet.hlsli"
#endif
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "../air/lookdev_fog.hlsli"
#if defined(WW_SUNSHADOW)
#include "../shadows/ww_sunshadow.hlsli"
#endif
#if defined(WW_CELLLIGHTS)
#if defined(WW_CELLLIGHTS)
#ifndef WW_CELL_PBR
#define WW_CELL_PBR 1	// as below, before this file
#endif
#endif
#if defined(WW_CELLLIGHTS) && !defined(WW_CELL_GI_MARCH_COPIES)
#ifndef WW_CELL_GI_MARCH_ONCE
#define WW_CELL_GI_MARCH_ONCE 1	// as below, before this file
#endif
#endif
#if defined(WW_CELLLIGHTS)
#ifndef CELL_GI_ONEREAD
#define CELL_GI_ONEREAD 1	// as below, before this file
#endif
#endif
#if defined(WW_CELLLIGHTS)
#ifndef CELL_RESTIR_MISS_PIXEL
#define CELL_RESTIR_MISS_PIXEL 1	// as below, before this file
#endif
#endif
#include "../lights/cell_lights.hlsli"
#include "../lights/cell_cluster.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && defined(WW_DEBUG_VIEWS)
#include "../overlays/debug_view.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH))
#include "../ao/cell_gihalfat.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && !(defined(WW_CELL_PRT)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#include "../ao/cell_gihalfat.hlsli"
#endif
#if defined(WW_CELLLIGHTS)
#include "../ao/cell_ao.hlsli"
#include "../reflections/cell_ssr.hlsli"
#include "cell_grass.hlsli"
#include "land_shore.hlsli"
#include "land_wet.hlsli"
#include "../water/water_info.hlsli"
#include "../water/water_shore.hlsli"
#include "land_swash.hlsli"
#include "../weather/rain_wet.hlsli"
#endif
#undef WW_LOOSE_UNIFORMS
#if defined(WW_CELLLIGHTS)
	float4 landLod;	// lane LANDSPEC2: as fo4_default.frag (src/features/far-ground/landlodspec.h), for ground with a .pbrm
	float4 landLodK;
#endif
	// Constants stand in wherever the corresponding channel is overridden. These
	// mirror the reader's resolved values (src/io/pbrmfile.h).
	float3 pbrBaseColor;
	float pbrOpacity;
	float pbrRoughness;
	float pbrMetallic;
	float pbrAo;
	float pbrF0;            // the untinted F0 of a weight-1 surface (v6 IorF0(ior), v4/v5 min(f0, .16))
	bool pbrSpecV6;         // bit 7 is the specular WEIGHT map (v6), not an F0 map (v4/v5)
	float pbrSpecWeight;    // v6 specularWeight (1 for v4/v5)
	float3 pbrSpecTint;       // the constant specular colour, linear (white = none)
	float pbrIorMax;        // the IOR map's ceiling
	float pbrDiffuseRoughness;	// EON above 0
	float pbrNormalStrength;
	float3 pbrEmissiveColor;		// raw sRGB as authored; decoded pow 2.2 here (the editor's)
	float pbrEmissiveIntensity;	// luminance / 100
	float pbrEmissiveMask;		// the constant A mask
	bool pbrEmissiveMapColor;	// the map's RGB replaces the constant (!overrideColor)
	bool pbrEmissiveMapMask;	// the map's A replaces the constant mask (!overrideMask)
	// Tint masks: the editor's special layer 0 (ED:2310-2316)
	bool pbrTintEnabled;
	int pbrTintUseTex;			// bit c: mask c comes from TintMaskMap
	float4 pbrTintMasks;			// the constant masks
	float3 pbrTintColors[4];		// raw sRGB (Q13: the editor does not decode them)
	int pbrTintMode;			// 0 Normalize, 1 Add, 2 Priority RGBA
	// Composition: -1 = the NIF alpha property (derived/legacy shapes);
	// 0 Opaque, 1 Alpha Test, 2 Alpha Blend, 3 Premultiplied, 4 Additive, 5 Multiply,
	// 6 Physical Transmission, 7 Water (6/7 blend like 2 until the merge)
	int pbrComposition;
	float pbrGlobalOpacity;
	float pbrAlphaThreshold;
	bool pbrAlphaConst;			// alphaSource "Constant"
	// Feature bits, matching PbrmMaterial::Feature so the shader never guesses which
	// channels are real. Bit set = sample the texture; clear = use the constant.
	int pbrFeatures;
	bool hasCubeMap;
	float envReflection;
	// Scene lighting. Only this program reads these, so the
	// legacy programs cannot move. sceneMode 0 = Legacy, 1 = Studio, 2 = Lookdev (Studio + the weather DALC).
	int sceneMode;
	float sceneExposure;		// 2^EV, Studio only
	int viewTransform;			// 0 Standard (clamp + sRGB), 1 AgX, 2 Khronos PBR Neutral
	bool baseIsSrgbTex;			// the bound base texture is sRGB-tagged: GL already decoded it
	bool emissiveIsSrgbTex;
	bool specIsSrgbTex;
	bool hasStudioCube;
	float studioProbe;			// >= 0: replace the lit result by this linear value (debug output)
	float3 lookdevDalc[6];		// Lookdev (sceneMode 2): DALC ambient, linear, X+ X- Y+ Y- Z+ Z-
	bool lookdevDalcFlip;		// debug "dalcflip": the other axis-sign convention
	float studioSun;			// WW_STUDIO_SUN: the Studio/Lookdev sun scale (0 = the white furnace)
	int r3Term;					// WW_R3_TERM: 0 all, 1 diffuse only, 2 specular only
	int r3Red;					// WW_R3_RED: 1 noms, 2 nosplit, 4 fo4csweight, 8 notint, 16 f90scaled, 32 lambert
	int r4Red;					// WW_R4_RED: 1 nodiv, 2 emitmul, 4 notintmask, 8 emitraw, 16 nocomp
	float alpha;
	int alphaFlags;
	bool landSplat;		// vertex alpha = this blended-ground pass's share
	int landSplatDbg;	// lane ALLON2: 0 edge guard, 1 red WW_TERRBLEND_RED=edgepass (no guard), 2 probe WW_TERRBLEND_PROBE=edge
	float alphaThreshold;
	float2 uvScale;
	float2 uvOffset;
};
#include "../air/lookdev_fog.hlsli"
#ifdef WW_SUNSHADOW
#include "../shadows/ww_sunshadow.hlsli"
#endif
#ifdef WW_CELLLIGHTS
#define WW_CELL_PBR 1
#ifndef WW_CELL_GI_MARCH_COPIES	// red (tests/spells/pbrm_headroom.sh): a march copy per call site, the old shader
#define WW_CELL_GI_MARCH_ONCE 1	// lane PBRMCSMHEADROOM: the march (PRT red + march red) once a fragment, as fo4_default
#endif
#define CELL_GI_ONEREAD 1	// DDGIREADPERF1: one volume read site (cellGiFirst in main, cell_lights.glsl)
#define CELL_RESTIR_MISS_PIXEL 1	// MODULES1 R7 item 2: a surface off the traced depth takes the pixel's shadow ratio (cellShadowF)
#include "../lights/cell_lights.hlsli"
#include "../ao/cell_ao.hlsli"
#include "../reflections/cell_ssr.hlsli"
#include "cell_grass.hlsli"
// lane WATERWETBAKE1: the land side of the shore plane; WATERLOOK1 step 13: the Wet shore band on it
#include "land_shore.hlsli"
#include "land_wet.hlsli"
// lane WATERLOOK1 step 14: the Wave run-up row reads the baked water's bodies and wave heights too
#include "../water/water_info.hlsli"
#include "../water/water_shore.hlsli"
#include "land_swash.hlsli"
#include "../weather/rain_wet.hlsli"
#define WW_CELL_FOGGED cellOn	// a cell-lit draw is fogged before its imagespace
Texture2D<float4> CellSpecMap : register(t49, space0); SamplerState _CellSpecMap_sampler : register(s49, space0);	// a legacy material's specular map (R scale, G smoothness), for the cube term
#else
#define WW_CELL_FOGGED false
#endif

// PBRM (PBR Material Editor) metallic/roughness path: the FO4CS runtime BRDF (truepbr_brdf.hlsli) with the v6
// specular, evaluated from one resolved surface (evalSurface). GGX + height-correlated Smith, Lazarov env DFG with
// multiscatter, Burley or EON diffuse, OpenPBR v1.1.1 specular weight; tint/emission/opacity follow the editor.

Texture2D<float4> BaseMap : register(t50, space0); SamplerState _BaseMap_sampler : register(s50, space0);      // sRGB RGB, A opacity
Texture2D<float4> NormalMap : register(t51, space0); SamplerState _NormalMap_sampler : register(s51, space0);    // linear RG (+B height, A curvature)
Texture2D<float4> RmaosMap : register(t52, space0); SamplerState _RmaosMap_sampler : register(s52, space0);     // R rough, G metal, B AO, A dielectric F0/porosity
Texture2D<float4> EmissiveMap : register(t53, space0); SamplerState _EmissiveMap_sampler : register(s53, space0);  // sRGB RGB, A intensity mask
Texture2D<float4> SpecColorMap : register(t54, space0); SamplerState _SpecColorMap_sampler : register(s54, space0); // v6: sRGB RGB specular tint (bit 25), A = IOR / iorMax (bit 30)
Texture2D<float4> TintMaskMap : register(t55, space0); SamplerState _TintMaskMap_sampler : register(s55, space0);  // linear RGBA: the four tint masks (special layer 0)
TextureCube<float4> CubeMap : register(t56, space0); SamplerState _CubeMap_sampler : register(s56, space0);
#ifdef WW_CELLLIGHTS
static float2 farLadder;	// lane LODLIVE1: the LOD coloration's band (fo4_default.vert)
#endif

#define F_BASECOLOR   1
#define F_NORMAL      2
#define F_HEIGHTBLUE  4
#define F_RMAOS       8
#define F_ROUGHNESS   16
#define F_METALLIC    32
#define F_AO          64
#define F_F0          128
#define F_EMISSIVE    256
#define F_OPACITY     2048
#define F_SPECCOLOR   0x2000000	// bit 25: the spec map's RGB is the tint
#define F_SPECIOR     0x40000000	// bit 30: the spec map's A x iorMax is the IOR


TextureCube<float4> StudioCube : register(t57, space0); SamplerState _StudioCube_sampler : register(s57, space0);		// SFCubeMapCache GGX-prefiltered specular cube (Studio)
TextureCube<float4> IrradianceMap : register(t58, space0); SamplerState _IrradianceMap_sampler : register(s58, space0);	// its 32 px diffuse cube (Studio)

static float3 LightDir;
static float3 ViewDir;
static float2 texCoord;

static float4 A;
static float4 C;
static float rawVertexAlpha;	// the vertex color's own alpha (fo4_default.vert)
static float wwFarSkyVis;	// MODULES1 10c: a far draw's baked sky factor (fo4_default.vert), 1 elsewhere
static float wwFarAo;	// MODULES1 10c: a far draw's baked AO (fo4_default.vert farVcAo), 1 elsewhere
static float4 D;

static float3x3 btnMatrix;
static float3x3 reflMatrix;

static float4 fragColor;
#ifdef WW_CELLLIGHTS
// lane SSGI2: the scene pass's second target, the emitted share alone (the bounce's emissive cast; 0 elsewhere)
static float4 cellEmitOut;
#endif

#ifndef M_PI
	#define M_PI 3.1415926535897932384626433832795
#endif

static float3 ViewDir_norm	/* = normalize( ViewDir ) : set in main() after the stage inputs */;
static float3x3 btnMatrix_norm	/* = float3x3( normalize( btnMatrix[0] ), normalize( btnMatrix[1] ), normalize( btnMatrix[2] ) ) : set in main() after the stage inputs */;

bool hasFeature( int bit ) { return ( pbrFeatures & bit ) != 0; }

float pow5( float x )
{
	float x2 = x * x;
	return x2 * x2 * x;
}

float3 fresnelSchlick( float c, float3 F0 )
{
	return F0 + ( (float3)( 1.0 ) - F0 ) * pow5( clamp( 1.0 - c, 0.0, 1.0 ) );
}

// OpenPBR F82 conductor Fresnel (the editor's, materialpreviewwidget.cpp:1789):
// Schlick bent so the 82-degree reflectance is tint x Schlick(82); tint 1 = Schlick.
float3 fresnelF82( float mu, float3 f0, float3 tint )
{
	const float mb = 1.0 / 7.0;
	float3 fs = fresnelSchlick( mu, f0 );
	float3 fsb = fresnelSchlick( mb, f0 );
	float3 b = fsb * ( (float3)( 1.0 ) - tint ) / ( mb * pow( 1.0 - mb, 6.0 ) );
	return clamp( fs - b * mu * pow( 1.0 - mu, 6.0 ), 0.0, 1.0 );
}

// Lazarov's analytic environment DFG (FO4CS truepbr_brdf.hlsli:90-93), perceptual
// roughness: env specular = F0 * x + y. No LUT texture.
float2 dfgLazarov( float NdotV, float r )
{
	float4 rr = r * float4( -1.0, -0.0275, -0.572, 0.022 ) + float4( 1.0, 0.0425, 1.04, -0.04 );
	float a004 = min( rr.x * rr.x, exp2( -9.28 * NdotV ) ) * rr.x + rr.y;
	return float2( -1.04, 1.04 ) * a004 + rr.zw;
}

// EON directional albedo (Portsmouth-Kutz-Hill 2024 fit; the editor's eonAlbedo)
float eonAlbedo( float mu, float r )
{
	float mc = 1.0 - mu, mc2 = mc * mc;
	float g = 0.0571085289 * mc + 0.491881867 * mc2 + ( -0.332181442 * mc + 0.0714429953 * mc2 ) * mc2;
	return ( 1.0 + r * g ) / ( 1.0 + 0.287861 * r );
}

float3 srgbToLinear( float3 c )
{
	return lerp( c / 12.92, pow( ( c + 0.055 ) / 1.055, (float3)( 2.4 ) ), step( (float3)( 0.04045 ), c ) );
}

// the sRGB OETF: the encode is here, never GL_FRAMEBUFFER_SRGB
float3 linearToSrgb( float3 c )
{
	c = clamp( c, 0.0, 1.0 );
	return lerp( c * 12.92, 1.055 * pow( c, (float3)( 1.0 / 2.4 ) ) - 0.055, step( (float3)( 0.0031308 ), c ) );
}

// AgX, Benjamin Wrensch's minimal fit (sRGB display out, the "base" look)
float3 agxContrast( float3 x )
{
	float3 x2 = x * x;
	float3 x4 = x2 * x2;
	return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}

float3 agxDisplay( float3 c )
{
	const float3x3 agxMat = float3x3( 0.842479062253094, 0.0423282422610123, 0.0423756549057051,
	                          0.0784335999999992, 0.878468636469772, 0.0784336,
	                          0.0792237451477643, 0.0791661274605434, 0.879142973793104 );
	const float3x3 agxMatInv = float3x3( 1.19687900512017, -0.0528968517574562, -0.0529716355144438,
	                             -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
	                             -0.0990297440797205, -0.0989611768448433, 1.15107367264116 );
	const float minEv = -12.47393;
	const float maxEv = 4.026069;
	c = mul(max( c, (float3)( 1e-10 ) ), agxMat);
	c = clamp( log2( c ), minEv, maxEv );
	c = ( c - minEv ) / ( maxEv - minEv );
	c = agxContrast( c );
	return clamp( mul(c, agxMatInv), 0.0, 1.0 );
}

// Khronos PBR Neutral (linear in, linear out)
float3 pbrNeutral( float3 color )
{
	const float startCompression = 0.8 - 0.04;
	const float desaturation = 0.15;
	float x = min( color.r, min( color.g, color.b ) );
	float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
	color -= offset;
	float peak = max( color.r, max( color.g, color.b ) );
	if ( peak < startCompression )
		return color;
	const float d = 1.0 - startCompression;
	float newPeak = 1.0 - d * d / ( peak + d - startCompression );
	color *= newPeak / peak;
	float g = 1.0 - 1.0 / ( desaturation * ( peak - newPeak ) + 1.0 );
	return lerp( color, (float3)( newPeak ), g );
}

float3 studioOutput( float3 c )
{
	c = max( c, (float3)( 0.0 ) ) * sceneExposure;
	if ( viewTransform == 1 )
		return agxDisplay( c );
	if ( viewTransform == 2 )
		return linearToSrgb( pbrNeutral( c ) );
	return linearToSrgb( c );	// Standard = "tonemap None"
}

// Same Hable/Uncharted curve as the FO4 path, so PBRM and BGSM materials in one scene tonemap identically.
float3 tonemap( float3 x )
{
	float a = 0.15;
	float b = 0.50;
	float c = 0.10;
	float d = 0.20;
	float e = 0.02;
	float f = 0.30;

	float3 z = x * x * D.a * ( A.a * 4.22978723 );
	z = ( z * ( a * z + b * c ) + d * e ) / ( z * ( a * z + b ) + d * f ) - e / f;
	return sqrt( z / ( A.a * 0.93333333 ) );
}

/* ---- the resolved surface (evalSurface) ---- */
struct Surface
{
	float3 base;			// linear base colour
	float3 N;				// view-space shading normal
	float rough;		// perceptual roughness, floored
	float metal;
	float ao;
	float weight;		// dielectric specular weight
	float3 f0d;			// tint x F0_ior: the dielectric F0 at weight 1
	float3 tint;			// the specular colour (the F82 edge tint of metals)
	float diffRough;	// EON roughness, 0 = Burley
};

/* The specular weight, OpenPBR v1.1.1: F0' = clamp( w F0(ior) ), and the dielectric Fresnel is that of
 * ior' = (1 + sqrt F0') / (1 - sqrt F0'). F90' = sat( 50 F0' ) keeps the grazing rise yet reaches 0 at weight 0.
 * Debug: "fo4csweight" = F90 = 1 at every weight, "f90scaled" = w x Schlick. */
float3 dielF0( Surface s ) { return clamp( s.weight * s.f0d, 0.0, 1.0 ); }
float3 dielF90( Surface s )
{
	if ( ( r3Red & 4 ) != 0 )
		return (float3)( 1.0 );
	return clamp( 50.0 * dielF0( s ), 0.0, 1.0 );
}

// The whole specular Fresnel of the surface at cosine c
float3 surfaceF( Surface s, float c )
{
	float3 fd;
	if ( ( r3Red & 16 ) != 0 ) {
		fd = s.weight * fresnelSchlick( c, s.f0d );
	} else {
		float3 F0 = dielF0( s );
		fd = F0 + ( dielF90( s ) - F0 ) * pow5( clamp( 1.0 - c, 0.0, 1.0 ) );
	}
	// metals: the weight scales the F82 conductor Fresnel (the specular colour is its edge tint)
	return lerp( fd, s.weight * fresnelF82( c, s.base, s.tint ), s.metal );
}

// The specular F0 the multiscatter term takes (FO4CS: lerp(F0_diel, base, metal))
float3 surfaceF0( Surface s )
{
	return lerp( s.weight * s.f0d, s.weight * s.base, s.metal );
}

// Multiscatter energy compensation (FO4CS truepbr_brdf.hlsli:104-105). Debug "noms": 1.
float3 multiScatter( Surface s, float2 dfg )
{
	if ( ( r3Red & 1 ) != 0 )
		return (float3)( 1.0 );
	float Ess = max( clamp( dfg.x + dfg.y, 0.0, 1.0 ), 0.05 );
	return (float3)( 1.0 ) + clamp( surfaceF0( s ), 0.0, 1.0 ) * ( 1.0 / Ess - 1.0 );
}

// E_spec: the specular lobe's directional albedo, multiscatter included -- the env
// specular scale and the energy the indirect diffuse gives up
float3 specAlbedo( Surface s, float2 dfg, float3 ms )
{
	// the split sum of Schlick( F0, F90 ) is F0 A + F90 B
	float3 diel = ( ( r3Red & 16 ) != 0 ) ? s.weight * ( s.f0d * dfg.x + dfg.y ) : dielF0( s ) * dfg.x + dielF90( s ) * dfg.y;
	float3 metl = s.weight * ( s.base * dfg.x + dfg.y * s.tint );	// the editor's F82 split-sum: B x tint
	return lerp( diel, metl, s.metal ) * ms;
}

// One directional light: diffuse and specular BRDF x NoL (radiance per unit irradiance). Lane LIGHTPHYS1: the
// specular lobe takes Ls and roughness rS (an area light's representative point); the diffuse keeps L.
void directLightA( Surface s, float3 L, float3 Ls, float rS, float3 V, float3 ms, out float3 dDiff, out float3 dSpec )
{
	float3 H = normalize( Ls + V );
	float NoL = clamp( dot( s.N, L ), 0.0, 1.0 );
	float NoLs = clamp( dot( s.N, Ls ), 0.0, 1.0 );
	float NoV = clamp( dot( s.N, V ), 1e-4, 1.0 );
	float NoH = clamp( dot( s.N, H ), 0.0, 1.0 );
	float VoH = clamp( dot( V, H ), 0.0, 1.0 );
	float LoH = clamp( dot( L, H ), 0.0, 1.0 );

	float alpha = rS * rS;
	float a2 = alpha * alpha;
	float d = max( NoH * NoH * ( a2 - 1.0 ) + 1.0, 1e-4 );
	float D = a2 / ( M_PI * d * d );
	float lambdaV = NoLs * sqrt( NoV * NoV * ( 1.0 - a2 ) + a2 );
	float lambdaL = NoV * sqrt( NoLs * NoLs * ( 1.0 - a2 ) + a2 );
	float Vis = 0.5 / max( lambdaV + lambdaL, 1e-4 );
	float3 F = surfaceF( s, VoH );
	dSpec = D * Vis * F * ms * NoLs;

	float3 rho = ( 1.0 - s.metal ) * s.base;
	float3 keep = clamp( (float3)( 1.0 ) - F, 0.0, 1.0 );
	if ( s.diffRough > 0.0 ) {
		// EON (OpenPBR base_diffuse_roughness; the editor's lines 2683-2689)
		float dr = s.diffRough, AF = 1.0 / ( 1.0 + 0.287861 * dr );
		float sON = dot( L, V ) - NoL * NoV;
		float stinv = sON > 0.0 ? sON / max( max( NoL, NoV ), 0.001 ) : sON;
		float3 fss = rho / M_PI * AF * ( 1.0 + dr * stinv );
		float EFo = eonAlbedo( NoV, dr ), EFi = eonAlbedo( NoL, dr ), avgEF = AF * ( 1.0 + 0.072488 * dr );
		float3 rhoMs = rho * rho * avgEF / max( (float3)( 1.0 ) - rho * ( 1.0 - avgEF ), (float3)( 0.001 ) );
		float3 fms = rhoMs / M_PI * max( 0.01, 1.0 - EFo ) * max( 0.01, 1.0 - EFi ) / max( 0.01, 1.0 - avgEF );
		dDiff = keep * ( fss + fms ) * NoL;
	} else {
		// Burley, the game's (FO4CS truepbr_brdf.hlsli:57-66), with the specular energy removed.
		// Debug "lambert": fd = 1.
		float f90 = 0.5 + 2.0 * s.rough * LoH * LoH;
		float fd = ( 1.0 + ( f90 - 1.0 ) * pow5( 1.0 - NoL ) ) * ( 1.0 + ( f90 - 1.0 ) * pow5( 1.0 - NoV ) );
		if ( ( r3Red & 32 ) != 0 )
			fd = 1.0;
		dDiff = keep * rho / M_PI * fd * NoL;
	}
}

void directLight( Surface s, float3 L, float3 V, float3 ms, out float3 dDiff, out float3 dSpec )
{
	directLightA( s, L, L, s.rough, V, ms, dDiff, dSpec );
}

void frag_main()
{
#ifdef WW_CELLLIGHTS
	cellEmitOut = (float4)( 0.0 );
#endif
	float2 offset = texCoord.xy * uvScale + uvOffset;

	// --- base color / opacity ---
	float3 base = pbrBaseColor;
	float op = pbrOpacity;
	if ( hasFeature( F_BASECOLOR ) ) {
		float4 bc = BaseMap.Sample( _BaseMap_sampler, offset );
		// decoded as sRGB whatever the DXGI tag says: an sRGB-tagged
		// texture arrives decoded by GL, an UNORM one is decoded here
		base = ( baseIsSrgbTex ? bc.rgb : srgbToLinear( bc.rgb ) ) * pbrBaseColor;
		// Alpha is coverage only when the material says so: legacy diffuse alpha is often zero or unrelated.
		if ( hasFeature( F_OPACITY ) )
			op = ( pbrComposition >= 0 ) ? bc.a : bc.a * pbrOpacity;	// the editor's baseAlpha replaces (ED:2258)
	}

	// --- tint masks (ED:2310-2316, verbatim) ---
	if ( pbrTintEnabled && ( r4Red & 4 ) == 0 ) {
		float4 masks = pbrTintMasks;
		if ( pbrTintUseTex != 0 ) {
			float4 tm = TintMaskMap.Sample( _TintMaskMap_sampler, offset );
			if ( ( pbrTintUseTex & 1 ) != 0 ) masks.r = tm.r;
			if ( ( pbrTintUseTex & 2 ) != 0 ) masks.g = tm.g;
			if ( ( pbrTintUseTex & 4 ) != 0 ) masks.b = tm.b;
			if ( ( pbrTintUseTex & 8 ) != 0 ) masks.a = tm.a;
		}
		float total = dot( masks, (float4)( 1.0 ) );
		if ( pbrTintMode == 0 && total > 1.0 && ( r4Red & 1 ) == 0 )
			masks /= total;
		else if ( pbrTintMode == 2 )
			masks = float4( masks.r, masks.g * ( 1.0 - masks.r ), masks.b * ( 1.0 - masks.r ) * ( 1.0 - masks.g ),
				masks.a * ( 1.0 - masks.r ) * ( 1.0 - masks.g ) * ( 1.0 - masks.b ) );
		float3 tintMix = max( (float3)( 0.0 ), (float3)( 1.0 - dot( masks, (float4)( 1.0 ) ) ) + pbrTintColors[0] * masks.r
			+ pbrTintColors[1] * masks.g + pbrTintColors[2] * masks.b + pbrTintColors[3] * masks.a );
		base *= tintMix;
	}
	base *= C.rgb;

	float4 color = float4( base, 1.0 );
	if ( pbrComposition >= 0 && ( r4Red & 16 ) == 0 ) {
		// the .pbrm composition (ED:2282-2283, 2336-2337); vertex alpha is not read (the editor's)
		float opacity = ( pbrAlphaConst ? 1.0 : op ) * pbrGlobalOpacity;
		if ( pbrComposition == 0 )
			opacity = 1.0;
		else if ( pbrComposition == 1 ) {
			if ( opacity < pbrAlphaThreshold )
				discard;
			opacity = 1.0;
		}
		color.a = opacity;
#ifdef WW_CELLLIGHTS
		if ( wwGrassFadeK( ViewDir ) <= 0.0 )	// a composed grass material past its fade
			discard;
#endif
	} else if ( alphaFlags > 0 ) {
#ifdef WW_CELLLIGHTS
		float a = wwGrassVertexA( C.a ) * op * alpha;
		a *= wwGrassFadeK( ViewDir );	// exactly 1.0 on every non-grass shape
#else
		float a = C.a * op * alpha;
#endif
		int m = ( a < alphaThreshold ? 0x2B2B : ( a > alphaThreshold ? 0x7171 : 0x4D4D ) );
		if ( ( m & ( 1 << alphaFlags ) ) == 0 )
			discard;
		if ( ( alphaFlags & 8 ) != 0 )
			color.a = a;
	}

	// --- normal ---
	// (s*255 - 128)/127 like the game's unpack, Z rebuilt (B is
	// height when the material says so, never a normal component)
	float3 normal = float3( 0.0, 0.0, 1.0 );
	if ( hasFeature( F_NORMAL ) ) {
		float4 nm = NormalMap.Sample( _NormalMap_sampler, offset );
		normal.rg = clamp( ( nm.rg * 255.0 - 128.0 ) / 127.0, -1.0, 1.0 ) * pbrNormalStrength;
		normal.b = sqrt( max( 1.0 - dot( normal.rg, normal.rg ), 0.0 ) );
	}
	// back faces: the GEOMETRIC normal flips before the tangent frame is applied
	float3x3 tbn = btnMatrix_norm;
	if ( !gl_FrontFacing )
		tbn[2] = -tbn[2];

	// --- evalSurface ---
	Surface s;
	s.base = base;
	s.N = normalize( mul(normal, tbn) );
#ifdef WW_CELLLIGHTS
	if ( cellOn && cellProbe == 20 && ( or( cellAoEarly == 1, cellAoEarly == 4 ) ) ) {	// lane PERF1: the obscurance pass reads the normal and depth only
		fragColor = cellAoPassOut( s.N, -ViewDir );
		return;
	}
#endif
	s.rough = pbrRoughness;
	s.metal = pbrMetallic;
	s.ao = pbrAo;
	s.weight = pbrSpecWeight;
	float f0ior = pbrF0;
	if ( hasFeature( F_RMAOS ) ) {
		float4 rm = RmaosMap.Sample( _RmaosMap_sampler, offset );
		if ( hasFeature( F_ROUGHNESS ) ) s.rough = rm.r;
		if ( hasFeature( F_METALLIC ) )  s.metal = rm.g;
		if ( hasFeature( F_AO ) )        s.ao    = rm.b;
		// bit 7: v6 the specular weight map, v4/v5 the dielectric F0 (clamped 0.16);
		// the reader clears it when alpha carries porosity
		if ( hasFeature( F_F0 ) ) {
			if ( pbrSpecV6 )
				s.weight = rm.a;
			else
				f0ior = min( rm.a, 0.16 );
		}
	}
	s.tint = pbrSpecTint;
	if ( or( hasFeature( F_SPECCOLOR ), hasFeature( F_SPECIOR ) ) ) {
		float4 sc = SpecColorMap.Sample( _SpecColorMap_sampler, offset );
		if ( hasFeature( F_SPECCOLOR ) )
			s.tint = specIsSrgbTex ? sc.rgb : srgbToLinear( sc.rgb );
		if ( hasFeature( F_SPECIOR ) ) {
			float ior = sc.a * pbrIorMax;
			float r = ( ior - 1.0 ) / ( ior + 1.0 );
			f0ior = min( r * r, 1.0 );
		}
	}
	if ( ( r3Red & 8 ) != 0 )
		s.tint = (float3)( 1.0 );
	s.f0d = s.tint * f0ior;
	s.rough = clamp( s.rough, 0.035, 1.0 );	// the design's floor (FO4CS uses 0.045)
	s.metal = clamp( s.metal, 0.0, 1.0 );
	s.weight = clamp( s.weight, 0.0, 1.0 );
	s.diffRough = clamp( pbrDiffuseRoughness, 0.0, 1.0 );
#if defined( WW_CELLLIGHTS ) && !defined( WW_LANDSPEC_NONE )
	// lane LANDSPEC2: the LOD land spec law; gloss 0.22 taken as roughness 0.78
	float landLodV = landLod.x > 1.5 ? 1.0 : ( landLod.x > 0.5
		? 1.0 - clamp( ( landLodK.z - distance( cellWorldPos( -ViewDir ).xy, landLod.yz ) ) * landLodK.w, 0.0, 1.0 ) : 0.0 );
	/* lane LANDSPEC3: the roughness whose GGX lobe matches the legacy draw's Blinn-Phong power 2^(10 g + 1) at the
	 * LOD gloss g (alpha^2 = 2 / (n + 2), rough = sqrt(alpha)): 0.650 at 0.22, not 1 - g = 0.78 */
	s.rough = lerp( s.rough, sqrt( sqrt( 2.0 / ( exp2( landLodK.x * 10.0 + 1.0 ) + 2.0 ) ) ), landLodV );
	s.weight = lerp( s.weight, landLodK.y, landLodV );
#elif defined( WW_CELLLIGHTS )
	const float landLodV = 0.0;
#endif
#ifdef WW_CELLLIGHTS
	if ( cellOn && ( or( landWetOn, landSwashOn ) ) ) {	// lane WATERLOOK1 steps 13-14: the Wet shore and Wave run-up rows
		float3 Pl = cellWorldPos( -ViewDir ), Nl = cellWorldDir( normalize( tbn[2] ) );
		float2 wet = wwLandWet( Pl, Nl );
		float4 sw = wwLandSwash( Pl, Nl, landSwashTime );
		wet = max( wet, sw.zx );
		float por = landShoreTerrain ? 1.0 - s.metal : 0.0;
		s.base *= wwLandWetBase( wet.x, por );
		s.base = lerp( s.base, (float3)( 0.8 ), sw.y );	// the run-up's foam
		s.rough = max( wwLandWetRough( s.rough, wet, por ), 0.035 );
	}
	if ( cellOn && rainWet.x > 0.0 ) {	// lane [reference game]WEATHER1: rain wetness, [reference game]'s law (metal from the F0)
		float rw = wwRainWetW( 1.0 - s.rough, wwRainWetMetal( lerp( s.weight * s.f0d, s.base, s.metal ) ) )
			* wwRainWetExposed( cellWorldPos( -ViewDir ), cellWorldDir( normalize( tbn[2] ) ) );	// [reference game]WEATHER2: dry under cover
		s.base *= 1.0 - 0.8 * rw;
		s.rough = max( ( 1.0 - 0.4 * rw ) * s.rough, 0.035 );
	}
#endif

	float3 V = ViewDir_norm;
	float3 R = reflect( -V, s.N );
	float NdotV = clamp( dot( s.N, V ), 1e-4, 1.0 );
	float2 dfg = dfgLazarov( NdotV, s.rough );
	float3 ms = multiScatter( s, dfg );
	float3 Espec = specAlbedo( s, dfg, ms );
	// the indirect diffuse gives up the energy the specular lobe took. Debug "nosplit".
	float3 keepInd = ( ( r3Red & 2 ) != 0 ) ? (float3)( 1.0 ) : clamp( (float3)( 1.0 ) - Espec, 0.0, 1.0 );
	float3 rho = s.base * ( 1.0 - s.metal );

	// Linear throughout. The sun's irradiance is the uploaded diffuse color x PI, so a white Lambert surface
	// facing a sun of 1 reads 1; D.rgb is sqrt(diffuse), so read the uniform rather than square a sqrt.
	float3 sunE = lightSourceDiffuse[0].rgb * M_PI * ( sceneMode >= 1 ? studioSun : 1.0 );
	float3 dDiff, dSpec;
	directLight( s, normalize( LightDir ), V, ms, dDiff, dSpec );
	float3 outDiff = dDiff * sunE;
	float3 outSpec = dSpec * sunE;
#ifdef WW_SUNSHADOW
	// the cascade factor on the SUN's diffuse and specular only
	float csmF = wwSunShadow( -ViewDir );
	outDiff *= csmF;
	if ( ( csmRed & 4 ) == 0 )
		outSpec *= csmF;
#endif
	// FOGSUN1: the sun through the air (1 where the air is off)
	float airSunT = wwVolAirSunT( -ViewDir );
	outDiff *= airSunT;
	outSpec *= airSunT;

	// --- ambient + environment ---
	// AO belongs on ambient only: applying it to a direct lobe double-darkens contact shadows.
	s.ao *= clamp( wwFarSkyVis, 0.0, 1.0 );	// MODULES1 10c: the far bake's sky occlusion on the ambient (1 elsewhere)
	float3 preAmbDiff = outDiff;	// what the block below adds is the ambient's diffuse
	float3 preAmbSpec = outSpec;	// lane SPECOCC1: what the block below adds is the cube reflection
#ifdef WW_DV
	wwDvSun = preAmbDiff;	// lane DEBUGVIEW1: outdoors the key light's direct terms (the cascades and the air in)
	wwDvSpec = preAmbSpec;
#endif
	if ( sceneMode >= 1 && hasStudioCube ) {
		// Studio: the irradiance cube is the radiance average, so Lambert takes it without 1/PI.
		// [reference game]'s roughness->lod law m = r*(10-4r) over the 7 filtered levels (f76_default.frag:188).
		float3 irr = IrradianceMap.Sample( _IrradianceMap_sampler, mul(s.N, reflMatrix) ).rgb;
		if ( sceneMode == 2 ) {
			// Lookdev: the weather's DALC 6-axis ambient on the WORLD normal (Z up); an up-facing normal
			// takes the Z- color (measured). The engine blends linearly in gamma space; this n^2 pick approximates it.
			float3 wn = mul(s.N, envMapRotation);
			float3 n2 = wn * wn;
			float3 ax = ( ( wn.x > 0.0 ) != lookdevDalcFlip ) ? lookdevDalc[1] : lookdevDalc[0];
			float3 ay = ( ( wn.y > 0.0 ) != lookdevDalcFlip ) ? lookdevDalc[3] : lookdevDalc[2];
			float3 az = ( ( wn.z > 0.0 ) != lookdevDalcFlip ) ? lookdevDalc[5] : lookdevDalc[4];
			irr = n2.x * ax + n2.y * ay + n2.z * az;
		}
		outDiff += irr * rho * keepInd * s.ao;
		float3 env = StudioCube.SampleLevel( _StudioCube_sampler, mul(R, reflMatrix), s.rough * ( 10.0 - 4.0 * s.rough ) ).rgb;
		outSpec += env * Espec * s.ao;
	} else {
		// Legacy (or Studio without a cube): A.rgb = sqrt(ambient) * 0.375, so A.rgb^2 is the
		// ambient the legacy shapes see in linear terms.
		outDiff += A.rgb * A.rgb * rho * keepInd * s.ao;
		if ( hasCubeMap ) {
			float3 cube = CubeMap.SampleLevel( _CubeMap_sampler, mul(R, reflMatrix), s.rough * 8.0 ).rgb;
			// the legacy cube is display-encoded: squared into the same linear space
			outSpec += cube * cube * envReflection * Espec * s.ao;
		}
	}
#ifdef WW_SUNSHADOW
	if ( ussPbrDim != 0 ) {
		// lane GAPLOOK1 (TERRSHADOWLEFT): FO4CS's terrain-shadow dimming of the ambient and the environment (red all: the sun too)
		float3 ad;
		float sd;
		ussTerrainDim( ad, sd );
		outDiff = ( ussPbrDim == 2 ? preAmbDiff * ad : preAmbDiff ) + ( outDiff - preAmbDiff ) * ad;
		outSpec = ( ussPbrDim == 2 ? preAmbSpec * sd : preAmbSpec ) + ( outSpec - preAmbSpec ) * sd;
	}
#endif
#ifdef WW_CELLLIGHTS
	/* the cell's own lights through this BRDF (irradiance x PI, as the sun above) and the bake's bounce.
	 * Interior: they replace the viewport light and ambient (DALC for the cube); exterior: they add. */
	float3 cubeK = (float3)( 0.0 );	// the interior cube reflection before its light (probe 50)
#ifdef CELL_GI_ONE
	cellGiOneRead = true;	// DDGIREADPERF1: every cellGiSample below takes cellGiFirst's answer (unconditional: no read compiled there)
#endif
	if ( cellOn ) {
		float3 Pw = cellWorldPos( -ViewDir );
		Surface sw = s;
		sw.N = cellWorldDir( s.N );
		float3 Vw = cellWorldDir( V );
#if defined( WW_CELL_GI_MARCH ) && defined( WW_CELL_GI_MARCH_ONCE )
		// lane PBRMCSMHEADROOM: the one GI read, at the (P, N) every later caller passes (cellPassOut's nonormal red: up)
		if ( cellGiOn )
			cellGiMarchPrime( Pw, cellPass > 0 && cellPass != 6 && cellProbe == 0 && ( cellPassRed & 2 ) != 0
			                      ? float3( 0.0, 0.0, 1.0 ) : sw.N );
#endif
		float3 cDiff = (float3)( 0.0 ), cSpec = (float3)( 0.0 ), cE = (float3)( 0.0 );
		float3 farF, farGP;	// the real lights fade over the band, the surfels take over (diffuse only)
		float farW = cellFarTerms( Pw, sw.N, farF, farGP );
		int farN;
		int farB = cellLightsBegin( Pw, farN );
		if ( farW >= 1.0 )
			farN = 0;
		for ( int k = 0; k < farN; k++ ) {
			int i = cellLightAt( farB, k );
			float3 Lw;
			bool noSpec;
			float3 E = cellLightE( i, Pw, sw.N, Lw, noSpec );
			if ( E.r + E.g + E.b <= 0.0 )
				continue;
			float3 dD, dS;
			float3 Ls = Lw;
			float rS = sw.rough;
			if ( cellLightPhysB.y > 0.0 && !noSpec )	// lane LIGHTPHYS1: Area lights
				cellAreaSpec( i, Pw, sw.N, Vw, Ls, rS );
			directLightA( sw, Lw, Ls, rS, Vw, ms, dD, dS );
			cE += E * max( dot( sw.N, Lw ), 0.0 );
			cDiff += dD * E * M_PI;
			if ( !noSpec )
				cSpec += dS * E * M_PI;
		}
#ifdef CELL_GI_ONE
		/* DDGIREADPERF1: the one volume read, after the light loop (before it the PBR loop ran slower), at the P and N
		 * every site asks for (cellGiE, cellGiSkyK, cellSpecSky, the probe and pass views; the PROBEVIEW1 pass red's up
		 * normal included) */
		cellGiFirst( Pw, sw.N, cellPass > 0 && cellPass != 6 && cellProbe == 0 && ( cellPassRed & 2 ) != 0 ? float3( 0.0, 0.0, 1.0 ) : sw.N,
		             cellWorldDir( R ), s.rough );
#endif
		float3 giE = cellSsgiE( CELL_GI_UP ? cellGiE( Pw, sw.N ) : (float3)( 0.0 ) ) / M_PI;	// lane PRTOFF1: the PRT's grid or the ray-traced volume
		if ( farW > 0.0 ) {
			cDiff = cDiff * ( 1.0 - farW ) + farW * rho * farF;
			cSpec *= 1.0 - farW;
			cE = cE * ( 1.0 - farW ) + farW * farF;
			giE = max( giE - farW * farGP, (float3)( 0.0 ) );
		}
		float3 giDiff = rho * keepInd * giE;
#ifdef WW_DV
		wwDvGi = giDiff;
#endif
		if ( cellInterior ) {
			outDiff = cDiff + giDiff;
			outSpec = cSpec;
#ifdef WW_DV
			wwDvSun = (float3)( 0.0 );	// indoors the sun is the block below's alone
			wwDvSpec = (float3)( 0.0 );
#endif
			cE += giE;
			if ( cellHasDalc && cellGiAmb == 0 ) {
				outDiff += cellAmbient( sw.N, Pw ) * rho * keepInd * s.ao;	// P picks the Ambient Only volume
				outSpec += cellAmbient( reflect( -Vw, sw.N ), Pw ) * Espec * s.ao;
				cE += cellAmbient( sw.N, Pw );
			} else if ( cellHasDalc ) {	// WW_CELL_GI_AMB (cellAmbGi); the reflection's ambient by the same luminance share
				float3 a0 = cellAmbient( sw.N, Pw );
				float3 a1 = cellAmbGi( a0, giE, Pw, sw.N );
				float k = dot( a1, float3( 0.2126, 0.7152, 0.0722 ) ) / max( dot( a0, float3( 0.2126, 0.7152, 0.0722 ) ), 1.0e-6 );
				outDiff += a1 * rho * keepInd * s.ao;
				outSpec += cellAmbient( reflect( -Vw, sw.N ), Pw ) * k * Espec * s.ao;
				cE += a1;
			}
			if ( cellHasDir ) {
				float3 dD, dS;
				directLight( sw, normalize( cellDirTo ), Vw, ms, dD, dS );
				float3 sunC = cellDirColor * ( cellInterior ? 1.0 : airSunT );	// FOGSUN1: outdoors the sun through the air
				outDiff += dD * sunC * M_PI;
				outSpec += dS * sunC * M_PI;
#ifdef WW_DV
				wwDvSun = dD * sunC * M_PI;
				wwDvSpec = dS * sunC * M_PI;
#endif
				cE += sunC * max( dot( sw.N, normalize( cellDirTo ) ), 0.0 );
			}
			// the material's cubemap, lit by the light reaching this point (a dark corner reflects dark);
			// legacy materials take the game's term, .pbrm shapes (and WW_CELL_LIT_RED=cubeold) the old law
			if ( hasCubeMap ) {
				if ( cellCubeGameOn() && ( cellRed & 512 ) == 0 ) {
					float2 sg = CellSpecMap.Sample( _CellSpecMap_sampler, offset ).rg;
					cubeK = cellCubeGame( CubeMap, _CubeMap_sampler, mul(R, reflMatrix), lerp( cellCubeMat.y * sg.g, landLodK.x, landLodV ),
					                      lerp( cellCubeMat.x * sg.r, landLodK.y, landLodV ), envReflection, -ViewDir );
				} else {
					float3 cube = CubeMap.SampleLevel( _CubeMap_sampler, mul(R, reflMatrix), s.rough * 8.0 ).rgb;
					cubeK = cube * cube * envReflection * Espec * s.ao;
				}
			}
			// the screen-space reflection over the cube term, by the march's confidence (cell_ssr.glsl)
			if ( or( cellSsrOn, cellProbe == 60 ) ) {
				float2 sg = CellSpecMap.Sample( _CellSpecMap_sampler, offset ).rg;
				cubeK = cellSsrMix( cubeK, lerp( cellCubeMat.y * sg.g, landLodK.x, landLodV ),
				                    lerp( cellCubeMat.x * sg.r, landLodK.y, landLodV ), envReflection );
			}
			outSpec += cubeK * cE;
#ifdef WW_DV
			wwDvRefl = cubeK * cE;
#endif
		} else if ( cellGiSky ) {
			// the grid's sky stands in for the weather's unshadowed ambient by its valid share,
			// and takes the material's AO as that ambient did
			outDiff += cDiff + giDiff * s.ao - cellGiSkyK( Pw, sw.N ) * ( outDiff - preAmbDiff );
			// lane SPECOCC1: the cube reflection keeps the sky visibility along R (exactly 1 with the row off: untouched)
			float specSky = cellSpecSky( Pw, sw.N, cellWorldDir( R ), s.rough );
			if ( specSky < 1.0 )
				outSpec = preAmbSpec + ( outSpec - preAmbSpec ) * specSky;
			outSpec += cSpec;
		} else {
			outDiff += cDiff + giDiff;
			outSpec += cSpec;
		}
	}
#endif
	color.rgb = ( r3Term == 1 ) ? outDiff : ( r3Term == 2 ) ? outSpec : outDiff + outSpec;
	if ( r3Term == 3 )	// debug: the surface Fresnel at N.V
		color.rgb = surfaceF( s, NdotV );

	// --- emission (ED:2519-2521): color x mask x luminance/100, added before exposure; a map REPLACES the
	// constant unless overridden. Debug "emitmul": constant x map, "emitraw": no /100.
	float3 pbrEmitLin = (float3)( 0.0 );	// lane SSGI2: the emission alone (cellEmitOut)
	if ( pbrEmissiveIntensity > 0.0 && r3Term == 0 ) {
		float3 ecol = pow( max( pbrEmissiveColor, (float3)( 0.0 ) ), (float3)( 2.2 ) );
		float emask = pbrEmissiveMask;
		if ( hasFeature( F_EMISSIVE ) ) {
			float4 em = EmissiveMap.Sample( _EmissiveMap_sampler, offset );
			float3 emap = emissiveIsSrgbTex ? em.rgb : srgbToLinear( em.rgb );
			if ( ( r4Red & 2 ) != 0 ) {
				ecol *= emap;
				emask *= em.a;
			} else {
				if ( pbrEmissiveMapColor ) ecol = emap;
				if ( pbrEmissiveMapMask ) emask = em.a;
			}
		}
		pbrEmitLin = ecol * emask * pbrEmissiveIntensity * ( ( r4Red & 8 ) != 0 ? 100.0 : 1.0 );
		color.rgb += pbrEmitLin;
	}

#ifdef WW_CELLLIGHTS
	float3 cellSsrScene = color.rgb;	// what the reflections' march samples (probe 60)
	if ( cellOn && cellProbe == 60 ) {
		float3 em = clamp( pbrEmitLin, (float3)( 0.0 ), max( cellSsrScene, (float3)( 0.0 ) ) );	// coverage in a
		cellEmitOut = float4( em, dot( em, float3( 0.2126, 0.7152, 0.0722 ) ) > 1.0e-6 ? 1.0 : 0.0 );
	}
	// lane AO1: the ambient obscurance on the whole lit colour, before the fog
	if ( cellOn )
		color.rgb *= cellAoShared( wwFarAo );	// MODULES1 10c: near (screen) and far (baked) through one rule
#ifdef WW_DV
	float3 wwDvLit = max( color.rgb, (float3)( 0.0 ) );	// lane DEBUGVIEW1: the lit colour before the fog
#endif
	// a cell-lit draw fogs here, once (the interior's fog, or the Lookdev weather's outdoors)
	if ( cellOn && ( or( fogOn, wwApOn() ) ) )	// lane SKYA: the sky's aerial perspective with the fog off too
		color.rgb = wwFog( max( color.rgb, (float3)( 0.0 ) ), -ViewDir );
	// the cell's imagespace in place of the viewer's curve; probe 6 writes the linear color raw
	float3 cellHdr = max( color.rgb, (float3)( 0.0 ) );
	if ( cellOn && cellIsOn )
		color.rgb = cellIsLinear ? cellHdr : cellImageSpace( sqrt( cellHdr ) );	// linear into the HDR frame
	else
#endif
	if ( sceneMode >= 1 ) {
		if ( sceneMode == 2 && !WW_CELL_FOGGED )
			color.rgb = wwFog( color.rgb, -ViewDir );	// linear, before the exposure
		if ( studioProbe >= 0.0 )
			color.rgb = (float3)( studioProbe );
		color.rgb = studioOutput( color.rgb );
	} else {
		// Legacy lighting: the linear result goes out through the legacy Hable + sqrt (sqrt-space input).
		color.rgb = tonemap( sqrt( max( color.rgb, (float3)( 0.0 ) ) ) );
	}
	// Premultiplied: the mapped color times opacity (ED:2921), blended ONE, 1 - SRC_ALPHA
	if ( pbrComposition == 3 && ( r4Red & 16 ) == 0 )
		color.rgb *= color.a;

	fragColor = color;
#ifdef WW_CELLLIGHTS
	/* lane PBRMVIEWS1: as fo4_default.frag (FO4CELLGI1), the probe and pass views compile only under the loader's
	 * WW_CELL_VIEWS in the programs with the ray-traced GI volume; elsewhere the source is unchanged */
#if defined( WW_CELL_VIEWS ) || !defined( WW_CELL_RTDDGI )
	if ( cellOn && cellProbe > 0 )
		fragColor = cellProbe == 6 ? float4( cellHdr, color.a )
			: float4( cellProbeRaw( cellWorldPos( -ViewDir ), cellWorldDir( s.N ) ), 1.0 );
#else
	if ( cellOn && cellProbe > 0 )	// 6 the measuring pass; 20, 60 and 61 (the AO and SSR passes) write over it below
		fragColor = cellProbe == 6 ? float4( cellHdr, color.a ) : float4( cellWorldDir( s.N ) * 0.5 + 0.5, 1.0 );
#endif
	if ( cellOn && cellProbe >= 50 && cellProbe <= 53 )
		fragColor = float4( cellProbe == 50 ? clamp( cubeK * 0.25, 0.0, 1.0 ) : cellCubeProbe( offset, s.N ), 1.0 );
#if defined( WW_CELL_VIEWS ) || !defined( WW_CELL_RTDDGI )
	if ( cellOn && cellPass > 0 && cellProbe == 0 )	// the PRTP band's Pass view
		fragColor = float4( cellPassOut( cellWorldPos( -ViewDir ), cellWorldDir( s.N ) ), 1.0 );
#endif
#endif
	float3 fogProbeOut;
	if ( wwFogProbe( -ViewDir, fogProbeOut ) )
		fragColor = float4( fogProbeOut, 1.0 );
#ifdef WW_SUNSHADOW
	float3 csmProbeOut;
	if ( wwSunShadowProbe( -ViewDir, fragColor.rgb, csmProbeOut ) )
		fragColor = float4( csmProbeOut, 1.0 );
#endif
#ifdef WW_CELLLIGHTS
	if ( cellOn && cellProbe == 20 ) {
		fragColor = cellAoPassOut( s.N, -ViewDir );
		if ( cellAoEarly == 2 )
			fragColor.w *= 1.001;	// lane PERF1 red: full shading runs (the old cost), and the gate must see the mark
	}
	if ( cellOn && ( or( cellProbe == 60, cellProbe == 61 ) ) )
		fragColor = cellSsrProbeOut( cellSsrScene );
#endif
#ifdef WW_DV
	if ( cellOn && wwDebugView != 0 ) {	// lane DEBUGVIEW1: one pass or term in place of the pixel (debug_view.glsl)
		wwDvAmb = max( outDiff - wwDvSun, (float3)( 0.0 ) );	// PBR: the ambient, the GI and the lamps, one sum
		if ( !cellInterior )
			wwDvRefl = max( outSpec - wwDvSpec, (float3)( 0.0 ) );	// outdoors the environment and the lamps' specular
		float dvSh = 1.0, dvCl = 1.0;
#ifdef WW_SUNSHADOW
		dvSh = wwSunShadowGeo( -ViewDir );
		float3 dvCi = ussCloudDvInfo( -ViewDir );	// SEAMCLOUD1: T, the height share, the far share
		dvCl = dvCi.x;
		wwDvCloud = dvCi.yz;
#endif
		float3 dvT = (float3)( 1.0 ), dvIn = (float3)( 0.0 );
		if ( or( fogOn, wwApOn() ) ) {
			dvIn = max( wwFog( (float3)( 0.0 ), -ViewDir ), (float3)( 0.0 ) );
			dvT = clamp( wwFog( (float3)( 1.0 ), -ViewDir ) - dvIn, 0.0, 1.0 );
		}
		fragColor = float4( wwDvSurface( s.base, cellWorldDir( s.N ), 1.0 - s.rough, s.metal, wwFarAo, cellAoFactor(),
			dvSh, dvCl, pbrEmitLin, dvT, dvIn, wwDvLit, cellHdr, length( ViewDir ) * length( cellRow[0].xyz ), wwDvOwner ),
			fragColor.a );
	}
#endif
	/* a blended-ground pass (src/cellsplat.h): the RAW vertex alpha (C.a may be overridden) is this pass's share;
	 * an opaque base is scaled by it, a layer is added at it (SRC_ALPHA, ONE). */
	/* never over the data passes (20, 60/61): the ground would write depth 1 there and the water SSR march
	 * would die on it */
	bool splatData = false;
#ifdef WW_CELLLIGHTS
	splatData = cellOn && ( or( or( cellProbe == 20, cellProbe == 60 ), cellProbe == 61 ) );
#endif
#ifdef WW_CELLLIGHTS
	/* lane LODLIVE1: the LOD coloration in the PBR path too (fo4_default.frag's colours; farLadder from fo4_default.vert:
	 * y = band + 1, x != 0 mid ladder fade) */
	if ( farLadder.y > 0.5 ) {
		float3 lt = farLadder.x != 0.0 ? float3( 1.0, 0.2, 1.0 ) : farLadder.y < 1.5 ? (float3)( 0.92 ) : farLadder.y < 2.5 ? float3( 0.2, 0.85, 0.25 )
			: farLadder.y < 3.5 ? float3( 0.95, 0.9, 0.15 ) : farLadder.y < 4.5 ? float3( 1.0, 0.55, 0.1 ) : float3( 0.95, 0.15, 0.15 );
		fragColor.rgb = lt * ( 0.25 + 0.75 * clamp( dot( fragColor.rgb, float3( 0.2126, 0.7152, 0.0722 ) ), 0.0, 1.0 ) );
	}
#endif
	if ( landSplat && !splatData ) {
		/* lane ALLON2 (bungo's thin dark lines along the ground's edges, 21.png): with MSAA a pixel on the
		 * OUTER edge of a layer's quads is shaded once at its centre, which lies outside every triangle of
		 * that layer, so every input is extrapolated past the edge -- the share below zero, and the lit
		 * colour can go non-finite. The blend then adds NaN x 0 = NaN and the resolve drops the whole
		 * pixel to black: one dark render pixel along every place a layer's quads stop. A share at or
		 * below zero adds nothing, so it is written as exactly nothing; a non-finite colour likewise.
		 * Red WW_TERRBLEND_RED=edgepass: the old output. Probe WW_TERRBLEND_PROBE=edge: such pixels green. */
		float	w = rawVertexAlpha;
		bool	bad = or( or( or( any( isnan( fragColor.rgb ) ), any( isinf( fragColor.rgb ) ) ), isnan( w ) ), isinf( w ) );
		if ( landSplatDbg == 2 ) {
			fragColor = bad ? float4( 0.0, 1.0, 0.0, 1.0 ) : ( ( alphaFlags & 8 ) != 0 ? (float4)( 0.0 ) : float4( 0.0, 0.0, 0.0, 1.0 ) );
		} else if ( landSplatDbg == 0 && ( or( bad, ( ( alphaFlags & 8 ) != 0 && !( w > 0.0 ) ) ) ) ) {
			fragColor = ( alphaFlags & 8 ) != 0 ? (float4)( 0.0 ) : float4( 0.0, 0.0, 0.0, 1.0 );
		} else if ( ( alphaFlags & 8 ) != 0 )
			fragColor.a = clamp( rawVertexAlpha, 0.0, 1.0 );
		else
			fragColor = float4( fragColor.rgb * clamp( rawVertexAlpha, 0.0, 1.0 ), 1.0 );
	}
}

struct SPIRV_Cross_Input
{
    WW_LOC(3) float3 LightDir : TEXCOORD3;
    WW_LOC(4) float3 ViewDir : TEXCOORD4;
    WW_LOC(8) float2 texCoord : TEXCOORD8;
    WW_LOC(0) nointerpolation float4 A : TEXCOORD0;
    WW_LOC(1) float4 C : TEXCOORD1;
    WW_LOC(12) float rawVertexAlpha : TEXCOORD12;
    WW_LOC(15) float wwFarSkyVis : TEXCOORD15;
    WW_LOC(14) float wwFarAo : TEXCOORD14;
    WW_LOC(2) nointerpolation float4 D : TEXCOORD2;
    WW_LOC(5) float3x3 btnMatrix : TEXCOORD5;
    WW_LOC(9) nointerpolation float3x3 reflMatrix : TEXCOORD9;
    float4 gl_FragCoord : SV_Position;
    bool gl_FrontFacing : SV_IsFrontFace;
#if defined(WW_CELLLIGHTS)
    WW_LOC(13) nointerpolation float2 farLadder : TEXCOORD13;
#endif
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
#if defined(WW_CELLLIGHTS)
    float4 cellEmitOut : SV_Target1;
#endif
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
#if defined(WW_CELLLIGHTS)
    farLadder = stage_input.farLadder;
#endif
    LightDir = stage_input.LightDir;
    ViewDir = stage_input.ViewDir;
    texCoord = stage_input.texCoord;
    A = stage_input.A;
    C = stage_input.C;
    rawVertexAlpha = stage_input.rawVertexAlpha;
    wwFarSkyVis = stage_input.wwFarSkyVis;
    wwFarAo = stage_input.wwFarAo;
    D = stage_input.D;
    btnMatrix = stage_input.btnMatrix;
    reflMatrix = stage_input.reflMatrix;
    gl_FrontFacing = stage_input.gl_FrontFacing;
    ViewDir_norm = normalize( ViewDir );
    btnMatrix_norm = float3x3( normalize( btnMatrix[0] ), normalize( btnMatrix[1] ), normalize( btnMatrix[2] ) );
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
#if defined(WW_CELLLIGHTS)
    stage_output.cellEmitOut = cellEmitOut;
#endif
    return stage_output;
}
