// ReSTIR GI on the DDGI volume's BVH (lane HLSLRESTIR1, 2026-11-12; from src/features/gi/rtddgi/restirgi.comp, statement for
// statement). Shared, no bindings: it reads the volume's set by the DDGI names and set 1 by the RestirGi* names (the shim).
// One invocation a pixel in 8 x 8 tiles; job.x picks the mode:
//   0 candidate   the bounce (shaded as a probe ray: trace_body.hlsli), then the reprojected history
//   1 spatial     K neighbours with the Jacobian, Z over the merged pixels
//   2 accumulate  the running mean, clamped to the like 3 x 3 neighbours, and the a-trous variance guide
//   3 filter      the like pixels' means in a window; 4 + it the a-trous passes (the last writes RestirGiOut)
// RESTIRGI_FILTER builds the filter kernel (modes 2..; no ray code). RESTIRGI_RED (the gate's red) drops mode 2's packed
// g-buffer write.
// restirgi.comp's header:
// Lane RTEMIS1 part 2 (2026-11-06): ReSTIR GI on the DDGI volume's BVH, one invocation a pixel (the RT quality preset
// High). The CPU twin is src/features/gi/restirgimath.h restirgi::frame() (its pass 0 = mode 0, pass 1 = mode 1); the
// per-pixel rules are restirgi_common.glsl (MIRROR restirgimath.h). A bounce ray's hit is shaded as a probe ray's
// (trace_body.glsl: direct + the volume's irradiance, x albedo / pi, + Le), a miss returns the volume's miss radiance.
// mode 2 keeps a running mean a pixel (reprojected, at most knobs.y frames, clamped to the like neighbours' estimates),
// mode 3 filters it in a depth / normal aware window (twice as wide while the mean is young: one frame must stand on its
// own, history only polishes), modes 4.. run the variance-guided a-trous passes on it and the last one writes the
// irradiance the cell shader reads (no a-trous passes: mode 3 writes it). Set 0 = the volume's own set, set 1 = the buffers below.
// (HLSL: set 1 is DDGIRestirGiBindingsVk.hlsli's; restirgi_common.glsl = restirgi_common.hlsli, trace_body.glsl = trace_body.hlsli)
#ifndef DDGI_RESTIRGI_HLSLI
#define DDGI_RESTIRGI_HLSLI

#include "trace_body.hlsli"
#include "restirgi_common.hlsli"

uint RestirGi_StbnByte( uint i ) { return ( RestirGiStbn[i >> 2u] >> ( ( i & 3u ) * 8u ) ) & 255u; }

// the g-buffer texel packed for the filter and a-trous taps (8 bytes, not 16): the depth's own float bits and the normal
// octahedral in two snorm16 [Cigolle 2014, "A Survey of Efficient Representations for Independent Unit Vectors"];
// a zero normal packs to (0, 0) and comes back +z (only where the depth is invalid)
// GLSL packSnorm2x16 / unpackSnorm2x16: SPIR-V takes GLSL.std.450's own instructions (the same as glslang's); DXIL the formula
#ifdef __spirv__
[[vk::ext_instruction( 56, "GLSL.std.450" )]] uint RestirGi_SpvPackSnorm2x16( float2 v );
[[vk::ext_instruction( 60, "GLSL.std.450" )]] float2 RestirGi_SpvUnpackSnorm2x16( uint p );
uint RestirGi_PackSnorm2x16( float2 v ) { return RestirGi_SpvPackSnorm2x16( v ); }
float2 RestirGi_UnpackSnorm2x16( uint p ) { return RestirGi_SpvUnpackSnorm2x16( p ); }
#else
uint RestirGi_PackSnorm2x16( float2 v )
{
	const int2 i = int2( round( clamp( v, -1.0, 1.0 ) * 32767.0 ) );
	return ( uint( i.x ) & 65535u ) | ( uint( i.y ) << 16u );
}
float2 RestirGi_UnpackSnorm2x16( uint p )
{
	const int2 i = int2( int( p << 16u ) >> 16, int( p ) >> 16 );
	return clamp( float2( i ) / 32767.0, -1.0, 1.0 );
}
#endif

/* the g-buffer texel packed for the filter and a-trous taps (8 bytes, not 16): the depth's own float bits and the normal
 * octahedral in two snorm16 [Cigolle 2014]; a zero normal packs to (0, 0) and comes back +z (only where the depth is invalid) */
