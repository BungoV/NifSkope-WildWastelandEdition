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

uniform bool volGeoN;		// lane GISUNBLOTCH1: the bias along the face's normal (false: the shading normal, red bumpn)

/* lane GISUNBLOTCH1: pixel p's view position (main's rule); false: off the screen or no surface */
bool volViewAt( ivec2 p, out vec3 v )
{
	v = vec3( 0.0 );
	if ( any( lessThan( p, ivec2( 0 ) ) ) || any( greaterThanEqual( p, volFull ) ) )
		return false;
	vec4 q = texelFetch( volGbuf, p, 0 );
	if ( q.w >= 1.0e5 || dot( q.xyz, q.xyz ) < 0.5 )
		return false;
	float z = -q.w / length( volRow[0].xyz );
	vec2 ndc = ( vec2( p ) + 0.5 ) / vec2( volFull ) * 2.0 - 1.0;
	v = vec3( -z * ndc / volProj, z );
	return true;
}

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
	if ( volGeoN ) {	// lane GISUNBLOTCH1: the face's normal from the depth, on the shading normal's side
		vec3 a0, a1, b0, b1;
		bool oa0 = volViewAt( f + ivec2( 1, 0 ), a0 ), oa1 = volViewAt( f - ivec2( 1, 0 ), a1 );
		bool ob0 = volViewAt( f + ivec2( 0, 1 ), b0 ), ob1 = volViewAt( f - ivec2( 0, 1 ), b1 );
		if ( ( oa0 || oa1 ) && ( ob0 || ob1 ) ) {
			vec3 dx = ( oa0 && ( !oa1 || abs( a0.z - pv.z ) <= abs( a1.z - pv.z ) ) ) ? a0 - pv.xyz : pv.xyz - a1;
			vec3 dy = ( ob0 && ( !ob1 || abs( b0.z - pv.z ) <= abs( b1.z - pv.z ) ) ) ? b0 - pv.xyz : pv.xyz - b1;
			vec3 nv = cross( dx, dy );
			if ( dot( nv, nv ) > 1.0e-20 ) {
				vec3 nw = normalize( vec3( dot( volRow[0].xyz, nv ), dot( volRow[1].xyz, nv ), dot( volRow[2].xyz, nv ) ) );
				N = dot( nw, N ) < 0.0 ? -nw : nw;
			}
		}
	}
	vec4 a = texelFetch( volShapes, 3 * vShape );
	vec4 b = texelFetch( volShapes, 3 * vShape + 1 );
	vec4 c = texelFetch( volShapes, 3 * vShape + 2 );
	vec3 l = transpose( volRot( c.xyz ) ) * ( P + N * volBias - a.xyz );
	if ( !volInside( l, int( a.w + 0.5 ), b.xyz ) )
		discard;
	volOut = vec4( b.w + 1.0 );
}
