// HLSL source of features/water/fo4_water.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "WaterBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// inlined (no readable .hlsli yet): uniforms.glsl
#ifndef WW_GLSL_MOD
#define WW_GLSL_MOD	// GLSL mod: x - y floor(x / y) (HLSL fmod truncates)
float mod(float x, float y) { return x - y * floor(x / y); }
float2 mod(float2 x, float2 y) { return x - y * floor(x / y); }
float3 mod(float3 x, float3 y) { return x - y * floor(x / y); }
float4 mod(float4 x, float4 y) { return x - y * floor(x / y); }
float2 mod(float2 x, float y) { return x - y * floor(x / y); }
float3 mod(float3 x, float y) { return x - y * floor(x / y); }
float4 mod(float4 x, float y) { return x - y * floor(x / y); }
#endif
#ifndef WW_GLSL_SIZE_Texture2D_float4_
#define WW_GLSL_SIZE_Texture2D_float4_
int2 textureSize(Texture2D<float4> t, int lod) { uint w, h, l; t.GetDimensions(uint(lod), w, h, l); return int2(w, h); }
#endif
#ifndef WW_GLSL_INVERSE
#define WW_GLSL_INVERSE	// GLSL inverse(): the classical adjoint over the determinant (the per-variant files' spirv-cross law)
float wwDet2x2(float a1, float a2, float b1, float b2) { return a1 * b2 - b1 * a2; }
float wwDet3x3(float a1, float a2, float a3, float b1, float b2, float b3, float c1, float c2, float c3)
{
	return a1 * wwDet2x2(b2, b3, c2, c3) - b1 * wwDet2x2(a2, a3, c2, c3) + c1 * wwDet2x2(a2, a3, b2, b3);
}
float4x4 wwInverse(float4x4 m)
{
	float4x4 adj;
	adj[0][0] =  wwDet3x3(m[1][1], m[1][2], m[1][3], m[2][1], m[2][2], m[2][3], m[3][1], m[3][2], m[3][3]);
	adj[0][1] = -wwDet3x3(m[0][1], m[0][2], m[0][3], m[2][1], m[2][2], m[2][3], m[3][1], m[3][2], m[3][3]);
	adj[0][2] =  wwDet3x3(m[0][1], m[0][2], m[0][3], m[1][1], m[1][2], m[1][3], m[3][1], m[3][2], m[3][3]);
	adj[0][3] = -wwDet3x3(m[0][1], m[0][2], m[0][3], m[1][1], m[1][2], m[1][3], m[2][1], m[2][2], m[2][3]);
	adj[1][0] = -wwDet3x3(m[1][0], m[1][2], m[1][3], m[2][0], m[2][2], m[2][3], m[3][0], m[3][2], m[3][3]);
	adj[1][1] =  wwDet3x3(m[0][0], m[0][2], m[0][3], m[2][0], m[2][2], m[2][3], m[3][0], m[3][2], m[3][3]);
	adj[1][2] = -wwDet3x3(m[0][0], m[0][2], m[0][3], m[1][0], m[1][2], m[1][3], m[3][0], m[3][2], m[3][3]);
	adj[1][3] =  wwDet3x3(m[0][0], m[0][2], m[0][3], m[1][0], m[1][2], m[1][3], m[2][0], m[2][2], m[2][3]);
	adj[2][0] =  wwDet3x3(m[1][0], m[1][1], m[1][3], m[2][0], m[2][1], m[2][3], m[3][0], m[3][1], m[3][3]);
	adj[2][1] = -wwDet3x3(m[0][0], m[0][1], m[0][3], m[2][0], m[2][1], m[2][3], m[3][0], m[3][1], m[3][3]);
	adj[2][2] =  wwDet3x3(m[0][0], m[0][1], m[0][3], m[1][0], m[1][1], m[1][3], m[3][0], m[3][1], m[3][3]);
	adj[2][3] = -wwDet3x3(m[0][0], m[0][1], m[0][3], m[1][0], m[1][1], m[1][3], m[2][0], m[2][1], m[2][3]);
	adj[3][0] = -wwDet3x3(m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2], m[3][0], m[3][1], m[3][2]);
	adj[3][1] =  wwDet3x3(m[0][0], m[0][1], m[0][2], m[2][0], m[2][1], m[2][2], m[3][0], m[3][1], m[3][2]);
	adj[3][2] = -wwDet3x3(m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2], m[3][0], m[3][1], m[3][2]);
	adj[3][3] =  wwDet3x3(m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2]);
	float det = (adj[0][0] * m[0][0]) + (adj[0][1] * m[1][0]) + (adj[0][2] * m[2][0]) + (adj[0][3] * m[3][0]);
	return (det != 0.0f) ? (adj * (1.0f / det)) : m;
}
#endif
static float4 gl_FragCoord;
/* The game's near-water pixel shader in the cell view, term for term (src/features/water/cellwater.h). World space (z up),
 * linear light; V runs from the eye to the pixel. The sun is the frame's light 0. waterSslr = 1 instead writes the
 * reflection ray for cellssr.cpp. The water looks (depth colour, swell, caustics, shore, reflections, sky
 * visibility, flow, ocean) compile into one twin, fo4_waterlook, each switched by its own uniform.
 * Probes (WW_CELL_WATER_PROBE, WW_CELL_WATER_INFO_PROBE): raw debug values, hi/lo bytes of 16-bit values. */

#define WW_FOG 1
#define WW_CELLLIGHTS 1

static float3 LightDir;
static float3 ViewDir;
static float2 texCoord;
static float3x3 btnMatrix;
static float4 A;
static float4 C;
static float4 D;
static float rawVertexAlpha;
static float3x3 reflMatrix;

static float4 fragColor;