uint2 RestirGi_Pack( float4 g )
{
	const float s = abs( g.x ) + abs( g.y ) + abs( g.z );
	float2 p = s > 0.0 ? g.xy / s : float2( 0.0, 0.0 );
	if ( s > 0.0 && g.z < 0.0 )
		p = ( 1.0 - abs( p.yx ) ) * float2( p.x >= 0.0 ? 1.0 : -1.0, p.y >= 0.0 ? 1.0 : -1.0 );
	return uint2( asuint( g.w ), RestirGi_PackSnorm2x16( p ) );
}

float4 RestirGi_Unpack( uint2 k )
{
	const float2 p = RestirGi_UnpackSnorm2x16( k.y );
	float3 n = float3( p, 1.0 - abs( p.x ) - abs( p.y ) );
	const float t = max( -n.z, 0.0 );
	n.x += n.x >= 0.0 ? -t : t;
	n.y += n.y >= 0.0 ? -t : t;
	return float4( normalize( n ), asfloat( k.x ) );
}

float3 RestirGi_ToWorld( float4 r0, float4 r1, float4 r2, float3 v )
{
	return float3( dot( r0.xyz, v ) + r0.w, dot( r1.xyz, v ) + r1.w, dot( r2.xyz, v ) + r2.w );
}

struct RgPixel
{
	bool ok;
	float D;
	float3 P;
	float3 N;
};

// MIRROR restirmath.h pixelAt (restir.comp rsPixel)
RgPixel RestirGi_Pixel( bool prev, int x, int y )
{
	RgPixel o;
	o.ok = false;
	o.P = float3( 0.0, 0.0, 0.0 );
	o.N = float3( 0.0, 0.0, 1.0 );
	const float4 g = prev ? RestirGiPrevGbuf[y * RG_G.size.x + x] : RestirGiGbuf[y * RG_G.size.x + x];
	o.D = g.w;
	if ( !( o.D > 0.0 ) || o.D >= 1e5 )
		return o;
	const float3 pv = Restir_ViewPos( float( x ), float( y ), float( RG_G.size.x ), float( RG_G.size.y ), o.D, prev ? RG_G.pproj : RG_G.proj );
	if ( prev ) {
		o.P = RestirGi_ToWorld( RG_G.prows[0], RG_G.prows[1], RG_G.prows[2], pv );
		o.N = normalize( float3( dot( RG_G.prows[0].xyz, g.xyz ), dot( RG_G.prows[1].xyz, g.xyz ), dot( RG_G.prows[2].xyz, g.xyz ) ) );
	} else {
		o.P = RestirGi_ToWorld( RG_G.rows[0], RG_G.rows[1], RG_G.rows[2], pv );
		o.N = normalize( float3( dot( RG_G.rows[0].xyz, g.xyz ), dot( RG_G.rows[1].xyz, g.xyz ), dot( RG_G.rows[2].xyz, g.xyz ) ) );
	}
	o.ok = true;
	return o;
}

#ifndef RESTIRGI_FILTER
/* the bounce hit's direct light (RTEMIS1 11-09, the cost cut; MIRROR rtddgi.cpp RgTwinScene directOne): the sun and the
 * directional term as ddgiDirect, the placed lights by ONE pick in proportion to its unshadowed luminance (a running
 * pick on the one uniform u, rescaled each step) with one shadow ray, over its probability: the mean of every light with
 * a ray each [Talbot 2005, RIS]; red alllights = ddgiDirect */
