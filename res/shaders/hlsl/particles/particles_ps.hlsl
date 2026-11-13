// HLSL source of features/particles/particles.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_AIRZONE, WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "ParticlesBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// inlined (no readable .hlsli yet): uniforms.glsl
static float4 gl_FragCoord;
// Particle fragment shader.
//
// Base behaviour (the known-good path): sample the sprite texture and multiply
// by the per-particle vertex colour. The alpha channel is the sprite's MASK and
// is preserved exactly (base.a * C.a) - combined with the NiAlphaProperty blend
// this is what keeps the transparent surround empty.
//
// When the linked BSEffectShaderProperty (.bgem) supplies them, the visible
// body of the sprite additionally gets the effect-shader features flat
// billboards otherwise miss in preview:
//   - normal-map driven view-angle falloff (soft edges / apparent volume)
//   - greyscale-to-palette colour gradient
//   - environment cube-map reflection (fake shine / depth)
// CRITICAL: every one of those only ever RECOLOURS / adds light to the sprite
// body, gated by the base coverage, and never raises the alpha mask. Otherwise
// (palette index maps + additive blend) they light up the void into dark
// squares - the regression this shader is careful to avoid.

struct Texture {
	float2 uvCenter;
	float2 uvScale;
	float2 uvOffset;
	float uvRotation;
	int coordSet;
	int textureUnit;
};

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
#ifdef WW_CELLLIGHTS
/* lane SUNCELL1 (particles_cell.frag): the game draws its particles with the effect shader, so in the cell view a
 * particle takes the cell effect's output law (fo4_effectshader.frag, lane EFX2): the colour decoded to linear,
 * fogged (an additive one dimmed, a blended one fogged toward the fog colour), then linear into the HDR frame,
 * or the cell's imagespace, or encoded again. */
#include "../air/lookdev_fog.hlsli"
#define WW_FX_AMBSHARE 1	// MODULES1 R8-1: a lit particle's ambient keeps the share the GI volume leaves (cell_fxlaw.glsl)
#include "../lights/cell_lights.hlsli"

// lane MODULES1 M3 (G7): the Effect Lighting law and the unlit unit
#include "../lights/cell_fxlaw.hlsli"


