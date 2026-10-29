/* Lane RTXGI1 (2026-10-29): ray-traced DDGI, milestone 1 (src/rtddgi.h). The volume's steps run twice from the same
 * packed inputs: on the GPU (src/rtddgi/rtddgi_vk.inc, inside bakert.cpp) or here on the CPU, where every function
 * mirrors one kernel (traceRay = trace.comp, blendProbe = blend.comp, borderProbe = border.comp, relocate/classify =
 * reloc.comp) and every rule comes from src/rtddgimath.h. */

#include "rtddgi.h"

#include "bakert.h"
#include "probebvh.h"
#include "probeplace.h"
#include "probesky.h"
#include "rtddgimath.h"
#include "rtddgi/rtddgi_gpu.h"
#include "gl/celllights.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <thread>

using namespace rtddgi;

namespace {

constexpr int kAttr = 8;     // floats a soup triangle in triAttr
constexpr int kLight = 32;   // floats a light in the packed list (8 vec4, ddgi_common.glsl binding 13)
constexpr unsigned kRedNoBack = 1u, kRedNoCheb = 2u, kRedNoBias = 4u;   // ddgi_common.glsl DDGI_RED_*
constexpr unsigned kRedHwHit = 8u, kRedNoTie = 16u;   // M1FIX: hwhit is GPU only (the twin has no hardware hit to trust)

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
	// MIRROR trace.comp ddgiIrradianceAt
	V3 sampleVolume( V3 Pw, V3 n, V3 o ) const
	{
		const float D = S.prm.stepv[3];
		V3 Pb = Pw;
		if ( !( red & kRedNoBias ) )
			Pb = Pb + biasVector( n, o, D, S.prm.blendp[2] );
		int base[3];
		float a[3];
		for ( int k = 0; k < 3; k++ ) {
			const float g = ( Pb[k] - origin[k] ) / step[k];
			base[k] = std::min( std::max( int( std::floor( g ) ), 0 ), std::max( dims[k] - 2, 0 ) );
			a[k] = clampf( g - float( base[k] ), 0.0f, 1.0f );
		}
		double sum[3] = { 0, 0, 0 }, wsum = 0;
		for ( int c = 0; c < 8; c++ ) {
			const int off[3] = { c & 1, ( c >> 1 ) & 1, ( c >> 2 ) & 1 };
			int pc[3];
			for ( int k = 0; k < 3; k++ )
				pc[k] = std::min( base[k] + off[k], dims[k] - 1 );
			const int p = pc[0] + dims[0] * ( pc[1] + dims[1] * pc[2] );
			if ( state( p ) != kVigilant || age[size_t( p )] == 0 )
				continue;
			float trilinear = 1.0f;
			for ( int k = 0; k < 3; k++ )
				trilinear *= off[k] ? a[k] : 1.0f - a[k];
			const V3 pp = probePos( p );
			const V3 toProbe = normalize( pp - Pw );
			float w = backfaceWeight( toProbe, n );
			const V3 bToProbe = pp - Pb;
			const float r = length( bToProbe );
			if ( !( red & kRedNoCheb ) && r > 0.0f ) {
				float m[2];
				sampleTile( &dist[size_t( p ) * 512], kDistTile, 2, bToProbe * ( -1.0f / r ), m );
				w *= chebyshevWeight( r, m[0], m[1] );
			}
			w = std::max( w, 1e-6f );
			w = crushWeight( w );
			w *= trilinear;
			float e[4];
			sampleTile( &irr[size_t( p ) * 256], kIrrTile, 4, n, e );
			for ( int k = 0; k < 3; k++ )
				sum[k] += double( w ) * std::pow( std::max( e[k], 0.0f ), 2.5f );
			wsum += w;
		}
		if ( wsum <= 0.0 )
			return V3( 0, 0, 0 );
		V3 m( float( sum[0] / wsum ), float( sum[1] / wsum ), float( sum[2] / wsum ) );
		return m * m * ( 2.0f * kPi );
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
				skyRadiance( R, d, sky );
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
		for ( int c = 0; c < 3; c++ )
			rec[c] = a0[c] * E[c] / kPi + a0[4 + c];
		rec[3] = tf;
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
					blendSkyVisTexel( td, dirs.data(), dd.data(), nb, h, at[3], &vis );   // M1FIX: the alpha
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

void runTwin( const Packed & S, const RtDdgiSpec & spec, RtDdgiVolume * out )
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
	}
	out->msFrames = double( tm.nsecsElapsed() ) / 1e6;
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

bool rtddgiRun( const ProbeSoup & soup, const WwCellLighting & lighting, const ProbeSkyLight & sky, const RtDdgiSpec & spec,
	RtDdgiVolume * out )
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
	const float st[3] = { spec.spacing, spec.spacing, spec.spacing * spec.zScale };
	float lo[3] = { 3.4e38f, 3.4e38f, 3.4e38f }, hi[3] = { -3.4e38f, -3.4e38f, -3.4e38f };
	bool have = false;
	if ( spec.place ) {
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
	for ( int k = 0; k < 3; k++ ) {
		out->step[k] = st[k];
		out->origin[k] = std::floor( lo[k] / st[k] ) * st[k];   // world multiples of the spacing
		out->dims[k] = std::max( 2, int( std::floor( ( hi[k] - out->origin[k] ) / st[k] ) ) + 1 );
		nP *= out->dims[k];
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
	Packed S;
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
	S.bvh.build();
	S.bvh.mask = &soup.amask;
	const size_t nTri = soup.tris.size() / 9;
	S.triAttr.assign( nTri * kAttr, 0.0f );
	const bool hasAlb = soup.alb.size() >= nTri * 3;
	for ( size_t t = 0; t < nTri; t++ ) {
		float * a = &S.triAttr[t * kAttr];
		for ( int c = 0; c < 3; c++ )
			a[c] = hasAlb ? soup.alb[t * 3 + size_t( c )] / 255.0f : 0.5f;   // ours: mid grey without ALB1
		a[3] = soup.isTwoSided( t ) ? 1.0f : 0.0f;
		if ( soup.glow.emits( int( t ) ) ) {   // the glow at the triangle's centre (ours: one texel a triangle)
			double le[3];
			soup.glow.le( int( t ), 1.0 / 3.0, 1.0 / 3.0, le );
			for ( int c = 0; c < 3; c++ )
				a[4 + c] = float( le[c] );
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
				P.sky[f][c] = R[f][c];
		P.sky[0][3] = 1.0f;
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

	// ---- the GPU when asked and able, else the twin
	bool done = false;
	QString why = QStringLiteral( "cpu (asked)" );
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
		runTwin( S, spec, out );
	}
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
		else if ( a == QLatin1String( "--lights" ) ) { lightsFile = nx; i++; }
		else if ( a == QLatin1String( "--one-bounce" ) ) spec.oneBounce = true;
		else if ( a == QLatin1String( "--no-sky" ) ) spec.noSky = true;
		else if ( a == QLatin1String( "--no-place" ) ) spec.place = false;
		else if ( a == QLatin1String( "--no-reloc" ) ) spec.relocate = false;
		else if ( a == QLatin1String( "--no-classify" ) ) spec.classify = false;
		else if ( a == QLatin1String( "--trace-asleep" ) ) spec.skipAsleep = false;
		else if ( a == QLatin1String( "--dump-fixed" ) ) spec.dumpFixed = true;
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
			"[--no-classify] [--trace-asleep] [--dump-fixed] [--red noback,nocheb,nobias,norot,hwhit,notie]\n" );
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
	return 0;
}
