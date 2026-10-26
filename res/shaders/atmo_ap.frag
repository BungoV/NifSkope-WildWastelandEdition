#version 410 core

// lane SKYA (ATMO2): the aerial-perspective froxel volume (Hillaire 2020 sec. 5.4), one slice a draw. Per froxel the
// light the air sends toward the eye from the camera to the slice's distance (rgb, scene units: pi k L as the sky)
// and the transmittance over the same stretch (a, the mean of the three channels as Hillaire stores it). Slices are
// quadratic in distance: slice k's centre sits at maxKm ((k + 0.5) / n)^2. The same medium, the same transmittance
// and multiple-scattering LUTs as the sky-view: distant geometry takes exactly the sky's haze and in-scatter.

#include "atmo_common.glsl"

uniform sampler2D atTransTex;
uniform sampler2D atMsTex;
uniform vec2 atMsSize;
uniform vec3 atApDims;	// width, height, slices
uniform float atApSlice;	// this draw's slice index
uniform float atApMaxKm;
uniform vec3 atFrustum0;	// world ray (not unit) at the viewport's lower-left corner
uniform vec3 atFrustumX;	// its change across the viewport's width
uniform vec3 atFrustumY;	// and height
uniform float atCamR;	// km from the planet centre
uniform vec3 atSunW;	// world, TO the sun
uniform vec3 atMoonW;
uniform float atSunK;	// pi k for the sun (atmosphere.cpp)
uniform float atMoonK;	// 0 = no moon term
uniform float atMsOn;
uniform float atApNoSunT;	// red "nosunt": the in-scatter ignores the sun's transmittance (must fail the AP gate)
uniform float atTintOn;	// SKY2B (SKYTINT2): 1 = the Sky Color row's Weather Tint (0 under red WW_ATMO_TINT_RED=physical)
uniform vec3 atTint[17];	// the weather dome's linear color at 90 (i / 16)^2 degrees (gl/atmomath.h tintTable)

out vec4 fragColor;

vec3 atLightS( vec3 up, float rp, vec3 rs, vec3 ms, vec3 v, vec3 l, float k )
{
	float mus = dot( up, l );
	float c = dot( v, l );
	vec3 Ts = atApNoSunT > 0.5 ? vec3( 1.0 ) : atTrans( atTransTex, rp, mus );
	return k * ( Ts * ( rs * atPhaseR( c ) + ms * atPhaseHg( atG, c ) )
		+ atMsOn * texture( atMsTex, atMsUv( rp, mus, atMsSize ) ).rgb * ( rs + ms ) );
}

// atmo_sky.frag atWeatherTint's twin (gl/atmomath.h weatherTint): the luminance kept, the dome's color at this elevation
vec3 atWeatherTint( vec3 c, float elevDeg )
{
	float t = sqrt( clamp( elevDeg, 0.0, 90.0 ) / 90.0 ) * 16.0;
	int i = min( int( t ), 15 );
	vec3 w = mix( atTint[i], atTint[i + 1], t - float( i ) );
	const vec3 y = vec3( 0.2126, 0.7152, 0.0722 );
	float lw = dot( w, y );
	return lw > 1e-8 ? w * ( dot( c, y ) / lw ) : c;
}

void main()
{
	vec2 uv = gl_FragCoord.xy / atApDims.xy;
	vec3 v = normalize( atFrustum0 + uv.x * atFrustumX + uv.y * atFrustumY );
	float z = ( atApSlice + 0.5 ) / atApDims.z;
	float D = atApMaxKm * z * z;
	const int N = 16;
	vec3 thr = vec3( 1.0 ), L = vec3( 0.0 );
	float t0 = 0.0;
	for ( int i = 0; i < N; i++ ) {
		float a = float( i + 1 ) / float( N );
		float t1 = D * a * a;	// packed toward the camera: the haze layer can be ~100 m thick
		float dt = t1 - t0;
		float t = t0 + dt * 0.5;
		t0 = t1;
		vec3 p = vec3( v.xy * t, atCamR + v.z * t );
		float rp = length( p );
		vec3 up = p / rp;
		vec3 rs, ms, e;
		atMedium( rp - kRb, rs, ms, e );
		vec3 S = atLightS( up, rp, rs, ms, v, atSunW, atSunK );
		if ( atMoonK > 0.0 )
			S += atLightS( up, rp, rs, ms, v, atMoonW, atMoonK );
		vec3 Tst = exp( -e * dt );
		L += thr * S * ( 1.0 - Tst ) / max( e, vec3( 1e-9 ) );
		thr *= Tst;
	}
	if ( atTintOn > 0.5 )
		L = atWeatherTint( L, degrees( asin( clamp( v.z, -1.0, 1.0 ) ) ) );
	fragColor = vec4( L, dot( thr, vec3( 1.0 / 3.0 ) ) );
}
