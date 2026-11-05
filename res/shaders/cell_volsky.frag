#version 410 core

/* lane VOLAIR1 (bungo 10-14): the sky through the air (src/gl/cellvolair.h wwVolAirSkyDraw). The sky is a surface at
 * infinite distance; lookdev_fog.glsl wwVolAirSky gives the air's light L and transmittance T along its ray.
 *   volSkyMode 0  into the cell's linear frame, under everything the scene draws: the air's light with coverage 1 - T.
 *                 The resolve (cell_hdr.frag mode 2) lays it over the backdrop: the sky x T + the tone-mapped fog
 *                 colour at infinity L / (1 - T) x (1 - T), the colour far geometry fades to.
 *   volSkyMode 1  the gates' probes, raw, on the pixels no draw reached: WW_CELL_AIR_PROBE 1 (L) / 2 (T), and the far
 *                 gates' geometry probe (WW_LOOKDEV_FOGPROBE 9 / 10) marks the sky (1, 1, 1): the geometry writes B 0.
 *   volSkyOn      false under the red nosky: the sky without the air (T 1, L 0). */

#include "uniforms.glsl"
#include "lookdev_fog.glsl"
#include "atmo_common.glsl"
#include "atmo_skyband.glsl"

uniform int volSkyMode;
uniform int atApSkyRule;	// lane HORIZONBAND1: 1 the horizon band (atmo_skyband.glsl), 0 the red WW_ATMO_AP_RED=skyclamp
// lane SKYA (ATMO2): with the aerial perspective on, the physical sky behind already holds the air along its ray to
// space; the air over it keeps only its residual (atmo_apfog.glsl), the same rule the surfaces use
uniform sampler2D atSkyTransTex;
uniform float atSkyCamR;
uniform bool volSkyOn;
// lane MUSGI1 r5: a sunlit interior's sky ray (cellvolair.cpp): the air to where it leaves the cell's box, world game units
uniform bool volSkyRoof;
uniform vec3 volSkyBoxLo;
uniform vec3 volSkyBoxHi;
uniform vec3 volSkyEye;

out vec4 fragColor;

// the view-space unit ray through this fragment (a pass with no geometry: the sky's). Here, not in lookdev_fog.glsl:
// it reads uniforms.glsl's projectionMatrix, and cell_volair.frag includes the fog without that block (10-15: it
// stopped compiling, and the air was refused on every run)
vec3 wwSkyRay()
{
	vec2 ndc = ( gl_FragCoord.xy - volRect.xy ) * volRect.zw * 2.0 - 1.0;
	return normalize( vec3( ( ndc.x + projectionMatrix[2][0] ) / projectionMatrix[0][0],
		( ndc.y + projectionMatrix[2][1] ) / projectionMatrix[1][1], -1.0 ) );
}

// (L, T) of the air from the eye to where the ray leaves the cell's box: the froxels to that distance, no tail past it
vec4 wwVolAirSkyRoof( vec3 dirView )
{
	if ( any( lessThan( volSkyEye, volSkyBoxLo ) ) || any( greaterThan( volSkyEye, volSkyBoxHi ) ) )
		return wwVolAirSky( dirView );	// the eye outside the box: the open-air ray
	vec3 w = transpose( viewMatrix ) * dirView;	// the world direction
	vec3 t = vec3( 1e9 );
	for ( int k = 0; k < 3; k++ )
		if ( abs( w[k] ) > 1e-6 )
			t[k] = ( ( w[k] > 0.0 ? volSkyBoxHi[k] : volSkyBoxLo[k] ) - volSkyEye[k] ) / w[k];
	float d = max( min( t.x, min( t.y, t.z ) ), 0.0 );
	vec2 uv = ( gl_FragCoord.xy - volRect.xy ) * volRect.zw;
	vec3 posView = dirView * ( d / max( fogDistScale, 1e-12 ) );
	if ( volFar.w > 0.5 )
		return wwVolAirAtFar( d, uv, posView );
	if ( d > volSlices.x * exp( volSlices.y ) )
		return wwVolAirAtUv( posView, uv );
	if ( d <= volSlices.x )
		return vec4( 0.0, 0.0, 0.0, 1.0 );
	float f = log( d / volSlices.x ) / volSlices.y * volSlices.z;
	vec4 v;
	if ( f < 1.0 ) {
		v = wwVolTex( vec3( uv, 0.5 / volSlices.z ), true );
		v = vec4( v.rgb * f, 1.0 - ( 1.0 - v.a ) * f );
	} else {
		v = wwVolTex( vec3( uv, ( f - 0.5 ) / volSlices.z ), false );
	}
	return vec4( max( v.rgb, vec3( 0.0 ) ), clamp( v.a, 0.0, 1.0 ) );
}