// (HLSL: ddgiDirect = DDGI_Direct)
float3 RestirGi_DirectOne( float3 Pw, float3 n, float u )
{
	float3 E = float3( 0.0, 0.0, 0.0 );
	const float3 o = Pw + n * 0.5;
	if ( DDGI_P.sunTo.w > 0.5 ) {
		const float nl = dot( n, DDGI_P.sunTo.xyz );
		if ( nl > 0.0 && !DDGI_Shadow( o, DDGI_P.sunTo.xyz, DDGI_P.sun.w ) )
			E += DDGI_P.sun.rgb * nl;
	}
	if ( DDGI_P.dirTo.w > 0.5 ) {
		const float nl = dot( n, DDGI_P.dirTo.xyz );
		if ( nl > 0.0 )
			E += DDGI_P.dirColor.rgb * nl;
	}
	float wsum = 0.0, wp = 0.0;
	float3 cp = float3( 0.0, 0.0, 0.0 ), lp = float3( 0.0, 0.0, 0.0 );
	for ( int li = 0; li < DDGI_P.counts.y; li++ ) {
		const uint b = uint( li ) * 8u;
		const float4 l0 = DDGILights[b], l1 = DDGILights[b + 1u], l2 = DDGILights[b + 2u], l3 = DDGILights[b + 3u];
		const float4 l7 = DDGILights[b + 7u];
		const float3 Lv = l0.xyz - Pw;
		const float d = length( Lv );
		if ( d >= l0.w )
			continue;
		const int shape = int( l7.x + 0.5 );
		if ( shape == 1 && dot( Pw - l0.xyz, l2.xyz ) < 0.0 )
			continue;
		if ( shape == 2 ) {
			bool inside = true;
			for ( int k = 0; k < 3; k++ ) {
				const float4 bx = DDGILights[b + 4u + uint( k )];
				if ( abs( dot( bx.xyz, Pw ) + bx.w ) > 1.0 )
					inside = false;
			}
			if ( !inside )
				continue;
		}
		const float3 L = Lv / max( d, 0.001 );
		const float nl = dot( n, L );
		if ( nl <= 0.0 )
			continue;
		float att = DDGI_Radial( d, l0.w, l3.x, l3.y, l3.z );
		if ( l1.w > 0.5 )
			att *= DDGI_Spot( -dot( L, l2.xyz ), l2.w, l3.w );
		if ( att * nl <= 0.0 )
			continue;
		const float3 c = l1.rgb * ( att * nl );
		const float w = RestirGi_Lum( c );
		if ( !( w > 0.0 ) )
			continue;
		wsum += w;
		const float pr = w / wsum;
		if ( u < pr ) {
			u = u / pr;
			wp = w;
			cp = c;
			lp = l0.xyz;
		} else {
			u = ( u - pr ) / max( 1.0 - pr, 1e-30 );
		}
	}
	if ( wp > 0.0 ) {
		const float3 dl = lp - o;
		const float len = length( dl );
		if ( !DDGI_Shadow( o, dl / max( len, 1e-6 ), len - DDGI_P.dirColor.w ) )
			E += cp * ( wsum / wp );
	}
	return E;
}

// the bounce ray, shaded as trace.comp main shades a probe ray (the twin: src/features/gi/rtddgi.cpp restirgiCli's scene)
RgRes RestirGi_Trace( float3 o, float3 d, float u, int red )
{
	uint pr;
	float t;
	if ( !DDGI_Nearest( o, d, DDGI_P.sun.w, pr, t ) ) {
		float3 Lm = float3( 0.0, 0.0, 0.0 );
		if ( DDGI_P.sky[0].w > 0.5 )
			Lm = DDGI_Miss( DDGI_P, d );
		return RestirGi_Sample( d, float3( 0.0, 0.0, 0.0 ), Lm, false );
	}
	const int tri = DDGIRemap[pr];
	const uint vb = pr * 9u;
	const float3 v0 = float3( DDGIVerts[vb], DDGIVerts[vb + 1u], DDGIVerts[vb + 2u] );
	const float3 e1 = float3( DDGIVerts[vb + 3u], DDGIVerts[vb + 4u], DDGIVerts[vb + 5u] ) - v0;
	const float3 e2 = float3( DDGIVerts[vb + 6u], DDGIVerts[vb + 7u], DDGIVerts[vb + 8u] ) - v0;
	const float3 nf = normalize( cross( e1, e2 ) );
	const float4 a0 = DDGITriAttr[uint( tri ) * 2u];
	const float3 n = dot( nf, d ) > 0.0 ? -nf : nf;
	const float3 Pw = o + d * t;
	if ( a0.w < 0.5 && dot( nf, d ) > 0.0 && ( uint( DDGI_P.flags.w ) & DDGI_RED_NOBACK ) == 0u )
		return RestirGi_Sample( Pw, n, float3( 0.0, 0.0, 0.0 ), true );   // a one-sided face from behind sends nothing
	float3 E;
	if ( ( red & RESTIRGI_BIT_ALLLIGHTS ) != 0 )
		E = DDGI_Direct( Pw, n );
	else
		E = RestirGi_DirectOne( Pw, n, u );
	if ( DDGI_P.flags.x == 0 )
		E += DDGI_IrradianceAt( Pw, n, -d );
	return RestirGi_Sample( Pw, n, DDGI_HitRadiance( a0.rgb, E, DDGITriAttr[uint( tri ) * 2u + 1u].rgb ), true );
}

