// HLSL source of features/gi/cell_ssgi.frag (fragment shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "GiBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#ifndef WW_GLSL_SIZE_Texture2D_float4_
#define WW_GLSL_SIZE_Texture2D_float4_
int2 textureSize(Texture2D<float4> t, int lod) { uint w, h, l; t.GetDimensions(uint(lod), w, h, l); return int2(w, h); }
#endif
static float4 gl_FragCoord;
/* lane SSGI1: screen-space GI for the cell view (src/features/gi/cellssgi.cpp), the method of our FO4CS's SSGI in our own
 * GLSL: a visibility-bitmask horizon march per slice. Inputs: gbuf (lane AO1's opaque pass: view normal, GL axes,
 * linear depth in game units; full size, bottom row first) and scene (the opaque lit linear colour without
 * reflection, obscurance and fog: lane SSR1's scene pass, full size, bottom row first). Every SSGI target is
 * stored bottom row first at the work size (full or half).
 *   stage 1  the cast: per slice, both sides, `steps` taps out to `radius` game units; each tap's front and back
 *            (front + thickness) angles cover sectors of a 32-bit mask over the slice's cosine-weighted hemisphere;
 *            sectors newly covered take the tap's radiance x its facing. rgb = bounce irradiance E (x pi),
 *            a = the unoccluded (cosine-weighted) share
 *            (lane SSGI3: a = the share the probe keeps; a sector leaves it only as far as the screen refills it)
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
 *            its donor without the emission (emitSub), so each emitter is counted once */

cbuffer LooseUniforms : register(b31, space0)
{
	int ssgiStage;
	int2 fullSize;
	int2 workSize;
	float2 proj;				// the projection's P00, P11
	int slices;
	int steps;
	float radius;			// game units
	float thickness;		// game units behind a tap's front face
	float sourceBrightness;
	float saturation;
	float historyBlend;
	float maxAccum;
	float blurRadius;		// work pixels
	float normalWeight;		// the denoise's normal similarity power (0: depth only)
	int frameIndex;
	bool historyValid;
	float motionAccum;		// the age cap while the camera moves (FO4CS fMotionAccumFrames)
	bool moving;
	row_major float3x3 curToPrev;			// game-unit view now -> game-unit view last frame (rotation)
	float3 curToPrevT;
	bool blueNoise;			// false: red `ign`, the closed-form IGN of SSGI1
	int hizLevels;			// 0: red `nohiz` (no interval skip, no early out)
	bool fullBreak;			// lane PERFFIX2 (3c): the full-mask early out also without the pyramid (not under `nohiz`)
	bool wantState;
	float robustRadius;		// stage 4, work pixels (FO4CS fBlurRadius)
	int robustTaps;			// lane PERFFIX2 (3d): stage 4's Poisson taps, the tier's (FO4CS: all 16)
	int2 quarterSize;
	bool emitSub;			// stage 1: the donor less the emission (the emissive cast carries it)
	int emitSlices;			// stage 8 (FO4CS iEmissiveSlices)
	bool emissivePass;		// stages 4, 7: the emissive cast's clip spans and retention
	bool emitAdd;
	bool oldLaw;			// lane SSGI3: red `oldlaw`, SSGI1's fill law (stage 1)
};

Texture2D<float4> gbuf : register(t0, space0); SamplerState _gbuf_sampler : register(s0, space0);
Texture2D<float4> scene : register(t1, space0); SamplerState _scene_sampler : register(s1, space0);
Texture2D<float4> src : register(t2, space0); SamplerState _src_sampler : register(s2, space0);			// stage 2: this frame's cast; stage 3: the accumulated result
Texture2D<float4> hist : register(t3, space0); SamplerState _hist_sampler : register(s3, space0);			// stage 2: last frame's accumulated result
Texture2D<float4> histMeta : register(t4, space0); SamplerState _histMeta_sampler : register(s4, space0);		// stage 2: last frame's depth, n, packed normal
// lane SSGI2
Texture2D<float4> noiseTex : register(t5, space0); SamplerState _noiseTex_sampler : register(s5, space0);		// EA FAST spatiotemporal blue noise, 128 x (64 x 128) RG8
Texture2D<float4> hiz : register(t6, space0); SamplerState _hiz_sampler : register(s6, space0);			// min/max linear depth, level i = mip i + 1 (sky: min huge, max 0)
Texture2D<float4> tstate : register(t7, space0); SamplerState _tstate_sampler : register(s7, space0);		// stage 2: last frame's moments; stages 4, 7: this frame's
Texture2D<float4> qraw : register(t8, space0); SamplerState _qraw_sampler : register(s8, space0);			// stages 6, 7: the quarter downsample
Texture2D<float4> qguide : register(t9, space0); SamplerState _qguide_sampler : register(s9, space0);		// its guide: normal, linear depth
Texture2D<float4> qfilt : register(t10, space0); SamplerState _qfilt_sampler : register(s10, space0);		// stage 7: the filtered quarter
Texture2D<float4> emitTex : register(t11, space0); SamplerState _emitTex_sampler : register(s11, space0);		// stages 1, 8: the scene pass's emitted share, full size, mips 0..4, a = coverage
Texture2D<float4> emitFin : register(t12, space0); SamplerState _emitFin_sampler : register(s12, space0);		// stages 3, 7: the emissive result, added to the ordinary one

static float4 fragColor;
static float4 fragMeta;
static float4 fragState;

static const float PI = 3.14159265;

float4 gAt( int2 fullTexel )
{
	return gbuf.Load( int3(clamp( fullTexel, (int2)( 0 ), fullSize - 1 ), 0) );
}