// uniforms.glsl inlined here: its .hlsli has no readable half yet (lane HLSLSRC1)
cbuffer globalUniforms : register(b0, space0)
{
	row_major float3x3	viewMatrix : packoffset(c0);					// rotation part of view transform
	row_major float3x3	envMapRotation : packoffset(c3);				// view space to environment map
	row_major float4x4	projectionMatrix : packoffset(c6);
	float4	lightSourcePosition[3] : packoffset(c10);
	float4	lightSourceDiffuse[3] : packoffset(c13);
	float4	lightSourceAmbient : packoffset(c16);
	float	toneMapScale : packoffset(c17);				// 1.0 = full tone mapping
	float	brightnessScale : packoffset(c17.y);
	float	glowScale : packoffset(c17.z);
	float	glowScaleSRGB : packoffset(c17.w);
	int4	viewportDimensions : packoffset(c18);			// X, Y, width, height
	uint	doSkinning : packoffset(c19);
	int	sceneOptions : packoffset(c19.y);
	int	cubeBgndMipLevel : packoffset(c19.z);
	int	sfParallaxMaxSteps : packoffset(c19.w);
	float	sfParallaxScale : packoffset(c20);
	float	sfParallaxOffset : packoffset(c20.y);
	float	unusedUniform1 : packoffset(c20.z);
	float	unusedUniform2 : packoffset(c20.w);
};
// end of uniforms.glsl
#define WW_LOOSE_TYPES 1
#include "../air/lookdev_fog.hlsli"
#include "../lights/cell_lights.hlsli"
#include "../lights/cell_cluster.hlsli"
#if defined(WW_DEBUG_VIEWS)
#include "../overlays/debug_view.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH))
#include "../ao/cell_gihalfat.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#endif
#if !(defined(WW_CELL_PRT)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#include "../ao/cell_gihalfat.hlsli"
#endif
#include "water_info.hlsli"
#if ((defined( WW_WATER_DEPTH ) || defined( WW_WATER_CAUSTICS )))
#include "water_depth.hlsli"
#endif
#if defined(WW_WATER_CLEAR)
#include "water_clear.hlsli"
#endif
#if defined(WW_WATER_SKYVIS)
#include "water_skyvis.hlsli"
#endif
#if defined(WW_WATER_CAUSTICS)
#include "../shadows/ww_sunshadow.hlsli"
#include "water_caustics.hlsli"
#endif
#if defined(WW_WATER_SHORE)
#include "water_shore.hlsli"
#endif
#if defined(WW_WATER_SURF)
#include "water_surf.hlsli"
#endif
#if defined(WW_WATER_WAVES)
#include "water_waves.hlsli"
#endif
#if defined(WW_WATER_REFL)
#include "water_refl.hlsli"
#endif
#if defined(WW_WATER_OCEAN)
#include "water_ocean.hlsli"
#include "water_ripple.hlsli"
#include "water_flowwaves.hlsli"
#endif
#if defined(WW_WATER_CONTACT)
#include "water_contact.hlsli"
#endif
#if defined(WW_WATER_RIVERFOAM)
#include "water_riverfoam.hlsli"
#endif
#if defined(WW_WATER_SKYFAR)
#include "water_skyfar.hlsli"
#endif
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "../air/lookdev_fog.hlsli"
#include "../lights/cell_lights.hlsli"
#include "../lights/cell_cluster.hlsli"
#if defined(WW_DEBUG_VIEWS)
#include "../overlays/debug_view.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH))
#include "../ao/cell_gihalfat.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#endif
#if !(defined(WW_CELL_PRT)) && defined(WW_CELL_RTDDGI)
#include "../gi/cell_ddgi.hlsli"
#include "../ao/cell_gihalfat.hlsli"
#endif
#include "water_info.hlsli"
#if ((defined( WW_WATER_DEPTH ) || defined( WW_WATER_CAUSTICS )))
#include "water_depth.hlsli"
#endif
#if defined(WW_WATER_CLEAR)
#include "water_clear.hlsli"
#endif
#if defined(WW_WATER_SKYVIS)
#include "water_skyvis.hlsli"
#endif
#if defined(WW_WATER_CAUSTICS)
#include "../shadows/ww_sunshadow.hlsli"
#include "water_caustics.hlsli"
#endif
#if defined(WW_WATER_SHORE)
#include "water_shore.hlsli"
#endif
#if defined(WW_WATER_SURF)
#include "water_surf.hlsli"
#endif
#if defined(WW_WATER_WAVES)
#include "water_waves.hlsli"
#endif
#if defined(WW_WATER_REFL)
#include "water_refl.hlsli"
#endif
#if defined(WW_WATER_OCEAN)
#include "water_ocean.hlsli"
#include "water_ripple.hlsli"
#include "water_flowwaves.hlsli"
#endif
#if defined(WW_WATER_CONTACT)
#include "water_contact.hlsli"
#endif
#if defined(WW_WATER_RIVERFOAM)
#include "water_riverfoam.hlsli"
#endif
#if defined(WW_WATER_SKYFAR)
#include "water_skyfar.hlsli"
#endif
#undef WW_LOOSE_UNIFORMS
	float4 waterMat[13];	// shallow, deep, reflection, underwater, light silt, dark silt, var, p1..p4, amplitude, uv scale
	float4 waterSky[3];	// horizon, lower, upper (linear)
	float4 waterScroll0;	// the noise layers' offsets (0 in the cell view)
	float4 waterScroll1;
	bool waterHaveDepth;
	bool waterFarMask;	// lane SHORE2: far shore-law water, discarded on the samples the .lodl calls dry
	int waterRed;		// debug: 1 norefl, 2 nofresnel, 4 nosilt, 8 nospec, 16 noshore, 32 nonormal,
	// 64 nodepthfog, 128 nofar (the noise sampler's, in cellwater.cpp), 256 nossr, 512 fogunits
	bool waterSsrOn;	// this frame's reflections are bound (half the view, top row first)
	int waterSslr;		// 1: the reflection-ray pass (waterDepth is then the obscurance pass's linear depth)
	int waterProbe;
	/* FARWATER1 round 2 (src/features/water/cellwater.cpp): 1 the air without the column clamp (the water is not in the
	 * obscurance depth), 2 the endless sea's curved point read in the flat air frame (lookdev_fog.glsl wwAirFlatPos); 0 under
	 * the red WW_ENDLESS_SEA_RED=air */
	int waterAir;
#if defined(WW_WATER_UNDER)
	bool waterUnderOn;	// the Underwater light row
	int waterUnderRed;	// 1 nosnell, 2 oldsun
	float3 waterUnderSun;	// lane WATERUNDER3: the underwater pass's sun (cellunderwater.h wwCellUnderwaterSun)
	float3 waterUnderSunTo;	// world, normalized
#endif
};
#include "../air/lookdev_fog.hlsli"
#include "../lights/cell_lights.hlsli"
#include "water_info.hlsli"
#if defined( WW_WATER_DEPTH ) || defined( WW_WATER_CAUSTICS )
#include "water_depth.hlsli"
#endif
#ifdef WW_WATER_CLEAR	// lane WATERLOOK1 step 8 (WATERCOLOR1): the water's own colour by body class (after water_depth)
#include "water_clear.hlsli"
#endif
// lane WATERLOOK1 step 10 (WATERSKYVIS1): the sky reflection and the sun's glare where their sky is seen (after cell_lights)
#ifdef WW_WATER_SKYVIS
#include "water_skyvis.hlsli"
#endif
#ifdef WW_WATER_CAUSTICS	// lane WATERCAUSTICS1: the sun's cascades at the bed, then the caustic law
#include "../shadows/ww_sunshadow.hlsli"
#include "water_caustics.hlsli"
#endif
#ifdef WW_WATER_SHORE	// lane WATERSHORE1: fo4_waterlook.prog only (fo4_water.prog keeps its machine code)
#include "water_shore.hlsli"
#endif
// lane WATERLOOK1 step 11 (WATERSURF1): shore waves on the sea (after water_shore)
#ifdef WW_WATER_SURF
#include "water_surf.hlsli"
#endif


Texture2D<float4> waterNoise0 : register(t46, space0); SamplerState _waterNoise0_sampler : register(s46, space0);
Texture2D<float4> waterNoise1 : register(t47, space0); SamplerState _waterNoise1_sampler : register(s47, space0);
Texture2D<float4> waterNoise2 : register(t48, space0); SamplerState _waterNoise2_sampler : register(s48, space0);
Texture2D<float4> waterScene : register(t49, space0); SamplerState _waterScene_sampler : register(s49, space0);	// the frame behind the water, the viewport's corner at texel 0
Texture2D<float4> waterDepth : register(t50, space0); SamplerState _waterDepth_sampler : register(s50, space0);	// the opaque pass's depth, likewise
Texture2D<float4> waterSsrRaw : register(t51, space0); SamplerState _waterSsrRaw_sampler : register(s51, space0);	// the march
Texture2D<float4> waterSsrFin : register(t52, space0); SamplerState _waterSsrFin_sampler : register(s52, space0);	// the march blurred
#ifdef WW_WATER_WAVES	// lane WATERWAVES1: fo4_waterlook.prog only (fo4_water.prog keeps its machine code)
#include "water_waves.hlsli"
#endif
#ifdef WW_WATER_REFL	// lane WATERREFL1: fo4_waterlook.prog only (fo4_water.prog keeps its machine code)
#include "water_refl.hlsli"
#include "water_ssrbend.hlsli"
// lane WATERLOOK1 step 9 (WATERGLINT1): the glare as counted facets
#include "water_glitter.hlsli"
#endif
#ifdef WW_WATER_OCEAN	// lane WATERLOOK1: fo4_waterlook.prog only (fo4_water.prog keeps its machine code)
#include "water_ocean.hlsli"
#include "water_ripple.hlsli"
#include "water_sss.hlsli"
#include "water_flowwaves.hlsli"
#endif
// lane FIX1101A (LATE ITEM 1): the WATR's own fine layers draw the sea spectrum's unresolved tail near the camera
#if defined( WW_WATER_OCEAN ) && defined( WW_WATER_REFL )
#include "water_detail.hlsli"
#endif
// lane WATERLOOK1 step 12 (WATERCONTACT1): foam where the water meets anything standing in it (after shore + ripple)
#ifdef WW_WATER_CONTACT
#include "water_contact.hlsli"
#endif
// lane WATERLOOK1 step 15 (WATERFLOW1): foam streaks carried by the baked river flow (after water_info + water_shore)
#ifdef WW_WATER_RIVERFOAM
#include "water_riverfoam.hlsli"
#endif
#ifdef WW_WATER_UNDER	// lane WATERLOOK1 step 16 (WATERUNDER2): Snell's window on the surface seen from below


