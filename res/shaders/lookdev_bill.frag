#version 410 core

/* Lookdev sky billboard (lane PBRWX1): rgb = texture (sRGB-decoded here: the
 * sky DDS carry no DX10 header, so GL samples them raw) x tint (linear),
 * a = texture alpha x alpha. Blended in display space (no HDR yet, ruling). */

#include "uniforms.glsl"

uniform sampler2D Tex;
uniform vec3 tint;
uniform float alpha;
uniform float sceneExposure;
uniform int viewTransform;

in vec2 uv;

out vec4 fragColor;

#include "lookdev_output.glsl"

void main()
{
	vec4 t = texture( Tex, uv );
	fragColor = vec4( studioOutput( srgbToLinear( t.rgb ) * tint ), t.a * alpha );
}
