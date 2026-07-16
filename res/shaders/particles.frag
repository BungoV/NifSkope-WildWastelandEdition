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

	fragColor = color;
}
