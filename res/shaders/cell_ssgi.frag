#version 410 core

/* lane SSGI1: screen-space GI for the cell view (src/gl/cellssgi.cpp), the method of our FO4CS's SSGI in our own
 * GLSL: a visibility-bitmask horizon march per slice. Inputs: gbuf (lane AO1's opaque pass: view normal, GL axes,
 * linear depth in game units; full size, bottom row first) and scene (the opaque lit linear colour without
 * reflection, obscurance and fog: lane SSR1's scene pass, full size, bottom row first). Every SSGI target is
 * stored bottom row first at the work size (full or half).
 *   stage 1  the cast: per slice, both sides, `steps` taps out to `radius` game units; each tap's front and back
 *            (front + thickness) angles cover sectors of a 32-bit mask over the slice's cosine-weighted hemisphere;
 *            sectors newly covered take the tap's radiance x its facing. rgb = bounce irradiance E (x pi),
 *            a = the unoccluded (cosine-weighted) share
 *   stage 2  temporal: the history reprojected through last frame's camera, refused on a depth or normal jump;
 *            out = mix(history, now, max(1 / n, 1 - historyBlend)), n capped at maxAccum (location 1: depth, n, normal)
 *   stage 3  the denoise: a depth- and normal-aware blur, `blurRadius` work pixels */

uniform int ssgiStage;
uniform sampler2D gbuf;
uniform sampler2D scene;
uniform sampler2D src;			// stage 2: this frame's cast; stage 3: the accumulated result
uniform sampler2D hist;			// stage 2: last frame's accumulated result
uniform sampler2D histMeta;		// stage 2: last frame's depth, n, packed normal
uniform ivec2 fullSize;
uniform ivec2 workSize;
uniform vec2 proj;				// the projection's P00, P11
uniform int slices;
uniform int steps;
uniform float radius;			// game units
uniform float thickness;		// game units behind a tap's front face
uniform float sourceBrightness;
uniform float saturation;
uniform float historyBlend;
uniform float maxAccum;
uniform float blurRadius;		// work pixels
uniform float normalWeight;		// the denoise's normal similarity power (0: depth only)
uniform int frameIndex;
uniform bool historyValid;
uniform float motionAccum;		// the age cap while the camera moves (FO4CS fMotionAccumFrames)
uniform bool moving;
uniform mat3 curToPrev;			// game-unit view now -> game-unit view last frame (rotation)
uniform vec3 curToPrevT;

layout ( location = 0 ) out vec4 fragColor;
layout ( location = 1 ) out vec4 fragMeta;

const float PI = 3.14159265;

vec4 gAt( ivec2 fullTexel )
{
	return texelFetch( gbuf, clamp( fullTexel, ivec2( 0 ), fullSize - 1 ), 0 );
}

// game-unit view position at a full-size uv (0..1, bottom row first) and linear depth z
vec3 viewPos( vec2 uv, float z )
{
	vec2 ndc = uv * 2.0 - 1.0;
	return vec3( ndc.x * z / proj.x, ndc.y * z / proj.y, -z );
}

vec2 viewToUv( vec3 p )
{
	float z = max( -p.z, 1.0e-4 );
	return vec2( p.x * proj.x / z, p.y * proj.y / z ) * 0.5 + 0.5;
}

// the normal turned toward the eye (the gbuf keeps a two-sided surface's drawn normal)
vec3 faceN( vec3 n, vec3 P )
{
	return dot( n, -P ) < 0.0 ? -n : n;
}

// interleaved gradient noise, rotated each frame (the FO4CS cast reads a spatiotemporal blue-noise sheet; this is
// the closed-form stand-in, no third-party texture)
float ign( vec2 p, int f )
{
	p += 5.588238 * float( f & 63 );
	return fract( 52.9829189 * fract( dot( p, vec2( 0.06711056, 0.00583715 ) ) ) );
}

uint sectorBits( float a0, float a1 )
{
	// a0 <= a1 in 0..1 across the slice: the sectors they cover
	int b0 = int( floor( clamp( a0, 0.0, 1.0 ) * 32.0 ) );
	int b1 = int( ceil( clamp( a1, 0.0, 1.0 ) * 32.0 ) );
	int n = b1 - b0;
	if ( n <= 0 )
		return 0u;
	uint m = n >= 32 ? 0xFFFFFFFFu : ( ( 1u << uint( n ) ) - 1u );
	return m << uint( b0 );
}

