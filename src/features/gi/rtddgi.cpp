/* Lane RTXGI1 (2026-10-29): ray-traced DDGI, milestone 1 (src/features/gi/rtddgi.h). The volume's steps run twice from the same
 * packed inputs: on the GPU (src/features/gi/rtddgi/rtddgi_vk.inc, inside bakert.cpp) or here on the CPU, where every function
 * mirrors one kernel (traceRay = trace.comp, blendProbe = blend.comp, borderProbe = border.comp, relocate/classify =
 * reloc.comp) and every rule comes from src/rtddgimath.h. */

#include "features/gi/rtddgi.h"

#include "features/gi/bakert.h"
#include "features/gi/probebvh.h"
#include "features/gi/probeplace.h"
#include "features/gi/probesky.h"
#include "features/gi/rtddgimath.h"
#include "restirmath.h"   // lane RESTIRDI1: the twin of restir.comp (restirCli)
#include "features/gi/restirgimath.h"   // RTEMIS1 part 2: the twin of restirgi.comp (restirgiCli)
#include "features/gi/rtddgi/rtddgi_gpu.h"
#include "gl/celllights.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>

using namespace rtddgi;

namespace {

constexpr int kAttr = 8;     // floats a soup triangle in triAttr
constexpr int kLight = 32;   // floats a light in the packed list (8 vec4, ddgi_common.glsl binding 13)
constexpr unsigned kRedNoBack = 1u, kRedNoCheb = 2u, kRedNoBias = 4u;   // ddgi_common.glsl DDGI_RED_*
constexpr unsigned kRedHwHit = 8u, kRedNoTie = 16u;   // M1FIX: hwhit is GPU only (the twin has no hardware hit to trust)
constexpr unsigned kRedBounceNoCover = 32u;   // FIX1101A: the bounce read renormalised, black past the usable probes (today's main)

void parallelFor( size_t n, const std::function<void( size_t )> & fn )
{
	const int nThreads = bakert::threads();
	std::atomic<size_t> next( 0 );
	std::vector<std::thread> pool;
	for ( int i = 0; i < nThreads; i++ )
		pool.emplace_back( [&]() {
			for ( size_t k; ( k = next.fetch_add( 1 ) ) < n; )
				fn( k );
		} );
	for ( std::thread & t : pool )
		t.join();
}

//! one light into 32 floats of the soup's local frame (positions and box planes moved by -O in x, y)
void packLight( const WwCellLight & l, const double O[2], float * o )
{
	std::fill( o, o + kLight, 0.0f );
	o[0] = float( double( l.pos[0] ) - O[0] );
	o[1] = float( double( l.pos[1] ) - O[1] );
	o[2] = l.pos[2];
	o[3] = l.radius;
	for ( int c = 0; c < 3; c++ ) {
		o[4 + c] = l.color[c];
		o[8 + c] = l.dir[c];
	}
	o[7] = l.spot ? 1.0f : 0.0f;
	o[11] = l.cosOuter;
	o[12] = l.bias;
	o[13] = l.scale;
	o[14] = l.exponent;
	o[15] = l.cone;
	for ( int k = 0; k < 3; k++ ) {   // dot(b, Pw) + w = dot(b, Pl) + (w + b.x O.x + b.y O.y)
		o[16 + k * 4 + 0] = l.box[k][0];
		o[16 + k * 4 + 1] = l.box[k][1];
		o[16 + k * 4 + 2] = l.box[k][2];
		o[16 + k * 4 + 3] = float( double( l.box[k][3] ) + double( l.box[k][0] ) * O[0] + double( l.box[k][1] ) * O[1] );
	}
	o[28] = float( l.shape );
	// lane RESTIRDI1 (unused by the DDGI kernels): ReSTIR DI's shadow sphere, lightphys.h's area rule min(8, 0.3 r)
	// clamped 2..32 units, and whether the light casts shadows (the ReSTIR candidates)
	o[29] = std::clamp( std::min( 8.0f, 0.3f * l.radius ), 2.0f, 32.0f );
	o[30] = l.shadow ? 1.0f : 0.0f;
}

unsigned redBits( const QString & red )
{
	unsigned b = 0;
	for ( const QString & r : red.split( ',', Qt::SkipEmptyParts ) ) {
		const QString t = r.trimmed();
		if ( t == QLatin1String( "noback" ) ) b |= kRedNoBack;
		if ( t == QLatin1String( "nocheb" ) ) b |= kRedNoCheb;
		if ( t == QLatin1String( "nobias" ) ) b |= kRedNoBias;
		if ( t == QLatin1String( "hwhit" ) ) b |= kRedHwHit;
		if ( t == QLatin1String( "notie" ) ) b |= kRedNoTie;
		if ( t == QLatin1String( "bouncenocover" ) ) b |= kRedBounceNoCover;
	}
	return b;
}

//! the inputs both runs read, in the soup's local frame (x, y moved by -O, probegi's frame)
struct Packed
{
	probebvh::Bvh bvh;
	double O[2] = { 0, 0 };
	std::vector<float> triAttr, lights, rotations;
	bakert::DdgiGpuParams prm;
	int nLights = 0;
};

// ------------------------------------------------------------------ the CPU twin
struct Twin
{
	const Packed & S;
	int N = 0, nP = 0;
	int dims[3] = { 0, 0, 0 };
	V3 origin, step;
	unsigned red = 0;
	std::vector<float> rays, probes, irr, dist;
	std::vector<quint32> age;
	// RTEMIS1 telemetry, a probe's blended rays (one thread a probe): emissive hits, their Le and every ray's radiance (luminance)
	std::vector<qint64> emHits;
	std::vector<double> emLum, allLum;

	explicit Twin( const Packed & s ) : S( s )
	{
		N = s.prm.counts[0];
		for ( int k = 0; k < 3; k++ ) {
			dims[k] = s.prm.dims[k];
			origin[k] = s.prm.origin[k];
			step[k] = s.prm.stepv[k];
		}
		nP = s.prm.dims[3];
		red = unsigned( s.prm.flags[3] );
		rays.assign( size_t( nP ) * size_t( N ) * 4, 0.0f );
		probes.assign( size_t( nP ) * 4, 0.0f );
		irr.assign( size_t( nP ) * 64 * 4, 0.0f );
		dist.assign( size_t( nP ) * 256 * 2, 0.0f );
		age.assign( size_t( nP ), 0 );
		emHits.assign( size_t( nP ), 0 );
		emLum.assign( size_t( nP ), 0.0 );
		allLum.assign( size_t( nP ), 0.0 );
		for ( int p = 0; p < nP; p++ )
			probes[size_t( p ) * 4 + 3] = float( kVigilant );
	}
	int state( int p ) const { return int( probes[size_t( p ) * 4 + 3] + 0.5f ); }
	bool active( int p ) const { const int st = state( p ); return !( st == kOff || ( st == kAsleep && S.prm.flags[1] ) ); }
	V3 probePos( int p ) const
	{
		const int x = p % dims[0], y = ( p / dims[0] ) % dims[1], z = p / ( dims[0] * dims[1] );
		const float * o = &probes[size_t( p ) * 4];
		return V3( origin.x + float( x ) * step.x + o[0], origin.y + float( y ) * step.y + o[1], origin.z + float( z ) * step.z + o[2] );
	}
	bool shadow( V3 o, V3 d, float tmax ) const
	{
		if ( tmax <= 1e-3f )
			return false;
		const double od[3] = { o.x, o.y, o.z }, dd[3] = { d.x, d.y, d.z };
		return S.bvh.anyHitIn( od, dd, 1e-4, double( tmax ), []( int ) { return true; } );
	}
	// MIRROR trace.comp ddgiIrradianceAt (M2: the rule is rtddgimath.h sampleVolume, the cell view's GLSL read's too)
	V3 sampleVolume( V3 Pw, V3 n, V3 o ) const
	{
		SampleVolume v;
		v.irr = irr.data();
		v.dist = dist.data();
		v.probes = probes.data();
		v.age = age.data();
		v.origin = origin;
		v.step = step;
		for ( int k = 0; k < 3; k++ )
			v.dims[k] = dims[k];
		v.D = S.prm.stepv[3];
		v.B = S.prm.blendp[2];
		// the volume's red bits: only nocheb / nobias reach the read (noback = 1 is the trace's, not kSampleTrilinear)
		const SampleResult s = rtddgi::sampleVolume( v, Pw, n, o, n, red & ( kSampleNoCheb | kSampleNoBias ) );
		// FIX1101A: the flat ambient takes what the volume does not hold (fade x cover), as on the cell view's surfaces
		const auto & P = S.prm;
		const float R[6][3] = { { P.sky[0][0], P.sky[0][1], P.sky[0][2] }, { P.sky[1][0], P.sky[1][1], P.sky[1][2] },
			{ P.sky[2][0], P.sky[2][1], P.sky[2][2] }, { P.sky[3][0], P.sky[3][1], P.sky[3][2] },
			{ P.sky[4][0], P.sky[4][1], P.sky[4][2] }, { P.sky[5][0], P.sky[5][1], P.sky[5][2] } };
		float flat[3];
		flatAmbient( R, P.sky[0][3] > 0.5f, n, flat );
		return bounceIrradiance( s, flat, !( red & kRedBounceNoCover ) );
	}
	// MIRROR trace.comp ddgiDirect
	V3 direct( V3 Pw, V3 n ) const
	{
		V3 E( 0, 0, 0 );
		const V3 o = Pw + n * 0.5f;
		const auto & P = S.prm;
		if ( P.sunTo[3] > 0.5f ) {
			const V3 to( P.sunTo[0], P.sunTo[1], P.sunTo[2] );
			const float nl = dot( n, to );
			if ( nl > 0.0f && !shadow( o, to, P.sun[3] ) )
				E = E + V3( P.sun[0], P.sun[1], P.sun[2] ) * nl;
		}
		if ( P.dirTo[3] > 0.5f ) {
			const float nl = dot( n, V3( P.dirTo[0], P.dirTo[1], P.dirTo[2] ) );
			if ( nl > 0.0f )
				E = E + V3( P.dirColor[0], P.dirColor[1], P.dirColor[2] ) * nl;
		}
		for ( int li = 0; li < S.nLights; li++ ) {
			const float * l = &S.lights[size_t( li ) * kLight];
			const V3 lp( l[0], l[1], l[2] );
			const V3 Lv = lp - Pw;
			const float d = length( Lv );
			if ( d >= l[3] )
				continue;
			const int shape = int( l[28] + 0.5f );
			if ( shape == 1 && dot( Pw - lp, V3( l[8], l[9], l[10] ) ) < 0.0f )
				continue;
			if ( shape == 2 ) {
				bool inside = true;
				for ( int k = 0; k < 3; k++ ) {
					const float * bx = &l[16 + k * 4];
					if ( std::fabs( bx[0] * Pw.x + bx[1] * Pw.y + bx[2] * Pw.z + bx[3] ) > 1.0f )
						inside = false;
				}
				if ( !inside )
					continue;
			}
			const V3 L = Lv * ( 1.0f / std::max( d, 0.001f ) );
			const float nl = dot( n, L );
			if ( nl <= 0.0f )
				continue;
			float att = lightRadial( d, l[3], l[12], l[13], l[14] );
			if ( l[7] > 0.5f )
				att *= spotCone( -dot( L, V3( l[8], l[9], l[10] ) ), l[11], l[15] );
			if ( att * nl <= 0.0f )
				continue;
			const V3 dl = lp - o;
			const float len = length( dl );
			if ( shadow( o, dl * ( 1.0f / std::max( len, 1e-6f ) ), len - P.dirColor[3] ) )
				continue;
			E = E + V3( l[4], l[5], l[6] ) * ( att * nl );
		}
		return E;
	}
	// MIRROR trace.comp ddgiBackFace: a one-sided face seen from behind
	bool backFace( int k, V3 d ) const
	{
		const float * v = &S.bvh.t[size_t( k ) * 9];
		const V3 v0( v[0], v[1], v[2] );
		const V3 nf = normalize( cross( V3( v[3], v[4], v[5] ) - v0, V3( v[6], v[7], v[8] ) - v0 ) );
		return S.triAttr[size_t( k ) * kAttr + 3] < 0.5f && dot( nf, d ) > 0.0f;
	}
	// MIRROR trace.comp ddgiNearest: probebvh's nearest hit (t in (1e-4, tmax], holes pass), then the tie pass: of
	// every face hit within tieWindow of that t, the one tieWins keeps (probebvh alone keeps the LAST one its tree
	// visits at the smallest t, the hardware any one: a flipped duplicate's back or front by chance)
	bool nearest( V3 o, V3 d, float tmax, double * tOut, int * triOut ) const
	{
		const double od[3] = { o.x, o.y, o.z }, dd[3] = { d.x, d.y, d.z };
		double t = 0.0;
		int tri = -1;
		if ( !S.bvh.ray( od, dd, double( tmax ), &t, &tri ) || tri < 0 )
			return false;
		if ( !( red & kRedNoTie ) ) {
			const double w = double( tieWindow( o, float( t ) ) );
			int keep = tri;
			bool keepBack = backFace( tri, d );
			S.bvh.anyHitIn( od, dd, std::max( double( kTraceTmin ), t - w ), t + w, [&]( int k ) {
				const bool b = backFace( k, d );
				if ( tieWins( b, k, keepBack, keep ) ) {
					keep = k;
					keepBack = b;
				}
				return false;   // see every face in the window
			} );
			if ( keep != tri ) {
				double tk = t;
				if ( probebvh::Bvh::tri( &S.bvh.t[size_t( keep ) * 9], od, dd, tk ) )
					t = tk;
				tri = keep;
			}
		}
		*tOut = t;
		*triOut = tri;
		return true;
	}
	// MIRROR trace.comp main
	void traceRay( int p, int i, const M3 & rot )
	{
		const auto & P = S.prm;
		const V3 o = probePos( p );
		const V3 d = rayDir( i, N, rot );
		double t = 0.0;
		int tri = -1;
		float * rec = &rays[( size_t( p ) * size_t( N ) + size_t( i ) ) * 4];
		if ( !nearest( o, d, P.sun[3], &t, &tri ) ) {
			float sky[3] = { 0, 0, 0 };
			if ( P.sky[0][3] > 0.5f && i >= kFixedRays ) {
				const float R[6][3] = { { P.sky[0][0], P.sky[0][1], P.sky[0][2] }, { P.sky[1][0], P.sky[1][1], P.sky[1][2] },
					{ P.sky[2][0], P.sky[2][1], P.sky[2][2] }, { P.sky[3][0], P.sky[3][1], P.sky[3][2] },
					{ P.sky[4][0], P.sky[4][1], P.sky[4][2] }, { P.sky[5][0], P.sky[5][1], P.sky[5][2] } };
				const float ground[3] = { P.sky[1][3], P.sky[2][3], P.sky[3][3] };
				missRadiance( R, ground, d, P.sky[4][3] > 0.5f, sky );   // lane SKYBELOW1: the ground below the horizon
			}
			rec[0] = sky[0];
			rec[1] = sky[1];
			rec[2] = sky[2];
			rec[3] = kMiss;
			return;
		}
		const float tf = float( t );
		const float * v = &S.bvh.t[size_t( tri ) * 9];
		const V3 v0( v[0], v[1], v[2] );
		const V3 nf = normalize( cross( V3( v[3], v[4], v[5] ) - v0, V3( v[6], v[7], v[8] ) - v0 ) );
		const float * a0 = &S.triAttr[size_t( tri ) * kAttr];
		const bool back = a0[3] < 0.5f && dot( nf, d ) > 0.0f;
		if ( back && !( red & kRedNoBack ) ) {
			rec[0] = rec[1] = rec[2] = 0.0f;
			rec[3] = -kBackfaceDist * tf;
			return;
		}
		if ( i < kFixedRays ) {
			rec[0] = rec[1] = rec[2] = 0.0f;
			rec[3] = tf;
			return;
		}
		const V3 n = dot( nf, d ) > 0.0f ? -nf : nf;
		const V3 Pw = o + d * tf;
		V3 E = direct( Pw, n );
		if ( !P.flags[0] )
			E = E + sampleVolume( Pw, n, -d );
		const float Ec[3] = { E[0], E[1], E[2] };
		hitRadiance( a0, Ec, a0 + 4, rec );   // RTEMIS1: MIRROR trace.comp ddgiHitRadiance
		rec[3] = tf;
		const double le = 0.2126 * a0[4] + 0.7152 * a0[5] + 0.0722 * a0[6];
		if ( le > 0.0 ) {
			emHits[size_t( p )]++;
			emLum[size_t( p )] += le;
		}
		allLum[size_t( p )] += 0.2126 * rec[0] + 0.7152 * rec[1] + 0.0722 * rec[2];
	}
	// MIRROR blend.comp
	void blendProbe( int p, const M3 & rot )
	{
		const int nb = N - kFixedRays;
		std::vector<V3> dirs( static_cast<size_t>( nb ) );
		std::vector<float> rad( static_cast<size_t>( nb ) * 3 ), dd( static_cast<size_t>( nb ) );
		for ( int r = 0; r < nb; r++ ) {
			dirs[size_t( r )] = rayDir( kFixedRays + r, N, rot );
			const float * rec = &rays[( size_t( p ) * size_t( N ) + size_t( kFixedRays + r ) ) * 4];
			for ( int c = 0; c < 3; c++ )
				rad[size_t( r ) * 3 + size_t( c )] = rec[c];
			dd[size_t( r )] = rec[3];
		}
		const float hBase = ageHysteresis( age[size_t( p )], S.prm.blendp[0] );
		float * it = &irr[size_t( p ) * 256];
		for ( int ty = 0; ty < kIrrInterior; ty++ )
			for ( int tx = 0; tx < kIrrInterior; tx++ ) {
				float * at = &it[( ( ty + 1 ) * kIrrTile + tx + 1 ) * 4];
				const V3 td = texelDir( tx, ty, kIrrInterior );
				float out[3], h = hBase, vis = at[3];
				if ( blendIrradianceTexel( td, dirs.data(), rad.data(), nb, hBase, at, out, &h ) ) {
					blendSkyVisTexel( td, dirs.data(), dd.data(), nb, h, at[3], &vis, S.prm.sky[4][3] > 0.5f );   // M1FIX: the alpha (SKYBELOW1: above the horizon)
					at[0] = out[0];
					at[1] = out[1];
					at[2] = out[2];
					at[3] = vis;
				}
			}
		float * dt = &dist[size_t( p ) * 512];
		for ( int ty = 0; ty < kDistInterior; ty++ )
			for ( int tx = 0; tx < kDistInterior; tx++ ) {
				float * at = &dt[( ( ty + 1 ) * kDistTile + tx + 1 ) * 2];
				float out[2];
				if ( blendDistanceTexel( texelDir( tx, ty, kDistInterior ), dirs.data(), dd.data(), nb, S.prm.blendp[1], S.prm.origin[3],
						hBase, at, out ) ) {
					at[0] = out[0];
					at[1] = out[1];
				}
			}
	}
	// MIRROR border.comp
	void borderProbe( int p )
	{
		float * it = &irr[size_t( p ) * 256];
		for ( int k = 0; k < 4 * kIrrTile - 4; k++ ) {
			int x, y, sx, sy;
			borderXY( k, kIrrTile, x, y );
			borderSource( x, y, kIrrTile, sx, sy );
			std::memcpy( &it[( y * kIrrTile + x ) * 4], &it[( sy * kIrrTile + sx ) * 4], 16 );
		}
		float * dt = &dist[size_t( p ) * 512];
		for ( int k = 0; k < 4 * kDistTile - 4; k++ ) {
			int x, y, sx, sy;
			borderXY( k, kDistTile, x, y );
			borderSource( x, y, kDistTile, sx, sy );
			std::memcpy( &dt[( y * kDistTile + x ) * 2], &dt[( sy * kDistTile + sx ) * 2], 8 );
		}
		age[size_t( p )]++;
	}
	void fixedRays( int p, V3 * dirs, float * d ) const
	{
		for ( int i = 0; i < kFixedRays; i++ ) {
			dirs[i] = sphericalFibonacci( i, kFixedRays );
			d[i] = rays[( size_t( p ) * size_t( N ) + size_t( i ) ) * 4 + 3];
		}
	}
	// MIRROR reloc.comp mode 4
	void relocate( int p )
	{
		V3 dirs[kFixedRays];
		float d[kFixedRays];
		fixedRays( p, dirs, d );
		float * o = &probes[size_t( p ) * 4];
		const V3 n = relocateStep( dirs, d, kFixedRays, V3( o[0], o[1], o[2] ), step );
		o[0] = n.x;
		o[1] = n.y;
		o[2] = n.z;
	}
	// MIRROR reloc.comp mode 5
	void classifyProbe( int p )
	{
		V3 dirs[kFixedRays];
		float d[kFixedRays];
		fixedRays( p, dirs, d );
		probes[size_t( p ) * 4 + 3] = float( classify( dirs, d, kFixedRays, step ) );
	}
};

M3 rotationOf( const Packed & S, int f )
{
	M3 r;
	if ( S.rotations.size() >= size_t( f + 1 ) * 9 )
		std::memcpy( r.m, &S.rotations[size_t( f ) * 9], 36 );
	return r;
}

void runTwin( const Packed & S, const RtDdgiSpec & spec, RtDdgiVolume * out, std::vector<float> * raysOut = nullptr )
{
	Twin T( S );
	QElapsedTimer tm;
	tm.start();
	const int iters = spec.relocate ? kRelocIterations : 0;
	const M3 ident = M3();
	for ( int it = 0; it < iters + ( spec.classify ? 1 : 0 ); it++ ) {
		parallelFor( size_t( T.nP ), [&]( size_t p ) {
			for ( int i = 0; i < kFixedRays; i++ )
				T.traceRay( int( p ), i, ident );
		} );
		parallelFor( size_t( T.nP ), [&]( size_t p ) {
			if ( it < iters )
				T.relocate( int( p ) );
			else
				T.classifyProbe( int( p ) );
		} );
		out->rays += qint64( T.nP ) * kFixedRays;
	}
	out->msReloc = double( tm.nsecsElapsed() ) / 1e6;
	tm.restart();
	for ( int f = 0; f < spec.frames; f++ ) {
		const M3 rot = rotationOf( S, f );
		std::atomic<qint64> traced( 0 );
		parallelFor( size_t( T.nP ), [&]( size_t p ) {
			if ( !T.active( int( p ) ) )
				return;
			for ( int i = 0; i < T.N; i++ )
				T.traceRay( int( p ), i, rot );
			traced += T.N;
		} );
		parallelFor( size_t( T.nP ), [&]( size_t p ) {
			if ( !T.active( int( p ) ) )
				return;
			T.blendProbe( int( p ), rot );
			T.borderProbe( int( p ) );
		} );
		out->rays += traced.load();
		out->emisRays += traced.load() / std::max( T.N, 1 ) * ( T.N - kFixedRays );   // RTEMIS1: the shaded (blended) rays
	}
	out->msFrames = double( tm.nsecsElapsed() ) / 1e6;
	out->emisHits = 0;   // RTEMIS1: the blended rays' emissive census (fixed rays are never shaded, so never counted)
	out->emisLum = out->allLum = 0.0;
	for ( int p = 0; p < T.nP; p++ ) {
		out->emisHits += T.emHits[size_t( p )];
		out->emisLum += T.emLum[size_t( p )];
		out->allLum += T.allLum[size_t( p )];
	}
	out->offset.resize( size_t( T.nP ) * 3 );
	out->state.resize( size_t( T.nP ) );
	for ( int p = 0; p < T.nP; p++ ) {
		for ( int k = 0; k < 3; k++ )
			out->offset[size_t( p ) * 3 + size_t( k )] = T.probes[size_t( p ) * 4 + size_t( k )];
		out->state[size_t( p )] = T.state( p );
	}
	if ( spec.dumpFixed ) {   // the last trace of each probe's fixed rays (trace.comp writes the same rows)
		out->fixedDist.resize( size_t( T.nP ) * kFixedRays );
		for ( int p = 0; p < T.nP; p++ )
			for ( int i = 0; i < kFixedRays; i++ )
				out->fixedDist[size_t( p ) * kFixedRays + size_t( i )] = T.rays[( size_t( p ) * size_t( T.N ) + size_t( i ) ) * 4 + 3];
		if ( raysOut )   // lane SKYBELOW1: the miss census
			*raysOut = std::move( T.rays );
	}
	out->age = std::move( T.age );
	out->irr = std::move( T.irr );
	out->dist = std::move( T.dist );
}

bool hasRed( const QString & red, const char * name )
{
	for ( const QString & r : red.split( ',', Qt::SkipEmptyParts ) )
		if ( r.trimmed() == QLatin1String( name ) )
			return true;
	return false;
}

} // namespace