// game-unit view position at a full-size uv (0..1, bottom row first) and linear depth z
float3 viewPos( float2 uv, float z )
{
	float2 ndc = uv * 2.0 - 1.0;
	return float3( ndc.x * z / proj.x, ndc.y * z / proj.y, -z );
}

float2 viewToUv( float3 p )
{
	float z = max( -p.z, 1.0e-4 );
	return float2( p.x * proj.x / z, p.y * proj.y / z ) * 0.5 + 0.5;
}

// the normal turned toward the eye (the gbuf keeps a two-sided surface's drawn normal)
float3 faceN( float3 n, float3 P )
{
	return dot( n, -P ) < 0.0 ? -n : n;
}

// interleaved gradient noise, rotated each frame (the FO4CS cast reads a spatiotemporal blue-noise sheet; this is
// the closed-form stand-in, no third-party texture)
float ign( float2 p, int f )
{
	p += 5.588238 * float( f & 63 );
	return frac( 52.9829189 * frac( dot( p, float2( 0.06711056, 0.00583715 ) ) ) );
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

float4 cast( int2 wp )
{
	float2 uv = ( float2( wp ) + 0.5 ) / float2( workSize );
	int2 ft = int2( uv * float2( fullSize ) );
	float4 g = gAt( ft );
	if ( g.w <= 0.0 )
		return float4( 0.0, 0.0, 0.0, 1.0 );
	float3 P = viewPos( ( float2( ft ) + 0.5 ) / float2( fullSize ), g.w );
	float3 V = normalize( -P );
	float3 N = faceN( normalize( g.xyz ), P );
	// the radius in full-size pixels at this depth
	float rPx = radius * proj.y * 0.5 * float( fullSize.y ) / g.w;
	if ( rPx < 1.0 )
		return float4( 0.0, 0.0, 0.0, 1.0 );
	float n0 = ign( float2( wp ), frameIndex ), n1 = ign( float2( wp ) + float2( 37.0, 11.0 ), frameIndex + 17 );
	if ( blueNoise ) {
		// FO4CS's SpatioTemporalNoise: x the slice rotation, y the step jitter
		float2 bn = noiseTex.Load( int3(int2( wp.x & 127, ( frameIndex & 63 ) * 128 + ( wp.y & 127 ) ), 0) ).rg;
		n0 = bn.x;
		n1 = bn.y;
	}
	float3 il = (float3)( 0.0 );
	float vis = 0.0, wsum = 0.0;
	for ( int s = 0; s < slices; s++ ) {
		float phi = ( float( s ) + n0 ) * PI / float( slices );
		float2 dir = float2( cos( phi ), sin( phi ) );
		float3 d3 = float3( dir, 0.0 );
		float3 ortho = normalize( d3 - V * dot( d3, V ) );
		float3 axis = normalize( cross( ortho, V ) );
		float3 Np = N - axis * dot( N, axis );
		float npLen = length( Np );
		if ( npLen < 1.0e-4 )
			continue;
		float n = sign( dot( Np, ortho ) ) * acos( clamp( dot( Np, V ) / npLen, -1.0, 1.0 ) );
		uint occ = 0u;
		float3 sil = (float3)( 0.0 );
		float taken = 0.0;	// lane SSGI3: the cosine-weighted share whose probe light the screen replaces
		// FO4CS: one step jitter per slice, slid by the golden ratio
		float sliceStep = frac( n1 + float( s ) * 0.618034 );
		for ( int side = 0; side < 2; side++ ) {
			float sgn = side == 0 ? 1.0 : -1.0;
			// every later tap of this slice adds no fresh sector (FO4CS breaks the same way)
			if ( ( or( hizLevels > 0, fullBreak ) ) && occ == 0xFFFFFFFFu )
				break;
			for ( int k = 0; k < steps; k++ ) {
				float t = ( float( k ) + ( blueNoise ? sliceStep : frac( n1 + float( k ) * 0.618034 ) ) ) / float( steps );
				float off = max( t * t * rPx, 1.0 + float( k ) );
				float2 sp = float2( ft ) + 0.5 + sgn * dir * off;
				if ( or( or( or( sp.x < 0.0, sp.y < 0.0 ), sp.x >= float( fullSize.x ) ), sp.y >= float( fullSize.y ) ) )
					break;
				// lane PERFFIX2 (3c): the full-mask early out without the interval pyramid (fullBreak; exact)
				if ( ( or( hizLevels > 0, fullBreak ) ) && occ == 0xFFFFFFFFu )
					break;
				if ( hizLevels > 0 ) {
					// FO4CS's geometry mip: a cell whose whole depth interval is beyond z +- (radius + 1) holds no tap
					// within the radius (|dz| <= distance), so the skip drops exactly the taps `len > radius` drops
					int m = min( int( max( clamp( log2( off ) - 3.3, 0.0, 5.0 ) - 1.0, 0.0 ) + 0.5 ), hizLevels );
					if ( m > 0 ) {
						int2 c = min( int2( sp ) >> m, textureSize( hiz, m - 1 ) - 1 );
						float2 iv = hiz.Load( int3(c, m - 1) ).rg;
						if ( or( iv.x > g.w + radius + 1.0, iv.y < g.w - radius - 1.0 ) )
							continue;
					}
				}
				float4 gs = gAt( int2( sp ) );
				if ( gs.w <= 0.0 )
					continue;
				float3 S = viewPos( sp / float2( fullSize ), gs.w );
				float3 dF = S - P;
				float len = length( dF );
				if ( or( len > radius, len < 1.0e-3 ) )
					continue;
				float3 dB = dF - V * thickness;
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
					float share = float( countbits( fresh ) ) / 32.0 * rt * rt * ( 3.0 - 2.0 * rt );
					float3 Ns = normalize( gs.xyz );
					float facing = clamp( dot( Ns, -dF / len ), 0.0, 1.0 );
					// FO4CS's radiance mips: farther taps read a prefiltered level (log2 offset - 3.3, levels 0..4)
					float lod = clamp( log2( off ) - 3.3, 0.0, 4.0 );
					float3 L = scene.SampleLevel( _scene_sampler, sp / float2( fullSize ), lod ).rgb;
					if ( emitSub ) {
						// the level the scene's nearest-mip lookup took (GL: ceil(lod + 0.5) - 1 above 0.5), so the
						// emission comes off the same box mean
						float lvl = lod <= 0.5 ? 0.0 : ceil( lod + 0.5 ) - 1.0;
						L -= emitTex.SampleLevel( _emitTex_sampler, sp / float2( fullSize ), lvl ).rgb;
					}
					if ( oldLaw ) {
						sil += share * facing * max( L, (float3)( 0.0 ) );
					} else {
						// lane SSGI3, the fill law: a diffuse donor's radiance does not fall with its cosine, and a sector
						// takes the probe's light only in the share the screen refills (band, front face); the rest keeps it
						float front = clamp( facing * 10.0, 0.0, 1.0 );
						sil += share * front * max( L, (float3)( 0.0 ) );
						taken += share * front;
					}
					occ |= bits;
				}
			}
		}
		il += sil * npLen;
		vis += ( oldLaw ? 1.0 - float( countbits( occ ) ) / 32.0 : 1.0 - taken ) * npLen;
		wsum += npLen;
	}
	if ( wsum <= 0.0 )
		return float4( 0.0, 0.0, 0.0, 1.0 );
	il = il / wsum * PI * sourceBrightness;	// cosine-weighted share x radiance x pi = irradiance
	float y = dot( il, float3( 0.2126, 0.7152, 0.0722 ) );
	il = max( lerp( (float3)( y ), il, saturation ), (float3)( 0.0 ) );
	return float4( il, vis / wsum );
}

