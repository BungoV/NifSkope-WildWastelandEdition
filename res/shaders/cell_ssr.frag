#version 410 core

/* lane SSR1: the game's screen-space reflections for the cell view (src/gl/cellssr.cpp, docs/PRTP_PLAN.md
 * "screen-space reflections"), read from the game's shader. Every target here is half the view and stored top
 * row first (the game's pixel order). Inputs: gbuf (lane AO1's opaque pass: view normal, GL axes, and the linear
 * depth in game units; full size, bottom row first), its min-of-2x2 depth mips 1..4 (top row first), and scene:
 * this frame's opaque lit color WITHOUT the reflection term, the obscurance and the fog (linear, full size,
 * bottom row first, bilinear), alpha = the material's reflection flag.
 *   stage 1  the ray: for a flagged pixel facing the eye, the reflection of the view ray about the normal (its
 *            world z doubled first), kept when it points more than ssrK.y into the view; the screen line through
 *            the pixel is run back to the near plane: (u0, v0, start depth)
 *   stage 2  the march: at most 32 steps over the depth mips (4 = coarsest), crossing one cell a step, going a
 *            level down where the ray is behind the depth and up where it is not; at level 1 a ray 50 units or
 *            more behind the surface is refused (it passes behind a thin thing). A hit samples scene there;
 *            alpha = the confidence squared (screen edge, travelled distance, depth gained)
 *   stage 3  the 5-tap blur across, stage 4 down: taps without confidence hand their weight to the others
 * ASSUMED (not read from the game): stage 1 point-samples its inputs (the full-size texel under the half pixel's
 * centre). Deviation: float targets (the game stores the march and the blurs in 8 bits, sRGB). */

uniform int ssrStage;
uniform sampler2D gbuf;
uniform sampler2D zMip1;
uniform sampler2D zMip2;
uniform sampler2D zMip3;
uniform sampler2D zMip4;
uniform sampler2D scene;
uniform sampler2D src;
uniform ivec2 fullSize;
uniform ivec2 halfSize;
uniform ivec2 mipSize[5];	// [0] = fullSize
uniform vec2 proj;			// the projection's P00, P11
uniform mat3 viewToWorld;
uniform vec4 ssrK;			// the game's four: color scale 1, angle gate 0.2, normal z scale 2, confidence scale 1
uniform vec2 ssrClip;		// the game camera's near (15) and far (the cell's clip distance), game units
uniform int ssrRed;			// WW_CELL_SSR_RED: 2 nogap (no 50-unit refusal), 4 nofade (confidence 1 on a hit)

out vec4 fragColor;

// the game's load: outside the mip reads 0
float zLoad( int m, vec2 cell )
{
	ivec2 t = ivec2( cell );
	if ( t.x < 0 || t.y < 0 || t.x >= mipSize[m].x || t.y >= mipSize[m].y )
		return 0.0;
	if ( m == 1 )
		return texelFetch( zMip1, t, 0 ).r;
	if ( m == 2 )
		return texelFetch( zMip2, t, 0 ).r;
	if ( m == 3 )
		return texelFetch( zMip3, t, 0 ).r;
	return texelFetch( zMip4, t, 0 ).r;
}

const float kDither[16] = float[16]( 0.0, 0.5, 0.125, 0.625, 0.75, 0.22, 0.875, 0.375,
                                     0.1875, 0.6875, 0.0625, 0.5625, 0.9375, 0.4375, 0.8125, 0.3125 );

bool rayOut( vec3 p )
{
	return p.x <= 0.0 || p.x >= 1.0 || p.y <= 0.0 || p.y >= 1.0 || 1.0 / p.z >= ssrClip.y;
}