void RtDdgiVolume::position( int p, float out[3] ) const
{
	const int idx[3] = { p % dims[0], ( p / dims[0] ) % dims[1], p / ( dims[0] * dims[1] ) };
	for ( int k = 0; k < 3; k++ )
		out[k] = origin[k] + float( idx[k] ) * step[k] + ( offset.size() > size_t( p ) * 3 + 2 ? offset[size_t( p ) * 3 + size_t( k )] : 0.0f );
}

//! the volume's grid and every packed input both runs read (rtddgiRun and RtDdgiLive::open)
static bool packVolume( const ProbeSoup & soup, const WwCellLighting & lighting, const ProbeSkyLight & sky, const RtDdgiSpec & spec,
	RtDdgiVolume * out, Packed & S, bool buildBvh = true )   // RTXCASCADE2: buildBvh false = S.bvh.t filled, its tree not built
{
	*out = RtDdgiVolume();
	QElapsedTimer tm;
	tm.start();
	if ( soup.tris.empty() ) {
		out->error = QStringLiteral( "an empty soup" );
		return false;
	}
	if ( spec.rays <= kFixedRays || spec.rays > kMaxRays || spec.spacing <= 0.0f || spec.zScale <= 0.0f ) {
		out->error = QStringLiteral( "rays must be %1..%2 and the spacing above 0" ).arg( kFixedRays + 1 ).arg( kMaxRays );
		return false;
	}
	// ---- the grid's box: the placement's probes (probegi's), one step past them each side (ours), else the soup's
	float st[3] = { spec.spacing, spec.spacing, spec.spacing * spec.zScale };
	float lo[3] = { 3.4e38f, 3.4e38f, 3.4e38f }, hi[3] = { -3.4e38f, -3.4e38f, -3.4e38f };
	bool have = false;
	if ( spec.box[3] > spec.box[0] && spec.box[4] > spec.box[1] && spec.box[5] > spec.box[2] ) {   // M2: the caller's box
		for ( int k = 0; k < 3; k++ ) {
			lo[k] = spec.box[k];
			hi[k] = spec.box[3 + k];
		}
		have = true;
	}
	if ( !have && spec.place ) {
		ProbePlaceSpec ps;
		ps.minX = spec.rect[0];
		ps.minY = spec.rect[1];
		ps.maxX = spec.rect[2];
		ps.maxY = spec.rect[3];
		ProbePlaceResult pr;
		if ( probePlace( soup, ps, &pr ) )
			for ( const ProbePoint & q : pr.probes ) {
				for ( int k = 0; k < 3; k++ ) {
					lo[k] = std::min( lo[k], q.pos[k] - st[k] );
					hi[k] = std::max( hi[k], q.pos[k] + st[k] );
				}
				have = true;
			}
	}
	if ( !have ) {
		for ( size_t i = 0; i < soup.tris.size(); i += 3 )
			for ( int k = 0; k < 3; k++ ) {
				lo[k] = std::min( lo[k], soup.tris[i + size_t( k )] );
				hi[k] = std::max( hi[k], soup.tris[i + size_t( k )] );
			}
		if ( spec.rect[2] > spec.rect[0] && spec.rect[3] > spec.rect[1] ) {
			lo[0] = std::max( lo[0], spec.rect[0] );
			lo[1] = std::max( lo[1], spec.rect[1] );
			hi[0] = std::min( hi[0], spec.rect[2] );
			hi[1] = std::min( hi[1], spec.rect[3] );
		}
	}
	qint64 nP = 1;
	for ( int pass = 0; pass < 64; pass++ ) {
		nP = 1;
		for ( int k = 0; k < 3; k++ ) {
			out->step[k] = st[k];
			out->origin[k] = std::floor( lo[k] / st[k] ) * st[k];   // world multiples of the spacing
			out->dims[k] = std::max( 2, int( std::floor( ( hi[k] - out->origin[k] ) / st[k] ) ) + 1 );
			nP *= out->dims[k];
		}
		if ( !spec.fitProbes || nP <= spec.maxProbes )
			break;
		// M2: the cell's volume keeps its cap by a wider spacing (the census names it), never by a smaller box
		const float g = std::max( 1.02f, float( std::cbrt( double( nP ) / double( std::max( spec.maxProbes, 8 ) ) ) ) );
		for ( int k = 0; k < 3; k++ )
			st[k] *= g;
	}
	out->probes = int( std::min<qint64>( nP, 0x7fffffff ) );
	if ( nP > spec.maxProbes ) {
		out->error = QStringLiteral( "%1 x %2 x %3 = %4 probes, over the cap %5 (a smaller rect or a wider spacing)" )
			.arg( out->dims[0] ).arg( out->dims[1] ).arg( out->dims[2] ).arg( nP ).arg( spec.maxProbes );
		return false;
	}
	out->distClamp = spec.distClamp > 0.0f ? spec.distClamp
		: 2.0f * std::sqrt( st[0] * st[0] + st[1] * st[1] + st[2] * st[2] );

	// ---- the packed inputs (the soup's local frame, probegi's: x, y moved by the floor of the box centre)
	{
		double mn[2] = { 1e300, 1e300 }, mx[2] = { -1e300, -1e300 };
		for ( size_t i = 0; i < soup.tris.size(); i += 3 )
			for ( int k = 0; k < 2; k++ ) {
				mn[k] = std::min( mn[k], double( soup.tris[i + size_t( k )] ) );
				mx[k] = std::max( mx[k], double( soup.tris[i + size_t( k )] ) );
			}
		for ( int k = 0; k < 2; k++ )
			S.O[k] = std::floor( ( mn[k] + mx[k] ) * 0.5 );
	}
	S.bvh.t.resize( soup.tris.size() );
	for ( size_t i = 0; i < soup.tris.size(); i += 3 ) {
		S.bvh.t[i + 0] = float( double( soup.tris[i + 0] ) - S.O[0] );
		S.bvh.t[i + 1] = float( double( soup.tris[i + 1] ) - S.O[1] );
		S.bvh.t[i + 2] = soup.tris[i + 2];
	}
	if ( buildBvh )
		S.bvh.build();
	S.bvh.mask = &soup.amask;
	const size_t nTri = soup.tris.size() / 9;
	S.triAttr.assign( nTri * kAttr, 0.0f );
	const bool hasAlb = soup.alb.size() >= nTri * 3;
	const bool noEmis = hasRed( spec.red, "noemissive" ), emiCentre = hasRed( spec.red, "emicentre" );
	out->emisTris = 0;
	out->emisArea = out->emisPower = 0.0;
	for ( size_t t = 0; t < nTri; t++ ) {
		float * a = &S.triAttr[t * kAttr];
		for ( int c = 0; c < 3; c++ )
			a[c] = hasAlb ? soup.alb[t * 3 + size_t( c )] / 255.0f : 0.5f;   // ours: mid grey without ALB1
		bounceAlbedo( a, spec.bounceSat );   // GIFIX1101 1B (1 = as read)
		a[3] = soup.isTwoSided( t ) ? 1.0f : 0.0f;
		if ( !noEmis && soup.glow.emits( int( t ) ) ) {   // RTEMIS1: the glow's area mean (red emicentre: M1's centre texel)
			double le[3];
			const int ti = int( t );
			if ( emiCentre )
				soup.glow.le( ti, 1.0 / 3.0, 1.0 / 3.0, le );
			else
				emitAreaMean( [&]( double b1, double b2, double v[3] ) { soup.glow.le( ti, b1, b2, v ); }, le );
			for ( int c = 0; c < 3; c++ )
				a[4 + c] = float( le[c] ) * lighting.glowScale;	// lane MUSGI1 r5: a sunlit interior's lamp scale
			const double lum = 0.2126 * a[4] + 0.7152 * a[5] + 0.0722 * a[6];
			if ( lum > 0.0 ) {
				const float * v = &soup.tris[t * 9];
				const double e1[3] = { v[3] - v[0], v[4] - v[1], v[5] - v[2] }, e2[3] = { v[6] - v[0], v[7] - v[1], v[8] - v[2] };
				const double cx = e1[1] * e2[2] - e1[2] * e2[1], cy = e1[2] * e2[0] - e1[0] * e2[2], cz = e1[0] * e2[1] - e1[1] * e2[0];
				const double area = 0.5 * std::sqrt( cx * cx + cy * cy + cz * cz );
				out->emisTris++;
				out->emisArea += area;
				out->emisPower += kPi * lum * area * ( a[3] > 0.5f ? 2.0 : 1.0 );
			}
		}
	}
	for ( const WwCellLight & l : lighting.lights ) {
		if ( l.radius <= 0.0f )
			continue;
		S.lights.resize( size_t( S.nLights + 1 ) * kLight );
		packLight( l, S.O, &S.lights[size_t( S.nLights ) * kLight] );
		S.nLights++;
	}
	const bool noRot = hasRed( spec.red, "norot" );
	S.rotations.resize( size_t( std::max( spec.frames, 0 ) ) * 9 );
	for ( int f = 0; f < spec.frames; f++ ) {
		const M3 r = noRot ? M3() : updateRotation( spec.seed, quint32( f ) );
		std::memcpy( &S.rotations[size_t( f ) * 9], r.m, 36 );
	}
	auto & P = S.prm;
	for ( int k = 0; k < 3; k++ ) {
		P.dims[k] = out->dims[k];
		P.origin[k] = out->origin[k] - ( k < 2 ? float( S.O[k] ) : 0.0f );
		P.stepv[k] = st[k];
	}
	P.dims[3] = int( nP );
	P.origin[3] = out->distClamp;
	P.stepv[3] = std::min( st[0], std::min( st[1], st[2] ) );
	const bool sunOn = sky.on && ( sky.sun[0] > 0.0f || sky.sun[1] > 0.0f || sky.sun[2] > 0.0f );
	const V3 sunTo = normalize( V3( sky.sunTo[0], sky.sunTo[1], sky.sunTo[2] ) );
	for ( int k = 0; k < 3; k++ ) {
		P.sunTo[k] = sunTo[k];
		P.sun[k] = sky.sun[k];
	}
	P.sunTo[3] = sunOn ? 1.0f : 0.0f;
	P.sun[3] = spec.traceMax;
	if ( lighting.interior && lighting.hasDirectional ) {   // probegi's interior directional: no shadow ray
		const V3 to = normalize( V3( lighting.dirTo[0], lighting.dirTo[1], lighting.dirTo[2] ) );
		for ( int k = 0; k < 3; k++ ) {
			P.dirTo[k] = to[k];
			P.dirColor[k] = lighting.dirColor[k];
		}
		P.dirTo[3] = 1.0f;
	}
	P.dirColor[3] = spec.fixtureClear;
	if ( sky.on && !spec.noSky ) {
		float R[6][3];
		out->skyClamped = skyRadianceCube( sky.amb, R );
		for ( int f = 0; f < 6; f++ )
			for ( int c = 0; c < 3; c++ )
				P.sky[f][c] = out->skyR[f][c] = R[f][c];
		out->skyOn = true;
		P.sky[0][3] = 1.0f;
	}
	/* lane SKYBELOW1 (bungo 10-30, "Shouldn't that be a rule for every probe"): a miss below the horizon returns the
	 * GROUND, never sky, and no below-horizon direction is sky visibility (the rule is P.sky[4].w, on unless red; the
	 * visibility half holds with no sky too). An exterior's ground = the soup's up-facing albedo (area-weighted, the
	 * triangles' ALB1, 0.5 grey without it as at a hit) x the weather's +Z irradiance (an open up-facing surface's) / pi;
	 * an interior's = 0, which replaces SKYINT1's zeroed ground-facing sky (rtddgicell.cpp skyLitSky) */
	out->skyBelowRule = !hasRed( spec.red, "skybelow" );
	P.sky[4][3] = out->skyBelowRule ? 1.0f : 0.0f;
	{
		double a[3] = { 0, 0, 0 }, w = 0;
		for ( size_t t = 0; t < nTri; t++ ) {
			const float * v = &S.bvh.t[t * 9];
			const V3 v0( v[0], v[1], v[2] );
			const V3 n = cross( V3( v[3], v[4], v[5] ) - v0, V3( v[6], v[7], v[8] ) - v0 );   // |n| = 2 x area
			const float l = length( n );
			if ( !( l > 0.0f ) || n.z < 0.7f * l )
				continue;
			for ( int c = 0; c < 3; c++ )
				a[c] += double( l ) * S.triAttr[t * kAttr + size_t( c )];
			w += double( l );
		}
		for ( int c = 0; c < 3; c++ )
			out->groundAlbedo[c] = w > 0.0 ? float( a[c] / w ) : 0.5f;
	}
	if ( sky.on && !spec.noSky ) {
		groundRadiance( out->groundAlbedo, sky.amb[4], lighting.interior, out->ground );
		for ( int c = 0; c < 3; c++ )
			P.sky[1 + c][3] = out->ground[c];
	}
	P.counts[0] = spec.rays;
	P.counts[1] = S.nLights;
	P.counts[2] = int( nTri );
	P.flags[0] = spec.oneBounce ? 1 : 0;
	P.flags[1] = spec.skipAsleep ? 1 : 0;
	P.flags[3] = int( redBits( spec.red ) );
	P.blendp[0] = spec.hysteresis;
	P.blendp[1] = spec.sharpness;
	P.blendp[2] = spec.bias;
	out->msBuild = double( tm.nsecsElapsed() ) / 1e6;
	return true;
}

