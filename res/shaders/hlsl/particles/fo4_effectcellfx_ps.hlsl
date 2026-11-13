// HLSL source of features/particles/fo4_effectcellfx.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_AIRZONE, WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "ParticlesBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// inlined (no readable .hlsli yet): fo4_effectshader.frag, uniforms.glsl
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
static float4 gl_FragCoord;
static bool gl_FrontFacing;
// lane FXRES1 (src/features/particles/cellfxres.h): fo4_effectcell with the soft-effect layer's transmittance and coverage outputs
#define WW_CELLLIGHTS 1
#define WW_CELL_FX 1
#define WW_FXRES 1
// fo4_effectshader.frag inlined here: its .hlsli has no readable half yet (lane HLSLSRC1)

Texture2D<float4> BaseMap : register(t46, space0); SamplerState _BaseMap_sampler : register(s46, space0);
Texture2D<float4> GreyscaleMap : register(t47, space0); SamplerState _GreyscaleMap_sampler : register(s47, space0);
TextureCube<float4> CubeMap : register(t48, space0); SamplerState _CubeMap_sampler : register(s48, space0);
Texture2D<float4> NormalMap : register(t49, space0); SamplerState _NormalMap_sampler : register(s49, space0);
Texture2D<float4> SpecularMap : register(t50, space0); SamplerState _SpecularMap_sampler : register(s50, space0);

static float3 LightDir;
static float3 ViewDir;

static float2 texCoord;

static float4 A;
static float4 C;
static float4 D;

static float3x3 btnMatrix;
static float3x3 reflMatrix;

#ifdef WW_FXRES
// lane FXRES1 (fo4_effectcellfx.frag, src/features/particles/cellfxres.h): the soft-effect layer's other two targets, blended by the
// draw's own factors: the transmittance (0 colour, the draw's alpha, over 1) and the coverage (1, as the stencil)
static float4 fragColor;
static float4 fxResT;
static float4 fxResCov;
#else
static float4 fragColor;
#endif

#define WW_LOOSE_TYPES 1
#if defined(WW_CELLLIGHTS)
#include "../air/lookdev_fog.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && defined(WW_SUNSHADOW)
#include "../shadows/ww_sunshadow.hlsli"
#endif
#if defined(WW_CELLLIGHTS)
#ifndef WW_CELL_FX
#define WW_CELL_FX 1	// as below, before this file
#endif
#if defined(WW_CELLLIGHTS) && !defined(WW_FX_DECALLIT)
#ifndef WW_FX_AMBSHARE
#define WW_FX_AMBSHARE 1	// as below, before this file
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
#if defined(WW_CELLLIGHTS) && defined(WW_FX_DECALLIT)
#include "../ao/cell_ao.hlsli"
#endif
#if defined(WW_CELLLIGHTS)
#if defined(WW_CELLLIGHTS) && !defined(WW_FX_DECALLIT)
#ifndef WW_FX_AMBSHARE
#define WW_FX_AMBSHARE 1	// as below, before this file
#endif
#endif
#include "../lights/cell_fxlaw.hlsli"
#include "../air/cell_volpart.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && !defined(WW_SUNSHADOW)
#include "../shadows/ww_sunshadow.hlsli"
#endif
#if defined(WW_CELLLIGHTS)
#ifndef WW_AIR_GIVOL
#define WW_AIR_GIVOL 1	// as below, before this file
#endif
#include "../air/cell_airmed.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && ((defined( WW_CELL_AIRZONE ) && !defined( WW_FX_DECALLIT )))
#include "../air/cell_airzone.hlsli"
#endif
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#if defined(WW_CELLLIGHTS)
#include "../air/lookdev_fog.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && defined(WW_SUNSHADOW)
#include "../shadows/ww_sunshadow.hlsli"
#endif
#if defined(WW_CELLLIGHTS)
#ifndef WW_CELL_FX
#define WW_CELL_FX 1	// as below, before this file
#endif
#if defined(WW_CELLLIGHTS) && !defined(WW_FX_DECALLIT)
#ifndef WW_FX_AMBSHARE
#define WW_FX_AMBSHARE 1	// as below, before this file
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
#if defined(WW_CELLLIGHTS) && defined(WW_FX_DECALLIT)
#include "../ao/cell_ao.hlsli"
#endif
#if defined(WW_CELLLIGHTS)
#if defined(WW_CELLLIGHTS) && !defined(WW_FX_DECALLIT)
#ifndef WW_FX_AMBSHARE
#define WW_FX_AMBSHARE 1	// as below, before this file
#endif
#endif
#include "../lights/cell_fxlaw.hlsli"
#include "../air/cell_volpart.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && !defined(WW_SUNSHADOW)
#include "../shadows/ww_sunshadow.hlsli"
#endif
#if defined(WW_CELLLIGHTS)
#ifndef WW_AIR_GIVOL
#define WW_AIR_GIVOL 1	// as below, before this file
#endif
#include "../air/cell_airmed.hlsli"
#endif
#if defined(WW_CELLLIGHTS) && ((defined( WW_CELL_AIRZONE ) && !defined( WW_FX_DECALLIT )))
#include "../air/cell_airzone.hlsli"
#endif
#undef WW_LOOSE_UNIFORMS
	bool hasSourceTexture;
	bool hasGreyscaleMap;
	bool hasCubeMap;
	bool hasNormalMap;
	bool hasEnvMask;
	bool greyscaleAlpha;
	bool greyscaleColor;
	bool useFalloff;
	bool hasRGBFalloff;
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
	// lane FXCHECK1: the game masks an effect's cube with the env mask's R (the _s specular); false = G, as before
	bool envMaskR;
	// lane GLASSPANE2 (FXNORMALA1): the game's env cube is also x the normal map's alpha; true = the red (the old cube
	// without it, WW_FX_NORMALA_RED). Red sense, so a program that never sets it keeps the game's rule.
	bool envNormalAlphaRed;
