#version 410 core

uniform sampler2D BaseMap;
uniform sampler2D GreyscaleMap;
uniform samplerCube CubeMap;
uniform sampler2D NormalMap;
uniform sampler2D SpecularMap;

uniform bool hasSourceTexture;
uniform bool hasGreyscaleMap;
uniform bool hasCubeMap;
uniform bool hasNormalMap;
uniform bool hasEnvMask;

uniform bool greyscaleAlpha;
uniform bool greyscaleColor;

uniform bool useFalloff;
uniform bool hasRGBFalloff;

uniform bool hasWeaponBlood;

uniform vec4 glowColor;
uniform float glowMult;

uniform int alphaFlags;			// bits 0 to 2: alpha test mode, bit 3: alpha blending enabled
uniform float alphaThreshold;

uniform vec2 uvScale;
uniform vec2 uvOffset;

uniform vec4 falloffParams;
uniform float falloffDepth;

uniform float lightingInfluence;
uniform float envReflection;

in vec3 LightDir;
in vec3 ViewDir;

in vec2 texCoord;

flat in vec4 A;
in vec4 C;
flat in vec4 D;

in mat3 btnMatrix;
flat in mat3 reflMatrix;

out vec4 fragColor;

#ifdef WW_CELLLIGHTS
/* lane EFX2 (fo4_effectcell.frag; src/gl/celllights.h wwCellFxRed): the game's effect pixel shader in the
 * cell view, transcribed from Shaders011.fxp (docs/PRTP_PLAN.md 2q):
 *   colour   = texture x vertex colour x base colour, all linear (sampled sRGB, base colour powered 2.2)
 *   lighting = mix( c, c x emit, lighting influence ), emit the script-set external emit colour, WHITE when
 *              unset. That is the UNLIT effect. One whose material sets the effect-lighting flag takes the
 *              game's lit shader instead (lane FXLIT1, src/gl/cellfxlit.h): emit = the directional colour +
 *              up to four placed lights, colour x pow( 1 - saturate( d / radius )^2, 2.2 ) x the spot cone,
 *              per pixel, no N.L and no shadow
 *   Soft     = saturate( (scene w - w) / soft depth ) x smoothstep( 0.075, 0.5, w / soft depth ), w the view
 *              depth; on alpha, or on the palette's row for a greyscale-to-palette effect
 *   fog      = mix( c, fog colour, f ) for a blended effect, c x (1 - f) for an additive one
 * then the cell's imagespace on its own (an effect blends over a surface that already took the bloom). */
#include "uniforms.glsl"
#include "lookdev_fog.glsl"
#include "cell_lights.glsl"

uniform bool fxSoft;
uniform sampler2D fxDepth;		// the opaque pass's depth, the viewport's corner at texel 0
uniform float fxSoftDepth;		// game units
uniform float fxDistScale;		// view units -> game units
uniform bool fxAdditive;
uniform int fxRed;				// 2 nosoft, 4 nolin, 8 hide
// lane FXLIT1: this shape's model's placed lights (src/gl/cellfxlit.h)
uniform int fxLitMode;			// 0 an unlit effect; 1 lit; red controls: 2 every light, 4 no 2.2 on the falloff, 8 self-lit
uniform vec4 fxLit;				// indices into cellLights, -1 = none
uniform vec4 fxLitScale;		// 1 (a red control's per-light ratio)
uniform vec3 fxLitId;			// probe 70: the model's serial + 1, 24 bits

