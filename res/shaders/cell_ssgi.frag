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
 *   stage 3  the denoise: a depth- and normal-aware blur, `blurRadius` work pixels (lane SSGI2: red `oldblur`)
 * lane SSGI2 (FO4CS parity rows 8, 15, 20):
 *   stage 1  reads EA's FAST blue noise (slice + per-slice step jitter, FO4CS's form) and skips taps whose depth
 *            interval (stage 10/11's min/max pyramid) lies wholly outside the receiver's radius shell
 *   stage 2  also writes the temporal moments (location 2: mean log-luma, its 2nd moment, confidence, age)
 *   stage 4  FO4CS's robust denoise: 16 Poisson taps in the tangent plane, log-luma pre-clip + reconstruction clamp
 *   stage 5  quarter downsample (nearest-depth guide), stage 6 its 5x5 bilateral, stage 7 the reconstruction
 *            (filtered quarter + the robust residual x retention), FO4CS's quarter pyramid
 *   stage 10 the interval pyramid's first level from the g-buffer, stage 11 each coarser level
 * lane SSGI2 (row 16): stage 8 FO4CS's emissive cast: emitSlices slices whose taps pick an emitting texel inside the
 *            tap's footprint by importance (mip by luminance, bilinear cell, then down to mip 0), with the small-emitter
 *            boost and the near facing wrap; its own temporal (2), robust blur (4) and pyramid (5-7) run with the
 *            emissive spans (emissivePass), and the ordinary result adds it last (emitAdd). The ordinary cast then reads
 *            its donor without the emission (emitSub), so each emitter is counted once
 * SSGI2FIX: stage 1 weighs a tap's light and its shut share alike (no donor cosine) and also writes the shut moment
 *            (location 1); stage 2 accumulates it (location 3); stage 12 puts it in the result's upper half */

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
// lane SSGI2
uniform sampler2D noiseTex;		// EA FAST spatiotemporal blue noise, 128 x (64 x 128) RG8
uniform bool blueNoise;			// false: red `ign`, the closed-form IGN of SSGI1
uniform sampler2D hiz;			// min/max linear depth, level i = mip i + 1 (sky: min huge, max 0)
uniform int hizLevels;			// 0: red `nohiz` (no interval skip, no early out)
uniform sampler2D tstate;		// stage 2: last frame's moments; stages 4, 7: this frame's
uniform bool wantState;
uniform float robustRadius;		// stage 4, work pixels (FO4CS fBlurRadius)
uniform ivec2 quarterSize;
uniform sampler2D qraw;			// stages 6, 7: the quarter downsample
uniform sampler2D qguide;		// its guide: normal, linear depth
uniform sampler2D qfilt;		// stage 7: the filtered quarter
uniform sampler2D emitTex;		// stages 1, 8: the scene pass's emitted share, full size, mips 0..4, a = coverage
uniform bool emitSub;			// stage 1: the donor less the emission (the emissive cast carries it)
uniform int emitSlices;			// stage 8 (FO4CS iEmissiveSlices)
uniform bool emissivePass;		// stages 4, 7: the emissive cast's clip spans and retention
uniform sampler2D emitFin;		// stages 3, 7: the emissive result, added to the ordinary one
uniform bool emitAdd;
uniform bool oldFill;			// stage 1, red `oldfill`: SSGI1's fill (donor cosine light, every shut sector's probe removed)
uniform sampler2D srcBent;		// SSGI2FIX stage 2: this frame's shut moment (the cast's location 1)
uniform sampler2D histBent;		// stage 2: last frame's, in last frame's view

layout ( location = 0 ) out vec4 fragColor;
layout ( location = 1 ) out vec4 fragMeta;
layout ( location = 2 ) out vec4 fragState;
layout ( location = 3 ) out vec4 fragBent;	// SSGI2FIX stage 2: the accumulated shut moment (view space)

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

vec4 cast( ivec2 wp, out vec3 bent )
{
	bent = vec3( 0.0 );
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
	if ( blueNoise ) {
		// FO4CS's SpatioTemporalNoise: x the slice rotation, y the step jitter
		vec2 bn = texelFetch( noiseTex, ivec2( wp.x & 127, ( frameIndex & 63 ) * 128 + ( wp.y & 127 ) ), 0 ).rg;
		n0 = bn.x;
		n1 = bn.y;
	}
	vec3 il = vec3( 0.0 ), bentSum = vec3( 0.0 );
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
		float shut = 0.0;
		// SSGI2FIX: the shut moment, each shut tap's share along its direction (the surfaces bend the probe read by it)
		vec3 shutDir = vec3( 0.0 );
		// FO4CS: one step jitter per slice, slid by the golden ratio
		float sliceStep = fract( n1 + float( s ) * 0.618034 );
		for ( int side = 0; side < 2; side++ ) {
			float sgn = side == 0 ? 1.0 : -1.0;
			// every later tap of this slice adds no fresh sector (FO4CS breaks the same way)
			if ( hizLevels > 0 && occ == 0xFFFFFFFFu )
				break;
			for ( int k = 0; k < steps; k++ ) {
				float t = ( float( k ) + ( blueNoise ? sliceStep : fract( n1 + float( k ) * 0.618034 ) ) ) / float( steps );
				float off = max( t * t * rPx, 1.0 + float( k ) );
				vec2 sp = vec2( ft ) + 0.5 + sgn * dir * off;
				if ( sp.x < 0.0 || sp.y < 0.0 || sp.x >= float( fullSize.x ) || sp.y >= float( fullSize.y ) )
					break;
				if ( hizLevels > 0 ) {
					if ( occ == 0xFFFFFFFFu )
						break;
					// FO4CS's geometry mip: a cell whose whole depth interval is beyond z +- (radius + 1) holds no tap
					// within the radius (|dz| <= distance), so the skip drops exactly the taps `len > radius` drops
					int m = min( int( max( clamp( log2( off ) - 3.3, 0.0, 5.0 ) - 1.0, 0.0 ) + 0.5 ), hizLevels );
					if ( m > 0 ) {
						ivec2 c = min( ivec2( sp ) >> m, textureSize( hiz, m - 1 ) - 1 );
						vec2 iv = texelFetch( hiz, c, m - 1 ).rg;
						if ( iv.x > g.w + radius + 1.0 || iv.y < g.w - radius - 1.0 )
							continue;
					}
				}
				vec4 gs = gAt( ivec2( sp ) );
				if ( gs.w <= 0.0 )
					continue;
				// the texel's own centre: its depth at a sub-texel xy puts S off the surface (SSGI2FIX)
				vec3 S = viewPos( ( oldFill ? sp : floor( sp ) + 0.5 ) / vec2( fullSize ), gs.w );
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
					// a lit sector's radiance carries no donor cosine; a back-facing or edge-on tap shuts no probe light it
					// cannot give back, so light and removal take the same weight (the DC2 energy balance)
					float facing = oldFill ? clamp( dot( Ns, -dF / len ), 0.0, 1.0 )
						: smoothstep( 0.0, 0.1, dot( faceN( Ns, S ), -dF / len ) );
					shut += share * facing;
					shutDir += share * facing * dF / len;
					// FO4CS's radiance mips: farther taps read a prefiltered level (log2 offset - 3.3, levels 0..4)
					float lod = clamp( log2( off ) - 3.3, 0.0, 4.0 );
					vec3 L = textureLod( scene, sp / vec2( fullSize ), lod ).rgb;
					if ( emitSub ) {
						// the level the scene's nearest-mip lookup took (GL: ceil(lod + 0.5) - 1 above 0.5), so the
						// emission comes off the same box mean
						float lvl = lod <= 0.5 ? 0.0 : ceil( lod + 0.5 ) - 1.0;
						L -= textureLod( emitTex, sp / vec2( fullSize ), lvl ).rgb;
					}
					sil += share * facing * max( L, vec3( 0.0 ) );
					occ |= bits;
				}
			}
		}
		il += sil * npLen;
		vis += ( 1.0 - ( oldFill ? float( bitCount( occ ) ) / 32.0 : min( shut, 1.0 ) ) ) * npLen;
		bentSum += shutDir * npLen;
		wsum += npLen;
	}
	if ( wsum <= 0.0 )
		return vec4( 0.0, 0.0, 0.0, 1.0 );
	bent = bentSum / wsum;
	il = il / wsum * PI * sourceBrightness;	// cosine-weighted share x radiance x pi = irradiance
	float y = dot( il, vec3( 0.2126, 0.7152, 0.0722 ) );
	il = max( mix( vec3( y ), il, saturation ), vec3( 0.0 ) );
	return vec4( il, vis / wsum );
}

