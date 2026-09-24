#version 410 core

/* Lookdev weather sky dome (lane PBRWX1, spec_weather_sky.md s2.1):
 * linear = SkyScale * (r*Horizon + g*SkyLower + b*SkyUpper), each colour the
 * CIELab-blended NAM0 row to the power 2.2 (done on the CPU). Out through the
 * Studio output path. Not fogged (ruling). */

#include "uniforms.glsl"

uniform vec3 skyH;
uniform vec3 skyL;
uniform vec3 skyU;
uniform float skyScale;
uniform float alphaMul;
uniform float sceneExposure;
uniform int viewTransform;

in vec4 vcol;

out vec4 fragColor;

#include "lookdev_output.glsl"

void main()
{
	vec3 c = skyScale * ( vcol.r * skyH + vcol.g * skyL + vcol.b * skyU );
	fragColor = vec4( studioOutput( c ), alphaMul );
}