#if defined(WW_CELLLIGHTS) && !defined(WW_FX_DECALLIT)
	/* MODULES1 R8-1 (bungo's fake volumetric rule: "Opacity gives the height, or rather 'depth' always"): a volumetric
	 * card is a lump whose depth is its opacity, alpha 1 = half a UV unit of the card. Its normal is tilted down the
	 * opacity's slope (Mikkelsen 2010, "Bump Mapping Unparametrized Surfaces on the GPU": the surface gradient from
	 * screen derivatives), so a flat card shades toward a lamp like a mound. No texture tap: three derivative pairs.
	 * With a normal map the two slopes ADD at weight 1 : 1 (Mikkelsen 2020, "Surface Gradient Based Bump Mapping
	 * Framework": heights add, so their gradients add; both are slopes in the same view units): the map's gradient
	 * against the card's face, N - n / ( n . N ), plus the opacity's, then one normalize.
	 * fxLumpRed (WW_CELL_FXLOCAL_RED): 1 `face` = no opacity slope at all, 2 `mapalone` = a card with a normal map takes
	 * the map alone (the first R8-1 commit), 4 `ball` = the UV ball stands in for the depth again (FX_PUFF_A). */
	int fxLumpRed;
	/* MODULES1 round 9 (bungo: a mist card "is a slab of denser air"): a blended lit MESH card on the volumetric law takes
	 * the light the air at its pixel scatters toward the eye and nothing else: the lamps x the air's phase, each in its own
	 * colour with its own falloff (red by a red lamp, orange by a fire), + the sky's share under the record's cap, x its
	 * opacity as density. No surface share (the wrapped diffuse of a lump: up to 4 x the air's scatter per lamp) and no GI
	 * surface term (the volume's irradiance on an up face, uncapped: a bluish-white sum over warm lamps = a grey-white
	 * bank). So a card is never brighter, nor of another hue, than thick air under the same lights. fxAirCard: 1 = a mesh
	 * card (renderer.cpp), 0 = a particle system (still a lump). Red WW_CELL_FXLOCAL_RED=lump (fxLocalRed bit 4): the lump. */
	int fxAirCard;
#endif
#if defined(WW_CELLLIGHTS)
	bool fxSoft;
	float fxSoftDepth;		// game units
	float fxDistScale;		// view units -> game units
	bool fxAdditive;
	int fxRed;				// 2 nosoft, 4 nolin, 8 hide, 32 unbound (MODULES1 item 2)
	// lane DECALLIT1 (WW_CELL_DECALLIT_PROBE, gates only): an effect decal in a probe pass draws a white colour, opaque
	// where it shows, so probe 6 holds the light it takes (lit as the wall, or self-lit under the red)
	bool fxDecalProbe;
	// lane GIDARK1: a lit effect outdoors drawn by this program (fo4_effectdecal / csm) takes the light of the spot it
	// stands in (fxRoomLitMul); cellFxBase = the game's lit-effect base (the weather's Effect Lighting x the sun scale)
	bool fxRoomLit;
	float3 cellFxBase;
	// lane FXLIT1: this shape's model's placed lights (src/features/lights/cellfxlit.h)
	int fxLitMode;			// 0 an unlit effect; 1 lit; red controls: 2 every light, 4 no 2.2 on the falloff, 8 self-lit
	float4 fxLit;				// indices into cellLights, -1 = none
	float4 fxLitScale;		// 1 (a red control's per-light ratio)
	float3 fxLitId;			// probe 70: the model's serial + 1, 24 bits
	float4 fxEmit;			// lane LIGHTS2 (XEMI1): the placement's emittance colour ^2.2 on an unlit effect; w 0 = none
	float2 vpDumpScale;		// gates only: the full-size pixels a quarter-size dump pixel stands for (0 off)
#endif
};
#ifdef WW_CELLLIGHTS
/* lane EFX2 (fo4_effectcell.frag; src/gl/celllights.h wwCellFxRed): the game's effect pixel shader in the
 * cell view, transcribed from Shaders011.fxp (docs/PRTP_PLAN.md 2q):
 *   colour   = texture x vertex colour x base colour, all linear (sampled sRGB, base colour powered 2.2)
 *   lighting = mix( c, c x emit, lighting influence ), emit the script-set external emit colour, WHITE when
 *              unset. That is the UNLIT effect. One whose material sets the effect-lighting flag takes the
 *              game's lit shader instead (lane FXLIT1, src/features/lights/cellfxlit.h): emit = the directional colour +
 *              up to four placed lights, colour x pow( 1 - saturate( d / radius )^2, 2.2 ) x the spot cone,
 *              per pixel, no N.L and no shadow
 *   Soft     = saturate( (scene w - w) / soft depth ) x smoothstep( 0.075, 0.5, w / soft depth ), w the view
 *              depth; on alpha, or on the palette's row for a greyscale-to-palette effect
 *   fog      = mix( c, fog colour, f ) for a blended effect, c x (1 - f) for an additive one; with the cell air on,
 *              the air as the surfaces take it, c T + L, or c T for an additive one (lane VOLAIR1, wwFogFx)
 * then the cell's imagespace on its own (an effect blends over a surface that already took the bloom). */
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
#include "../air/lookdev_fog.hlsli"
#ifdef WW_SUNSHADOW
#include "../shadows/ww_sunshadow.hlsli"
#endif
#ifndef WW_FX_DECALLIT
#define WW_FX_AMBSHARE 1	// MODULES1 R8-1: a lit card's ambient keeps the share the GI volume leaves (cell_fxlaw.glsl)
#endif
#include "../lights/cell_lights.hlsli"
#ifndef WW_FX_DECALLIT

