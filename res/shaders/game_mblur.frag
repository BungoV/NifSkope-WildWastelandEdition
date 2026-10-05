#version 410 core

/* lane GRASSMB1: the game's motion blur, instruction for instruction (src/gl/cellpost.h). The pass works in the
 * game's texture space (v down): the pixel, the vectors (y turned over) and c1's corner are taken there, every
 * tap is read back through V(). t0 the frame, t1 the vectors (previous minus current, texture units);
 * c0 = (scale x 0.001 / dt, max blur, threshold, 0), c1 = (1, 1, (W - 1) / W, (H - 1) / H). */

uniform sampler2D t0;
uniform sampler2D t1;
uniform vec4 c0;
uniform vec4 c1;

out vec4 fragColor;

float sat( float v )	// D3D _sat: NaN -> 0
{
	return v >= 0.0 ? min( v, 1.0 ) : 0.0;
}

vec2 V( vec2 d )	// game texture space -> this view's
{
	return vec2( d.x, 1.0 - d.y );
}

vec2 mvAt( vec2 d )
{
	vec2 m = texture( t1, V( d ) ).xy;
	return vec2( m.x, -m.y );
}

float tapW( vec2 m, float L )	// dp2, sqrt, add, mad_sat
{
	float l = sqrt( dot( m, m ) );
	return sat( -abs( L - l ) * c0.z + 1.0 );
}

void main()
{
	vec2 g = gl_FragCoord.xy / vec2( textureSize( t0, 0 ) );
	vec2 v1 = V( g );
	vec2 m = mvAt( v1 );
	float l2 = dot( m, m );
	float L = sqrt( l2 );
	vec2 dc = v1 - c1.zw;
	float rr = sqrt( dot( dc, dc ) );
	rr = sat( rr * 0.1 + -0.018 );
	rr = L * rr;
	if ( rr < 0.001 ) {
		fragColor = texture( t0, g );
		return;
	}
	vec2 dir = inversesqrt( l2 ) * m;
	float len = min( rr, c0.y );
	dir = len * dir;
	dir = dir * c0.x;

	vec2 p0 = min( v1, c1.xy );
	float w0 = tapW( mvAt( p0 ), L );
	vec3 col = texture( t0, V( p0 ) ).rgb;
	vec4 p12 = min( dir.xyxy * vec4( 0.25, 0.25, 0.5, 0.5 ) + v1.xyxy, c1.xyxy );
	float w1 = tapW( mvAt( p12.xy ), L );
	float ws = w0 + w1;
	vec3 c1t = w1 * texture( t0, V( p12.xy ) ).rgb;
	col = col * w0 + c1t;
	float w2 = tapW( mvAt( p12.zw ), L );
	ws = w2 + ws;
	col = texture( t0, V( p12.zw ) ).rgb * w2 + col;
	vec2 p3 = min( dir * 0.75 + v1, c1.xy );
	float w3 = tapW( mvAt( p3 ), L );
	ws = w3 + ws;
	col = texture( t0, V( p3 ) ).rgb * w3 + col;
	ws = ws + 0.001;
	fragColor = vec4( col / ws, 1.0 );
}
