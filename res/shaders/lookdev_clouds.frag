#version 410 core

/* Lookdev cloud layer (lane PBRWX1, spec_clouds.md s2.4):
 * rgb = tex.rgb (sRGB-decoded) x vertex rgb x layer colour (linear) x SkyScale,
 * a = tex.a x vertex alpha x layer alpha; SRC_ALPHA / ONE_MINUS_SRC_ALPHA in
 * display space. probe: left half the shaded colour, right half the alpha as
 * grey (raw, no view transform). */

#include "uniforms.glsl"

uniform sampler2D Tex;
uniform vec3 colLin;
uniform float alpha;
uniform float skyScale;
uniform bool probe;
uniform float sceneExposure;
uniform int viewTransform;

in vec4 vcol;
in vec2 uv;
in vec2 ndc;

out vec4 fragColor;

#include "lookdev_output.glsl"

void main()
{
	vec4 t = texture( Tex, uv );
	vec3 c = srgbToLinear( t.rgb ) * vcol.rgb * colLin * skyScale;
	float a = t.a * vcol.a * alpha;
	if ( probe ) {
		fragColor = ndc.x < 0.0 ? vec4( studioOutput( c ), 1.0 ) : vec4( vec3( a ), 1.0 );
		return;
	}
	fragColor = vec4( studioOutput( c ), a );
}
