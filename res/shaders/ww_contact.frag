#version 410 core

// Copyright 2023 Sony Interactive Entertainment.
// Licensed under the Apache License, Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0).
//
// lane TERRSHADOW1: contact shadows, a per-pixel OpenGL port of Bend Studio's screen-space shadows (the
// light's projected coordinate, the march along the screen line to it, the perspective-correct ray depth
// lin0 * dist0 / dist, the minor-axis bilinear neighbour with its edge test, the thickness window and the
// four-lane contrast resolve). Bend's wavefront/LDS layout is a compute shader; GL 4.1 has none, so every
// pixel marches its own line, in linear view depth (game units, the shared opaque pass's .a).

uniform sampler2D gbuf;		// RGBA32F, bottom row first: view normal, linear depth (game units; 1e6 = none)
uniform ivec2 fullSize;
uniform float maskScale;	// full pixels per mask pixel (1 or 2)
uniform vec4 lightCoord;	// the light's projected point in mask pixels (xy), unused z, w = sign of clip w
uniform vec4 sssParams;		// thickness (fraction of depth), bilinear threshold, contrast, samples
uniform float sssFloor;		// the thickness floor, game units

out float fragMask;

float linAt( vec2 maskPx, out bool valid )
{
	ivec2 q = ivec2( floor( maskPx * maskScale ) );
	if ( any( lessThan( q, ivec2( 0 ) ) ) || any( greaterThanEqual( q, fullSize ) ) ) {
		valid = false;
		return 1e6;
	}
	float l = texelFetch( gbuf, q, 0 ).a;
	valid = l < 9.0e5;
	return l;
}

void main()
{
	vec2 p = gl_FragCoord.xy;
	bool v0;
	float lin0 = linAt( p, v0 );
	fragMask = 1.0;
	if ( !v0 || lin0 <= 0.0 )
		return;
	vec2 delta = p - lightCoord.xy;
	bool majorX = abs( delta.x ) > abs( delta.y );
	float dist0 = majorX ? abs( delta.x ) : abs( delta.y );
	if ( dist0 < 1.0 )
		return;
	// light in front (w > 0): the march heads for its vanishing point; behind: away from it
	float direction = lightCoord.w > 0.0 ? -1.0 : 1.0;
	vec2 stepXY = delta / dist0 * direction;	// one mask pixel along the major axis
	int n = int( sssParams.w );
	float contrast = sssParams.z;
	vec4 lanes = vec4( 1.0 );
	for ( int k = 1; k <= 128; k++ ) {
		if ( k > n )
			break;
		float dist = dist0 + float( k ) * direction;
		if ( dist < 1.0 )
			break;
		vec2 q = p + stepXY * float( k );
		// Bend's minor-axis bilinear pair: the nearer neighbour across the line, point-filtered at an edge
		float minor = majorX ? q.y : q.x;
		float f = fract( minor ) - 0.5;
		vec2 off = majorX ? vec2( 0.0, f > 0.0 ? 1.0 : -1.0 ) : vec2( f > 0.0 ? 1.0 : -1.0, 0.0 );
		bool va, vb;
		float la = linAt( q, va );
		float lb = linAt( q + off, vb );
		if ( !va )
			continue;	// sky or off the screen: empty, never an occluder
		bool edge = !vb || abs( la - lb ) > la * sssParams.y;
		float ls = edge ? la : mix( la, lb, abs( f ) );
		float rayLin = lin0 * dist0 / dist;	// the ray's depth at this pixel (1/z is affine along the line)
		float thick = max( sssFloor, sssParams.x * rayLin );
		float d = abs( ( rayLin - ls ) / thick - 1.0 );
		int lane = k & 3;
		lanes[lane] = min( lanes[lane], d );
	}
	lanes = clamp( lanes * contrast + ( 1.0 - contrast ), 0.0, 1.0 );
	fragMask = dot( lanes, vec4( 0.25 ) );
}
