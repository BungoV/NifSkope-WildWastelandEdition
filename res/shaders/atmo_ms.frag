#version 410 core

// lane ATMO1: the multiple-scattering LUT (Hillaire 2020 sec. 5.5): per (cos sun zenith, height), 64 directions of
// isotropic 2nd order + the ground, Psi = L2 / (1 - f_ms). "noms" red: the sky pass reads 0 instead.

#include "atmo_common.glsl"

uniform sampler2D atTransTex;
uniform vec2 atMsSize;

out vec4 fragColor;

void main()
{
	vec2 uv = gl_FragCoord.xy / atMsSize;
	float mus = atSubToUnit( uv.x, atMsSize.x ) * 2.0 - 1.0;
	float r = kRb + max( 0.01, atSubToUnit( uv.y, atMsSize.y ) * ( kRt - kRb ) );
	vec3 s = vec3( sqrt( max( 0.0, 1.0 - mus * mus ) ), 0.0, mus );
	const int K = 8;
	const int N = 20;
	vec3 L2 = vec3( 0.0 ), F = vec3( 0.0 );
	for ( int a = 0; a < K; a++ ) {
		for ( int b = 0; b < K; b++ ) {
			float th = 2.0 * kPi * ( float( a ) + 0.5 ) / float( K );
			float ph = acos( 1.0 - 2.0 * ( float( b ) + 0.5 ) / float( K ) );
			vec3 v = vec3( sin( ph ) * cos( th ), sin( ph ) * sin( th ), cos( ph ) );
			bool gnd = atHitsGround( r, v.z );
			float d = gnd ? atDistGround( r, v.z ) : atDistTop( r, v.z );
			float dt = d / float( N );
			vec3 thr = vec3( 1.0 ), L = vec3( 0.0 ), f = vec3( 0.0 );
			for ( int i = 0; i < N; i++ ) {
				float t = ( float( i ) + 0.5 ) * dt;
				vec3 p = vec3( v.xy * t, r + v.z * t );
				float rp = length( p );
				vec3 rs, ms, e;
				atMedium( rp - kRb, rs, ms, e );
				vec3 Ts = atTrans( atTransTex, rp, dot( p / rp, s ) );
				vec3 sc = rs + ms;
				vec3 Tst = exp( -e * dt );
				vec3 ie = 1.0 / max( e, vec3( 1e-9 ) );
				L += thr * ( Ts * sc / ( 4.0 * kPi ) ) * ( 1.0 - Tst ) * ie;
				f += thr * sc * ( 1.0 - Tst ) * ie;
				thr *= Tst;
			}
			if ( gnd ) {
				vec3 pg = vec3( v.xy * d, r + v.z * d );
				vec3 n = normalize( pg );
				float c = dot( n, s );
				L += thr * atTrans( atTransTex, kRb + 1e-4, c ) * max( c, 0.0 ) * kAlbedo / kPi;
			}
			L2 += L;
			F += f;
		}
	}
	L2 /= float( K * K );
	F /= float( K * K );
	fragColor = vec4( L2 / max( vec3( 1e-4 ), 1.0 - F ), 1.0 );
}
