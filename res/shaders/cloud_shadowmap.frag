#version 410 core

/* lane CLOUD3: the cloud shadow map (src/gl/cloudshadow.cpp), a Beer shadow map (Hillaire 2016 sec. 5.5). One fragment =
 * one line toward the sun. Out: x = the height (km) where the cloud starts seen from the sun, y = the mean extinction per
 * km of height below it, z = the line's whole optical depth, w = 1 where any cloud was met. The receiver
 * (ww_sunshadow.glsl ussCloudT) takes OD = min(z, y x max(x - height, 0)).
 * csMode 1: the volume's density (cloud_common.glsl cloudDensity, bound by wwCloudVolBindDensity); 2: the cloud map's
 * coverage alone (the Low tier, or no volume): T = 1 - c (1 - exp(-tau)) per band. */

#include "cloud_common.glsl"

uniform vec3 csE1;	// light space, world: across the sun
uniform vec3 csE2;
uniform vec3 csS;	// TO the sun (z >= sin 3 deg)
uniform vec4 csFit;	// a0, b0 (km, the map's centre in light space), the side km, the texel km
uniform vec2 csSize;	// texels across
uniform int csMode;
uniform int csSteps;	// per band (volume mode)

out vec4 fragColor;

// the point of this texel's line at height h km
vec3 csLine( vec2 ab, float h )
{
	float lam = ( h - ab.y * csE2.z ) / csS.z;
	return ab.x * csE1 + ab.y * csE2 + lam * csS;
}

void main()
{
	vec2 uv = gl_FragCoord.xy / csSize;
	vec2 ab = csFit.xy + ( uv - 0.5 ) * csFit.z;
	// a clear line stores the lowest live base: a texel filtered with a cloudy one then never shadows a point above
	// the cloud's top (the highest top did, near every cloud edge)
	float floorKm = 1e4;
	for ( int b = 0; b < 2; b++ )
		if ( cvBands.density[b] > 0.0 && cvBands.topKm[b] > cvBands.baseKm[b] )
			floorKm = min( floorKm, cvBands.baseKm[b] );
	float od = 0.0, zTop = -1.0, zBot = 0.0;
	if ( csMode == 1 ) {
		cvFootKm = csFit.w;	// the noise mips at the map's texel
		for ( int b = 1; b >= 0; b-- ) {
			float base = cvBands.baseKm[b], top = cvBands.topKm[b];
			if ( cvBands.density[b] <= 0.0 || top <= base )
				continue;
			float dh = ( top - base ) / float( csSteps );
			for ( int i = 0; i < csSteps; i++ ) {
				float h = top - ( float( i ) + 0.5 ) * dh;
				float d = cloudDensity( csLine( ab, h ) );
				if ( d <= 0.0 )
					continue;
				if ( zTop < 0.0 )
					zTop = h + 0.5 * dh;
				zBot = h - 0.5 * dh;
				od += kCvSigma * d * dh / csS.z;
			}
		}
	} else {
		for ( int b = 1; b >= 0; b-- ) {
			float base = cvBands.baseKm[b], top = cvBands.topKm[b];
			if ( cvBands.density[b] <= 0.0 || top <= base )
				continue;
			vec4 m = cvMapAt( csLine( ab, 0.5 * ( base + top ) ).xy );
			float c = clamp( b == 0 ? m.r : m.g, 0.0, 1.0 );
			float tau = kCvSigma * cvBands.density[b] * 0.4 * ( top - base ) / csS.z;
			float T = 1.0 - c * ( 1.0 - exp( -tau ) );
			if ( T >= 1.0 )
				continue;
			if ( zTop < 0.0 )
				zTop = top;
			zBot = base;
			od += -log( max( T, 1e-4 ) );
		}
	}
	if ( zTop < 0.0 || od <= 0.0 ) {
		fragColor = vec4( floorKm < 1e4 ? floorKm : 0.0, 0.0, 0.0, 0.0 );
		return;
	}
	fragColor = vec4( zTop, od / max( zTop - zBot, 1e-3 ), od, 1.0 );
}