// one placed light on a lit effect at world point P, as the game's lit effect shader sums it
vec3 cellFxLight( int i, vec3 P )
{
	vec4 t0 = texelFetch( cellLights, i * CELL_TPL );	// lane FXLIT1: the stride is HEMI1's 8 texels a light
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

// what a lit effect's colour is multiplied by at P: mix( 1, directional + its lights, lighting influence )
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

float cellFxFade()
{
	if ( !fxSoft || ( fxRed & 2 ) != 0 )
		return 1.0;
	float d = texelFetch( fxDepth, ivec2( gl_FragCoord.xy ) - viewportDimensions.xy, 0 ).r;
	// view depth from the projection: w = B / (A + z_ndc)
	float sceneW = projectionMatrix[3][2] / ( projectionMatrix[2][2] + ( 2.0 * d - 1.0 ) );
	float k = fxDistScale / fxSoftDepth;
	float fragW = 1.0 / gl_FragCoord.w;
	float soft = clamp( ( sceneW - fragW ) * k, 0.0, 1.0 );
	// the game subtracts its own near term here (INFERRED to be about one unit; read as none)
	float t = clamp( ( fragW * k - 0.075 ) * 2.352941, 0.0, 1.0 );
	return soft * t * t * ( 3.0 - 2.0 * t );
}
#endif

vec3 ViewDir_norm = normalize( ViewDir );
mat3 btnMatrix_norm = mat3( normalize( btnMatrix[0] ), normalize( btnMatrix[1] ), normalize( btnMatrix[2] ) );


void main()
{
#ifdef WW_CELLLIGHTS
	if ( cellOn && ( fxRed & 8 ) != 0 )
		discard;
#endif
	vec2 offset = texCoord.st * uvScale + uvOffset;

	vec4 baseMap = texture( BaseMap, offset );
	vec4 normalMap = texture( NormalMap, offset );
	vec4 specMap = texture( SpecularMap, offset );

	vec3 normal = normalize(normalMap.rgb * 2.0 - 1.0);
	// Calculate missing blue channel
	normal.b = sqrt(max(1.0 - dot(normal.rg, normal.rg), 0.0));
	normal = normalize( btnMatrix_norm * normal );
	if ( !gl_FrontFacing )
		normal *= -1.0;

	vec3 L = normalize(LightDir);
	vec3 V = ViewDir_norm;
	vec3 R = reflect(-V, normal);
	vec3 H = normalize( L + V );

	float NdotL = max( dot(normal, L), 0.000001 );
	float NdotH = max( dot(normal, H), 0.000001 );
	float NdotV = max( dot(normal, V), 0.000001 );
	float LdotH = max( dot(L, H), 0.000001 );
	float NdotNegL = max( dot(normal, -L), 0.000001 );

	vec3 reflectedWS = reflMatrix * R;

	if ( greyscaleAlpha )
		baseMap.a = 1.0;

	vec4 baseColor = glowColor;
	if ( !greyscaleColor )
		baseColor.rgb *= glowMult;

	// Falloff
	float falloff = 1.0;
	if ( useFalloff || hasRGBFalloff ) {
		falloff = smoothstep( falloffParams.x, falloffParams.y, abs(dot(normal, V)) );
		falloff = mix( max(falloffParams.z, 0.0), min(falloffParams.w, 1.0), falloff );

		if ( useFalloff )
			baseMap.a *= falloff;

		if ( hasRGBFalloff )
			baseMap.rgb *= falloff;
	}

	float alphaMult = baseColor.a * baseColor.a;

	vec4 color = baseMap * C;
	color.rgb *= baseColor.rgb;
	color.a *= alphaMult;

#ifdef WW_CELLLIGHTS
	// lane EFX2: the Soft fades scale alpha, or a palette's row (the game's PS multiplies its palette coordinates)
	float fxFade = cellOn ? cellFxFade() : 1.0;
	if ( !greyscaleAlpha )
		color.a *= fxFade;
#define WW_FX_FADE * fxFade
#else
#define WW_FX_FADE
#endif
	if ( greyscaleColor )
		color.rgb = textureLod( GreyscaleMap, vec2( texture( BaseMap, offset ).g, baseColor.r * C.r * falloff WW_FX_FADE ), 0.0 ).rgb;

	if ( greyscaleAlpha )
		color.a = textureLod( GreyscaleMap, vec2( texture( BaseMap, offset ).a, color.a WW_FX_FADE ), 0.0 ).a;

	if ( alphaFlags > 0 ) {
		// 0: always, 1: <, 2: ==, 3: <=, 4: >, 5: !=, 6: >=, 7: never
		int	m = ( color.a < alphaThreshold ? 0x2B2B : ( color.a > alphaThreshold ? 0x7171 : 0x4D4D ) );
		if ( ( m & ( 1 << alphaFlags ) ) == 0 )
			discard;
	}
	if ( alphaFlags < 8 )
		color.a = 1.0;

	vec3 diffuse = A.rgb + (D.rgb * NdotL);
#ifdef WW_CELLLIGHTS
	if ( !cellOn )	// lane EFX2: the cell view's emit colour is white, so the lighting influence changes nothing
#endif
	color.rgb = mix( color.rgb, color.rgb * D.rgb, lightingInfluence );

	// Specular
	float g = 1.0;
	float s = 1.0;
	if ( hasEnvMask ) {
		g = specMap.r;
		s = specMap.g;
	}

	// Environment
	vec4 cube = texture( CubeMap, reflectedWS );
	if ( hasCubeMap ) {
		cube.rgb *= envReflection * s;
#ifdef WW_CELLLIGHTS
		if ( !cellOn )
#endif
		cube.rgb = mix( cube.rgb, cube.rgb * D.rgb, lightingInfluence );

		color.rgb += cube.rgb * falloff;
	}

#ifdef WW_CELLLIGHTS
	if ( cellOn ) {
		vec3 lin = ( fxRed & 4 ) != 0 ? max( color.rgb, vec3( 0.0 ) ) : pow( max( color.rgb, vec3( 0.0 ) ), vec3( 2.2 ) );
		if ( fxLitMode != 0 ) {	// lane FXLIT1: a lit effect, in linear light, before the fog
			vec3 P = cellWorldPos( -ViewDir );
			vec3 m = cellFxLit( P );
			if ( cellProbe >= 70 && cellProbe <= 74 ) {
				// its probes, opaque: 70 the model, 71 / 72 the position (as probes 2 / 3), 73 / 74 the multiplier / 4
				vec3 q = cellProbe >= 73 ? floor( clamp( m * 0.25, 0.0, 1.0 ) * 65535.0 + 0.5 )
					: clamp( floor( P - cellCenter + 32768.0 ), 0.0, 65535.0 );
				bool high = cellProbe == 71 || cellProbe == 73;
				fragColor = vec4( cellProbe == 70 ? fxLitId : high ? floor( q / 256.0 ) / 255.0 : mod( q, 256.0 ) / 255.0, 1.0 );
				return;
			}
			lin *= m;
		}
		if ( fogOn ) {
			vec3 posView = -ViewDir;
			float hb;
			vec3 fogCol;
			float f = wwFogEval( length( posView ) * fogDistScale, dot( fogView.xyz, posView ) + fogView.w, hb, fogCol );
			lin = fxAdditive ? lin * ( 1.0 - f ) : mix( lin, fogCol, f );
		}
		// probe 6 (the imagespace's measure): the linear value, blended as drawn
		if ( cellProbe == 6 )
			fragColor = vec4( lin, color.a );
		else
			fragColor = vec4( cellIsLinear ? lin	// lane HDR1: linear, summed in the HDR frame, tone-mapped once
				: cellIsOn ? cellImageSpace( sqrt( lin ) ) : pow( lin, vec3( 1.0 / 2.2 ) ), color.a );
		return;
	}
#endif
	fragColor = vec4( color.rgb * sqrt(D.a), color.a );
}
