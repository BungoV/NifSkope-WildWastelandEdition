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
uniform int sssPar;		// lane SHADOWPAR1: FO4CS's evidence, completion and edge fades, plane and normal rejects
uniform vec3 sssLightView;	// the direction TO the light, view space (the normal reject)

out float fragMask;

// lane SHADOWPAR1: FO4CS's receiver plane (bend_sss_gpu.hlsli) -- the depth 4 pixels either side along the line
const float kPlaneBias = 12.0;	// fReceiverPlaneBias, game units, ramped over the first 32 samples
const float kNormalReject = 0.1;	// a caster facing the light edge-on (|N.L| <= 0.1) is no evidence
const float kEvidence = 4.0;	// fContactEvidenceSamples
const float kEdgeBand = 16.0;	// fEdgeFadeBandPx

vec3 normalAt( vec2 maskPx )
{
	ivec2 q = clamp( ivec2( floor( maskPx * maskScale ) ), ivec2( 0 ), fullSize - 1 );
	return texelFetch( gbuf, q, 0 ).rgb;
}

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
	bool par = sssPar != 0;
	// the receiver plane: 1/z is affine along the line, its slope from the depths 4 pixels either side
	bool va4, vb4;
	float lf = linAt( p + stepXY * 4.0, va4 ), lr = linAt( p - stepXY * 4.0, vb4 );
	bool plane = par && va4 && vb4 && abs( lf - lr ) < 0.25 * lin0;
	float invSlope = plane ? ( 1.0 / lf - 1.0 / lr ) / 8.0 : 0.0;
	float hard = 1.0, support = 0.0;
	int exitK = n;
	for ( int k = 1; k <= 384; k++ ) {
		if ( k > n )
			break;
		float dist = dist0 + float( k ) * direction;
		if ( dist < 1.0 )
			break;
		vec2 q = p + stepXY * float( k );
		if ( par && exitK == n && ( any( lessThan( q * maskScale, vec2( 0.0 ) ) ) || any( greaterThanEqual( q * maskScale, vec2( fullSize ) ) ) ) )
			exitK = k;
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
		if ( par ) {
			// the receiver's own plane is no occluder (within the bias, ramped in over 32 samples)
			if ( plane ) {
				float planeLin = 1.0 / max( 1.0 / lin0 + invSlope * float( k ), 1e-9 );
				if ( ls > planeLin - kPlaneBias * clamp( float( k ) / 32.0, 0.125, 1.0 ) )
					d = 1e10;
			}
			// a caster edge-on to the light
			if ( d < 1.0 && abs( dot( normalAt( q ), sssLightView ) ) <= kNormalReject )
				d = 1e10;
			// the completion fade: the last 16 samples weigh less
			float c = float( n - k ) / 16.0;
			float fadeOut = k > n - 16 ? clamp( 1.0 - c, 0.0, 1.0 ) * 0.75 : 0.0;
			if ( k <= 4 ) {
				hard = min( hard, d );
				support += clamp( ( 1.0 - d ) * contrast, 0.0, 1.0 );
			}
			d += fadeOut;
		}
		int lane = k & 3;
		lanes[lane] = min( lanes[lane], d );
	}
	lanes = clamp( lanes * contrast + ( 1.0 - contrast ), 0.0, 1.0 );
	fragMask = dot( lanes, vec4( 0.25 ) );
	if ( par ) {
		// the hard samples count only with enough evidence; the ray that leaves the screen early fades out
		float h = mix( 1.0, clamp( hard * contrast + ( 1.0 - contrast ), 0.0, 1.0 ), clamp( support / kEvidence, 0.0, 1.0 ) );
		fragMask = min( h, fragMask );
		fragMask = mix( 1.0, fragMask, smoothstep( 0.0, kEdgeBand, float( exitK ) * maskScale ) );
	}
}