/* lane SKYBELOW1: the miss census of the last update's rays (`rays` = the rays buffer, 4 floats a probe x ray; the
 * states in out). Counts the blended rays (i >= 32) of every probe the update traced. A miss "returns sky" when its
 * radiance is the sky cube's for its direction (within 1e-5 relative: the GPU's float order) and not the ground's;
 * aboveHash = FNV-1a over the above-horizon misses' radiance bytes in ray order */
static void missCensus( const Packed & S, const RtDdgiSpec & spec, const std::vector<float> & rays, RtDdgiVolume * out )
{
	const auto & P = S.prm;
	const int N = P.counts[0];
	const qint64 nP = out->probeCount();
	if ( spec.frames <= 0 || N <= kFixedRays || rays.size() < size_t( nP ) * size_t( N ) * 4 || out->state.size() < size_t( nP ) )
		return;
	const M3 rot = rotationOf( S, spec.frames - 1 );
	float R[6][3];
	for ( int f = 0; f < 6; f++ )
		for ( int c = 0; c < 3; c++ )
			R[f][c] = P.sky[0][3] > 0.5f ? P.sky[f][c] : 0.0f;
	const float * G = out->ground;
	auto near3 = []( const float * a, const float * b ) {
		for ( int c = 0; c < 3; c++ )
			if ( std::fabs( a[c] - b[c] ) > 1e-5f * std::max( 1.0f, std::fabs( b[c] ) ) )
				return false;
		return true;
	};
	out->missBelow = out->missBelowSky = out->missBelowGround = out->missAbove = out->missAboveSky = 0;
	quint64 h = 0xCBF29CE484222325ULL;
	for ( qint64 p = 0; p < nP; p++ ) {
		const int st = out->state[size_t( p )];
		if ( st == kOff || ( st == kAsleep && P.flags[1] ) )
			continue;
		for ( int i = kFixedRays; i < N; i++ ) {
			const float * rec = &rays[( size_t( p ) * size_t( N ) + size_t( i ) ) * 4];
			if ( !( rec[3] >= kMiss * 0.5f ) )
				continue;
			const V3 d = rayDir( i, N, rot );
			if ( std::fabs( d.z ) < 1e-6f )
				continue;   // on the horizon: the GPU's rotation may put it on the other side
			float sky[3];
			skyRadiance( R, d, sky );
			if ( d.z < 0.0f ) {
				out->missBelow++;
				const bool isGround = near3( rec, G );
				out->missBelowGround += isGround ? 1 : 0;
				out->missBelowSky += ( near3( rec, sky ) && !isGround ) ? 1 : 0;
			} else {
				out->missAbove++;
				out->missAboveSky += near3( rec, sky ) ? 1 : 0;
				const unsigned char * b = reinterpret_cast<const unsigned char *>( rec );
				for ( int k = 0; k < 12; k++ )
					h = ( h ^ b[k] ) * 0x100000001B3ULL;
			}
		}
	}
	out->aboveHash = h;
}

bool rtddgiRun( const ProbeSoup & soup, const WwCellLighting & lighting, const ProbeSkyLight & sky, const RtDdgiSpec & spec,
	RtDdgiVolume * out )
{
	Packed S;
	if ( !packVolume( soup, lighting, sky, spec, out, S ) )
		return false;
	const qint64 nP = out->probeCount();

	// ---- the GPU when asked and able, else the twin
	bool done = false;
	QString why = QStringLiteral( "cpu (asked)" );
	std::vector<float> lastRays;   // lane SKYBELOW1: the miss census (spec.dumpFixed runs)
	if ( spec.gpu > 0 ) {
		bakert::DdgiGpuJob job;
		job.bvh = &S.bvh;
		job.params = S.prm;
		job.triAttr = S.triAttr;
		job.lights = S.lights;
		job.rotations = S.rotations;
		job.frames = spec.frames;
		job.relocate = spec.relocate;
		job.classify = spec.classify;
		job.dumpFixed = spec.dumpFixed;
		bakert::DdgiGpuOut g;
		if ( bakert::ddgiGpuRun( job, &g ) ) {
			done = true;
			out->path = QStringLiteral( "gpu (%1)" ).arg( QString::fromStdString( g.device ) );
			out->offset.resize( size_t( nP ) * 3 );
			out->state.resize( size_t( nP ) );
			for ( qint64 p = 0; p < nP; p++ ) {
				for ( int k = 0; k < 3; k++ )
					out->offset[size_t( p ) * 3 + size_t( k )] = g.probes[size_t( p ) * 4 + size_t( k )];
				out->state[size_t( p )] = int( g.probes[size_t( p ) * 4 + 3] + 0.5f );
			}
			out->age.assign( g.age.begin(), g.age.end() );
			out->irr = std::move( g.irr );
			out->dist = std::move( g.dist );
			out->fixedDist = std::move( g.fixedDist );
			lastRays = std::move( g.raysAll );
			out->blasGrow = g.blasGrow;
			out->rays = g.rays;
			out->msBuild += g.msBuild;
			out->msReloc = g.msReloc;
			out->msFrames = g.msFrames;
		} else {
			why = QStringLiteral( "cpu (the GPU did not run: %1)" ).arg( QString::fromStdString( g.why ) );
		}
	}
	if ( !done ) {
		out->path = why;
		runTwin( S, spec, out, &lastRays );
	}
	if ( spec.dumpFixed )
		missCensus( S, spec, lastRays, out );
	for ( int p = 0; p < out->probes; p++ ) {
		const int s = out->state[size_t( p )];
		out->off += s == kOff ? 1 : 0;
		out->asleep += s == kAsleep ? 1 : 0;
		out->vigilant += s == kVigilant ? 1 : 0;
		const float * o = &out->offset[size_t( p ) * 3];
		out->moved += ( o[0] * o[0] + o[1] * o[1] + o[2] * o[2] ) > 1e-6f ? 1 : 0;
	}
	out->ok = true;
	return true;
}

// ------------------------------------------------------------------ M2: the live volume (the cell view's)
struct RtDdgiLive::Impl
{
	RtDdgiSpec spec;
	RtDdgiVolume vol;
	bakert::DdgiGpuSession * s = nullptr;
	QString why;
	bool busy = false, submitted = false;
	int updates = 0, sinceScroll = 0;
	double ms[5] = { -1, -1, -1, -1, -1 };
	double wall = 0;
	qint64 rays = 0;
	double O[2] = { 0, 0 };   // M3: the soup's local frame (packVolume's), for the params' first probe after a scroll
	std::vector<float> lights;   // lane RESTIRDI1: the packed lights the session holds
	int scrolls = 0, scrollCells = 0, scrollFresh = 0;
	qint64 freshTotal = 0;
	// RTXCASCADE2: the scene (BLAS) this volume traces: built here or taken from another volume (sceneFrom)
	quint64 sceneKey = 0;
	bool builtBlas = false;
	QString sceneWhy;
	int skipped = 0;

	//! RTXCASCADE2: one step's device order, made from an RtDdgiStep before the submit (step and stepGroup)
	struct Prep
	{
		bakert::DdgiGpuStepIn in;
		std::vector<float> rot;
		bool scroll = false;
		float origin[3] = { 0, 0, 0 };
		int fresh = 0, k = 0, cells[3] = { 0, 0, 0 };
	};
	void prepare( const RtDdgiStep & order, Prep & P )
	{
		busy = false;
		submitted = false;
		P.k = std::max( 0, std::min( order.k, 16 ) );
		P.rot.assign( size_t( std::max( P.k, 1 ) ) * 9, 0.0f );
		const bool noRot = hasRed( spec.red, "norot" );
		for ( int f = 0; f < P.k; f++ ) {
			const M3 r = noRot ? M3() : updateRotation( spec.seed, quint32( updates + f ) );
			std::memcpy( &P.rot[size_t( f ) * 9], r.m, 36 );
		}
		RtDdgiVolume & v = vol;
		bakert::DdgiGpuStepIn & in = P.in;
		in.rotations = P.rot.data();
		in.k = P.k;
		in.frame0 = quint32( updates );
		in.host = order.host;
		in.wait = order.wait;
		in.beforeSubmit = order.beforeSubmit;
		for ( int c = 0; c < 3; c++ ) {
			P.cells[c] = order.scroll[c];
			P.origin[c] = v.origin[c];
		}
		P.scroll = order.scroll[0] != 0 || order.scroll[1] != 0 || order.scroll[2] != 0 || order.resetAll;
		if ( P.scroll ) {   // rtddgimath.h: the kept probes as copy runs; the new first probe s steps on (world, then local)
			in.scroll = true;
			if ( order.resetAll )   // RTXCASCADE2: a camera jump keeps no plane
				P.fresh = v.probeCount();
			else
				P.fresh = scrollRuns( v.dims, order.scroll, [&in]( int src, int dst, int n ) {
					in.runs.push_back( quint32( src ) );
					in.runs.push_back( quint32( dst ) );
					in.runs.push_back( quint32( n ) );
				} );
			for ( int c = 0; c < 3; c++ ) {
				P.origin[c] = float( double( v.origin[c] ) + double( order.scroll[c] ) * double( v.step[c] ) );
				in.origin[c] = float( double( P.origin[c] ) - ( c < 2 ? O[c] : 0.0 ) );
			}
		}
		// RTXCASCADE2: the covered-probe skip against the finer cascade, on this volume's grid after the scroll; an order
		// without it clears a range an earlier step set (M3's volume never had one: nothing is written for it)
		in.skipSet = true;
		skipped = 0;
		if ( order.skip ) {
			double oC[3], sC[3];
			for ( int c = 0; c < 3; c++ ) {
				oC[c] = double( P.origin[c] );
				sC[c] = double( v.step[c] );
			}
			int lo[3], hi[3];
			if ( cascadeCovered( oC, sC, v.dims, order.fineOrigin, order.fineStep, order.fineDims, lo, hi ) ) {
				for ( int c = 0; c < 3; c++ ) {
					in.skipLo[c] = lo[c];
					in.skipHi[c] = hi[c];
				}
				in.skipLo[3] = 1;
				skipped = ( hi[0] - lo[0] + 1 ) * ( hi[1] - lo[1] + 1 ) * ( hi[2] - lo[2] + 1 );
			}
		}
	}
	//! after the submit went in (so: a finished submit's report, maybe this one's)
	void after( const RtDdgiStep & order, const Prep & P, const bakert::DdgiGpuStepOut & so )
	{
		submitted = so.submitted;
		if ( !order.wait )
			take( so );   // an earlier submit's report: before this scroll moves the CPU's copy
		RtDdgiVolume & v = vol;
		if ( P.scroll ) {
			// the CPU's copy of the states follows at once (kept probes moved, the rest kFresh until the device's
			// classification comes back with a finished step's probesNew)
			const int nP = v.probeCount();
			std::vector<int> st( size_t( nP ), kFresh );
			std::vector<float> off( size_t( nP ) * 3, 0.0f );
			const std::vector<quint32> & runs = P.in.runs;
			for ( size_t i = 0; i + 2 < runs.size(); i += 3 )
				for ( quint32 j = 0; j < runs[i + 2]; j++ ) {
					const size_t a = size_t( runs[i] + j ), b = size_t( runs[i + 1] + j );
					st[b] = v.state[a];
					for ( int c = 0; c < 3; c++ )
						off[b * 3 + size_t( c )] = v.offset[a * 3 + size_t( c )];
				}
			v.state.swap( st );
			v.offset.swap( off );
			for ( int c = 0; c < 3; c++ )
				v.origin[c] = P.origin[c];
			count();
			scrolls++;
			scrollCells = std::abs( P.cells[0] ) + std::abs( P.cells[1] ) + std::abs( P.cells[2] );
			scrollFresh = P.fresh;
			freshTotal += P.fresh;
			sinceScroll = 0;
		}
		if ( order.wait )
			take( so );   // this step's own report (a scroll's states replace the CPU's guess)
		updates += P.k;
		sinceScroll += P.k;
		wall = so.msWall;
		rays = so.rays;
		v.rays += so.rays;
		v.msFrames += so.msWall;
	}

	//! the census of the volume's states (and moved offsets) from vol.state / vol.offset
	void count()
	{
		RtDdgiVolume & v = vol;
		v.off = v.asleep = v.vigilant = v.moved = v.fresh = 0;
		const int nP = v.probeCount();
		for ( int p = 0; p < nP && size_t( p ) < v.state.size(); p++ ) {
			const int st = v.state[size_t( p )];
			v.off += st == kOff ? 1 : 0;
			v.asleep += st == kAsleep ? 1 : 0;
			v.vigilant += st == kVigilant ? 1 : 0;
			v.fresh += st == kFresh ? 1 : 0;
			const float * o = &v.offset[size_t( p ) * 3];
			v.moved += ( o[0] * o[0] + o[1] * o[1] + o[2] * o[2] ) > 1e-6f ? 1 : 0;
		}
	}
	//! the states and offsets a finished scroll sent back (4 a probe: offset xyz, state)
	void statesFrom( const std::vector<float> & probes )
	{
		const float * pr = probes.data();
		RtDdgiVolume & v = vol;
		const int nP = v.probeCount();
		if ( nP <= 0 || probes.size() < size_t( nP ) * 4 )
			return;
		v.offset.resize( size_t( nP ) * 3 );
		v.state.resize( size_t( nP ) );
		for ( int p = 0; p < nP; p++ ) {
			for ( int k = 0; k < 3; k++ )
				v.offset[size_t( p ) * 3 + size_t( k )] = pr[size_t( p ) * 4 + size_t( k )];
			v.state[size_t( p )] = int( pr[size_t( p ) * 4 + 3] + 0.5f );
		}
		count();
	}
	//! a finished submit's report (ms, and the states of a scroll)
	void take( const bakert::DdgiGpuStepOut & so )
	{
		if ( !so.done )
			return;
		for ( int c = 0; c < 5; c++ )
			ms[c] = so.ms[c];
		if ( so.probesNew )
			statesFrom( so.probes );
	}
};

RtDdgiLive::RtDdgiLive() : d( new Impl ) {}
RtDdgiLive::~RtDdgiLive()
{
	close();
	delete d;
}