// MIRROR restirgimath.h frame's sees: a shadow ray from the receiver to the sample (a miss: along its direction)
bool RestirGi_Sees( float3 x, float3 n, RgRes y, int red )
{
	if ( ( red & RESTIRGI_BIT_NOVIS ) != 0 )
		return true;
	const float3 o = x + n * RESTIRGI_EPS;
	if ( y.hit < 0.5 )
		return !DDGI_Shadow( o, y.xs, DDGI_P.sun.w );
	const float3 d = y.xs - o;
	const float l = length( d );
	if ( !( l > 2.0 * RESTIRGI_EPS ) )
		return false;
	return !DDGI_Shadow( o, d / l, l - 2.0 * RESTIRGI_EPS );
}

// mode 0, MIRROR restirgimath.h frame pass 0: the bounce, then the reprojected history
void RestirGi_ModeCandidate( int x, int y, uint id )
{
	RestirGiHistPix[id] = -1;
	RestirGiCur[id] = RestirGi_Empty();
	RestirGiCand[id] = float4( 0.0, 0.0, 0.0, -1.0 );
	const RgPixel p = RestirGi_Pixel( false, x, y );
	if ( !p.ok )
		return;
	const int red = RG_G.opts.w;
	const uint pix = uint( RG_G.size.w ) ^ id;
	const uint s1 = Restir_Seed( pix, uint( RG_G.size.z ), 1u );
	const bool mask = RG_G.knobs.z > 0.5;
	uint m0 = 0u, m1 = 0u;
	if ( mask ) {
		m0 = RestirGi_StbnByte( RestirGi_StbnIndex( x, y, uint( RG_G.size.z ), uint( RG_G.size.w ), 0 ) );
		m1 = RestirGi_StbnByte( RestirGi_StbnIndex( x, y, uint( RG_G.size.z ), uint( RG_G.size.w ), 1 ) );
	}
	// the half / quarter trace: a skipped pixel draws nothing (M 0) and keeps its history; mode 2 fills its estimate
	RgRes r = RestirGi_Empty();
	if ( RestirGi_Traced( x, y, uint( RG_G.size.z ), red ) ) {
		const float3 d = RestirGi_BounceDir( p.N, x, y, RG_G.size.x, uint( RG_G.size.w ), uint( RG_G.size.z ), red, mask, m0, m1 );
		const RgRes yv = RestirGi_Trace( p.P + p.N * RESTIRGI_EPS, d, Restir_Rand( s1, 1u ), red );
		RestirGiCand[id] = float4( yv.Lo, yv.hit );
		r = RestirGi_Candidate( p.P, p.N, yv, red );
	}
	RestirGiCur[id] = r;
	if ( ( red & RESTIRGI_BIT_NOTEMPORAL ) != 0 || RG_G.knobs.x < 0.5 )
		return;
	// world -> the previous view (restir.comp modeCandidates)
	const float3 t = p.P - float3( RG_G.prows[0].w, RG_G.prows[1].w, RG_G.prows[2].w );
	const float3 pv = float3( RG_G.prows[0].x * t.x + RG_G.prows[1].x * t.y + RG_G.prows[2].x * t.z,
		RG_G.prows[0].y * t.x + RG_G.prows[1].y * t.y + RG_G.prows[2].y * t.z, RG_G.prows[0].z * t.x + RG_G.prows[1].z * t.y + RG_G.prows[2].z * t.z );
	if ( !( pv.z < -1e-3 ) )
		return;
	const float Dp = -pv.z;
	const float nx = ( RG_G.pproj.x * pv.x - RG_G.pproj.z * Dp ) / Dp, ny = ( RG_G.pproj.y * pv.y - RG_G.pproj.w * Dp ) / Dp;
	const int qx = int( floor( ( nx + 1.0 ) * 0.5 * float( RG_G.size.x ) ) ), qy = int( floor( ( ny + 1.0 ) * 0.5 * float( RG_G.size.y ) ) );
	if ( qx < 0 || qy < 0 || qx >= RG_G.size.x || qy >= RG_G.size.y )
		return;
	const RgPixel q = RestirGi_Pixel( true, qx, qy );
	if ( !q.ok || !Restir_Similar( Dp, p.N, q.D, q.N ) )
		return;
	RestirGiHistPix[id] = qy * RG_G.size.x + qx;
	const RgRes h = RestirGiHist[qy * RG_G.size.x + qx];
	if ( h.any < 0.5 )
		return;
	const float Mh = min( h.M, RESTIRGI_HISTORY_CAP );
	RgRes s = RestirGi_Update( RestirGi_Empty(), r, RestirGi_Target( p.P, p.N, r ) * r.W * r.M, r.M, 0.0 );
	s = RestirGi_Update( s, h, RestirGi_Target( p.P, p.N, h ) * h.W * Mh, Mh, Restir_Rand( s1, 0u ) );
	RestirGiCur[id] = RestirGi_Finalize( s, RestirGi_Target( p.P, p.N, s ), s.M );
}

