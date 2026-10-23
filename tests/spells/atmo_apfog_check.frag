#version 410 core
// lane SKYA (ATMO2): the offline gate's driver for res/shaders/atmo_apfog.glsl (tests/spells/atmo_lut_check.py,
// gate 5). Pixel x combines the aerial perspective apIn[x] (in-scatter, transmittance) with the fog fogIn[x].
#include "../../res/shaders/atmo_apfog.glsl"

uniform vec4 apIn[64];
uniform vec4 fogIn[64];
uniform int doubled;

out vec4 fragColor;

void main()
{
	int i = int( gl_FragCoord.x );
	fragColor = wwApFogCombine( apIn[i], fogIn[i], doubled != 0 );
}
