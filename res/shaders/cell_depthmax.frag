#version 410 core

// lane PERF4: the linear frame's farthest depth per pixel (the max over its samples), for the far cards' depth reject
// (impostor_oct.frag, WW_CARDINST). Drawn by wwCellHdrDepthMax (src/gl/cellhdr.cpp) into an R32F target.

uniform sampler2DMS depthMs;
uniform int depthSamples;

out vec4 fragColor;

void main()
{
	ivec2 p = ivec2( gl_FragCoord.xy );
	float m = 0.0;
	for ( int i = 0; i < depthSamples; i++ )
		m = max( m, texelFetch( depthMs, p, i ).r );
	fragColor = vec4( m, 0.0, 0.0, 1.0 );
}