float3 fxLumpNormal( float3 N, float3 Nf, float3 pv, float a )
{
	if ( or( ( fxLumpRed & 1 ) != 0, ( hasNormalMap && ( fxLumpRed & 2 ) != 0 ) ) )
		return N;
	if ( hasNormalMap ) {	// the map's normal as a slope on the face, then the opacity's slope on the same face
		float3 n = N / max( dot( N, Nf ), 0.25 );
		N = Nf;
		float3 dpx = ddx( pv ), dpy = ddy( pv );
		float3 r1 = cross( dpy, N ), r2 = cross( N, dpx );
		float det = dot( dpx, r1 );
		float2 dux = ddx( texCoord.xy ), duy = ddy( texCoord.xy );
		float duv = abs( dux.x * duy.y - dux.y * duy.x );
		if ( or( abs( det ) < 1e-20, duv < 1e-20 ) )
			return normalize( n );
		float L = 0.5 * sqrt( abs( det ) / duv );
		return normalize( n - sign( det ) * L * ( ddx( a ) * r1 + ddy( a ) * r2 ) / abs( det ) );
	}
	float3 dpx = ddx( pv ), dpy = ddy( pv );
	float3 r1 = cross( dpy, N ), r2 = cross( N, dpx );
	float det = dot( dpx, r1 );
	float2 dux = ddx( texCoord.xy ), duy = ddy( texCoord.xy );
	float duv = abs( dux.x * duy.y - dux.y * duy.x );
	if ( or( abs( det ) < 1e-20, duv < 1e-20 ) )
		return N;
	float L = 0.5 * sqrt( abs( det ) / duv );	// half a UV unit in view units: the lump's height at alpha 1
	return normalize( abs( det ) * N - sign( det ) * L * ( ddx( a ) * r1 + ddy( a ) * r2 ) );
}
#define FX_LUMP_N( n, nf, pv, a ) fxLumpNormal( n, nf, pv, a )
// the opacity is the card's depth: the thickness its light reads is the opacity itself, not a ball over its UV
#define FX_PUFF_A( a, q ) ( ( fxLumpRed & 4 ) != 0 ? vpPuffAlpha( a, q ) : ( a ) )
#else
#define FX_LUMP_N( n, nf, pv, a ) ( n )
#define FX_PUFF_A( a, q ) vpPuffAlpha( a, q )
#endif
#ifdef WW_FX_DECALLIT
#include "../ao/cell_ao.hlsli"
#endif


Texture2D<float4> fxDepth : register(t51, space0); SamplerState _fxDepth_sampler : register(s51, space0);		// the opaque pass's depth, the viewport's corner at texel 0
// lane MODULES1 M3 (G7): the Effect Lighting law; cellFxEmitK / cellFxUnlitK live there
#include "../lights/cell_fxlaw.hlsli"
static float3 fxRoomShare = (float3)( 1.0 );	// probe 75

// one placed light on a lit effect at world point P, as the game's lit effect shader sums it
float3 cellFxLight( int i, float3 P )
{
	float4 t0 = cellLights.Load( i * CELL_TPL );	// lane FXLIT1: the stride is HEMI1's 8 texels a light
	float4 t1 = cellLights.Load( i * CELL_TPL + 1 );
	float3 Lv = t0.xyz - P;
	float d = length( Lv );
	float q = clamp( d / max( t0.w, 0.001 ), 0.0, 1.0 );
	float a = 1.0 - q * q;
	if ( ( fxLitMode & 4 ) == 0 )
		a = pow( a, 2.2 );
	if ( cellLightPhys.x > 0.5 && ( fxLitMode & 4 ) == 0 )	// lane LIGHTPHYS1: the surfaces' physical curve, so effects and air agree
		a = cellPhysFalloff( a, q, t0.w, float3( 0.0, 1.0, 2.0 ) );
	float3 g = (float3)( 1.0 );	// lane LIGHTS5 (GOBO-FX): the light's pattern, as on surfaces (GOBO2: every shape)
	float3 gAx = (float3)( 0.0 );
	if ( t1.w > -1.5 ) {	// a spot: the game's cone, its cosine + 0.001
		float4 t2 = cellLights.Load( i * CELL_TPL + 2 );
		float c = clamp( dot( -Lv / max( d, 0.001 ), t2.xyz ), 0.0, 1.0 );
		float base = clamp( 1.0 - ( 1.0 - c ) / max( 1.0 - ( t1.w + 0.001 ), 1e-4 ), 0.0, 1.0 );
		a *= min( pow( base, max( t2.w, 1e-3 ) ), 1.0 );
		gAx = t2.xyz;
	}
	if ( a > 0.0 )
		g = cellGobo( i, -Lv, gAx, t1.w > -1.5 );
	return t1.rgb * g * a;
}