void main()
{
	ivec2 q = ivec2( gl_FragCoord.xy );
	vec2 uv = ( vec2( q ) + 0.5 ) / vec2( halfSize );
	fragColor = vec4( 0.0 );

	if ( ssrStage == 1 ) {
		ivec2 t = min( ( ( 2 * q + 1 ) * fullSize ) / ( 2 * halfSize ), fullSize - 1 );
		t.y = fullSize.y - 1 - t.y;
		if ( texelFetch( scene, t, 0 ).a * ssrK.w - 0.01 < 0.0 )
			return;
		vec4 g = texelFetch( gbuf, t, 0 );
		float z = g.a;
		vec3 P = vec3( ( 2.0 * uv.x - 1.0 ) * z / proj.x, ( 1.0 - 2.0 * uv.y ) * z / proj.y, -z );
		vec3 V = -normalize( P );
		if ( dot( g.xyz, V ) < 0.0 )
			return;
		vec3 Nw = viewToWorld * g.xyz;
		Nw.z *= ssrK.z;
		vec3 N = normalize( Nw ) * viewToWorld;	// back into the view
		vec3 R = reflect( -V, N );
		if ( !( -R.z > ssrK.y ) )
			return;
		vec3 Q = P + 1000.0 * R;
		float zq = -Q.z;
		vec2 uvq = vec2( 0.5 + 0.5 * proj.x * Q.x / zq, 0.5 - 0.5 * proj.y * Q.y / zq );
		// the screen line is straight in (u, v, 1 / depth): run it back to depth = near
		float s = ( 1.0 / ssrClip.x - 1.0 / z ) / ( 1.0 / zq - 1.0 / z );
		fragColor = vec4( uv + ( uvq - uv ) * s, z, 1.0 );
		return;
	}

	if ( ssrStage == 2 ) {
		vec4 ray = texelFetch( src, q, 0 );
		float z0 = ray.z;
		if ( !( ssrClip.x < z0 ) )
			return;
		float dj = kDither[( q.y & 3 ) + 4 * ( q.x & 3 )];
		vec3 A = vec3( ray.xy, 1.0 / ssrClip.x + ( dj - 0.5 ) * 0.004 );
		vec3 D = vec3( uv, 1.0 / z0 ) - A;
		vec2 S = uv + D.xy * dj * 0.002;
		vec2 G = floor( vec2( halfSize ) / 8.0 );
		vec2 cell = floor( G * S );
		int level = 4;
		vec2 s01 = vec2( greaterThanEqual( D.xy, vec2( 0.0 ) ) );
		vec2 sgn = 2.0 * s01 - 1.0;
		vec2 stepv;
		vec3 p;
		// one cell on: the nearer of the cell's two far edges
		vec2 tc = ( ( cell + s01 ) / G - A.xy ) / D.xy;
		bool xfirst = tc.y >= tc.x;
		stepv = sgn * vec2( xfirst ? 1.0 : 0.0, xfirst ? 0.0 : 1.0 );
		cell += stepv;
		p = A + min( tc.x, tc.y ) * D;
		float zfirst = 1.0 / p.z;
		bool alive = true, refused = false;
		int count = 0;
		float d = 0.0;
		for ( int n = 1; n <= 32; n++ ) {
			alive = alive && level >= 1 && !rayOut( p );
			if ( !alive )
				break;
			d = zLoad( level, cell );
			float rz = 1.0 / p.z;
			bool isL1 = level == 1 && ( ssrRed & 2 ) == 0;
			bool ref = refused || ( isL1 && rz - d >= 50.0 );
			if ( d < rz && !ref ) {
				level -= 1;
				vec2 h = vec2( greaterThanEqual( p.xy - ( cell + 0.5 ) / G, vec2( 0.0 ) ) );
				cell = 2.0 * cell + h;
				G *= 2.0;
				refused = false;
			} else {
				tc = ( ( cell + s01 ) / G - A.xy ) / D.xy;
				xfirst = tc.y >= tc.x;
				stepv = sgn * vec2( xfirst ? 1.0 : 0.0, xfirst ? 0.0 : 1.0 );
				cell += stepv;
				p = A + min( tc.x, tc.y ) * D;
				float rz2 = 1.0 / p.z;
				refused = ref || ( isL1 && rz2 - d >= 50.0 );
				if ( !refused ) {
					if ( d < rz2 ) {
						p = A + ( ( 1.0 / d - A.z ) / D.z ) * D;
						level -= 1;
						G *= 2.0;
						cell = floor( G * p.xy );
					} else if ( level != 4 ) {
						G = floor( G / 2.0 );
						cell = cell / 2.0 + vec2( stepv.x >= 0.0 ? 0.0 : -0.5, stepv.y >= 0.0 ? 0.0 : -0.5 );
						level += 1;
					}
				}
			}
			count = n;
		}
		if ( rayOut( p ) || count == 32 || refused )
			return;
		float c = 1.0 - pow( min( 2.0 * length( p.xy - 0.5 ), 1.0 ), 2.0 );
		c *= max( 1.0 - 2.0 * length( S - p.xy ), 0.0 );
		c *= clamp( 1.0 - 25.0 * ( d - zfirst ) / ( ssrClip.y - ssrClip.x ), 0.0, 1.0 );
		if ( ( ssrRed & 4 ) != 0 )
			c = 1.0;
		fragColor = vec4( clamp( textureLod( scene, vec2( p.x, 1.0 - p.y ), 0.0 ).rgb, 0.0, 1.0 ), c * c );
		return;
	}

	// stages 3, 4: the blur (src bilinear)
	const float off[5] = float[5]( -3.294215, -1.407333, 0.0, 1.407333, 3.294215 );
	const float wgt[5] = float[5]( 0.093913, 0.304005, 0.204164, 0.304005, 0.093913 );
	vec2 dir = ssrStage == 3 ? vec2( 1.0 / float( halfSize.x ), 0.0 ) : vec2( 0.0, 1.0 / float( halfSize.y ) );
	vec4 tap[5];
	float lost = 0.0, present = 0.0, a = 0.0;
	for ( int i = 0; i < 5; i++ ) {
		tap[i] = textureLod( src, uv + dir * off[i], 0.0 );
		a += wgt[i] * tap[i].a;
		if ( tap[i].a < 0.01 )
			lost += wgt[i];
		else
			present += 1.0;
	}
	vec3 rgb = vec3( 0.0 );
	if ( present > 0.0 ) {
		float e = lost / present;
		for ( int i = 0; i < 5; i++ )
			if ( !( tap[i].a < 0.01 ) )
				rgb += ( wgt[i] + e ) * tap[i].rgb;
	}
	fragColor = vec4( rgb, a );
}
