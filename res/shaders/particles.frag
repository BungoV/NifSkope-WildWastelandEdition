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
 * flag alone, beside Ptcl (checked in Todd's treat), so a particle whose
 * effect shader is lit takes the same placed lights as a lit effect card (lane FXLIT1, fo4_effectshader.frag):
 * mix( 1, directional + the model's four lights, lighting influence ), in linear light, before the fog.
 * glparticles.cpp sets these from src/gl/cellfxlit.h; fxLitMode 0 = unlit, as before. */
uniform float lightingInfluence;
uniform int fxLitMode;			// 0 unlit; 1 lit; red controls: 2 every light, 4 no 2.2 on the falloff, 8 self-lit
uniform vec4 fxLit;				// indices into cellLights, -1 = none
uniform vec4 fxLitScale;		// 1 (a red control's per-light ratio)
/* lane SUNCELL1 final: the game's particle shaders raise the WHOLE vertex colour to 2.2, alpha too (log / mul 2.2 /
 * exp on v2.xyzw, asm 00613 / 00604 / 00903); true = the red WW_CELL_PARTICLES_RED=linalpha (alpha as written) */
uniform bool pfxAlphaRed;

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
	// lane SUNCELL1 final: an exterior with no weather loaded has no sun to light it -- self-lit, as before
	if ( ( fxLitMode & 8 ) != 0 || ( !cellInterior && !cellHasDir ) )
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

#ifdef WW_CELLLIGHTS
/* lane VOLPART1: VOLUMETRIC PARTICLES (src/gl/glparticles.cpp; the row "Volumetric Particles" ships off). A puff of
 * steam or fog is a little piece of the air: lit by the light that is really there, from the very list the air
 * (Volumetric Air, cell_volair.frag) reads -- the directional light x its cascade shadow, every placed light (colour x
 * radial curve x cone x its shadow cube) x the air's transmittance to it, the GI and the ambient as six-axis cubes --
 * evaluated DIRECTLY per pixel at the sprite's own point, never from the integrated volume. Then it is seen through
 * the air between it and the eye (the INTEGRATED froxel volume, as every surface: colour x T + L), so steam and air
 * fade alike.
 *   thin   the light the medium scatters toward the eye, the air's phase: per unit opacity, the air's own J
 *   thick  a wrapped diffuse at the puff's normal (the normal map turned by the sprite's rotation, else a sphere over
 *          the sprite): max(0, N.L + w) / (1 + w) per light, the cubes read at N
 *   alpha as thickness: the pixel's opacity a is 1 - exp(-tau), so its single-scatter share is exp(-tau) = 1 - a;
 *          lit = (1 - a) thin + a thick, wrap w = 1 - a (a thin edge lets the light round it)
 *   soft   the sprite is a ball of half-depth h = r sqrt(1 - |q|^2) round its plane; the share of it in front of the
 *          scene (the opaque pass's depth) is its opacity: no hard line where a card meets the ground or a wall
 * A lit system only (the effect's lighting flag, as the game); lightingInfluence mixes from 1 as before. An additive
 * one keeps the effect law (it emits) and takes the air's transmittance alone. */
#include "ww_sunshadow.glsl"
#include "cell_airmed.glsl"
uniform int volPart;			// 0 off: the law above, as before; 1 on
uniform int volPartRed;			// gates only: 1 flat, 2 nothick, 4 nosoft, 8 nolights, 16 white, 32 nofog, 64 noshadow
uniform int volPartDump;		// gates only: the probe row's width in pixels (particles.geom), 0 off
uniform bool volCsm;			// the sun's cascades are drawn this frame
uniform int volTerms;			// the air's mask (WW_CELL_AIR_TERMS): 1 directional, 2 placed, 4 GI, 8 ambient
uniform bool fxSoft;			// the opaque pass's depth is bound
uniform sampler2D fxDepth;		// the opaque pass's depth, the viewport's corner at texel 0

in vec3 pPos;
in vec2 pQ;
flat in vec4 pSprite;

vec3 vpTint( vec3 e )
{
	return ( volPartRed & 16 ) != 0 ? vec3( dot( e, vec3( 0.2126, 0.7152, 0.0722 ) ) ) : e;	// red white: grey light
}

float vpWrap( float nl, float w )
{
	return max( nl + w, 0.0 ) / ( 1.0 + w );
}

// the six-axis cube at N: the sum of n_a^2 over the axes N faces
vec3 vpCubeAt( vec3 C[6], vec3 N )
{
	vec3 n2 = N * N;
	return n2.x * ( N.x >= 0.0 ? C[0] : C[1] ) + n2.y * ( N.y >= 0.0 ? C[2] : C[3] ) + n2.z * ( N.z >= 0.0 ? C[4] : C[5] );
}

/* the light at the sprite's point (posView, world P, height z in the fog's frame) toward the eye along dW (world):
 * thin = the air's J there (pi x radiance per unit opacity), thick = the wrapped diffuse at world normal N */
void vpLight( vec3 posView, vec3 P, vec3 dW, float z, vec3 N, float w, out vec3 thin, out vec3 thick )
{
	thin = vec3( 0.0 );
	thick = vec3( 0.0 );
	if ( ( volPartRed & 1 ) != 0 )
		N = cellWorldDir( vec3( 0.0, 0.0, 1.0 ) );	// red flat: every pixel faces the eye's axis
	if ( ( volTerms & 1 ) != 0 && cellHasDir ) {
		vec3 to = normalize( cellDirTo );
		float vis = ( !cellInterior && volCsm && ( volPartRed & 64 ) == 0 ) ? wwSunShadow( posView ) : 1.0;
		vec3 e = vpTint( cellDirColor ) * vis * airTup( z, to.z );
		thin += e * volPhasePi( dot( to, dW ) );
		thick += e * vpWrap( dot( N, to ), w );
	}
	float zOff = z - P.z;	// the fog's height frame minus world z
	if ( ( volTerms & 2 ) != 0 && ( volPartRed & 8 ) == 0 ) {
		for ( int i = 0; i < cellLightCount; i++ ) {
			vec4 t0 = texelFetch( cellLights, i * CELL_TPL );
			float d = length( t0.xyz - P );
			if ( d >= t0.w || d < 0.001 )
				continue;
			vec3 L = ( t0.xyz - P ) / d, Lo;
			bool noSpec;
			vec3 e = cellLightE( i, P, L, Lo, noSpec );	// N = L, as the air: no N.L, the shadow lifted toward the light
			if ( all( equal( e, vec3( 0.0 ) ) ) )
				continue;
			e = vpTint( e ) * exp( -airOd( z, t0.z + zOff, d ) );
			thin += e * volPhasePi( dot( L, dW ) );
			thick += e * vpWrap( dot( N, L ), w );
		}
	}
	bool wantGi = ( volTerms & 4 ) != 0 && cellGiOn;
	bool ambDalc = cellInterior ? cellHasDalc : cellExtDalc;
	bool wantAmb = ( volTerms & 8 ) != 0 && ambDalc && !( cellInterior && cellGiAmb == 2 );
	if ( !wantGi && !wantAmb )
		return;
	vec3 E[6];
	float share[6];
	for ( int a = 0; a < 6; a++ ) {
		E[a] = vec3( 0.0 );
		share[a] = 0.0;
	}
	if ( cellGiOn )
		volCubeAll( P, E, share );
	if ( wantGi ) {
		vec3 Eg[6];
		for ( int a = 0; a < 6; a++ )
			Eg[a] = E[a] * ( 1.0 / PI );	// the surfaces' GI unit -> the ambient's, as the air
		thin += volCubeScatter( Eg, dW );
		thick += vpCubeAt( Eg, N );
	}
	if ( wantAmb ) {
		bool replace = cellGiOn && ( cellInterior ? cellGiAmb == 1 : cellGiSky );
		const vec3 axes[6] = vec3[6]( vec3( 1, 0, 0 ), vec3( -1, 0, 0 ), vec3( 0, 1, 0 ), vec3( 0, -1, 0 ), vec3( 0, 0, 1 ), vec3( 0, 0, -1 ) );
		vec3 Am[6];
		for ( int a = 0; a < 6; a++ )
			Am[a] = cellAmbient( axes[a], P ) * ( replace ? 1.0 - share[a] : 1.0 );
		thin += volCubeScatter( Am, dW );
		thick += vpCubeAt( Am, N );
	}
}

// the puff's normal (view space): the normal map turned by the sprite's rotation, else a sphere over the sprite
vec3 vpNormal( vec2 uvN )
{
	vec2 T = pSprite.xy, B = vec2( -pSprite.y, pSprite.x );
	vec3 n;
	if ( hasNormalMap ) {
		n = texture( NormalMap, uvN ).rgb * 2.0 - 1.0;
		n.z = sqrt( max( 1.0 - dot( n.xy, n.xy ), 0.0 ) );
	} else {
		float r2 = dot( pQ, pQ );
		n = r2 < 1.0 ? vec3( pQ, sqrt( 1.0 - r2 ) ) : vec3( pQ * inversesqrt( r2 ), 0.0 );
	}
	return normalize( vec3( n.x * T + n.y * B, n.z ) );
}

// the share of the puff's ball in front of the scene (1 = all of it)
float vpSoft()
{
	if ( !fxSoft || ( volPartRed & 4 ) != 0 )
		return 1.0;
	float dz = texelFetch( fxDepth, ivec2( gl_FragCoord.xy ) - viewportDimensions.xy, 0 ).r;
	float sceneW = projectionMatrix[3][2] / ( projectionMatrix[2][2] + ( 2.0 * dz - 1.0 ) );
	float h = pSprite.z * max( sqrt( max( 1.0 - dot( pQ, pQ ), 0.0 ) ), 0.2 );
	return clamp( ( sceneW - ( -pPos.z - h ) ) / ( 2.0 * h ), 0.0, 1.0 );
}

// the lit factor at this pixel (linear), in place of cellFxLit
vec3 vpLit( vec3 posView, vec3 Nview, float a )
{
	if ( ( fxLitMode & 8 ) != 0 || ( !cellInterior && !cellHasDir ) )
		return vec3( 1.0 );	// self-lit, as cellFxLit
	float m = ( volPartRed & 2 ) != 0 ? 1.0 : 1.0 - clamp( a, 0.0, 1.0 );
	vec3 thin, thick;
	vpLight( posView, cellWorldPos( posView ), cellWorldDir( posView ), dot( fogView.xyz, posView ) + fogView.w,
		cellWorldDir( Nview ), m, thin, thick );
	return mix( vec3( 1.0 ), m * thin + ( 1.0 - m ) * thick, lightingInfluence );
}

/* gates only (volPartDump): this particle's 8 probe pixels, at its centre: 0 thin (the air's J there), 1 thick at
 * N = +Z view, 2 thick at N = -Z view (w = 0), 3 world P + height z, 4 the eye's ray (world) + its distance (game
 * units), 5 the integrated air (L, T) at its column as the draw applies it (a = -1: none applied), 6 the lit factor at N = +Z with its own alpha (alpha in a),
 * 7 its view point + alpha */
vec4 vpDump( float a )
{
	int slot = int( gl_FragCoord.x ) - int( pSprite.w + 0.5 ) * 8;
	vec3 posView = pPos;
	vec3 P = cellWorldPos( posView );
	vec3 dW = cellWorldDir( posView );
	float z = dot( fogView.xyz, posView ) + fogView.w;
	vec3 Zw = cellWorldDir( vec3( 0.0, 0.0, 1.0 ) );
	vec3 thin, thick;
	if ( slot <= 1 ) {
		vpLight( posView, P, dW, z, Zw, 0.0, thin, thick );
		return vec4( slot == 0 ? thin : thick, 1.0 );
	}
	if ( slot == 2 ) {
		vpLight( posView, P, dW, z, -Zw, 0.0, thin, thick );
		return vec4( thick, 1.0 );
	}
	if ( slot == 3 )
		return vec4( P, z );
	if ( slot == 4 )
		return vec4( dW, length( posView ) * fogDistScale );
	if ( slot == 5 ) {
		vec4 c = projectionMatrix * vec4( posView, 1.0 );
		return ( fogOn && ( volPartRed & 32 ) == 0 && volOn && volSlices.w > 0.5 ) ? wwVolAirAtUv( posView, c.xy / c.w * 0.5 + 0.5 ) : vec4( 0.0, 0.0, 0.0, -1.0 );
	}
	if ( slot == 6 )
		return vec4( vpLit( posView, vec3( 0.0, 0.0, 1.0 ), a ), a );
	return vec4( posView, a );
}
#endif

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
		if ( !pfxAlphaRed )
			color.a = baseMap.a * pow( max( C.a, 0.0 ), 2.2 );	// texture alpha (falloff folded in) x vertex alpha^2.2
		vec3 lin = pow( max( color.rgb, vec3( 0.0 ) ), vec3( 2.2 ) );
		if ( volPart != 0 ) {
			// lane VOLPART1: lit at its own point, seen through the integrated air, soft against the scene
			if ( volPartDump > 0 ) {
				fragColor = vpDump( color.a );
				return;
			}
			if ( fxLitMode != 0 )
				lin *= fxAdditive ? cellFxLit( cellWorldPos( -ViewDir ) ) : vpLit( pPos, vpNormal( uv ), color.a );
			color.a *= vpSoft();
			if ( fogOn && ( volPartRed & 32 ) == 0 && volOn && volSlices.w > 0.5 ) {
				vec4 air = wwVolAirAt( pPos );
				lin = fxAdditive ? lin * air.a : lin * air.a + air.rgb;	// blended: a x (c T + L) over the fogged frame
				if ( cellProbe == 6 )
					fragColor = vec4( lin, color.a );
				else
					fragColor = vec4( cellIsLinear ? lin : cellIsOn ? cellImageSpace( sqrt( lin ) ) : pow( lin, vec3( 1.0 / 2.2 ) ), color.a );
				return;
			}
		} else if ( fxLitMode != 0 )
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
