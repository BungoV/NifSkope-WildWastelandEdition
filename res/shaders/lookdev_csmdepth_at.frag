#version 410 core

/* The cascade caster pass, alpha-tested shapes (lane VOLAIR1): the same test the colour pass
 * runs (fo4_default.frag: baseMap.a * alpha against alphaThreshold), kept where it passes. */

uniform sampler2D BaseMap;
uniform vec2 uvScale;
uniform vec2 uvOffset;
uniform float alpha;
uniform float alphaThreshold;

in vec2 texCoord;

void main()
{
	if ( texture( BaseMap, texCoord * uvScale + uvOffset ).a * alpha < alphaThreshold )
		discard;
}