void main()
{
	gl_FragDepth = 1.0;	// the probes' pass tests GL_LEQUAL against the frame: only the pixels at the far plane
	if ( !fogOn || !volOn ) {
		fragColor = vec4( 0.0 );
		return;
	}
	if ( volSkyMode == 1 && fogProbe.z > 8.5 ) {
		fragColor = vec4( 1.0 );
		return;
	}
	vec4 a = volSkyOn ? ( volSkyRoof ? wwVolAirSkyRoof( wwSkyRay() ) : wwVolAirSky( wwSkyRay() ) ) : vec4( 0.0, 0.0, 0.0, 1.0 );
	if ( volSkyMode == 0 && wwApOn() && atSkyCamR > 0.0 ) {
		vec3 vr = wwSkyRay();
		float mu = dot( vr, viewMatrix[2] );	// world up: viewMatrix's columns are the world axes in view space
		float muH = -sqrt( max( 0.0, 1.0 - kRb * kRb / ( atSkyCamR * atSkyCamR ) ) );
		vec3 ts = atTrans( atSkyTransTex, atSkyCamR, max( mu, muH + 1e-4 ) );	// below the horizon: the horizon's
		float tSky = dot( ts, vec3( 1.0 / 3.0 ) );
		if ( atApSkyRule >= 1 && atApDouble <= 0.5 && volSkyOn ) {
			// lane HORIZONBAND1: the horizon band, coloured by the aerial perspective's far end along this ray (rule 1, now
			// the red WW_ATMO_AP_RED=apband). Lane FIX1101A (rule 2, shipped): by what a BLACK surface at the far plane
			// gets through the surfaces' own fog path (wwFog: the aerial perspective then the air's residual), the colour
			// the clipped far terrain under the band fades to. By day the two agree; at night the air carries the weather's
			// night ambient and the aerial perspective does not: rule 1 drew a dark flat stripe (spot 3 22:00, 18,19,21
			// between a 25,40,60 sky and a 36,59,90 far land, hard edges).
			vec4 ap = vec4( 0.0 );	// unused where the band's weight is 0 (wwSkyBandL mixes it by w)
			if ( atApSkyRule == 2 ) {
				if ( wwSkyBandW( tSky ) > 0.0 ) {	// the fog path only in the band (~2 deg above the horizon and under it)
					float farV = projectionMatrix[3][2] / min( projectionMatrix[2][2] + 1.0, -1e-6 );	// the far plane (view units)
					ap = vec4( wwFog( vec3( 0.0 ), vr * abs( farV ) ), 0.0 );
				}
			} else {
				ap = wwAtmoApAt( vr * ( atAp.y / max( atAp.z, 1e-12 ) ) );
			}
			float tb = wwSkyBandT( a.a, tSky, 1 );
			a = vec4( wwSkyBandL( a.rgb, a.a, ap.rgb, ap.a, tSky, tb, 1 ), tb );
		} else {
			float tr = wwApFogResidualT( tSky, a.a, atApDouble > 0.5 );
			a = vec4( atApDouble > 0.5 ? a.rgb : wwApFogResidualL( a.rgb, a.a, tr ), tr );
		}
	}
	if ( volSkyMode == 1 ) {
		fragColor = vec4( volProbe == 1 ? a.rgb : vec3( a.a ), 1.0 );
		return;
	}
	fragColor = vec4( a.rgb, 1.0 - a.a );
}
