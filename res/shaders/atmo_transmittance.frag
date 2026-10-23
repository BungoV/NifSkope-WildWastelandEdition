#version 410 core

// lane ATMO1: the transmittance LUT (256x64): exp(-optical depth) to the top of the atmosphere

#include "atmo_common.glsl"

out vec4 fragColor;

void main()
{
	float r, mu;
	atTransRmu( gl_FragCoord.xy / kTransSize, r, mu );
	float d = atDistTop( r, mu );
	const int N = 40;
	vec3 od = vec3( 0.0 );
	for ( int i = 0; i < N; i++ ) {
		float t = ( float( i ) + 0.5 ) / float( N ) * d;
		float rr = sqrt( r * r + t * t + 2.0 * r * mu * t );
		vec3 rs, ms, e;
		atMedium( rr - kRb, rs, ms, e );
		od += e;
	}
	fragColor = vec4( exp( -od * d / float( N ) ), 1.0 );
}
