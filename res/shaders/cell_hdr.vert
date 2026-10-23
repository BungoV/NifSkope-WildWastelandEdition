#version 410 core

// lane PERF2: the tone map's full-screen quad at the far plane (window depth 1.0, as cell_hdr_fd.frag wrote per
// sample): no depth written in the fragment, so the early stencil and depth tests skip the pixels a mode does not take

layout ( location = 0 ) in vec3 vertexPosition;

void main()
{
	gl_Position = vec4( vertexPosition.xy, 1.0, 1.0 );
}
