#version 410 core
// lane FO76WEATHER2: res/shaders/features/weather/rain_wet.glsl wwRainWetExposed on the real driver (tests/features/weather/raincover/raincover_gpu.py)
#include "../../../../res/shaders/features/weather/rain_wet.glsl"

in vec3 inP;
in vec3 inN;
out float vsE;

void main()
{
	vsE = wwRainWetExposed( inP, inN );
	gl_Position = vec4( 0.0 );
}
