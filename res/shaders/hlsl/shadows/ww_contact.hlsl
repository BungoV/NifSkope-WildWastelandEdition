// HLSL source of features/shadows/ww_contact.comp (compute shader), lane hlslsrc2 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: cs_6_0
#include "ShadowsBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static uint3 gl_WorkGroupID;
static uint3 gl_LocalInvocationID;
// Copyright 2023 Sony Interactive Entertainment.
// Licensed under the Apache License, Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0).
//
// lane PERFFIX2 (item 7): the contact march (ww_contact.frag: lane TERRSHADOW1's port of Bend Studio's screen-space
// shadows, FIX1101A's reach) in Bend's own layout: one 64-lane workgroup per light-space LINE. A line runs from the
// light's projected point through a band of 64 mask columns (major axis) and on toward the light; its depths (the
// minor-axis bilinear pair with the edge test, as the fragment pass reads them) are read ONCE per line position into
// shared memory, and lane t, the line's pixel at position t, marches positions t +- 1 .. t +- n out of it. Lines are
// anchored one minor pixel apart at the band's FAR column, so they are at most a pixel apart anywhere in the band and
// every pixel has one owner: the line nearest its centre (src/gl/contactlines.h, the band list and the owner rule,
// unit-tested in tests/contactlines). As in Bend's, the receiver's own depth is the line's sample at its position, so
// a plane never shadows itself out of the half-pixel between a pixel centre and its line.
// Per sample (Bend's "under 5 instructions"): the depths are kept x their distance to the light (S = depth x D, the
// ray is then the constant C = depth0 x D0) with 1 / (thickness floor x D) beside them, so a sample is two shared
// reads, a min, a multiply-add and an abs; positions with no occluder (sky, off screen, past the light) read S = R = 0
// (d = 1: no shadow, as the fragment pass's `continue` / `break`). The reach is a per-lane sample count, found by a
// bisection on the fragment pass's own distance test. Not for SHADOWPAR1's rules (its tiers keep the fragment pass).

#define WW_NUMTHREADS numthreads(64, 1, 1)	// the entry: [WW_NUMTHREADS] void main(...)
RWTexture2D<unorm float> maskOut : register(u0, space0);

struct Band { int4 a; };	// x sector (0 X+, 1 X-, 2 Y+, 3 Y-), y the anchor's column rank from the light, z first line, w first wave
StructuredBuffer<Band> bands : register(t0, space0);

Texture2D<float4> gbuf : register(t1, space0); SamplerState _gbuf_sampler : register(s1, space0);		// RGBA32F, bottom row first: view normal, linear depth (game units; 1e6 = none)
cbuffer LooseUniforms : register(b31, space0)
{
	int2 fullSize;
	int2 maskSize;
	float maskScale;	// full pixels per mask pixel (1 or 2)
	float4 lightCoord;	// the light's projected point in mask pixels (xy), unused z, w = sign of clip w
	float4 sssParams;		// thickness (fraction of depth), bilinear threshold, contrast, samples
	float sssFloor;		// the thickness floor, game units
	float4 sssReach;		// lane FIX1101A: x the reach (0 none), y its share of depth, z the cascades' reach D
	float2 sssTan;
	int bandCount;
	int waveCount;		// the dispatch is (32768, ceil(waves / 32768)) groups
	int4 sideFirst;	// per sector: the column (major index) nearest the light on that side
};


static const int kMaxN = 384;
groupshared float sS[64 + kMaxN + 1];	// per line position: its sample depth x its distance to the light (0: none)
groupshared float sR[64 + kMaxN + 1];	// 1 / (thickness floor x that distance) (0: none)
groupshared float sL[64];				// the band's own positions: the sample depth (-1: none), the receivers' depth

float3 sssViewPos( float2 maskPx, float z )
{
	float2 ndc = maskPx * maskScale / float2( fullSize ) * 2.0 - 1.0;
	return float3( ndc.x * sssTan.x * z, ndc.y * sssTan.y * z, z );
}

/* a line's minor coordinate at position u (rank kA - u): through the light and (anchor, j + 0.5), written about the
 * anchor so it keeps float precision with the light far off screen (Ln ~ 1e4): j + 0.5 - offset x u / Da. `precise`
 * (no fused or reordered math): every lane gets the same bits for the same line, which the owner rule needs */