// ---- lane SSGI2: FO4CS's robust denoise and quarter pyramid (BlurCS, PyramidDownsample/Filter/ReconstructCS) ----
const vec2 kPoisson[16] = vec2[16]( vec2( -0.94201624, -0.39906216 ), vec2( 0.94558609, -0.76890725 ),
	vec2( -0.09418410, -0.92938870 ), vec2( 0.34495938, 0.29387760 ), vec2( -0.91588581, 0.45771432 ),
	vec2( -0.81544232, -0.87912464 ), vec2( -0.38277543, 0.27676845 ), vec2( 0.97484398, 0.75648379 ),
	vec2( 0.44323325, -0.97511554 ), vec2( 0.53742981, -0.47373420 ), vec2( -0.26496911, -0.41893023 ),
	vec2( 0.79197514, 0.19090188 ), vec2( -0.24188840, 0.99706507 ), vec2( -0.81409955, 0.91437590 ),
	vec2( 0.19984126, 0.78641367 ), vec2( 0.14383161, -0.14100790 ) );
const vec3 kLuma = vec3( 0.2126, 0.7152, 0.0722 );
const float kVarEps = 1.0e-4;

float lumaOf( vec3 v )
{
	return dot( max( v, vec3( 0.0 ) ), kLuma );
}

float logLuma( vec3 v )
{
	return log2( 1.0 + lumaOf( v ) );
}

float logScale( vec3 v, float target )
{
	float s = lumaOf( v );
	return s <= 1.0e-8 ? 1.0 : max( exp2( max( target, 0.0 ) ) - 1.0, 0.0 ) / s;
}

