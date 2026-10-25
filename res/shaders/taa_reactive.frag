#version 410 core

/* lane TAAINPUTS1: the mask's R (reactive: the layer's coverage, base alpha x vertex alpha x material alpha) and
 * G (composition: 1 where a see-through surface covers the pixel). outRG picks which the shape writes. */

uniform sampler2D BaseMap;
uniform vec2 uvScale;
uniform vec2 uvOffset;
uniform int useTex;
uniform int hasColor;
uniform float matAlpha;
uniform vec2 outRG;

in vec4 vColor;
in vec2 texCoord;

layout ( location = 1 ) out vec4 mask;

void main()
{
	float a = matAlpha;
	if ( hasColor != 0 )
		a *= vColor.a;
	if ( useTex != 0 )
		a *= texture( BaseMap, texCoord * uvScale + uvOffset ).a;
	a = clamp( a, 0.0, 1.0 );
	if ( a < 1.0 / 255.0 )
		discard;
	mask = vec4( outRG.x * a, outRG.y, 0.0, 0.0 );
}
