#version 410 core

out vec3 LightDir;
out vec3 ViewDir;

out vec2 texCoord;

out mat3 btnMatrix;

flat out vec4 A;
out vec4 C;
flat out vec4 D;
// the vertex colour's OWN alpha, before vertexColorOverride forces it to 1 on a tree-
// animation shape: the wind weight W the impostor bake reads (channel 11 G, CARDFIX1 step 6)
out float rawVertexAlpha;

flat out mat3 reflMatrix;

#include "uniforms.glsl"

uniform mat3 normalMatrix;			// in row-major order
uniform mat4 modelViewMatrix;

uniform vec4 vertexColorOverride;	// components greater than zero replace the vertex color

// lane FARLOD1: the far field's wind. 0 (the default, and whenever the Far LOD row is off) = no sway;
// a tree-animation shape's vertex alpha is the weight, as in the game
uniform float farSwayAmp;
uniform float farSwayTime;
uniform bool lodTreeAnim;

layout ( location = 0 ) in vec3	vertexPosition;
layout ( location = 1 ) in vec4	vertexColor;
layout ( location = 2 ) in vec3	normalVector;
layout ( location = 3 ) in vec3	tangentVector;
layout ( location = 4 ) in vec3	bitangentVector;
layout ( location = 7 ) in vec2	multiTexCoord0;

#include "bonetransform.glsl"

void main()
{
	vec4	v = vec4( vertexPosition, 1.0 );
	vec3	n = normalVector;
	vec3	t = tangentVector;
	vec3	b = bitangentVector;

	if ( boneWeights[0].x > 0.0 && doSkinning )
		boneTransform( v, n, t, b );

	v = modelViewMatrix * v;
	if ( farSwayAmp > 0.0 && lodTreeAnim ) {
		// world-horizontal offset, phase by the vertex's own place so a forest does not move as one
		float ph = dot( vertexPosition.xy, vec2( 0.0021, 0.0017 ) );
		vec3 w = vec3( sin( farSwayTime * 1.3 + ph ), cos( farSwayTime * 0.9 + ph * 1.7 ) * 0.6, 0.0 );
		v.xyz += viewMatrix * ( w * ( farSwayAmp * vertexColor.a ) );
	}
	gl_Position = projectionMatrix * v;
	texCoord = multiTexCoord0;

	btnMatrix[2] = normalize( n * normalMatrix );
	btnMatrix[1] = normalize( t * normalMatrix );
	btnMatrix[0] = normalize( b * normalMatrix );

	reflMatrix = envMapRotation;
	reflMatrix[0][2] *= -1.0;
	reflMatrix[1][2] *= -1.0;
	reflMatrix[2][2] *= -1.0;

	if ( projectionMatrix[3][3] == 1.0 )
		ViewDir = vec3(0.0, 0.0, 1.0);	// orthographic view
	else
		ViewDir = -v.xyz;
	LightDir = lightSourcePosition[0].xyz;

	A = vec4( sqrt(lightSourceAmbient.rgb) * 0.375, toneMapScale );
	C = mix( vertexColor, vertexColorOverride, greaterThan( vertexColorOverride, vec4( 0.0 ) ) );
	rawVertexAlpha = vertexColor.a;
	D = vec4( sqrt(lightSourceDiffuse[0].rgb), brightnessScale );
}
