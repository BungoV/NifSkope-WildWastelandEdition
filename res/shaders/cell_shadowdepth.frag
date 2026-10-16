#version 410 core

/* Cell light shadow caster pass: depth = distance to the light over its radius; nothing nearer than
 * the near clip casts. No color attachment. */

uniform vec2 shRadiusNear;	// radius, near clip

in vec3 shRel;

void main()
{
	float d = length( shRel );
	if ( d < shRadiusNear.y )
		discard;
	gl_FragDepth = clamp( d / shRadiusNear.x, 0.0, 1.0 );
}