// ---- lane SSGI2: FO4CS's robust denoise and quarter pyramid (BlurCS, PyramidDownsample/Filter/ReconstructCS) ----
static const float2 kPoisson[16] = { float2( -0.94201624, -0.39906216 ), float2( 0.94558609, -0.76890725 ),
	float2( -0.09418410, -0.92938870 ), float2( 0.34495938, 0.29387760 ), float2( -0.91588581, 0.45771432 ),
	float2( -0.81544232, -0.87912464 ), float2( -0.38277543, 0.27676845 ), float2( 0.97484398, 0.75648379 ),
	float2( 0.44323325, -0.97511554 ), float2( 0.53742981, -0.47373420 ), float2( -0.26496911, -0.41893023 ),
	float2( 0.79197514, 0.19090188 ), float2( -0.24188840, 0.99706507 ), float2( -0.81409955, 0.91437590 ),
	float2( 0.19984126, 0.78641367 ), float2( 0.14383161, -0.14100790 ) };
static const float3 kLuma = float3( 0.2126, 0.7152, 0.0722 );
static const float kVarEps = 1.0e-4;

float lumaOf( float3 v )
{
	return dot( max( v, (float3)( 0.0 ) ), kLuma );
}

float logLuma( float3 v )
{
	return log2( 1.0 + lumaOf( v ) );
}

float logScale( float3 v, float target )
{
	float s = lumaOf( v );
	return s <= 1.0e-8 ? 1.0 : max( exp2( max( target, 0.0 ) ) - 1.0, 0.0 ) / s;
}

// the work pixel's guide: eye-facing view normal, linear depth (0: sky)
float4 guideAt( int2 w )
{
	int2 f = int2( ( float2( w ) + 0.5 ) / float2( workSize ) * float2( fullSize ) );
	float4 g = gAt( f );
	if ( g.w <= 0.0 )
		return float4( 0.0, 0.0, 1.0, 0.0 );
	return float4( faceN( normalize( g.xyz ), viewPos( ( float2( f ) + 0.5 ) / float2( fullSize ), g.w ) ), g.w );
}

float tapWeight( int tap, float3 P, float3 N, float3 T, float3 B, float halfAngle, float tol, out int2 q )
{
	q = (int2)( 0 );
	float3 pp = P + T * kPoisson[tap].x + B * kPoisson[tap].y;
	if ( pp.z > -1.0e-3 )
		return 0.0;
	float2 puv = viewToUv( pp );
	if ( or( any( (puv < (float2)( 0.0 )) ), any( (puv > (float2)( 1.0 )) ) ) )
		return 0.0;
	q = clamp( int2( puv * float2( workSize ) ), (int2)( 0 ), workSize - 1 );
	float4 gs = guideAt( q );
	if ( gs.w <= 0.0 )
		return 0.0;
	float3 S = viewPos( puv, gs.w );
	float w = exp( -0.66 * dot( kPoisson[tap], kPoisson[tap] ) );
	w *= clamp( 1.0 - abs( dot( N, S - P ) ) / tol, 0.0, 1.0 );
	float c = dot( gs.xyz, N );
	w *= smoothstep( 0.1, 0.6, c );
	// one normal serves as both geometric and shading normal here
	w *= 1.0 - clamp( acos( clamp( c, -1.0, 1.0 ) ) / max( halfAngle, 1.0e-4 ), 0.0, 1.0 );
	return w;
}