// mode 1, MIRROR restirgimath.h frame pass 1: K neighbours with the Jacobian (unshadowed weights; red mergevis: seen);
// Z over the merged pixels
void RestirGi_ModeSpatial( int x, int y, uint id )
{
	RestirGiEst[id] = float4( 0.0, 0.0, 0.0, 0.0 );
	const RgPixel p = RestirGi_Pixel( false, x, y );
	const int red = RG_G.opts.w;
	if ( !p.ok || !RestirGi_Traced( x, y, uint( RG_G.size.z ), red ) )
		return;
	const int W = RG_G.size.x, H = RG_G.size.y;
	const uint s2 = Restir_Seed( uint( RG_G.size.w ) ^ id, uint( RG_G.size.z ), 2u );
	const RgRes own = RestirGiCur[id];
	RgRes r = RestirGi_Update( RestirGi_Empty(), own, RestirGi_Target( p.P, p.N, own ) * own.W * own.M, own.M, Restir_Rand( s2, 0u ) );
	int from[16];
	int nf = 0;
	from[nf] = int( id );
	nf++;
	const int R = RG_G.opts.y;
	const int side = 2 * R + 1;
	for ( int k = 0; k < min( RG_G.opts.x, 15 ); k++ ) {
		const int o = int( Restir_Rand( s2, uint( 1 + 3 * k ) ) * float( side * side ) );
		const int2 qs = RestirGi_SnapTraced( x + o % side - R, y + o / side - R, uint( RG_G.size.z ), red );
		const int qx = qs.x, qy = qs.y;
		if ( qx < 0 || qy < 0 || qx >= W || qy >= H || ( qx == x && qy == y ) )
			continue;
		const int q = qy * W + qx;
		const RgPixel pq = RestirGi_Pixel( false, qx, qy );
		if ( !pq.ok )
			continue;
		const RgRes rq = RestirGiCur[q];
		float j = 1.0;
		if ( rq.any > 0.5 )
			j = RestirGi_Jacobian( p.P, pq.P, rq, red );
		const float jk = RestirGi_JacobianKept( j, red );
		if ( jk < 0.0 )
			continue;
		from[nf] = q;
		nf++;
		float w = 0.0;
		if ( rq.any > 0.5 && ( ( red & RESTIRGI_BIT_MERGEVIS ) == 0 || RestirGi_Sees( p.P, p.N, rq, red ) ) )
			w = RestirGi_Target( p.P, p.N, rq ) * rq.W * rq.M * jk;
		r = RestirGi_Update( r, rq, w, rq.M, Restir_Rand( s2, uint( 2 + 3 * k ) ) );
	}
	// the own pixel's shadow ray is traced once: Z's k = 0 term and pHat ask the same question
	const bool seesOwn = r.any > 0.5 && RestirGi_Sees( p.P, p.N, r, red );
	float tOwn = 0.0;
	if ( r.any > 0.5 )
		tOwn = RestirGi_Target( p.P, p.N, r );
	float Z = tOwn > 0.0 && seesOwn ? own.M : 0.0;
	for ( int k = 1; k < nf; k++ ) {
		const RgPixel ps = RestirGi_Pixel( false, from[k] % W, from[k] / W );
		if ( r.any > 0.5 && RestirGi_Target( ps.P, ps.N, r ) > 0.0 && RestirGi_Sees( ps.P, ps.N, r, red ) )
			Z += RestirGiCur[from[k]].M;
	}
	const float pHat = seesOwn ? tOwn : 0.0;
	r = RestirGi_Finalize( r, pHat, Z );
	RestirGiEst[id] = float4( RestirGi_Estimate( p.P, p.N, r ), 1.0 );
}
#endif

