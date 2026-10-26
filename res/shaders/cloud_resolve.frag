#version 410 core

/* lane CLOUD2: the volume clouds' temporal resolve (Schneider & Vos 2015: 1/16 of the pixels marched a frame, the rest
 * reprojected). A history pixel whose 4x4 slot is this frame's phase takes the fresh march; the others follow the
 * cloud's depth into the previous history. Off screen or no history: the march, bilinear. */

uniform sampler2D cvMarch;	// rgb = light, a = transmittance, at history / stride
uniform sampler2D cvMarchDepth;	// km
uniform sampler2D cvPrev;	// last frame's history
uniform sampler2D cvPrevDepth;
uniform vec2 cvHistSize;
uniform float cvStride;
uniform vec2 cvPhase;
uniform mat3 cvCam;	// this frame: world columns right, up, back
uniform vec4 cvProj;	// P00, P11, P20, P21
uniform vec3 cvCamKm;
uniform mat3 cvPrevCam;	// last frame's
uniform vec4 cvPrevProj;
uniform vec3 cvPrevCamKm;
uniform float cvHistOn;	// 0 = no history yet (first frame, resize, a jump)
uniform float cvReproj;	// 0 = the red noreproj: the same pixel of the history, the camera's motion ignored

layout ( location = 0 ) out vec4 cvOut;
layout ( location = 1 ) out vec4 cvDepth;

vec4 cvCatmull( float t )
{
	float t2 = t * t, t3 = t2 * t;
	return 0.5 * vec4( -t3 + 2.0 * t2 - t, 3.0 * t3 - 5.0 * t2 + 2.0, -3.0 * t3 + 4.0 * t2 + t, t3 - t2 );
}

void main()
{
	vec2 q = floor( gl_FragCoord.xy );
	vec2 slot = mod( q, cvStride );
	vec2 uv = ( q + 0.5 ) / cvHistSize;
	if ( all( equal( slot, cvPhase ) ) ) {
		ivec2 m = ivec2( floor( q / cvStride ) );
		cvOut = texelFetch( cvMarch, m, 0 );
		cvDepth = texelFetch( cvMarchDepth, m, 0 );
		return;
	}
	vec4 coarse = texture( cvMarch, uv );
	float dk = texture( cvMarchDepth, uv ).r;
	if ( cvHistOn < 0.5 ) {
		cvOut = coarse;
		cvDepth = vec4( dk );
		return;
	}
	vec2 pv = uv;
	if ( cvReproj > 0.5 ) {
		vec2 ndc = uv * 2.0 - 1.0;
		vec3 d = normalize( cvCam * vec3( ( ndc.x + cvProj.z ) / cvProj.x, ( ndc.y + cvProj.w ) / cvProj.y, -1.0 ) );
		// the cloud this pixel saw last frame: the previous depth along the ray (a sky pixel: far away)
		float dp = texture( cvPrevDepth, uv ).r;
		float dist = min( dp < 1e3 ? dp : dk, 400.0 );
		vec3 v = transpose( cvPrevCam ) * ( cvCamKm + d * dist - cvPrevCamKm );
		if ( v.z >= -1e-4 ) {
			cvOut = coarse;
			cvDepth = vec4( dk );
			return;
		}
		vec2 vr = v.xy / -v.z;
		vec2 pn = vec2( vr.x * cvPrevProj.x - cvPrevProj.z, vr.y * cvPrevProj.y - cvPrevProj.w );
		pv = pn * 0.5 + 0.5;
		if ( any( lessThan( pv, vec2( 0.0 ) ) ) || any( greaterThan( pv, vec2( 1.0 ) ) ) ) {
			cvOut = coarse;
			cvDepth = vec4( dk );
			return;
		}
	}
	// Catmull-Rom: a bilinear read each frame blurs the history over the up to 16 resamples before a refresh, and the
	// nearest texel drifts; the depth stays nearest (it is a distance, not a picture)
	vec2 xy = pv * cvHistSize - 0.5, f = fract( xy );
	ivec2 i0 = ivec2( floor( xy ) ) - 1, hi = ivec2( cvHistSize ) - 1;
	vec4 wx = cvCatmull( f.x ), wy = cvCatmull( f.y );
	vec4 c = vec4( 0.0 );
	for ( int j = 0; j < 4; j++ )
		for ( int i = 0; i < 4; i++ )
			c += wx[i] * wy[j] * texelFetch( cvPrev, clamp( i0 + ivec2( i, j ), ivec2( 0 ), hi ), 0 );
	cvOut = vec4( max( c.rgb, vec3( 0.0 ) ), clamp( c.a, 0.0, 1.0 ) );
	cvDepth = texelFetch( cvPrevDepth, clamp( ivec2( floor( pv * cvHistSize ) ), ivec2( 0 ), hi ), 0 );
}
