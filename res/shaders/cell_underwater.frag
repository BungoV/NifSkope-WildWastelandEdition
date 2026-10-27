#version 410 core

// lane WATERUNDER1: the camera-underwater fog over the linear frame (src/gl/cellunderwater.cpp draws it with
// glBlendFuncSeparate( GL_ONE, GL_SRC1_COLOR, GL_ZERO, GL_ONE ): frame = frame * mul + add, alpha kept).

#include "water_under.glsl"

uniform sampler2D uwDepth;	// R32F: the frame's farthest window depth per pixel (cellhdr.cpp wwCellHdrDepthMax)
uniform mat4 uwInvProj;		// clip -> view
uniform vec4 uwRow[3];		// view -> world rows
uniform vec4 uwVp;		// the viewport x, y, w, h
uniform vec3 uwEye;		// the camera, world
uniform vec4 uwFog;		// amount, near, far, surface height
uniform vec3 uwColor;
uniform vec3 uwTint;
uniform int uwRed;

layout ( location = 0, index = 0 ) out vec4 fragAdd;
layout ( location = 0, index = 1 ) out vec4 fragMul;

vec3 uwWorld( vec2 ndc, float z )
{
	vec4 v = uwInvProj * vec4( ndc, z, 1.0 );
	vec4 p = vec4( v.xyz / v.w, 1.0 );
	return vec3( dot( uwRow[0], p ), dot( uwRow[1], p ), dot( uwRow[2], p ) );
}

void main()
{
	float d = texelFetch( uwDepth, ivec2( gl_FragCoord.xy ), 0 ).r;
	vec2 ndc = ( gl_FragCoord.xy - uwVp.xy ) / uwVp.zw * 2.0 - 1.0;
	vec3 Pn = uwWorld( ndc, -1.0 );
	vec3 Pw = uwWorld( ndc, d * 2.0 - 1.0 );
	vec3 add, mul;
	wwUnderwater( uwEye, Pn, Pw, d >= 1.0, uwFog, uwColor, uwTint, uwRed, add, mul );
	fragAdd = vec4( add, 0.0 );
	fragMul = vec4( mul, 1.0 );
}
