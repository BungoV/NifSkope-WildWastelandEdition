// profile: cs_6_0
// Copyright 2023 Sony Interactive Entertainment.
// Licensed under the Apache License, Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0).
//
// lane VKLIGHT1 (2026-11-12): the near contact march (res/shaders/features/shadows/ww_contact.comp) on Vulkan compute.
// One 64-lane group per light-space line; the bands come from src/features/shadows/contactlines.h (wwContactBands), the
// dispatch is (min(waves, 32768), ceil(waves / 32768)) groups, the R8 mask is cleared to 1 first (the light's own pixel).
// T0 the g-buffer (RGBA32F: view normal, linear depth; 1e6 = none); out0 the R8 mask; binding 9 the bands (int4 each:
// sector, anchor rank, first line, first wave).
// Push: c[0] lightCoord, c[1] sssParams (thickness, bilinear threshold, contrast, samples), c[2] sssReach,
// c[3] (maskScale, sssFloor, sssTan.x, sssTan.y), c[4] (fullW, fullH, maskW, maskH), c[5] (bandCount, waveCount);
// i = sideFirst. CONTACT_RED (gate only): the thickness doubled.
#define LPX_OUT0_FORMAT "r8"
#define LPX_OUT0_TYPE float
#define LPX_BUF_TYPE int4
#include "LightBindingsVk.hlsli"
#include "ContactCommon.hlsli"

#define CT_MAXN 384
groupshared float sS[64 + CT_MAXN + 1];	// per line position: its sample depth x its distance to the light (0: none)
groupshared float sR[64 + CT_MAXN + 1];	// 1 / (thickness floor x that distance) (0: none)
groupshared float sL[64];				// the band's own positions: the sample depth (-1: none)

float CtLin( float2 maskPx, float maskScale, int2 fullSize, out bool valid )
{
	int2 q = int2( floor( maskPx * maskScale ) );
	if ( any( q < 0 ) || any( q >= fullSize ) ) {
		valid = false;
		return 1e6f;
	}
	float l = LpxT0.Load( int3( q, 0 ) ).a;
	valid = l < 9.0e5f;
	return l;
}