float lineY( int j, float lineOff, int u, float Da )
{
	precise float r = float( u ) / Da;
	precise float y = ( float( j ) + 0.5 ) - lineOff * r;
	return y;
}

float linAt( float2 maskPx, out bool valid )
{
	int2 q = int2( floor( maskPx * maskScale ) );
	if ( or( any( (q < (int2)( 0 )) ), any( (q >= fullSize) ) ) ) {
		valid = false;
		return 1e6;
	}
	float l = gbuf.Load( int3(q, 0) ).a;
	valid = l < 9.0e5;
	return l;
}

void comp_main()
{
	int g = int( gl_WorkGroupID.y ) * 32768 + int( gl_WorkGroupID.x );
	if ( g >= waveCount )
		return;	// the whole group: before any barrier
	int lo = 0, hi = bandCount - 1;
	while ( lo < hi ) {
		int mid = ( lo + hi + 1 ) >> 1;
		if ( bands[mid].a.w <= g )
			lo = mid;
		else
			hi = mid - 1;
	}
	int4 b = bands[lo].a;
	int sector = b.x;
	bool majorX = sector < 2;
	float s = ( sector & 1 ) == 0 ? 1.0 : -1.0;	// the side's direction away from the light
	float Lm = majorX ? lightCoord.x : lightCoord.y, Ln = majorX ? lightCoord.y : lightCoord.x;
	float anchorC = float( sideFirst[sector] ) + s * float( b.y ) + 0.5;
	float Da = s * ( anchorC - Lm );
	int j = b.z + ( g - b.w );
	precise float lineOff = float( j ) + 0.5 - Ln;	// the line's minor offset at the anchor
	precise float lineOffPrev = float( j - 1 ) + 0.5 - Ln;
	float direction = lightCoord.w > 0.0 ? -1.0 : 1.0;	// toward the light (rank up) or away
	int n = min( int( sssParams.w ), kMaxN );
	int t = int( gl_LocalInvocationID.x );
	int base = direction < 0.0 ? 0 : n;	// shared index = position + base
	for ( int idx = t; idx < 64 + n + 1; idx += 64 ) {
		int u = idx - base;
		float c = anchorC - s * float( u );
		float D = s * ( c - Lm );
		float y = lineY( j, lineOff, u, Da );
		/* Bend's minor-axis bilinear pair with its edge test, between the two FULL-size texels around the line (at a
		 * half-size mask the fragment pass blends texels two rows apart by a mask-pixel weight, which a plane reads as
		 * a depth step every other column); at scale 1 it is the fragment pass's pair exactly */
		float yF = y * maskScale - 0.5;
		float r0 = floor( yF ), w = yF - r0;
		float cF = floor( c * maskScale ) + 0.5;
		bool va, vb, vn;
		float la = linAt( ( majorX ? float2( cF, r0 + 0.5 ) : float2( r0 + 0.5, cF ) ) / maskScale, va );
		float lb = linAt( ( majorX ? float2( cF, r0 + 1.5 ) : float2( r0 + 1.5, cF ) ) / maskScale, vb );
		float ln = w < 0.5 ? la : lb;	// the texel the line is in (the point filter at an edge)
		vn = w < 0.5 ? va : vb;
		bool edge = or( or( !va, !vb ), abs( la - lb ) > ln * sssParams.y );
		float ls = edge ? ln : lerp( la, lb, w );
		va = vn;
		bool use = va && D >= 1.0;
		sS[idx] = use ? ls * D : 0.0;
		sR[idx] = use ? 1.0 / ( sssFloor * D ) : 0.0;
		if ( u >= 0 && u < 64 )
			sL[u] = va ? ls : -1.0;
	}
	GroupMemoryBarrierWithGroupSync();
	// this lane's pixel and whether it owns it: the line nearest the pixel's centre
	float cT = anchorC - s * float( t );
	float dist0 = s * ( cT - Lm );
	float yT = lineY( j, lineOff, t, Da );
	int2 pix = majorX ? int2( int( floor( cT ) ), int( floor( yT ) ) ) : int2( int( floor( yT ) ), int( floor( cT ) ) );
	if ( or( or( dist0 <= 0.0, any( (pix < (int2)( 0 )) ) ), any( (pix >= maskSize) ) ) )
		return;
	float minorOff = floor( yT ) + 0.5 - Ln;
	bool inSector = majorX ? dist0 > abs( minorOff ) : dist0 >= abs( minorOff );	// the fragment pass's |dx| > |dy|
	// the owner: the first line (smallest j) landing in the pixel; lines are under a pixel apart here, so one always does
	if ( or( !inSector, floor( lineY( j - 1, lineOffPrev, t, Da ) ) >= floor( yT ) ) )
		return;
	float lin0 = sL[t];
	float mask = 1.0;
	if ( lin0 > 0.0 && dist0 >= 1.0 ) {
		float2 p = majorX ? float2( cT, yT ) : float2( yT, cT );
		int nk = n;
		if ( direction < 0.0 )
			nk = min( nk, int( floor( dist0 - 1.0 ) ) );	// past that the samples are beyond the light
		float C = lin0 * dist0;
		if ( sssReach.x > 0.0 && nk > 0 ) {
			// FIX1101A's reach: the march stops at the first sample further than reachEff from the receiver (in view
			// space the ray's samples move away monotonically), so a bisection finds the last one inside
			float3 pv0 = sssViewPos( p, lin0 );
			float ss = clamp( dot( pv0, pv0 ) / max( sssReach.z * sssReach.z, 1.0 ), 0.0, 1.0 );
			float s2 = ss * ss;
			float wgt = 1.0 - s2 * s2;
			float reachEff = wgt > 0.0 ? ( sssReach.x + sssReach.y * lin0 ) / wgt : 1e30;
			int kl = 0, kh = nk;	// sample kl is inside (0: the receiver), kh is the last candidate
			for ( int it = 0; it < 10 && kl < kh; it++ ) {
				int km = ( kl + kh + 1 ) >> 1;
				float dist = dist0 + float( km ) * direction;
				int um = t - int( direction ) * km;
				float yq = lineY( j, lineOff, um, Da );
				float2 q = majorX ? float2( anchorC - s * float( um ), yq ) : float2( yq, anchorC - s * float( um ) );
				if ( distance( sssViewPos( q, C / dist ), pv0 ) > reachEff )
					kh = km - 1;
				else
					kl = km;
			}
			nk = kl;
		}
		float Kc = 1.0 / ( sssParams.x * C );	// 1 / (thickness fraction x depth x distance)
		float4 lanes = (float4)( 1.0 );
		int step = -int( direction );	// shared index step per sample
		int i0 = t + base;
		int k = 1;
		for ( ; k + 3 <= nk; k += 4 ) {
			int i = i0 + step * k;
			float d1 = abs( ( C - sS[i] ) * min( sR[i], Kc ) - 1.0 );
			float d2 = abs( ( C - sS[i + step] ) * min( sR[i + step], Kc ) - 1.0 );
			float d3 = abs( ( C - sS[i + 2 * step] ) * min( sR[i + 2 * step], Kc ) - 1.0 );
			float d4 = abs( ( C - sS[i + 3 * step] ) * min( sR[i + 3 * step], Kc ) - 1.0 );
			// sample k lands in lane k & 3: k = 1 mod 4 here
			lanes = min( lanes, float4( d4, d1, d2, d3 ) );
		}
		for ( ; k <= nk; k++ ) {
			int i = i0 + step * k;
			float d = abs( ( C - sS[i] ) * min( sR[i], Kc ) - 1.0 );
			int lane = k & 3;
			lanes[lane] = min( lanes[lane], d );
		}
		float contrast = sssParams.z;
		lanes = clamp( lanes * contrast + ( 1.0 - contrast ), 0.0, 1.0 );
		mask = dot( lanes, (float4)( 0.25 ) );
	}
	maskOut[pix] = ((float4)( mask )).x;
}

struct SPIRV_Cross_Input
{
    uint3 gl_WorkGroupID : SV_GroupID;
    uint3 gl_LocalInvocationID : SV_GroupThreadID;
};

[WW_NUMTHREADS]
void main(SPIRV_Cross_Input stage_input)
{
    gl_WorkGroupID = stage_input.gl_WorkGroupID;
    gl_LocalInvocationID = stage_input.gl_LocalInvocationID;
    comp_main();
}
