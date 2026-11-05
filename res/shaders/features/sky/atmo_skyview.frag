#version 410 core

// lane ATMO1: the sky-view LUT (Hillaire 2020 sec. 5.3), one light (the sun, or the moon on its own LUT):
// single scattering through the transmittance LUT + the multiple-scattering term (atmo_common.glsl atSkyMarch)

#include "atmo_common.glsl"

uniform sampler2D atTransTex;
uniform sampler2D atMsTex;
uniform vec2 atMsSize;
uniform vec2 atViewSize;
uniform float atCamR;	// km from the planet center
uniform float atLightMu;	// cos zenith of the light
uniform float atMsOn;	// 0 under the red "noms"
uniform float atGround;	// lane SKY2B: 1 = the lit ground below the horizon; 0 under the red "noground"

out vec4 fragColor;

void main()
{
	float cosZ, cosAz;
	atSkyUvToDir( gl_FragCoord.xy / atViewSize, atViewSize, atCamR, cosZ, cosAz );
	float sinZ = sqrt( max( 0.0, 1.0 - cosZ * cosZ ) );
	vec3 v = vec3( sinZ * cosAz, sinZ * sqrt( max( 0.0, 1.0 - cosAz * cosAz ) ), cosZ );
	vec3 s = vec3( sqrt( max( 0.0, 1.0 - atLightMu * atLightMu ) ), 0.0, atLightMu );
	vec3 L = atSkyMarch( atTransTex, atMsTex, atMsSize, atMsOn, atGround, atCamR, v, s );
	fragColor = vec4( L, 1.0 );
}