// mode 2: the running mean, through the reprojected pixel (none: starts over; mode 0 found none on a cut, a light change
// or a disocclusion). The history is clamped to the 3 x 3 like pixels' estimates first [Karis 2014, TAA's clamp], so a
// stale mean (a light gone, a thing moved) cannot outlive the frame that shows it. The same 3 x 3 gives the frame's
// spatial variance of the estimate's luminance; over the mean's frames it is the mean's variance, the a-trous guide
// (a young mean: one frame's spread, so the guide is wide exactly when the history is short) [Schied 2017, SVGF 4.2].
void RestirGi_ModeAccumulate( int x, int y, uint id )
{
	float4 e = RestirGiEst[id];
	RestirGiAtrB[id] = float4( 0.0, 0.0, 0.0, 0.0 );
	const float4 g = RestirGiGbuf[id];
#ifndef RESTIRGI_RED
	RestirGiPk[id] = RestirGi_Pack( g );
#endif
	// a pixel the half / quarter trace skipped (valid depth, no estimate): the mean of its traced like 3 x 3 neighbours;
	// none like it: its own history's mean, then any traced neighbour, then black (the filters read acc.w > 0 exactly
	// where the depth is valid, so a valid pixel always gets one)
	if ( e.w < 0.5 && g.w > 0.0 && g.w < 1e5 && !RestirGi_Traced( x, y, uint( RG_G.size.z ), RG_G.opts.w ) ) {
		float3 sum = float3( 0.0, 0.0, 0.0 ), sumAny = float3( 0.0, 0.0, 0.0 );
		float nf = 0.0, nAny = 0.0;
		for ( int dy = -1; dy <= 1; dy++ )
			for ( int dx = -1; dx <= 1; dx++ ) {
				const int qx = x + dx, qy = y + dy;
				if ( ( dx == 0 && dy == 0 ) || qx < 0 || qy < 0 || qx >= RG_G.size.x || qy >= RG_G.size.y )
					continue;
				const int q = qy * RG_G.size.x + qx;
				const float4 h = RestirGiGbuf[q];
				const float4 eq = RestirGiEst[q];
				if ( eq.w < 0.5 )
					continue;
				sumAny += eq.xyz;
				nAny += 1.0;
				if ( Restir_Similar( g.w, g.xyz, h.w, h.xyz ) ) {
					sum += eq.xyz;
					nf += 1.0;
				}
			}
		const int hq = RestirGiHistPix[id];
		if ( nf > 0.0 )
			e = float4( sum / nf, 1.0 );
		else if ( hq >= 0 && RestirGiAccPrev[hq].w > 0.0 )
			e = float4( RestirGiAccPrev[hq].xyz, 1.0 );
		else
			e = float4( nAny > 0.0 ? sumAny / nAny : float3( 0.0, 0.0, 0.0 ), 1.0 );
	}
	if ( e.w < 0.5 ) {
		RestirGiAcc[id] = float4( 0.0, 0.0, 0.0, 0.0 );
		return;
	}
	float3 lo = e.xyz, hi = e.xyz;
	const float le = RestirGi_Lum( e.xyz );
	float s1 = le, s2 = le * le, ns = 1.0;
	for ( int dy = -1; dy <= 1; dy++ )
		for ( int dx = -1; dx <= 1; dx++ ) {
			const int qx = x + dx, qy = y + dy;
			if ( ( dx == 0 && dy == 0 ) || qx < 0 || qy < 0 || qx >= RG_G.size.x || qy >= RG_G.size.y )
				continue;
			const int q = qy * RG_G.size.x + qx;
			const float4 h = RestirGiGbuf[q];
			const float4 eq = RestirGiEst[q];
			if ( eq.w < 0.5 || !Restir_Similar( g.w, g.xyz, h.w, h.xyz ) )
				continue;
			lo = min( lo, eq.xyz );
			hi = max( hi, eq.xyz );
			const float l = RestirGi_Lum( eq.xyz );
			s1 += l;
			s2 += l * l;
			ns += 1.0;
		}
	const float vs = max( s2 / ns - ( s1 / ns ) * ( s1 / ns ), 0.0 );
	const int hp = RestirGiHistPix[id];
	float n = 1.0;
	if ( hp < 0 ) {
		RestirGiAcc[id] = float4( e.xyz, 1.0 );
	} else {
		const float4 a = RestirGiAccPrev[hp];
		const float3 ah = clamp( a.xyz, lo, hi );
		n = min( a.w + 1.0, max( RG_G.knobs.y, 1.0 ) );
		RestirGiAcc[id] = float4( ah + ( e.xyz - ah ) / n, n );
	}
	RestirGiAtrB[id] = float4( 0.0, 0.0, 0.0, vs / n );
}