// what a lit effect's colour is multiplied by at P: mix( 1, directional + its lights, lighting influence )
float3 cellFxLit( float3 P )
{
	// lane SUNCELL1 final: an exterior with no weather loaded has no sun to light it -- self-lit, as before
	// MODULES1 R7-1: a blended lit card outdoors takes the local light: the surfaces' sun under its shadow + the ambient
	bool loc = fxLocalOn() && !cellInterior && !fxAdditive;
	if ( or( ( fxLitMode & 8 ) != 0, ( !cellInterior && !cellHasDir && !loc ) ) )
		return (float3)( 1.0 );
	// R8-1: the ambient's share and the GI as a surface there (fxLocalSky, set by the caller; 1 and 0 before it)
	float3 E = loc ? vpSun.rgb * fxLocalVis + cellAmbient( float3( 0.0, 0.0, 1.0 ), P ) FX_LOCAL_GI : cellHasDir ? cellDirColor : (float3)( 0.0 );
	if ( ( fxLitMode & 2 ) != 0 ) {
		// flat on purpose: an effect's light has no hemisphere clip, which the cluster lists cull by
		for ( int i = 0; i < cellLightCount; i++ )
			E += cellFxLight( i, P );
	} else {
		for ( int k = 0; k < 4; k++ )
			if ( fxLit[k] > -0.5 )
				E += cellFxLight( int( fxLit[k] + 0.5 ), P ) * fxLitScale[k];
	}
	return fxLawMix( E, lightingInfluence, fxAdditive );	// MODULES1 M3: was mix( 1, E, lightingInfluence )
}

float cellFxFade()
{
	if ( or( !fxSoft, ( fxRed & 2 ) != 0 ) )
		return 1.0;
	float d = fxDepth.Load( int3(int2( gl_FragCoord.xy ) - viewportDimensions.xy, 0) ).r;
	// view depth from the projection: w = B / (A + z_ndc)
	float sceneW = projectionMatrix[3][2] / ( projectionMatrix[2][2] + ( 2.0 * d - 1.0 ) );
	float k = fxDistScale / fxSoftDepth;
	float fragW = 1.0 / gl_FragCoord.w;
	float soft = clamp( ( sceneW - fragW ) * k, 0.0, 1.0 );
	// the game subtracts its own near term here (INFERRED to be about one unit; read as none)
	float t = clamp( ( fragW * k - 0.075 ) * 2.352941, 0.0, 1.0 );
	return soft * t * t * ( 3.0 - 2.0 * t );
}

#ifdef WW_FX_DECALLIT
/* lane DECALLIT1 (fo4_effectdecal.frag, src/gl/celllights.h wwIsCellDecalProgramName): an effect shader with the Decal flag is
 * not an effect to the game: it goes to the decal groups the deferred prepass draws, so its colour lands in the
 * surface's albedo and the deferred lights, ambient, sky, obscurance and fog take it exactly as they take the wall
 * under it. Here: the colour is the albedo of fo4_default's cell path (sqrt of linear), lit by the same laws with no
 * specular of its own (the wall's shows through its alpha), then the same fog. */

float3 fxDecalLin( float3 albedo, float3 normal )
{
	float3 Pv = -ViewDir;
	float3 Vv = normalize( ViewDir );
	float3 Pw = cellWorldPos( Pv );
	float3 Nw = cellWorldDir( normal );
	float3 color;
#ifdef WW_CELL_GI_MARCH
	// MERGE1025: the sky share read once (each cellGiSkyK call inlines the whole march: three took this program past the
	// driver's instruction limit); the same call on the same point, so the same value
	float kSky = cellGiSky ? cellGiSkyK( Pw, Nw ) : 0.0;
#define WW_FX_DECAL_SKYK kSky
#else
#define WW_FX_DECAL_SKYK cellGiSkyK( Pw, Nw )
#endif
	if ( !cellInterior && cellHasDir ) {
		float3 Ax = A.rgb;
		if ( cellExtDalc )
			Ax = sqrt( cellAmbient( Nw, Pw ) );
		cellExtAmb = Ax * Ax;
		color = Ax * albedo;
		if ( cellGiSky && ( cellRed & 131072 ) == 0 ) {
			float k = WW_FX_DECAL_SKYK;
			color *= sqrt( 1.0 - k );
			cellExtAmb *= 1.0 - k;
		}
	} else {
		color = albedo * ( A.rgb + D.rgb * max( dot( normal, normalize( LightDir ) ), 0.0 ) );
		if ( cellGiSky && ( cellRed & 131072 ) == 0 )
			color -= ( 1.0 - sqrt( 1.0 - WW_FX_DECAL_SKYK ) ) * A.rgb * albedo;
		else if ( cellGiSky )
			color -= WW_FX_DECAL_SKYK * A.rgb * albedo;
	}
	cellExtSpec = (float3)( 0.0 );
	float3 lit = cellLit( color, albedo, normal, Pv, Vv, 0.0, (float3)( 0.0 ), 1.0, 0.0, (float3)( 0.0 ), (float3)( 0.0 ) );
	lit *= sqrt( cellAoFactor() );
	return lit * lit;
}

/* lane GIDARK1 (Concord Hardware's counter at noon): the game's lit effect outdoors takes the weather's Effect Lighting x
 * the sun scale as its base, with no shadow and no ambient (cellFxLit), so a glass lens deep inside a shop shone as it
 * would in the open street while the room around it read six times darker. Here that base is scaled by the share of
 * the open-air light that reaches the spot, taken the way a white surface there takes it (the same terms as cellLit's
 * exterior branch, Lambert only):
 *   ( sun x N.L x the cascades' shadow + the weather's ambient x ( 1 - k ) + the probe GI ) / ( sun x N.L + the weather's ambient )
 * Under open sky that is about 1 (the game's base); in a shaded room it follows the room: GI on or off, day or night.
 * The model's placed lights add as before. Red WW_CELL_FXROOM_RED=game draws the game's base (fo4_effectcell). */
