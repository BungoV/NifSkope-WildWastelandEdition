#version 410 core

/* lane VOLAIR1 (bungo 10-14): the sky through the air (src/features/air/cellvolair.h wwVolAirSkyDraw). The sky is a surface at
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
/* MODULES1 R7 night sky d (bungo 11-11, Foggy h22: "the sky is pitch black, why"): the share of the drawn sky that is the
 * weather record's night dome (atmo_sky.frag atRecNight.x: the master Off's night share; 0 with the master On, by day and
 * under the reds). The dome holds none of the physical sky's air, so over that share the backdrop takes the air's own
 * (L, T) once: not the residual T / tSky (a credit for air the dome does not hold) and not the horizon band's cap (the
 * physical sky's opaque air hiding a backdrop it is not in front of). fogNight (lookdev_fog.glsl): the saturated air's
 * night colour, the cover's light is at least that x its coverage. Red WW_NIGHTAIR_RED=1 (live): both 0, as before. */
uniform float volSkyDome;

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
	vec4 a = volSkyOn ? wwVolAirSky( wwSkyRay() ) : vec4( 0.0, 0.0, 0.0, 1.0 );
	vec4 air = a;	// the air alone
	if ( volSkyMode == 0 && wwApOn() && atSkyCamR > 0.0 ) {
		vec3 vr = wwSkyRay();
		float mu = dot( vr, viewMatrix[2] );	// world up: viewMatrix's columns are the world axes in view space
		float muH = -sqrt( max( 0.0, 1.0 - kRb * kRb / ( atSkyCamR * atSkyCamR ) ) );
		vec3 ts = atTrans( atSkyTransTex, atSkyCamR, max( mu, muH + 1e-4 ) );	// below the horizon: the horizon's
		if ( atApSkyRule == 3 && mu < muH ) {
			// MODULES1 item 18 (rule 3): a ray that meets the planet keeps the air's transmittance to the ground
			// (T(ground, up) / T(camera, up) along the reversed ray), not the horizon's ~0 that banded a high view black
			float dg = atDistGround( atSkyCamR, mu );
			float mg = clamp( ( atSkyCamR * mu + dg ) / kRb, -1.0, 0.0 );
			ts = min( atTrans( atSkyTransTex, kRb, -mg ) / max( atTrans( atSkyTransTex, atSkyCamR, -mu ), vec3( 1e-30 ) ), vec3( 1.0 ) );
		}
		float tSky = dot( ts, vec3( 1.0 / 3.0 ) );
		if ( atApSkyRule >= 1 && atApDouble <= 0.5 && volSkyOn ) {
			// lane HORIZONBAND1: the horizon band, coloured by the aerial perspective's far end along this ray (rule 1, now
			// the red WW_ATMO_AP_RED=apband). Lane FIX1101A (rule 2, shipped): by what a BLACK surface at the far plane
			// gets through the surfaces' own fog path (wwFog: the aerial perspective then the air's residual), the colour
			// the clipped far terrain under the band fades to. By day the two agree; at night the air carries the weather's
			// night ambient and the aerial perspective does not: rule 1 drew a dark flat stripe (spot 3 22:00, 18,19,21
			// between a 25,40,60 sky and a 36,59,90 far land, hard edges).
			vec4 ap = vec4( 0.0 );	// unused where the band's weight is 0 (wwSkyBandL mixes it by w)
			if ( atApSkyRule >= 2 ) {
				if ( wwSkyBandW( tSky ) > 0.0 ) {	// the fog path only in the band (~2 deg above the horizon and under it)
					float farV = projectionMatrix[3][2] / min( projectionMatrix[2][2] + 1.0, -1e-6 );	// the far plane (view units)
					float dV = abs( farV );
					// MODULES1 item 18 (rule 3): a ray that meets the planet ends there (km to units: / 1.42875e-5)
					if ( atApSkyRule == 3 && mu < muH )
						dV = min( dV, atDistGround( atSkyCamR, mu ) / 1.42875e-5 );
					ap = vec4( wwFog( vec3( 0.0 ), vr * dV ), 0.0 );
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
	if ( volSkyMode == 0 && volSkyOn && fogNight.a > 0.0 ) {	// MODULES1 R7 night sky d
		a = mix( a, air, volSkyDome );
		a.rgb = max( a.rgb, fogNight.rgb * ( 1.0 - a.a ) );
	}
	if ( volSkyMode == 1 ) {
		fragColor = vec4( volProbe == 1 ? a.rgb : vec3( a.a ), 1.0 );
		return;
	}
	fragColor = vec4( a.rgb, 1.0 - a.a );
}
