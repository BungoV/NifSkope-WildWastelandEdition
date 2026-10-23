#version 410 core

/* lane FXRES1 (src/gl/cellfxres.h): the soft-effect layer over the full-resolution frame. The layer holds the sorted
 * soft draws' sum as an offset C and a transmittance T (every draw is dst = src x A + dst x B, B a function of the
 * draw's alpha only), so the frame takes frame x T + C, exactly what the draws in place would have made at the
 * layer's resolution. Upsampled depth-aware (Jansen + Bavoil 2011, Cantlay 2007): where the four layer texels around
 * the pixel lie at the pixel's own depth, bilinear; at an edge, the one texel nearest the pixel's depth. Written by
 * dual-source blending: frame = outC + frame x outT. Where no draw landed, nothing is written (the stencil keeps
 * who drew last, as the tone map reads it). */

uniform sampler2D fxrC;		// the offset, linear light (RGBA16F)
uniform sampler2D fxrT;		// the transmittance: r colour, a alpha (RGBA16F)
uniform sampler2D fxrCov;	// 1 where a draw landed (R8)
uniform sampler2D fxrZ;		// the layer's depth (cell_fxres_down)
uniform sampler2D fxrDepth;	// the frame's opaque depth, full resolution
uniform int fxrScale;
uniform ivec2 fxrSize;		// the layer's texels
uniform vec2 fxrProj;		// the projection's [2][2] and [3][2]
uniform float fxrEdge;		// the relative view-depth step that counts as an edge
uniform int fxrMode;		// 0 depth-aware; 1 plain bilinear (information only)

layout ( location = 0, index = 0 ) out vec4 outC;
layout ( location = 0, index = 1 ) out vec4 outT;

float viewW( float d )
{
	return fxrProj.y / ( fxrProj.x + ( 2.0 * d - 1.0 ) );
}

void main()
{
	vec2 lc = gl_FragCoord.xy / float( fxrScale ) - 0.5;
	ivec2 b = ivec2( floor( lc ) );
	vec2 f = lc - vec2( b );
	ivec2 t[4] = ivec2[4]( b, b + ivec2( 1, 0 ), b + ivec2( 0, 1 ), b + ivec2( 1, 1 ) );
	float wb[4] = float[4]( ( 1.0 - f.x ) * ( 1.0 - f.y ), f.x * ( 1.0 - f.y ), ( 1.0 - f.x ) * f.y, f.x * f.y );
	float w = viewW( texelFetch( fxrDepth, ivec2( gl_FragCoord.xy ), 0 ).r );
	bool edge = false;
	int best = 0;
	float bestD = 1e30;
	for ( int i = 0; i < 4; i++ ) {
		t[i] = clamp( t[i], ivec2( 0 ), fxrSize - 1 );
		float dz = abs( viewW( texelFetch( fxrZ, t[i], 0 ).r ) - w );
		if ( dz > fxrEdge * w )
			edge = true;
		if ( dz < bestD ) {
			bestD = dz;
			best = i;
		}
	}
	vec4 c = vec4( 0.0 ), tr = vec4( 0.0 );
	float cov = 0.0;
	if ( fxrMode == 1 || !edge ) {
		for ( int i = 0; i < 4; i++ ) {
			c += texelFetch( fxrC, t[i], 0 ) * wb[i];
			tr += texelFetch( fxrT, t[i], 0 ) * wb[i];
			cov += texelFetch( fxrCov, t[i], 0 ).r * wb[i];
		}
	} else {
		c = texelFetch( fxrC, t[best], 0 );
		tr = texelFetch( fxrT, t[best], 0 );
		cov = texelFetch( fxrCov, t[best], 0 ).r;
	}
	if ( cov <= 0.0 )
		discard;
	outC = c;
	outT = vec4( tr.rrr, tr.a );
}
