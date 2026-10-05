#version 410 core

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
	vec2 uvCenter;
	vec2 uvScale;
	vec2 uvOffset;
	float uvRotation;
	int coordSet;
	int textureUnit;
};

#include "uniforms.glsl"
#ifdef WW_CELLLIGHTS
/* lane SUNCELL1 (particles_cell.frag): the game draws its particles with the effect shader, so in the cell view a
 * particle takes the cell effect's output law (fo4_effectshader.frag, lane EFX2): the colour decoded to linear,
 * fogged (an additive one dimmed, a blended one fogged toward the fog colour), then linear into the HDR frame,
 * or the cell's imagespace, or encoded again. */
#include "lookdev_fog.glsl"
#include "cell_lights.glsl"
uniform bool fxAdditive;
/* lane SUNCELL1 (last round): A LIT PARTICLE. The game's effect technique sets Lit from the property's lighting
 * flag alone, beside Ptcl (Todd's treat: BSEffectShaderProperty::DetermineTechniqueID), so a particle whose
 * effect shader is lit takes the same placed lights as a lit effect card (lane FXLIT1, fo4_effectshader.frag):
 * mix( 1, directional + the model's four lights, lighting influence ), in linear light, before the fog.
 * glparticles.cpp sets these from src/gl/cellfxlit.h; fxLitMode 0 = unlit, as before. */
uniform float lightingInfluence;
uniform int fxLitMode;			// 0 unlit; 1 lit; red controls: 2 every light, 4 no 2.2 on the falloff, 8 self-lit
uniform vec4 fxLit;				// indices into cellLights, -1 = none
uniform vec4 fxLitScale;		// 1 (a red control's per-light ratio)

vec3 cellFxLight( int i, vec3 P )
{
	vec4 t0 = texelFetch( cellLights, i * CELL_TPL );
	vec4 t1 = texelFetch( cellLights, i * CELL_TPL + 1 );
	vec3 Lv = t0.xyz - P;
	float d = length( Lv );
	float q = clamp( d / max( t0.w, 0.001 ), 0.0, 1.0 );
	float a = 1.0 - q * q;
	if ( ( fxLitMode & 4 ) == 0 )
		a = pow( a, 2.2 );
	if ( t1.w > -1.5 ) {	// a spot: the game's cone, its cosine + 0.001
		vec4 t2 = texelFetch( cellLights, i * CELL_TPL + 2 );
		float c = clamp( dot( -Lv / max( d, 0.001 ), t2.xyz ), 0.0, 1.0 );
		float base = clamp( 1.0 - ( 1.0 - c ) / max( 1.0 - ( t1.w + 0.001 ), 1e-4 ), 0.0, 1.0 );
		a *= min( pow( base, max( t2.w, 1e-3 ) ), 1.0 );
	}
	return t1.rgb * a;
}

vec3 cellFxLit( vec3 P )
{
	if ( ( fxLitMode & 8 ) != 0 )
		return vec3( 1.0 );
	vec3 E = cellHasDir ? cellDirColor : vec3( 0.0 );
	if ( ( fxLitMode & 2 ) != 0 ) {
		for ( int i = 0; i < cellLightCount; i++ )
			E += cellFxLight( i, P );
	} else {
		for ( int k = 0; k < 4; k++ )
			if ( fxLit[k] > -0.5 )
				E += cellFxLight( int( fxLit[k] + 0.5 ), P ) * fxLitScale[k];
	}
	return mix( vec3( 1.0 ), E, lightingInfluence );
}
#endif

uniform sampler2D textureUnits[10];
uniform Texture textures[10];

uniform sampler2D NormalMap;
uniform sampler2D GreyscaleMap;
uniform samplerCube CubeMap;

uniform bool hasNormalMap;
uniform bool hasGreyscaleMap;
uniform bool hasCubeMap;

uniform bool greyscaleColor;
uniform bool greyscaleAlpha;
uniform bool useFalloff;
uniform bool hasRGBFalloff;

uniform vec4 falloffParams;			// x,y = start/stop angle, z,w = start/stop opacity
uniform vec4 glowColor;				// BGEM emissive colour (defaults to white)
uniform float glowMult;				// BGEM emissive multiple (defaults to 1)
uniform float envReflection;

