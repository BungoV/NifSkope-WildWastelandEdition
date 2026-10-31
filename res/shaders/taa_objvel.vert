#version 410 core

/* lane TAAINPUTS1: one moving shape's current and previous clip positions (src/gl/taainputs.h).
 * velMode 0: rigid (the same vertex, last frame's matrix); 1: skinned (the CPU skin now in 0, last frame's in 8);
 * 2: the cell's grass wind at this and last frame's phase (fo4_default.vert's offset, step for step). */

uniform mat4 clipJit;	// the frame's jittered projection x model-view: lands on the frame's depth
uniform mat4 clipCur;	// unjittered, this frame
uniform mat4 clipPrev;	// unjittered, last frame
uniform mat4 modelViewMatrix;
uniform int velMode;
uniform vec4 wwGrassWind;
uniform vec4 wwGrassWind2;

uniform mat3 normalMatrix;	// lane TREELEAF4: the shape's view rotation, as Shape::setUniforms gives the colour pass

layout ( location = 0 ) in vec3 vertexPosition;
layout ( location = 1 ) in vec4 vertexColor;
layout ( location = 2 ) in vec3 normalVector;
layout ( location = 3 ) in vec3 tangentVector;
layout ( location = 4 ) in vec3 bitangentVector;
layout ( location = 7 ) in vec2 multiTexCoord0;
layout ( location = 8 ) in vec3 prevPosition;

out vec4 cCur;
out vec4 cPrev;
out vec3 viewPos;
out vec2 texCoord;
out float vAlpha;	// lane TREELEAF3: the vertex alpha an effect shape's colour pass multiplies in (fxMode & 1)
// lane TREELEAF4 (FXCOVRESID1): fo4_default.vert's btnMatrix (bitangent, tangent, normal in view space), for the falloff
out mat3 btnMatrix;

vec3 grassOffset( float phase )
{
	float L = length( bitangentVector );
	float off = L > 2.0 ? L - 1024.0 : 0.0;
	float p = ( phase - off ) * wwGrassWind2.z;
	float half_range = ( wwGrassWind2.y - wwGrassWind2.x ) * 0.5;
	float sp = sin( p ), cp = cos( p );
	float w = sin( sp * 3.14159274 ) + sin( sp * 6.28318548 );
	w = w * 0.3 + cos( cp * 3.14159274 ) * 0.2;
	w = w + 1.0;
	w = w * half_range + wwGrassWind2.x;
	w = w * ( vertexColor.a * vertexColor.a * 0.5 );
	return vec3( cos( wwGrassWind.x ), sin( wwGrassWind.x ), 0.0 ) * w;
}

void main()
{
	vec3 cur = vertexPosition;
	vec3 prev = velMode == 1 ? prevPosition : vertexPosition;
	if ( velMode == 2 && wwGrassWind2.w > 0.5 ) {
		cur += grassOffset( wwGrassWind.w );
		prev += grassOffset( wwGrassWind.z );
	}
	cCur = clipCur * vec4( cur, 1.0 );
	cPrev = clipPrev * vec4( prev, 1.0 );
	viewPos = ( modelViewMatrix * vec4( cur, 1.0 ) ).xyz;
	texCoord = multiTexCoord0;
	vAlpha = vertexColor.a;
	btnMatrix[2] = normalize( normalVector * normalMatrix );
	btnMatrix[1] = normalize( tangentVector * normalMatrix );
	btnMatrix[0] = normalize( bitangentVector * normalMatrix );
	gl_Position = clipJit * vec4( cur, 1.0 );
}