float3 fxRoomLitMul( float3 Pv, float3 normal )
{
	if ( or( ( fxLitMode & 8 ) != 0, !cellHasDir ) )
		return (float3)( 1.0 );
	float3 Pw = cellWorldPos( Pv );
	float3 Nw = cellWorldDir( normal );
	float nl = max( dot( Nw, normalize( cellDirTo ) ), 0.0 );
	float3 amb = cellExtDalc ? cellAmbient( Nw, Pw ) : A.rgb * A.rgb;
	float k = cellGiSkyK( Pw, Nw );
	float3 gi = CELL_GI_UP ? cellGiE( Pw, Nw ) * 0.31830989 : (float3)( 0.0 );	// lane PRTOFF1: the PRT's grid or the ray-traced volume
	float3 open = cellDirColor * nl + amb;
	float3 here = cellDirColor * nl * cellDirVis + amb * ( 1.0 - k ) + gi;
	fxRoomShare = clamp( here / max( open, (float3)( 1e-5 ) ), (float3)( 0.0 ), (float3)( 4.0 ) );
	float3 E = cellFxBase * fxRoomShare;
	if ( ( fxLitMode & 2 ) != 0 ) {
		// flat on purpose: an effect's light has no hemisphere clip, which the cluster lists cull by
		for ( int i = 0; i < cellLightCount; i++ )
			E += cellFxLight( i, Pw );
	} else {
		for ( int j = 0; j < 4; j++ )
			if ( fxLit[j] > -0.5 )
				E += cellFxLight( int( fxLit[j] + 0.5 ), Pw ) * fxLitScale[j];
	}
	return fxLawMix( E, lightingInfluence, fxAdditive );	// MODULES1 M3: was mix( 1, E, lightingInfluence )
}
#endif

/* lane VOLPART1: a lit effect card (the street steam) as a piece of the air, as the volumetric particles
 * (cell_volpart.glsl): lit at its own point from the air's light list with its normal (the normal map, else the
 * card's own) and its alpha as thickness, seen through the integrated air, and soft: the card is a slab of
 * half-depth h round its plane (h = fxSoftDepth: its soft depth when the material sets one, else the row's card depth), and
 * its opacity is the share of the slab in front of the scene. It draws with no depth test (the fade is the
 * occlusion: a surface h in front of the card takes it all), so no hard line where a card meets the ground or a wall. */
#include "../air/cell_volpart.hlsli"



// the soft slab's inputs: (the scene's view depth, the card's, the half-depth h) in view units
float3 vpCardSlab()
{
	int2 px = vpDumpScale.x > 0.0 ? int2( gl_FragCoord.xy * vpDumpScale ) : int2( gl_FragCoord.xy ) - viewportDimensions.xy;
	float d = fxDepth.Load( int3(px, 0) ).r;
	float sceneW = projectionMatrix[3][2] / ( projectionMatrix[2][2] + ( 2.0 * d - 1.0 ) );
	return float3( sceneW, 1.0 / gl_FragCoord.w, max( fxSoftDepth, 1.0 ) / max( fxDistScale, 1e-6 ) );
}

float vpCardSoft()
{
	if ( or( !fxSoft, ( volPartRed & 4 ) != 0 ) )
		return 1.0;
	float3 s = vpCardSlab();
	return clamp( ( s.x - ( s.y - s.z ) ) / ( 2.0 * s.z ), 0.0, 1.0 );
}
#endif

static float3 ViewDir_norm = normalize( ViewDir );
static float3x3 btnMatrix_norm = float3x3( normalize( btnMatrix[0] ), normalize( btnMatrix[1] ), normalize( btnMatrix[2] ) );


#ifdef WW_FXRES
#define frag_main wwFxResMain
#endif
void frag_main()
{
#ifdef WW_CELLLIGHTS
	if ( cellOn && ( fxRed & 8 ) != 0 )
		discard;
#endif
	float2 offset = texCoord.xy * uvScale + uvOffset;

	float4 baseMap = BaseMap.Sample( _BaseMap_sampler, offset );
	float4 normalMap = NormalMap.Sample( _NormalMap_sampler, offset );
	float4 specMap = SpecularMap.Sample( _SpecularMap_sampler, offset );

	float3 normal = normalize(normalMap.rgb * 2.0 - 1.0);
	// Calculate missing blue channel
	normal.b = sqrt(max(1.0 - dot(normal.rg, normal.rg), 0.0));
	normal = normalize( mul(normal, btnMatrix_norm) );
	if ( !gl_FrontFacing )
		normal *= -1.0;

	float3 L = normalize(LightDir);
	float3 V = ViewDir_norm;
	float3 R = reflect(-V, normal);
	float3 H = normalize( L + V );

	float NdotL = max( dot(normal, L), 0.000001 );
	float NdotH = max( dot(normal, H), 0.000001 );
	float NdotV = max( dot(normal, V), 0.000001 );
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
		falloff = smoothstep( falloffParams.x, falloffParams.y, abs(dot(normal, V)) );
		falloff = lerp( max(falloffParams.z, 0.0), min(falloffParams.w, 1.0), falloff );

		if ( useFalloff )
			baseMap.a *= falloff;

		if ( hasRGBFalloff )
			baseMap.rgb *= falloff;
	}

	float alphaMult = baseColor.a * baseColor.a;

	float4 color = baseMap * C;
	color.rgb *= baseColor.rgb;
	color.a *= alphaMult;

#ifdef WW_CELLLIGHTS
	/* lane VOLPART1: a volumetric lit card is a fake volume (cell_volpart.glsl): its opacity from the path through a
	 * ball of its size (the puff, the thickness its light reads), x the edge-on fade at its face normal x the near fade */
	float4 vpVol = float4( 1.0, 1.0, 1.0, color.a );	// edge, near, the puff's opacity, a0
	float3 vpFaceN = float3( 0.0, 0.0, 1.0 );
	if ( cellOn && volPart != 0 ) {
		float3 pv = -ViewDir;
		vpFaceN = normalize( cross( ddx( pv ), ddy( pv ) ) );
		if ( dot( vpFaceN, ViewDir_norm ) < 0.0 )
			vpFaceN = -vpFaceN;
		vpVol.x = vpEdgeFade( abs( dot( vpFaceN, ViewDir_norm ) ) );
		vpVol.y = vpNearFade( -pv.z * fxDistScale );
		vpVol.z = FX_PUFF_A( color.a, clamp( texCoord.xy, 0.0, 1.0 ) * 2.0 - 1.0 );
		if ( !greyscaleAlpha )
			color.a = vpVol.z * vpVol.x * vpVol.y;
	}
	// lane EFX2: the Soft fades scale alpha, or a palette's row (the game's PS multiplies its palette coordinates)
	float fxFade = cellOn ? ( volPart != 0 ? vpCardSoft() : cellFxFade() ) : 1.0;
	if ( !greyscaleAlpha )
		color.a *= fxFade;
