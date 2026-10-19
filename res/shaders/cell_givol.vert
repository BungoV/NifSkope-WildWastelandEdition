#version 410 core

/* lane PERF1: one GI volume shape per instance, its hull (a box grown by the mask's bias) from gl_VertexID, 36
 * vertices, outward faces counter-clockwise; drawn back faces only (src/gl/celllights.cpp giVolMaskPass) */

#include "cell_givol.glsl"

uniform vec4 volClip[4];	// clip = volClip[r] . (world, 1), r = x, y, z, w
uniform float volGrow;		// game units

flat out int vShape;

const vec3 FN[6] = vec3[6]( vec3( 1, 0, 0 ), vec3( -1, 0, 0 ), vec3( 0, 1, 0 ), vec3( 0, -1, 0 ), vec3( 0, 0, 1 ), vec3( 0, 0, -1 ) );
const vec3 FU[6] = vec3[6]( vec3( 0, 1, 0 ), vec3( 0, 0, 1 ), vec3( 0, 0, 1 ), vec3( 1, 0, 0 ), vec3( 1, 0, 0 ), vec3( 0, 1, 0 ) );
const vec3 FV[6] = vec3[6]( vec3( 0, 0, 1 ), vec3( 0, 1, 0 ), vec3( 1, 0, 0 ), vec3( 0, 0, 1 ), vec3( 0, 1, 0 ), vec3( 1, 0, 0 ) );
const vec2 FC[4] = vec2[4]( vec2( -1, -1 ), vec2( 1, -1 ), vec2( 1, 1 ), vec2( -1, 1 ) );

void main()
{
	int s = gl_InstanceID;
	vShape = s;
	vec4 a = texelFetch( volShapes, 3 * s );
	vec4 b = texelFetch( volShapes, 3 * s + 1 );
	vec4 c = texelFetch( volShapes, 3 * s + 2 );
	int f = gl_VertexID / 6, k = gl_VertexID % 6;
	vec2 q = FC[k < 3 ? k : k == 3 ? 0 : k - 1];
	vec3 l = 0.5 * ( FN[f] + q.x * FU[f] + q.y * FV[f] ) * ( b.xyz + 2.0 * volGrow );
	vec4 w = vec4( volRot( c.xyz ) * l + a.xyz, 1.0 );
	gl_Position = vec4( dot( volClip[0], w ), dot( volClip[1], w ), dot( volClip[2], w ), dot( volClip[3], w ) );
}
