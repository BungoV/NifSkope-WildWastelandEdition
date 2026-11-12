// ReSTIR DI for the shadow-casting cell lights, one invocation a pixel (lane VKCOMPUTE1, 2026-11-12; from
// src/features/gi/rtddgi/restir.comp). The CPU twin is src/restirmath.h restir::frame() (passCandidates = mode 0,
// passSpatial = mode 1, accumulate = mode 2, passRatio = mode 3); the per-pixel rules are RestirCommon.hlsli.
// Shared, no bindings: it reads DDGI_P, DDGILights, DDGI_JOB (set 0 shim) and RESTIR_R, RestirGbuf ... RestirOutRatio
// (the ReSTIR shim) by name. The g-buffer is view normal + linear depth (1e6 none), bottom row first. Ray queries: cs_6_5.
#ifndef DDGI_RESTIR_HLSLI
#define DDGI_RESTIR_HLSLI

#include "DDGIRestirTypes.hlsli"
#include "DDGIRayQuery.hlsli"
#include "RestirCommon.hlsli"

float3 Restir_ToWorld( float4 r0, float4 r1, float4 r2, float3 v )
{
	return float3( dot( r0.xyz, v ) + r0.w, dot( r1.xyz, v ) + r1.w, dot( r2.xyz, v ) + r2.w );
}

struct RestirPixel { bool ok; float D; float3 P; float3 N; float3 Nv; };

// MIRROR restirmath.h pixelAt
RestirPixel Restir_PixelAt( bool prev, int x, int y )
{
	RestirPixel o;
	o.ok = false;
	o.P = float3( 0.0, 0.0, 0.0 );
	o.N = float3( 0.0, 0.0, 1.0 );
	o.Nv = o.N;
	const float4 g = prev ? RestirPrevGbuf[y * RESTIR_R.size.x + x] : RestirGbuf[y * RESTIR_R.size.x + x];
	o.D = g.w;
	bool wwh0 = !( o.D > 0.0 );
	bool wwh1 = o.D >= 1e5;
	if (wwh0 || wwh1)
		return o;
	o.Nv = g.xyz;
	const float4 pj = prev ? RESTIR_R.pproj : RESTIR_R.proj;
	const float3 pv = Restir_ViewPos( float( x ), float( y ), float( RESTIR_R.size.x ), float( RESTIR_R.size.y ), o.D, pj );
	if ( prev ) {
		o.P = Restir_ToWorld( RESTIR_R.prows[0], RESTIR_R.prows[1], RESTIR_R.prows[2], pv );
		o.N = normalize( float3( dot( RESTIR_R.prows[0].xyz, g.xyz ), dot( RESTIR_R.prows[1].xyz, g.xyz ), dot( RESTIR_R.prows[2].xyz, g.xyz ) ) );
	} else {
		o.P = Restir_ToWorld( RESTIR_R.rows[0], RESTIR_R.rows[1], RESTIR_R.rows[2], pv );
		o.N = normalize( float3( dot( RESTIR_R.rows[0].xyz, g.xyz ), dot( RESTIR_R.rows[1].xyz, g.xyz ), dot( RESTIR_R.rows[2].xyz, g.xyz ) ) );
	}
	o.ok = true;
	return o;
}

// MIRROR restirmath.h targetOf
float Restir_TargetOf( int y, RestirPixel p )
{
	bool wwh2 = y < 0;
	bool wwh3 = y >= DDGI_P.counts.y;
	if (wwh2 || wwh3 || !p.ok)
		return 0.0;
	const uint b = uint( y ) * 8u;
	return Restir_Target( DDGILights[b], DDGILights[b + 1u], DDGILights[b + 2u], DDGILights[b + 3u], DDGILights[b + 4u], DDGILights[b + 5u],
		DDGILights[b + 6u], DDGILights[b + 7u], p.P, p.N );
}

