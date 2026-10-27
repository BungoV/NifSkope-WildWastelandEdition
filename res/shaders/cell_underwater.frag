#version 410 core

// lane WATERUNDER1: the camera-underwater fog over the linear frame (src/gl/cellunderwater.cpp draws it with
// glBlendFuncSeparate( GL_ONE, GL_SRC1_COLOR, GL_ZERO, GL_ONE ): frame = frame * mul + add, alpha kept).
// lane WATERUNDER4: with the frame's depth per sample (uwSamples > 0) this draw fogs only the samples near the
// pixel's farthest depth; its twin cell_underwater_ms.prog (WW_UW_SAMPLE) runs per sample and fogs the rest.

#include "water_under.glsl"
// lane WATERLOOK1 step 16 (WATERUNDER2): the Underwater light row, the clear colour's coefficients + the meniscus
#include "water_info.glsl"
#include "water_depth.glsl"
#include "water_clear.glsl"
#include "water_underlight.glsl"

uniform sampler2D uwDepth;	// R32F: the frame's farthest window depth per pixel (cellhdr.cpp wwCellHdrDepthMax)
uniform sampler2DMS uwDepthMs;	// lane WATERUNDER4: the frame's depth per sample (cellhdr.cpp wwCellHdrDepthMsBegin)
uniform int uwSamples;		// its samples; 0 = one fog per pixel from the farthest depth (one sample, or red maxdepth)
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

// a sample this near the farthest one takes its fog: the law moves under 1/512 over the gap
float uwEdgeTol()
{
	float t = min( max( uwFog.z - uwFog.y, 1e-3 ), max( uwFog.z, 1e-3 ) ) / ( 512.0 * max( uwFog.x, 1e-3 ) );
	if ( uwLight != 0 ) {
		vec3 bb;
		vec3 k = wwClearK( uwClass, bb );
		t = min( t, 1.0 / ( 1536.0 * max( max( k.r, max( k.g, k.b ) ), 1e-6 ) * kWwUnitMetres ) );
	}
	return t;
}

// the same answer in both draws (precise), so every sample is fogged exactly once
bool uwNearMax( vec2 ndc, float dMax, float dS, float tol )
{
	if ( ( dS >= 1.0 ) != ( dMax >= 1.0 ) )
		return false;
	precise vec3 a = uwWorld( ndc, dMax * 2.0 - 1.0 );
	precise vec3 b = uwWorld( ndc, dS * 2.0 - 1.0 );
	precise float e = distance( a, b );
	return e <= tol;
}

void main()
{
	float d = texelFetch( uwDepth, ivec2( gl_FragCoord.xy ), 0 ).r;
	vec2 ndc = ( gl_FragCoord.xy - uwVp.xy ) / uwVp.zw * 2.0 - 1.0;
#ifdef WW_UW_SAMPLE
	float dS = texelFetch( uwDepthMs, ivec2( gl_FragCoord.xy ), gl_SampleID ).r;
	if ( uwNearMax( ndc, d, dS, uwEdgeTol() ) )
		discard;
	d = dS;
#else
	int mask = -1;
	if ( uwSamples > 0 ) {
		float tol = uwEdgeTol();
		mask = 0;
		for ( int i = 0; i < uwSamples; i++ )
			if ( uwNearMax( ndc, d, texelFetch( uwDepthMs, ivec2( gl_FragCoord.xy ), i ).r, tol ) )
				mask |= 1 << i;
	}
	gl_SampleMask[0] = mask;
#endif
	vec3 Pn = uwWorld( ndc, -1.0 );
	vec3 Pw = uwWorld( ndc, d * 2.0 - 1.0 );
	vec3 add, mul;
	if ( uwLight != 0 && ( uwLightRed & 4 ) == 0 )
		wwUnderLight( uwEye, Pn, Pw, d >= 1.0, uwFog.w, add, mul );
	else
		wwUnderwater( uwEye, Pn, Pw, d >= 1.0, uwFog, uwColor, uwTint, uwRed,
			( uwRed & 4 ) != 0 ? vec3( 0.0 ) : -normalize( vec3( uwRow[0].z, uwRow[1].z, uwRow[2].z ) ), add, mul );
	float men = wwUnderMeniscus( Pn.z - uwFog.w, fwidth( Pn.z ) * uwMenK.x );	// 0 with the row off
	fragAdd = vec4( add * ( 1.0 - men ), 0.0 );
	fragMul = vec4( mul * ( 1.0 - men ), 1.0 );
}