// the work pixel's guide: eye-facing view normal, linear depth (0: sky)
vec4 guideAt( ivec2 w )
{
	ivec2 f = ivec2( ( vec2( w ) + 0.5 ) / vec2( workSize ) * vec2( fullSize ) );
	vec4 g = gAt( f );
	if ( g.w <= 0.0 )
		return vec4( 0.0, 0.0, 1.0, 0.0 );
	return vec4( faceN( normalize( g.xyz ), viewPos( ( vec2( f ) + 0.5 ) / vec2( fullSize ), g.w ) ), g.w );
}

float tapWeight( int tap, vec3 P, vec3 N, vec3 T, vec3 B, float halfAngle, float tol, out ivec2 q )
{
	q = ivec2( 0 );
	vec3 pp = P + T * kPoisson[tap].x + B * kPoisson[tap].y;
	if ( pp.z > -1.0e-3 )
		return 0.0;
	vec2 puv = viewToUv( pp );
	if ( any( lessThan( puv, vec2( 0.0 ) ) ) || any( greaterThan( puv, vec2( 1.0 ) ) ) )
		return 0.0;
	q = clamp( ivec2( puv * vec2( workSize ) ), ivec2( 0 ), workSize - 1 );
	vec4 gs = guideAt( q );
	if ( gs.w <= 0.0 )
		return 0.0;
	vec3 S = viewPos( puv, gs.w );
	float w = exp( -0.66 * dot( kPoisson[tap], kPoisson[tap] ) );
	w *= clamp( 1.0 - abs( dot( N, S - P ) ) / tol, 0.0, 1.0 );
	float c = dot( gs.xyz, N );
	w *= smoothstep( 0.1, 0.6, c );
	// one normal serves as both geometric and shading normal here
	w *= 1.0 - clamp( acos( clamp( c, -1.0, 1.0 ) ) / max( halfAngle, 1.0e-4 ), 0.0, 1.0 );
	return w;
}

vec4 robustBlur( ivec2 wp )
{
	vec4 c0 = texelFetch( src, wp, 0 );
	vec4 gd = guideAt( wp );
	if ( gd.w <= 0.0 )
		return c0;
	vec3 P = viewPos( ( vec2( ivec2( ( vec2( wp ) + 0.5 ) / vec2( workSize ) * vec2( fullSize ) ) ) + 0.5 ) / vec2( fullSize ), gd.w );
	vec3 N = gd.xyz;
	vec4 ts = texelFetch( tstate, wp, 0 );
	float conf = clamp( ts.z, 0.0, 1.0 );
	float tvar = max( ts.y - ts.x * ts.x, 0.0 );
	float need = max( 1.0 - conf, clamp( sqrt( tvar ) * 2.0, 0.0, 1.0 ) );
	float rad = robustRadius * mix( 0.85, 2.0, need );
	float worldR = rad * gd.w * 2.0 / ( proj.x * float( workSize.x ) );
	// FO4CS GetBasis (branchless orthonormal basis)
	float sz = N.z >= 0.0 ? 1.0 : -1.0;
	float a = 1.0 / ( sz + N.z ), ya = N.y * a, b = N.x * ya, cc = N.x * sz;
	vec3 T = vec3( cc * N.x * a - 1.0, sz * b, cc ) * worldR;
	vec3 B = vec3( b, N.y * ya - sz, N.y ) * worldR;
	float tol = max( 0.25 * worldR, 0.5 );
	float halfAngle = mix( 0.5 * PI, 0.25 * PI, sqrt( conf ) );
	float cl = logLuma( c0.rgb );
	float refL = mix( cl, max( ts.x, 0.0 ), conf );
	float loc = cl;
	ivec2 wm = workSize - 1;
	loc += logLuma( texelFetch( src, clamp( wp + ivec2( 1, 0 ), ivec2( 0 ), wm ), 0 ).rgb );
	loc += logLuma( texelFetch( src, clamp( wp + ivec2( -1, 0 ), ivec2( 0 ), wm ), 0 ).rgb );
	loc += logLuma( texelFetch( src, clamp( wp + ivec2( 0, 1 ), ivec2( 0 ), wm ), 0 ).rgb );
	loc += logLuma( texelFetch( src, clamp( wp + ivec2( 0, -1 ), ivec2( 0 ), wm ), 0 ).rgb );
	loc *= 0.2;
	float spanScale = clamp( max( max( refL, cl ), loc ), 0.0, 1.0 );
	float spanEps = kVarEps * spanScale;
	float tSigma = sqrt( tvar + spanEps );
	float pre = emissivePass ? max( 6.0 * tSigma, 1.25 * spanScale ) : max( 4.0 * tSigma, 0.75 * spanScale );
	float ls = cl, ls2 = cl * cl, lw = 1.0;
	ivec2 qs[16];
	float ws[16];
	for ( int i = 0; i < 16; i++ ) {
		ws[i] = tapWeight( i, P, N, T, B, halfAngle, tol, qs[i] );
		if ( ws[i] <= 1.0e-4 )
			continue;
		float rel = mix( 0.5, 1.0, clamp( texelFetch( tstate, qs[i], 0 ).z, 0.0, 1.0 ) );
		float l = clamp( logLuma( texelFetch( src, qs[i], 0 ).rgb ), max( refL - pre, 0.0 ), refL + pre );
		float w = ws[i] * rel;
		ls += l * w;
		ls2 += l * l * w;
		lw += w;
	}
	float lm = ls / lw;
	float lv = max( ls2 / lw - lm * lm, 0.0 );
	float rSigma = sqrt( max( lv, tvar ) + spanEps );
	float rScale = clamp( max( lm, spanScale ), 0.0, 1.0 );
	float span = emissivePass ? max( 4.0 * rSigma, 0.65 * rScale ) : max( 2.5 * rSigma, 0.30 * rScale );
	float lo = max( lm - span, 0.0 ), hi = lm + span;
	float cRel = mix( 0.35, 1.0, conf ) / ( 1.0 + 2.0 * sqrt( tvar + kVarEps ) );
	vec3 sum = c0.rgb * ( logScale( c0.rgb, clamp( cl, lo, hi ) ) * cRel );
	float sumA = c0.a * cRel;
	float wsum = cRel;
	for ( int i = 0; i < 16; i++ ) {
		if ( ws[i] <= 1.0e-4 )
			continue;
		vec4 cq = texelFetch( src, qs[i], 0 );
		vec4 sq = texelFetch( tstate, qs[i], 0 );
		float rel = mix( 0.35, 1.0, clamp( sq.z, 0.0, 1.0 ) ) / ( 1.0 + 2.0 * sqrt( max( sq.y - sq.x * sq.x, 0.0 ) + kVarEps ) );
		float l = logLuma( cq.rgb );
		float w = ws[i] * rel;
		sum += cq.rgb * ( logScale( cq.rgb, clamp( l, lo, hi ) ) * w );
		// the open share rides the same guide weights, unclamped (SSGI1's blur smoothed it too)
		sumA += cq.a * w;
		wsum += w;
	}
	return vec4( sum, sumA ) / wsum;
}