bool RtDdgiLive::open( const ProbeSoup & soup, const WwCellLighting & lighting, const ProbeSkyLight & sky, const RtDdgiSpec & specIn,
	bool share, const RtDdgiLive * sceneFrom )
{
	close();
	d->spec = specIn;
	d->spec.frames = 0;   // the packed rotations: none, step() makes each update's own
	d->spec.gpu = 1;
	d->why.clear();
	d->sceneWhy.clear();
	d->builtBlas = false;
	d->skipped = 0;
	const bool want = sceneFrom && sceneFrom->d->s;   // RTXCASCADE2: the BLAS of an open volume on the same soup
	Packed S;
	if ( !packVolume( soup, lighting, sky, d->spec, &d->vol, S, !want ) ) {
		d->why = d->vol.error;
		return false;
	}
	/* RTXCASCADE2: what the device's scene holds (the soup in its local frame, the triangle attributes, the lights): a
	 * volume may trace another's BLAS only when this key matches (else it builds its own; sceneWhy says why) */
	{
		quint64 h = 0xCBF29CE484222325ULL;
		auto eat = [&h]( const void * p, size_t n ) {
			const unsigned char * b = static_cast<const unsigned char *>( p );
			for ( size_t i = 0; i < n; i++ )
				h = ( h ^ b[i] ) * 0x100000001B3ULL;
		};
		eat( S.O, sizeof S.O );
		const size_t nt = S.bvh.t.size(), na = S.triAttr.size(), nl = S.lights.size();
		eat( &nt, sizeof nt );
		eat( &na, sizeof na );
		eat( &nl, sizeof nl );
		for ( size_t i = 0; i < nt; i += 4099 )   // a sample of the soup and its attributes (the cell's key already holds them whole)
			eat( &S.bvh.t[i], 4 );
		if ( nt >= 9 )
			eat( &S.bvh.t[nt - 9], 36 );
		for ( size_t i = 0; i < na; i += 4093 )
			eat( &S.triAttr[i], 4 );
		if ( nl )
			eat( S.lights.data(), nl * 4 );
		d->sceneKey = h;
	}
	const bool take = want && sceneFrom->d->sceneKey == d->sceneKey;
	if ( want && !take ) {
		d->sceneWhy = QStringLiteral( "the soup, its attributes or the lights differ from the other volume's: a BLAS of its own" );
		S.bvh.build();
	}
	bakert::DdgiGpuJob job;
	job.sceneFrom = take ? sceneFrom->d->s : nullptr;
	job.bvh = &S.bvh;
	job.params = S.prm;
	job.triAttr = std::move( S.triAttr );
	d->lights = S.lights;   // lane RESTIRDI1
	job.lights = std::move( S.lights );
	job.frames = 0;
	job.relocate = d->spec.relocate;
	job.classify = d->spec.classify;
	job.share = share;
	job.scroll = true;   // M3: the cell view's volume follows the camera
	d->O[0] = S.O[0];
	d->O[1] = S.O[1];
	bakert::DdgiGpuOut g;
	d->s = bakert::ddgiGpuOpen( job, &g );
	if ( !d->s ) {
		d->why = QStringLiteral( "the GPU did not run: %1" ).arg( QString::fromStdString( g.why ) );
		return false;
	}
	d->builtBlas = g.blasBuilt;
	RtDdgiVolume & v = d->vol;
	const int nP = v.probeCount();
	v.path = QStringLiteral( "gpu (%1)" ).arg( QString::fromStdString( g.device ) );
	v.offset.resize( size_t( nP ) * 3 );
	v.state.resize( size_t( nP ) );
	for ( int p = 0; p < nP; p++ ) {
		for ( int k = 0; k < 3; k++ )
			v.offset[size_t( p ) * 3 + size_t( k )] = g.probes[size_t( p ) * 4 + size_t( k )];
		v.state[size_t( p )] = int( g.probes[size_t( p ) * 4 + 3] + 0.5f );
	}
	d->count();
	v.blasGrow = g.blasGrow;
	v.rays = g.rays;
	v.msBuild += g.msBuild;
	v.msReloc = g.msReloc;
	v.ok = true;
	d->updates = d->sinceScroll = 0;
	d->scrolls = d->scrollCells = d->scrollFresh = 0;
	d->freshTotal = 0;
	return true;
}

bool RtDdgiLive::isOpen() const { return d->s != nullptr; }

bool RtDdgiLive::step( int k )
{
	RtDdgiStep o;
	o.k = k;
	return step( o );
}

bool RtDdgiLive::step( const RtDdgiStep & order )
{
	d->busy = false;
	d->submitted = false;
	if ( !d->s ) {
		d->why = QStringLiteral( "not open" );
		return false;
	}
	Impl::Prep P;
	d->prepare( order, P );
	bakert::DdgiGpuStepOut so;
	if ( !bakert::ddgiGpuStepEx( d->s, P.in, &so ) ) {
		d->busy = so.busy;
		d->why = QString::fromStdString( so.why );
		d->take( so );   // a finished earlier submit may have been collected before the refusal
		return false;
	}
	d->after( order, P, so );
	return true;
}

bool RtDdgiLive::stepGroup( RtDdgiLive * const * lives, const RtDdgiStep * orders, int n )
{
	if ( n <= 0 )
		return false;
	std::vector<Impl::Prep> P( static_cast<size_t>( n ) );   // sized once: each in.rotations points into its own P.rot
	std::vector<bakert::DdgiGpuSession *> ss( size_t( n ), nullptr );
	std::vector<bakert::DdgiGpuStepIn> ins( static_cast<size_t>( n ) );
	std::vector<bakert::DdgiGpuStepOut> outs( static_cast<size_t>( n ) );
	for ( int i = 0; i < n; i++ ) {
		Impl * d = lives[i]->d;
		d->busy = false;
		d->submitted = false;
		if ( !d->s || !bakert::ddgiGpuSameScene( d->s, lives[0]->d->s ) ) {
			d->why = d->s ? QStringLiteral( "not on one BLAS with the group" ) : QStringLiteral( "not open" );
			return false;
		}
		d->prepare( orders[i], P[size_t( i )] );
		ss[size_t( i )] = d->s;
		ins[size_t( i )] = P[size_t( i )].in;
	}
	if ( !bakert::ddgiGpuStepGroup( ss.data(), ins.data(), outs.data(), n ) ) {
		for ( int i = 0; i < n; i++ ) {
			Impl * d = lives[i]->d;
			d->busy = outs[size_t( i )].busy;
			d->why = QString::fromStdString( outs[size_t( i )].why );
			d->take( outs[size_t( i )] );
		}
		return false;
	}
	for ( int i = 0; i < n; i++ )
		lives[i]->d->after( orders[i], P[size_t( i )], outs[size_t( i )] );
	return true;
}

bool RtDdgiLive::sameScene( const RtDdgiLive & other ) const { return bakert::ddgiGpuSameScene( d->s, other.d->s ); }
bool RtDdgiLive::builtBlas() const { return d->builtBlas; }
QString RtDdgiLive::sceneWhy() const { return d->sceneWhy; }
int RtDdgiLive::lastSkipped() const { return d->skipped; }

bool RtDdgiLive::finish()
{
	if ( !d->s )
		return false;
	bakert::DdgiGpuStepOut so;
	if ( !bakert::ddgiGpuFinish( d->s, &so ) ) {
		d->why = QString::fromStdString( so.why );
		return false;
	}
	d->take( so );
	return true;
}

void RtDdgiLive::close()
{
	if ( d->s )
		bakert::ddgiGpuClose( d->s );
	d->s = nullptr;
	d->updates = d->sinceScroll = 0;
	d->submitted = false;
}

const RtDdgiVolume & RtDdgiLive::volume() const { return d->vol; }
const RtDdgiSpec & RtDdgiLive::spec() const { return d->spec; }
QString RtDdgiLive::why() const { return d->why; }
bool RtDdgiLive::busy() const { return d->busy; }
int RtDdgiLive::updates() const { return d->updates; }
int RtDdgiLive::updatesSinceScroll() const { return d->sinceScroll; }
bool RtDdgiLive::submitted() const { return d->submitted; }
bakert::DdgiGpuSession * RtDdgiLive::session() const { return d->s; }
const std::vector<float> & RtDdgiLive::packedLights() const { return d->lights; }
double RtDdgiLive::localX() const { return d->O[0]; }
double RtDdgiLive::localY() const { return d->O[1]; }
int RtDdgiLive::scrolls() const { return d->scrolls; }
int RtDdgiLive::lastScrollCells() const { return d->scrollCells; }
int RtDdgiLive::lastScrollFresh() const { return d->scrollFresh; }
qint64 RtDdgiLive::freshTotal() const { return d->freshTotal; }
const float * RtDdgiLive::irr() const { return bakert::ddgiGpuHostIrr( d->s ); }
const float * RtDdgiLive::dist() const { return bakert::ddgiGpuHostDist( d->s ); }
const quint32 * RtDdgiLive::age() const { return reinterpret_cast<const quint32 *>( bakert::ddgiGpuHostAge( d->s ) ); }
const double * RtDdgiLive::lastMs() const { return d->ms; }
double RtDdgiLive::lastWallMs() const { return d->wall; }
qint64 RtDdgiLive::lastRays() const { return d->rays; }

void RtDdgiLive::snapshot( RtDdgiVolume * out ) const
{
	*out = d->vol;
	const int nP = d->vol.probeCount();
	const float * ir = irr(), * di = dist();
	const quint32 * ag = age();
	out->irr.assign( size_t( nP ) * 256, 0.0f );
	out->dist.assign( size_t( nP ) * 512, 0.0f );
	out->age.assign( size_t( nP ), 0u );
	if ( !ir || !di || !ag )
		return;
	std::memcpy( out->irr.data(), ir, out->irr.size() * 4 );
	std::memcpy( out->dist.data(), di, out->dist.size() * 4 );
	std::memcpy( out->age.data(), ag, out->age.size() * 4 );
}

// ------------------------------------------------------------------ RTXCASCADE2: the cascades offline (the CLI's gate)
bool rtddgiRunCascades( const ProbeSoup & soup, const WwCellLighting & lighting, const ProbeSkyLight & sky,
	const std::vector<RtDdgiSpec> & specs, bool oneBlas, bool oneSubmit, bool cskip, std::vector<RtDdgiVolume> * vols,
	RtDdgiCascadeRun * census )
{
	const int n = int( specs.size() );
	vols->assign( size_t( std::max( n, 1 ) ), RtDdgiVolume() );
	*census = RtDdgiCascadeRun();
	if ( n <= 0 ) {
		( *vols )[0].error = QStringLiteral( "no cascades" );
		return false;
	}
	const long long blas0 = bakert::ddgiGpuBlasBuilds(), sub0 = bakert::ddgiGpuSubmits();
	std::vector<std::unique_ptr<RtDdgiLive>> L;
	for ( int c = 0; c < n; c++ ) {
		L.push_back( std::make_unique<RtDdgiLive>() );
		if ( !L.back()->open( soup, lighting, sky, specs[size_t( c )], false, oneBlas && c > 0 ? L[0].get() : nullptr ) ) {
			( *vols )[0].error = QStringLiteral( "cascade %1: %2" ).arg( c ).arg( L.back()->why() );
			return false;
		}
	}
	census->path = L[0]->volume().path;
	const int frames = std::max( specs[0].frames, 0 );
	std::vector<RtDdgiLive *> lives;
	for ( auto & l : L )
		lives.push_back( l.get() );
	for ( int f = 0; f < frames || f == 0; ) {
		const int k = std::min( 16, frames - f );
		const bool last = f + k >= frames;
		std::vector<RtDdgiStep> orders( static_cast<size_t>( n ) );
		for ( int c = 0; c < n; c++ ) {
			RtDdgiStep & o = orders[size_t( c )];
			o.k = std::max( k, 0 );
			o.host = last;
			o.wait = true;
			if ( cskip && c > 0 ) {   // the next finer cascade's grid (the CLI's volumes do not scroll)
				const RtDdgiVolume & fv = L[size_t( c - 1 )]->volume();
				o.skip = true;
				for ( int a = 0; a < 3; a++ ) {
					o.fineOrigin[a] = double( fv.origin[a] );
					o.fineStep[a] = double( fv.step[a] );
					o.fineDims[a] = fv.dims[a];
				}
			}
		}
		bool ok = false;
		for ( int tries = 0; tries < 10000; tries++ ) {   // the steps never wait for the device's lock; the CLI does
			if ( oneSubmit ) {
				ok = RtDdgiLive::stepGroup( lives.data(), orders.data(), n );
			} else {
				ok = true;
				for ( int c = 0; c < n && ok; c++ )
					ok = L[size_t( c )]->step( orders[size_t( c )] );
			}
			bool busy = false;
			for ( auto & l : L )
				busy = busy || l->busy();
			if ( ok || !busy )
				break;
			std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
		}
		if ( !ok ) {
			QString why;
			for ( int c = 0; c < n; c++ )
				if ( !L[size_t( c )]->why().isEmpty() )
					why = QStringLiteral( "cascade %1: %2" ).arg( c ).arg( L[size_t( c )]->why() );
			( *vols )[0].error = why;
			return false;
		}
		if ( frames <= 0 )
			break;
		f += k;
	}
	census->blasBuilds = bakert::ddgiGpuBlasBuilds() - blas0;
	census->submits = bakert::ddgiGpuSubmits() - sub0;
	for ( int c = 0; c < n; c++ ) {
		RtDdgiVolume & v = ( *vols )[size_t( c )];
		L[size_t( c )]->snapshot( &v );
		v.ok = true;
		v.off = v.asleep = v.vigilant = v.moved = 0;
		for ( int p = 0; p < v.probes && size_t( p ) < v.state.size(); p++ ) {
			const int s = v.state[size_t( p )];
			v.off += s == kOff ? 1 : 0;
			v.asleep += s == kAsleep ? 1 : 0;
			v.vigilant += s == kVigilant ? 1 : 0;
			const float * o = &v.offset[size_t( p ) * 3];
			v.moved += ( o[0] * o[0] + o[1] * o[1] + o[2] * o[2] ) > 1e-6f ? 1 : 0;
		}
	}
	return true;
}

quint64 rtddgiLightsKey( const WwCellLighting & lighting )
{
	quint64 h = 0xCBF29CE484222325ULL;
	auto eat = [&h]( const void * p, size_t n ) {
		const unsigned char * b = static_cast<const unsigned char *>( p );
		for ( size_t i = 0; i < n; i++ )
			h = ( h ^ b[i] ) * 0x100000001B3ULL;
	};
	const double O[2] = { 0, 0 };
	float o[kLight];
	for ( const WwCellLight & l : lighting.lights ) {
		if ( l.radius <= 0.0f )
			continue;
		packLight( l, O, o );   // exactly what the volume's lights buffer gets (in the world frame)
		eat( o, sizeof o );
	}
	const int flags[2] = { lighting.interior ? 1 : 0, lighting.hasDirectional ? 1 : 0 };
	eat( flags, sizeof flags );
	eat( lighting.dirTo, sizeof lighting.dirTo );
	eat( lighting.dirColor, sizeof lighting.dirColor );
	return h;
}

void rtddgiProbeIrradiance( const RtDdgiVolume & vol, int p, const float dir[3], float out[3] )
{
	// the sampler's convention [J21 fig 4]: interpolate enc^2.5, square, x 2 pi
	float t[kIrrTile * kIrrTile * 3];
	const float * src = &vol.irr[size_t( p ) * 256];
	for ( int i = 0; i < kIrrTile * kIrrTile; i++ )
		for ( int c = 0; c < 3; c++ )
			t[i * 3 + c] = std::pow( std::max( src[i * 4 + c], 0.0f ), 2.5f );
	float e[3];
	sampleTile( t, kIrrTile, 3, normalize( V3( dir[0], dir[1], dir[2] ) ), e );
	for ( int c = 0; c < 3; c++ )
		out[c] = e[c] * e[c] * 2.0f * kPi;
}

float rtddgiProbeSkyVis( const RtDdgiVolume & vol, int p, const float dir[3] )
{
	float e[4];   // the tile's alpha, linear (no gamma)
	sampleTile( &vol.irr[size_t( p ) * 256], kIrrTile, 4, normalize( V3( dir[0], dir[1], dir[2] ) ), e );
	return e[3];
}

/* lane SKYBELOW1: the rule, the ground and the miss census in one line (the dump header and the CLI's stdout).
 * Gate: "below returning sky 0" under the rule, > 0 under the red; the above hash equal between the two */
static QByteArray skyBelowLine( const RtDdgiVolume & vol )
{
	char b[512];
	std::snprintf( b, sizeof b, "skybelow: rule %s; ground %.6g %.6g %.6g (albedo %.4f %.4f %.4f); misses below %lld (returning sky %lld, "
		"ground %lld), above %lld (sky %lld), above hash %016llx", vol.skyBelowRule ? "on" : "OFF (red)", double( vol.ground[0] ),
		double( vol.ground[1] ), double( vol.ground[2] ), double( vol.groundAlbedo[0] ), double( vol.groundAlbedo[1] ),
		double( vol.groundAlbedo[2] ), (long long)vol.missBelow, (long long)vol.missBelowSky, (long long)vol.missBelowGround,
		(long long)vol.missAbove, (long long)vol.missAboveSky, (unsigned long long)vol.aboveHash );
	return QByteArray( b );
}

/* lane RTEMIS1: the emission at the hits in one line (CLI stdout): the pack census (both paths), then the twin's hit census
 * (hits per update, the share of the shaded hits' radiance that is Le); the GPU keeps no per-hit count */
static QByteArray emissiveLine( const RtDdgiVolume & vol, const RtDdgiSpec & spec )
{
	char b[512];
	const int n = std::snprintf( b, sizeof b, "emissive: %d triangles, area %.1f u2, power %.6g (pi x lum Le x area); ", vol.emisTris,
		vol.emisArea, vol.emisPower );
	if ( vol.emisHits < 0 )
		std::snprintf( b + n, sizeof b - size_t( n ), "hits not counted on this path" );
	else
		std::snprintf( b + n, sizeof b - size_t( n ), "hits %lld of %lld shaded rays (%.1f an update), Le %.6g of hit radiance %.6g (%.3f %%)",
			(long long)vol.emisHits, (long long)vol.emisRays, double( vol.emisHits ) / double( std::max( spec.frames, 1 ) ),
			vol.emisLum, vol.allLum, vol.allLum > 0.0 ? 100.0 * vol.emisLum / vol.allLum : 0.0 );
	return QByteArray( b );
}

