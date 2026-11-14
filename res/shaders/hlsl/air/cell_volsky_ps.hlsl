// HLSL source of features/air/cell_volsky.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_FOGLEAK (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "AirBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
// inlined (no readable .hlsli yet): uniforms.glsl
static float4 gl_FragCoord;
static float gl_FragDepth;
/* lane VOLAIR1 (bungo 10-14): the sky through the air (src/features/air/cellvolair.h wwVolAirSkyDraw). The sky is a surface at
 * infinite distance; lookdev_fog.glsl wwVolAirSky gives the air's light L and transmittance T along its ray.
 *   volSkyMode 0  into the cell's linear frame, under everything the scene draws: the air's light with coverage 1 - T.
 *                 The resolve (cell_hdr.frag mode 2) lays it over the backdrop: the sky x T + the tone-mapped fog
 *                 colour at infinity L / (1 - T) x (1 - T), the colour far geometry fades to.
 *   volSkyMode 1  the gates' probes, raw, on the pixels no draw reached: WW_CELL_AIR_PROBE 1 (L) / 2 (T), and the far
 *                 gates' geometry probe (WW_LOOKDEV_FOGPROBE 9 / 10) marks the sky (1, 1, 1): the geometry writes B 0.
 *   volSkyOn      false under the red nosky: the sky without the air (T 1, L 0). */

// uniforms.glsl inlined here: its .hlsli has no readable half yet (lane HLSLSRC1)
cbuffer globalUniforms : register(b0, space0)
{
	row_major float3x3	viewMatrix : packoffset(c0);					// rotation part of view transform
	row_major float3x3	envMapRotation : packoffset(c3);				// view space to environment map
	row_major float4x4	projectionMatrix : packoffset(c6);
	float4	lightSourcePosition[3] : packoffset(c10);
	float4	lightSourceDiffuse[3] : packoffset(c13);
	float4	lightSourceAmbient : packoffset(c16);
	float	toneMapScale : packoffset(c17);				// 1.0 = full tone mapping
	float	brightnessScale : packoffset(c17.y);
	float	glowScale : packoffset(c17.z);
	float	glowScaleSRGB : packoffset(c17.w);
	int4	viewportDimensions : packoffset(c18);			// X, Y, width, height
	uint	doSkinning : packoffset(c19);
	int	sceneOptions : packoffset(c19.y);
	int	cubeBgndMipLevel : packoffset(c19.z);
	int	sfParallaxMaxSteps : packoffset(c19.w);
	float	sfParallaxScale : packoffset(c20);
	float	sfParallaxOffset : packoffset(c20.y);
	float	unusedUniform1 : packoffset(c20.z);
	float	unusedUniform2 : packoffset(c20.w);
};
// end of uniforms.glsl
#define WW_LOOSE_TYPES 1
#include "lookdev_fog.hlsli"
#include "../sky/atmo_common.hlsli"
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "lookdev_fog.hlsli"
#include "../sky/atmo_common.hlsli"
#undef WW_LOOSE_UNIFORMS
	int volSkyMode;
	int atApSkyRule;	// lane HORIZONBAND1: 1 the horizon band (atmo_skyband.glsl), 0 the red WW_ATMO_AP_RED=skyclamp
	float atSkyCamR;
	bool volSkyOn;
	/* MODULES1 R7 night sky d (bungo 11-11, Foggy h22: "the sky is pitch black, why"): the share of the drawn sky that is the
	 * weather record's night dome (atmo_sky.frag atRecNight.x: the master Off's night share; 0 with the master On, by day and
	 * under the reds). The dome holds none of the physical sky's air, so over that share the backdrop takes the air's own
	 * (L, T) once: not the residual T / tSky (a credit for air the dome does not hold) and not the horizon band's cap (the
	 * physical sky's opaque air hiding a backdrop it is not in front of). fogNight (lookdev_fog.glsl): the saturated air's
	 * night colour, the cover's light is at least that x its coverage. Red WW_NIGHTAIR_RED=1 (live): both 0, as before. */
	float volSkyDome;
	/* FARWATER1 round 2 (bungo b71: a deep blue strip over the sea's edge, gone with WW_ENDLESS_SEA=0): the endless sea pushes
	 * the far plane to its rim (glview.cpp: x 1.05, ~4.4 M u), and the band's far-plane point on a ray between eye level and
	 * the sea's dip then lay ~20000 u under the flat air frame's ground (deep blue). True while the sea draws: the point as the
	 * flat frame sees the curved earth (lookdev_fog.glsl wwAirFlatPos, the sea's own rule) and no column clamp. Red
	 * WW_ENDLESS_SEA_RED=air: false. */
	bool volSkySea;
};
#include "lookdev_fog.hlsli"
#include "../sky/atmo_common.hlsli"
#include "../sky/atmo_skyband.hlsli"