vec4 cast( ivec2 wp )
{
	vec2 uv = ( vec2( wp ) + 0.5 ) / vec2( workSize );
	ivec2 ft = ivec2( uv * vec2( fullSize ) );
	vec4 g = gAt( ft );
	if ( g.w <= 0.0 )
		return vec4( 0.0, 0.0, 0.0, 1.0 );
	vec3 P = viewPos( ( vec2( ft ) + 0.5 ) / vec2( fullSize ), g.w );
	vec3 V = normalize( -P );
	vec3 N = faceN( normalize( g.xyz ), P );
	// the radius in full-size pixels at this depth
	float rPx = radius * proj.y * 0.5 * float( fullSize.y ) / g.w;
	if ( rPx < 1.0 )
		return vec4( 0.0, 0.0, 0.0, 1.0 );
	float n0 = ign( vec2( wp ), frameIndex ), n1 = ign( vec2( wp ) + vec2( 37.0, 11.0 ), frameIndex + 17 );
	vec3 il = vec3( 0.0 );
	float vis = 0.0, wsum = 0.0;
	for ( int s = 0; s < slices; s++ ) {
		float phi = ( float( s ) + n0 ) * PI / float( slices );
		vec2 dir = vec2( cos( phi ), sin( phi ) );
		vec3 d3 = vec3( dir, 0.0 );
		vec3 ortho = normalize( d3 - V * dot( d3, V ) );
		vec3 axis = normalize( cross( ortho, V ) );
		vec3 Np = N - axis * dot( N, axis );
		float npLen = length( Np );
		if ( npLen < 1.0e-4 )
			continue;
		float n = sign( dot( Np, ortho ) ) * acos( clamp( dot( Np, V ) / npLen, -1.0, 1.0 ) );
		uint occ = 0u;
		vec3 sil = vec3( 0.0 );
		for ( int side = 0; side < 2; side++ ) {
			float sgn = side == 0 ? 1.0 : -1.0;
			for ( int k = 0; k < steps; k++ ) {
				float t = ( float( k ) + fract( n1 + float( k ) * 0.618034 ) ) / float( steps );
				float off = max( t * t * rPx, 1.0 + float( k ) );
				vec2 sp = vec2( ft ) + 0.5 + sgn * dir * off;
				if ( sp.x < 0.0 || sp.y < 0.0 || sp.x >= float( fullSize.x ) || sp.y >= float( fullSize.y ) )
					break;
				vec4 gs = gAt( ivec2( sp ) );
				if ( gs.w <= 0.0 )
					continue;
				vec3 S = viewPos( sp / vec2( fullSize ), gs.w );
				vec3 dF = S - P;
				float len = length( dF );
				if ( len > radius || len < 1.0e-3 )
					continue;
				vec3 dB = dF - V * thickness;
				// signed angles from V toward this side, then relative to the normal's angle, mapped by sin (cosine-weighted)
				float aF = sgn * acos( clamp( dot( dF / len, V ), -1.0, 1.0 ) );
				float aB = sgn * acos( clamp( dot( normalize( dB ), V ), -1.0, 1.0 ) );
				float uF = 0.5 + 0.5 * sin( clamp( aF - n, -0.5 * PI, 0.5 * PI ) );
				float uB = 0.5 + 0.5 * sin( clamp( aB - n, -0.5 * PI, 0.5 * PI ) );
				uint bits = sectorBits( min( uF, uB ), max( uF, uB ) );
				uint fresh = bits & ~occ;
				if ( fresh != 0u ) {
					// FO4CS's donor range weight: a smoothstep band over the outer quarter of the radius
					float rt = clamp( ( radius - len ) / max( 0.25 * radius, 1.0 ), 0.0, 1.0 );
					float share = float( bitCount( fresh ) ) / 32.0 * rt * rt * ( 3.0 - 2.0 * rt );
					vec3 Ns = normalize( gs.xyz );
					float facing = clamp( dot( Ns, -dF / len ), 0.0, 1.0 );
					// FO4CS's radiance mips: farther taps read a prefiltered level (log2 offset - 3.3, levels 0..4)
					vec3 L = textureLod( scene, sp / vec2( fullSize ), clamp( log2( off ) - 3.3, 0.0, 4.0 ) ).rgb;
					sil += share * facing * max( L, vec3( 0.0 ) );
					occ |= bits;
				}
			}
		}
		il += sil * npLen;
		vis += ( 1.0 - float( bitCount( occ ) ) / 32.0 ) * npLen;
		wsum += npLen;
	}
	if ( wsum <= 0.0 )
		return vec4( 0.0, 0.0, 0.0, 1.0 );
	il = il / wsum * PI * sourceBrightness;	// cosine-weighted share x radiance x pi = irradiance
	float y = dot( il, vec3( 0.2126, 0.7152, 0.0722 ) );
	il = max( mix( vec3( y ), il, saturation ), vec3( 0.0 ) );
	return vec4( il, vis / wsum );
}

vec3 ffClamp( vec3 c, float mean, float span )
{
	float y = dot( c, vec3( 0.2126, 0.7152, 0.0722 ) ) + 1.0e-4;
	float l = log2( y );
	float lc = clamp( l, mean - span, mean + span );
	return c * exp2( lc - l );
}

