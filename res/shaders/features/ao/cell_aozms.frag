#version 410 core

/* RESUME 15: stage 6 of the cell view's obscurance (src/features/ao/ao.cpp wwCellAoPass): the depth pre-pass's linear
 * view depth (R32F, multisampled as the frame's depth) to one value a pixel, sample 0 as the depth resolve takes it, so
 * a silhouette pixel holds one surface's depth and never an average of two. */

uniform sampler2DMS zms;

out vec4 fragColor;

void main()
{
	fragColor = vec4( texelFetch( zms, ivec2( gl_FragCoord.xy ), 0 ).r );
}