float3 cellFxLight( int i, float3 P )
{
	float4 t0 = cellLights.Load( i * CELL_TPL );
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

float3 cellFxLit( float3 P )
{
	// lane SUNCELL1 final: an exterior with no weather loaded has no sun to light it -- self-lit, as before
	// MODULES1 R7-1: a blended lit particle outdoors takes the local light: the surfaces' sun + the ambient at it
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
#endif

Texture2D<float4> textureUnits[10] : register(t46, space0); SamplerState _textureUnits_sampler[10] : register(s46, space0);
#define WW_LOOSE_TYPES 1
#if defined(WW_CELLLIGHTS)
#include "../air/lookdev_fog.hlsli"
#if defined(WW_CELLLIGHTS)
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
#if defined(WW_CELLLIGHTS)
#if defined(WW_CELLLIGHTS)
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
#if defined(WW_CELLLIGHTS)
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
#if defined(WW_CELLLIGHTS)
#if defined(WW_CELLLIGHTS)
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
#if defined(WW_CELLLIGHTS)
	bool fxAdditive;
	/* lane SUNCELL1 (last round): A LIT PARTICLE. The game's effect technique sets Lit from the property's lighting
	 * flag alone, beside Ptcl (checked in Todd's treat), so a particle whose
	 * effect shader is lit takes the same placed lights as a lit effect card (lane FXLIT1, fo4_effectshader.frag):
	 * mix( 1, directional + the model's four lights, lighting influence ), in linear light, before the fog.
	 * glparticles.cpp sets these from src/features/lights/cellfxlit.h; fxLitMode 0 = unlit, as before. */
	float lightingInfluence;
	float4 fxEmit;			// lane LIGHTS3 (XEMI-PARTICLES): the placement's emittance colour ^2.2 on an unlit system; w 0 = none
	int fxLitMode;			// 0 unlit; 1 lit; red controls: 2 every light, 4 no 2.2 on the falloff, 8 self-lit
	float4 fxLit;				// indices into cellLights, -1 = none
	float4 fxLitScale;		// 1 (a red control's per-light ratio)
	/* lane SUNCELL1 final: the game's particle shaders raise the WHOLE vertex colour to 2.2, alpha too (log / mul 2.2 /
	 * exp on v2.xyzw, asm 00613 / 00604 / 00903); true = the red WW_CELL_PARTICLES_RED=linalpha (alpha as written) */
	bool pfxAlphaRed;
#endif
	Texture textures[10];
	bool hasNormalMap;
	bool hasGreyscaleMap;
	bool hasCubeMap;
	bool greyscaleColor;
	bool greyscaleAlpha;
	bool useFalloff;
	bool hasRGBFalloff;
	float4 falloffParams;			// x,y = start/stop angle, z,w = start/stop opacity
	float4 glowColor;				// BGEM emissive colour (defaults to white)
	float glowMult;				// BGEM emissive multiple (defaults to 1)
	float envReflection;
	/* lane FXPARITY1: Particle Game Rules (src/features/particles/fxparity.h); 0 = as before. Bit 1: the effect card's law for a
	 * particle (material alpha squared, no falloff: the game sets Fo only off Ptcl, the palette row as the colour). Bit 2: its soft fade */
	int pfxGame;
#if defined(WW_CELLLIGHTS)
	bool fxSoft;			// the opaque pass's depth is bound
	// lane FXPARITY1: a Soft particle's fade near the surface behind it and near the eye (fo4_effectshader.frag cellFxFade)
	float fxSoftDepth;		// game units
	float fxDistScale;		// view units -> game units
#endif
};


Texture2D<float4> NormalMap : register(t56, space0); SamplerState _NormalMap_sampler : register(s56, space0);
Texture2D<float4> GreyscaleMap : register(t57, space0); SamplerState _GreyscaleMap_sampler : register(s57, space0);
TextureCube<float4> CubeMap : register(t58, space0); SamplerState _CubeMap_sampler : register(s58, space0);

static float3 ViewDir;

static float2 texCoords[9];
static float4 C;

static float4 fragColor;

#ifdef WW_CELLLIGHTS
/* lane VOLPART1: VOLUMETRIC PARTICLES -- the law is cell_volpart.glsl (shared with the effect cards). Here the
 * sprite's own parts: its normal (the normal map turned by the sprite's rotation, else a sphere) and the soft ball. */
#include "../air/cell_volpart.hlsli"

Texture2D<float4> fxDepth : register(t59, space0); SamplerState _fxDepth_sampler : register(s59, space0);		// the opaque pass's depth, the viewport's corner at texel 0

static float3 pPos;
static float2 pQ;
static float4 pSprite;

// the puff's normal (view space): the normal map turned by the sprite's rotation, else a sphere over the sprite
float3 vpNormal( float2 uvN )
{
	float2 T = pSprite.xy, B = float2( -pSprite.y, pSprite.x );
	float3 n;
	if ( hasNormalMap ) {
		n = NormalMap.Sample( _NormalMap_sampler, uvN ).rgb * 2.0 - 1.0;
		n.z = sqrt( max( 1.0 - dot( n.xy, n.xy ), 0.0 ) );
	} else {
		float r2 = dot( pQ, pQ );
		n = r2 < 1.0 ? float3( pQ, sqrt( 1.0 - r2 ) ) : float3( pQ * rsqrt( r2 ), 0.0 );
	}
	return normalize( float3( n.x * T + n.y * B, n.z ) );
}

// the share of the puff's ball in front of the scene (1 = all of it)
float vpSoft()
{
	if ( or( !fxSoft, ( volPartRed & 4 ) != 0 ) )
		return 1.0;
	float dz = fxDepth.Load( int3(int2( gl_FragCoord.xy ) - viewportDimensions.xy, 0) ).r;
	float sceneW = projectionMatrix[3][2] / ( projectionMatrix[2][2] + ( 2.0 * dz - 1.0 ) );
	float h = pSprite.z * max( sqrt( max( 1.0 - dot( pQ, pQ ), 0.0 ) ), 0.2 );
	return clamp( ( sceneW - ( -pPos.z - h ) ) / ( 2.0 * h ), 0.0, 1.0 );
}
float pfxSoftFade()
{
	if ( or( or( ( pfxGame & 2 ) == 0, !fxSoft ), volPart != 0 ) )
		return 1.0;
	float d = fxDepth.Load( int3(int2( gl_FragCoord.xy ) - viewportDimensions.xy, 0) ).r;
	float sceneW = projectionMatrix[3][2] / ( projectionMatrix[2][2] + ( 2.0 * d - 1.0 ) );
	float k = fxDistScale / max( fxSoftDepth, 1.0 );
	float fragW = 1.0 / gl_FragCoord.w;
	float soft = clamp( ( sceneW - fragW ) * k, 0.0, 1.0 );
	float t = clamp( ( fragW * k - 0.075 ) * 2.352941, 0.0, 1.0 );
	return soft * t * t * ( 3.0 - 2.0 * t );
}

// gates only (volPartDump): this particle's 8 probe pixels at its centre (cell_volpart.glsl vpProbe)
float4 vpDump( float a )
{
	return vpProbe( int( gl_FragCoord.x ) - int( pSprite.w + 0.5 ) * 8, pPos, a );
}
#endif

float4 getTexture( int n )
{
	float	r_c = cos( textures[n].uvRotation );
	float	r_s = sin( textures[n].uvRotation ) * -1.0;
	float2	offs = texCoords[textures[n].coordSet].xy - textures[n].uvCenter;
	offs = float2( offs.x * r_c - offs.y * r_s, offs.x * r_s + offs.y * r_c );
	offs = offs * textures[n].uvScale + textures[n].uvCenter + textures[n].uvOffset;

	return textureUnits[textures[n].textureUnit - 1].Sample( _textureUnits_sampler[textures[n].textureUnit - 1], offs );
}

void frag_main()
{
	float2	uv = texCoords[0].xy;

	float4	baseMap = getTexture( 0 );
	float4	baseMap0 = baseMap;	// lane FXPARITY1: before the falloff
	// palette lookups index the untouched base green channel
	float	baseG = baseMap.g;

	// sprite coverage: how "present" this texel is. Alpha for masked sprites,
	// luminance for additively-keyed glow sprites (black == empty). Effect
	// recolour / reflection is multiplied by this so it can only ever appear on
	// the sprite body, never in the surround.
	float	baseLum = max( baseMap.r, max( baseMap.g, baseMap.b ) );
	float	coverage = clamp( max( baseMap.a, baseLum ), 0.0, 1.0 );

	// billboard: view-space tangent frame is identity, so the sampled
	// tangent-space normal is already the view-space normal
	float3	V = normalize( ViewDir );
	float3	normal = float3( 0.0, 0.0, 1.0 );
	if ( hasNormalMap ) {
		float3 n = NormalMap.Sample( _NormalMap_sampler, uv ).rgb * 2.0 - 1.0;
		n.b = sqrt( max( 1.0 - dot( n.rg, n.rg ), 0.0 ) );
		normal = normalize( n );
	}

	// recolour from the palette only when it actually resolved and was bound
	bool	doGreyColor = greyscaleColor && hasGreyscaleMap;

	float4	baseColor = glowColor;
	if ( !doGreyColor )
		baseColor.rgb *= glowMult;

	// view-angle falloff: soft edges / apparent volume. Only ever REDUCES the
	// mask (alpha) or dims the body (rgb) - it can never make a sprite opaque.
	float	falloff = 1.0;
	if ( or( useFalloff, hasRGBFalloff ) ) {
		falloff = smoothstep( falloffParams.x, falloffParams.y, abs( dot( normal, V ) ) );
		falloff = lerp( max( falloffParams.z, 0.0 ), min( falloffParams.w, 1.0 ), falloff );

		if ( useFalloff )
			baseMap.a *= falloff;

		if ( hasRGBFalloff )
			baseMap.rgb *= falloff;
	}

	// the working alpha mask: base texture alpha * vertex-colour alpha (with the
	// optional soft-edge falloff already folded into baseMap.a). Nothing below
	// is allowed to raise it.
	float4	color = baseMap * C;
	color.rgb *= baseColor.rgb;

	// greyscale-to-palette colour gradient (rgb only), gated by coverage so the
	// palette-indexed surround stays dark instead of painting the void
	if ( doGreyColor ) {
		float3 pal = GreyscaleMap.SampleLevel( _GreyscaleMap_sampler, float2( baseG, baseColor.r * C.r * falloff ), 0.0 ).rgb;
		color.rgb = pal * coverage * C.rgb;
	}

	// environment reflection, also masked by coverage so it shines on the sprite
	// body only
	if ( hasCubeMap ) {
		float3 refl = mul(reflect( -V, normal ), envMapRotation);
		float3 cube = CubeMap.Sample( _CubeMap_sampler, refl ).rgb;
		color.rgb += cube * envReflection * falloff * coverage;
	}

	if ( ( pfxGame & 1 ) != 0 ) {
		// lane FXPARITY1: the game's effect law for a particle, replacing the viewer's above
		float ca = C.a;
#ifdef WW_CELLLIGHTS
		if ( cellOn && !pfxAlphaRed )
			ca = pow( max( C.a, 0.0 ), 2.2 );	// lane SUNCELL1's vertex alpha, kept
		float fade = cellOn ? pfxSoftFade() : 1.0;
#else
		float fade = 1.0;
#endif
		float4 bc = glowColor;
		if ( !greyscaleColor )
			bc.rgb *= glowMult;
		color = float4( baseMap0.rgb * C.rgb * bc.rgb, ( greyscaleAlpha ? 1.0 : baseMap0.a ) * ca * bc.a * bc.a );
		if ( !greyscaleAlpha )
			color.a *= fade;
		if ( doGreyColor )
			color.rgb = GreyscaleMap.SampleLevel( _GreyscaleMap_sampler, float2( baseG, bc.r * C.r * fade ), 0.0 ).rgb;
		if ( greyscaleAlpha && hasGreyscaleMap )
			color.a = GreyscaleMap.SampleLevel( _GreyscaleMap_sampler, float2( baseMap0.a, color.a * fade ), 0.0 ).a;
		if ( hasCubeMap )
			color.rgb += CubeMap.Sample( _CubeMap_sampler, mul(reflect( -V, normal ), envMapRotation) ).rgb * envReflection;
	}

#ifdef WW_CELLLIGHTS
	if ( cellOn ) {
		if ( cellProbe != 0 && cellProbe != 6 )
			discard;	// the probe passes read surfaces; a particle is none
		if ( !pfxAlphaRed && ( pfxGame & 1 ) == 0 )
			color.a = baseMap.a * pow( max( C.a, 0.0 ), 2.2 );	// texture alpha (falloff folded in) x vertex alpha^2.2
		if ( ( pfxGame & 1 ) == 0 )
			color.a *= pfxSoftFade();	// lane FXPARITY1: red law keeps the soft fade alone (1 while off)
		if ( fxEmit.a > 0.0 && fxLitMode == 0 )	// lane LIGHTS3 (XEMI-PARTICLES): the unlit effect's emittance, as fo4_effectshader
			color.rgb = lerp( color.rgb, color.rgb * fxEmit.rgb, lightingInfluence );
		float3 lin = pow( max( color.rgb, (float3)( 0.0 ) ), (float3)( 2.2 ) );
		if ( fxLitMode == 0 )	// MODULES1 M3: an unlit system on the unlit effects' unit, as an unlit card (fo4_effectshader)
			lin *= fxLawUnit( fxAdditive );
		if ( volPart != 0 ) {
			// lane VOLPART1: lit at its own point, seen through the integrated air, soft against the scene
			if ( volPartDump > 0 ) {
				fragColor = vpDump( color.a );
				return;
			}
			if ( fxLitMode != 0 ) {
				fxLocalSky( cellWorldPos( pPos ), fxAdditive );	// R8-1: the sky light a surface takes at the particle
				lin *= fxAdditive ? cellFxLit( cellWorldPos( -ViewDir ) ) : vpLit( pPos, vpNormal( uv ), color.a ) FX_LOCAL_GI_LAW( lightingInfluence );
			}
#if defined( WW_CELL_AIRZONE ) && !defined( WW_FX_DECALLIT )
			color.a = azPartAlpha( color.a );	// lane AIRZONE1: the volume's particles density (no zone read: untouched)
#endif
			color.a *= vpSoft();
			// the landed air at the pixel's own point (wwFogFx: c T + L, additive c T); red nofog: the game's fog alone
			if ( ( volPartRed & 32 ) == 0 ) {
				lin = wwFogFx( lin, pPos, fxAdditive );
			} else if ( fogOn ) {
				float hb;
				float3 fc;
				float f = wwFogEval( length( pPos ) * fogDistScale, dot( fogView.xyz, pPos ) + fogView.w, hb, fc );
				lin = fxAdditive ? lin * ( 1.0 - f ) : lerp( lin, fc, f );
			}
		} else {
		if ( fxLitMode != 0 ) {
			fxLocalSky( cellWorldPos( -ViewDir ), fxAdditive );	// R8-1
			lin *= cellFxLit( cellWorldPos( -ViewDir ) );	// a lit particle, before the fog
		}
		lin = wwFogFx( lin, -ViewDir, fxAdditive );	// lane VOLAIR1: the air, as the surfaces take it (red fxfog: the game's fog alone)
		}
		// lane MODULES1 item 2: kept finite and inside the RGBA16F frame, as fo4_effectshader (pfxGame 4: red unbound)
		if ( ( pfxGame & 4 ) == 0 ) {
			if ( or( or( any( isnan( lin ) ), any( isinf( lin ) ) ), isnan( color.a ) ) ) {
				lin = (float3)( 0.0 );
				color.a = 0.0;
			}
			lin = min( lin, (float3)( 32768.0 ) );
		}
		if ( cellProbe == 6 )
			fragColor = float4( lin, color.a );	// the imagespace's measure: the linear value, blended as drawn
		else
			fragColor = float4( cellIsLinear ? lin : cellIsOn ? cellImageSpace( sqrt( lin ) ) : pow( lin, (float3)( 1.0 / 2.2 ) ), color.a );
		return;
	}
#endif
	fragColor = color;
}

struct SPIRV_Cross_Input
{
    WW_LOC(4) float3 ViewDir : TEXCOORD4;
    WW_LOC(6) float2 texCoords[9] : TEXCOORD6;
    WW_LOC(1) float4 C : TEXCOORD1;
    float4 gl_FragCoord : SV_Position;
#if defined(WW_CELLLIGHTS)
    WW_LOC(15) float3 pPos : TEXCOORD15;
    WW_LOC(16) float2 pQ : TEXCOORD16;
    WW_LOC(17) nointerpolation float4 pSprite : TEXCOORD17;
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
    ViewDir = stage_input.ViewDir;
    texCoords = stage_input.texCoords;
    C = stage_input.C;
#if defined(WW_CELLLIGHTS)
    pPos = stage_input.pPos;
    pQ = stage_input.pQ;
    pSprite = stage_input.pSprite;
#endif
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
