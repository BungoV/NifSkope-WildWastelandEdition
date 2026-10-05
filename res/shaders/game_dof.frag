#version 410 core

/* lane GRASSMB1: the game's depth of field composite, instruction for instruction (src/gl/cellpost.h).
 * t0 the frame, t1 its blur, t2 the depth; c0 = (range, range, distance, -), c1 = (strength, near on, far on,
 * sky sharp), c2 = (-1e8, near, far - near, far * near), c3.zw the blur's uv scale (1 here).
 * The depth is this view's GL window depth: with the default depth range it equals the D3D depth the game's
 * formula expects, far * near / ((1 - d)(far - near) + near) = the view distance.
 * farOnly = the technique the effect picks when (flags & 3) == 2 (no near test, no near/far switches).
 * dofRed = the gate's red: the factor ignores the depth (strength everywhere). */

uniform sampler2D t0;
uniform sampler2D t1;
uniform sampler2D t2;
uniform vec4 c0;
uniform vec4 c1;
uniform vec4 c2;
uniform vec4 c3;
uniform int farOnly;
uniform int dofRed;

out vec4 fragColor;

float sat( float v )	// D3D _sat: NaN -> 0
{
	return v >= 0.0 ? min( v, 1.0 ) : 0.0;
}

void main()
{
	vec2 v1 = gl_FragCoord.xy / vec2( textureSize( t0, 0 ) );
	float x = 1.0 - texture( t2, v1 ).r;		// add r0.x, -r0.x, 1
	float y = x * c2.z + c2.y;					// mad
	bool notSky = 0.00001 < x;					// lt
	y = c2.w / y;								// div: the view distance
	float k;
	if ( farOnly != 0 ) {
		k = sat( ( y - c0.z ) / c0.y );			// add, div_sat
		k = k * c1.x;
		notSky = notSky || !( 0.0 != c1.w );		// ne, not, or
	} else {
		float nearT = ( c0.z - y ) / c0.x;
		bool nearOn = ( y < c0.z ) && ( 0.0 != c1.y );
		nearT = nearOn ? nearT : 0.0;			// and with the mask
		float farT = y - c0.z;
		bool farOn = ( c0.z < y ) && ( 0.0 != c1.z );
		notSky = notSky || !( 0.0 != c1.w );
		farT = farT / c0.y;
		k = sat( farOn ? farT : nearT );			// movc_sat
		k = k * c1.x;
	}
	if ( dofRed != 0 )
		k = c1.x;
	k = notSky ? k : 0.0;						// and r0.x, r0.y, r0.x
	vec4 b = texture( t1, v1 * c3.zw );
	vec4 a = texture( t0, v1 );
	fragColor = k * ( b - a ) + a;				// add, mad
}
