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

out vec4 fragColor;

#include "lookdev_output.glsl"
#include "lightning_field.glsl"

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
	if ( probe ) {
		fragColor = ndc.x < 0.0 ? vec4( studioOutput( c ), 1.0 ) : vec4( vec3( a ), 1.0 );
		return;
	}
	fragColor = vec4( studioOutput( c ), a );
}