float4 robustBlur( int2 wp )
{
	float4 c0 = src.Load( int3(wp, 0) );
	float4 gd = guideAt( wp );
	if ( gd.w <= 0.0 )
		return c0;
	float3 P = viewPos( ( float2( int2( ( float2( wp ) + 0.5 ) / float2( workSize ) * float2( fullSize ) ) ) + 0.5 ) / float2( fullSize ), gd.w );
	float3 N = gd.xyz;
	float4 ts = tstate.Load( int3(wp, 0) );
	float conf = clamp( ts.z, 0.0, 1.0 );
	float tvar = max( ts.y - ts.x * ts.x, 0.0 );
	float need = max( 1.0 - conf, clamp( sqrt( tvar ) * 2.0, 0.0, 1.0 ) );
	float rad = robustRadius * lerp( 0.85, 2.0, need );
	float worldR = rad * gd.w * 2.0 / ( proj.x * float( workSize.x ) );
	// FO4CS GetBasis (branchless orthonormal basis)
	float sz = N.z >= 0.0 ? 1.0 : -1.0;
	float a = 1.0 / ( sz + N.z ), ya = N.y * a, b = N.x * ya, cc = N.x * sz;
	float3 T = float3( cc * N.x * a - 1.0, sz * b, cc ) * worldR;
	float3 B = float3( b, N.y * ya - sz, N.y ) * worldR;
	float tol = max( 0.25 * worldR, 0.5 );
	float halfAngle = lerp( 0.5 * PI, 0.25 * PI, sqrt( conf ) );
	float cl = logLuma( c0.rgb );
	float refL = lerp( cl, max( ts.x, 0.0 ), conf );
	float loc = cl;
	int2 wm = workSize - 1;
	loc += logLuma( src.Load( int3(clamp( wp + int2( 1, 0 ), (int2)( 0 ), wm ), 0) ).rgb );
	loc += logLuma( src.Load( int3(clamp( wp + int2( -1, 0 ), (int2)( 0 ), wm ), 0) ).rgb );
	loc += logLuma( src.Load( int3(clamp( wp + int2( 0, 1 ), (int2)( 0 ), wm ), 0) ).rgb );
	loc += logLuma( src.Load( int3(clamp( wp + int2( 0, -1 ), (int2)( 0 ), wm ), 0) ).rgb );
	loc *= 0.2;
	float spanScale = clamp( max( max( refL, cl ), loc ), 0.0, 1.0 );
	float spanEps = kVarEps * spanScale;
	float tSigma = sqrt( tvar + spanEps );
	float pre = emissivePass ? max( 6.0 * tSigma, 1.25 * spanScale ) : max( 4.0 * tSigma, 0.75 * spanScale );
	float ls = cl, ls2 = cl * cl, lw = 1.0;
	int2 qs[16];
	float ws[16];
	int nTaps = clamp( robustTaps, 1, 16 );
	for ( int i = 0; i < nTaps; i++ ) {
		ws[i] = tapWeight( i, P, N, T, B, halfAngle, tol, qs[i] );
		if ( ws[i] <= 1.0e-4 )
			continue;
		float rel = lerp( 0.5, 1.0, clamp( tstate.Load( int3(qs[i], 0) ).z, 0.0, 1.0 ) );
		float l = clamp( logLuma( src.Load( int3(qs[i], 0) ).rgb ), max( refL - pre, 0.0 ), refL + pre );
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
	float cRel = lerp( 0.35, 1.0, conf ) / ( 1.0 + 2.0 * sqrt( tvar + kVarEps ) );
	float3 sum = c0.rgb * ( logScale( c0.rgb, clamp( cl, lo, hi ) ) * cRel );
	float sumA = c0.a * cRel;
	float wsum = cRel;
	for ( int i = 0; i < nTaps; i++ ) {
		if ( ws[i] <= 1.0e-4 )
			continue;
		float4 cq = src.Load( int3(qs[i], 0) );
		float4 sq = tstate.Load( int3(qs[i], 0) );
		float rel = lerp( 0.35, 1.0, clamp( sq.z, 0.0, 1.0 ) ) / ( 1.0 + 2.0 * sqrt( max( sq.y - sq.x * sq.x, 0.0 ) + kVarEps ) );
		float l = logLuma( cq.rgb );
		float w = ws[i] * rel;
		sum += cq.rgb * ( logScale( cq.rgb, clamp( l, lo, hi ) ) * w );
		// the open share rides the same guide weights, unclamped (SSGI1's blur smoothed it too)
		sumA += cq.a * w;
		wsum += w;
	}
	return float4( sum, sumA ) / wsum;
}

float depthMetric( float z )
{
	return z > 0.0 ? z : 1.0e30;
}

// stage 5: work -> quarter, the nearest depth of the 2x2 is the reference (location 1: its guide)
void quarterDown( int2 qp )
{
	int2 o = qp * 2;
	float4 gd[4];
	int2 px[4];
	int ref = 0;
	for ( int i = 0; i < 4; i++ ) {
		px[i] = clamp( o + int2( i & 1, i >> 1 ), (int2)( 0 ), workSize - 1 );
		gd[i] = guideAt( px[i] );
		if ( depthMetric( gd[i].w ) < depthMetric( gd[ref].w ) )
			ref = i;
	}
	if ( gd[ref].w <= 0.0 ) {
		fragColor = src.Load( int3(px[ref], 0) );
		fragMeta = gd[ref];
		return;
	}
	float4 sum = (float4)( 0.0 );
	float3 ns = (float3)( 0.0 );
	float ws = 0.0;
	for ( int i = 0; i < 4; i++ ) {
		if ( gd[i].w <= 0.0 )
			continue;
		float wn = pow( clamp( ( dot( gd[ref].xyz, gd[i].xyz ) - 0.25 ) / 0.75, 0.0, 1.0 ), 4.0 );
		float w = exp2( -4.0 * abs( gd[i].w - gd[ref].w ) / max( 0.04 * gd[ref].w, 0.05 ) ) * wn;
		sum += src.Load( int3(px[i], 0) ) * w;
		ns += gd[i].xyz * w;
		ws += w;
	}
	if ( ws <= 1.0e-5 ) {
		fragColor = src.Load( int3(px[ref], 0) );
		fragMeta = gd[ref];
		return;
	}
	fragColor = sum / ws;
	fragMeta = float4( normalize( ns ), gd[ref].w );
}

// stage 6: the quarter's 5x5 bilateral
float4 quarterFilter( int2 qp )
{
	float4 c = qguide.Load( int3(qp, 0) );
	float4 c0 = qraw.Load( int3(qp, 0) );
	if ( c.w <= 0.0 )
		return c0;
	float3 sum = (float3)( 0.0 );
	float ws = 0.0;
	for ( int y = -2; y <= 2; y++ )
		for ( int x = -2; x <= 2; x++ ) {
			int2 q = clamp( qp + int2( x, y ), (int2)( 0 ), quarterSize - 1 );
			float4 s = qguide.Load( int3(q, 0) );
			if ( s.w <= 0.0 )
				continue;
			float wd = exp2( -3.0 * abs( s.w - c.w ) / max( 0.06 * c.w, 0.10 ) );
			float wn = pow( clamp( ( dot( c.xyz, s.xyz ) - 0.30 ) / 0.70, 0.0, 1.0 ), 4.0 );
			float w = exp2( -0.35 * float( x * x + y * y ) ) * wd * wn;
			sum += qraw.Load( int3(q, 0) ).rgb * w;
			ws += w;
		}
	return float4( sum / max( ws, 1.0e-5 ), c0.a );
}

// stage 7: filtered quarter + the robust residual x retention
float4 quarterReconstruct( int2 wp )
{
	float4 rb = src.Load( int3(wp, 0) );
	float4 gd = guideAt( wp );
	if ( gd.w <= 0.0 )
		return rb;
	float2 qpos = ( float2( wp ) + 0.5 ) / float2( workSize ) * float2( quarterSize ) - 0.5;
	int2 base = int2( floor( qpos ) );
	float2 fr = qpos - float2( base );
	float3 raw = (float3)( 0.0 ), filt = (float3)( 0.0 );
	float ws = 0.0;
	for ( int y = 0; y <= 1; y++ )
		for ( int x = 0; x <= 1; x++ ) {
			int2 q = clamp( base + int2( x, y ), (int2)( 0 ), quarterSize - 1 );
			float4 s = qguide.Load( int3(q, 0) );
			float bl = ( x == 0 ? 1.0 - fr.x : fr.x ) * ( y == 0 ? 1.0 - fr.y : fr.y );
			float wd = s.w > 0.0 ? clamp( 1.0 - abs( s.w - gd.w ) / max( 0.06 * gd.w, 0.10 ), 0.0, 1.0 ) : 0.0;
			float wn = pow( clamp( ( dot( gd.xyz, s.xyz ) - 0.25 ) / 0.75, 0.0, 1.0 ), 3.0 );
			float w = bl * wd * wn;
			raw += qraw.Load( int3(q, 0) ).rgb * w;
			filt += qfilt.Load( int3(q, 0) ).rgb * w;
			ws += w;
		}
	if ( ws > 1.0e-5 ) {
		raw /= ws;
		filt /= ws;
	} else {
		int2 q = clamp( int2( round( qpos ) ), (int2)( 0 ), quarterSize - 1 );
		raw = qraw.Load( int3(q, 0) ).rgb;
		filt = qfilt.Load( int3(q, 0) ).rgb;
	}
	float4 ts = tstate.Load( int3(wp, 0) );
	float var = max( ts.y - ts.x * ts.x, 0.0 );
	float relRes = dot( abs( rb.rgb - raw ), kLuma ) / max( dot( max( filt, (float3)( 0.0 ) ), kLuma ), 1.0e-5 );
	float keep = emissivePass
		? lerp( 0.08, 0.35, clamp( ts.z, 0.0, 1.0 ) ) / ( 1.0 + relRes * 1.25 ) / ( 1.0 + sqrt( var ) * 4.0 )
		: lerp( 0.04, 0.24, clamp( ts.z, 0.0, 1.0 ) ) / ( 1.0 + relRes * 2.0 ) / ( 1.0 + sqrt( var ) * 6.0 );
	return float4( max( filt + ( rb.rgb - raw ) * keep, (float3)( 0.0 ) ), rb.a );
}

// stages 10/11: the min/max depth-interval pyramid; texel i covers the finer texels 2i, 2i+1 and, on the last
// row/column of an odd-sized finer level, 2i+2 as well, so every finer texel lies in the cell the cast looks up
float2 hizReduce( int2 p, bool fromGbuf )
{
	int2 fs = fromGbuf ? fullSize : textureSize( hiz, 0 );
	int2 os = max( fs / 2, (int2)( 1 ) );
	int2 n = (int2)( 2 ) + int2( (p == (os - 1)) ) * int2( (fs > (os * 2)) );
	float2 r = float2( 1.0e30, 0.0 );
	for ( int y = 0; y < 3; y++ )
		for ( int x = 0; x < 3; x++ ) {
			if ( or( x >= n.x, y >= n.y ) )
				continue;
			int2 q = min( p * 2 + int2( x, y ), fs - 1 );
			if ( fromGbuf ) {
				float z = gAt( q ).w;
				if ( z > 0.0 )
					r = float2( min( r.x, z ), max( r.y, z ) );
			} else {
				float2 iv = hiz.Load( int3(q, 0) ).rg;
				r = float2( min( r.x, iv.x ), max( r.y, iv.y ) );
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

int selectIdx( float4 w, float r, out float p )
{
	float tot = dot( w, (float4)( 1.0 ) );
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

void addLod( inout float4 w, float lod, float scale )
{
	float l = clamp( lod, 1.0, 4.0 );
	int lo = int( floor( l ) );
	int hi = min( lo + 1, 4 );
	float f = frac( l );
	w[lo - 1] += scale * ( 1.0 - f );
	w[hi - 1] += scale * f;
}

// an emitting mip-0 texel inside the footprint at fuv (level emip), drawn by luminance; rad = its emission over the
// draw's probability (the footprint's mean in expectation), cov = the footprint's emitting share
bool sampleEmission( float2 fuv, float emip, uint key, out int2 leaf, out float3 rad, out float cov, out float selMip )
{
	leaf = (int2)( 0 );
	rad = (float3)( 0.0 );
	cov = 0.0;
	selMip = emip;
	float coarseMip = min( emip + 1.0, 4.0 );
	float coarseW = 0.5 * clamp( 4.0 - emip, 0.0, 1.0 );	// FO4CS kEmissionFootprintCoarseWeight
	float4 fp = max( lerp( emitTex.SampleLevel( _emitTex_sampler, fuv, emip ), emitTex.SampleLevel( _emitTex_sampler, fuv, coarseMip ), coarseW ), (float4)( 0.0 ) );
	if ( lumaOf( fp.rgb ) <= 1.0e-6 )
		return false;
	float4 tw = (float4)( 0.0 );
	addLod( tw, emip, 1.0 - coarseW );
	addLod( tw, coarseMip, coarseW );
	float4 sw = (float4)( 0.0 );
	for ( int i = 0; i < 4; i++ )
		if ( tw[i] > 0.0 )
			sw[i] = tw[i] * lumaOf( emitTex.SampleLevel( _emitTex_sampler, fuv, float( i + 1 ) ).rgb );
	if ( dot( sw, (float4)( 1.0 ) ) <= 1.0e-8 )
		return false;
	float p;
	int mi = selectIdx( sw, impRand( key, 0u ), p );
	int sm = mi + 1;
	selMip = float( sm );
	float norm = tw[mi] / max( p, 1.0e-8 );
	int2 ms = max( fullSize >> sm, (int2)( 1 ) );
	float2 mpos = fuv * float2( ms ) - 0.5;
	int2 mb = int2( floor( mpos ) );
	float2 mf = mpos - float2( mb );
	float4 bw = float4( ( 1.0 - mf.x ) * ( 1.0 - mf.y ), mf.x * ( 1.0 - mf.y ), ( 1.0 - mf.x ) * mf.y, mf.x * mf.y );
	int2 cc[4];
	float4 cp[4];
	float4 cw = (float4)( 0.0 );
	for ( int c = 0; c < 4; c++ ) {
		cc[c] = clamp( mb + int2( c & 1, c >> 1 ), (int2)( 0 ), ms - 1 );
		cp[c] = max( emitTex.Load( int3(cc[c], sm) ), (float4)( 0.0 ) );
		cw[c] = bw[c] * lumaOf( cp[c].rgb );
	}
	if ( dot( cw, (float4)( 1.0 ) ) <= 1.0e-8 )
		return false;
	int sc = selectIdx( cw, impRand( key, 1u ), p );
	norm *= bw[sc] / max( p, 1.0e-8 );
	int2 coord = cc[sc];
	float4 pay = cp[sc];
	for ( int pm = sm; pm > 0; pm-- ) {
		int chm = pm - 1;
		int2 chs = max( fullSize >> chm, (int2)( 1 ) );
		int2 kc[4];
		float4 kp[4];
		float4 kw = (float4)( 0.0 );
		for ( int k = 0; k < 4; k++ ) {
			kc[k] = clamp( coord * 2 + int2( k & 1, k >> 1 ), (int2)( 0 ), chs - 1 );
			kp[k] = max( emitTex.Load( int3(kc[k], chm) ), (float4)( 0.0 ) );
			kw[k] = lumaOf( kp[k].rgb );
		}
		if ( dot( kw, (float4)( 1.0 ) ) <= 1.0e-8 )
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
float4 castEmissive( int2 wp )
{
	float2 uv = ( float2( wp ) + 0.5 ) / float2( workSize );
	int2 ft = int2( uv * float2( fullSize ) );
	float4 g = gAt( ft );
	if ( g.w <= 0.0 )
		return (float4)( 0.0 );
	float3 P = viewPos( ( float2( ft ) + 0.5 ) / float2( fullSize ), g.w );
	float3 V = normalize( -P );
	float3 N = faceN( normalize( g.xyz ), P );
	float rPx = radius * proj.y * 0.5 * float( fullSize.y ) / g.w;
	if ( rPx < 1.0 )
		return (float4)( 0.0 );
	float n0 = ign( float2( wp ), frameIndex ), n1 = ign( float2( wp ) + float2( 37.0, 11.0 ), frameIndex + 17 );
	if ( blueNoise ) {
		float2 bn = noiseTex.Load( int3(int2( wp.x & 127, ( frameIndex & 63 ) * 128 + ( wp.y & 127 ) ), 0) ).rg;
		n0 = bn.x;
		n1 = bn.y;
	}
	float3 il = (float3)( 0.0 );
	float wsum = 0.0;
	for ( int s = 0; s < emitSlices; s++ ) {
		float phi = ( float( s ) + n0 ) * PI / float( emitSlices );
		float2 dir = float2( cos( phi ), sin( phi ) );
		float3 d3 = float3( dir, 0.0 );
		float3 ortho = normalize( d3 - V * dot( d3, V ) );
		float3 axis = normalize( cross( ortho, V ) );
		float3 Np = N - axis * dot( N, axis );
		float npLen = length( Np );
		if ( npLen < 1.0e-4 )
			continue;
		float n = sign( dot( Np, ortho ) ) * acos( clamp( dot( Np, V ) / npLen, -1.0, 1.0 ) );
		uint occ = 0u;
		float3 sil = (float3)( 0.0 );
		float sliceStep = frac( n1 + float( s ) * 0.618034 );
		for ( int side = 0; side < 2 && occ != 0xFFFFFFFFu; side++ ) {
			float sgn = side == 0 ? 1.0 : -1.0;
			for ( int k = 0; k < steps && occ != 0xFFFFFFFFu; k++ ) {
				float t = ( float( k ) + sliceStep ) / float( steps );
				float off = max( t * t * rPx, 1.0 + float( k ) );
				float2 sp = float2( ft ) + 0.5 + sgn * dir * off;
				if ( or( or( or( sp.x < 0.0, sp.y < 0.0 ), sp.x >= float( fullSize.x ) ), sp.y >= float( fullSize.y ) ) )
					break;
				// FO4CS's emission level for this step (its radianceMip), in full-size levels 1..4
				float emip = clamp( max( clamp( log2( off ) - 3.3, 0.0, 5.0 ), 2.0 ) - 1.0, 1.0, 4.0 );
				uint key = uint( wp.x ) + uint( wp.y ) * uint( workSize.x );
				key ^= uint( frameIndex ) * 0xA511E9B3u;
				key ^= uint( s ) * 0x63D83595u;
				key ^= uint( k ) * 0xB5297A4Du;
				key ^= uint( side ) * 0x68E31DA4u;
				int2 leaf;
				float3 rad;
				float cov, selMip;
				bool hit = sampleEmission( sp / float2( fullSize ), emip, key, leaf, rad, cov, selMip );
				float2 dp = hit ? float2( leaf ) + 0.5 : sp;
				float4 gs = gAt( int2( dp ) );
				if ( gs.w <= 0.0 )
					continue;
				float3 S = viewPos( dp / float2( fullSize ), gs.w );
				float3 dF = S - P;
				float len = length( dF );
				if ( or( len > radius, len < 1.0e-3 ) )
					continue;
				float3 dB = dF - V * thickness;
				float aF = sgn * acos( clamp( dot( dF / len, V ), -1.0, 1.0 ) );
				float aB = sgn * acos( clamp( dot( normalize( dB ), V ), -1.0, 1.0 ) );
				float uF = 0.5 + 0.5 * sin( clamp( aF - n, -0.5 * PI, 0.5 * PI ) );
				float uB = 0.5 + 0.5 * sin( clamp( aB - n, -0.5 * PI, 0.5 * PI ) );
				uint bits = sectorBits( min( uF, uB ), max( uF, uB ) );
				uint fresh = bits & ~occ;
				occ |= bits;
				if ( or( fresh == 0u, !hit ) )
					continue;
				float el = lumaOf( rad );
				// FO4CS's small-emitter boost: min(1/sqrt(coverage), 16), for footprints of 1..3 game units only
				float wpp = 2.0 * gs.w / ( proj.y * float( fullSize.y ) );
				float foot = wpp * exp2( clamp( floor( selMip + 0.5 ), 1.0, 4.0 ) );
				float boost = 1.0;
				if ( el > 1.0e-6 && cov > 0.0 )
					boost = 1.0 + ( min( rsqrt( max( clamp( cov, 0.0, 1.0 ), 1.0 / 256.0 ) ), 16.0 ) - 1.0 )
						* ( 1.0 - smoothstep( 1.0, 3.0, foot ) );
				float raw = dot( normalize( gs.xyz ), -dF / len );
				float facing = clamp( raw, 0.0, 1.0 );
				// FO4CS: an emitter turned away still lights what lies within 64 units, at a wrapped facing
				if ( facing <= 0.0 )
					facing = clamp( ( raw + 0.5 ) / 1.5, 0.0, 1.0 ) * ( 1.0 - smoothstep( 48.0, 64.0, len ) );
				// FO4CS's emissive world range (256): the ordinary band's shape
				float rt = clamp( ( radius - len ) / max( 0.25 * radius, 1.0 ), 0.0, 1.0 );
				float share = float( countbits( fresh ) ) / 32.0 * rt * rt * ( 3.0 - 2.0 * rt );
				sil += share * facing * rad * boost;
			}
		}
		il += sil * npLen;
		wsum += npLen;
	}
	if ( wsum <= 0.0 )
		return (float4)( 0.0 );
	il = il / wsum * PI * sourceBrightness;
	float y = dot( il, float3( 0.2126, 0.7152, 0.0722 ) );
	return float4( max( lerp( (float3)( y ), il, saturation ), (float3)( 0.0 ) ), 0.0 );
}

float3 ffClamp( float3 c, float mean, float span )
{
	float y = dot( c, float3( 0.2126, 0.7152, 0.0722 ) ) + 1.0e-4;
	float l = log2( y );
	float lc = clamp( l, mean - span, mean + span );
	return c * exp2( lc - l );
}

void frag_main()
{
	int2 wp = int2( gl_FragCoord.xy );
	fragMeta = (float4)( 0.0 );
	fragState = (float4)( 0.0 );
	if ( or( ssgiStage == 1, ssgiStage == 8 ) ) {
		fragColor = ssgiStage == 1 ? cast( wp ) : castEmissive( wp );
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
				fragColor.rgb += emitFin.Load( int3(wp, 0) ).rgb;
		}
		else
			fragColor = float4( hizReduce( wp, ssgiStage == 10 ), 0.0, 1.0 );
		return;
	}
	float2 uv = ( float2( wp ) + 0.5 ) / float2( workSize );
	int2 ft = int2( uv * float2( fullSize ) );
	float4 g = gAt( ft );
	if ( ssgiStage == 2 ) {
		float4 now = src.Load( int3(wp, 0) );
		float n = 1.0;
		float4 outc = now;
		float4 ps = (float4)( 0.0 );
		bool accepted = false;
		if ( historyValid && g.w > 0.0 ) {
			float3 P = viewPos( ( float2( ft ) + 0.5 ) / float2( fullSize ), g.w );
			float3 Pp = mul(P, curToPrev) + curToPrevT;
			float2 puv = viewToUv( Pp );
			if ( all( (puv >= (float2)( 0.0 )) ) && all( (puv < (float2)( 1.0 )) ) ) {
				int2 pt = int2( puv * float2( workSize ) );
				float4 m = histMeta.Load( int3(pt, 0) );	// depth, n, normal xy (z toward the eye)
				float3 pn = float3( m.zw, sqrt( max( 1.0 - dot( m.zw, m.zw ), 0.0 ) ) );
				float nz = -Pp.z;
				bool sameZ = m.x > 0.0 && abs( m.x - nz ) <= 0.05 * nz;
				float3 nNow = normalize( mul(faceN( normalize( g.xyz ), P ), curToPrev) );
				bool sameN = dot( pn, nNow ) > 0.8;
				if ( sameZ && sameN ) {
					float4 h = hist.Load( int3(pt, 0) );
					n = min( m.y + 1.0, moving ? min( maxAccum, motionAccum ) : maxAccum );
					float w = max( 1.0 / n, 1.0 - historyBlend );
					outc = lerp( h, now, w );
					if ( wantState ) {
						ps = tstate.Load( int3(pt, 0) );
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
			float conf = accepted ? clamp( lerp( ps.z, 1.0, 0.15 + 0.10 * clamp( ( age - 1.0 ) / 15.0, 0.0, 1.0 ) ), 0.0, 1.0 ) : 0.25;
			fragState = g.w > 0.0 ? float4( accepted ? lerp( ps.x, l, rw ) : l, accepted ? lerp( ps.y, l * l, rw ) : l * l, conf, age )
				: (float4)( 0.0 );
		}
		float3 nm = g.w > 0.0 ? faceN( normalize( g.xyz ), viewPos( ( float2( ft ) + 0.5 ) / float2( fullSize ), g.w ) ) : float3( 0.0, 0.0, 1.0 );
		fragMeta = float4( g.w > 0.0 ? g.w : 0.0, n, nm.xy );
		return;
	}
	// stage 3: the denoise
	float4 c0 = src.Load( int3(wp, 0) );
	float3 eAdd = emitAdd ? emitFin.Load( int3(wp, 0) ).rgb : (float3)( 0.0 );
	if ( or( g.w <= 0.0, blurRadius <= 0.0 ) ) {
		fragColor = c0 + float4( eAdd, 0.0 );
		return;
	}
	float3 n0 = normalize( g.xyz );
	float stepPx = max( blurRadius * 0.5, 1.0 );
	// FO4CS's firefly suppression: every tap's log-luma is held to the footprint's mean +- max(2.5 sigma, 0.3)
	float lm = 0.0, lv = 0.0;
	for ( int y = -2; y <= 2; y++ )
		for ( int x = -2; x <= 2; x++ ) {
			int2 q = clamp( wp + int2( float2( x, y ) * stepPx ), (int2)( 0 ), workSize - 1 );
			float l = log2( dot( src.Load( int3(q, 0) ).rgb, float3( 0.2126, 0.7152, 0.0722 ) ) + 1.0e-4 );
			lm += l;
			lv += l * l;
		}
	lm /= 25.0;
	float span = max( 2.5 * sqrt( max( lv / 25.0 - lm * lm, 0.0 ) ), 0.3 );
	float4 sum = float4( ffClamp( c0.rgb, lm, span ), c0.a );
	float ws = 1.0;
	for ( int y = -2; y <= 2; y++ )
		for ( int x = -2; x <= 2; x++ ) {
			if ( x == 0 && y == 0 )
				continue;
			int2 q = wp + int2( float2( x, y ) * stepPx );
			if ( or( any( (q < (int2)( 0 )) ), any( (q >= workSize) ) ) )
				continue;
			float4 gq = gAt( int2( ( float2( q ) + 0.5 ) / float2( workSize ) * float2( fullSize ) ) );
			if ( gq.w <= 0.0 )
				continue;
			float wz = max( 1.0 - abs( gq.w - g.w ) / ( 0.05 * g.w ), 0.0 );
			float wn = pow( max( dot( n0, normalize( gq.xyz ) ), 0.0 ), 8.0 * normalWeight );
			float wd = exp( -float( x * x + y * y ) / 4.5 );
			float w = wz * wn * wd;
			float4 cq = src.Load( int3(q, 0) );
			sum += float4( ffClamp( cq.rgb, lm, span ), cq.a ) * w;
			ws += w;
		}
	fragColor = sum / ws + float4( eAdd, 0.0 );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
    float4 fragMeta : SV_Target1;
    float4 fragState : SV_Target2;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    stage_output.fragMeta = fragMeta;
    stage_output.fragState = fragState;
    return stage_output;
}
