#version 410 core

// lane LIGHTNING2 test: a full-screen quad for res/shaders/lookdev_clouds.frag, with the varyings lookdev_clouds.vert
// hands it; wdir = the camera ray of the pixel (tCam columns right, up, back; tProj P00, P11, P20, P21)

layout ( location = 0 ) in vec3 vertexPosition;

uniform mat3 tCam;
uniform vec4 tProj;

out vec4 vcol;
out vec2 uv;
out vec2 uv2;
out vec2 ndc;
out vec3 wdir;

void main()
{
	vec2 q = vertexPosition.xy;
	gl_Position = vec4( q, 0.0, 1.0 );
	vcol = vec4( 1.0 );
	uv = q * 0.5 + 0.5;
	uv2 = uv;
	ndc = vec2( 0.0 );
	wdir = tCam * vec3( ( q.x + tProj.z ) / tProj.x, ( q.y + tProj.w ) / tProj.y, -1.0 );
}
