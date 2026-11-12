#version 430 core
layout(location = 0) in vec2 aIJ;
uniform sampler2D uHeight;
uniform mat4 uViewProj;
uniform vec2 uOrigin;
uniform float uSpacing;
out vec3 vWorld;
void main()
{
	float z = texelFetch( uHeight, ivec2( aIJ ), 0 ).r;
	vWorld = vec3( uOrigin + aIJ * uSpacing, z );
	gl_Position = uViewProj * vec4( vWorld, 1.0 );
}
