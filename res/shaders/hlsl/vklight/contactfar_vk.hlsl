// profile: cs_6_0
// Copyright 2023 Sony Interactive Entertainment.
// Licensed under the Apache License, Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0).
//
// lane VKSHADOW3 (2026-11-15): the far contact tier (src/features/shadows/shadowset.cpp contactFarPass, lane GAPLOOK1 C7)
// on Vulkan compute, one kernel per GL draw. The math is the readable fragment twins' (res/shaders/hlsl/shadows/
// ww_contact_farmin_ps.hlsl, ww_contact_ps.hlsl, ww_contact_farcomp_ps.hlsl) in their order; a thread is the fragment at
// its pixel centre (gl_FragCoord.xy = id + 0.5).
// CFAR_STAGE 1 (farmin): T0 the g-buffer (RGBA32F: view normal, linear depth; 1e6 = none) -> out0 farDepth (RGBA32F, the
//   nearest surface of each divisor x divisor block). c[4] (fullW, fullH, -, -), i.x divisor.
// CFAR_STAGE 2 (march): T0 farDepth -> out0 farMask (R8). c[0] lightCoord (coarse pixels, w = sign of clip w), c[1]
//   sssParams (thickness, bilinear threshold, contrast, samples), c[3] (maskScale, sssFloor, -, -), c[4] (coarseW, coarseH,
//   -, -), c[5].xyz sssLightView; i.x sssPar, i.y sssExit. The far tier's sssReach is 0 (no reach), so it has no reach code.
// CFAR_STAGE 3 (farcomp): T0 the g-buffer, T1 farDepth, T2 farMask -> out0 the near mask (R8, read and written: GL's
//   GL_MIN blend; R8 rounding is monotone, so min before the store equals GL's min of the stored values). c[0] (fullW,
//   fullH, coarseW, coarseH), c[1] (maskScale, tolerance, divisor, -), c[2] farFade.
// Reds (gate only): CFAR_MIN_RED the block's first texel, CFAR_MARCH_RED the light's w sign flipped, CFAR_HALO_RED the
// tolerance 1e6 (no depth agreement), CFAR_INSIDE_RED farFade off (the tier at every distance).
#if CFAR_STAGE == 1
#define LPX_OUT0_FORMAT "rgba32f"
#define LPX_OUT0_TYPE float4
#else
#define LPX_OUT0_FORMAT "r8"
#define LPX_OUT0_TYPE float
#endif
#include "LightBindingsVk.hlsli"

#if CFAR_STAGE == 1
// ww_contact_farmin: each coarse texel keeps the NEAREST surface of its divisor x divisor block, with that texel's normal
[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !LpxTexel( id, size ) )
		return;
	const int2 fullSize = int2( LpxJobPc.c[4].xy );
	const int divisor = LpxJobPc.i.x;
	int2 base = int2( id.xy ) * divisor;
	float4 best = float4( 0.0f, 0.0f, 1.0f, 1e6f );
	for ( int j = 0; j < 8; j++ ) {
		if ( j >= divisor )
			break;
		for ( int i = 0; i < 8; i++ ) {
			if ( i >= divisor )
				break;
			int2 q = base + int2( i, j );
			if ( any( q >= fullSize ) )
				continue;
			float4 t = LPX_FETCH( LpxT0, q );
#ifdef CFAR_MIN_RED
			if ( best.a >= 9.0e5f )
				best = t;
#else
			if ( t.a < best.a )
				best = t;
#endif
		}
	}
	LpxOut0[id.xy] = best;
}
#endif

#if CFAR_STAGE == 2
// ww_contact (the march), over the coarse min depth, as contactFarPass sets it (maskScale 1, sssReach 0)
static const float kPlaneBias = 12.0f;
static const float kNormalReject = 0.1f;
static const float kEvidence = 4.0f;
static const float kEdgeBand = 16.0f;

float CfLin( float2 maskPx, float maskScale, int2 fullSize, out bool valid )
{
	int2 q = int2( floor( maskPx * maskScale ) );
	if ( any( q < 0 ) || any( q >= fullSize ) ) {
		valid = false;
		return 1e6f;
	}
	float l = LPX_FETCH( LpxT0, q ).a;
	valid = l < 9.0e5f;
	return l;
}

