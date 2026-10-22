#version 410 core

/* The cell lights' shadow caster pass, alpha-tested shapes (lane LIGHTBOOTH1): cell_shadowdepth.frag behind the
 * colour pass's alpha test (fo4_default.frag: baseMap.a * alpha against alphaThreshold). */

uniform vec2 shRadiusNear;	// radius, near clip
uniform sampler2D BaseMap;
uniform vec2 uvScale;
uniform vec2 uvOffset;
uniform float alpha;
uniform float alphaThreshold;

in vec3 shRel;
in vec2 texCoord;

void main()
{
	float d = length( shRel );
	if ( d < shRadiusNear.y )
		discard;
	if ( texture( BaseMap, texCoord * uvScale + uvOffset ).a * alpha < alphaThreshold )
		discard;
	gl_FragDepth = clamp( d / shRadiusNear.x, 0.0, 1.0 );
}
