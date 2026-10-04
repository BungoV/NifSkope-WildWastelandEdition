#version 410 core

// lane MOTION1: the temporal AA's picture back into the frame, texel for texel (src/gl/gametaa.h)

uniform sampler2D srcTex;

out vec4 fragColor;

void main()
{
	fragColor = texelFetch( srcTex, ivec2( gl_FragCoord.xy ), 0 );
}