float3 CfNormal( float2 maskPx, float maskScale, int2 fullSize )
{
	int2 q = clamp( int2( floor( maskPx * maskScale ) ), int2( 0, 0 ), fullSize - 1 );
	return LPX_FETCH( LpxT0, q ).rgb;
}

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !LpxTexel( id, size ) )
		return;
	float4 lightCoord = LpxJobPc.c[0];
#ifdef CFAR_MARCH_RED
	lightCoord.w = -lightCoord.w;
#endif
	const float4 sssParams = LpxJobPc.c[1];
	const float maskScale = LpxJobPc.c[3].x, sssFloor = LpxJobPc.c[3].y;
	const int2 fullSize = int2( LpxJobPc.c[4].xy );
	const float3 sssLightView = LpxJobPc.c[5].xyz;
	const bool par = LpxJobPc.i.x != 0;
	const bool sssExit = LpxJobPc.i.y != 0;

	float2 p = float2( id.xy ) + 0.5f;
	bool v0;
	float lin0 = CfLin( p, maskScale, fullSize, v0 );
	float fragMask = 1.0f;
	if ( !v0 || lin0 <= 0.0f ) {
		LpxOut0[id.xy] = fragMask;
		return;
	}
	float2 delta = p - lightCoord.xy;
	bool majorX = abs( delta.x ) > abs( delta.y );
	float dist0 = majorX ? abs( delta.x ) : abs( delta.y );
	if ( dist0 < 1.0f ) {
		LpxOut0[id.xy] = fragMask;
		return;
	}
	// light in front (w > 0): the march heads for its vanishing point; behind: away from it
	float direction = lightCoord.w > 0.0f ? -1.0f : 1.0f;
	float2 stepXY = delta / dist0 * direction;	// one mask pixel along the major axis
	int n = int( sssParams.w );
	float contrast = sssParams.z;
	float4 lanes = float4( 1.0f, 1.0f, 1.0f, 1.0f );
	// the receiver plane: 1/z is affine along the line, its slope from the depths 4 pixels either side
	bool va4, vb4;
	float lf = CfLin( p + stepXY * 4.0f, maskScale, fullSize, va4 ), lr = CfLin( p - stepXY * 4.0f, maskScale, fullSize, vb4 );
	bool plane = par && va4 && vb4 && abs( lf - lr ) < 0.25f * lin0;
	float invSlope = plane ? ( 1.0f / lf - 1.0f / lr ) / 8.0f : 0.0f;
	float hard = 1.0f, support = 0.0f;
	int exitK = n;
	for ( int k = 1; k <= 384; k++ ) {
		if ( k > n )
			break;
		float dist = dist0 + float( k ) * direction;
		if ( dist < 1.0f )
			break;
		float2 q = p + stepXY * float( k );
		if ( par && exitK == n && ( any( ( q * maskScale ) < 0.0f ) || any( ( q * maskScale ) >= float2( fullSize ) ) ) )
			exitK = k;
		// Bend's minor-axis bilinear pair: the nearer neighbour across the line, point-filtered at an edge
		float minor = majorX ? q.y : q.x;
		float f = frac( minor ) - 0.5f;
		float2 off = majorX ? float2( 0.0f, f > 0.0f ? 1.0f : -1.0f ) : float2( f > 0.0f ? 1.0f : -1.0f, 0.0f );
		bool va, vb;
		float la = CfLin( q, maskScale, fullSize, va );
		float lb = CfLin( q + off, maskScale, fullSize, vb );
		if ( !va )
			continue;	// sky or off the screen: empty, never an occluder
		bool edge = !vb || abs( la - lb ) > la * sssParams.y;
		float ls = edge ? la : lerp( la, lb, abs( f ) );
		float rayLin = lin0 * dist0 / dist;	// the ray's depth at this pixel (1/z is affine along the line)
		float thick = max( sssFloor, sssParams.x * rayLin );
		float d = abs( ( rayLin - ls ) / thick - 1.0f );
		if ( par ) {
			// the receiver's own plane is no occluder (within the bias, ramped in over 32 samples)
			if ( plane ) {
				float planeLin = 1.0f / max( 1.0f / lin0 + invSlope * float( k ), 1e-9f );
				if ( ls > planeLin - kPlaneBias * clamp( float( k ) / 32.0f, 0.125f, 1.0f ) )
					d = 1e10f;
			}
			// a caster edge-on to the light
			if ( d < 1.0f && abs( dot( CfNormal( q, maskScale, fullSize ), sssLightView ) ) <= kNormalReject )
				d = 1e10f;
			// the completion fade: the last 16 samples weigh less
			float c = float( n - k ) / 16.0f;
			float fadeOut = k > n - 16 ? clamp( 1.0f - c, 0.0f, 1.0f ) * 0.75f : 0.0f;
			if ( k <= 4 ) {
				hard = min( hard, d );
				support += clamp( ( 1.0f - d ) * contrast, 0.0f, 1.0f );
			}
			d += fadeOut;
		}
		int lane = k & 3;
		lanes[lane] = min( lanes[lane], d );
		// every lane resolves to 0 from here; past the hard samples and the 16-pixel edge band nothing else can move
		if ( sssExit && k >= 4 && float( k ) * maskScale >= kEdgeBand && all( lanes <= 1.0f - 1.0f / contrast ) )
			break;
	}
	lanes = clamp( lanes * contrast + ( 1.0f - contrast ), 0.0f, 1.0f );
	fragMask = dot( lanes, float4( 0.25f, 0.25f, 0.25f, 0.25f ) );
	if ( par ) {
		// the hard samples count only with enough evidence; the ray that leaves the screen early fades out
		float h = lerp( 1.0f, clamp( hard * contrast + ( 1.0f - contrast ), 0.0f, 1.0f ), clamp( support / kEvidence, 0.0f, 1.0f ) );
		fragMask = min( h, fragMask );
		fragMask = lerp( 1.0f, fragMask, smoothstep( 0.0f, kEdgeBand, float( exitK ) * maskScale ) );
	}
	LpxOut0[id.xy] = fragMask;
}
#endif

