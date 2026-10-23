#version 410 core

/* The cell lights' shadow caster pass (lane SHADOW1): depth = the distance to the light over its
 * radius (a cube's texel holds the nearest caster along its direction); nothing nearer the light
 * than its near clip casts. No colour attachment. */

uniform vec2 shRadiusNear;	// radius, near clip
// lane GAPFIX1: alpha-tested casters keep what the colour pass keeps (as lookdev_csmdepth_at.frag)
uniform int shAlphaTest;
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
	if ( shAlphaTest != 0 && texture( BaseMap, texCoord * uvScale + uvOffset ).a * alpha < alphaThreshold )
		discard;
	gl_FragDepth = clamp( d / shRadiusNear.x, 0.0, 1.0 );
}