void main()
{
	ivec2 wp = ivec2( gl_FragCoord.xy );
	fragMeta = vec4( 0.0 );
	if ( ssgiStage == 1 ) {
		fragColor = cast( wp );
		return;
	}
	vec2 uv = ( vec2( wp ) + 0.5 ) / vec2( workSize );
	ivec2 ft = ivec2( uv * vec2( fullSize ) );
	vec4 g = gAt( ft );
	if ( ssgiStage == 2 ) {
		vec4 now = texelFetch( src, wp, 0 );
		float n = 1.0;
		vec4 outc = now;
		if ( historyValid && g.w > 0.0 ) {
			vec3 P = viewPos( ( vec2( ft ) + 0.5 ) / vec2( fullSize ), g.w );
			vec3 Pp = curToPrev * P + curToPrevT;
			vec2 puv = viewToUv( Pp );
			if ( all( greaterThanEqual( puv, vec2( 0.0 ) ) ) && all( lessThan( puv, vec2( 1.0 ) ) ) ) {
				ivec2 pt = ivec2( puv * vec2( workSize ) );
				vec4 m = texelFetch( histMeta, pt, 0 );	// depth, n, normal xy (z toward the eye)
				vec3 pn = vec3( m.zw, sqrt( max( 1.0 - dot( m.zw, m.zw ), 0.0 ) ) );
				float nz = -Pp.z;
				bool sameZ = m.x > 0.0 && abs( m.x - nz ) <= 0.05 * nz;
				vec3 nNow = normalize( curToPrev * faceN( normalize( g.xyz ), P ) );
				bool sameN = dot( pn, nNow ) > 0.8;
				if ( sameZ && sameN ) {
					vec4 h = texelFetch( hist, pt, 0 );
					n = min( m.y + 1.0, moving ? min( maxAccum, motionAccum ) : maxAccum );
					float w = max( 1.0 / n, 1.0 - historyBlend );
					outc = mix( h, now, w );
				}
			}
		}
		fragColor = outc;
		vec3 nm = g.w > 0.0 ? faceN( normalize( g.xyz ), viewPos( ( vec2( ft ) + 0.5 ) / vec2( fullSize ), g.w ) ) : vec3( 0.0, 0.0, 1.0 );
		fragMeta = vec4( g.w > 0.0 ? g.w : 0.0, n, nm.xy );
		return;
	}
	// stage 3: the denoise
	vec4 c0 = texelFetch( src, wp, 0 );
	if ( g.w <= 0.0 || blurRadius <= 0.0 ) {
		fragColor = c0;
		return;
	}
	vec3 n0 = normalize( g.xyz );
	float stepPx = max( blurRadius * 0.5, 1.0 );
	// FO4CS's firefly suppression: every tap's log-luma is held to the footprint's mean +- max(2.5 sigma, 0.3)
	float lm = 0.0, lv = 0.0;
	for ( int y = -2; y <= 2; y++ )
		for ( int x = -2; x <= 2; x++ ) {
			ivec2 q = clamp( wp + ivec2( vec2( x, y ) * stepPx ), ivec2( 0 ), workSize - 1 );
			float l = log2( dot( texelFetch( src, q, 0 ).rgb, vec3( 0.2126, 0.7152, 0.0722 ) ) + 1.0e-4 );
			lm += l;
			lv += l * l;
		}
	lm /= 25.0;
	float span = max( 2.5 * sqrt( max( lv / 25.0 - lm * lm, 0.0 ) ), 0.3 );
	vec4 sum = vec4( ffClamp( c0.rgb, lm, span ), c0.a );
	float ws = 1.0;
	for ( int y = -2; y <= 2; y++ )
		for ( int x = -2; x <= 2; x++ ) {
			if ( x == 0 && y == 0 )
				continue;
			ivec2 q = wp + ivec2( vec2( x, y ) * stepPx );
			if ( any( lessThan( q, ivec2( 0 ) ) ) || any( greaterThanEqual( q, workSize ) ) )
				continue;
			vec4 gq = gAt( ivec2( ( vec2( q ) + 0.5 ) / vec2( workSize ) * vec2( fullSize ) ) );
			if ( gq.w <= 0.0 )
				continue;
			float wz = max( 1.0 - abs( gq.w - g.w ) / ( 0.05 * g.w ), 0.0 );
			float wn = pow( max( dot( n0, normalize( gq.xyz ) ), 0.0 ), 8.0 * normalWeight );
			float wd = exp( -float( x * x + y * y ) / 4.5 );
			float w = wz * wn * wd;
			vec4 cq = texelFetch( src, q, 0 );
			sum += vec4( ffClamp( cq.rgb, lm, span ), cq.a ) * w;
			ws += w;
		}
	fragColor = sum / ws;
}
