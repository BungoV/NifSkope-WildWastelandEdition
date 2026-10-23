#version 410 core

// lane ATMO1: the sky-view LUT (Hillaire 2020 sec. 5.3), one light (the sun, or the moon on its own LUT):
// single scattering through the transmittance LUT + the multiple-scattering term, 30 quadratic steps

#include "atmo_common.glsl"

uniform sampler2D atTransTex;
uniform sampler2D atMsTex;
uniform vec2 atMsSize;
uniform vec2 atViewSize;
uniform float atCamR;	// km from the planet center
uniform float atLightMu;	// cos zenith of the light
uniform float atMsOn;	// 0 under the red "noms"

out vec4 fragColor;

void main()
{
	float cosZ, cosAz;
	atSkyUvToDir( gl_FragCoord.xy / atViewSize, atViewSize, atCamR, cosZ, cosAz );
	float sinZ = sqrt( max( 0.0, 1.0 - cosZ * cosZ ) );
	vec3 v = vec3( sinZ * cosAz, sinZ * sqrt( max( 0.0, 1.0 - cosAz * cosAz ) ), cosZ );
	vec3 s = vec3( sqrt( max( 0.0, 1.0 - atLightMu * atLightMu ) ), 0.0, atLightMu );
	float r = atCamR;
	bool gnd = atHitsGround( r, v.z );
	float d = gnd ? atDistGround( r, v.z ) : atDistTop( r, v.z );
	float cvs = dot( v, s );
	float pr = atPhaseR( cvs ), pm = atPhaseHg( atG, cvs );
	const int N = 30;
	vec3 thr = vec3( 1.0 ), L = vec3( 0.0 );
	float t0 = 0.0;
	for ( int i = 0; i < N; i++ ) {
		float a = float( i + 1 ) / float( N );
		float t1 = d * a * a;
		float dt = t1 - t0;
		float t = t0 + dt * 0.5;
		t0 = t1;
		vec3 p = vec3( v.xy * t, r + v.z * t );
		float rp = length( p );
		vec3 up = p / rp;
		float mus = dot( up, s );
		vec3 rs, ms, e;
		atMedium( rp - kRb, rs, ms, e );
		vec3 Ts = atTrans( atTransTex, rp, mus );
		vec3 S = Ts * ( rs * pr + ms * pm )
			+ atMsOn * texture( atMsTex, atMsUv( rp, mus, atMsSize ) ).rgb * ( rs + ms );
		vec3 Tst = exp( -e * dt );
		L += thr * S * ( 1.0 - Tst ) / max( e, vec3( 1e-9 ) );
		thr *= Tst;
	}
	fragColor = vec4( L, 1.0 );
}