bool rtddgiDump( const RtDdgiVolume & vol, const RtDdgiSpec & spec, const QString & dir, QString * err )
{
	if ( !QDir().mkpath( dir ) ) {
		*err = QStringLiteral( "cannot make %1" ).arg( dir );
		return false;
	}
	const QDir d( dir );
	const int nP = vol.probeCount();
	std::FILE * f = std::fopen( QFile::encodeName( d.filePath( QStringLiteral( "rtddgi_probes.tsv" ) ) ).constData(), "wb" );
	std::FILE * q = std::fopen( QFile::encodeName( d.filePath( QStringLiteral( "rtddgi_points.txt" ) ) ).constData(), "wb" );
	if ( !f || !q ) {
		if ( f ) std::fclose( f );
		if ( q ) std::fclose( q );
		*err = QStringLiteral( "cannot write in %1" ).arg( dir );
		return false;
	}
	std::fprintf( f, "# rtddgi M1: path %s\n", qPrintable( vol.path ) );
	std::fprintf( f, "# grid %d x %d x %d step %.3f %.3f %.3f origin %.3f %.3f %.3f distClamp %.3f\n", vol.dims[0], vol.dims[1],
		vol.dims[2], double( vol.step[0] ), double( vol.step[1] ), double( vol.step[2] ), double( vol.origin[0] ),
		double( vol.origin[1] ), double( vol.origin[2] ), double( vol.distClamp ) );
	std::fprintf( f, "# spec rays %d frames %d hysteresis %.4f sharpness %.2f bias %.3f seed %u oneBounce %d noSky %d skipAsleep %d "
		"relocate %d classify %d red '%s'\n", spec.rays, spec.frames, double( spec.hysteresis ), double( spec.sharpness ),
		double( spec.bias ), unsigned( spec.seed ), spec.oneBounce ? 1 : 0, spec.noSky ? 1 : 0, spec.skipAsleep ? 1 : 0,
		spec.relocate ? 1 : 0, spec.classify ? 1 : 0, qPrintable( spec.red ) );
	std::fprintf( f, "# probes %d off %d asleep %d vigilant %d moved %d rays %lld ms build %.1f reloc %.1f frames %.1f skyClamped %d "
		"blasGrow %.4f\n", vol.probes, vol.off, vol.asleep, vol.vigilant, vol.moved, (long long)vol.rays, vol.msBuild, vol.msReloc,
		vol.msFrames, vol.skyClamped, vol.blasGrow );
	std::fprintf( f, "# %s\n", skyBelowLine( vol ).constData() );   // lane SKYBELOW1
	std::fprintf( f, "# index\tix\tiy\tiz\tx\ty\tz\tstate\tage\tE+X r g b\tE-X\tE+Y\tE-Y\tE+Z\tE-Z (decoded x 2 pi)"
		"\tV+X\tV-X\tV+Y\tV-Y\tV+Z\tV-Z (sky visibility)\n" );
	static const float kAx[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	for ( int p = 0; p < nP; p++ ) {
		float pos[3];
		vol.position( p, pos );
		const int ix = p % vol.dims[0], iy = ( p / vol.dims[0] ) % vol.dims[1], iz = p / ( vol.dims[0] * vol.dims[1] );
		const quint32 ag = vol.age.size() > size_t( p ) ? vol.age[size_t( p )] : 0;
		std::fprintf( f, "%d\t%d\t%d\t%d\t%.3f\t%.3f\t%.3f\t%d\t%u", p, ix, iy, iz, double( pos[0] ), double( pos[1] ), double( pos[2] ),
			vol.state[size_t( p )], unsigned( ag ) );
		for ( int a = 0; a < 6; a++ ) {
			float e[3];
			rtddgiProbeIrradiance( vol, p, kAx[a], e );
			std::fprintf( f, "\t%.6f\t%.6f\t%.6f", double( e[0] ), double( e[1] ), double( e[2] ) );
		}
		for ( int a = 0; a < 6; a++ )
			std::fprintf( f, "\t%.6f", double( rtddgiProbeSkyVis( vol, p, kAx[a] ) ) );
		std::fprintf( f, "\n" );
		if ( vol.state[size_t( p )] != kOff && ag > 0 )
			for ( int a = 0; a < 6; a++ )
				std::fprintf( q, "%.4f %.4f %.4f %g %g %g\n", double( pos[0] ), double( pos[1] ), double( pos[2] ), double( kAx[a][0] ),
					double( kAx[a][1] ), double( kAx[a][2] ) );
	}
	std::fclose( f );
	std::fclose( q );
	auto bin = [&]( const char * name, int S, const std::vector<float> & v ) -> bool {
		std::FILE * b = std::fopen( QFile::encodeName( d.filePath( QString::fromLatin1( name ) ) ).constData(), "wb" );
		if ( !b )
			return false;
		const qint32 h[2] = { nP, S };
		std::fwrite( h, 4, 2, b );
		std::fwrite( v.data(), 4, v.size(), b );
		std::fclose( b );
		return true;
	};
	if ( !bin( "rtddgi_irr.bin", kIrrTile, vol.irr ) || !bin( "rtddgi_dist.bin", kDistTile, vol.dist ) ) {
		*err = QStringLiteral( "cannot write the tiles in %1" ).arg( dir );
		return false;
	}
	if ( spec.dumpFixed && vol.fixedDist.size() >= size_t( nP ) * kFixedRays ) {   // M1FIX: the twin-vs-GPU ray check
		std::FILE * x = std::fopen( QFile::encodeName( d.filePath( QStringLiteral( "rtddgi_fixed.tsv" ) ) ).constData(), "wb" );
		if ( !x ) {
			*err = QStringLiteral( "cannot write rtddgi_fixed.tsv in %1" ).arg( dir );
			return false;
		}
		std::fprintf( x, "# rtddgi M1FIX fixed rays: path %s; index, state, then the 32 stored distances (1e27 miss, < 0 backface "
			"= -0.2 t)\n", qPrintable( vol.path ) );
		for ( int p = 0; p < nP; p++ ) {
			std::fprintf( x, "%d\t%d", p, vol.state[size_t( p )] );
			for ( int i = 0; i < kFixedRays; i++ )
				std::fprintf( x, "\t%.9g", double( vol.fixedDist[size_t( p ) * kFixedRays + size_t( i )] ) );
			std::fprintf( x, "\n" );
		}
		std::fclose( x );
	}
	return true;
}

int rtddgiCli( const QStringList & args )
{
	QString soupPath, outDir, rect, lightsFile;
	RtDdgiSpec spec;
	int cascades = 1, crays = 0;   // RTXCASCADE2
	bool perBlas = false, cskip = false;
	bool forceInterior = false;   // lane SKYBELOW1
	spec.gpu = 0;
	WwCellLighting L;
	L.interior = true;
	ProbeSkyLight sky;
	for ( int i = 0; i < args.size(); i++ ) {
		const QString & a = args[i];
		const QString nx = i + 1 < args.size() ? args[i + 1] : QString();
		if ( a == QLatin1String( "--soup" ) ) { soupPath = nx; i++; }
		else if ( a == QLatin1String( "--out" ) ) { outDir = nx; i++; }
		else if ( a == QLatin1String( "--rect" ) ) { rect = nx; i++; }
		else if ( a == QLatin1String( "--spacing" ) ) { spec.spacing = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--zscale" ) ) { spec.zScale = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--rays" ) ) { spec.rays = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--frames" ) ) { spec.frames = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--hyst" ) ) { spec.hysteresis = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--bias" ) ) { spec.bias = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--seed" ) ) { spec.seed = nx.toUInt(); i++; }
		else if ( a == QLatin1String( "--max-probes" ) ) { spec.maxProbes = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--gpu" ) ) { spec.gpu = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--red" ) ) { spec.red = nx; i++; }
		else if ( a == QLatin1String( "--cascades" ) ) { cascades = std::clamp( nx.toInt(), 1, rtddgi::kMaxCascades ); i++; }
		else if ( a == QLatin1String( "--cascade-red" ) ) { perBlas = nx.split( ',' ).contains( QStringLiteral( "perblas" ) ); i++; }
		else if ( a == QLatin1String( "--cskip" ) ) { cskip = nx.toInt() != 0; i++; }
		else if ( a == QLatin1String( "--crays" ) ) { crays = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--lights" ) ) { lightsFile = nx; i++; }
		else if ( a == QLatin1String( "--one-bounce" ) ) spec.oneBounce = true;
		else if ( a == QLatin1String( "--no-sky" ) ) spec.noSky = true;
		else if ( a == QLatin1String( "--bounce-sat" ) ) { spec.bounceSat = std::clamp( nx.toFloat(), 0.0f, 1.0f ); i++; }   // GIFIX1101 1B
		else if ( a == QLatin1String( "--no-place" ) ) spec.place = false;
		else if ( a == QLatin1String( "--no-reloc" ) ) spec.relocate = false;
		else if ( a == QLatin1String( "--no-classify" ) ) spec.classify = false;
		else if ( a == QLatin1String( "--trace-asleep" ) ) spec.skipAsleep = false;
		else if ( a == QLatin1String( "--dump-fixed" ) ) spec.dumpFixed = true;
		else if ( a == QLatin1String( "--interior" ) ) forceInterior = true;   // SKYBELOW1: a sky-lit interior (SKYINT1's kind)
		else if ( a == QLatin1String( "--sun" ) || a == QLatin1String( "--sky" ) ) {   // probegi's arguments
			const QStringList v = nx.split( ',' );
			const bool sun = a == QLatin1String( "--sun" );
			if ( sun && v.size() == 6 ) {
				const double l = std::sqrt( double( v[0].toFloat() ) * v[0].toFloat() + double( v[1].toFloat() ) * v[1].toFloat()
					+ double( v[2].toFloat() ) * v[2].toFloat() );
				for ( int c = 0; c < 3 && l > 0.0; c++ ) {
					sky.sunTo[c] = float( v[c].toFloat() / l );
					sky.sun[c] = v[3 + c].toFloat();
				}
			} else if ( !sun && ( v.size() == 3 || v.size() == 18 ) ) {
				for ( int f = 0; f < 6; f++ )
					for ( int c = 0; c < 3; c++ )
						sky.amb[f][c] = v[v.size() == 3 ? c : f * 3 + c].toFloat();
			}
			sky.on = true;
			sky.label = QStringLiteral( "cli" );
			L.interior = false;
			i++;
		}
		else if ( a == QLatin1String( "--light" ) ) {
			const QStringList v = nx.split( ',' );
			if ( v.size() == 7 ) {
				WwCellLight l;
				for ( int c = 0; c < 3; c++ ) {
					l.pos[c] = v[c].toFloat();
					l.color[c] = v[4 + c].toFloat();
				}
				l.radius = v[3].toFloat();
				L.lights.push_back( l );
			}
			i++;
		}
	}
	if ( forceInterior )   // lane SKYBELOW1: --sky/--sun with --interior = an interior that shows the sky (its ground is 0)
		L.interior = true;
	if ( !lightsFile.isEmpty() ) {   // tests/bounce_reference's lights.txt: "x y z radius r g b" a line, the default falloff
		QFile lf( lightsFile );
		if ( !lf.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
			std::fprintf( stderr, "rtddgi: cannot read %s\n", qPrintable( lightsFile ) );
			return 1;
		}
		while ( !lf.atEnd() ) {
			const QStringList v = QString::fromLatin1( lf.readLine() ).simplified().split( ' ', Qt::SkipEmptyParts );
			if ( v.size() != 7 || v[0].startsWith( '#' ) )
				continue;
			WwCellLight l;
			for ( int c = 0; c < 3; c++ ) {
				l.pos[c] = v[c].toFloat();
				l.color[c] = v[4 + c].toFloat();
			}
			l.radius = v[3].toFloat();
			L.lights.push_back( l );
		}
	}
	const QStringList rc = rect.split( ',' );
	if ( soupPath.isEmpty() || outDir.isEmpty() || rc.size() != 4 || ( L.lights.isEmpty() && !sky.on ) ) {
		std::fprintf( stderr, "usage: rtddgi --soup <file> --rect minX,minY,maxX,maxY --out <dir> [--light x,y,z,radius,r,g,b ...] "
			"[--lights <file>] [--sun tx,ty,tz,r,g,b] [--sky r,g,b | 18 values] [--spacing s] [--zscale k] [--rays n] [--frames n] "
			"[--hyst h] [--bias b] [--seed n] [--max-probes n] [--one-bounce] [--no-sky] [--gpu 0|1] [--no-place] [--no-reloc] "
			"[--no-classify] [--trace-asleep] [--dump-fixed] [--red noback,nocheb,nobias,norot,hwhit,notie,skybelow,bouncenocover,noemissive,emicentre]\n" );
		return 2;
	}
	for ( int k = 0; k < 4; k++ )
		spec.rect[k] = rc[k].toFloat();
	ProbeSoup soup;
	QString err;
	if ( !probeSoupRead( soupPath, &soup, &err ) ) {
		std::fprintf( stderr, "rtddgi: %s\n", qPrintable( err ) );
		return 1;
	}
	if ( cascades > 1 ) {   // RTXCASCADE2: the cascades about the rect's centre, cascade c at spacing x 2^c over a rect 2^c as wide
		std::vector<RtDdgiSpec> specs;
		const double cx = 0.5 * ( double( spec.rect[0] ) + double( spec.rect[2] ) ), cy = 0.5 * ( double( spec.rect[1] ) + double( spec.rect[3] ) );
		const double hx = 0.5 * ( double( spec.rect[2] ) - double( spec.rect[0] ) ), hy = 0.5 * ( double( spec.rect[3] ) - double( spec.rect[1] ) );
		for ( int c = 0; c < cascades; c++ ) {
			RtDdgiSpec s = spec;
			const double k = rtddgi::cascadeScale( c );
			s.spacing = float( double( spec.spacing ) * k );
			s.rays = rtddgi::cascadeRays( spec.rays, crays, c );
			s.rect[0] = float( cx - hx * k );
			s.rect[1] = float( cy - hy * k );
			s.rect[2] = float( cx + hx * k );
			s.rect[3] = float( cy + hy * k );
			specs.push_back( s );
		}
		std::vector<RtDdgiVolume> vols;
		RtDdgiCascadeRun cr;
		if ( !rtddgiRunCascades( soup, L, sky, specs, !perBlas, !perBlas, cskip, &vols, &cr ) ) {
			std::fprintf( stderr, "rtddgi: %s\n", qPrintable( vols.empty() ? QString() : vols[0].error ) );
			return 1;
		}
		std::printf( "rtddgi cascades: %s; %d cascades, BLAS builds %lld, submits %lld (%s)\n", qPrintable( cr.path ), cascades,
			cr.blasBuilds, cr.submits, perBlas ? "the perblas red: a BLAS and a submit a cascade" : "one BLAS, one submit a chunk of updates" );
		for ( int c = 0; c < cascades; c++ ) {
			const RtDdgiVolume & v = vols[size_t( c )];
			const QString dir = outDir + QStringLiteral( "/cascade%1" ).arg( c );
			if ( !rtddgiDump( v, specs[size_t( c )], dir, &err ) ) {
				std::fprintf( stderr, "rtddgi: %s\n", qPrintable( err ) );
				return 1;
			}
			std::printf( "rtddgi cascade %d: spacing %.1f, rays %d, grid %d x %d x %d (%d probes: off %d, asleep %d, vigilant %d, moved %d); "
				"rays traced %lld\n", c, double( specs[size_t( c )].spacing ), specs[size_t( c )].rays, v.dims[0], v.dims[1], v.dims[2],
				v.probes, v.off, v.asleep, v.vigilant, v.moved, (long long)v.rays );
		}
		return 0;
	}
	RtDdgiVolume vol;
	if ( !rtddgiRun( soup, L, sky, spec, &vol ) ) {
		std::fprintf( stderr, "rtddgi: %s\n", qPrintable( vol.error ) );
		return 1;
	}
	if ( !rtddgiDump( vol, spec, outDir, &err ) ) {
		std::fprintf( stderr, "rtddgi: %s\n", qPrintable( err ) );
		return 1;
	}
	std::printf( "rtddgi: %s; grid %d x %d x %d (%d probes: off %d, asleep %d, vigilant %d, moved %d); rays %lld; "
		"ms build %.1f, relocate+classify %.1f, %d updates %.1f; BLAS grown %.4f u (re-tested on the true triangles)\n",
		qPrintable( vol.path ), vol.dims[0], vol.dims[1], vol.dims[2], vol.probes, vol.off, vol.asleep, vol.vigilant, vol.moved,
		(long long)vol.rays, vol.msBuild, vol.msReloc, spec.frames, vol.msFrames, vol.blasGrow );
	std::printf( "%s\n", skyBelowLine( vol ).constData() );   // lane SKYBELOW1
	std::printf( "%s\n", emissiveLine( vol, spec ).constData() );   // lane RTEMIS1
	return 0;
}