float depthMetric( float z )
{
	return z > 0.0 ? z : 1.0e30;
}

// stage 5: work -> quarter, the nearest depth of the 2x2 is the reference (location 1: its guide)
void quarterDown( ivec2 qp )
{
	ivec2 o = qp * 2;
	vec4 gd[4];
	ivec2 px[4];
	int ref = 0;
	for ( int i = 0; i < 4; i++ ) {
		px[i] = clamp( o + ivec2( i & 1, i >> 1 ), ivec2( 0 ), workSize - 1 );
		gd[i] = guideAt( px[i] );
		if ( depthMetric( gd[i].w ) < depthMetric( gd[ref].w ) )
			ref = i;
	}
	if ( gd[ref].w <= 0.0 ) {
		fragColor = texelFetch( src, px[ref], 0 );
		fragMeta = gd[ref];
		return;
	}
	vec4 sum = vec4( 0.0 );
	vec3 ns = vec3( 0.0 );
	float ws = 0.0;
	for ( int i = 0; i < 4; i++ ) {
		if ( gd[i].w <= 0.0 )
			continue;
		float wn = pow( clamp( ( dot( gd[ref].xyz, gd[i].xyz ) - 0.25 ) / 0.75, 0.0, 1.0 ), 4.0 );
		float w = exp2( -4.0 * abs( gd[i].w - gd[ref].w ) / max( 0.04 * gd[ref].w, 0.05 ) ) * wn;
		sum += texelFetch( src, px[i], 0 ) * w;
		ns += gd[i].xyz * w;
		ws += w;
	}
	if ( ws <= 1.0e-5 ) {
		fragColor = texelFetch( src, px[ref], 0 );
		fragMeta = gd[ref];
		return;
	}
	fragColor = sum / ws;
	fragMeta = vec4( normalize( ns ), gd[ref].w );
}

// stage 6: the quarter's 5x5 bilateral
vec4 quarterFilter( ivec2 qp )
{
	vec4 c = texelFetch( qguide, qp, 0 );
	vec4 c0 = texelFetch( qraw, qp, 0 );
	if ( c.w <= 0.0 )
		return c0;
	vec3 sum = vec3( 0.0 );
	float ws = 0.0;
	for ( int y = -2; y <= 2; y++ )
		for ( int x = -2; x <= 2; x++ ) {
			ivec2 q = clamp( qp + ivec2( x, y ), ivec2( 0 ), quarterSize - 1 );
			vec4 s = texelFetch( qguide, q, 0 );
			if ( s.w <= 0.0 )
				continue;
			float wd = exp2( -3.0 * abs( s.w - c.w ) / max( 0.06 * c.w, 0.10 ) );
			float wn = pow( clamp( ( dot( c.xyz, s.xyz ) - 0.30 ) / 0.70, 0.0, 1.0 ), 4.0 );
			float w = exp2( -0.35 * float( x * x + y * y ) ) * wd * wn;
			sum += texelFetch( qraw, q, 0 ).rgb * w;
			ws += w;
		}
	return vec4( sum / max( ws, 1.0e-5 ), c0.a );
}