// mode 3: the running means of the window's like pixels (depth within 10 %, normal within 25 degrees: restirSimilar)
// (HLSL: restirSimilar = Restir_Similar)
void RestirGi_ModeFilter( int x, int y, uint id )
{
	const float4 g = RestirGi_Unpack( RestirGiPk[id] );
	float3 s = float3( 0.0, 0.0, 0.0 );
	float n = 0.0;
	const int f = RG_G.opts.z;
	// a young mean (TAA off, a cut, a disocclusion): the same taps, twice as wide
	const int st = RestirGiAcc[id].w < 4.0 ? 2 : 1;
	if ( g.w > 0.0 && g.w < 1e5 )
		for ( int dy = -f; dy <= f; dy++ )
			for ( int dx = -f; dx <= f; dx++ ) {
				const int qx = x + dx * st, qy = y + dy * st;
				if ( qx < 0 || qy < 0 || qx >= RG_G.size.x || qy >= RG_G.size.y )
					continue;
				const float4 h = RestirGi_Unpack( RestirGiPk[qy * RG_G.size.x + qx] );
				if ( ( dx != 0 || dy != 0 ) && !Restir_Similar( g.w, g.xyz, h.w, h.xyz ) )
					continue;
				const float4 a = RestirGiAcc[qy * RG_G.size.x + qx];
				if ( a.w > 0.0 ) {
					s += a.xyz;
					n += 1.0;
				}
			}
	const float3 m = n > 0.0 ? s / n : float3( 0.0, 0.0, 0.0 );
	RestirGiAtrA[id] = float4( m, RestirGiAtrB[id].w / max( n, 1.0 ) );
	RestirGiOut[id] = float4( m, g.w );
}

/* mode 4 + it: one a-trous pass of the edge-avoiding wavelet filter [Dammertz 2010; Schied 2017, SVGF 4.3]: the 5 x 5
 * B3 spline kernel (3/8, 1/4, 1/16) with holes, step 2^it; edge stops on depth and normal (restirSimilar, then
 * cos^128 of the normals) and on luminance against the variance guide (sigma_l 4: SVGF's); the variance is filtered
 * with the squared weights. Ping-pong atrA -> atrB -> atrA ...; the last pass (knobs.w of them) writes outE. */
// (HLSL: atrA / atrB / outE = RestirGiAtrA / RestirGiAtrB / RestirGiOut)
void RestirGi_ModeAtrous( int x, int y, uint id, int it )
{
	const bool fromA = ( it & 1 ) == 0;
	const float4 g = RestirGi_Unpack( RestirGiPk[id] );
	const float4 c = fromA ? RestirGiAtrA[id] : RestirGiAtrB[id];
	float4 o = c;
	if ( g.w > 0.0 && g.w < 1e5 ) {
		const int st = int( 1 ) << it;
		const float lc = RestirGi_Lum( c.xyz );
		const float sl = 4.0 * sqrt( max( c.w, 0.0 ) ) + 1e-4;
		const float hk[3] = { 0.375, 0.25, 0.0625 };
		float3 s = float3( 0.0, 0.0, 0.0 );
		float v = 0.0, ws = 0.0;
		for ( int dy = -2; dy <= 2; dy++ )
			for ( int dx = -2; dx <= 2; dx++ ) {
				const int qx = x + dx * st, qy = y + dy * st;
				if ( qx < 0 || qy < 0 || qx >= RG_G.size.x || qy >= RG_G.size.y )
					continue;
				const int q = qy * RG_G.size.x + qx;
				const float4 h = RestirGi_Unpack( RestirGiPk[q] );
				// acc[q].w > 0 exactly where the depth is valid (modes 1-2): the g-buffer read answers it, one read fewer a tap
				if ( ( dx != 0 || dy != 0 ) && ( !Restir_Similar( g.w, g.xyz, h.w, h.xyz ) || !( h.w > 0.0 && h.w < 1e5 ) ) )
					continue;
				const float4 cq = fromA ? RestirGiAtrA[q] : RestirGiAtrB[q];
				const float wn = pow( max( dot( g.xyz, h.xyz ), 0.0 ), 128.0 );
				const float wl = exp( -abs( lc - RestirGi_Lum( cq.xyz ) ) / sl );
				const float w = hk[abs( dx )] * hk[abs( dy )] * wn * wl;
				s += cq.xyz * w;
				v += cq.w * w * w;
				ws += w;
			}
		if ( ws > 0.0 )
			o = float4( s / ws, v / ( ws * ws ) );
	}
	if ( fromA )
		RestirGiAtrB[id] = o;
	else
		RestirGiAtrA[id] = o;
	if ( it + 1 == int( RG_G.knobs.w ) )
		RestirGiOut[id] = float4( o.xyz, g.w );
}

