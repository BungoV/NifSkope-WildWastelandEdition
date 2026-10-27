#version 410 core

/* The cascade caster pass, alpha-tested shapes (lane VOLAIR1): lookdev_csmdepth.vert plus the
 * shape's first UV set, in the renderer's location 7. */

uniform mat4 csmClipFromView;
uniform mat4 modelViewMatrix;

layout ( location = 0 ) in vec3 vertexPosition;

// lane FARDRAW1: an instanced far ring's placement rows (fo4_default.vert); false = every other caster
uniform bool farInst;
layout ( location = 10 ) in vec4 farInstRow0;
layout ( location = 11 ) in vec4 farInstRow1;
layout ( location = 12 ) in vec4 farInstRow2;
layout ( location = 13 ) in vec4 farInstExtra;
layout ( location = 7 ) in vec2 multiTexCoord0;

out vec2 texCoord;

void main()
{
	texCoord = multiTexCoord0;
	if ( farInst && farInstExtra.w > 1.5 )
		texCoord.x = farInstExtra.x - texCoord.x;
	vec4 v = vec4( vertexPosition, 1.0 );
	if ( farInst )
		v.xyz = vec3( dot( farInstRow0, v ), dot( farInstRow1, v ), dot( farInstRow2, v ) );
	gl_Position = csmClipFromView * ( modelViewMatrix * v );
}