// stage 7: filtered quarter + the robust residual x retention
vec4 quarterReconstruct( ivec2 wp )
{
	vec4 rb = texelFetch( src, wp, 0 );
	vec4 gd = guideAt( wp );
	if ( gd.w <= 0.0 )
		return rb;
	vec2 qpos = ( vec2( wp ) + 0.5 ) / vec2( workSize ) * vec2( quarterSize ) - 0.5;
	ivec2 base = ivec2( floor( qpos ) );
	vec2 fr = qpos - vec2( base );
	vec3 raw = vec3( 0.0 ), filt = vec3( 0.0 );
	float ws = 0.0;
	for ( int y = 0; y <= 1; y++ )
		for ( int x = 0; x <= 1; x++ ) {
			ivec2 q = clamp( base + ivec2( x, y ), ivec2( 0 ), quarterSize - 1 );
			vec4 s = texelFetch( qguide, q, 0 );
			float bl = ( x == 0 ? 1.0 - fr.x : fr.x ) * ( y == 0 ? 1.0 - fr.y : fr.y );
			float wd = s.w > 0.0 ? clamp( 1.0 - abs( s.w - gd.w ) / max( 0.06 * gd.w, 0.10 ), 0.0, 1.0 ) : 0.0;
			float wn = pow( clamp( ( dot( gd.xyz, s.xyz ) - 0.25 ) / 0.75, 0.0, 1.0 ), 3.0 );
			float w = bl * wd * wn;
			raw += texelFetch( qraw, q, 0 ).rgb * w;
			filt += texelFetch( qfilt, q, 0 ).rgb * w;
			ws += w;
		}
	if ( ws > 1.0e-5 ) {
		raw /= ws;
		filt /= ws;
	} else {
		ivec2 q = clamp( ivec2( round( qpos ) ), ivec2( 0 ), quarterSize - 1 );
		raw = texelFetch( qraw, q, 0 ).rgb;
		filt = texelFetch( qfilt, q, 0 ).rgb;
	}
	vec4 ts = texelFetch( tstate, wp, 0 );
	float var = max( ts.y - ts.x * ts.x, 0.0 );
	float relRes = dot( abs( rb.rgb - raw ), kLuma ) / max( dot( max( filt, vec3( 0.0 ) ), kLuma ), 1.0e-5 );
	float keep = emissivePass
		? mix( 0.08, 0.35, clamp( ts.z, 0.0, 1.0 ) ) / ( 1.0 + relRes * 1.25 ) / ( 1.0 + sqrt( var ) * 4.0 )
		: mix( 0.04, 0.24, clamp( ts.z, 0.0, 1.0 ) ) / ( 1.0 + relRes * 2.0 ) / ( 1.0 + sqrt( var ) * 6.0 );
	return vec4( max( filt + ( rb.rgb - raw ) * keep, vec3( 0.0 ) ), rb.a );
}

// stages 10/11: the min/max depth-interval pyramid; texel i covers the finer texels 2i, 2i+1 and, on the last
// row/column of an odd-sized finer level, 2i+2 as well, so every finer texel lies in the cell the cast looks up
vec2 hizReduce( ivec2 p, bool fromGbuf )
{
	ivec2 fs = fromGbuf ? fullSize : textureSize( hiz, 0 );
	ivec2 os = max( fs / 2, ivec2( 1 ) );
	ivec2 n = ivec2( 2 ) + ivec2( equal( p, os - 1 ) ) * ivec2( greaterThan( fs, os * 2 ) );
	vec2 r = vec2( 1.0e30, 0.0 );
	for ( int y = 0; y < 3; y++ )
		for ( int x = 0; x < 3; x++ ) {
			if ( x >= n.x || y >= n.y )
				continue;
			ivec2 q = min( p * 2 + ivec2( x, y ), fs - 1 );
			if ( fromGbuf ) {
				float z = gAt( q ).w;
				if ( z > 0.0 )
					r = vec2( min( r.x, z ), max( r.y, z ) );
			} else {
				vec2 iv = texelFetch( hiz, q, 0 ).rg;
				r = vec2( min( r.x, iv.x ), max( r.y, iv.y ) );
			}
		}
	return r;
}

// ---- lane SSGI2 (row 16): FO4CS's emissive cast (GICastCS PASS 2, SampleEmissionImportance) ----
uint impHash( uint v )
{
	v ^= v >> 16u;
	v *= 0x7FEB352Du;
	v ^= v >> 15u;
	v *= 0x846CA68Bu;
	v ^= v >> 16u;
	return v;
}

float impRand( uint key, uint dim )
{
	return min( ( float( impHash( key ^ ( dim * 0x9E3779B9u ) ) ) + 0.5 ) * ( 1.0 / 4294967296.0 ), 0.99999994 );
}

int selectIdx( vec4 w, float r, out float p )
{
	float tot = dot( w, vec4( 1.0 ) );
	float th = r * tot, cum = w.x;
	int i = 0;
	if ( th >= cum ) { i = 1; cum += w.y; }
	if ( th >= cum ) { i = 2; cum += w.z; }
	if ( th >= cum ) i = 3;
	while ( i > 0 && w[i] <= 0.0 )	// a rounded threshold never lands on an empty entry
		i--;
	p = w[i] / max( tot, 1.0e-8 );
	return i;
}