// water -> air Fresnel, unpolarised and exact: 1 past the critical angle (48.6 deg), total internal reflection
float wwUnderFresnel( float ci )
{
	const float n = 1.333;
	float st2 = n * n * ( 1.0 - ci * ci );
	if ( st2 >= 1.0 )
		return 1.0;
	float ct = sqrt( 1.0 - st2 );
	float rs = ( n * ci - ct ) / ( n * ci + ct ), rp = ( ci - n * ct ) / ( ci + n * ct );
	return 0.5 * ( rs * rs + rp * rp );
}
#endif
#ifdef WW_WATER_DISP	// lane WATERLOOK1 step 2: fo4_waterlookd.prog, the displaced water: the undisplaced world xy (u)
static float2 wwOcBase;
#endif
#ifdef WW_WATER_SKYFAR	// lane WATERSKYVISFAR1: fo4_waterlook.prog only (fo4_water.prog keeps its machine code)
#include "water_skyfar.hlsli"
#endif

float3 tonemap( float3 x )	// fo4_default.frag's
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

// the inverse of tonemap() above: a stored frame value back to linear light
float3 untonemap( float3 y )
{
	const float a = 0.15, b = 0.50, c = 0.10, d = 0.20, e = 0.02, f = 0.30;
	float3 k = min( y * y * ( A.a * 0.93333333 ) + e / f, (float3)( 0.99999 ) );
	float3 qa = a * ( k - 1.0 );
	float3 qb = b * ( k - c );
	float3 qc = d * ( f * k - e );
	float3 z = ( -qb - sqrt( max( qb * qb - 4.0 * qa * qc, (float3)( 0.0 ) ) ) ) / ( 2.0 * qa );
	return max( z, (float3)( 0.0 ) ) / ( D.a * A.a * 4.22978723 );
}

// the frame stores what the cell programs wrote: linear (the HDR frame), or after the curve
float3 frameToLinear( float3 t )
{
	if ( cellOn && cellIsOn )
		return cellIsLinear ? t : pow( max( t, (float3)( 0.0 ) ), (float3)( 2.2 ) );	// the imagespace: approximate (interiors)
	return untonemap( t );
}

float3 noiseN( Texture2D<float4> s, SamplerState _s_sampler, float2 uv, out float2 tap )
{
	tap = s.Sample( _s_sampler, uv ).xy;
	float2 t = tap * 2.0 - 1.0;
	return float3( t, sqrt( 1.0 - min( dot( t, t ), 1.0 ) ) );
}

float3 fix24( float3 v, int b )	// byte b (0 = high) of 24-bit fixed
{
	float3 q = floor( clamp( v, 0.0, 1.0 ) * 16777215.0 + 0.5 );
	return b == 0 ? floor( q / 65536.0 ) / 255.0 : b == 1 ? mod( floor( q / 256.0 ), 256.0 ) / 255.0 : mod( q, 256.0 ) / 255.0;
}

float smooth01( float x )
{
	return x * x * ( 3.0 - 2.0 * x );
}

// the game's depth-color pass over a scene texel, linear
float3 depthFog( float3 c, float along, float vert )
{
	if ( ( waterRed & 64 ) != 0 )
		return c;
	float4 P1 = waterMat[7], P3 = waterMat[9], P4 = waterMat[10];
	float k = ( waterRed & 512 ) != 0 ? 0.0142875 : 1.0;	// debug: the distances taken in meters
#ifdef WW_WATER_CLEAR	// lane WATERLOOK1 step 8: absorption + lit in-scatter replace the WATR's ramp
	if ( waterClearOn )
		return wwWaterClear( c, along, vert );
#endif
	float rzF = clamp( 1.0 - vert * k / P1.w, 0.0, 1.0 );
	float rwF = clamp( 1.0 - along * k / P1.w, 0.0, 1.0 );
	float xf = clamp( ( rwF - P3.y ) / ( P3.x - P3.y ), 0.0, 1.0 );
	float a = pow( max( 1.0 - ( 3.0 - 2.0 * xf ) * xf * xf, 0.0 ), 0.33 ) * ( P3.w - P3.z ) + P3.z;
	float xc = smooth01( clamp( ( rzF - P4.y ) / ( P4.x - P4.y ), 0.0, 1.0 ) );
#ifdef WW_WATER_DEPTH	// lane WATERDEPTH1 (WATERCOMBINE1: its own row's uniform)
	if ( waterDepthOn )
		return wwWaterDepthMix( c, lerp( waterMat[1].rgb, waterMat[0].rgb, xc ), a, along );
#endif
	return lerp( c, lerp( waterMat[1].rgb, waterMat[0].rgb, xc ), a );
}

float3 hi16( float3 v )
{
	float3 q = floor( clamp( v, 0.0, 1.0 ) * 65535.0 + 0.5 );
	return floor( q / 256.0 ) / 255.0;
}

float3 lo16( float3 v )
{
	float3 q = floor( clamp( v, 0.0, 1.0 ) * 65535.0 + 0.5 );
	return mod( q, 256.0 ) / 255.0;
}

