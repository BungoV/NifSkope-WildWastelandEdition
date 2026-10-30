#version 410 core

/* lane TAAINPUTS1: the object vector, previous minus current in texture units (game_taa_mv.frag's convention),
 * and the own-motion flag in the mask's B. The alpha test and the grass fade drop what the frame dropped. */

uniform sampler2D BaseMap;
uniform vec2 uvScale;
uniform vec2 uvOffset;
uniform int alphaTest;
uniform float alpha;
uniform float alphaThreshold;
uniform vec3 wwGrassFade;
/* lane TREELEAF3 (src/gl/wwfxcov.h, its GLSL mirror): 0 = today's test, base.a x alpha; 4 = an effect shape's composed
 * alpha as fo4_effectshader.frag tests it (alpha = baseColor.a^2), + 1 x the vertex alpha, + 2 the palette's alpha at
 * (base.a, the rest) */
uniform int fxMode;
uniform sampler2D GreyscaleMap;

in vec4 cCur;
in vec4 cPrev;
in vec3 viewPos;
in vec2 texCoord;
in float vAlpha;

layout ( location = 0 ) out vec2 vel;
layout ( location = 1 ) out vec4 flag;

void main()
{
	if ( wwGrassFade.z > 0.5 && length( viewPos ) - wwGrassFade.x >= max( wwGrassFade.y, 1e-3 ) )
		discard;
	if ( alphaTest != 0 ) {
		float a = texture( BaseMap, texCoord * uvScale + uvOffset ).a;
		if ( fxMode != 0 ) {
			float k = ( fxMode & 1 ) != 0 ? vAlpha : 1.0;
			a = ( fxMode & 2 ) != 0 ? textureLod( GreyscaleMap, vec2( a, k * alpha ), 0.0 ).a : a * k * alpha;
		} else {
			a *= alpha;
		}
		if ( a < alphaThreshold )
			discard;
	}
	if ( cCur.w <= 0.0 || cPrev.w <= 0.0 ) {
		vel = vec2( 0.0 );	// behind the eye last frame: no usable history
	} else {
		vel = ( cPrev.xy / cPrev.w - cCur.xy / cCur.w ) * 0.5;
	}
	flag = vec4( 0.0, 0.0, 1.0, 0.0 );
}