// ------------------------------------------------------------------ lane RESTIRDI1: ReSTIR DI, twin vs GPU
int restirCli( const QStringList & args )
{
	QString soupPath, outDir, rect, lightsFile, redText;
	RtDdgiSpec spec;
	spec.gpu = 1;
	spec.place = false;
	spec.fitProbes = true;
	spec.maxProbes = 4096;
	spec.rays = 64;
	WwCellLighting L;
	L.interior = true;
	ProbeSkyLight sky;
	float cam[6] = { 0, 0, 0, 0, 1, 0 };
	bool haveCam = false;
	int W = 160, H = 96, frames = 4, only = -1, gpu = 1;
	float fov = 55.0f;
	bakert::RestirGpuIn base;
	auto addLight = [&L]( const QStringList & v ) {
		WwCellLight l;
		for ( int c = 0; c < 3; c++ ) {
			l.pos[c] = v[c].toFloat();
			l.color[c] = v[4 + c].toFloat();
		}
		l.radius = v[3].toFloat();
		l.shadow = 3;   // every CLI light casts shadows (an omni)
		L.lights.push_back( l );
	};
	for ( int i = 0; i < args.size(); i++ ) {
		const QString & a = args[i];
		const QString nx = i + 1 < args.size() ? args[i + 1] : QString();
		if ( a == QLatin1String( "--soup" ) ) { soupPath = nx; i++; }
		else if ( a == QLatin1String( "--out" ) ) { outDir = nx; i++; }
		else if ( a == QLatin1String( "--rect" ) ) { rect = nx; i++; }
		else if ( a == QLatin1String( "--lights" ) ) { lightsFile = nx; i++; }
		else if ( a == QLatin1String( "--red" ) ) { redText = nx; i++; }
		else if ( a == QLatin1String( "--frames" ) ) { frames = std::max( 1, nx.toInt() ); i++; }
		else if ( a == QLatin1String( "--only" ) ) { only = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--gpu" ) ) { gpu = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--fov" ) ) { fov = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--candidates" ) ) { base.candidates = std::max( 1, nx.toInt() ); i++; }
		else if ( a == QLatin1String( "--spatial" ) ) { base.spatial = std::clamp( nx.toInt(), 0, 16 ); i++; }
		else if ( a == QLatin1String( "--size" ) ) {
			const QStringList v = nx.split( ',' );
			if ( v.size() == 2 ) {
				W = std::max( 8, v[0].toInt() );
				H = std::max( 8, v[1].toInt() );
			}
			i++;
		} else if ( a == QLatin1String( "--cam" ) ) {
			const QStringList v = nx.split( ',' );
			if ( v.size() == 6 ) {
				for ( int k = 0; k < 6; k++ )
					cam[k] = v[k].toFloat();
				haveCam = true;
			}
			i++;
		} else if ( a == QLatin1String( "--light" ) ) {
			const QStringList v = nx.split( ',' );
			if ( v.size() == 7 )
				addLight( v );
			i++;
		}
	}
	if ( !lightsFile.isEmpty() ) {
		QFile lf( lightsFile );
		if ( lf.open( QIODevice::ReadOnly | QIODevice::Text ) )
			while ( !lf.atEnd() ) {
				const QStringList v = QString::fromLatin1( lf.readLine() ).simplified().split( ' ', Qt::SkipEmptyParts );
				if ( v.size() == 7 && !v[0].startsWith( '#' ) )
					addLight( v );
			}
	}
	const QStringList rc = rect.split( ',' );
	if ( soupPath.isEmpty() || outDir.isEmpty() || rc.size() != 4 || L.lights.isEmpty() || !haveCam ) {
		std::fprintf( stderr, "usage: restir --soup <file> --rect minX,minY,maxX,maxY --out <dir> --cam ex,ey,ez,tx,ty,tz "
			"--light x,y,z,radius,r,g,b [...] [--lights <file>] [--size W,H] [--fov deg] [--frames n] [--only i] [--candidates C] "
			"[--spatial K] [--gpu 0|1] [--red novis,nopdf,notemporal,noclamp,nohole]\n" );
		return 2;
	}
	for ( int k = 0; k < 4; k++ )
		spec.rect[k] = rc[k].toFloat();
	int red = 0;
	for ( const QString & r : redText.split( ',', Qt::SkipEmptyParts ) ) {
		const QString t = r.trimmed();
		red |= t == QLatin1String( "novis" ) ? restir::kRedNoVis : t == QLatin1String( "nopdf" ) ? restir::kRedNoPdf
			: t == QLatin1String( "notemporal" ) ? restir::kRedNoTemporal : t == QLatin1String( "noclamp" ) ? restir::kRedNoClamp
			: t == QLatin1String( "nohole" ) ? restir::kRedNoHole : 0;
	}
	ProbeSoup soup;
	QString err;
	if ( !probeSoupRead( soupPath, &soup, &err ) ) {
		std::fprintf( stderr, "restir: %s\n", qPrintable( err ) );
		return 1;
	}
	RtDdgiVolume vol;
	Packed S;
	if ( !packVolume( soup, L, sky, spec, &vol, S ) ) {
		std::fprintf( stderr, "restir: %s\n", qPrintable( vol.error ) );
		return 1;
	}
	// the camera in the soup's local frame; the g-buffer traced on the CPU (view normal, linear depth, bottom row first)
	const V3 eye( float( cam[0] - S.O[0] ), float( cam[1] - S.O[1] ), cam[2] );
	const V3 at( float( cam[3] - S.O[0] ), float( cam[4] - S.O[1] ), cam[5] );
	const V3 f = normalize( at - eye );
	const V3 rt = normalize( cross( f, std::fabs( f.z ) > 0.99f ? V3( 0, 1, 0 ) : V3( 0, 0, 1 ) ) );
	const V3 up = cross( rt, f );
	restir::Cam C;
	const float rows[12] = { rt.x, up.x, -f.x, eye.x, rt.y, up.y, -f.y, eye.y, rt.z, up.z, -f.z, eye.z };
	std::copy( rows, rows + 12, C.rows );
	const float fy = 1.0f / std::tan( 0.5f * fov * kPi / 180.0f );
	C.proj[0] = fy * float( H ) / float( W );
	C.proj[1] = fy;
	C.proj[2] = C.proj[3] = 0.0f;
	const size_t n = size_t( W ) * size_t( H );
	std::vector<float> gbuf( n * 4, 0.0f );
	int hits = 0;
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ ) {
			float * g = &gbuf[( size_t( y ) * size_t( W ) + size_t( x ) ) * 4];
			g[3] = 1e6f;
			const float nxv = ( float( x ) + 0.5f ) / float( W ) * 2.0f - 1.0f, nyv = ( float( y ) + 0.5f ) / float( H ) * 2.0f - 1.0f;
			const V3 dv( nxv / C.proj[0], nyv / C.proj[1], -1.0f );
			const V3 dw = C.dirToWorld( dv );
			const float len = length( dw );
			const double od[3] = { eye.x, eye.y, eye.z }, dd[3] = { dw.x / len, dw.y / len, dw.z / len };
			double t = 0.0;
			int tri = -1;
			if ( !S.bvh.ray( od, dd, 1e6, &t, &tri ) || tri < 0 )
				continue;
			const float * v = &S.bvh.t[size_t( tri ) * 9];
			V3 nf = normalize( cross( V3( v[3] - v[0], v[4] - v[1], v[5] - v[2] ), V3( v[6] - v[0], v[7] - v[1], v[8] - v[2] ) ) );
			if ( dot( nf, dw ) > 0.0f )
				nf = -nf;   // the side the camera sees
			g[0] = dot( nf, rt );
			g[1] = dot( nf, up );
			g[2] = -dot( nf, f );
			g[3] = float( t ) / len;   // view z of the hit = -t / |dv|
			hits++;
		}
	std::vector<int> cand;
	const int nL = S.nLights;
	for ( int i = 0; i < nL; i++ )
		if ( S.lights[size_t( i ) * kLight + 30] > 0.5f && ( only < 0 || i == only ) )
			cand.push_back( i );
	// the twin (probebvh with the soup's alpha masks; NOHOLE is the GPU's red: the twin keeps the holes)
	restir::Buffers B;
	restir::FrameIn F;
	F.W = W;
	F.H = H;
	F.gbuf = gbuf.data();
	F.cam = C;
	F.prev = C;
	F.lights = S.lights.data();
	F.nLights = nL;
	F.cand = cand.data();
	F.nCand = int( cand.size() );
	F.red = red & ~restir::kRedNoHole;
	F.set.candidates = base.candidates;
	F.set.spatial = base.spatial;
	auto vis = [&S]( V3 o, V3 d, float tmax ) {
		if ( tmax <= 1e-3f )
			return false;
		const double od[3] = { o.x, o.y, o.z }, dd[3] = { d.x, d.y, d.z };
		return S.bvh.anyHitIn( od, dd, 1e-4, double( tmax ), []( int ) { return true; } );
	};
	// the GPU: the volume's session (its BVH and packed lights), the same frames
	RtDdgiLive live;
	bool gpuOk = false;
	QString gpuWhy = QStringLiteral( "not asked (--gpu 0)" );
	if ( gpu ) {
		RtDdgiSpec gs = spec;
		gs.relocate = false;
		gs.classify = false;
		gpuOk = live.open( soup, L, sky, gs, false );
		gpuWhy = gpuOk ? QString() : live.why();
	}
	QDir().mkpath( outDir );
	std::printf( "restir: %d x %d, %d of %d pixels hit, %d candidate lights of %d, %d frames, red %d; twin + %s\n", W, H, hits, int( n ),
		int( cand.size() ), nL, frames, red, gpuOk ? "gpu" : qPrintable( QStringLiteral( "no gpu (%1)" ).arg( gpuWhy ) ) );
	bakert::RestirGpuOut go;
	for ( int fr = 0; fr < frames; fr++ ) {
		F.frame = uint32_t( fr );
		F.prevGbuf = fr ? gbuf.data() : nullptr;
		QElapsedTimer tm;
		tm.start();
		restir::frame( F, B, vis );
		const double cpuMs = double( tm.nsecsElapsed() ) / 1e6;
		if ( !gpuOk ) {
			std::printf( "frame %d: twin %.1f ms\n", fr, cpuMs );
			continue;
		}
		bakert::RestirGpuIn in = base;
		in.W = W;
		in.H = H;
		in.frame = uint32_t( fr );
		in.red = red;
		std::copy( C.rows, C.rows + 12, in.rows );
		std::copy( C.proj, C.proj + 4, in.proj );
		std::copy( C.rows, C.rows + 12, in.prows );
		std::copy( C.proj, C.proj + 4, in.pproj );
		in.history = fr > 0;
		in.cand = cand;
		in.gbuf = gbuf.data();
		in.readBack = true;
		if ( !bakert::restirGpuStep( live.session(), in, &go ) ) {
			std::printf( "frame %d: the GPU refused (%s)\n", fr, go.why.c_str() );
			gpuOk = false;
			continue;
		}
		// the differences: the light kept, the shadow ray's answer where both kept the same light (the same ray), the ratio
		int valid = 0, lightDiff = 0, sameRay = 0, blockDiff = 0;
		double ratioMax = 0, ratioSum = 0, cpuMean = 0, gpuMean = 0;
		for ( size_t i = 0; i < n; i++ ) {
			if ( !( gbuf[i * 4 + 3] < restir::kNone ) )
				continue;
			valid++;
			const int yc = B.hist[i].y, yg = int( go.res[i * 4] );
			lightDiff += yc != yg ? 1 : 0;
			const int bc = B.blocked[i], bg = int( go.numDen[i * 4 + 2] + 0.5f );
			if ( yc == yg && bc != 2 && bg != 2 ) {
				sameRay++;
				blockDiff += bc != bg ? 1 : 0;
			}
			const double d = std::fabs( double( B.rat[i] ) - go.ratio[i * 2] );
			ratioMax = std::max( ratioMax, d );
			ratioSum += d;
			cpuMean += B.rat[i];
			gpuMean += go.ratio[i * 2];
		}
		const double vd = std::max( valid, 1 );
		std::printf( "frame %d: %d pixels; light kept differs %d (%.3f %%); same light %d, shadow ray hit/miss differs %d; ratio |twin - gpu| "
			"mean %.5f max %.4f; mean ratio twin %.4f gpu %.4f; twin %.1f ms, gpu wall %.1f ms\n", fr, valid, lightDiff, 100.0 * lightDiff / vd,
			sameRay, blockDiff, ratioSum / vd, ratioMax, cpuMean / vd, gpuMean / vd, cpuMs, go.msWall );
		std::fflush( stdout );   // a hung GPU step must not hide the frames before it
	}
	QFile o( outDir + QStringLiteral( "/restir_frame.tsv" ) );
	if ( o.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
		o.write( "x\ty\tdepth\tlight_twin\tblocked_twin\tratio_twin\tlight_gpu\tblocked_gpu\tratio_gpu\n" );
		for ( int y = 0; y < H; y++ )
			for ( int x = 0; x < W; x++ ) {
				const size_t i = size_t( y ) * size_t( W ) + size_t( x );
				const bool g = gpuOk && go.res.size() == n * 4;
				o.write( QStringLiteral( "%1\t%2\t%3\t%4\t%5\t%6\t%7\t%8\t%9\n" ).arg( x ).arg( y ).arg( gbuf[i * 4 + 3] ).arg( B.hist[i].y )
					.arg( int( B.blocked[i] ) ).arg( B.rat[i] ).arg( g ? int( go.res[i * 4] ) : -9 ).arg( g ? int( go.numDen[i * 4 + 2] + 0.5f ) : -9 )
					.arg( g ? go.ratio[i * 2] : -1.0f ).toLatin1() );
			}
	}
	return 0;
}

/* RTEMIS1 part 2: ReSTIR GI, the CPU twin (restirgimath.h frame) against the GPU (restirgi.comp) on one soup. The volume is
 * traced on the GPU (--updates n), read back into the twin, so both shade the bounce from the same probes. Prints the
 * frame-0 bounce (Lo, hit) and every frame's estimate differences; writes restirgi_frame.tsv. */
namespace {
struct RgTwinScene
{
	const Twin & T;
	restirgi::Sample trace( V3 o, V3 d ) const   // MIRROR restirgi.comp rgTrace
	{
		const auto & P = T.S.prm;
		restirgi::Sample y;
		double t = 0.0;
		int tri = -1;
		if ( !T.nearest( o, d, P.sun[3], &t, &tri ) ) {
			float sky[3] = { 0, 0, 0 };
			if ( P.sky[0][3] > 0.5f ) {
				const float R[6][3] = { { P.sky[0][0], P.sky[0][1], P.sky[0][2] }, { P.sky[1][0], P.sky[1][1], P.sky[1][2] },
					{ P.sky[2][0], P.sky[2][1], P.sky[2][2] }, { P.sky[3][0], P.sky[3][1], P.sky[3][2] },
					{ P.sky[4][0], P.sky[4][1], P.sky[4][2] }, { P.sky[5][0], P.sky[5][1], P.sky[5][2] } };
				const float ground[3] = { P.sky[1][3], P.sky[2][3], P.sky[3][3] };
				missRadiance( R, ground, d, P.sky[4][3] > 0.5f, sky );
			}
			y.xs = d;
			y.ns = V3( 0, 0, 0 );
			y.Lo = V3( sky[0], sky[1], sky[2] );
			return y;
		}
		const float * v = &T.S.bvh.t[size_t( tri ) * 9];
		const V3 v0( v[0], v[1], v[2] );
		const V3 nf = normalize( cross( V3( v[3], v[4], v[5] ) - v0, V3( v[6], v[7], v[8] ) - v0 ) );
		const float * a0 = &T.S.triAttr[size_t( tri ) * kAttr];
		y.hit = true;
		y.ns = dot( nf, d ) > 0.0f ? -nf : nf;
		y.xs = o + d * float( t );
		y.Lo = V3( 0, 0, 0 );
		if ( a0[3] < 0.5f && dot( nf, d ) > 0.0f && !( T.red & kRedNoBack ) )
			return y;
		V3 E = T.direct( y.xs, y.ns );
		if ( !P.flags[0] )
			E = E + T.sampleVolume( y.xs, y.ns, -d );
		const float Ec[3] = { E[0], E[1], E[2] };
		float L[4] = { 0, 0, 0, 0 };
		hitRadiance( a0, Ec, a0 + 4, L );
		y.Lo = V3( L[0], L[1], L[2] );
		return y;
	}
	bool blocked( V3 o, V3 d, float tmax ) const { return T.shadow( o, d, tmax < 0.0f ? T.S.prm.sun[3] : tmax ); }
};
} // namespace

const unsigned char * restirGiStbnMask()
{
	static std::vector<unsigned char> m;
	static bool tried = false;
	if ( !tried ) {
		tried = true;
		QFile f( QStringLiteral( ":/air/stbn_scalar_128x128x64.r8" ) );
		const size_t want = size_t( restirgi::kStbnSide ) * size_t( restirgi::kStbnSide ) * size_t( restirgi::kStbnSlices );
		if ( f.open( QIODevice::ReadOnly ) ) {
			const QByteArray b = f.readAll();
			if ( size_t( b.size() ) == want )
				m.assign( b.constData(), b.constData() + b.size() );
		}
	}
	return m.empty() ? nullptr : m.data();
}