void addLod( inout vec4 w, float lod, float scale )
{
	float l = clamp( lod, 1.0, 4.0 );
	int lo = int( floor( l ) );
	int hi = min( lo + 1, 4 );
	float f = fract( l );
	w[lo - 1] += scale * ( 1.0 - f );
	w[hi - 1] += scale * f;
}

// an emitting mip-0 texel inside the footprint at fuv (level emip), drawn by luminance; rad = its emission over the
// draw's probability (the footprint's mean in expectation), cov = the footprint's emitting share
bool sampleEmission( vec2 fuv, float emip, uint key, out ivec2 leaf, out vec3 rad, out float cov, out float selMip )
{
	leaf = ivec2( 0 );
	rad = vec3( 0.0 );
	cov = 0.0;
	selMip = emip;
	float coarseMip = min( emip + 1.0, 4.0 );
	float coarseW = 0.5 * clamp( 4.0 - emip, 0.0, 1.0 );	// FO4CS kEmissionFootprintCoarseWeight
	vec4 fp = max( mix( textureLod( emitTex, fuv, emip ), textureLod( emitTex, fuv, coarseMip ), coarseW ), vec4( 0.0 ) );
	if ( lumaOf( fp.rgb ) <= 1.0e-6 )
		return false;
	vec4 tw = vec4( 0.0 );
	addLod( tw, emip, 1.0 - coarseW );
	addLod( tw, coarseMip, coarseW );
	vec4 sw = vec4( 0.0 );
	for ( int i = 0; i < 4; i++ )
		if ( tw[i] > 0.0 )
			sw[i] = tw[i] * lumaOf( textureLod( emitTex, fuv, float( i + 1 ) ).rgb );
	if ( dot( sw, vec4( 1.0 ) ) <= 1.0e-8 )
		return false;
	float p;
	int mi = selectIdx( sw, impRand( key, 0u ), p );
	int sm = mi + 1;
	selMip = float( sm );
	float norm = tw[mi] / max( p, 1.0e-8 );
	ivec2 ms = max( fullSize >> sm, ivec2( 1 ) );
	vec2 mpos = fuv * vec2( ms ) - 0.5;
	ivec2 mb = ivec2( floor( mpos ) );
	vec2 mf = mpos - vec2( mb );
	vec4 bw = vec4( ( 1.0 - mf.x ) * ( 1.0 - mf.y ), mf.x * ( 1.0 - mf.y ), ( 1.0 - mf.x ) * mf.y, mf.x * mf.y );
	ivec2 cc[4];
	vec4 cp[4];
	vec4 cw = vec4( 0.0 );
	for ( int c = 0; c < 4; c++ ) {
		cc[c] = clamp( mb + ivec2( c & 1, c >> 1 ), ivec2( 0 ), ms - 1 );
		cp[c] = max( texelFetch( emitTex, cc[c], sm ), vec4( 0.0 ) );
		cw[c] = bw[c] * lumaOf( cp[c].rgb );
	}
	if ( dot( cw, vec4( 1.0 ) ) <= 1.0e-8 )
		return false;
	int sc = selectIdx( cw, impRand( key, 1u ), p );
	norm *= bw[sc] / max( p, 1.0e-8 );
	ivec2 coord = cc[sc];
	vec4 pay = cp[sc];
	for ( int pm = sm; pm > 0; pm-- ) {
		int chm = pm - 1;
		ivec2 chs = max( fullSize >> chm, ivec2( 1 ) );
		ivec2 kc[4];
		vec4 kp[4];
		vec4 kw = vec4( 0.0 );
		for ( int k = 0; k < 4; k++ ) {
			kc[k] = clamp( coord * 2 + ivec2( k & 1, k >> 1 ), ivec2( 0 ), chs - 1 );
			kp[k] = max( texelFetch( emitTex, kc[k], chm ), vec4( 0.0 ) );
			kw[k] = lumaOf( kp[k].rgb );
		}
		if ( dot( kw, vec4( 1.0 ) ) <= 1.0e-8 )
			return false;
		int sk = selectIdx( kw, impRand( key, uint( 2 + pm ) ), p );
		norm *= 0.25 / max( p, 1.0e-8 );
		coord = kc[sk];
		pay = kp[sk];
	}
	leaf = coord;
	rad = pay.rgb * norm;
	cov = fp.a;
	return true;
}

