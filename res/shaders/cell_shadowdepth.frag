#version 410 core

/* The cell lights' shadow caster pass (lane SHADOW1): depth = the distance to the light over its
 * radius (a cube's texel holds the nearest caster along its direction); nothing nearer the light
 * than its near clip casts. No colour attachment. */

uniform vec2 shRadiusNear;	// radius, near clip

in vec3 shRel;

void main()
{
	float d = length( shRel );
	if ( d < shRadiusNear.y )
		discard;
	gl_FragDepth = clamp( d / shRadiusNear.x, 0.0, 1.0 );
}