// mode 0, MIRROR restirmath.h passCandidates
void Restir_Candidates( int x, int y, uint id )
{
	RestirHistPix[id] = -1;
	RestirCur[id] = Restir_Make( -1, 0.0, 0.0, 0.0 );
	const RestirPixel p = Restir_PixelAt( false, x, y );
	const int nCand = RESTIR_R.size.z;
	bool wwh4 = nCand <= 0;
	if (!p.ok || wwh4)
		return;
	const int red = RESTIR_R.opts.w;
	const uint s0 = Restir_Seed( id, uint( RESTIR_R.size.w ), 0u );
	RestirRes r = Restir_Make( -1, 0.0, 0.0, 0.0 );
	for ( int k = 0; k < RESTIR_R.opts.x; k++ ) {
		const int j = min( int( Restir_Rand( s0, uint( 2 * k ) ) * float( nCand ) ), nCand - 1 );
		const int l = RestirCand[j];
		const float w = Restir_TargetOf( l, p ) * float( nCand );
		r = Restir_Update( r, l, w, 1.0, Restir_Rand( s0, uint( 2 * k + 1 ) ) );
	}
	r = Restir_Finalize( r, Restir_TargetOf( r.y, p ), r.M, red );
	RestirCur[id] = r;
	bool wwh5 = ( red & RESTIR_BIT_NOTEMPORAL ) != 0;
	bool wwh6 = RESTIR_R.knobs.w < 0.5;
	if (wwh5 || wwh6)
		return;
	// world -> the previous view: the transpose of its rows' rotation
	const float3 t = p.P - float3( RESTIR_R.prows[0].w, RESTIR_R.prows[1].w, RESTIR_R.prows[2].w );
	const float3 pv = float3( RESTIR_R.prows[0].x * t.x + RESTIR_R.prows[1].x * t.y + RESTIR_R.prows[2].x * t.z,
		RESTIR_R.prows[0].y * t.x + RESTIR_R.prows[1].y * t.y + RESTIR_R.prows[2].y * t.z,
		RESTIR_R.prows[0].z * t.x + RESTIR_R.prows[1].z * t.y + RESTIR_R.prows[2].z * t.z );
	if ( !( pv.z < -1e-3 ) )
		return;
	const float Dp = -pv.z;
	const float nx = ( RESTIR_R.pproj.x * pv.x - RESTIR_R.pproj.z * Dp ) / Dp, ny = ( RESTIR_R.pproj.y * pv.y - RESTIR_R.pproj.w * Dp ) / Dp;
	const int qx = int( floor( ( nx + 1.0 ) * 0.5 * float( RESTIR_R.size.x ) ) ), qy = int( floor( ( ny + 1.0 ) * 0.5 * float( RESTIR_R.size.y ) ) );
	bool wwh7 = qx < 0;
	bool wwh8 = qy < 0;
	bool wwh9 = qx >= RESTIR_R.size.x;
	bool wwh10 = qy >= RESTIR_R.size.y;
	if (wwh7 || wwh8 || wwh9 || wwh10)
		return;
	const RestirPixel q = Restir_PixelAt( true, qx, qy );
	if ( !q.ok || !Restir_Similar( Dp, p.N, q.D, q.N ) )
		return;
	RestirHistPix[id] = qy * RESTIR_R.size.x + qx;
	RestirRes h = RestirHist[qy * RESTIR_R.size.x + qx];
	h.M = Restir_ClampM( h.M, float( RESTIR_R.opts.x ), red );
	const uint s1 = Restir_Seed( id, uint( RESTIR_R.size.w ), 1u );
	RestirRes s = Restir_Make( -1, 0.0, 0.0, 0.0 );
	s = Restir_Update( s, r.y, Restir_TargetOf( r.y, p ) * r.W * r.M, r.M, Restir_Rand( s1, 0u ) );
	s = Restir_Update( s, h.y, Restir_TargetOf( h.y, p ) * h.W * h.M, h.M, Restir_Rand( s1, 1u ) );
	float Z = 0.0;
	if ( Restir_TargetOf( s.y, p ) > 0.0 )
		Z += r.M;
	if ( Restir_TargetOf( s.y, q ) > 0.0 )
		Z += h.M;
	RestirCur[id] = Restir_Finalize( s, Restir_TargetOf( s.y, p ), Z, red );
}