// stage 8: the ordinary march's slices and sectors, every tap's donor an emitter drawn from its footprint
vec4 castEmissive( ivec2 wp )
{
	vec2 uv = ( vec2( wp ) + 0.5 ) / vec2( workSize );
	ivec2 ft = ivec2( uv * vec2( fullSize ) );
	vec4 g = gAt( ft );
	if ( g.w <= 0.0 )
		return vec4( 0.0 );
	vec3 P = viewPos( ( vec2( ft ) + 0.5 ) / vec2( fullSize ), g.w );
	vec3 V = normalize( -P );
	vec3 N = faceN( normalize( g.xyz ), P );
	float rPx = radius * proj.y * 0.5 * float( fullSize.y ) / g.w;
	if ( rPx < 1.0 )
		return vec4( 0.0 );
	float n0 = ign( vec2( wp ), frameIndex ), n1 = ign( vec2( wp ) + vec2( 37.0, 11.0 ), frameIndex + 17 );
	if ( blueNoise ) {
		vec2 bn = texelFetch( noiseTex, ivec2( wp.x & 127, ( frameIndex & 63 ) * 128 + ( wp.y & 127 ) ), 0 ).rg;
		n0 = bn.x;
		n1 = bn.y;
	}
	vec3 il = vec3( 0.0 );
	float wsum = 0.0;
	for ( int s = 0; s < emitSlices; s++ ) {
		float phi = ( float( s ) + n0 ) * PI / float( emitSlices );
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
		float sliceStep = fract( n1 + float( s ) * 0.618034 );
		for ( int side = 0; side < 2 && occ != 0xFFFFFFFFu; side++ ) {
			float sgn = side == 0 ? 1.0 : -1.0;
			for ( int k = 0; k < steps && occ != 0xFFFFFFFFu; k++ ) {
				float t = ( float( k ) + sliceStep ) / float( steps );
				float off = max( t * t * rPx, 1.0 + float( k ) );
				vec2 sp = vec2( ft ) + 0.5 + sgn * dir * off;
				if ( sp.x < 0.0 || sp.y < 0.0 || sp.x >= float( fullSize.x ) || sp.y >= float( fullSize.y ) )
					break;
				// FO4CS's emission level for this step (its radianceMip), in full-size levels 1..4
				float emip = clamp( max( clamp( log2( off ) - 3.3, 0.0, 5.0 ), 2.0 ) - 1.0, 1.0, 4.0 );
				uint key = uint( wp.x ) + uint( wp.y ) * uint( workSize.x );
				key ^= uint( frameIndex ) * 0xA511E9B3u;
				key ^= uint( s ) * 0x63D83595u;
				key ^= uint( k ) * 0xB5297A4Du;
				key ^= uint( side ) * 0x68E31DA4u;
				ivec2 leaf;
				vec3 rad;
				float cov, selMip;
				bool hit = sampleEmission( sp / vec2( fullSize ), emip, key, leaf, rad, cov, selMip );
				vec2 dp = hit ? vec2( leaf ) + 0.5 : sp;
				vec4 gs = gAt( ivec2( dp ) );
				if ( gs.w <= 0.0 )
					continue;
				vec3 S = viewPos( dp / vec2( fullSize ), gs.w );
				vec3 dF = S - P;
				float len = length( dF );
				if ( len > radius || len < 1.0e-3 )
					continue;
				vec3 dB = dF - V * thickness;
				float aF = sgn * acos( clamp( dot( dF / len, V ), -1.0, 1.0 ) );
				float aB = sgn * acos( clamp( dot( normalize( dB ), V ), -1.0, 1.0 ) );
				float uF = 0.5 + 0.5 * sin( clamp( aF - n, -0.5 * PI, 0.5 * PI ) );
				float uB = 0.5 + 0.5 * sin( clamp( aB - n, -0.5 * PI, 0.5 * PI ) );
				uint bits = sectorBits( min( uF, uB ), max( uF, uB ) );
				uint fresh = bits & ~occ;
				occ |= bits;
				if ( fresh == 0u || !hit )
					continue;
				float el = lumaOf( rad );
				// FO4CS's small-emitter boost: min(1/sqrt(coverage), 16), for footprints of 1..3 game units only
				float wpp = 2.0 * gs.w / ( proj.y * float( fullSize.y ) );
				float foot = wpp * exp2( clamp( floor( selMip + 0.5 ), 1.0, 4.0 ) );
				float boost = 1.0;
				if ( el > 1.0e-6 && cov > 0.0 )
					boost = 1.0 + ( min( inversesqrt( max( clamp( cov, 0.0, 1.0 ), 1.0 / 256.0 ) ), 16.0 ) - 1.0 )
						* ( 1.0 - smoothstep( 1.0, 3.0, foot ) );
				float raw = dot( normalize( gs.xyz ), -dF / len );
				float facing = clamp( raw, 0.0, 1.0 );
				// FO4CS: an emitter turned away still lights what lies within 64 units, at a wrapped facing
				if ( facing <= 0.0 )
					facing = clamp( ( raw + 0.5 ) / 1.5, 0.0, 1.0 ) * ( 1.0 - smoothstep( 48.0, 64.0, len ) );
				// FO4CS's emissive world range (256): the ordinary band's shape
				float rt = clamp( ( radius - len ) / max( 0.25 * radius, 1.0 ), 0.0, 1.0 );
				float share = float( bitCount( fresh ) ) / 32.0 * rt * rt * ( 3.0 - 2.0 * rt );
				sil += share * facing * rad * boost;
			}
		}
		il += sil * npLen;
		wsum += npLen;
	}
	if ( wsum <= 0.0 )
		return vec4( 0.0 );
	il = il / wsum * PI * sourceBrightness;
	float y = dot( il, vec3( 0.2126, 0.7152, 0.0722 ) );
	return vec4( max( mix( vec3( y ), il, saturation ), vec3( 0.0 ) ), 0.0 );
}

