#version 410 core

// lane WATERLOOK1 (src/gl/cellocean.cpp): one radix-2 Stockham stage of the inverse FFT (sign +, no 1 / N), N = 128, on
// every 128-texel row (axis 0) or column (axis 1) of the six tiles; four complex numbers a texel. Stage Ns = 1, 2 .. 64;
// output i = ( i / 2Ns ) Ns + i mod Ns = j:  y = x[ j ] + ( -1 )^( i / Ns ) e^( i pi ( i mod Ns ) / Ns ) x[ j + N / 2 ].
// Seven stages a direction leave the transform in natural order (Stockham's autosort).

uniform sampler2D oceanIn0;
uniform sampler2D oceanIn1;
uniform int oceanNs;
uniform int oceanAxis;

layout ( location = 0 ) out vec4 o0;
layout ( location = 1 ) out vec4 o1;

vec2 cmul( vec2 a, vec2 b )
{
	return vec2( a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x );
}

void main()
{
	ivec2 p = ivec2( gl_FragCoord.xy );
	int i = oceanAxis == 0 ? p.x & 127 : p.y;
	int k = i % oceanNs;
	int j = ( i / ( 2 * oceanNs ) ) * oceanNs + k;
	float sg = ( ( i / oceanNs ) & 1 ) != 0 ? -1.0 : 1.0;
	ivec2 a = oceanAxis == 0 ? ivec2( p.x - i + j, p.y ) : ivec2( p.x, j );
	ivec2 b = oceanAxis == 0 ? ivec2( a.x + 64, p.y ) : ivec2( p.x, j + 64 );
	float th = 3.14159265 * float( k ) / float( oceanNs );
	vec2 w = sg * vec2( cos( th ), sin( th ) );
	vec4 a0 = texelFetch( oceanIn0, a, 0 ), a1 = texelFetch( oceanIn1, a, 0 );
	vec4 b0 = texelFetch( oceanIn0, b, 0 ), b1 = texelFetch( oceanIn1, b, 0 );
	o0 = a0 + vec4( cmul( b0.xy, w ), cmul( b0.zw, w ) );
	o1 = a1 + vec4( cmul( b1.xy, w ), cmul( b1.zw, w ) );
}
