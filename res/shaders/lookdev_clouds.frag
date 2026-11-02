#version 410 core

/* Lookdev cloud layer (lane PBRWX1, spec_clouds.md s2.4):
 * rgb = tex.rgb (sRGB-decoded) x vertex rgb x layer colour (linear) x SkyScale,
 * a = tex.a x vertex alpha x layer alpha; SRC_ALPHA / ONE_MINUS_SRC_ALPHA in
 * display space. probe: left half the shaded colour, right half the alpha as
 * grey (raw, no view transform). */

#include "uniforms.glsl"

uniform sampler2D Tex;
uniform sampler2D Tex2;	// lane WEATHERBLEND2: during a change Tex = the left weather's layer, Tex2 the new one's
uniform float texBlend;	// pct; 0 = Tex alone
uniform vec3 colLin;
uniform float alpha;
uniform float skyScale;
uniform bool probe;
uniform float sceneExposure;
uniform int viewTransform;

in vec4 vcol;
in vec2 uv;
in vec2 uv2;
in vec2 ndc;
/* lane LIGHTNING2 (LIGHTNINGSKY1): a storm's flash on the layer (Volume Clouds Off / Low), the volume's strike: the texel
 * is lit as the Low band's base where its direction meets the deck (ldLtCam.w km, one extinction length in), the field
 * of lightning_field.glsl x ldLtCol (the flash's colour x level x power, the units of colLin x skyScale). ldLtOn 0 (no
 * storm, between flashes, WW_LIGHTNING_SKY off / red) skips it: the layer as before. */
in vec3 wdir;
uniform int ldLtOn;
uniform vec4 ldLtSrc;
uniform vec4 ldLtPrm;
uniform vec3 ldLtCol;
uniform vec4 ldLtCam;	// the camera, world x, y km, height km; w = the deck's height km
/* lane WXBAND (WEATHER-HORIZON-BAND): the sky's aerial perspective to the deck, as cloud_march.frag's cvApOn:
 * tair = T(cam, mu) / T(deck, mu1) on the Low base's shell; drawn as alpha a x tm, colour c x tair / tm (tm = max
 * channel), i.e. a x tair x c + (1 - a x tm) x the sky behind. ldApOn 0 (red WW_LOOKDEV_RED=cloudnoap, no physical
 * sky this frame, camera above the deck): the layer as before */
uniform int ldApOn;
uniform sampler2D ldTrans;	// the sky's transmittance LUT
uniform vec2 ldApR;	// km from the planet's centre: the camera, the deck's base

out vec4 fragColor;

#include "lookdev_output.glsl"
#include "lightning_field.glsl"
#include "atmo_common.glsl"

void main()
{
	vec4 t = texture( Tex, uv );
	if ( texBlend > 0.0 )
		t = mix( t, texture( Tex2, uv2 ), texBlend );
	vec3 c = srgbToLinear( t.rgb ) * vcol.rgb * colLin * skyScale;
	if ( ldLtOn != 0 ) {
		vec3 d = normalize( wdir );
		float h = ldLtCam.w - ldLtCam.z;
		if ( d.z > 0.02 && h > 0.0 ) {
			vec3 p = vec3( ldLtCam.xy + d.xy * ( h / d.z ), ldLtCam.w );
			c += srgbToLinear( t.rgb ) * vcol.rgb * ldLtCol * ldLtField( ldLtSrc, ldLtPrm, p, d );
		}
	}
	float a = t.a * vcol.a * alpha;
	if ( ldApOn != 0 ) {
		vec3 d = normalize( wdir );
		float r0 = ldApR.x, R1 = ldApR.y, mu = d.z;
		float s = -r0 * mu + sqrt( max( 0.0, r0 * r0 * ( mu * mu - 1.0 ) + R1 * R1 ) );
		float r1 = sqrt( r0 * r0 + s * s + 2.0 * r0 * mu * s ), mu1 = ( r0 * mu + s ) / r1;
		vec3 tair = clamp( atTrans( ldTrans, r0, mu ) / max( atTrans( ldTrans, r1, mu1 ), vec3( 1e-6 ) ), 0.0, 1.0 );
		float tm = max( tair.r, max( tair.g, tair.b ) );
		c *= tair / max( tm, 1e-6 );
		a *= tm;
	}
	if ( probe ) {
		fragColor = ndc.x < 0.0 ? vec4( studioOutput( c ), 1.0 ) : vec4( vec3( a ), 1.0 );
		return;
	}
	fragColor = vec4( studioOutput( c ), a );
}