#define WW_FX_FADE * fxFade
#else
#define WW_FX_FADE
#endif
	if ( greyscaleColor )
		color.rgb = GreyscaleMap.SampleLevel( _GreyscaleMap_sampler, float2( BaseMap.Sample( _BaseMap_sampler, offset ).g, baseColor.r * C.r * falloff WW_FX_FADE ), 0.0 ).rgb;

	if ( greyscaleAlpha )
		color.a = GreyscaleMap.SampleLevel( _GreyscaleMap_sampler, float2( BaseMap.Sample( _BaseMap_sampler, offset ).a, color.a WW_FX_FADE ), 0.0 ).a;

	if ( alphaFlags > 0 ) {
		// 0: always, 1: <, 2: ==, 3: <=, 4: >, 5: !=, 6: >=, 7: never
		int	m = ( color.a < alphaThreshold ? 0x2B2B : ( color.a > alphaThreshold ? 0x7171 : 0x4D4D ) );
		if ( ( m & ( 1 << alphaFlags ) ) == 0 )
			discard;
	}
	if ( alphaFlags < 8 )
		color.a = 1.0;

	float3 diffuse = A.rgb + (D.rgb * NdotL);
#ifdef WW_CELLLIGHTS
	if ( !cellOn )	// lane EFX2: the cell view's emit colour is white, so the lighting influence changes nothing
#endif
	color.rgb = lerp( color.rgb, color.rgb * D.rgb, lightingInfluence );
#ifdef WW_CELLLIGHTS
	if ( cellOn && fxEmit.a > 0.0 )	// lane LIGHTS2 (XEMI1): the game's unlit effect, c = mix( c, c x emit, influence )
		color.rgb = lerp( color.rgb, color.rgb * fxEmit.rgb, lightingInfluence );
#endif

	// Specular
	float g = 1.0;
	float s = 1.0;
	if ( hasEnvMask ) {
		g = specMap.r;
		s = envMaskR ? specMap.r : specMap.g;
	}

	// Environment
	float4 cube = CubeMap.Sample( _CubeMap_sampler, reflectedWS );
	if ( hasCubeMap ) {
		cube.rgb *= envReflection * s;
		if ( !envNormalAlphaRed )
			cube.rgb *= normalMap.a;
#ifdef WW_CELLLIGHTS
		if ( !cellOn )
#endif
		cube.rgb = lerp( cube.rgb, cube.rgb * D.rgb, lightingInfluence );
#ifdef WW_CELLLIGHTS
		if ( cellOn && fxEmit.a > 0.0 )
			cube.rgb = lerp( cube.rgb, cube.rgb * fxEmit.rgb, lightingInfluence );
#endif

		color.rgb += cube.rgb * falloff;
	}

#ifdef WW_FX_DECALLIT
#if defined( WW_SUNSHADOW ) && defined( WW_CELL_GI_MARCH )
	/* lane SHADOWSHIMMER2: ONE wwSunShadow site in the GI-march mode. Its two branch copies below each inlined the whole
	 * cascade chain, and fo4_effectdecalcsm stopped linking in march + WW_CELL_PRT (too many instructions) after
	 * SHADOWSHIMMER1's world-locked kernel. Same call, same point: both branches below run only under cellOn and read
	 * cellDirVis after it. Other modes keep the two sites (their machine code unchanged). */
	if ( cellOn )
		cellDirVis = wwSunShadow( -ViewDir );