// mode 1, MIRROR restirmath.h passSpatial
void Restir_Spatial( int x, int y, uint id )
{
	RestirNumDen[id] = float4( 0.0, 0.0, 2.0, 0.0 );
	RestirHist[id] = Restir_Make( -1, 0.0, 0.0, 0.0 );
	const RestirPixel p = Restir_PixelAt( false, x, y );
	if ( !p.ok )
		return;
	const int red = RESTIR_R.opts.w;
	const uint s2 = Restir_Seed( id, uint( RESTIR_R.size.w ), 2u );
	const RestirRes own = RestirCur[id];
	RestirRes s = Restir_Make( -1, 0.0, 0.0, 0.0 );
	s = Restir_Update( s, own.y, Restir_TargetOf( own.y, p ) * own.W * own.M, own.M, Restir_Rand( s2, 0u ) );
	int qs[16];
	int nq = 0;
	for ( int k = 0; k < RESTIR_R.opts.y && k < 16; k++ ) {
		const float a = 6.283185307179586 * Restir_Rand( s2, uint( 1 + 3 * k ) );
		const float rr = RESTIR_R.knobs.x * sqrt( Restir_Rand( s2, uint( 2 + 3 * k ) ) );
		const int qx = x + int( floor( rr * cos( a ) + 0.5 ) ), qy = y + int( floor( rr * sin( a ) + 0.5 ) );
		bool wwh11 = ( qx == x && qy == y );
		bool wwh12 = qx < 0;
		bool wwh13 = qy < 0;
		bool wwh14 = qx >= RESTIR_R.size.x;
		bool wwh15 = qy >= RESTIR_R.size.y;
		if (wwh11 || wwh12 || wwh13 || wwh14 || wwh15)
			continue;
		const RestirPixel q = Restir_PixelAt( false, qx, qy );
		if ( !q.ok || !Restir_Similar( p.D, p.Nv, q.D, q.Nv ) )
			continue;
		const RestirRes n = RestirCur[qy * RESTIR_R.size.x + qx];
		s = Restir_Update( s, n.y, Restir_TargetOf( n.y, p ) * n.W * n.M, n.M, Restir_Rand( s2, uint( 3 + 3 * k ) ) );
		qs[nq] = qy * RESTIR_R.size.x + qx;
		nq++;
	}
	float Z = Restir_TargetOf( s.y, p ) > 0.0 ? own.M : 0.0;
	for ( int k = 0; k < nq; k++ ) {
		const RestirPixel q = Restir_PixelAt( false, qs[k] % RESTIR_R.size.x, qs[k] / RESTIR_R.size.x );
		if ( Restir_TargetOf( s.y, q ) > 0.0 )
			Z += RestirCur[qs[k]].M;
	}
	s = Restir_Finalize( s, Restir_TargetOf( s.y, p ), Z, red );
	RestirHist[id] = s;
	bool wwh16 = s.y < 0;
	bool wwh17 = !( s.W > 0.0 );
	if (wwh16 || wwh17)
		return;
	const uint b = uint( s.y ) * 8u;
	const uint s3 = Restir_Seed( id, uint( RESTIR_R.size.w ), 3u );
	const float3 X = Restir_SpherePoint( DDGILights[b].xyz, DDGILights[b + 7u].y, p.P, Restir_Rand( s3, 0u ), Restir_Rand( s3, 1u ) );
	const float3 o = p.P + p.N * RESTIR_R.knobs.y;
	const float3 dl = X - o;
	const float len = length( dl );
	const bool blocked = len > 1e-6 && DDGI_AnyHit( o, dl * ( 1.0 / len ), len - RESTIR_R.knobs.z, false, ( red & RESTIR_BIT_NOHOLE ) != 0 );
	const float ph = Restir_TargetOf( s.y, p );
	RestirNumDen[id] = float4( Restir_Shadowed( ph, s.W, !blocked, red ), ph * s.W, blocked ? 1.0 : 0.0, 0.0 );
}

// mode 2, MIRROR restirmath.h frame's pass 2 (accumulate)
void Restir_AccumulatePass( uint id )
{
	const int hp = RestirHistPix[id];
	const float4 nd = RestirNumDen[id];
	RestirAcc[id] = Restir_Accumulate( hp >= 0 ? RestirAccPrev[hp] : float4( 0.0, 0.0, 0.0, 0.0 ), nd.x, nd.y, hp >= 0, RESTIR_R.opts.w );
}

// mode 3, MIRROR restirmath.h passRatio
void Restir_RatioPass( int x, int y, uint id )
{
	const float4 g = RestirGbuf[id];
	float n = 0.0, d = 0.0;
	const int f = RESTIR_R.opts.z;
	for ( int dy = -f; dy <= f; dy++ )
		for ( int dx = -f; dx <= f; dx++ ) {
			const int qx = x + dx, qy = y + dy;
			bool wwh18 = qx < 0;
			bool wwh19 = qy < 0;
			bool wwh20 = qx >= RESTIR_R.size.x;
			bool wwh21 = qy >= RESTIR_R.size.y;
			if (wwh18 || wwh19 || wwh20 || wwh21)
				continue;
			const float4 h = RestirGbuf[qy * RESTIR_R.size.x + qx];
			if ( ( dx != 0 || dy != 0 ) && !Restir_Similar( g.w, g.xyz, h.w, h.xyz ) )
				continue;
			n += RestirAcc[qy * RESTIR_R.size.x + qx].x;
			d += RestirAcc[qy * RESTIR_R.size.x + qx].y;
		}
	RestirOutRatio[id] = float2( Restir_Ratio( n, d ), g.w );
}

void Restir_CS( uint id )
{
	if ( id >= uint( RESTIR_R.size.x * RESTIR_R.size.y ) )
		return;
	const int x = int( id % uint( RESTIR_R.size.x ) ), y = int( id / uint( RESTIR_R.size.x ) );
	const uint mode = DDGI_JOB.job.x;
	if ( mode == 0u )
		Restir_Candidates( x, y, id );
	else if ( mode == 1u )
		Restir_Spatial( x, y, id );
	else if ( mode == 2u )
		Restir_AccumulatePass( id );
	else
		Restir_RatioPass( x, y, id );
}

#endif