// tid / gid / gi: SV_DispatchThreadID, SV_GroupID, SV_GroupIndex of a 64-wide group (restirgi_vk.inc splits x at 65535)
void RestirGi_CS( uint3 tid, uint3 gid, uint gi )
{
	// a group is an 8 x 8 tile, so its rays and taps stay close (a pixel's result does not depend on which invocation runs it)
	int x, y;
	if ( ( RG_G.opts.w & RESTIRGI_BIT_LINEAR ) != 0 ) {
		const uint l = DDGI_LinearId( tid );
		x = int( l % uint( RG_G.size.x ) );
		y = int( l / uint( RG_G.size.x ) );
	} else {
		const uint t = DDGI_GroupId( gid ), tw = ( uint( RG_G.size.x ) + 7u ) / 8u;
		x = int( ( t % tw ) * 8u + ( gi & 7u ) );
		y = int( ( t / tw ) * 8u + ( gi >> 3u ) );
	}
	const uint mode = DDGI_JOB.job.x;
#ifndef RESTIRGI_FILTER
	// the half / quarter trace, modes 0 and 1: an invocation a 2 x 1 (half) or 2 x 2 (quarter) block, so every invocation
	// holds exactly one traced pixel (a checkerboard over invocations leaves half of each subgroup idle in the ray loops)
	const int tb = RG_G.opts.w & ( RESTIRGI_BIT_HALF | RESTIRGI_BIT_QUARTER );
	if ( mode <= 1u && tb != 0 && ( RG_G.opts.w & RESTIRGI_BIT_LINEAR ) == 0 ) {
		const int bh = ( tb & RESTIRGI_BIT_QUARTER ) != 0 ? 2 : 1;
		const uint t = DDGI_GroupId( gid ), tw = ( uint( ( RG_G.size.x + 1 ) / 2 ) + 7u ) / 8u;   // tiles over the block grid
		const int bx = int( ( t % tw ) * 8u + ( gi & 7u ) ), by = int( ( t / tw ) * 8u + ( gi >> 3u ) );
		// the traced pixel first (k = 0 in every invocation: the rays run together), then the skipped ones (history only)
		const uint f = uint( RG_G.size.z );
		const int k3 = int( f & 3u );
		const int o0 = bh == 2 ? ( k3 == 0 ? 0 : k3 == 1 ? 3 : k3 == 2 ? 1 : 2 ) : ( by + int( f & 1u ) ) & 1;
		for ( int k = 0; k < 2 * bh; k++ ) {
			const int s = ( k + o0 ) % ( 2 * bh );
			const int px = 2 * bx + ( s & 1 ), py = bh * by + ( s >> 1 );
			if ( px >= RG_G.size.x || py >= RG_G.size.y )
				continue;
			if ( mode == 0u )
				RestirGi_ModeCandidate( px, py, uint( py * RG_G.size.x + px ) );
			else
				RestirGi_ModeSpatial( px, py, uint( py * RG_G.size.x + px ) );
		}
		return;
	}
#endif
	if ( x >= RG_G.size.x || y >= RG_G.size.y )
		return;
	const uint id = uint( y * RG_G.size.x + x );
#ifdef RESTIRGI_FILTER
	// the filter kernel (rtddgi_spirv.sh -DRESTIRGI_FILTER -> restirgi_filt.spv.h): modes 2.. only, so no ray code and
	// its registers stays in it (one module for every mode sized the a-trous passes' occupancy by the bounce's)
	if ( mode == 2u )
#else
	if ( mode == 0u )
		RestirGi_ModeCandidate( x, y, id );
	else if ( mode == 1u )
		RestirGi_ModeSpatial( x, y, id );
	else if ( mode == 2u )
#endif
		RestirGi_ModeAccumulate( x, y, id );
	else if ( mode == 3u )
		RestirGi_ModeFilter( x, y, id );
	else
		RestirGi_ModeAtrous( x, y, id, int( mode ) - 4 );
}

#endif