#define WW_FX_SUNVIS_ONCE 1
#endif
	if ( cellOn && fxRoomLit ) {	// lane GIDARK1: a lit effect outdoors, lit by the spot it stands in (fxRoomLitMul)
		float3 lin = ( fxRed & 4 ) != 0 ? max( color.rgb, (float3)( 0.0 ) ) : pow( max( color.rgb, (float3)( 0.0 ) ), (float3)( 2.2 ) );
		float3 Ng = normalize( btnMatrix[2] );	// the face the eye sees
		if ( !gl_FrontFacing )
			Ng = -Ng;
#if defined( WW_SUNSHADOW ) && !defined( WW_FX_SUNVIS_ONCE )
		cellDirVis = wwSunShadow( -ViewDir );
#endif
		float3 m = fxRoomLitMul( -ViewDir, Ng );
		if ( cellProbe >= 70 && cellProbe <= 75 ) {
			// FXLIT1's probes (70 the model, 71 / 72 the position, 73 / 74 the multiplier / 4) and 75: the room share / 4
			float3 P = cellWorldPos( -ViewDir );
			float3 v = cellProbe == 75 ? fxRoomShare : m;
			float3 q = cellProbe >= 73 ? floor( clamp( v * 0.25, 0.0, 1.0 ) * 65535.0 + 0.5 )
				: clamp( floor( P - cellCenter + 32768.0 ), 0.0, 65535.0 );
			bool high = or( or( cellProbe == 71, cellProbe == 73 ), cellProbe == 75 );
			fragColor = float4( cellProbe == 70 ? fxLitId : high ? floor( q / 256.0 ) / 255.0 : mod( q, 256.0 ) / 255.0, 1.0 );
			return;
		}
		lin *= m;
		// FOGFIX1 (bungo 10-16, the Concord truck's glass bright in the fog): this branch kept the game's fog alone while
		// every other effect path took the air; now the air as the surfaces take it (red fxfog: the game's fog alone)
		lin = wwFogFx( lin, -ViewDir, fxAdditive );
		if ( cellProbe == 6 )
			fragColor = float4( lin * cellProbeGain, color.a );
		else
			fragColor = float4( cellIsLinear ? lin : cellIsOn ? cellImageSpace( sqrt( lin ) ) : pow( lin, (float3)( 1.0 / 2.2 ) ), color.a );
		return;
	}
	if ( cellOn ) {
		float3 alb = max( color.rgb, (float3)( 0.0 ) );
		if ( fxDecalProbe ) {
			if ( color.a < 0.1 )
				discard;
			alb = (float3)( 1.0 );
		}
		float3 Ng = normalize( btnMatrix[2] );	// the decal's plane: the surface it lies on
		if ( !gl_FrontFacing )
			Ng = -Ng;
#if defined( WW_SUNSHADOW ) && !defined( WW_FX_SUNVIS_ONCE )
		cellDirVis = wwSunShadow( -ViewDir );
#endif
		float3 lin = fxDecalLin( alb, Ng );
		if ( or( fogOn, wwApOn() ) )	// lane SKYA: the sky's aerial perspective with the fog off too
			lin = max( wwFog( lin, -ViewDir ), (float3)( 0.0 ) );
		float a = fxDecalProbe ? 1.0 : color.a;
		if ( cellProbe == 6 )
			fragColor = float4( lin * cellProbeGain, a );
		else
			fragColor = float4( cellIsLinear ? lin : cellIsOn ? cellImageSpace( sqrt( lin ) ) : pow( lin, (float3)( 1.0 / 2.2 ) ), a );
		return;
	}
#endif
/* lane SHADOWSHIMMER4 (EFFECTDECALHEADROOM): in a lit-decal program every cellOn path above returns or discards (the
 * fxRoomLit branch and the decal branch), so with cellOn a uniform the block below never runs there; the NV compiler kept
 * it all the same (its cellFxLit / vpLit lights and its wwFogFx: fo4_effectdecalcsm in march + WW_CELL_PRT 65482 -> 35670
 * fragment instructions, the driver's limit 65536). Dropped in the GI-march mode only; the other modes keep their source
 * (their machine code unchanged). */