// lane SKYA (ATMO2): with the aerial perspective on, the physical sky behind already holds the air along its ray to
// space; the air over it keeps only its residual (atmo_apfog.glsl), the same rule the surfaces use
Texture2D<float4> atSkyTransTex : register(t31, space0); SamplerState _atSkyTransTex_sampler : register(s31, space0);

static float4 fragColor;

// the view-space unit ray through this fragment (a pass with no geometry: the sky's). Here, not in lookdev_fog.glsl:
// it reads uniforms.glsl's projectionMatrix, and cell_volair.frag includes the fog without that block (10-15: it
// stopped compiling, and the air was refused on every run)
float3 wwSkyRay()
{
	float2 ndc = ( gl_FragCoord.xy - volRect.xy ) * volRect.zw * 2.0 - 1.0;
	return normalize( float3( ( ndc.x + projectionMatrix[2][0] ) / projectionMatrix[0][0],
		( ndc.y + projectionMatrix[2][1] ) / projectionMatrix[1][1], -1.0 ) );
}

void frag_main()
{
	gl_FragDepth = 1.0;	// the probes' pass tests GL_LEQUAL against the frame: only the pixels at the far plane
	if ( or( !fogOn, !volOn ) ) {
		fragColor = (float4)( 0.0 );
		return;
	}
	if ( volSkyMode == 1 && fogProbe.z > 8.5 ) {
		fragColor = (float4)( 1.0 );
		return;
	}
	float4 a = volSkyOn ? wwVolAirSky( wwSkyRay() ) : float4( 0.0, 0.0, 0.0, 1.0 );
	float4 air = a;	// the air alone
	if ( volSkyMode == 0 && wwApOn() && atSkyCamR > 0.0 ) {
		float3 vr = wwSkyRay();
		float mu = dot( vr, viewMatrix[2] );	// world up: viewMatrix's columns are the world axes in view space
		float muH = -sqrt( max( 0.0, 1.0 - kRb * kRb / ( atSkyCamR * atSkyCamR ) ) );
		float3 ts = atTrans( atSkyTransTex, _atSkyTransTex_sampler, atSkyCamR, max( mu, muH + 1e-4 ) );	// below the horizon: the horizon's
		if ( atApSkyRule >= 3 && mu < muH ) {
			// MODULES1 item 18 (rule 3): a ray that meets the planet keeps the air's transmittance to the ground
			// (T(ground, up) / T(camera, up) along the reversed ray), not the horizon's ~0 that banded a high view black
			float dg = atDistGround( atSkyCamR, mu );
			float mg = clamp( ( atSkyCamR * mu + dg ) / kRb, -1.0, 0.0 );
			ts = min( atTrans( atSkyTransTex, _atSkyTransTex_sampler, kRb, -mg ) / max( atTrans( atSkyTransTex, _atSkyTransTex_sampler, atSkyCamR, -mu ), (float3)( 1e-30 ) ), (float3)( 1.0 ) );
		}
		float tSky = dot( ts, (float3)( 1.0 / 3.0 ) );
		if ( atApSkyRule >= 1 && atApDouble <= 0.5 && volSkyOn ) {
			// lane HORIZONBAND1: the horizon band, coloured by the aerial perspective's far end along this ray (rule 1, now
			// the red WW_ATMO_AP_RED=apband). Lane FIX1101A (rule 2, shipped): by what a BLACK surface at the far plane
			// gets through the surfaces' own fog path (wwFog: the aerial perspective then the air's residual), the colour
			// the clipped far terrain under the band fades to. By day the two agree; at night the air carries the weather's
			// night ambient and the aerial perspective does not: rule 1 drew a dark flat stripe (spot 3 22:00, 18,19,21
			// between a 25,40,60 sky and a 36,59,90 far land, hard edges).
			float4 ap = (float4)( 0.0 );	// unused where the band's weight is 0 (wwSkyBandL mixes it by w)
			float tFar = 1.0;	// MODULES1 round 10 (rule 4): the far path's transmittance; 1 = no air, no band
			if ( atApSkyRule >= 2 ) {
				if ( wwSkyBandW( tSky ) > 0.0 ) {	// the fog path only in the band (~2 deg above the horizon and under it)
					float farV = projectionMatrix[3][2] / min( projectionMatrix[2][2] + 1.0, -1e-6 );	// the far plane (view units)
					float dV = abs( farV );
					// MODULES1 item 18 (rule 3): a ray that meets the planet ends there (km to units: / 1.42875e-5)
					if ( atApSkyRule >= 3 && mu < muH )
						dV = min( dV, atDistGround( atSkyCamR, mu ) / 1.42875e-5 );
					float3 pFar = vr * dV;
					if ( volSkySea ) {
						pFar = wwAirFlatPos( pFar );
						wwAirFree = true;
					}
					ap = float4( wwFog( (float3)( 0.0 ), pFar ), 0.0 );
					if ( atApSkyRule >= 4 )	// wwFog is linear in the surface's colour on the air's path: white - black = T
						tFar = clamp( dot( wwFog( (float3)( 1.0 ), pFar ) - ap.rgb, (float3)( 1.0 / 3.0 ) ), 0.0, 1.0 );
				}
			} else {
				ap = wwAtmoApAt( vr * ( atAp.y / max( atAp.z, 1e-12 ) ) );
			}
			float tb = atApSkyRule >= 4 ? wwSkyBandT4( a.a, tSky, tFar ) : wwSkyBandT( a.a, tSky, 1 );
			a = float4( atApSkyRule >= 4 ? wwSkyBandL4( a.rgb, a.a, ap.rgb, tFar, tSky, tb ) : wwSkyBandL( a.rgb, a.a, ap.rgb, ap.a, tSky, tb, 1 ), tb );
		} else {
			float tr = wwApFogResidualT( tSky, a.a, atApDouble > 0.5 );
			a = float4( atApDouble > 0.5 ? a.rgb : wwApFogResidualL( a.rgb, a.a, tr ), tr );
		}
	}
	if ( volSkyMode == 0 && volSkyOn && fogNight.a > 0.0 ) {	// MODULES1 R7 night sky d
		a = lerp( a, air, volSkyDome );
		a.rgb = max( wwAirNightHue( a.rgb ), fogNight.rgb * ( 1.0 - a.a ) );	// FARWATER1 late 3: the night sky's tint
	}
	if ( volSkyMode == 1 ) {
		fragColor = float4( volProbe == 1 ? a.rgb : (float3)( a.a ), 1.0 );
		return;
	}
	fragColor = float4( a.rgb, 1.0 - a.a );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
    float gl_FragDepth : SV_Depth;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_FragDepth = gl_FragDepth;
    stage_output.fragColor = fragColor;
    return stage_output;
}
