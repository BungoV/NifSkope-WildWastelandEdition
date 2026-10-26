#version 410 core

/* lane CLOUD1: one weather cloud layer projected from the dome onto its band's plane (src/gl/cloudmap.cpp).
 * The Clouds.nif vertex is a direction from the eye; the plane at height cmHeight above the eye is met at
 * dir.xy * cmHeight / dir.z. w = dir.z makes that a perspective divide (uv interpolates correctly on the plane) and
 * clips everything at or under the horizon. show: a quad in the corner of the screen showing the map. */

layout ( location = 0 ) in vec3 vertexPosition;
layout ( location = 1 ) in vec4 vertexColor;	// alpha = the layer's edge fade
layout ( location = 7 ) in vec2 multiTexCoord0;

uniform float cmHeight;	// the band plane above the eye, game units
uniform float cmHalfExtent;	// the map square's half side, game units
uniform vec2 uvOffset;	// the layer's scroll
uniform bool cmShow;

out float vfade;
out vec2 uv;

void main()
{
	if ( cmShow ) {
		vfade = 1.0;
		uv = vertexPosition.xy * 0.5 + 0.5;
		gl_Position = vec4( vertexPosition.xy, 0.0, 1.0 );
		return;
	}
	vfade = vertexColor.a;
	uv = multiTexCoord0 + uvOffset;
	gl_Position = vec4( vertexPosition.xy * ( cmHeight / cmHalfExtent ), 0.0, vertexPosition.z );
}