void frag_main()
{
	float4 Var = waterMat[6], P1 = waterMat[7], P2 = waterMat[8], P3 = waterMat[9], P4 = waterMat[10];
	float4 Amp = waterMat[11], UV = waterMat[12];
	float3 posView = -ViewDir;
	float3 P = cellWorldPos( posView );
	if ( waterFarMask && wwFarWaterDry( P.xy ) )
		discard;
	// lane FARSEA1: the endless sheet leaves the bake's box to the baked water and the terrain (water_info.glsl)
	// lane SKYSEA1: waterSeam.w = the sheet's reach under the baked water (no crack where its last texels end)
	if ( waterSeam.z > 0.0 && all( (P.xy > waterSeamBox.xy + waterSeam.w) )
		&& all( (P.xy < waterSeamBox.zw - waterSeam.w) ) )
		discard;
	/* FARWATER1 late item 2 (bungo b71, open sea: a thin dotted dark line where the endless sea meets the map's sea): the
	 * map water's mesh edge at the box rim was the join, its edge pixels part covered (dark samples). On the map water
	 * (z 0) waterSeam.w is the hand-over: it leaves the outer band to the endless sheet under it (which reaches 4096 u
	 * in), so the join is a per-pixel cut over one continuous sheet, no mesh edge. Red WW_ENDLESS_SEA_RED=edge: w 0. */
	if ( waterSeam.z <= 0.0 && waterSeam.w > 0.0 && ( or( any( (P.xy < waterSeamBox.xy + waterSeam.w) ), any( (P.xy > waterSeamBox.zw - waterSeam.w) ) ) ) )
		discard;
	float3 eye = cellWorldPos( (float3)( 0.0 ) );
	float3 ray = P - eye;
	float dist = length( ray );
	float3 V = ray / max( dist, 1e-6 );
	bool under = eye.z < P.z;
#ifdef WW_WATER_REFL
	if ( waterSslr == 1 && waterSsrFlat ) {
		// lane FIX1101A: the ray pass off the flat surface, before any normal work; the colour pass bends the read
		float3 Rf = float3( V.xy, -V.z );
		float3 Rv = float3( dot( Rf, cellWorldDir( float3( 1.0, 0.0, 0.0 ) ) ), dot( Rf, cellWorldDir( float3( 0.0, 1.0, 0.0 ) ) ),
			dot( Rf, cellWorldDir( float3( 0.0, 0.0, 1.0 ) ) ) );
		fragColor = float4( -Rv.z > 0.2 && !under ? Rv : (float3)( 0.0 ), max( -posView.z, 1e-3 ) );
		return;
	}
#endif

	float dfade = clamp( ( dist - 8192.0 ) / ( P1.x - 8192.0 ), 0.0, 1.0 );

	// the opaque scene point behind this pixel
	int2 px = int2( gl_FragCoord.xy ) - viewportDimensions.xy;
	float vert = 1e9, along = 1e9;
#ifdef WW_WATER_OCEAN
	float3 ripS = P;	// lane WATERLOOK1 step 3: the opaque point behind, world
#endif
#ifdef WW_WATER_CAUSTICS
	float3 cauSv = (float3)( 0.0 );	// lane WATERCAUSTICS1: the scene point behind, view space; its factor and probe values
	float3 cauDbg = float3( 1.0, 0.0, 0.0 );
	float cauC = 1.0;
#endif
	if ( waterSslr == 1 ) {
		// the obscurance pass's target: .a = the opaque scene's view depth (game units), 0 where nothing drew
		float gz = waterDepth.Load( int3(int2( gl_FragCoord.xy ), 0) ).a;
		if ( gz > 0.0 ) {
			float3 S = cellWorldPos( posView * ( gz / max( -posView.z, 1e-6 ) ) );
			vert = abs( P.z - S.z );
			along = length( S - P );
#ifdef WW_WATER_OCEAN
			ripS = S;
#endif
		}
	} else if ( waterHaveDepth ) {
		float dz = waterDepth.Load( int3(px, 0) ).r;
		if ( dz < 1.0 ) {
			float sceneW = projectionMatrix[3][2] / ( projectionMatrix[2][2] + ( 2.0 * dz - 1.0 ) );
			float fragW = 1.0 / gl_FragCoord.w;
			float3 S = cellWorldPos( posView * ( sceneW / max( fragW, 1e-6 ) ) );
			vert = abs( P.z - S.z );
			along = length( S - P );
#ifdef WW_WATER_OCEAN
			ripS = S;
#endif
#ifdef WW_WATER_CAUSTICS
			cauSv = posView * ( sceneW / max( fragW, 1e-6 ) );
#endif
		}
	}
	float rz = clamp( 1.0 - vert / P1.w, 0.0, 1.0 );
	float rw = clamp( 1.0 - along / P1.w, 0.0, 1.0 );

	float x = clamp( ( rw - P3.y ) / ( P3.x - P3.y ), 0.0, 1.0 );
	float alphaV = pow( max( 1.0 - ( 3.0 - 2.0 * x ) * x * x, 0.0 ), 0.33 ) * ( P3.w - P3.z ) + P3.z;
	float shore = ( waterRed & 16 ) != 0 ? 0.0 : smooth01( clamp( ( rz - P2.x ) / ( 1.0 - P2.x ), 0.0, 1.0 ) );
#ifdef WW_WATER_SHORE
	// lane WATERSHORE1: the baked shore distance also fades the edge, so far water (no depth term) fades as near does
	float2 shoreField = waterShoreOn != 0 ? wwShoreField( P.xy ) : (float2)( 0.0 );
	// lane WATERFOAMDETAIL1: the soft contact from the water path to the scene behind and the bake (law 1), red: the bake
	float shoreEdgeV = ( waterShoreOn & 1 ) == 0 ? 0.0
		: waterShoreEdgeLaw == 1 ? wwShoreEdgeSoft( shoreField, along, V.z ) : wwShoreEdge( shoreField );
	// lane WATERFOAMDETAIL1: the foam by the wave energy at the shore (waterShoreLaw 1), the old band as its red (0)
	float shoreFoot = max( length( ddx( P.xy ) ), length( ddy( P.xy ) ) );
	float shoreColFoot = fwidth( min( vert, 4096.0 ) );
	float3 shoreFoamDbg = (float3)( 0.0 );
	float shoreFoamV = or( ( waterShoreOn & 2 ) == 0, under ) ? 0.0
		: waterShoreLaw == 1 ? wwShoreFoamWave( shoreField, P.xy, waterShoreTime, vert, shoreColFoot, shoreFoot, shoreFoamDbg )
		: wwShoreFoam( shoreField, P.xy, waterShoreTime );
	if ( ( waterRed & 16 ) == 0 )
		shore = max( shore, shoreEdgeV );
#endif
	float flattenWw = ( 1.0 - smooth01( clamp( ( rz - P2.w ) / ( P2.z - P2.w ), 0.0, 1.0 ) ) ) * P2.y;

	// the three noise layers
	float2 wxy = P.xy;
	float2 tap0, tap1, tap2;
	float3 n1 = noiseN( waterNoise0, _waterNoise0_sampler, wxy / UV.x + waterScroll0.xy, tap0 );
	float3 n2 = noiseN( waterNoise1, _waterNoise1_sampler, wxy / UV.y + waterScroll0.zw, tap1 );
	float3 n3 = noiseN( waterNoise2, _waterNoise2_sampler, wxy / UV.z + waterScroll1.xy, tap2 );
	float3 N = ( n1 - float3( 0.0, 0.0, 1.0 ) ) * Amp.x + float3( 0.0, 0.0, 1.0 );
	N += ( n2 * Amp.y + n3 * Amp.z ) * dfade;
	N = normalize( N );
#ifdef WW_WATER_WAVES
	// lane WATERWAVES1: the swell's slopes add to the noise layers' (slope-space blend), before the game's flatten
	float swellFoot = max( length( ddx( P.xy ) ), length( ddy( P.xy ) ) );
	float4 swell = waterWavesOn != 0 ? wwWaterSwell( P.xy, waterWavesK.z, swellFoot ) : (float4)( 0.0 );
	if ( swell.z > 0.0 )
		N = normalize( float3( N.xy / max( N.z, 1e-3 ) + swell.xy, 1.0 ) );
#endif
#ifdef WW_WATER_OCEAN
	// lane WATERLOOK1: on a baked body the wind's spectrum replaces the noise layers and the swell (slope space)
#ifdef WW_WATER_DISP
	float2 ocP = wwOcBase;	// the spectrum is defined on the undisplaced grid
#else
	float2 ocP = P.xy;
#endif
	float2 ocDx = ddx( ocP ), ocDy = ddy( ocP );
	WwOcean oc = waterOceanOn ? wwWaterOceanFlowed( ocP, ocDx, ocDy ) : { (float2)( 0.0 ), 0.0, 0.0, 0.0, 1.0 };
	if ( oc.w > 0.0 )
		N = normalize( float3( lerp( N.xy / max( N.z, 1e-3 ), oc.slope, oc.w ), 1.0 ) );
	// step 3: the incident short waves reflect off what stands in the water (red norip); uniform flow (derivatives)
	if ( waterOceanOn && ( waterOceanRed & 64 ) == 0 ) {
		float3 rip = wwContactRippleAt( vert, P, ripS, eye, oc.w * sqrt( oc.var ) );
		N = normalize( float3( N.xy / max( N.z, 1e-3 ) + rip.xy, 1.0 ) );
		oc.var += rip.z;
	}
#ifdef WW_WATER_REFL
	// lane FIX1101A (LATE ITEM 1, water_detail.glsl): the record's fine layers as the spectrum's tail, crisp near
	float ocKeep = 0.0;
	if ( waterOceanOn && oc.w > 0.0 ) {
		float2 det = wwWaterOceanDetail( wxy, UV.z, waterScroll1.xy, oc.var, dfade * oc.w, ocKeep );
		N = normalize( float3( N.xy / max( N.z, 1e-3 ) + det, 1.0 ) );
	}
#endif
#endif
#ifdef WW_WATER_CONTACT
	// lane WATERLOOK1 step 12: the contact foam's density, distance, trust, foot (uniform flow: derivatives)
	float4 contact = wwWaterContact( vert, P, ripS, eye, waterShoreTime );
	if ( under )
		contact.x = 0.0;
#endif
#ifdef WW_WATER_RIVERFOAM
	// lane WATERLOOK1 step 15: river foam (density, alpha, C T, speed); the pixel's width (u) in uniform flow
	float4 rfoam = wwWaterRiverFoam( P.xy, waterShoreTime, sqrt( length( cross( ddx( P ), ddy( P ) ) ) ) );
	if ( under )
		rfoam = (float4)( 0.0 );
#endif
	float3 N0 = N;
	N = normalize( flattenWw * ( N - float3( 0.0, 0.0, 1.0 ) ) + float3( 0.0, 0.0, 1.0 ) );
#ifdef WW_WATER_SURF
	// lane WATERLOOK1 step 11: the shore waves (slope space), after the game's flatten, which would erase them in the shallows
	float surfFoot = max( length( ddx( P.xy ) ), length( ddy( P.xy ) ) );
	WwSurf surf = under ? { (float2)( 0.0 ), 0.0, 0.0, 0.0 } : wwWaterSurf( P.xy, waterShoreTime, surfFoot );
	if ( surf.w > 0.0 )
		N = normalize( float3( N.xy / max( N.z, 1e-3 ) + surf.slope, 1.0 ) );
#endif
#ifdef WW_WATER_REFL	// lane WATERREFL1: GGX alpha^2 from the slopes the filtered noise lost + the footprint
	bool reflPhys = wwWaterReflPhys();	// lane FIX1031B: the physical Fresnel with the clear colour too (water_refl.glsl)
	float reflA2 = or( reflPhys, waterGlintOn ) ? wwWaterAlpha2( wxy / UV.x + waterScroll0.xy, wxy / UV.y + waterScroll0.zw,
		wxy / UV.z + waterScroll1.xy, tap0, tap1, tap2, Amp, dfade, flattenWw, Var.x, N, ( waterGlintRed & 2 ) == 0 ) : 0.0;
#endif
#if defined( WW_WATER_OCEAN ) && defined( WW_WATER_REFL )
	// lane WATERLOOK1: on a baked body the roughness is the spectrum's: the slopes the mips lost, the tail, the footprint
	if ( waterOceanOn && ( or( reflPhys, waterGlintOn ) ) ) {
		float2 ocS = N.xy / max( N.z, 0.05 );
		float2 ocSx = ddx( ocS ), ocSy = ddy( ocS );
		bool ocLean = ( waterGlintRed & 2 ) == 0;
		float ocFoot = ocLean ? min( 0.5 * ( dot( ocSx, ocSx ) + dot( ocSy, ocSy ) ), waterGlintK.z ) : 0.0;
		reflA2 = lerp( reflA2, 1e-4 + ( ocLean ? oc.var * ( 1.0 - ocKeep ) * flattenWw * flattenWw : 0.0 ) + ocFoot, oc.w );
	}
#endif

	if ( waterSslr == 1 ) {
		// the game's water ray pass: half the flattening, the ray into the view, the 0.2 gate
		float3 Nr = normalize( 0.5 * flattenWw * ( N0 - float3( 0.0, 0.0, 1.0 ) ) + float3( 0.0, 0.0, 1.0 ) );
		float3 Rw = reflect( V, Nr );
		float3 Rv = float3( dot( Rw, cellWorldDir( float3( 1.0, 0.0, 0.0 ) ) ), dot( Rw, cellWorldDir( float3( 0.0, 1.0, 0.0 ) ) ),
			dot( Rw, cellWorldDir( float3( 0.0, 0.0, 1.0 ) ) ) );
		fragColor = float4( -Rv.z > 0.2 && !under ? Rv : (float3)( 0.0 ), max( -posView.z, 1e-3 ) );
		return;
	}
	if ( ( waterRed & 32 ) != 0 )
		N = float3( 0.0, 0.0, 1.0 );
	float3 Ns = under ? -N : N;	// INFERRED: from below the surface faces the eye

	float c5 = 1.0 - clamp( dot( -V, Ns ), 0.0, 1.0 );
	float F = ( waterRed & 2 ) != 0 ? P4.z : P4.z + ( 1.0 - P4.z ) * c5 * c5 * c5 * c5 * c5;

	float3 R = reflect( V, Ns );
	float3 sky = lerp( waterSky[0].rgb, waterSky[1].rgb, clamp( R.z + 0.75, 0.0, 1.0 ) );
	sky = lerp( sky, waterSky[2].rgb, clamp( R.z * 1.9 + 0.35, 0.0, 1.0 ) );
#ifdef WW_WATER_REFL	// lane FARWATER1: at grazing the reflected ray meets the horizon, so the WTHR horizon row, not the gradient's
	// 0.65 x the lower/horizon mix + 0.35 x the upper there (red WW_CELL_WATER_REFL_RED=grazing)
	if ( ( waterReflRed & 32 ) == 0 )
		sky = lerp( waterSky[0].rgb, sky, smoothstep( 0.0, 0.08, R.z ) );
#endif
#ifdef WW_WATER_SKYFAR	// lane WATERSKYVISFAR1: the baked sky visibility of the far water (1 under water)
	float skyFarV = under ? 1.0 : wwWaterSkyFar( P );
#ifndef WW_WATER_SKYVIS	// lane WATERREBASE1: with the near grid compiled, its read carries the far byte (one factor)
	sky *= skyFarV;
#endif
#endif
#ifdef WW_WATER_REFL
	float3 reflSky = waterReflOn ? wwWaterCloudSky( sky, P, R ) : sky;	// lane WATERREFL1: before the game's SSR blend
	float4 reflRaw = (float4)( 0.0 ), reflFin = (float4)( 0.0 );
#endif
#ifdef WW_WATER_SKYVIS	// lane WATERLOOK1 step 10: the sky seen on the reflection's direction (the SSR over it is not dimmed)
#ifndef WW_WATER_SKYFAR
	const float skyFarV = 1.0;
#endif
#ifdef WW_WATER_REFL
	float skyVis = under ? 1.0 : wwWaterSkyVis( P, Ns, R, sqrt( max( reflA2, 0.0 ) ), skyFarV );
	reflSky *= skyVis;
#else
	float skyVis = under ? 1.0 : wwWaterSkyVis( P, Ns, R, 0.0, skyFarV );
#endif
	sky *= skyVis;
#endif
	// the game's screen-space reflection over the sky
	float4 ssr = (float4)( 0.0 );
	if ( waterSsrOn && !under ) {
		float2 suv = clamp( ( gl_FragCoord.xy - float2( viewportDimensions.xy ) ) / float2( viewportDimensions.zw ), 0.0, 1.0 );
		float2 st = float2( suv.x, 1.0 - suv.y );
#ifdef WW_WATER_REFL
		if ( waterSsrFlat )
			st += wwWaterSsrBend( R, float3( V.xy, -V.z ) );	// lane FIX1101A: the flat ray's hit, bent by this normal
#endif
		ssr = lerp( waterSsrRaw.SampleLevel( _waterSsrRaw_sampler, st, 0.0 ), waterSsrFin.SampleLevel( _waterSsrFin_sampler, st, 0.0 ), 0.9 );
#ifdef WW_WATER_REFL
		reflRaw = waterSsrRaw.SampleLevel( _waterSsrRaw_sampler, st, 0.0 );
		reflFin = waterSsrFin.SampleLevel( _waterSsrFin_sampler, st, 0.0 );
#endif
		if ( ( waterRed & 256 ) == 0 )
			sky = lerp( sky, ssr.rgb, ssr.a );
	}

	// the scene behind: in place, and offset by the normal (the game's shore tap)
	int2 size = textureSize( waterScene, 0 );
	float2 uv = ( float2( px ) + 0.5 ) / float2( size );
	float2 offUV = uv + float2( -N.x, -N.y ) * ( N.z * 0.0625 );
	int2 opx = clamp( int2( offUV * float2( size ) ), (int2)( 0 ), size - 1 );
	float3 texIn = waterScene.Load( int3(px, 0) ).rgb;
	float3 texOff = waterScene.Load( int3(opx, 0) ).rgb;
	float3 refr = frameToLinear( texIn );
	float3 refrOff = frameToLinear( texOff );
#ifdef WW_WATER_SHORE
	float3 refrIn = refr;	// lane WATERFOAMDETAIL1: the scene at this very pixel, before any water colour
#endif
	// the depth-color pass is drawn into the copy before the water: apply it here at this pixel's distances, and
	// at the offset texel's (its view ray met with the water plane)
	float offAlong = 0.0, offVert = 0.0, offHit = 0.0;
#ifdef WW_WATER_CAUSTICS
	// lane WATERCAUSTICS1: the sun's visibility at the bed (uniform flow: the cascades take derivatives)
	float cauSh = waterCauOn && waterCauShadow ? wwSunShadow( cauSv ) : 1.0;
	// lane CAUSTICPATTERN1: the bed's footprint per pixel (u), widening the net's lines so they never sparkle
	float cauFw = min( length( fwidth( cellWorldPos( cauSv ).xy ) ), 64.0 );
#endif
#ifdef WW_WATER_DEPTH
	WwWaterBed bed = { false, false, 1e9, 1e9, -1.0 };	// lane WATERDEPTH1: the bed depth this pixel uses
#ifdef WW_WATER_CLEAR	// lane WATERLOOK1 step 8: the clear colour reads the same bed, lit by this frame's sun and sky
	wwClearDeep = waterMat[1].rgb;	// lane FIX1031B: the in-scatter's hue is the cell's WATR deep colour
	if ( waterClearOn )
		wwWaterClearSetup( P, V, D.rgb * D.rgb, normalize( cellWorldDir( LightDir ) ), waterSky[0].rgb, waterSky[2].rgb );
	if ( or( waterDepthOn, waterClearOn ) )
#else
	if ( waterDepthOn )
#endif
		bed = wwWaterBedAt( P, V, vert, along );
#endif
	if ( !under && waterSslr == 0 ) {
#ifdef WW_WATER_CAUSTICS
		// lane WATERCAUSTICS1: under a baked body, on the drawn bed (not the bake's), before the depth colour takes it
		float cauBd = waterCauOn ? wwWaterBedDepth( P ) : -1.0;
#ifdef WW_WATER_OCEAN
		wwCauPathGain = lerp( 1.0, oc.cau, oc.w );	// lane WATERLOOK1: the net focuses at the spectrum's own curvature
#endif
		if ( cauBd >= 0.0 && vert < 1e8 && vert <= cauBd + 256.0 ) {
			float3 cauC3 = wwWaterCaustic( cellWorldPos( cauSv ), vert, normalize( cellWorldDir( LightDir ) ), D.rgb * D.rgb,
				0.5 * ( waterSky[0].rgb + waterSky[2].rgb ), cauSh, cauFw, cauDbg );
			cauC = cauC3.g;	// CAUSTICPATTERN1: per channel (the edges' colour split); the probes read green
			refr *= cauC3;
		}
#endif
#ifdef WW_WATER_DEPTH
#ifdef WW_WATER_CLEAR	// lane WATERLOOK1 step 8: no bed drawn = the class's bed albedo, lit
		if ( waterClearOn && waterClearRed != 4 ) {
			// lane FIX1031B: the baked and the drawn bed blend (no hard switch: the teeth); red old = the step below
			float bedW = wwWaterBedNearW( bed.bd, vert, dist, waterClearRed != 6 );	// lane FIX1101A: near = the scene, per pixel
			float3 bedC = bedW > 0.0 ? depthFog( wwWaterClearBed(), bed.bd / max( -V.z, 1e-3 ), bed.bd ) : (float3)( 0.0 );
			refr = bedW >= 1.0 ? bedC : lerp( depthFog( refr, along, vert ), bedC, bedW );
		} else
			refr = bed.baked ? depthFog( waterClearOn ? wwWaterClearBed() : waterMat[0].rgb, bed.along, bed.vert )
				: depthFog( refr, along, vert );
#else
		refr = bed.baked ? depthFog( waterMat[0].rgb, bed.along, bed.vert ) : depthFog( refr, along, vert );
#endif
#else
		refr = depthFog( refr, along, vert );
#endif
		float2 ndc = ( float2( opx ) + 0.5 ) / float2( size ) * 2.0 - 1.0;
		float odz = waterHaveDepth ? waterDepth.Load( int3(opx, 0) ).r : 1.0;
		float4x4 iP = wwInverse( projectionMatrix );
		float4 h = mul(float4( ndc, 2.0 * min( odz, 0.999999 ) - 1.0, 1.0 ), iP);
		float3 Sv = h.xyz / h.w;
		float3 Sw = cellWorldPos( Sv );
		float3 dW = normalize( Sw - eye );
		float t = abs( dW.z ) > 1e-6 ? ( P.z - eye.z ) / dW.z : -1.0;
		bool sceneAt = odz < 1.0;
		if ( t > 0.0 && ( or( !sceneAt, t < length( Sw - eye ) ) ) ) {
			float3 Wp = eye + dW * t;
			offAlong = sceneAt ? length( Sw - Wp ) : 1e9;
			offVert = sceneAt ? abs( Wp.z - Sw.z ) : 1e9;
			offHit = 1.0;
			refrOff = depthFog( refrOff, offAlong, offVert );
		}
	}

	float3 silted = refr;
#ifdef WW_WATER_CLEAR	// lane WATERLOOK1 step 8: the in-scatter is the water's murk; the WATR's silt is not added on top
	if ( ( waterRed & 4 ) == 0 && !waterClearOn )
#else
	if ( ( waterRed & 4 ) == 0 )
#endif
		silted = refr + waterMat[4].w * ( 1.0 - alphaV ) * ( lerp( waterMat[5].rgb, waterMat[4].rgb, refr ) - refr );

	float3 Ldir = normalize( cellWorldDir( LightDir ) );
	float3 sun = D.rgb * D.rgb;
	float3 spec = sun * pow( clamp( dot( R, Ldir ), 0.0, 1.0 ), Var.x ) * waterMat[1].w
		+ sun * pow( clamp( dot( N, float3( -0.099, -0.099, 0.99 ) ), 0.0, 1.0 ), waterMat[0].w ) * P1.z;
#ifdef WW_WATER_REFL
	// lane WATERREFL1: the sun's glare from the roughness (no fixed-direction sparkle); red game keeps today's spec
	float3 glare = (float3)( 0.0 );
	float glFoot = length( cross( ddx( P ), ddy( P ) ) );	// lane WATERLOOK1 step 9: the footprint (u^2), uniform flow
	if ( waterGlintOn && ( waterGlintRed & 1 ) == 0 && !under ) {
		// step 9: the sun's disc in the lobe; the mean glare, its shoulder, x the facets counted in this pixel (FIX1031B)
		float glX = wwWaterGlareGlit( Ns, V, Ldir, max( reflA2, 1e-6 ), P.xy, glFoot );
		glare = sun * glX;
		spec = glare;
	}
	float reflAlpha = sqrt( max( reflA2, 0.0 ) );
	float reflF = wwWaterFresnel( dot( -V, Ns ), reflAlpha, waterGlintK.y );
	float3 reflEnv = wwWaterReflEnv( reflSky, reflRaw, reflFin, waterSsrOn && !under && ( waterRed & 256 ) == 0, reflAlpha );
#endif
#ifdef WW_WATER_SKYVIS
	if ( !under )
		spec *= wwWaterSkyVisSun( P, Ns, Ldir, skyFarV );	// lane WATERLOOK1 step 10: the glare where the sun's sky is seen
#elif defined( WW_WATER_SKYFAR )
	spec *= skyFarV;	// lane WATERSKYVISFAR2: the byte as specular occlusion (water walled in by cliffs glints less)
#endif
	if ( ( waterRed & 8 ) != 0 )
		spec = (float3)( 0.0 );

	float3 lin;
	if ( under ) {
		// the game's underwater variant: half the sky and half the underwater color, the scene by 1 - F
		float3 refl = 0.5 * ( sky + waterMat[3].rgb );
		lin = lerp( refl, refr, 1.0 - F );
#ifdef WW_WATER_UNDER
		if ( waterUnderOn && ( waterUnderRed & 1 ) == 0 ) {
			/* lane WATERLOOK1 step 16: Snell's window: the air's light comes through only inside the critical cone; past
			 * it the surface mirrors the water below, the clear colour's deep water along the mirrored ray */
			if ( ( waterUnderRed & 2 ) != 0 )
				wwWaterClearSetup( P, V, D.rgb * D.rgb, normalize( cellWorldDir( LightDir ) ), waterSky[0].rgb, waterSky[2].rgb );
			else
				wwWaterClearSetup( P, V, waterUnderSun, waterUnderSunTo, waterSky[0].rgb, waterSky[2].rgb );
			lin = lerp( refr, wwWaterClear( (float3)( 0.0 ), 1e9, 1e9 ), wwUnderFresnel( clamp( dot( -V, Ns ), 0.0, 1.0 ) ) );
		}
#endif
	} else {
		float wRefl = ( waterRed & 1 ) != 0 ? 0.0 : F * Var.y;
		float3 far = lerp( waterMat[2].rgb, sky, waterMat[2].w );
#ifdef WW_WATER_DEPTH
		far = wwWaterDepthFar( far, silted, F, bed.known );	// lane WATERDEPTH1
#endif
		lin = lerp( silted, sky, wRefl );
		lin = lerp( far, lin, dfade ) + spec;
#ifdef WW_WATER_REFL
		if ( reflPhys && ( waterRed & 1 ) == 0 )	// lane WATERREFL1: physical Fresnel, near law to the horizon
			lin = wwWaterReflMix( silted, reflEnv, reflF, F, Var.y, far, dfade ) + spec;
#endif
		lin = lerp( lin, refrOff, shore );
#ifdef WW_WATER_SHORE
		/* lane WATERFOAMDETAIL1: at the contact the water path is 0, so no depth colour, murk or shifted tap either: the
		 * pixel becomes the scene it covers and the dry side continues without a seam */
		if ( waterShoreEdgeLaw == 1 && ( waterRed & 16 ) == 0 )
			lin = lerp( lin, refrIn, shoreEdgeV );
		/* lane WATERSHORE1: the foam, a Lambert white of albedo 0.55 (fresh sea foam, Koepke 1984, Applied Optics
		 * 23(11)) lit by the sun and the water's upper sky row; opacity K1.w lets the water read through its holes */
		float3 foamLin = 0.55 * ( sun * clamp( Ldir.z, 0.0, 1.0 ) + waterSky[2].rgb );
		lin = lerp( lin, foamLin, shoreFoamV * waterShoreK1.w );
		/* lane FIX1101A (P0, the manager's rvc6_on 10-31: a large grey shape on the dry bank): the water plane runs on
		 * under a bank that lies a little below it, where the soft edge has made the water the scene it covers; the
		 * river, contact foam and whitecaps still painted there. They float only on water: x the water's own share
		 * left by the edge, and nothing over a column under 4 u (6 cm; full from 16 u). Red
		 * WW_CELL_WATER_RIVERFOAM_RED=paint (32) */
#ifdef WW_WATER_RIVERFOAM
		float foamWet = wwFoamWet( shoreEdgeV, vert );
#else
		float foamWet = 1.0;
#endif
#ifdef WW_WATER_SURF
		lin = lerp( lin, foamLin, wwSurfFoamAlpha( surf.foam, P.xy, surfFoot, waterShoreTime ) );	// step 11: the breakers' foam
#endif
#ifdef WW_WATER_CONTACT
		lin = lerp( lin, foamLin, foamWet * wwContactFoamAlpha( contact.x, P.xy, contact.w, waterShoreTime ) );	// step 12: contact foam
#endif
#ifdef WW_WATER_RIVERFOAM
		lin = lerp( lin, foamLin, foamWet * rfoam.y );	// step 15: river foam streaks
#endif
#endif
#ifdef WW_WATER_OCEAN
		// lane WATERLOOK1 step 4: sunlight through the crests, the body colour, near faces and far mean alike (red noglow)
		if ( waterOceanOn && ( waterOceanRed & 128 ) == 0 && oc.w > 0.0 )
			lin += ( 1.0 - F ) * oc.w * wwWaterCrestGlow( N0.xy / max( N0.z, 1e-3 ), oc.var, V, Ldir, sun, waterMat[0].rgb );
		/* lane WATERLOOK1: whitecaps where the chop folds the surface (its Jacobian, Tessendorf 2001 section 4.4), from
		 * 4 m/s; lit as WATERSHORE1's foam (albedo 0.55, Koepke 1984) */
		if ( oc.white > 0.0 )
#if defined( WW_WATER_SHORE ) && defined( WW_WATER_RIVERFOAM )
			lin = lerp( lin, 0.55 * ( sun * clamp( Ldir.z, 0.0, 1.0 ) + waterSky[2].rgb ), 0.8 * oc.white * foamWet );
#else
			lin = lerp( lin, 0.55 * ( sun * clamp( Ldir.z, 0.0, 1.0 ) + waterSky[2].rgb ), 0.8 * oc.white );
#endif
#endif
	}
	lin = max( lin, (float3)( 0.0 ) );
	float3 pAir = posView;	// FARWATER1 round 2: the air as the far land at the same distance gets it (waterAir)
	wwAirFree = ( waterAir & 1 ) != 0;
	if ( ( waterAir & 2 ) != 0 )
		pAir = wwAirFlatPos( posView );
	if ( or( fogOn, wwApOn() ) )	// lane SKYA: the sky's aerial perspective with the fog off too
		lin = max( wwFog( lin, pAir ), (float3)( 0.0 ) );

	float3 s = sqrt( lin );
	float3 outC = ( cellOn && cellIsOn ) ? ( cellIsLinear ? lin : cellImageSpace( s ) ) : tonemap( s );
	fragColor = float4( outC, 1.0 );
#ifdef WW_DV
	if ( cellOn && wwDebugView != 0 ) {	// lane DEBUGVIEW1: the water's views (src/features/overlays/debugview.h)
		int dvOwn = waterSeam.z > 0.5 ? 7 : wwDvOwner == 6 ? 6 : 5;	// endless sea, far water, near water
		float dvSkyFar = 1.0;
#if defined( WW_WATER_SKYFAR ) || defined( WW_WATER_SKYVIS )
		dvSkyFar = skyFarV;
#endif
		int v = wwDebugView;
		float3 o = (float3)( 0.0 );
		if ( v == WW_DV_WATER )
			o = wwDvOwnerColour( dvOwn ) * ( 0.5 + 0.5 * dfade );	// brighter = further into the far colour swap
		else if ( v == WW_DV_WATERREFL )
			o = float3( clamp( F, 0.0, 1.0 ), clamp( dvSkyFar, 0.0, 1.0 ), dfade );
		else if ( v == WW_DV_WATERHEIGHT )
			o = wwDvRamp( degrees( asin( clamp( -V.z, -1.0, 1.0 ) ) ) * 0.5 );	// 0 .. 2 degrees under the level
		else if ( v == WW_DV_NORMALS )
			o = normalize( N ) * 0.5 + 0.5;
		else if ( v == WW_DV_FAROWNER )
			o = wwDvOwnerColour( dvOwn );
		else if ( v == WW_DV_EXPOSURE )
			o = wwDvAbs( lin );
		else if ( v == WW_DV_DEPTH )
			o = wwDvDepth( dist );
		else if ( v == WW_DV_AIR )	// FARWATER1 round 2: the air's transmittance per channel (white clear, black opaque)
			o = clamp( wwFog( (float3)( 1.0 ), pAir ) - wwFog( (float3)( 0.0 ), pAir ), 0.0, 1.0 );
		else if ( v == WW_DV_WATERSKY ) {	// lane FARWATER1: the sky this tier mirrors (before the march), x / (1 + x)
#ifdef WW_WATER_REFL
			o = reflSky / ( (float3)( 1.0 ) + max( reflSky, (float3)( 0.0 ) ) );
#else
			o = sky / ( (float3)( 1.0 ) + max( sky, (float3)( 0.0 ) ) );
#endif
		}
		fragColor = float4( o, 1.0 );
	}
#endif

#ifndef WW_WATER_NOPROBE	// lane WATERFIX7: the looks' shipping twins leave the probes to their probe twins (*p.prog)
	if ( waterProbe > 0 ) {
		float3 o = (float3)( 0.0 );
		if ( or( waterProbe == 1, waterProbe == 2 ) )
			o = waterProbe == 1 ? hi16( N * 0.5 + 0.5 ) : lo16( N * 0.5 + 0.5 );
		else if ( or( waterProbe == 3, waterProbe == 4 ) )
			o = waterProbe == 3 ? hi16( V * 0.5 + 0.5 ) : lo16( V * 0.5 + 0.5 );
		else if ( or( waterProbe == 5, waterProbe == 6 ) )
			o = waterProbe == 5 ? hi16( float3( rz, rw, dfade ) ) : lo16( float3( rz, rw, dfade ) );
		else if ( waterProbe == 7 )
			o = texIn;
		else if ( waterProbe == 8 )
			o = texOff;
		else if ( or( waterProbe == 9, waterProbe == 10 ) ) {
			int col = int( gl_FragCoord.x ) % 4;
			float3 v = col == 0 ? sun * 0.25 : col == 1 ? Ldir * 0.5 + 0.5
				: col == 2 ? float3( A.a / 8.0, D.a / 8.0, under ? 1.0 : 0.0 )
				: float3( ( cellOn && cellIsOn ) ? 1.0 : 0.0, cellIsLinear ? 1.0 : 0.0, fogOn ? 1.0 : 0.0 );
			o = waterProbe == 9 ? hi16( v ) : lo16( v );
		} else if ( or( waterProbe == 11, waterProbe == 12 ) )
			o = waterProbe == 11 ? hi16( float3( tap0, tap1.x ) ) : lo16( float3( tap0, tap1.x ) );
		else if ( or( waterProbe == 13, waterProbe == 14 ) )
			o = waterProbe == 13 ? hi16( float3( tap1.y, tap2 ) ) : lo16( float3( tap1.y, tap2 ) );
		else if ( or( waterProbe == 15, waterProbe == 16 ) )
			o = waterProbe == 15 ? hi16( ssr.rgb ) : lo16( ssr.rgb );
		else if ( or( waterProbe == 17, waterProbe == 18 ) )
			o = waterProbe == 17 ? hi16( float3( ssr.a, 0.0, 0.0 ) ) : lo16( float3( ssr.a, 0.0, 0.0 ) );
		else if ( waterProbe >= 19 && waterProbe <= 21 )
			o = fix24( float3( dist / 16384.0, along / 4096.0, vert / 4096.0 ), waterProbe - 19 );
		else if ( waterProbe >= 22 && waterProbe <= 24 )
			o = fix24( float3( offAlong / 4096.0, offVert / 4096.0, offHit ), waterProbe - 22 );
		else if ( or( waterProbe == 25, waterProbe == 26 ) )	// the linear light this pixel writes, / 4
			o = waterProbe == 25 ? hi16( lin * 0.25 ) : lo16( lin * 0.25 );
		else if ( waterProbe == 27 )	// the water's pixels, magenta
			o = float3( 1.0, 0.0, 1.0 );
#ifdef WW_WATER_INFO_PROBE
		else if ( waterProbe >= 28 && waterProbe <= 31 ) {	// WATERDATA1: the baked water info at this pixel
			uint w = wwWaterInfoWord( P.xy );
			float4 wi = wwWaterInfo( P.xy );
			uint id = w & 0xFFFFu;
			o = waterProbe == 28 ? float3( float( ( w >> 16 ) & 0xFFu ), float( w >> 24 ), waterInfoOn ? 255.0 : 0.0 ) / 255.0
				: waterProbe == 29 ? float3( float( id >> 8 ), float( id & 0xFFu ), wi.w + 1.0 ) / 255.0
				: waterProbe == 30 ? ( id != 0u ? float3( 0.0, 1.0, 1.0 ) : float3( 1.0, 0.0, 0.0 ) )
				: (float3)( clamp( wi.y / 1024.0, 0.0, 1.0 ) );
		}
		else if ( waterProbe == 41 ) {	// WATERFLOWBAKE1: the baked flow at this pixel, u/s (32 in its lane)
			float2 fv = wwWaterFlow( P.xy );
			o = float3( clamp( fv * 0.125 + 0.5, 0.0, 1.0 ), waterFlowOn ? 1.0 : 0.0 );
		}
#endif
#ifdef WW_WATER_DEPTH
		else if ( or( waterProbe == 32, waterProbe == 33 ) )	// WATERDEPTH1: 32 the depth used / 1024 grey; 33 baked cyan, scene green, none red
			o = waterProbe == 32 ? (float3)( clamp( bed.vert / 1024.0, 0.0, 1.0 ) )
				: bed.baked ? float3( 0.0, 1.0, 1.0 ) : bed.known ? float3( 0.0, 1.0, 0.0 ) : float3( 1.0, 0.0, 0.0 );
#endif
#ifdef WW_WATER_CAUSTICS
		else if ( or( waterProbe == 35, waterProbe == 36 ) )	// WATERCAUSTICS1: 35 the factor / 2 grey; 36 (I / 8, env, share)
			o = waterProbe == 35 ? (float3)( clamp( cauC * 0.5, 0.0, 1.0 ) ) : float3( cauDbg.x / 8.0, cauDbg.y, cauDbg.z );
#endif
#ifdef WW_WATER_SHORE
		else if ( waterProbe == 37 )	// WATERSHORE1: (edge, foam, shore distance / 1024) at this pixel
			o = float3( shoreEdgeV, shoreFoamV, clamp( shoreField.x / 1024.0, 0.0, 1.0 ) );
		else if ( waterProbe == 38 )	// WATERFOAMDETAIL1: (Hs / 64 u, the energy amount, the column used / 64 u)
			o = clamp( shoreFoamDbg, 0.0, 1.0 );
#endif
#ifdef WW_WATER_REFL
		else if ( waterProbe == 44 )	// WATERREFL1: (F, sqrt( alpha ) / 0.5, SSR confidence); 45/46 the glare / 16
			o = float3( reflF, clamp( sqrt( reflAlpha ) * 2.0, 0.0, 1.0 ), clamp( lerp( reflRaw.a, reflFin.a, 0.5 ), 0.0, 1.0 ) );
		else if ( or( waterProbe == 45, waterProbe == 46 ) )
			o = waterProbe == 45 ? hi16( glare / 16.0 ) : lo16( glare / 16.0 );
#endif
#ifdef WW_WATER_SKYFAR
		else if ( waterProbe == 47 )	// WATERSKYVISFAR2: (sky visibility, the sun spec's luminance / 16, 0)
			o = float3( skyFarV, clamp( dot( spec, float3( 0.2126, 0.7152, 0.0722 ) ) / 16.0, 0.0, 1.0 ), 0.0 );
#endif
#ifdef WW_WATER_WAVES
		else if ( waterProbe == 34 )	// WATERWAVES1: (0.5 + 4 x the swell's slope x, y; its weight) at this pixel
			o = float3( clamp( 0.5 + 4.0 * swell.xy, 0.0, 1.0 ), swell.z );
#endif
#ifdef WW_WATER_OCEAN
		else if ( or( waterProbe == 39, waterProbe == 40 ) )	// WATERLOOK1: the spectrum (header)
			o = waterProbe == 39 ? float3( clamp( 0.5 + 4.0 * oc.slope, 0.0, 1.0 ), oc.w )
				: float3( oc.white, sqrt( oc.var ) / 0.5, oc.cau / 3.0 );
#endif
#ifdef WW_WATER_CONTACT
		else if ( waterProbe == 47 )	// WATERLOOK1 step 12: (contact foam density, distance / 1024, trust)
			o = float3( contact.x, clamp( contact.y / 1024.0, 0.0, 1.0 ), contact.z );
#endif
#ifdef WW_WATER_RIVERFOAM
		else if ( waterProbe == 48 )	// WATERLOOK1 step 15: (river foam density, alpha, 0.5 + 0.5 C T)
			o = float3( rfoam.x, rfoam.y, clamp( 0.5 + 0.5 * rfoam.z, 0.0, 1.0 ) );
#endif
		fragColor = float4( o, 1.0 );
	}
#endif
}

