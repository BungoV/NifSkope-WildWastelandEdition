#version 410 core

/* The octahedral impostor card, vertex stage, INSTANCED (lane FARCARD1, 2026-10-22).
 *
 * impostor_oct.vert with its per-card uniforms read from one buffer instead: the shipped-game impostor draw
 * (one instanced draw per card set, per-instance data in a buffer). The CPU still picks the frames and builds
 * the billboard (src/gl/impostordraw.cpp, the same function the per-card path uses), so every number here is
 * the float the old uniform held; the expressions below are impostor_oct.vert's, character for character.
 *
 * cardData, 6 texels a card (cardBase + gl_InstanceID):
 *   0 centre.xyz, depthSpan      1 half.xy, swayPhase, scale      2 right.xyz, frameCount
 *   3 up.xyz, frame 0            4 fwd.xyz, frame 1               5 weight 0..2, frame 2
 * cardFrames, 4 texels a frame of the set (frameTableBase + frame * 4):
 *   0 rect (u0 v0 du dv)   1 right.xyz, offset right   2 up.xyz, offset up   3 fwd.xyz, 0
 */

#include "uniforms.glsl"

uniform mat4 modelViewMatrix;

uniform samplerBuffer cardData;
uniform samplerBuffer cardFrames;
uniform int   cardBase;          // this draw's first card in cardData
uniform int   frameTableBase;    // this set's first texel in cardFrames
uniform vec3  cardEyeModel;      // the camera's position in model space
uniform bool  cardOrtho;         // true: no eye point, the ray is cardFwd

layout ( location = 0 ) in vec3 vertexPosition;   // the unit quad, -1..1 in xy
layout ( location = 7 ) in vec2 multiTexCoord0;   // 0..1 across the quad

out vec3 cardPosModel;
out vec2 cardQuadUv;
out vec3 cardViewRay;

// what impostor_oct.frag reads as uniforms on the per-card path
flat out vec4  frameRect[3];
flat out vec3  frameWeightV;
flat out vec3  frameRight[3];
flat out vec3  frameUp[3];
flat out vec3  frameFwd[3];
flat out int   frameCount;
flat out vec2  frameOffset[3];
flat out vec3  cardCenter;
flat out vec2  cardHalf;
flat out float cardDepthSpan;
flat out float swayPhase;

void main()
{
	int base = ( cardBase + gl_InstanceID ) * 6;
	vec4 t0 = texelFetch( cardData, base );
	vec4 t1 = texelFetch( cardData, base + 1 );
	vec4 t2 = texelFetch( cardData, base + 2 );
	vec4 t3 = texelFetch( cardData, base + 3 );
	vec4 t4 = texelFetch( cardData, base + 4 );
	vec4 t5 = texelFetch( cardData, base + 5 );

	vec3 inCenter = t0.xyz;
	vec2 inHalf = t1.xy;
	vec3 inRight = t2.xyz;
	vec3 inUp = t3.xyz;
	vec3 inFwd = t4.xyz;
	float scale = t1.w;

	cardCenter = inCenter;
	cardHalf = inHalf;
	cardDepthSpan = t0.w;
	swayPhase = t1.z;
	frameCount = int( t2.w );
	frameWeightV = t5.xyz;
	float frames[3] = float[3]( t3.w, t4.w, t5.w );
	for ( int k = 0; k < 3; k++ ) {
		int f = frameTableBase + int( frames[k] ) * 4;
		vec4 f1 = texelFetch( cardFrames, f + 1 );
		vec4 f2 = texelFetch( cardFrames, f + 2 );
		frameRect[k] = texelFetch( cardFrames, f );
		frameRight[k] = f1.xyz;
		frameUp[k] = f2.xyz;
		frameFwd[k] = texelFetch( cardFrames, f + 3 ).xyz;
		// the per-card path multiplied on the CPU: one float multiply either way
		frameOffset[k] = vec2( f1.w * scale, f2.w * scale );
	}

	// impostor_oct.vert from here, with its uniforms as the locals above
	vec3 p = inCenter
		+ inRight * ( vertexPosition.x * inHalf.x )
		+ inUp    * ( vertexPosition.y * inHalf.y );

	cardPosModel = p;
	cardQuadUv   = multiTexCoord0;

	bool orthoCam = cardOrtho || abs( projectionMatrix[2][3] ) < 1.0e-6;
	cardViewRay = orthoCam ? -inFwd : normalize( p - cardEyeModel );

	gl_Position = projectionMatrix * ( modelViewMatrix * vec4( p, 1.0 ) );
}
