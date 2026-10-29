// lane CLOUD5: the gate's probe of cloudDensity (res/shaders/cloud_common.glsl), one fragment = one point or column.
// cloudvar_probe.frag defines WW_CLOUD_VARIETY (the twin's density); cloudvar_probe_old.frag does not (CLOUD2's).
//   prMode 0: the density at prO + i prU + j prV (mip 0)
//   prMode 1: the column at xy = prO.xy + i prU.xy + j prV.xy over prZ: (opacity, top km, base km, optical depth); top /
//             base = the highest / lowest height whose density > prEdge (-1 = none)
//   prMode 2 (variety only): (weather noise, curl x, y, z) at the mode-0 point
#include "../../res/shaders/cloud_common.glsl"

uniform int prMode;
uniform vec3 prO;
uniform vec3 prU;
uniform vec3 prV;
uniform vec2 prZ;
uniform int prNZ;
uniform float prEdge;

out vec4 fragColor;

void main()
{
	vec2 ij = floor( gl_FragCoord.xy );
	vec3 p = prO + ij.x * prU + ij.y * prV;
	cvFootKm = 0.0;
	if ( prMode == 0 ) {
		fragColor = vec4( cloudDensity( p ) );
		return;
	}
#ifdef WW_CLOUD_VARIETY
	if ( prMode == 2 ) {
		fragColor = vec4( cvWeatherNoise( p.xy ), cvCurlRaw( p ) );
		return;
	}
#endif
	float dz = ( prZ.y - prZ.x ) / float( prNZ ), tau = 0.0, top = -1.0, bot = -1.0;
	for ( int k = 0; k < prNZ; k++ ) {
		float z = prZ.x + ( float( k ) + 0.5 ) * dz;
		float d = cloudDensity( vec3( p.xy, z ) );
		if ( d > prEdge ) {
			top = z;
			if ( bot < 0.0 )
				bot = z;
		}
		tau += d * kCvSigma * dz;
	}
	fragColor = vec4( 1.0 - exp( -tau ), top, bot, tau );
}
