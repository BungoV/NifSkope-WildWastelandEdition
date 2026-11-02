#version 410 core

/* lane POSTFX1: per-object motion blur, pass 1 of 4 (src/gl/cellpost.h). The blur vector of each pixel in PIXELS,
 * current minus previous, times the shutter, clamped to maxPx; the camera's own motion is taken out, so a still
 * world under a turning camera gets 0 (bungo's rule: objects blur, the camera never does).
 * velTex = TAAINPUTS1's velocity (previous minus current, texture units, the camera's reprojection where nothing
 * moved on its own), maskTex.b = 1 where that velocity is an object vector. The camera's vector at the pixel is
 * the game_taa_mv reprojection of the same depth. objRed = 1 (WW_CELL_OBJBLUR_RED=camera): the camera is kept
 * (the whole velocity), the gate's red. */

uniform sampler2D velTex;
uniform sampler2D maskTex;
uniform sampler2D depthTex;
uniform mat4 reproj;
uniform vec2 jitterNdc;
uniform vec2 invSize;
uniform float shutter;
uniform float maxPx;
uniform int objRed;

out vec2 blur;

void main()
{
	ivec2 p = ivec2( gl_FragCoord.xy );
	vec2 uv = gl_FragCoord.xy * invSize;
	vec2 vel = texelFetch( velTex, p, 0 ).xy;
	vec2 v;
	if ( objRed != 0 ) {
		v = vel;
	} else {
		float d = texelFetch( depthTex, p, 0 ).r;
		vec3 ndc = vec3( uv * 2.0 - 1.0 - jitterNdc, d * 2.0 - 1.0 );
		vec4 q = reproj * vec4( ndc, 1.0 );
		vec2 cam = ( q.xy / q.w - ndc.xy ) * 0.5;
		float own = texelFetch( maskTex, p, 0 ).b;
		v = own > 0.5 ? vel - cam : vec2( 0.0 );
	}
	v = -v / invSize * shutter;	// texture units -> pixels, previous-minus-current -> the way it moved
	float l = length( v );
	if ( !( l < 1e6 ) )
		v = vec2( 0.0 );
	else if ( l > maxPx )
		v *= maxPx / l;
	blur = v;
}
