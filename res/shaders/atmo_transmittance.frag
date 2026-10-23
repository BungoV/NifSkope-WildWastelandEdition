#version 410 core

// lane ATMO1: the transmittance LUT (256x64): exp(-optical depth) to the top of the atmosphere

#include "atmo_common.glsl"

out vec4 fragColor;

void main()
{
	float r, mu;
	atTransRmu( gl_FragCoord.xy / kTransSize, r, mu );
	float d = atDistTop( r, mu );
	// two segments meeting at the ray's lowest point, samples packed toward it: the fog's haze layer can be ~100 m
	// thick on a 600 km grazing ray (uniform steps missed it: tests/spells/atmo_lut_check.py transmittance-haze)
	float tm = clamp( -r * mu, 0.0, d );
	const int N = 40;
	vec3 od = vec3( 0.0 );
	for ( int i = 0; i < N; i++ ) {
		float a0 = float( i ) / float( N ), a1 = float( i + 1 ) / float( N ), am = ( a0 + a1 ) * 0.5;
		// segment A [0, tm]: t = tm (1 - (1 - a)^2); segment B [tm, d]: t = tm + (d - tm) a^2
		float tA = tm * ( 1.0 - ( 1.0 - am ) * ( 1.0 - am ) ), wA = tm * ( ( 1.0 - a0 ) * ( 1.0 - a0 ) - ( 1.0 - a1 ) * ( 1.0 - a1 ) );
		float tB = tm + ( d - tm ) * am * am, wB = ( d - tm ) * ( a1 * a1 - a0 * a0 );
		vec3 rs, ms, e;
		float rr = sqrt( r * r + tA * tA + 2.0 * r * mu * tA );
		atMedium( rr - kRb, rs, ms, e );
		od += e * wA;
		rr = sqrt( r * r + tB * tB + 2.0 * r * mu * tB );
		atMedium( rr - kRb, rs, ms, e );
		od += e * wB;
	}
	fragColor = vec4( exp( -od ), 1.0 );
}