int restirgiCli( const QStringList & args )
{
	QString soupPath, outDir, rect, lightsFile, redText;
	bool twin = true;   // --no-twin: the GPU alone (the cost at a real size, 1080p: the twin takes minutes a frame there)
	int refFrames = 64;   // the one-frame bar's reference: the mean of this many independent (no history) GPU frames
	RtDdgiSpec spec;
	spec.gpu = 1;
	spec.place = false;
	spec.fitProbes = true;
	spec.maxProbes = 4096;
	spec.rays = 64;
	WwCellLighting L;
	L.interior = true;
	ProbeSkyLight sky;
	float cam[6] = { 0, 0, 0, 0, 1, 0 };
	bool haveCam = false;
	int W = 160, H = 96, frames = 4, updates = 8;
	float fov = 55.0f;
	uint32_t seed = 0x9e3779b9u;
	int jumpAt = -1, relightAt = -1;   // a frame whose previous camera is far away / whose sceneKey changes
	bakert::RestirGiGpuIn base;
	auto addLight = [&L]( const QStringList & v ) {
		WwCellLight l;
		for ( int c = 0; c < 3; c++ ) {
			l.pos[c] = v[c].toFloat();
			l.color[c] = v[4 + c].toFloat();
		}
		l.radius = v[3].toFloat();
		l.shadow = 3;
		L.lights.push_back( l );
	};
	for ( int i = 0; i < args.size(); i++ ) {
		const QString & a = args[i];
		const QString nx = i + 1 < args.size() ? args[i + 1] : QString();
		if ( a == QLatin1String( "--soup" ) ) { soupPath = nx; i++; }
		else if ( a == QLatin1String( "--out" ) ) { outDir = nx; i++; }
		else if ( a == QLatin1String( "--rect" ) ) { rect = nx; i++; }
		else if ( a == QLatin1String( "--lights" ) ) { lightsFile = nx; i++; }
		else if ( a == QLatin1String( "--red" ) ) { redText = nx; i++; }
		else if ( a == QLatin1String( "--frames" ) ) { frames = std::max( 1, nx.toInt() ); i++; }
		else if ( a == QLatin1String( "--updates" ) ) { updates = std::max( 0, nx.toInt() ); i++; }
		else if ( a == QLatin1String( "--fov" ) ) { fov = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--seed" ) ) { seed = nx.toUInt(); i++; }
		else if ( a == QLatin1String( "--spatial" ) ) { base.spatial = std::clamp( nx.toInt(), 0, 15 ); i++; }
		else if ( a == QLatin1String( "--radius" ) ) { base.radius = std::clamp( nx.toInt(), 1, 16 ); i++; }
		else if ( a == QLatin1String( "--filter" ) ) { base.filter = std::clamp( nx.toInt(), 0, 8 ); i++; }
		else if ( a == QLatin1String( "--acc" ) ) { base.accFrames = std::max( 1.0f, nx.toFloat() ); i++; }
		else if ( a == QLatin1String( "--jump" ) ) { jumpAt = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--relight" ) ) { relightAt = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--atrous" ) ) { base.atrous = std::clamp( nx.toInt(), 0, 5 ); i++; }
		else if ( a == QLatin1String( "--ref" ) ) { refFrames = std::max( 0, nx.toInt() ); i++; }
		else if ( a == QLatin1String( "--no-twin" ) ) twin = false;
		else if ( a == QLatin1String( "--size" ) ) {
			const QStringList v = nx.split( ',' );
			if ( v.size() == 2 ) {
				W = std::max( 8, v[0].toInt() );
				H = std::max( 8, v[1].toInt() );
			}
			i++;
		} else if ( a == QLatin1String( "--cam" ) ) {
			const QStringList v = nx.split( ',' );
			if ( v.size() == 6 ) {
				for ( int k = 0; k < 6; k++ )
					cam[k] = v[k].toFloat();
				haveCam = true;
			}
			i++;
		} else if ( a == QLatin1String( "--light" ) ) {
			const QStringList v = nx.split( ',' );
			if ( v.size() == 7 )
				addLight( v );
			i++;
		}
	}
	if ( !lightsFile.isEmpty() ) {
		QFile lf( lightsFile );
		if ( lf.open( QIODevice::ReadOnly | QIODevice::Text ) )
			while ( !lf.atEnd() ) {
				const QStringList v = QString::fromLatin1( lf.readLine() ).simplified().split( ' ', Qt::SkipEmptyParts );
				if ( v.size() == 7 && !v[0].startsWith( '#' ) )
					addLight( v );
			}
	}
	const QStringList rc = rect.split( ',' );
	if ( soupPath.isEmpty() || outDir.isEmpty() || rc.size() != 4 || L.lights.isEmpty() || !haveCam ) {
		std::fprintf( stderr, "usage: restirgi --soup <file> --rect minX,minY,maxX,maxY --out <dir> --cam ex,ey,ez,tx,ty,tz "
			"--light x,y,z,radius,r,g,b [...] [--lights <file>] [--size W,H] [--fov deg] [--frames n] [--updates n] [--seed n] "
			"[--spatial K] [--radius R] [--filter f] [--acc n] [--atrous n] [--ref n] [--no-twin] [--jump frame] [--relight frame] "
			"[--red nojacobian,novis,nopdf,notemporal,white,r2,nojclamp,linear,half,quarter,fullres]\n" );
		return 2;
	}
	for ( int k = 0; k < 4; k++ )
		spec.rect[k] = rc[k].toFloat();
	// every pixel traced is the default (half: one-frame cut 2.61x, under the 3x bar); half: the checkerboard, quarter: one
	// of each 2 x 2 (cost arms); fullres: the default named
	int red = 0;
	for ( const QString & r : redText.split( ',', Qt::SkipEmptyParts ) ) {
		const QString t = r.trimmed();
		if ( t == QLatin1String( "fullres" ) || t == QLatin1String( "half" ) || t == QLatin1String( "quarter" ) ) {
			red &= ~( restirgi::kTraceHalf | restirgi::kTraceQuarter );
			red |= t == QLatin1String( "quarter" ) ? restirgi::kTraceQuarter : t == QLatin1String( "half" ) ? restirgi::kTraceHalf : 0;
			continue;
		}
		red |= t == QLatin1String( "nojacobian" ) ? restirgi::kRedNoJacobian : t == QLatin1String( "novis" ) ? restirgi::kRedNoVis
			: t == QLatin1String( "nopdf" ) ? restirgi::kRedNoPdf : t == QLatin1String( "notemporal" ) ? restirgi::kRedNoTemporal
			: t == QLatin1String( "white" ) ? restirgi::kRedWhite : t == QLatin1String( "r2" ) ? restirgi::kRedR2
			: t == QLatin1String( "nojclamp" ) ? restirgi::kRedNoJClamp : t == QLatin1String( "linear" ) ? restirgi::kRedLinear : 0;
	}
	const unsigned char * mask = restirGiStbnMask();
	ProbeSoup soup;
	QString err;
	if ( !probeSoupRead( soupPath, &soup, &err ) ) {
		std::fprintf( stderr, "restirgi: %s\n", qPrintable( err ) );
		return 1;
	}
	RtDdgiVolume vol;
	Packed S;
	if ( !packVolume( soup, L, sky, spec, &vol, S ) ) {
		std::fprintf( stderr, "restirgi: %s\n", qPrintable( vol.error ) );
		return 1;
	}
	// the GPU volume, traced, then read into the twin (both shade the bounce from the same probes)
	RtDdgiLive live;
	RtDdgiSpec gs = spec;
	gs.relocate = false;
	gs.classify = false;
	if ( !live.open( soup, L, sky, gs, false ) ) {
		std::printf( "restirgi: no gpu (%s)\n", qPrintable( live.why() ) );
		return 1;
	}
	if ( updates > 0 && !live.step( updates ) ) {
		std::printf( "restirgi: the volume's updates failed (%s)\n", qPrintable( live.why() ) );
		return 1;
	}
	Twin T( S );
	RtDdgiVolume lv;
	live.snapshot( &lv );   // volume() holds no atlas (it stays on the device): the host copies the last step made
	const size_t nP = size_t( T.nP );
	if ( lv.irr.size() != T.irr.size() || lv.dist.size() != T.dist.size() || lv.offset.size() < nP * 3 || lv.state.size() < nP ) {
		std::printf( "restirgi: the GPU volume (%d probes) is not the twin's (%d)\n", int( lv.state.size() ), T.nP );
		return 1;
	}
	T.irr = lv.irr;
	T.dist = lv.dist;
	for ( size_t p = 0; p < nP; p++ ) {
		for ( int c = 0; c < 3; c++ )
			T.probes[p * 4 + size_t( c )] = lv.offset[p * 3 + size_t( c )];
		T.probes[p * 4 + 3] = float( lv.state[p] );
		T.age[p] = p < lv.age.size() ? lv.age[p] : 0u;
	}
	// the camera and the g-buffer exactly as restirCli
	const V3 eye( float( cam[0] - S.O[0] ), float( cam[1] - S.O[1] ), cam[2] );
	const V3 at( float( cam[3] - S.O[0] ), float( cam[4] - S.O[1] ), cam[5] );
	const V3 f = normalize( at - eye );
	const V3 rt = normalize( cross( f, std::fabs( f.z ) > 0.99f ? V3( 0, 1, 0 ) : V3( 0, 0, 1 ) ) );
	const V3 up = cross( rt, f );
	restir::Cam C;
	const float rows[12] = { rt.x, up.x, -f.x, eye.x, rt.y, up.y, -f.y, eye.y, rt.z, up.z, -f.z, eye.z };
	std::copy( rows, rows + 12, C.rows );
	const float fy = 1.0f / std::tan( 0.5f * fov * kPi / 180.0f );
	C.proj[0] = fy * float( H ) / float( W );
	C.proj[1] = fy;
	C.proj[2] = C.proj[3] = 0.0f;
	const size_t n = size_t( W ) * size_t( H );
	std::vector<float> gbuf( n * 4, 0.0f );
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ ) {
			float * g = &gbuf[( size_t( y ) * size_t( W ) + size_t( x ) ) * 4];
			g[3] = 1e6f;
			const float nxv = ( float( x ) + 0.5f ) / float( W ) * 2.0f - 1.0f, nyv = ( float( y ) + 0.5f ) / float( H ) * 2.0f - 1.0f;
			const V3 dv( nxv / C.proj[0], nyv / C.proj[1], -1.0f );
			const V3 dw = C.dirToWorld( dv );
			const float len = length( dw );
			const double od[3] = { eye.x, eye.y, eye.z }, dd[3] = { dw.x / len, dw.y / len, dw.z / len };
			double t = 0.0;
			int tri = -1;
			if ( !S.bvh.ray( od, dd, 1e6, &t, &tri ) || tri < 0 )
				continue;
			const float * v = &S.bvh.t[size_t( tri ) * 9];
			V3 nf = normalize( cross( V3( v[3] - v[0], v[4] - v[1], v[5] - v[2] ), V3( v[6] - v[0], v[7] - v[1], v[8] - v[2] ) ) );
			if ( dot( nf, dw ) > 0.0f )
				nf = -nf;
			g[0] = dot( nf, rt );
			g[1] = dot( nf, up );
			g[2] = -dot( nf, f );
			g[3] = float( t ) / len;
		}
	std::vector<restirgi::Pixel> px( n );
	int hits = 0;
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ ) {
			const restir::Pixel q = restir::pixelAt( gbuf.data(), W, H, C, x, y );
			restirgi::Pixel & p = px[size_t( y ) * size_t( W ) + size_t( x )];
			p.x = q.P;
			p.n = q.N;
			p.valid = q.ok;
			hits += q.ok ? 1 : 0;
		}
	const RgTwinScene scene{ T };
	restirgi::Settings set;
	set.spatialK = base.spatial;
	set.radius = base.radius;
	set.mask = mask;
	std::vector<float> jmax;
	set.jmax = &jmax;
	std::vector<restirgi::Res> hist;
	std::vector<V3> est;
	QDir().mkpath( outDir );
	const char * noise = ( red & restirgi::kRedWhite ) ? "white" : ( mask && !( red & restirgi::kRedR2 ) ) ? "stbn" : "r2";
	std::printf( "restirgi: %d x %d, %d of %d pixels hit, %d probes after %d updates, %d frames, K %d R %d, red %d, noise %s, "
		"a-trous %d, twin %d\n", W, H, hits, int( n ), T.nP, updates, frames, set.spatialK, set.radius, red, noise, base.atrous, int( twin ) );
	// frame 0's bounce on the twin (the kernel's ray: the same seed, the same direction); the gates count the traced pixels
	auto tracedAt = [&]( size_t i, int fr ) { return restirgi::traced( int( i % size_t( W ) ), int( i / size_t( W ) ), uint32_t( fr ), red ); };
	std::vector<restirgi::Sample> cand0( n );
	if ( twin )
		for ( size_t i = 0; i < n; i++ )
			if ( px[i].valid && tracedAt( i, 0 ) ) {
				const V3 d = restirgi::bounceDir( px[i].n, int( i % size_t( W ) ), int( i / size_t( W ) ), W, seed, 0u, red, mask );
				cand0[i] = scene.trace( px[i].x + px[i].n * restirgi::kEps, d );
			}
	bakert::RestirGiGpuOut go;
	std::vector<float> est0, out0;
	double wallSum = 0, gpuSum = 0, denSum = 0;
	int gpuN = 0;
	bool gpuOk = true;
	int worstFrame = -1;
	double worstMatch = 1.0;
	auto gpuIn = [&]( int fr, int r, bool history ) {
		bakert::RestirGiGpuIn in = base;
		in.W = W;
		in.H = H;
		in.frame = uint32_t( fr );
		in.seed = seed;
		in.red = r;
		in.stbn = mask;
		std::copy( C.rows, C.rows + 12, in.rows );
		std::copy( C.proj, C.proj + 4, in.proj );
		std::copy( C.rows, C.rows + 12, in.prows );
		std::copy( C.proj, C.proj + 4, in.pproj );
		in.history = history;
		in.gbuf = gbuf.data();
		in.readBack = true;
		return in;
	};
	for ( int fr = 0; fr < frames && gpuOk; fr++ ) {
		QElapsedTimer tm;
		// a cut or a light change: the twin starts over by hand, the kernel must find it on its own (else the match fails)
		if ( fr == jumpAt || fr == relightAt )
			hist.clear();
		tm.start();
		if ( twin )
			restirgi::frame( scene, px, W, H, seed, uint32_t( fr ), set, red, hist, est );
		const double cpuMs = double( tm.nsecsElapsed() ) / 1e6;
		bakert::RestirGiGpuIn in = gpuIn( fr, red, fr > 0 );
		if ( fr == jumpAt )
			in.prows[3] += 1000.0f;
		in.sceneKey = relightAt >= 0 && fr >= relightAt ? 1u : 0u;
		if ( !bakert::restirGiGpuStep( live.session(), in, &go ) || go.est.size() != n * 4 || go.cand.size() != n * 4 ) {
			std::printf( "frame %d: the GPU refused (%s)\n", fr, go.why.c_str() );
			gpuOk = false;
			break;
		}
		if ( fr > 0 && go.msGpu >= 0.0 ) {
			gpuSum += go.msGpu;
			denSum += go.msDenoise;
			gpuN++;
		}
		if ( fr == 0 ) {
			est0 = go.est;
			out0 = go.out;
		}
		wallSum += fr > 0 ? go.msWall : 0.0;
		if ( !twin ) {
			std::printf( "frame %d: gpu wall %.2f ms, gpu %.3f ms, filter + a-trous %.3f ms, stages", fr, go.msWall, go.msGpu, go.msDenoise );
			for ( double ms : go.msStage )
				if ( ms >= 0.0 )
					std::printf( " %.3f", ms );
			std::printf( "\n" );
			std::fflush( stdout );
			continue;
		}
		if ( fr == 0 ) {
			int valid = 0, hitDiff = 0, loMatch = 0;
			double loMax = 0;
			for ( size_t i = 0; i < n; i++ ) {
				if ( !px[i].valid || !tracedAt( i, 0 ) )
					continue;
				valid++;
				const bool hg = go.cand[i * 4 + 3] > 0.5f;
				hitDiff += hg != cand0[i].hit ? 1 : 0;
				const double lc = restirgi::lum( cand0[i].Lo ), lg = restir::luminance( go.cand[i * 4], go.cand[i * 4 + 1], go.cand[i * 4 + 2] );
				const double rel = std::fabs( lc - lg ) / std::max( 1e-4, std::max( lc, lg ) );
				loMatch += rel <= 1e-3 ? 1 : 0;
				loMax = std::max( loMax, rel );
			}
			const double vd = std::max( valid, 1 );
			std::printf( "bounce: %d pixels; hit/miss differs %d (%.3f %%); Lo within 1e-3 relative %.3f %%; worst %.4f\n", valid, hitDiff,
				100.0 * hitDiff / vd, 100.0 * loMatch / vd, loMax );
		}
		int valid = 0, match = 0;
		double sumC = 0, sumG = 0, sumAbs = 0;
		std::vector<std::pair<double, size_t>> worst;
		for ( size_t i = 0; i < n; i++ ) {
			if ( !px[i].valid || !tracedAt( i, fr ) )
				continue;
			valid++;
			const double ec = restirgi::lum( est[i] ), eg = restir::luminance( go.est[i * 4], go.est[i * 4 + 1], go.est[i * 4 + 2] );
			sumC += ec;
			sumG += eg;
			sumAbs += std::fabs( ec - eg );
			match += std::fabs( ec - eg ) <= 1e-2 * std::max( 1e-4, std::max( ec, eg ) ) ? 1 : 0;
			worst.push_back( { std::fabs( ec - eg ), i } );
		}
		const double vd = std::max( valid, 1 );
		const double frac = match / vd;
		if ( frac < worstMatch ) {
			worstMatch = frac;
			worstFrame = fr;
		}
		std::printf( "frame %d: estimate within 1 %% %.3f %% of pixels; mean twin %.5f gpu %.5f; mean |twin - gpu| / mean %.5f; twin %.1f ms, "
			"gpu wall %.1f ms\n", fr, 100.0 * frac, sumC / vd, sumG / vd, sumAbs / std::max( 1e-9, sumC ), cpuMs, go.msWall );
		// the diagnosis: the pixels that carry the difference (a firefly is a few pixels), what the twin held there
		const size_t top = std::min<size_t>( 4, worst.size() );
		std::partial_sort( worst.begin(), worst.begin() + std::ptrdiff_t( top ), worst.end(), []( const auto & a, const auto & b ) { return a.first > b.first; } );
		double topSum = 0;
		for ( size_t k = 0; k < top; k++ )
			topSum += worst[k].first;
		std::printf( "  frame %d: the %d worst pixels carry %.1f %% of the |difference|; brightest twin pixel %.3g\n", fr, int( top ),
			100.0 * topSum / std::max( 1e-9, sumAbs ), [&]() { double m = 0; for ( size_t i = 0; i < n; i++ ) m = std::max( m, double( restirgi::lum( est[i] ) ) ); return m; }() );
		for ( size_t k = 0; k < top && fr > 0; k++ ) {
			const size_t i = worst[k].second;
			const int x = int( i % size_t( W ) ), y = int( i / size_t( W ) );
			const restirgi::Res & h = hist[i];
			const restirgi::Sample yc = scene.trace( px[i].x + px[i].n * restirgi::kEps, restirgi::bounceDir( px[i].n, x, y, W, seed, uint32_t( fr ), red, mask ) );
			std::printf( "    (%d,%d): twin %.4g gpu %.4g; twin temporal M %.0f W %.4g Lo %.4g hit %d; largest J merged %.4g; bounce twin Lo %.4g hit %d, "
				"gpu Lo %.4g hit %d\n", x, y, double( restirgi::lum( est[i] ) ), restir::luminance( go.est[i * 4], go.est[i * 4 + 1], go.est[i * 4 + 2] ),
				double( h.M ), double( h.W ), double( restirgi::lum( h.y.Lo ) ), int( h.y.hit ), double( jmax[i] ), double( restirgi::lum( yc.Lo ) ), int( yc.hit ),
				restir::luminance( go.cand[i * 4], go.cand[i * 4 + 1], go.cand[i * 4 + 2] ), int( go.cand[i * 4 + 3] > 0.5f ) );
		}
		std::fflush( stdout );
	}
	if ( gpuOk ) {
		if ( twin )
			std::printf( "restirgi: worst frame %d, %.3f %% of pixels within 1 %%\n", worstFrame, 100.0 * worstMatch );
		std::printf( "history dropped: cuts %u, light changes %u\n", go.cuts, go.relights );
		std::printf( "gpu wall mean (frames 1..) %.3f ms, filter half %d\n", wallSum / std::max( 1, frames - 1 ), base.filter );
		if ( gpuN > 0 )
			std::printf( "gpu timestamps mean (frames 1..) %.3f ms; filter + a-trous %.3f ms (%d x %d, a-trous %d)\n", gpuSum / gpuN, denSum / gpuN,
				W, H, base.atrous );
		else
			std::printf( "gpu timestamps: none (the device has no timer)\n" );
		auto oneFrame = [&]( const std::vector<float> & ref, const char * what ) {
			double rs = 0, dOut = 0, dEst = 0;
			int v = 0;
			for ( size_t i = 0; i < n; i++ ) {
				if ( !px[i].valid )
					continue;
				v++;
				const double r = restir::luminance( ref[i * 4], ref[i * 4 + 1], ref[i * 4 + 2] );
				rs += r;
				dOut += std::fabs( restir::luminance( out0[i * 4], out0[i * 4 + 1], out0[i * 4 + 2] ) - r );
				dEst += std::fabs( restir::luminance( est0[i * 4], est0[i * 4 + 1], est0[i * 4 + 2] ) - r );
			}
			rs = std::max( rs, 1e-9 );
			std::printf( "%s: filtered frame 0 mean |diff| / mean %.4f; raw estimate %.4f; the filter cuts it %.2fx (%d pixels)\n", what, dOut / rs,
				dEst / rs, dEst / std::max( dOut, 1e-9 ), v );
		};
		// bar (1), one frame stands alone: frame 0 (no history) against the last frame's filtered mean (the old reference)
		if ( frames > 1 && go.out.size() == n * 4 && out0.size() == n * 4 )
			oneFrame( go.out, QStringLiteral( "one frame vs frame %1" ).arg( frames - 1 ).toLatin1().constData() );
		// and against the unfiltered truth: the mean of refFrames independent frames' estimates (no history, no filter; every
		// pixel traced: a half / quarter trace's raw estimate is 0 on the pixels it skipped)
		if ( refFrames > 0 && out0.size() == n * 4 ) {
			std::vector<double> acc( n * 4, 0.0 );
			bool ok = true;
			const int refRed = ( red | restirgi::kRedNoTemporal ) & ~( restirgi::kTraceHalf | restirgi::kTraceQuarter );
			for ( int k = 0; k < refFrames && ok; k++ ) {
				bakert::RestirGiGpuIn in = gpuIn( 1000 + k, refRed, false );
				in.sceneKey = go.relights > 0 ? 1u : 0u;
				bakert::RestirGiGpuOut ro;
				ok = bakert::restirGiGpuStep( live.session(), in, &ro ) && ro.est.size() == n * 4;
				for ( size_t j = 0; ok && j < n * 4; j++ )
					acc[j] += ro.est[j];
			}
			if ( ok ) {
				std::vector<float> ref( n * 4 );
				for ( size_t j = 0; j < n * 4; j++ )
					ref[j] = float( acc[j] / refFrames );
				oneFrame( ref, QStringLiteral( "one frame vs the reference (%1 independent frames)" ).arg( refFrames ).toLatin1().constData() );
			} else {
				std::printf( "one frame vs the reference: the GPU refused a reference frame\n" );
			}
		}
	}
	QFile o( outDir + QStringLiteral( "/restirgi_frame.tsv" ) );
	if ( o.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
		o.write( "x\ty\tvalid\tE_twin\tE_gpu\tLo0_twin\thit0_twin\n" );
		const bool g = gpuOk && go.est.size() == n * 4;
		for ( int y = 0; y < H; y++ )
			for ( int x = 0; x < W; x++ ) {
				const size_t i = size_t( y ) * size_t( W ) + size_t( x );
				o.write( QStringLiteral( "%1\t%2\t%3\t%4\t%5\t%6\t%7\n" ).arg( x ).arg( y ).arg( int( px[i].valid ) )
					.arg( i < est.size() ? restirgi::lum( est[i] ) : 0.0f )
					.arg( g ? restir::luminance( go.est[i * 4], go.est[i * 4 + 1], go.est[i * 4 + 2] ) : -1.0f )
					.arg( restirgi::lum( cand0[i].Lo ) ).arg( int( cand0[i].hit ) ).toLatin1() );
			}
	}
	return gpuOk ? 0 : 1;
}

