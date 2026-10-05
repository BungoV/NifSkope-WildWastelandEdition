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
/* lane GRASSMB1: the game's grass wind, op for op from its grass vertex shader (Shaders011 entry 02183, the deferred
 * prepass's grass technique; src/gl/cellgrass.h). wwGrassWind = cb2[11] = (angle, 0, previous phase, phase),
 * wwGrassWind2 = cb2[12] = (min speed * 300, max speed * 300, frequency) with .w = 1 on a grass shape, 0 on
 * every other (where nothing below runs). The blade's (h0 + h1) * 0.0078125 rides in the bitangent's length
 * (1024 + it). The game's world position  is the bucket's model position plus its translation, so the offset
 * adds here in model space. */
uniform vec4 wwGrassWind;
uniform vec4 wwGrassWind2;

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

	vec4 vc = vertexColor;
	if ( wwGrassWind2.w > 0.5 ) {
		float L = length( b );
		float off = L > 2.0 ? L - 1024.0 : 0.0;
		b = b * inversesqrt( dot( b, b ) );				// dp3, rsq, mul
		n = n * inversesqrt( dot( n, n ) );
		t = t * inversesqrt( dot( t, t ) );
		float p = ( wwGrassWind.w - off ) * wwGrassWind2.z;	// mad -(v5.x + v5.y), 0.0078125, cb2[11].w; mul cb2[12].z
		float half_range = ( wwGrassWind2.y - wwGrassWind2.x ) * 0.5;
		float sp = sin( p ), cp = cos( p );				// sincos
		float w = sin( sp * 3.14159274 ) + sin( sp * 6.28318548 );
		w = w * 0.3 + cos( cp * 3.14159274 ) * 0.2;
		w = w + 1.0;
		w = w * half_range + wwGrassWind2.x;
		w = w * ( vertexColor.a * vertexColor.a * 0.5 );
		vec3 d = vec3( cos( wwGrassWind.x ), sin( wwGrassWind.x ), 0.0 );	// sincos cb2[11].x
		n = n + d * w;
		n = n * inversesqrt( dot( n, n ) );
		t = t + d * w;
		t = t * inversesqrt( dot( t, t ) );
		b = b + d * w;
		b = b * inversesqrt( dot( b, b ) );
		v.xyz = d * w + v.xyz;
		vc.rgb = exp2( log2( vertexColor.rgb ) * 2.2 );		// log, mul 2.2, exp: o6.rgb = (v4.rgb * v5.w)^2.2
	}

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
	C = mix( vc, vertexColorOverride, greaterThan( vertexColorOverride, vec4( 0.0 ) ) );
	rawVertexAlpha = vertexColor.a;
	D = vec4( sqrt(lightSourceDiffuse[0].rgb), brightnessScale );
}