vec3 ffClamp( vec3 c, float mean, float span )
{
	float y = dot( c, vec3( 0.2126, 0.7152, 0.0722 ) ) + 1.0e-4;
	float l = log2( y );
	float lc = clamp( l, mean - span, mean + span );
	return c * exp2( lc - l );
}

// SSGI2FIX stage 12: the accumulated shut moment, a depth-aware 3x3, into the result's upper half (the
// surfaces tilt the probe toward it, cell_lights.glsl cellSsgiE)
vec4 bentOut( ivec2 wp )
{
	wp.y -= workSize.y;
	vec4 g = gAt( ivec2( ( vec2( wp ) + 0.5 ) / vec2( workSize ) * vec2( fullSize ) ) );
	if ( g.w <= 0.0 )
		return vec4( 0.0 );
	vec3 sum = vec3( 0.0 );
	float ws = 0.0;
	for ( int y = -1; y <= 1; y++ )
		for ( int x = -1; x <= 1; x++ ) {
			ivec2 q = clamp( wp + ivec2( x, y ), ivec2( 0 ), workSize - 1 );
			vec4 gq = gAt( ivec2( ( vec2( q ) + 0.5 ) / vec2( workSize ) * vec2( fullSize ) ) );
			float w = gq.w > 0.0 ? max( 1.0 - abs( gq.w - g.w ) / ( 0.05 * g.w ), 0.0 ) : 0.0;
			sum += texelFetch( src, q, 0 ).xyz * w;
			ws += w;
		}
	return vec4( sum / max( ws, 1.0e-4 ), 0.0 );
}

void main()
{
	ivec2 wp = ivec2( gl_FragCoord.xy );
	fragMeta = vec4( 0.0 );
	fragState = vec4( 0.0 );
	fragBent = vec4( 0.0 );
	if ( ssgiStage == 1 ) {
		vec3 bent;
		fragColor = cast( wp, bent );
		fragMeta = vec4( bent, 0.0 );
		return;
	}
	if ( ssgiStage == 8 ) {
		fragColor = castEmissive( wp );
		return;
	}
	if ( ssgiStage >= 4 ) {
		if ( ssgiStage == 4 )
			fragColor = robustBlur( wp );
		else if ( ssgiStage == 5 )
			quarterDown( wp );
		else if ( ssgiStage == 6 )
			fragColor = quarterFilter( wp );
		else if ( ssgiStage == 7 ) {
			fragColor = quarterReconstruct( wp );
			if ( emitAdd )
				fragColor.rgb += texelFetch( emitFin, wp, 0 ).rgb;
		}
		else if ( ssgiStage == 12 )
			fragColor = bentOut( wp );
		else
			fragColor = vec4( hizReduce( wp, ssgiStage == 10 ), 0.0, 1.0 );
		return;
	}
	vec2 uv = ( vec2( wp ) + 0.5 ) / vec2( workSize );
	ivec2 ft = ivec2( uv * vec2( fullSize ) );
	vec4 g = gAt( ft );
	if ( ssgiStage == 2 ) {
		vec4 now = texelFetch( src, wp, 0 );
		vec3 bNow = texelFetch( srcBent, wp, 0 ).xyz;
		fragBent = vec4( bNow, 0.0 );
		float n = 1.0;
		vec4 outc = now;
		vec4 ps = vec4( 0.0 );
		bool accepted = false;
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
					fragBent = vec4( mix( transpose( curToPrev ) * texelFetch( histBent, pt, 0 ).xyz, bNow, w ), 0.0 );
					if ( wantState ) {
						ps = texelFetch( tstate, pt, 0 );
						accepted = ps.w >= 1.0 && ps.z > 1.0e-4;
					}
				}
			}
		}
		fragColor = outc;
		if ( wantState ) {
			// FO4CS's temporal moments of the raw cast's log-luma: age cap 16, confidence grows with maturity
			float l = logLuma( now.rgb );
			float age = accepted ? min( ps.w + 1.0, 16.0 ) : 1.0;
			float rw = 1.0 / age;
			float conf = accepted ? clamp( mix( ps.z, 1.0, 0.15 + 0.10 * clamp( ( age - 1.0 ) / 15.0, 0.0, 1.0 ) ), 0.0, 1.0 ) : 0.25;
			fragState = g.w > 0.0 ? vec4( accepted ? mix( ps.x, l, rw ) : l, accepted ? mix( ps.y, l * l, rw ) : l * l, conf, age )
				: vec4( 0.0 );
		}
		vec3 nm = g.w > 0.0 ? faceN( normalize( g.xyz ), viewPos( ( vec2( ft ) + 0.5 ) / vec2( fullSize ), g.w ) ) : vec3( 0.0, 0.0, 1.0 );
		fragMeta = vec4( g.w > 0.0 ? g.w : 0.0, n, nm.xy );
		return;
	}
	// stage 3: the denoise
	vec4 c0 = texelFetch( src, wp, 0 );
	vec3 eAdd = emitAdd ? texelFetch( emitFin, wp, 0 ).rgb : vec3( 0.0 );
	if ( g.w <= 0.0 || blurRadius <= 0.0 ) {
		fragColor = c0 + vec4( eAdd, 0.0 );
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
	fragColor = sum / ws + vec4( eAdd, 0.0 );
}