/* MODULES1 RESUME 15: the air rays' gate (airrays.comp). A still camera: after F frames the GPU's running share must
 * approach the exact share sum_i w_i vis_i / sum_i w_i over every lamp reaching the froxel (w = arTarget, vis = the
 * CPU walk of the soup's BVH from the froxel to the lamp), which is what the estimator's means converge to. */
int airraysCli( const QStringList & args )
{
	QString soupPath, outDir, rect, redText;
	RtDdgiSpec spec;
	spec.gpu = 1;
	spec.place = false;
	spec.fitProbes = true;
	spec.maxProbes = 4096;
	spec.rays = 64;
	WwCellLighting L;
	L.interior = true;
	ProbeSkyLight sky;
	float cam[6] = { 0, 0, 0, 0, 1, 0 };
	bool haveCam = false;
	int X = 40, Y = 24, Z = 32, frames = 64, samples = 2, timeX = 0;
	float fov = 55.0f, nearT = 8.0f, farT = 4000.0f;
	for ( int i = 0; i < args.size(); i++ ) {
		const QString & a = args[i];
		const QString nx = i + 1 < args.size() ? args[i + 1] : QString();
		if ( a == QLatin1String( "--soup" ) ) { soupPath = nx; i++; }
		else if ( a == QLatin1String( "--out" ) ) { outDir = nx; i++; }
		else if ( a == QLatin1String( "--rect" ) ) { rect = nx; i++; }
		else if ( a == QLatin1String( "--red" ) ) { redText = nx; i++; }
		else if ( a == QLatin1String( "--frames" ) ) { frames = std::max( 1, nx.toInt() ); i++; }
		else if ( a == QLatin1String( "--samples" ) ) { samples = std::clamp( nx.toInt(), 1, 4 ); i++; }
		else if ( a == QLatin1String( "--fov" ) ) { fov = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--near" ) ) { nearT = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--far" ) ) { farT = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--time-grid" ) ) { timeX = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--grid" ) ) {
			const QStringList v = nx.split( ',' );
			if ( v.size() == 3 ) {
				X = std::max( 1, v[0].toInt() );
				Y = std::max( 1, v[1].toInt() );
				Z = std::max( 1, v[2].toInt() );
			}
			i++;
		} else if ( a == QLatin1String( "--cam" ) ) {
			const QStringList v = nx.split( ',' );
			if ( v.size() == 6 ) {
				for ( int k = 0; k < 6; k++ )
					cam[k] = v[k].toFloat();
				haveCam = true;
			}
			i++;
		} else if ( a == QLatin1String( "--light" ) ) {
			const QStringList v = nx.split( ',' );
			if ( v.size() == 7 ) {
				WwCellLight l;
				for ( int c = 0; c < 3; c++ ) {
					l.pos[c] = v[c].toFloat();
					l.color[c] = v[4 + c].toFloat();
				}
				l.radius = v[3].toFloat();
				l.shadow = 3;
				L.lights.push_back( l );
			}
			i++;
		}
	}
	const QStringList rc = rect.split( ',' );
	if ( soupPath.isEmpty() || outDir.isEmpty() || rc.size() != 4 || L.lights.isEmpty() || !haveCam || !( farT > nearT && nearT > 0.0f ) ) {
		std::fprintf( stderr, "usage: airrays --soup <file> --rect minX,minY,maxX,maxY --out <dir> --cam ex,ey,ez,tx,ty,tz "
			"--light x,y,z,radius,r,g,b [...] [--grid X,Y,Z] [--near t] [--far t] [--fov deg] [--frames n] [--samples S] "
			"[--time-grid X (cost at X x X*9/16 x 64)] [--red nohole,nohist,noshadow]\n" );
		return 2;
	}
	for ( int k = 0; k < 4; k++ )
		spec.rect[k] = rc[k].toFloat();
	int red = 0;
	for ( const QString & r : redText.split( ',', Qt::SkipEmptyParts ) ) {
		const QString t = r.trimmed();
		red |= t == QLatin1String( "nohole" ) ? 1 : t == QLatin1String( "nohist" ) ? 2 : t == QLatin1String( "noshadow" ) ? 4 : 0;
	}
	ProbeSoup soup;
	QString err;
	if ( !probeSoupRead( soupPath, &soup, &err ) ) {
		std::fprintf( stderr, "airrays: %s\n", qPrintable( err ) );
		return 1;
	}
	RtDdgiVolume vol;
	Packed S;
	if ( !packVolume( soup, L, sky, spec, &vol, S ) ) {
		std::fprintf( stderr, "airrays: %s\n", qPrintable( vol.error ) );
		return 1;
	}
	const V3 eye( float( cam[0] - S.O[0] ), float( cam[1] - S.O[1] ), cam[2] );
	const V3 at( float( cam[3] - S.O[0] ), float( cam[4] - S.O[1] ), cam[5] );
	const V3 f = normalize( at - eye );
	const V3 rt = normalize( cross( f, std::fabs( f.z ) > 0.99f ? V3( 0, 1, 0 ) : V3( 0, 0, 1 ) ) );
	const V3 up = cross( rt, f );
	const float rows[12] = { rt.x, up.x, -f.x, eye.x, rt.y, up.y, -f.y, eye.y, rt.z, up.z, -f.z, eye.z };
	const float fy = 1.0f / std::tan( 0.5f * fov * kPi / 180.0f );
	const float proj[4] = { fy * float( Y ) / float( X ), fy, 0.0f, 0.0f };
	const float logR = std::log( farT / nearT ), bias = 2.0f;
	RtDdgiLive live;
	RtDdgiSpec gs = spec;
	gs.relocate = false;
	gs.classify = false;
	if ( !live.open( soup, L, sky, gs, false ) ) {
		std::printf( "airrays: no gpu (%s)\n", qPrintable( live.why() ) );
		return 1;
	}
	auto inFor = [&]( int gx, int gy, int gz ) {
		bakert::AirRaysGpuIn in;
		in.X = gx;
		in.Y = gy;
		in.Z = gz;
		in.samples = samples;
		in.frames = frames;
		in.red = red;
		in.nearT = nearT;
		in.logRange = logR;
		in.bias = bias;
		in.viewUnit = 1.0f;
		std::copy( rows, rows + 12, in.rows );
		std::copy( rows, rows + 12, in.prows );
		const float pr[4] = { fy * float( gy ) / float( gx ), fy, 0.0f, 0.0f };
		std::copy( pr, pr + 4, in.proj );
		std::copy( pr, pr + 4, in.pproj );
		return in;
	};
	bakert::AirRaysGpuOut go;
	for ( int fr = 0; fr < frames; fr++ ) {
		bakert::AirRaysGpuIn in = inFor( X, Y, Z );
		in.frame = uint32_t( fr );
		in.history = fr > 0;
		in.readBack = fr + 1 == frames;
		if ( !bakert::airraysGpuStep( live.session(), in, &go ) ) {
			std::printf( "airrays: frame %d, the GPU refused (%s)\n", fr, go.why.c_str() );
			return 1;
		}
	}
	// the exact share per froxel (the kernel's froxel point and lamp weight, the CPU walk for the shadow)
	const int nL = S.nLights;
	const size_t n = size_t( X ) * size_t( Y ) * size_t( Z );
	std::vector<float> exact( n, -1.0f );
	int lit = 0, within05 = 0, within10 = 0, missing = 0;
	double sumAbs = 0, maxAbs = 0, meanG = 0, meanE = 0;
	for ( size_t id = 0; id < n; id++ ) {
		const int x = int( id % size_t( X ) ), y = int( id / size_t( X ) % size_t( Y ) ), k = int( id / ( size_t( X ) * size_t( Y ) ) );
		const float t = nearT * std::exp( ( float( k ) + 0.5f ) / float( Z ) * logR );
		const V3 dv = normalize( V3( ( ( float( x ) + 0.5f ) / float( X ) * 2.0f - 1.0f ) / proj[0], ( ( float( y ) + 0.5f ) / float( Y ) * 2.0f - 1.0f ) / proj[1], -1.0f ) ) * t;
		const V3 P( rows[0] * dv.x + rows[1] * dv.y + rows[2] * dv.z + rows[3], rows[4] * dv.x + rows[5] * dv.y + rows[6] * dv.z + rows[7],
			rows[8] * dv.x + rows[9] * dv.y + rows[10] * dv.z + rows[11] );
		double ws = 0, wv = 0;
		for ( int i = 0; i < nL; i++ ) {
			const float * l = &S.lights[size_t( i ) * kLight];
			const V3 Lv( l[0] - P.x, l[1] - P.y, l[2] - P.z );
			const float d = length( Lv );
			if ( d >= l[3] )
				continue;
			const int shape = int( l[28] + 0.5f );
			if ( shape == 1 && -( Lv.x * l[8] + Lv.y * l[9] + Lv.z * l[10] ) < 0.0f )
				continue;
			if ( shape == 2 && ( std::fabs( l[16] * P.x + l[17] * P.y + l[18] * P.z + l[19] ) > 1.0f
				|| std::fabs( l[20] * P.x + l[21] * P.y + l[22] * P.z + l[23] ) > 1.0f || std::fabs( l[24] * P.x + l[25] * P.y + l[26] * P.z + l[27] ) > 1.0f ) )
				continue;
			float att = lightRadial( d, l[3], l[12], l[13], l[14] );
			if ( l[7] > 0.5f )
				att *= spotCone( -( Lv.x * l[8] + Lv.y * l[9] + Lv.z * l[10] ) / std::max( d, 0.001f ), l[11], l[15] );
			const float w = std::max( restir::luminance( l[4], l[5], l[6] ) * att, 0.0f );
			if ( !( w > 0.0f ) )
				continue;
			bool v = true;
			if ( l[30] > 0.5f && d - 2.0f * bias > 1e-3f ) {   // the reds bend the GPU only: the exact share keeps every shadow
				const V3 dir = Lv * ( 1.0f / std::max( d, 1e-4f ) );
				const V3 o = P + dir * bias;
				const double od[3] = { o.x, o.y, o.z }, dd[3] = { dir.x, dir.y, dir.z };
				v = !S.bvh.anyHitIn( od, dd, 1e-4, double( d - 2.0f * bias ), []( int ) { return true; } );
			}
			ws += w;
			wv += v ? w : 0.0;
		}
		if ( !( ws > 0.0 ) || go.ratio.size() != n * 2 )
			continue;
		if ( !( go.ratio[id * 2 + 1] > 0.0f ) ) {
			missing++;   // a lamp reaches the froxel and the GPU found none
			continue;
		}
		exact[id] = float( wv / ws );
		const double dlt = std::fabs( double( go.ratio[id * 2] ) - wv / ws );
		lit++;
		sumAbs += dlt;
		maxAbs = std::max( maxAbs, dlt );
		within05 += dlt <= 0.05 ? 1 : 0;
		within10 += dlt <= 0.10 ? 1 : 0;
		meanG += go.ratio[id * 2];
		meanE += wv / ws;
	}
	const double lv = std::max( lit, 1 );
	std::printf( "airrays: grid %d x %d x %d, %d lamps, %d frames, %d samples, red %d; %d froxels lit, %d with no gpu lamp; |gpu - exact| mean %.4f max %.4f, "
		"within 0.05 %.2f %%, within 0.10 %.2f %%; mean share gpu %.4f exact %.4f\n", X, Y, Z, nL, frames, samples, red, lit, missing, sumAbs / lv,
		maxAbs, 100.0 * within05 / lv, 100.0 * within10 / lv, meanG / lv, meanE / lv );
	std::fflush( stdout );
	QDir().mkpath( outDir );
	QFile o( outDir + QStringLiteral( "/airrays.tsv" ) );
	if ( o.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
		o.write( "x\ty\tk\tgpu\tframes\texact\n" );
		for ( size_t id = 0; id < n && go.ratio.size() == n * 2; id++ )
			o.write( QStringLiteral( "%1\t%2\t%3\t%4\t%5\t%6\n" ).arg( id % size_t( X ) ).arg( id / size_t( X ) % size_t( Y ) )
				.arg( id / ( size_t( X ) * size_t( Y ) ) ).arg( go.ratio[id * 2] ).arg( go.ratio[id * 2 + 1] ).arg( exact[id] ).toLatin1() );
		o.close();
	}
	if ( timeX > 0 ) {
		// the cost at the size of a real grid: wall ms a step (submit + wait), median of 32 after 4 warm-ups
		const int ty = std::max( 1, timeX * 9 / 16 );
		std::vector<double> ms;
		for ( int fr = 0; fr < 36; fr++ ) {
			bakert::AirRaysGpuIn in = inFor( timeX, ty, 64 );
			in.frame = uint32_t( fr );
			in.history = fr > 0;
			if ( !bakert::airraysGpuStep( live.session(), in, &go ) )
				break;
			if ( fr >= 4 )
				ms.push_back( go.msWall );
		}
		std::sort( ms.begin(), ms.end() );
		std::printf( "airrays cost: grid %d x %d x 64 (%d froxels), %d lamps, %d samples: wall ms a step median %.3f min %.3f max %.3f\n", timeX, ty,
			timeX * ty * 64, nL, samples, ms.empty() ? -1.0 : ms[ms.size() / 2], ms.empty() ? -1.0 : ms.front(), ms.empty() ? -1.0 : ms.back() );
	}
	return 0;
}
