#version 410 core
// lane FO76WEATHER2: res/shaders/rain_wet.glsl wwRainWetExposed on the real driver (tests/raincover/raincover_gpu.py)
#include "../../res/shaders/rain_wet.glsl"

in vec3 inP;
in vec3 inN;
out float vsE;

void main()
{
	vsE = wwRainWetExposed( inP, inN );
	gl_Position = vec4( 0.0 );
}