in vec3 ViewDir;

in vec2 texCoords[9];
in vec4 C;

out vec4 fragColor;

vec4 getTexture( int n )
{
	float	r_c = cos( textures[n].uvRotation );
	float	r_s = sin( textures[n].uvRotation ) * -1.0;
	vec2	offs = texCoords[textures[n].coordSet].st - textures[n].uvCenter;
	offs = vec2( offs.x * r_c - offs.y * r_s, offs.x * r_s + offs.y * r_c );
	offs = offs * textures[n].uvScale + textures[n].uvCenter + textures[n].uvOffset;

	return texture( textureUnits[textures[n].textureUnit - 1], offs );
}

void main()
{
	vec2	uv = texCoords[0].st;

	vec4	baseMap = getTexture( 0 );
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
	vec3	V = normalize( ViewDir );
	vec3	normal = vec3( 0.0, 0.0, 1.0 );
	if ( hasNormalMap ) {
		vec3 n = texture( NormalMap, uv ).rgb * 2.0 - 1.0;
		n.b = sqrt( max( 1.0 - dot( n.rg, n.rg ), 0.0 ) );
		normal = normalize( n );
	}

	// recolour from the palette only when it actually resolved and was bound
	bool	doGreyColor = greyscaleColor && hasGreyscaleMap;

	vec4	baseColor = glowColor;
	if ( !doGreyColor )
		baseColor.rgb *= glowMult;

	// view-angle falloff: soft edges / apparent volume. Only ever REDUCES the
	// mask (alpha) or dims the body (rgb) - it can never make a sprite opaque.
	float	falloff = 1.0;
	if ( useFalloff || hasRGBFalloff ) {
		falloff = smoothstep( falloffParams.x, falloffParams.y, abs( dot( normal, V ) ) );
		falloff = mix( max( falloffParams.z, 0.0 ), min( falloffParams.w, 1.0 ), falloff );

		if ( useFalloff )
			baseMap.a *= falloff;

		if ( hasRGBFalloff )
			baseMap.rgb *= falloff;
	}

	// the working alpha mask: base texture alpha * vertex-colour alpha (with the
	// optional soft-edge falloff already folded into baseMap.a). Nothing below
	// is allowed to raise it.
	vec4	color = baseMap * C;
	color.rgb *= baseColor.rgb;

	// greyscale-to-palette colour gradient (rgb only), gated by coverage so the
	// palette-indexed surround stays dark instead of painting the void
	if ( doGreyColor ) {
		vec3 pal = textureLod( GreyscaleMap, vec2( baseG, baseColor.r * C.r * falloff ), 0.0 ).rgb;
		color.rgb = pal * coverage * C.rgb;
	}

	// environment reflection, also masked by coverage so it shines on the sprite
	// body only
	if ( hasCubeMap ) {
		vec3 refl = envMapRotation * reflect( -V, normal );
		vec3 cube = texture( CubeMap, refl ).rgb;
		color.rgb += cube * envReflection * falloff * coverage;
	}

#ifdef WW_CELLLIGHTS
	if ( cellOn ) {
		if ( cellProbe != 0 && cellProbe != 6 )
			discard;	// the probe passes read surfaces; a particle is none
		vec3 lin = pow( max( color.rgb, vec3( 0.0 ) ), vec3( 2.2 ) );
		if ( fxLitMode != 0 )
			lin *= cellFxLit( cellWorldPos( -ViewDir ) );	// a lit particle, before the fog
		if ( fogOn ) {
			vec3 posView = -ViewDir;
			float hb;
			vec3 fogCol;
			float f = wwFogEval( length( posView ) * fogDistScale, dot( fogView.xyz, posView ) + fogView.w, hb, fogCol );
			lin = fxAdditive ? lin * ( 1.0 - f ) : mix( lin, fogCol, f );
		}
		if ( cellProbe == 6 )
			fragColor = vec4( lin, color.a );	// the imagespace's measure: the linear value, blended as drawn
		else
			fragColor = vec4( cellIsLinear ? lin : cellIsOn ? cellImageSpace( sqrt( lin ) ) : pow( lin, vec3( 1.0 / 2.2 ) ), color.a );
		return;
	}
#endif
	fragColor = color;
}