[numthreads( 64, 1, 1 )]
void main( uint3 gid : SV_GroupID, uint3 lt : SV_GroupThreadID )
{
	const float4 lightCoord = LpxJobPc.c[0];
	const float4 sssParams = LpxJobPc.c[1];
	const float4 sssReach = LpxJobPc.c[2];
	const float maskScale = LpxJobPc.c[3].x, sssFloor = LpxJobPc.c[3].y;
	const float2 sssTan = LpxJobPc.c[3].zw;
	const int2 fullSize = int2( LpxJobPc.c[4].xy ), maskSize = int2( LpxJobPc.c[4].zw );
	const int bandCount = int( LpxJobPc.c[5].x ), waveCount = int( LpxJobPc.c[5].y );
	const int4 sideFirst = LpxJobPc.i;

	int g = int( gid.y ) * 32768 + int( gid.x );
	if ( g >= waveCount )
		return;	// the whole group: before any barrier
	int lo = 0, hi = bandCount - 1;
	while ( lo < hi ) {
		int mid = ( lo + hi + 1 ) >> 1;
		if ( LpxBuf[mid].w <= g )
			lo = mid;
		else
			hi = mid - 1;
	}
	int4 b = LpxBuf[lo];
	int sector = b.x;
	bool majorX = sector < 2;
	float s = ( sector & 1 ) == 0 ? 1.0f : -1.0f;
	float Lm = majorX ? lightCoord.x : lightCoord.y, Ln = majorX ? lightCoord.y : lightCoord.x;
	float anchorC = float( sideFirst[sector] ) + s * float( b.y ) + 0.5f;
	float Da = s * ( anchorC - Lm );
	int j = b.z + ( g - b.w );
	precise float lineOff = float( j ) + 0.5f - Ln;
	precise float lineOffPrev = float( j - 1 ) + 0.5f - Ln;
	float direction = lightCoord.w > 0.0f ? -1.0f : 1.0f;
	int n = min( int( sssParams.w ), CT_MAXN );
	int t = int( lt.x );
	int base = direction < 0.0f ? 0 : n;
	for ( int idx = t; idx < 64 + n + 1; idx += 64 ) {
		int u = idx - base;
		float c = anchorC - s * float( u );
		float D = s * ( c - Lm );
		float y = F4FX_ContactLineY( j, lineOff, u, Da );
		float yF = y * maskScale - 0.5f;
		float r0 = floor( yF ), w = yF - r0;
		float cF = floor( c * maskScale ) + 0.5f;
		bool va, vb, vn;
		float la = CtLin( ( majorX ? float2( cF, r0 + 0.5f ) : float2( r0 + 0.5f, cF ) ) / maskScale, maskScale, fullSize, va );
		float lb = CtLin( ( majorX ? float2( cF, r0 + 1.5f ) : float2( r0 + 1.5f, cF ) ) / maskScale, maskScale, fullSize, vb );
		float ln = w < 0.5f ? la : lb;
		vn = w < 0.5f ? va : vb;
		bool edge = !va || !vb || abs( la - lb ) > ln * sssParams.y;
		float ls = edge ? ln : lerp( la, lb, w );
		va = vn;
		bool use = va && D >= 1.0f;
		sS[idx] = use ? ls * D : 0.0f;
		sR[idx] = use ? 1.0f / ( sssFloor * D ) : 0.0f;
		if ( u >= 0 && u < 64 )
			sL[u] = va ? ls : -1.0f;
	}
	GroupMemoryBarrierWithGroupSync();
	float cT = anchorC - s * float( t );
	float dist0 = s * ( cT - Lm );
	float yT = F4FX_ContactLineY( j, lineOff, t, Da );
	int2 pix = majorX ? int2( int( floor( cT ) ), int( floor( yT ) ) ) : int2( int( floor( yT ) ), int( floor( cT ) ) );
	if ( dist0 <= 0.0f || any( pix < 0 ) || any( pix >= maskSize ) )
		return;
	float minorOff = floor( yT ) + 0.5f - Ln;
	bool inSector = majorX ? dist0 > abs( minorOff ) : dist0 >= abs( minorOff );
	if ( !inSector || floor( F4FX_ContactLineY( j - 1, lineOffPrev, t, Da ) ) >= floor( yT ) )
		return;
	float lin0 = sL[t];
	float mask = 1.0f;
	if ( lin0 > 0.0f && dist0 >= 1.0f ) {
		float2 p = majorX ? float2( cT, yT ) : float2( yT, cT );
		int nk = n;
		if ( direction < 0.0f )
			nk = min( nk, int( floor( dist0 - 1.0f ) ) );
		float C = lin0 * dist0;
		if ( sssReach.x > 0.0f && nk > 0 ) {
			float3 pv0 = F4FX_ContactViewPos( p, lin0, maskScale, float2( fullSize ), sssTan );
			float ss = clamp( dot( pv0, pv0 ) / max( sssReach.z * sssReach.z, 1.0f ), 0.0f, 1.0f );
			float s2 = ss * ss;
			float wgt = 1.0f - s2 * s2;
			float reachEff = wgt > 0.0f ? ( sssReach.x + sssReach.y * lin0 ) / wgt : 1e30f;
			int kl = 0, kh = nk;
			for ( int it = 0; it < 10 && kl < kh; it++ ) {
				int km = ( kl + kh + 1 ) >> 1;
				float dist = dist0 + float( km ) * direction;
				int um = t - int( direction ) * km;
				float yq = F4FX_ContactLineY( j, lineOff, um, Da );
				float2 q = majorX ? float2( anchorC - s * float( um ), yq ) : float2( yq, anchorC - s * float( um ) );
				if ( distance( F4FX_ContactViewPos( q, C / dist, maskScale, float2( fullSize ), sssTan ), pv0 ) > reachEff )
					kh = km - 1;
				else
					kl = km;
			}
			nk = kl;
		}
#ifdef CONTACT_RED
		float Kc = 1.0f / ( 2.0f * sssParams.x * C );
#else
		float Kc = 1.0f / ( sssParams.x * C );
#endif
		float4 lanes = float4( 1.0f, 1.0f, 1.0f, 1.0f );
		int stp = -int( direction );
		int i0 = t + base;
		int k = 1;
		for ( ; k + 3 <= nk; k += 4 ) {
			int i = i0 + stp * k;
			float d1 = F4FX_ContactSample( C, sS[i], sR[i], Kc );
			float d2 = F4FX_ContactSample( C, sS[i + stp], sR[i + stp], Kc );
			float d3 = F4FX_ContactSample( C, sS[i + 2 * stp], sR[i + 2 * stp], Kc );
			float d4 = F4FX_ContactSample( C, sS[i + 3 * stp], sR[i + 3 * stp], Kc );
			lanes = min( lanes, float4( d4, d1, d2, d3 ) );
		}
		for ( ; k <= nk; k++ ) {
			int i = i0 + stp * k;
			float d = F4FX_ContactSample( C, sS[i], sR[i], Kc );
			int lane = k & 3;
			lanes[lane] = min( lanes[lane], d );
		}
		float contrast = sssParams.z;
		lanes = clamp( lanes * contrast + ( 1.0f - contrast ), 0.0f, 1.0f );
		mask = dot( lanes, float4( 0.25f, 0.25f, 0.25f, 0.25f ) );
	}
	LpxOut0[uint2( pix )] = mask;
}