struct SPIRV_Cross_Input
{
    WW_LOC(3) float3 LightDir : TEXCOORD3;
    WW_LOC(4) float3 ViewDir : TEXCOORD4;
    WW_LOC(8) float2 texCoord : TEXCOORD8;
    WW_LOC(5) float3x3 btnMatrix : TEXCOORD5;
    WW_LOC(0) nointerpolation float4 A : TEXCOORD0;
    WW_LOC(1) float4 C : TEXCOORD1;
    WW_LOC(2) nointerpolation float4 D : TEXCOORD2;
    WW_LOC(12) float rawVertexAlpha : TEXCOORD12;
    WW_LOC(9) nointerpolation float3x3 reflMatrix : TEXCOORD9;
    float4 gl_FragCoord : SV_Position;
#if defined(WW_WATER_DISP)
    float2 wwOcBase : TEXCOORD9;
#endif
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    LightDir = stage_input.LightDir;
    ViewDir = stage_input.ViewDir;
    texCoord = stage_input.texCoord;
    btnMatrix = stage_input.btnMatrix;
    A = stage_input.A;
    C = stage_input.C;
    D = stage_input.D;
    rawVertexAlpha = stage_input.rawVertexAlpha;
    reflMatrix = stage_input.reflMatrix;
#if defined(WW_WATER_DISP)
    wwOcBase = stage_input.wwOcBase;
#endif
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
