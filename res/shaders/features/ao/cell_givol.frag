#version 410 core

/* lane PERF1: the GI volume mask. Each pixel the shape's hull covers (behind its surface: the depth test) rebuilds its
 * surface from the shared opaque g-buffer and tests the point a bias in front of it against the shape; inside, the
 * shape's room + 1 (blended by max). 0: no interior volume, the outdoors (or the room grid, cell_gihalf.frag). */

#include "cell_givol.glsl"

uniform sampler2D volGbuf;	// view normal, linear depth in game units (1e6 empty)
uniform ivec2 volFull;
uniform vec2 volProj;		// the projection's P00, P11
uniform vec4 volRow[3];		// view -> world (cell_lights.glsl cellRow)
uniform float volBias;		// game units

flat in int vShape;

layout ( location = 0 ) out vec4 volOut;

void main()
{
	ivec2 f = ivec2( gl_FragCoord.xy );
	vec4 g = texelFetch( volGbuf, f, 0 );
	if ( g.w >= 1.0e5 || dot( g.xyz, g.xyz ) < 0.5 )
		discard;
	float z = -g.w / length( volRow[0].xyz );
	vec2 ndc = ( vec2( f ) + 0.5 ) / vec2( volFull ) * 2.0 - 1.0;
	vec4 pv = vec4( -z * ndc / volProj, z, 1.0 );
	vec3 P = vec3( dot( volRow[0], pv ), dot( volRow[1], pv ), dot( volRow[2], pv ) );
	vec3 N = normalize( vec3( dot( volRow[0].xyz, g.xyz ), dot( volRow[1].xyz, g.xyz ), dot( volRow[2].xyz, g.xyz ) ) );
	vec4 a = texelFetch( volShapes, 3 * vShape );
	vec4 b = texelFetch( volShapes, 3 * vShape + 1 );
	vec4 c = texelFetch( volShapes, 3 * vShape + 2 );
	vec3 l = transpose( volRot( c.xyz ) ) * ( P + N * volBias - a.xyz );
	if ( !volInside( l, int( a.w + 0.5 ), b.xyz ) )
		discard;
	volOut = vec4( b.w + 1.0 );
}
