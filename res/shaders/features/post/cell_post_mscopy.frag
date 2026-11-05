#version 410 core

/* lane POSTFX3: the linear post's picture back into a multisampled linear frame, every sample keeping its own value
 * where the blur left the pixel alone (src/features/post/cellpost.cpp 4b). Dual-source blend ONE, SRC1_COLOR (alpha ZERO, ONE):
 * sample s -> s * w + (B - w * R), R = the frame's resolve (the post's input), B = the picture, w = min(1, B / R) per
 * channel. Mean of the samples = B; none negative (B < R: s * B / R; else s + (B - R)); B == R: s exactly. */

uniform sampler2D srcTex;	// B, the picture
uniform sampler2D refTex;	// R, the frame as the post read it (its samples resolved)

layout ( location = 0, index = 0 ) out vec4 addOut;
layout ( location = 0, index = 1 ) out vec4 scaleOut;

void main()
{
	ivec2 p = ivec2( gl_FragCoord.xy );
	vec3 b = texelFetch( srcTex, p, 0 ).rgb;
	vec3 r = texelFetch( refTex, p, 0 ).rgb;
	// B >= R takes w = 1 by a compare, not B / R: a GPU's divide by itself is not always exactly 1
	vec3 w = vec3( b.r >= r.r || r.r <= 0.0 ? 1.0 : b.r / r.r, b.g >= r.g || r.g <= 0.0 ? 1.0 : b.g / r.g,
		b.b >= r.b || r.b <= 0.0 ? 1.0 : b.b / r.b );
	addOut = vec4( b - w * r, 0.0 );
	scaleOut = vec4( w, 0.0 );
}
