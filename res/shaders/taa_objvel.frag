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

in vec4 cCur;
in vec4 cPrev;
in vec3 viewPos;
in vec2 texCoord;

layout ( location = 0 ) out vec2 vel;
layout ( location = 1 ) out vec4 flag;

void main()
{
	if ( wwGrassFade.z > 0.5 && length( viewPos ) - wwGrassFade.x >= max( wwGrassFade.y, 1e-3 ) )
		discard;
	if ( alphaTest != 0 && texture( BaseMap, texCoord * uvScale + uvOffset ).a * alpha < alphaThreshold )
		discard;
	if ( cCur.w <= 0.0 || cPrev.w <= 0.0 ) {
		vel = vec2( 0.0 );	// behind the eye last frame: no usable history
	} else {
		vel = ( cPrev.xy / cPrev.w - cCur.xy / cCur.w ) * 0.5;
	}
	flag = vec4( 0.0, 0.0, 1.0, 0.0 );
}