#if CFAR_STAGE == 3
// ww_contact_farcomp: the coarse mask back at the mask's resolution through a depth-aware bilinear upsample, min into the
// near mask
[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !LpxTexel( id, size ) )
		return;
	const int2 fullSize = int2( LpxJobPc.c[0].xy ), coarseSize = int2( LpxJobPc.c[0].zw );
	const float maskScale = LpxJobPc.c[1].x;
	const int divisor = int( LpxJobPc.c[1].z );
#ifdef CFAR_HALO_RED
	const float tolerance = 1e6f;
#else
	const float tolerance = LpxJobPc.c[1].y;
#endif
	float4 farFade = LpxJobPc.c[2];
#ifdef CFAR_INSIDE_RED
	farFade.y = 0.0f;
#endif
	float fragMask = 1.0f;
	float2 full = ( float2( id.xy ) + 0.5f ) * maskScale;
	int2 fq = clamp( int2( floor( full ) ), int2( 0, 0 ), fullSize - 1 );
	float fine = LPX_FETCH( LpxT0, fq ).a;
	if ( !( fine >= 9.0e5f || fine <= 0.0f ) ) {
		float2 cp = full / float( divisor ) - 0.5f;
		int2 base = int2( floor( cp ) );
		float2 fr = cp - float2( base );
		float wsum = 0.0f, acc = 0.0f;
		for ( int j = 0; j < 2; j++ ) {
			for ( int i = 0; i < 2; i++ ) {
				int2 c = base + int2( i, j );
				if ( any( c < 0 ) || any( c >= coarseSize ) )
					continue;
				float bw = ( i == 0 ? 1.0f - fr.x : fr.x ) * ( j == 0 ? 1.0f - fr.y : fr.y );
				float cd = LPX_FETCH( LpxT1, c ).a;
				if ( bw <= 0.0f || cd >= 9.0e5f )
					continue;
				float w = bw * clamp( 1.0f - abs( fine - cd ) / fine / tolerance, 0.0f, 1.0f );
				if ( w <= 0.0f )
					continue;
				wsum += w;
				acc += w * LPX_FETCH( LpxT2, c ).r;
			}
		}
		if ( wsum > 1e-4f )
			fragMask = acc / wsum;
		// lane SHADOWBAND1: inside the cascades' reach the tier counts by 1 - their fade weight, (r / D)^8
		if ( farFade.y > 0.5f ) {
			float2 ndc = full / float2( fullSize ) * 2.0f - 1.0f;
			float r = fine * length( float3( ndc * farFade.zw, 1.0f ) );
			float s = clamp( r * r / max( farFade.x * farFade.x, 1.0f ), 0.0f, 1.0f );
			float s2 = s * s;
			fragMask = lerp( 1.0f, fragMask, s2 * s2 );
		}
	}
	LpxOut0[id.xy] = min( LpxOut0[id.xy], fragMask );
}
#endif