#if defined( WW_CELLLIGHTS ) && !( defined( WW_FX_DECALLIT ) && defined( WW_CELL_GI_MARCH ) )
	if ( cellOn && fxDecalProbe ) {
		if ( color.a < 0.1 )
			discard;
		color = (float4)( 1.0 );
	}
	if ( cellOn ) {
		float3 lin = ( fxRed & 4 ) != 0 ? max( color.rgb, (float3)( 0.0 ) ) : pow( max( color.rgb, (float3)( 0.0 ) ), (float3)( 2.2 ) );
		if ( fxLitMode != 0 ) {	// lane FXLIT1: a lit effect, in linear light, before the fog
			float3 P = cellWorldPos( -ViewDir );
#ifndef WW_FX_DECALLIT
			if ( volPart == 0 && volCsm && fxLocalOn() && !cellInterior && !fxAdditive )
				fxLocalVis = wwSunShadow( -ViewDir );	// R7-1: the plain lit card's sun, shadowed as the puff's
			fxLocalSky( P, fxAdditive );	// R8-1: the sky light a surface takes here
#endif
			float3 m = cellFxLit( P );
			if ( cellProbe >= 70 && cellProbe <= 74 ) {
				// its probes, opaque: 70 the model, 71 / 72 the position (as probes 2 / 3), 73 / 74 the multiplier / 4
				float3 q = cellProbe >= 73 ? floor( clamp( m * 0.25, 0.0, 1.0 ) * 65535.0 + 0.5 )
					: clamp( floor( P - cellCenter + 32768.0 ), 0.0, 65535.0 );
				bool high = or( cellProbe == 71, cellProbe == 73 );
				fragColor = float4( cellProbe == 70 ? fxLitId : high ? floor( q / 256.0 ) / 255.0 : mod( q, 256.0 ) / 255.0, 1.0 );
				return;
			}
			if ( volPart == 0 )
				lin *= m;
		}
		if ( volPart != 0 ) {
			// lane VOLPART1: lit at its own point, seen through the integrated air (cell_volpart.glsl)
			float3 posView = -ViewDir;
			float aThick = greyscaleAlpha ? color.a : vpVol.z;	// the puff's opacity is its thickness; the fades are coverage
			if ( volPartDump >= 100 ) {	// gates only: probe slot volPartDump - 100 at every pixel, raw
				int k = volPartDump - 100;
				float3 dW = cellWorldDir( posView ), Nw = cellWorldDir( vpFaceN );
				if ( k == 8 )	// the card volume: |N.V| at the face, edge fade, near fade, depth (game units)
					fragColor = float4( abs( dot( vpFaceN, ViewDir_norm ) ), vpVol.x, vpVol.y, -posView.z * fxDistScale );
				else if ( k == 9 )	// the puff: q, a0, its opacity
					fragColor = float4( clamp( texCoord.xy, 0.0, 1.0 ) * 2.0 - 1.0, vpVol.w, vpVol.z );
				else if ( k == 10 )	// one white light at the eye's side (to = -dW), its N.to in w
					fragColor = float4( vpOneLight( dW, Nw, -dW, aThick ), dot( Nw, -dW ) );
				else if ( k == 11 )	// the same light behind the card (to = dW); w: the coverage drawn (alpha)
					fragColor = float4( vpOneLight( dW, Nw, dW, aThick ), color.a );
				else if ( k == 12 )	// the soft slab: scene depth, card depth, h (game units), the fade drawn
					fragColor = float4( vpCardSlab() * fxDistScale, fxFade );
				else
					fragColor = vpProbe( k, posView, aThick );
				return;
			}
#ifdef WW_FX_AMBSHARE
			if ( fxAirCard != 0 && !fxAdditive && ( fxLocalRed & 4 ) == 0 ) {	// round 9: a slab of air (vpLit's thin share alone)
#ifndef WW_FX_DECALLIT
				vpAirCardA = ( fxLocalRed & 8 ) == 0 ? clamp( aThick, 0.0, 1.0 ) : 0.0;	// FARWATER1 late item 4: lamps' multiple scatter
#endif
				aThick = 0.0;
				fxLocalGi = (float3)( 0.0 );
			}
#endif
			if ( fxLitMode != 0 )
				lin *= fxAdditive ? cellFxLit( cellWorldPos( posView ) )
					: vpLit( posView, FX_LUMP_N( dot( normal, V ) < 0.0 ? -normal : normal, vpFaceN, posView, clamp( vpVol.w, 0.0, 1.0 ) ), aThick )
						FX_LOCAL_GI_LAW( lightingInfluence );
		}
		if ( fxLitMode == 0 ) {	// lane PHYSUNITS1: unlit effects in real units
			float uk = fxAdditive ? cellFxEmitK : cellFxUnlitK;
			if ( uk > 0.0 )
				lin *= uk;
		}
		// the landed air (wwFogFx, as the surfaces take it); VOLPART1 red nofog: the game's fog alone
		if ( volPart != 0 && ( volPartRed & 32 ) != 0 ) {
			if ( fogOn ) {
				float hb;
				float3 fc;
				float f = wwFogEval( length( ViewDir ) * fogDistScale, dot( fogView.xyz, -ViewDir ) + fogView.w, hb, fc );
				lin = fxAdditive ? lin * ( 1.0 - f ) : lerp( lin, fc, f );
			}
		} else
		lin = wwFogFx( lin, -ViewDir, fxAdditive );	// lane VOLAIR1: the air, as the surfaces take it (red fxfog: the game's fog alone)
		/* lane MODULES1 item 2 (bungo 11-09, black shards on the ground in daylight): in real units the lit base is
		 * ~25000, so a card brighter than ~2.6 overflows the RGBA16F frame to inf and TAA/bloom turn it into NaN = black.
		 * The light is kept under half the half-float range and a non-finite value draws nothing. Red WW_CELL_FX_RED=unbound. */
		if ( ( fxRed & 32 ) == 0 ) {
			if ( or( or( any( isnan( lin ) ), any( isinf( lin ) ) ), isnan( color.a ) ) ) {
				lin = (float3)( 0.0 );
				color.a = 0.0;
			}
			lin = min( lin, (float3)( 32768.0 ) );
		}
		// probe 6 (the imagespace's measure): the linear value, blended as drawn
		if ( cellProbe == 6 )
			fragColor = float4( lin * cellProbeGain, color.a );
		else
			fragColor = float4( cellIsLinear ? lin	// lane HDR1: linear, summed in the HDR frame, tone-mapped once
				: cellIsOn ? cellImageSpace( sqrt( lin ) ) : pow( lin, (float3)( 1.0 / 2.2 ) ), color.a );
		return;
	}
#endif
	fragColor = float4( color.rgb * sqrt(D.a), color.a );
}

#ifdef WW_FXRES
#undef frag_main
void frag_main()
{
	wwFxResMain();
	fxResT = float4( 0.0, 0.0, 0.0, fragColor.a );
	fxResCov = (float4)( 1.0 );
}
#endif
// end of fo4_effectshader.frag

struct SPIRV_Cross_Input
{
    WW_LOC(3) float3 LightDir : TEXCOORD3;
    WW_LOC(4) float3 ViewDir : TEXCOORD4;
    WW_LOC(8) float2 texCoord : TEXCOORD8;
    WW_LOC(0) nointerpolation float4 A : TEXCOORD0;
    WW_LOC(1) float4 C : TEXCOORD1;
    WW_LOC(2) nointerpolation float4 D : TEXCOORD2;
    WW_LOC(5) float3x3 btnMatrix : TEXCOORD5;
    WW_LOC(9) nointerpolation float3x3 reflMatrix : TEXCOORD9;
    float4 gl_FragCoord : SV_Position;
    bool gl_FrontFacing : SV_IsFrontFace;
};

struct SPIRV_Cross_Output
{
#if defined(WW_FXRES)
    float4 fragColor : SV_Target0;
    float4 fxResT : SV_Target1;
    float4 fxResCov : SV_Target2;
#endif
#if !(defined(WW_FXRES))
    float4 fragColor : SV_Target0;
#endif
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    LightDir = stage_input.LightDir;
    ViewDir = stage_input.ViewDir;
    texCoord = stage_input.texCoord;
    A = stage_input.A;
    C = stage_input.C;
    D = stage_input.D;
    btnMatrix = stage_input.btnMatrix;
    reflMatrix = stage_input.reflMatrix;
    gl_FrontFacing = stage_input.gl_FrontFacing;
    frag_main();
    SPIRV_Cross_Output stage_output;
#if defined(WW_FXRES)
    stage_output.fragColor = fragColor;
    stage_output.fxResT = fxResT;
    stage_output.fxResCov = fxResCov;
#endif
#if !(defined(WW_FXRES))
    stage_output.fragColor = fragColor;
#endif
    return stage_output;
}
