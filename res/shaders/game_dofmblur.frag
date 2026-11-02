#version 410 core

/* lane POSTFX2: the game's depth of field composite (game_dof.frag) and its motion blur (game_mblur.frag) in ONE
 * full-screen pass, for frames where both run (src/gl/cellpost.cpp). The two-pass chain writes the composite to a
 * half-float target, copies it back and reads it with linear filtering at the blur's taps; here every tap takes
 * the composite of the four texels the filter would read and blends them with the filter's weights, so no target,
 * copy or second pass is needed. Red WW_CELL_POSTFUSE_RED=1 keeps the two passes.
 * t0 the frame, t1 the motion vectors, t2 the depth of field's blur, t3 the depth; dc0..dc3 = game_dof's c0..c3,
 * mc0, mc1 = game_mblur's c0, c1. */

uniform sampler2D t0;
uniform sampler2D t1;
uniform sampler2D t2;
uniform sampler2D t3;
uniform vec4 dc0;
uniform vec4 dc1;
uniform vec4 dc2;
uniform vec4 dc3;
uniform vec4 mc0;
uniform vec4 mc1;
uniform int farOnly;
uniform int dofRed;

out vec4 fragColor;

float sat( float v )	// D3D _sat: NaN -> 0
{
	return v >= 0.0 ? min( v, 1.0 ) : 0.0;
}

vec2 V( vec2 d )	// game texture space -> this view's
{
	return vec2( d.x, 1.0 - d.y );
}

// game_dof.frag's composite at one texel
vec4 dofTexel( ivec2 p, vec2 size )
{
	vec2 v1 = ( vec2( p ) + 0.5 ) / size;
	float x = 1.0 - texelFetch( t3, p, 0 ).r;
	float y = x * dc2.z + dc2.y;
	bool notSky = 0.00001 < x;
	y = dc2.w / y;
	float k;
	if ( farOnly != 0 ) {
		k = sat( ( y - dc0.z ) / dc0.y );
		k = k * dc1.x;
		notSky = notSky || !( 0.0 != dc1.w );
	} else {
		float nearT = ( dc0.z - y ) / dc0.x;
		bool nearOn = ( y < dc0.z ) && ( 0.0 != dc1.y );
		nearT = nearOn ? nearT : 0.0;
		float farT = y - dc0.z;
		bool farOn = ( dc0.z < y ) && ( 0.0 != dc1.z );
		notSky = notSky || !( 0.0 != dc1.w );
		farT = farT / dc0.y;
		k = sat( farOn ? farT : nearT );
		k = k * dc1.x;
	}
	if ( dofRed != 0 )
		k = dc1.x;
	k = notSky ? k : 0.0;
	vec4 b = texture( t2, v1 * dc3.zw );
	vec4 a = texelFetch( t0, p, 0 );
	return k * ( b - a ) + a;
}

// the composite read as a linear filter reads its target at uv (clamped to the edge)
vec3 dofAt( vec2 uv, vec2 size )
{
	vec2 x = uv * size - 0.5;
	vec2 i0 = floor( x );
	vec2 f = x - i0;
	ivec2 lo = ivec2( i0 );
	ivec2 hi = min( lo + 1, ivec2( size ) - 1 );
	lo = max( lo, ivec2( 0 ) );
	hi = max( hi, ivec2( 0 ) );
	vec3 r = dofTexel( lo, size ).rgb * ( ( 1.0 - f.x ) * ( 1.0 - f.y ) );
	if ( f.x > 0.0 )
		r += dofTexel( ivec2( hi.x, lo.y ), size ).rgb * ( f.x * ( 1.0 - f.y ) );
	if ( f.y > 0.0 ) {
		r += dofTexel( ivec2( lo.x, hi.y ), size ).rgb * ( ( 1.0 - f.x ) * f.y );
		if ( f.x > 0.0 )
			r += dofTexel( hi, size ).rgb * ( f.x * f.y );
	}
	return r;
}

vec2 mvAt( vec2 d )
{
	vec2 m = texture( t1, V( d ) ).xy;
	return vec2( m.x, -m.y );
}

float tapW( vec2 m, float L )
{
	float l = sqrt( dot( m, m ) );
	return sat( -abs( L - l ) * mc0.z + 1.0 );
}

void main()
{
	vec2 size = vec2( textureSize( t0, 0 ) );
	vec2 g = gl_FragCoord.xy / size;
	vec2 v1 = V( g );
	vec2 m = mvAt( v1 );
	float l2 = dot( m, m );
	float L = sqrt( l2 );
	vec2 dc = v1 - mc1.zw;
	float rr = sqrt( dot( dc, dc ) );
	rr = sat( rr * 0.1 + -0.018 );
	rr = L * rr;
	if ( rr < 0.001 ) {
		fragColor = dofTexel( ivec2( gl_FragCoord.xy ), size );
		return;
	}
	vec2 dir = inversesqrt( l2 ) * m;
	float len = min( rr, mc0.y );
	dir = len * dir;
	dir = dir * mc0.x;

	vec2 p0 = min( v1, mc1.xy );
	float w0 = tapW( mvAt( p0 ), L );
	vec3 col = dofAt( V( p0 ), size );
	vec4 p12 = min( dir.xyxy * vec4( 0.25, 0.25, 0.5, 0.5 ) + v1.xyxy, mc1.xyxy );
	float w1 = tapW( mvAt( p12.xy ), L );
	float ws = w0 + w1;
	vec3 c1t = w1 * dofAt( V( p12.xy ), size );
	col = col * w0 + c1t;
	float w2 = tapW( mvAt( p12.zw ), L );
	ws = w2 + ws;
	col = dofAt( V( p12.zw ), size ) * w2 + col;
	vec2 p3 = min( dir * 0.75 + v1, mc1.xy );
	float w3 = tapW( mvAt( p3 ), L );
	ws = w3 + ws;
	col = dofAt( V( p3 ), size ) * w3 + col;
	ws = ws + 0.001;
	fragColor = vec4( col / ws, 1.0 );
}
