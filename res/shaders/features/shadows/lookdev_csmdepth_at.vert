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

/* lane SHADOWSHIMMER2 (FARSWAYSHADOW): the far field's wind, fo4_default.vert's block op for op with the same uniforms
 * (gl/sunshadow.cpp sets them from the one per-paint sample cellfarlod.cpp's colour uniforms take; farSwayAmp 0 = none,
 * the red WW_SUN_SHADOW_STABLE_RED=sway). viewMatrix is the colour pass's own (globalUniforms). */
#include "uniforms.glsl"
uniform float farSwayAmp;
uniform float farSwayTime;
uniform vec4 farSwayBlock;
uniform bool lodTreeAnim;
layout ( location = 1 ) in vec4 vertexColor;

out vec2 texCoord;

void main()
{
	texCoord = multiTexCoord0;
	if ( farInst && farInstExtra.w > 1.5 )
		texCoord.x = farInstExtra.x - texCoord.x;
	vec4 v = vec4( vertexPosition, 1.0 );
	vec2 farXY = vertexPosition.xy;
	if ( farInst ) {
		v.xyz = vec3( dot( farInstRow0, v ), dot( farInstRow1, v ), dot( farInstRow2, v ) );
		farXY = v.xy;
	}
	v = modelViewMatrix * v;
	vec3 farRel = transpose( viewMatrix ) * ( v.xyz - farSwayBlock.xyz );	// world-axis offset from the block's centre
	if ( farSwayAmp > 0.0 && lodTreeAnim && max( abs( farRel.x ), abs( farRel.y ) ) > farSwayBlock.w ) {
		// world-horizontal offset, phase by the vertex's own place so a forest does not move as one
		float ph = dot( farXY, vec2( 0.0021, 0.0017 ) );
		vec3 w = vec3( sin( farSwayTime * 1.3 + ph ), cos( farSwayTime * 0.9 + ph * 1.7 ) * 0.6, 0.0 );
		v.xyz += viewMatrix * ( w * ( farSwayAmp * vertexColor.a ) );
	}
	gl_Position = csmClipFromView * v;
}
