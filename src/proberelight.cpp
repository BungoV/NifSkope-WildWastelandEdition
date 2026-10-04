/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

// lane GPURELIGHT1: the relight without rays, on the CPU (double) and the GPU (GL 4.3 compute). src/proberelight.h.

#include "proberelight.h"

#include "gl/celllights.h"
#include "probebake.h"
#include "probegi.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions_4_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <QRegularExpression>
#include <QTextStream>
#include <QtEndian>
#include <qfloat16.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <thread>

namespace {

constexpr double kPi = 3.141592653589793;

void parallelFor( size_t n, const std::function<void( size_t )> & fn )
{
	const int nThreads = std::max( 1, int( std::thread::hardware_concurrency() ) );
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

// PRTP2 section 1, probegi.cpp's radial() term for term
double radialL( double d, double r, const ProbeRelightOps::Light & l )
{
	const double x = std::min( std::max( d / std::max( r, 0.001 ), 0.0 ), 1.0 );
	const double xe = l.exponent > 0.0f ? std::pow( x, double( l.exponent ) ) : 1.0;
	const double k = 1.0 - std::min( std::max( l.scale * xe + l.bias, 0.0 ), 1.0 );
	return std::pow( k, 2.2 );
}

enum RedMode { RedNone = 0, RedBlanket = 1, RedGlassOpaque = 2, RedDoorOpen = 3 };

//! the state's red, tokens joined by '+' ("glassclear" is the D1 gate's knob, not a defect)
bool hasToken( const QString & r, const char * t )
{
	return r.split( QLatin1Char( '+' ) ).contains( QLatin1String( t ) );
}

int redMode( const QString & r )
{
	if ( hasToken( r, "doorblanket" ) )
		return RedBlanket;
	if ( hasToken( r, "glassopaque" ) )
		return RedGlassOpaque;
	if ( hasToken( r, "dooropen" ) )
		return RedDoorOpen;
	return RedNone;
}

/*! An entry's factor for channel c through its closed doors. vis: a visibility entry (the feed's, the blend's):
 *  0 where a solid face stops it, else 1 (the bake's visibility rays pass glass); glassClear (the gate's knob): the
 *  light entries read their doors that way too (the merged reference traces no glass). */
inline double doorFactor( const int door[2], const float * T, int c, bool vis, const std::vector<quint8> & closed, int red )
{
	if ( red == RedDoorOpen )
		return 1.0;
	double f = 1.0;
	for ( int s = 0; s < 2; s++ ) {
		const int d = door[s];
		if ( d < 0 || size_t( d ) >= closed.size() || !closed[size_t( d )] )
			continue;
		if ( red == RedBlanket )
			return 0.0;
		const float * t = T + s * 4;
		if ( red == RedGlassOpaque )
			f *= t[3];
		else if ( vis )
			f *= ( t[0] > 0.0f || t[1] > 0.0f || t[2] > 0.0f ) ? 1.0 : 0.0;
		else
			f *= t[c];
	}
	return f;
}

inline bool anyClosed( const int door[2], const std::vector<quint8> & closed )
{
	for ( int s = 0; s < 2; s++ )
		if ( door[s] >= 0 && size_t( door[s] ) < closed.size() && closed[size_t( door[s] )] )
			return true;
	return false;
}

}	// namespace

// ======================================================================== the doors' tracer

void ProbeDoorTracer::build( const ProbeSoup & soup, const double Oin[2] )
{
	O[0] = Oin[0];
	O[1] = Oin[1];
	boxes.clear();
	boxes.resize( soup.doors.size() );
	const ProbeSoup::DoorGeom & g = soup.doorGeom;
	hasGeom = !g.empty();
	for ( size_t k = 0; k < soup.doors.size(); k++ ) {
		D & d = boxes[k];
		for ( int c = 0; c < 3; c++ ) {
			d.lo[c] = double( soup.doors[k].lo[c] ) - 0.5;
			d.hi[c] = double( soup.doors[k].hi[c] ) + 0.5;
		}
		std::map<int, int> mapLocal;   // only the maps this door's triangles use (a cell holds many maps)
		for ( size_t i = 0; i * 9 < g.tris.size(); i++ ) {
			if ( g.door[i] != int( k ) )
				continue;
			const int local = int( d.solid.t.size() / 9 );
			for ( int v = 0; v < 3; v++ ) {   // the bake tracer's conversion (probegi.cpp)
				d.solid.t.push_back( float( double( g.tris[i * 9 + size_t( v ) * 3 + 0] ) - O[0] ) );
				d.solid.t.push_back( float( double( g.tris[i * 9 + size_t( v ) * 3 + 1] ) - O[1] ) );
				d.solid.t.push_back( g.tris[i * 9 + size_t( v ) * 3 + 2] );
			}
			d.mask.triOf.push_back( -1 );
			if ( i < g.amask.triOf.size() && g.amask.triOf[i] >= 0 ) {
				probebvh::AlphaMask::Tri t = g.amask.tris[size_t( g.amask.triOf[i] )];
				if ( t.map >= 0 && size_t( t.map ) < g.amask.maps.size() ) {
					auto it = mapLocal.find( t.map );
					if ( it == mapLocal.end() ) {
						it = mapLocal.emplace( t.map, int( d.mask.maps.size() ) ).first;
						d.mask.maps.push_back( g.amask.maps[size_t( t.map )] );
						d.mask.mapNames.push_back( size_t( t.map ) < g.amask.mapNames.size() ? g.amask.mapNames[size_t( t.map )]
							: std::string() );
					}
					t.map = it->second;
					d.mask.triOf[size_t( local )] = int( d.mask.tris.size() );
					d.mask.tris.push_back( t );
				}
			}
		}
		if ( d.mask.tris.empty() ) {
			d.mask.maps.clear();
			d.mask.mapNames.clear();
		}
		d.solid.build();
		for ( size_t i = 0; i * 9 < g.glass.size(); i++ ) {
			if ( g.glassDoor[i] != int( k ) )
				continue;
			for ( int v = 0; v < 3; v++ ) {
				d.glass.push_back( float( double( g.glass[i * 9 + size_t( v ) * 3 + 0] ) - O[0] ) );
				d.glass.push_back( float( double( g.glass[i * 9 + size_t( v ) * 3 + 1] ) - O[1] ) );
				d.glass.push_back( g.glass[i * 9 + size_t( v ) * 3 + 2] );
			}
			for ( int c = 0; c < 3; c++ )
				d.glassT.push_back( g.glassT[i * 3 + size_t( c )] / 255.0f );
		}
	}
	for ( D & d : boxes )   // after the vector holds still
		d.solid.mask = &d.mask;
}

bool ProbeDoorTracer::boxHit( const D & d, const double p[3], const double q[3], double clearEnd, double * tEnter ) const
{
	const double dv[3] = { q[0] - p[0], q[1] - p[1], q[2] - p[2] };
	const double len = std::sqrt( dv[0] * dv[0] + dv[1] * dv[1] + dv[2] * dv[2] );
	const double tmax = len - clearEnd;
	if ( tmax <= 1e-3 || len <= 0.0 )
		return false;
	double t0 = 0.0, t1 = tmax;
	for ( int k = 0; k < 3; k++ ) {
		const double dk = dv[k] / len;
		if ( std::fabs( dk ) < 1e-300 ) {
			if ( p[k] < d.lo[k] || p[k] > d.hi[k] )
				return false;
			continue;
		}
		double a = ( d.lo[k] - p[k] ) / dk, b = ( d.hi[k] - p[k] ) / dk;
		if ( a > b )
			std::swap( a, b );
		t0 = std::max( t0, a );
		t1 = std::min( t1, b );
		if ( t0 > t1 )
			return false;
	}
	*tEnter = t0;
	return true;
}

void ProbeDoorTracer::trace( const D & d, const double p[3], const double q[3], double clearEnd, float T[4] ) const
{
	T[0] = T[1] = T[2] = T[3] = 1.0f;
	if ( !hasGeom ) {   // no geometry known: a closed door is a blanket cut (census: doorGeometry 0)
		T[0] = T[1] = T[2] = T[3] = 0.0f;
		return;
	}
	double dir[3] = { q[0] - p[0], q[1] - p[1], q[2] - p[2] };
	const double len = std::sqrt( dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2] );
	const double tmax = len - clearEnd;
	if ( tmax <= 1e-3 )
		return;
	for ( double & c : dir )
		c /= len;
	const double o[3] = { p[0] - O[0], p[1] - O[1], p[2] };
	double tHit;
	if ( !d.solid.t.empty() && d.solid.ray( o, dir, tmax, &tHit ) ) {
		T[0] = T[1] = T[2] = T[3] = 0.0f;
		return;
	}
	for ( size_t i = 0; i * 9 < d.glass.size(); i++ ) {
		double tt, u, v;
		if ( probebvh::Bvh::tri( &d.glass[i * 9], o, dir, tt, u, v ) && tt > 1e-4 && tt <= tmax ) {
			for ( int c = 0; c < 3; c++ )
				T[c] *= d.glassT[i * 3 + size_t( c )];
			T[3] = 0.0f;
		}
	}
}

int ProbeDoorTracer::crossed( const double p[3], const double q[3], double clearEnd, int slot[2], float T[8], int * over ) const
{
	slot[0] = slot[1] = -1;
	for ( int k = 0; k < 8; k++ )
		T[k] = 1.0f;
	*over = 0;
	if ( boxes.empty() )
		return 0;
	std::pair<double, int> hit[2] = { { 1e300, -1 }, { 1e300, -1 } };
	int n = 0;
	for ( size_t k = 0; k < boxes.size(); k++ ) {
		double te;
		if ( !boxHit( boxes[k], p, q, clearEnd, &te ) )
			continue;
		n++;
		const std::pair<double, int> h { te, int( k ) };
		if ( h < hit[0] ) {
			hit[1] = hit[0];
			hit[0] = h;
		} else if ( h < hit[1] )
			hit[1] = h;
	}
	*over = std::max( 0, n - 2 );
	for ( int s = 0; s < 2; s++ ) {
		if ( hit[s].second < 0 )
			continue;
		slot[s] = hit[s].second;
		trace( boxes[size_t( hit[s].second )], p, q, clearEnd, T + s * 4 );
	}
	return std::min( n, 2 );
}

void ProbeDoorTracer::linkMean( const double probe[3], const double p[3], const double n[3], double cs, int slot[2], float T[8] ) const
{
	slot[0] = slot[1] = -1;
	for ( int k = 0; k < 8; k++ )
		T[k] = 1.0f;
	if ( boxes.empty() )
		return;
	// two tangents of the surfel's plane
	const double a[3] = { std::fabs( n[0] ) < 0.9 ? 1.0 : 0.0, std::fabs( n[0] ) < 0.9 ? 0.0 : 1.0, 0.0 };
	double t1[3] = { a[1] * n[2] - a[2] * n[1], a[2] * n[0] - a[0] * n[2], a[0] * n[1] - a[1] * n[0] };
	const double l1 = std::sqrt( t1[0] * t1[0] + t1[1] * t1[1] + t1[2] * t1[2] );
	for ( double & c : t1 )
		c /= l1;
	const double t2[3] = { n[1] * t1[2] - n[2] * t1[1], n[2] * t1[0] - n[0] * t1[2], n[0] * t1[1] - n[1] * t1[0] };
	double pts[16][3];
	for ( int b = 0; b < 4; b++ )
		for ( int c = 0; c < 4; c++ ) {
			const double u = ( ( c + 0.5 ) / 4.0 - 0.5 ) * cs, v = ( ( b + 0.5 ) / 4.0 - 0.5 ) * cs;
			for ( int k = 0; k < 3; k++ )
				pts[b * 4 + c][k] = p[k] + n[k] * 1.0 + t1[k] * u + t2[k] * v;
		}
	// the doors the link crosses: its own centre ray first, then the samples' (first seen first), two kept
	std::vector<int> seen;
	auto collect = [&]( const double q[3] ) {
		for ( size_t k = 0; k < boxes.size(); k++ ) {
			double te;
			if ( boxHit( boxes[k], probe, q, 0.0, &te ) && std::find( seen.begin(), seen.end(), int( k ) ) == seen.end() )
				seen.push_back( int( k ) );
		}
	};
	const double pc[3] = { p[0] + n[0], p[1] + n[1], p[2] + n[2] };
	collect( pc );
	for ( const auto & q : pts )
		collect( q );
	for ( int s = 0; s < 2 && s < int( seen.size() ); s++ ) {
		const D & d = boxes[size_t( seen[size_t( s )] )];
		slot[s] = seen[size_t( s )];
		// the mean over the samples whose ray crosses this door's box (the rest pass beside it, through the wall's
		// own rays the bake already judged); none: the centre ray's
		double acc[4] = { 0, 0, 0, 0 };
		int m = 0;
		for ( const auto & q : pts ) {
			double te;
			if ( !boxHit( d, probe, q, 0.0, &te ) )
				continue;
			float t[4];
			trace( d, probe, q, 0.0, t );
			for ( int c = 0; c < 4; c++ )
				acc[c] += t[c];
			m++;
		}
		if ( !m ) {
			float t[4];
			trace( d, probe, pc, 0.0, t );
			for ( int c = 0; c < 4; c++ )
				acc[c] = t[c];
			m = 1;
		}
		for ( int c = 0; c < 4; c++ )
			T[s * 4 + c] = float( acc[c] / m );
	}
}

// ======================================================================== the CPU relight

bool probeRelightCpu( const ProbeRelightOps & X, const ProbeRelightState & st, ProbeRelightOut * out, QString * err )
{
	QElapsedTimer clock;
	clock.start();
	ProbeRelightOut & O = *out;
	O = ProbeRelightOut();
	if ( !X.built ) {
		*err = QStringLiteral( "no operators recorded" );
		return false;
	}
	const size_t nS = size_t( X.surfels ), nP = size_t( X.probes ), nL = X.lights.size();
	const int red = redMode( st.red );
	const bool glassClear = hasToken( st.red, "glassclear" );   // the D1 gate's knob (its reference merges no panes)
	// the live lights
	std::vector<double> col( nL * 3 ), rad( nL );
	std::vector<quint8> on( nL, 1 );
	for ( size_t l = 0; l < nL; l++ ) {
		const ProbeRelightOps::Light & L = X.lights[l];
		bool any = false;
		for ( int c = 0; c < 3; c++ ) {
			col[l * 3 + size_t( c )] = st.color.size() == nL * 3 ? double( st.color[l * 3 + size_t( c )] ) : double( L.onAtStart ? L.color[c] : 0.0f );
			any = any || col[l * 3 + size_t( c )] != 0.0;
		}
		on[l] = any;
		double r = L.radius;
		if ( st.radius.size() == nL ) {
			r = st.radius[l];
			if ( r > L.radius ) {
				r = L.radius;
				O.radiusClamped++;
			}
		}
		rad[l] = r;
	}
	std::vector<quint8> closed = st.doorClosed;
	closed.resize( X.doorRefs.size(), 0 );
	bool doors = false;
	for ( quint8 c : closed )
		doors = doors || c;
	double sun[3];
	for ( int c = 0; c < 3; c++ )
		sun[c] = st.sun[0] >= 0.0f ? double( st.sun[c] ) : X.sun[c];
	// 1. direct
	O.B1.resize( nS * 3 );
	std::vector<double> B1( nS * 3 ), Bk( nS * 3 );
	parallelFor( nS, [&]( size_t i ) {
		double E[3] = { 0, 0, 0 };
		for ( int k = X.pairStart[i]; k < X.pairStart[i + 1]; k++ ) {
			const size_t li = size_t( X.pairLight[size_t( k )] );
			if ( !on[li] )
				continue;
			const ProbeRelightOps::Light & L = X.lights[li];
			const double d = X.pairD[size_t( k )], nl = X.pairNL[size_t( k )];
			if ( d >= rad[li] )
				continue;
			double a = radialL( d, rad[li], L );
			if ( L.spot ) {
				const double dl = X.pairDL[size_t( k )];
				const double base = std::min( std::max( 1.0 - ( 1.0 - dl ) / std::max( 1.0 - L.cosOuter, 1e-4 ), 0.0 ), 1.0 );
				a *= std::min( std::pow( base, std::max( double( L.cone ), 1e-3 ) ), 1.0 );
			}
			if ( a * nl <= 0.0 )
				continue;
			const int * dr = &X.pairDoor[size_t( k ) * 2];
			if ( doors && anyClosed( dr, closed ) ) {
				const float * T = &X.pairT[size_t( k ) * 8];
				for ( int c = 0; c < 3; c++ )
					E[c] += col[li * 3 + size_t( c )] * a * nl * doorFactor( dr, T, c, glassClear, closed, red );
			} else
				for ( int c = 0; c < 3; c++ )
					E[c] += col[li * 3 + size_t( c )] * a * nl;
		}
		if ( X.hasDir && X.dirK[i] > 0.0 )
			for ( int c = 0; c < 3; c++ )
				E[c] += X.dirColor[c] * X.dirK[i];
		if ( X.sunOn && X.sunK[i] > 0.0 )
			for ( int c = 0; c < 3; c++ )
				E[c] += sun[c] * X.sunK[i];
		for ( int c = 0; c < 3; c++ ) {
			double b = X.alb[i * 3 + size_t( c )] * E[c];
			b += X.le[i * 3 + size_t( c )];
			B1[i * 3 + size_t( c )] = b;
			Bk[i * 3 + size_t( c )] = b;
			O.B1[i * 3 + size_t( c )] = float( b );
		}
	} );
	// 2. gather (step 2 and every pass after: the same expression in the same order)
	std::vector<double> PE( nP * 18 );
	auto gather = [&]() {
		parallelFor( nP, [&]( size_t j ) {
			double E[18] = {};
			for ( int k = X.linkStart[j]; k < X.linkStart[j + 1]; k++ ) {
				const double * B = &Bk[size_t( X.linkSurf[size_t( k )] ) * 3];
				const double * tint = &X.linkTint[size_t( k ) * 3];
				const double omega = X.linkOmega[size_t( k )];
				const double * cosA = &X.linkCos[size_t( k ) * 6];
				const int * dr = &X.linkDoor[size_t( k ) * 2];
				if ( doors && anyClosed( dr, closed ) ) {
					const float * T = &X.linkT[size_t( k ) * 8];
					double f[3];
					for ( int c = 0; c < 3; c++ )
						f[c] = doorFactor( dr, T, c, false, closed, red );
					for ( int a = 0; a < 6; a++ )
						for ( int c = 0; c < 3; c++ )
							E[a * 3 + c] += B[c] * tint[c] * omega * cosA[a] * f[c];
				} else
					for ( int a = 0; a < 6; a++ )
						for ( int c = 0; c < 3; c++ )
							E[a * 3 + c] += B[c] * tint[c] * omega * cosA[a];
			}
			if ( X.kUnl[j] != 1.0 )
				for ( double & c : E )
					c *= X.kUnl[j];
			if ( X.skyOn )
				for ( int k = 0; k < 18; k++ )
					E[k] += X.skyE[j * 18 + size_t( k )];
			std::memcpy( &PE[j * 18], E, sizeof E );
		} );
	};
	gather();
	// 3. the passes
	double sum1 = 0.0, max1 = 0.0;
	for ( double b : Bk ) {
		sum1 += b;
		max1 = std::max( max1, b );
	}
	O.passLog = { 0.0, sum1, max1 };
	bool fixed = X.fixedPasses;
	int cap = X.maxPasses;
	if ( st.passes > 0 ) {
		fixed = true;
		cap = st.passes;
	}
	if ( hasToken( st.red, "nobounce" ) ) {
		fixed = true;
		cap = 1;
	}
	O.passes = 1;
	O.settled = fixed;
	for ( int pass = 2; pass <= cap; pass++ ) {
		std::vector<double> Bn( Bk.size() );
		parallelFor( nS, [&]( size_t i ) {
			const double * n = &X.nrm[i * 3];
			double E[3] = { 0, 0, 0 }, ws = 0.0;
			for ( int k = X.feedStart[i]; k < X.feedStart[i + 1]; k++ ) {
				const int * dr = &X.feedDoor[size_t( k ) * 2];
				if ( doors && anyClosed( dr, closed ) && doorFactor( dr, &X.feedT[size_t( k ) * 8], 0, true, closed, red ) <= 0.0 )
					continue;
				const double * P = &PE[size_t( X.feedProbe[size_t( k )] ) * 18];
				const double w = X.feedW[size_t( k )];
				ws += w;
				for ( int a = 0; a < 3; a++ ) {
					const int ax = 2 * a + ( n[a] >= 0.0 ? 0 : 1 );
					for ( int c = 0; c < 3; c++ )
						E[c] += w * n[a] * n[a] * P[ax * 3 + c];
				}
			}
			for ( int c = 0; c < 3; c++ ) {
				const double a = X.redGrow ? 1.5 : X.alb[i * 3 + size_t( c )];
				Bn[i * 3 + size_t( c )] = B1[i * 3 + size_t( c )] + ( ws > 0.0 ? a * ( E[c] / ws ) / kPi : 0.0 );
			}
		} );
		double ch = 0.0, sum = 0.0, mx = 0.0;
		for ( size_t k = 0; k < Bn.size(); k++ ) {
			ch = std::max( ch, std::fabs( Bn[k] - Bk[k] ) );
			sum += Bn[k];
			mx = std::max( mx, Bn[k] );
		}
		Bk.swap( Bn );
		gather();
		O.passes = pass;
		O.passLog.insert( O.passLog.end(), { ch, sum, mx } );
		if ( !fixed && ch <= X.settle * mx ) {
			O.settled = true;
			break;
		}
	}
	O.B.assign( Bk.begin(), Bk.end() );
	O.E.assign( PE.begin(), PE.end() );
	// 4. the grid
	const size_t nVox = X.nVox;
	O.grid.assign( nVox * 24, 0.0f );
	O.grid2.assign( nVox * 24, 0.0f );
	const size_t nSlot = X.slotVox.size();
	std::atomic<int> emptied( 0 );
	parallelFor( nSlot, [&]( size_t s ) {
		const int b = X.blendStart[s], e = X.blendStart[s + 1];
		if ( b == e )
			return;
		double acc[18] = {}, wsum = 0.0;
		for ( int k = b; k < e; k++ ) {
			const int * dr = &X.blendDoor[size_t( k ) * 2];
			if ( doors && anyClosed( dr, closed ) && doorFactor( dr, &X.blendT[size_t( k ) * 8], 0, true, closed, red ) <= 0.0 )
				continue;
			const double w = X.blendW[size_t( k )];
			const double * P = &PE[size_t( X.blendProbe[size_t( k )] ) * 18];
			wsum += w;
			for ( int q = 0; q < 18; q++ )
				acc[q] += w * P[q];
		}
		std::vector<float> & G = X.slotWhich[s] ? O.grid2 : O.grid;
		const size_t v = size_t( X.slotVox[s] );
		for ( int a = 0; a < 6; a++ ) {
			const size_t o = ( size_t( a ) * nVox + v ) * 4;
			for ( int c = 0; c < 3; c++ )
				G[o + size_t( c )] = wsum > 0.0 ? float( acc[a * 3 + c] / wsum ) : 0.0f;
			G[o + 3] = 1.0f;
		}
		if ( wsum <= 0.0 )
			emptied++;
	} );
	O.slotsEmptied = emptied;
	auto growRange = [&]( int g0, int g1 ) {
		for ( int g = g0; g < g1; g++ ) {
			const size_t s = size_t( X.growSlot[size_t( g )] );
			double acc[18] = {};
			const int b = X.growStart[size_t( g )], e = X.growStart[size_t( g ) + 1];
			for ( int k = b; k < e; k++ ) {
				const size_t q = size_t( X.growSrc[size_t( k )] );
				const std::vector<float> & G = X.slotWhich[q] ? O.grid2 : O.grid;
				for ( int a = 0; a < 6; a++ )
					for ( int c = 0; c < 3; c++ )
						acc[a * 3 + c] += G[( size_t( a ) * nVox + size_t( X.slotVox[q] ) ) * 4 + size_t( c )];
			}
			std::vector<float> & G = X.slotWhich[s] ? O.grid2 : O.grid;
			const int n = e - b;
			for ( int a = 0; a < 6; a++ ) {
				const size_t o = ( size_t( a ) * nVox + size_t( X.slotVox[s] ) ) * 4;
				for ( int c = 0; c < 3; c++ )
					G[o + size_t( c )] = float( acc[a * 3 + c] / n );
				G[o + 3] = 1.0f;
			}
		}
	};
	/* ring 1 reads only blended slots, so its order within the ring does not matter; ring 2 may read ring 1's.
	 * probegi.cpp computes a ring from the state before it, then writes: the same, as no slot of a ring reads
	 * another of the same ring (they were empty then). */
	growRange( 0, X.growRing0 );
	growRange( X.growRing0, int( X.growSlot.size() ) );
	O.ms = clock.nsecsElapsed() / 1e6;
	return true;
}

QString probeRelightCensusText( const ProbeRelightOps & X )
{
	qint64 pairsLit = 0;
	for ( const ProbeRelightOps::Light & l : X.lights )
		pairsLit += l.onAtStart ? 0 : 1;
	return QStringLiteral( "gpurelight: record %1 surfels %2 probes %3 lights (%4 off at the start) | %5 pairs %6 links "
						   "%7 feed %8 slots %9 blend %10 grown | doors %11 (geometry %12): crossing pairs %13 "
						   "(stopped %14, partly %15) links %16 (%17, %18) feed %19 (%20) blend %21 (%22), over 2 %23 | "
						   "%24 ms" )
		.arg( X.surfels ).arg( X.probes ).arg( X.lights.size() ).arg( pairsLit )
		.arg( X.pairLight.size() ).arg( X.linkSurf.size() ).arg( X.feedProbe.size() ).arg( X.slotVox.size() )
		.arg( X.blendProbe.size() ).arg( X.growSlot.size() ).arg( X.doorRefs.size() ).arg( X.doorGeometry ? 1 : 0 )
		.arg( X.doorEntries[0] ).arg( X.doorStopped[0] ).arg( X.doorTinted[0] ).arg( X.doorEntries[1] )
		.arg( X.doorStopped[1] ).arg( X.doorTinted[1] ).arg( X.doorEntries[2] ).arg( X.doorStopped[2] )
		.arg( X.doorEntries[3] ).arg( X.doorStopped[3] ).arg( X.doorOver2 ).arg( X.msRecord, 0, 'f', 1 );
}

// ======================================================================== the GPU relight

namespace {

const char * kGlslHead = R"(#version 430
layout( local_size_x = 64 ) in;
layout( std430, binding = 0 ) buffer Surf { float surf[]; };          // 16 a surfel: n, dirK, alb, sunK, le, -, B1, -
layout( std430, binding = 1 ) readonly buffer PairStart { int pairStart[]; };
layout( std430, binding = 2 ) readonly buffer Pairs { float pairs[]; };   // 8: light, d, nl, dl, doorRec, -, -, -
layout( std430, binding = 3 ) readonly buffer DoorRec { float doorRec[]; }; // 12: door0, door1, -, -, T[8]
layout( std430, binding = 4 ) readonly buffer Lights { float lights[]; }; // 16: pos, radius, color, bias, scale, exp, cone, cosOuter, spot
layout( std430, binding = 5 ) coherent buffer Ctl { uint ctl[]; };       // 0 max change, 1 max B, 2.. door closed
layout( std430, binding = 6 ) readonly buffer LinkStart { int linkStart[]; };
layout( std430, binding = 7 ) readonly buffer Links { float links[]; };   // 12: surf, omega, tint, cos[6], doorRec
layout( std430, binding = 8 ) readonly buffer ProbeK { float probeK[]; }; // 20: kUnl, -, sky[18]
layout( std430, binding = 9 ) buffer PE { float pe[]; };                 // 18 a probe
layout( std430, binding = 10 ) buffer Bsrc { float bsrc[]; };           // 4 a surfel
layout( std430, binding = 11 ) buffer Bdst { float bdst[]; };
layout( std430, binding = 12 ) readonly buffer FeedStart { int feedStart[]; };
layout( std430, binding = 13 ) readonly buffer Feed { float feed[]; };    // 4: probe, w, doorRec, -
layout( std430, binding = 14 ) readonly buffer Slots { int slots[]; };   // 2: voxel, which
layout( std430, binding = 15 ) buffer Grid { float grid[]; };           // 2 x nVox x 24
uniform int uN;
uniform int uRed;          // 1 doorblanket, 2 glassopaque, 3 dooropen
uniform int uHasDir;
uniform vec3 uDirColor;
uniform int uSunOn;
uniform vec3 uSun;
uniform int uSkyOn;
uniform int uRedGrow;
uniform int uNVox;
uniform int uFirst;

float doorF( int rec, int c, bool vis )
{
	if ( rec < 0 || uRed == 3 )
		return 1.0;
	float f = 1.0;
	for ( int s = 0; s < 2; s++ ) {
		int d = floatBitsToInt( doorRec[rec * 12 + s] );
		if ( d < 0 || ctl[2 + d] == 0u )
			continue;
		if ( uRed == 1 )
			return 0.0;
		int o = rec * 12 + 4 + s * 4;
		if ( uRed == 2 )
			f *= doorRec[o + 3];
		else if ( vis )
			f *= ( doorRec[o] > 0.0 || doorRec[o + 1] > 0.0 || doorRec[o + 2] > 0.0 ) ? 1.0 : 0.0;
		else
			f *= doorRec[o + c];
	}
	return f;
}
)";

const char * kGlslDirect = R"(
void main()
{
	int i = int( gl_GlobalInvocationID.x );
	if ( i >= uN )
		return;
	vec3 E = vec3( 0.0 );
	for ( int k = pairStart[i]; k < pairStart[i + 1]; k++ ) {
		int li = floatBitsToInt( pairs[k * 8] );
		float d = pairs[k * 8 + 1], nl = pairs[k * 8 + 2], dl = pairs[k * 8 + 3];
		int rec = floatBitsToInt( pairs[k * 8 + 4] );
		int lo = li * 16;
		vec3 col = vec3( lights[lo + 4], lights[lo + 5], lights[lo + 6] );
		float r = lights[lo + 3];
		if ( col == vec3( 0.0 ) || d >= r )
			continue;
		float x = clamp( d / max( r, 0.001 ), 0.0, 1.0 );
		float xe = lights[lo + 9] > 0.0 ? pow( x, lights[lo + 9] ) : 1.0;
		float kk = 1.0 - clamp( lights[lo + 8] * xe + lights[lo + 7], 0.0, 1.0 );
		float a = kk > 0.0 ? pow( kk, 2.2 ) : 0.0;
		if ( lights[lo + 12] > 0.5 ) {
			float base = clamp( 1.0 - ( 1.0 - dl ) / max( 1.0 - lights[lo + 11], 1e-4 ), 0.0, 1.0 );
			a *= base > 0.0 ? min( pow( base, max( lights[lo + 10], 1e-3 ) ), 1.0 ) : 0.0;
		}
		if ( a * nl <= 0.0 )
			continue;
		vec3 f = vec3( doorF( rec, 0, false ), doorF( rec, 1, false ), doorF( rec, 2, false ) );
		E += col * ( a * nl ) * f;
	}
	int so = i * 16;
	if ( uHasDir != 0 && surf[so + 3] > 0.0 )
		E += uDirColor * surf[so + 3];
	if ( uSunOn != 0 && surf[so + 7] > 0.0 )
		E += uSun * surf[so + 7];
	vec3 B = vec3( surf[so + 4], surf[so + 5], surf[so + 6] ) * E + vec3( surf[so + 8], surf[so + 9], surf[so + 10] );
	surf[so + 12] = B.x;
	surf[so + 13] = B.y;
	surf[so + 14] = B.z;
	bsrc[i * 4] = B.x;
	bsrc[i * 4 + 1] = B.y;
	bsrc[i * 4 + 2] = B.z;
}
)";

const char * kGlslGather = R"(
void main()
{
	int j = int( gl_GlobalInvocationID.x );
	if ( j >= uN )
		return;
	float E[18];
	for ( int q = 0; q < 18; q++ )
		E[q] = 0.0;
	for ( int k = linkStart[j]; k < linkStart[j + 1]; k++ ) {
		int o = k * 12;
		int s = floatBitsToInt( links[o] );
		int rec = floatBitsToInt( links[o + 11] );
		vec3 B = vec3( bsrc[s * 4], bsrc[s * 4 + 1], bsrc[s * 4 + 2] );
		vec3 w = B * vec3( links[o + 2], links[o + 3], links[o + 4] ) * links[o + 1];
		if ( rec >= 0 )
			w *= vec3( doorF( rec, 0, false ), doorF( rec, 1, false ), doorF( rec, 2, false ) );
		for ( int a = 0; a < 6; a++ ) {
			float ca = links[o + 5 + a];
			E[a * 3] += w.x * ca;
			E[a * 3 + 1] += w.y * ca;
			E[a * 3 + 2] += w.z * ca;
		}
	}
	float kU = probeK[j * 20];
	for ( int q = 0; q < 18; q++ ) {
		float e = E[q] * kU;
		if ( uSkyOn != 0 )
			e += probeK[j * 20 + 2 + q];
		pe[j * 18 + q] = e;
	}
}
)";

const char * kGlslFeed = R"(
void main()
{
	int i = int( gl_GlobalInvocationID.x );
	if ( i >= uN )
		return;
	int so = i * 16;
	vec3 n = vec3( surf[so], surf[so + 1], surf[so + 2] );
	vec3 E = vec3( 0.0 );
	float ws = 0.0;
	for ( int k = feedStart[i]; k < feedStart[i + 1]; k++ ) {
		int rec = floatBitsToInt( feed[k * 4 + 2] );
		if ( rec >= 0 && doorF( rec, 0, true ) <= 0.0 )
			continue;
		int p = floatBitsToInt( feed[k * 4] );
		float w = feed[k * 4 + 1];
		ws += w;
		for ( int a = 0; a < 3; a++ ) {
			int ax = 2 * a + ( n[a] >= 0.0 ? 0 : 1 );
			int po = p * 18 + ax * 3;
			E += w * n[a] * n[a] * vec3( pe[po], pe[po + 1], pe[po + 2] );
		}
	}
	vec3 alb = uRedGrow != 0 ? vec3( 1.5 ) : vec3( surf[so + 4], surf[so + 5], surf[so + 6] );
	vec3 B1 = vec3( surf[so + 12], surf[so + 13], surf[so + 14] );
	vec3 Bn = B1 + ( ws > 0.0 ? alb * ( E / ws ) / 3.14159265358979 : vec3( 0.0 ) );
	vec3 Bo = vec3( bsrc[i * 4], bsrc[i * 4 + 1], bsrc[i * 4 + 2] );
	bdst[i * 4] = Bn.x;
	bdst[i * 4 + 1] = Bn.y;
	bdst[i * 4 + 2] = Bn.z;
	vec3 ch = abs( Bn - Bo );
	atomicMax( ctl[0], floatBitsToUint( max( max( ch.x, ch.y ), ch.z ) ) );
	atomicMax( ctl[1], floatBitsToUint( max( max( max( Bn.x, Bn.y ), Bn.z ), 0.0 ) ) );
}
)";

// blend: Feed's binding holds the blend entries, FeedStart the blend starts
const char * kGlslBlend = R"(
void main()
{
	int s = int( gl_GlobalInvocationID.x );
	if ( s >= uN )
		return;
	int b = feedStart[s], e = feedStart[s + 1];
	if ( b == e )
		return;
	float acc[18];
	for ( int q = 0; q < 18; q++ )
		acc[q] = 0.0;
	float wsum = 0.0;
	for ( int k = b; k < e; k++ ) {
		int rec = floatBitsToInt( feed[k * 4 + 2] );
		if ( rec >= 0 && doorF( rec, 0, true ) <= 0.0 )
			continue;
		int p = floatBitsToInt( feed[k * 4] );
		float w = feed[k * 4 + 1];
		wsum += w;
		for ( int q = 0; q < 18; q++ )
			acc[q] += w * pe[p * 18 + q];
	}
	int v = slots[s * 2], which = slots[s * 2 + 1];
	int base = which * uNVox * 24;
	for ( int a = 0; a < 6; a++ ) {
		int o = base + ( a * uNVox + v ) * 4;
		for ( int c = 0; c < 3; c++ )
			grid[o + c] = wsum > 0.0 ? acc[a * 3 + c] / wsum : 0.0;
		grid[o + 3] = 1.0;
	}
}
)";

// grow: PairStart holds growStart, LinkStart the grown slots, FeedStart (as ints) the sources
const char * kGlslGrow = R"(
void main()
{
	int g = uFirst + int( gl_GlobalInvocationID.x );
	if ( int( gl_GlobalInvocationID.x ) >= uN )
		return;
	int s = linkStart[g];
	int b = pairStart[g], e = pairStart[g + 1];
	float acc[18];
	for ( int q = 0; q < 18; q++ )
		acc[q] = 0.0;
	for ( int k = b; k < e; k++ ) {
		int q = feedStart[k];
		int base = slots[q * 2 + 1] * uNVox * 24;
		for ( int a = 0; a < 6; a++ ) {
			int o = base + ( a * uNVox + slots[q * 2] ) * 4;
			for ( int c = 0; c < 3; c++ )
				acc[a * 3 + c] += grid[o + c];
		}
	}
	int base = slots[s * 2 + 1] * uNVox * 24;
	float n = float( e - b );
	for ( int a = 0; a < 6; a++ ) {
		int o = base + ( a * uNVox + slots[s * 2] ) * 4;
		for ( int c = 0; c < 3; c++ )
			grid[o + c] = acc[a * 3 + c] / n;
		grid[o + 3] = 1.0;
	}
}
)";

}	// namespace

struct ProbeRelightGpu::Impl
{
	QOffscreenSurface * surface = nullptr;
	QOpenGLContext * ctx = nullptr;
	QOpenGLFunctions_4_3_Core * gl = nullptr;
	QString renderer;
	GLuint prog[5] = { 0, 0, 0, 0, 0 };   // direct, gather, feed, blend, grow
	enum { bSurf, bPairStart, bPairs, bDoorRec, bLights, bCtl, bLinkStart, bLinks, bProbeK, bPE, bBa, bBb, bFeedStart,
		bFeed, bSlots, bGrid, bBlendStart, bBlend, bGrowStart, bGrowSlot, bGrowSrc, bCount };
	GLuint buf[bCount] = {};
	const ProbeRelightOps * ops = nullptr;
	int nS = 0, nP = 0, nSlot = 0, nGrow = 0, nDoor = 0;
	double msUp = 0;
	QOpenGLContext * prevCtx = nullptr;
	QSurface * prevSurf = nullptr;

	bool current()
	{
		prevCtx = QOpenGLContext::currentContext();
		prevSurf = prevCtx ? prevCtx->surface() : nullptr;
		return ctx && ctx->makeCurrent( surface );
	}
	void restore()
	{
		if ( ctx )
			ctx->doneCurrent();
		if ( prevCtx && prevSurf )
			prevCtx->makeCurrent( prevSurf );
		prevCtx = nullptr;
		prevSurf = nullptr;
	}
	void data( int b, const void * p, size_t bytes )
	{
		gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, buf[b] );
		gl->glBufferData( GL_SHADER_STORAGE_BUFFER, GLsizeiptr( std::max<size_t>( bytes, 16 ) ), nullptr, GL_DYNAMIC_DRAW );
		if ( bytes && p )
			gl->glBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, GLsizeiptr( bytes ), p );
	}
	void bind( int binding, int b ) { gl->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, GLuint( binding ), buf[b] ); }
	GLint loc( int p, const char * n ) { return gl->glGetUniformLocation( prog[p], n ); }
	void dispatch( int n )
	{
		if ( n > 0 )
			gl->glDispatchCompute( GLuint( ( n + 63 ) / 64 ), 1, 1 );
		gl->glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT );
	}
};

ProbeRelightGpu::ProbeRelightGpu() : d( new Impl ) {}

ProbeRelightGpu::~ProbeRelightGpu()
{
	if ( d->ctx && d->gl && d->current() ) {
		d->gl->glDeleteBuffers( Impl::bCount, d->buf );
		for ( GLuint p : d->prog )
			if ( p )
				d->gl->glDeleteProgram( p );
		d->restore();
	}
	delete d->ctx;
	delete d->surface;
	delete d;
}

QString ProbeRelightGpu::renderer() const { return d->renderer; }
double ProbeRelightGpu::msUpload() const { return d->msUp; }

bool ProbeRelightGpu::init( QString * why )
{
	if ( !qobject_cast<QGuiApplication *>( QCoreApplication::instance() ) ) {
		*why = QStringLiteral( "no GUI platform in this process" );
		return false;
	}
	QSurfaceFormat fmt;
	fmt.setVersion( 4, 3 );
	fmt.setProfile( QSurfaceFormat::CoreProfile );
	d->surface = new QOffscreenSurface();
	d->surface->setFormat( fmt );
	d->surface->create();
	if ( !d->surface->isValid() ) {
		*why = QStringLiteral( "no offscreen surface" );
		return false;
	}
	d->ctx = new QOpenGLContext();
	d->ctx->setFormat( fmt );
	if ( !d->ctx->create() ) {
		*why = QStringLiteral( "the driver made no OpenGL context" );
		return false;
	}
	const QSurfaceFormat got = d->ctx->format();
	if ( got.majorVersion() * 10 + got.minorVersion() < 43 ) {
		*why = QStringLiteral( "OpenGL %1.%2 < 4.3" ).arg( got.majorVersion() ).arg( got.minorVersion() );
		return false;
	}
	if ( !d->current() ) {
		*why = QStringLiteral( "makeCurrent failed" );
		return false;
	}
	d->gl = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_4_3_Core>( d->ctx );
	if ( !d->gl || !d->gl->initializeOpenGLFunctions() ) {
		*why = QStringLiteral( "no OpenGL 4.3 core functions" );
		d->restore();
		return false;
	}
	auto * gl = d->gl;
	d->renderer = QString::fromLatin1( reinterpret_cast<const char *>( gl->glGetString( GL_RENDERER ) ) );
	const char * bodies[5] = { kGlslDirect, kGlslGather, kGlslFeed, kGlslBlend, kGlslGrow };
	for ( int p = 0; p < 5; p++ ) {
		const QByteArray src = QByteArray( kGlslHead ) + bodies[p];
		const char * s = src.constData();
		GLuint sh = gl->glCreateShader( GL_COMPUTE_SHADER );
		gl->glShaderSource( sh, 1, &s, nullptr );
		gl->glCompileShader( sh );
		GLint ok = 0;
		gl->glGetShaderiv( sh, GL_COMPILE_STATUS, &ok );
		if ( !ok ) {
			char log[2048] = {};
			gl->glGetShaderInfoLog( sh, sizeof( log ) - 1, nullptr, log );
			*why = QStringLiteral( "kernel %1 did not compile: %2" ).arg( p ).arg( QString::fromLatin1( log ).left( 600 ).simplified() );
			gl->glDeleteShader( sh );
			d->restore();
			return false;
		}
		d->prog[p] = gl->glCreateProgram();
		gl->glAttachShader( d->prog[p], sh );
		gl->glLinkProgram( d->prog[p] );
		gl->glDeleteShader( sh );
		gl->glGetProgramiv( d->prog[p], GL_LINK_STATUS, &ok );
		if ( !ok ) {
			char log[2048] = {};
			gl->glGetProgramInfoLog( d->prog[p], sizeof( log ) - 1, nullptr, log );
			*why = QStringLiteral( "kernel %1 did not link: %2" ).arg( p ).arg( QString::fromLatin1( log ).left( 600 ).simplified() );
			d->restore();
			return false;
		}
	}
	gl->glGenBuffers( Impl::bCount, d->buf );
	const bool ok = gl->glGetError() == GL_NO_ERROR;
	d->restore();
	if ( !ok )
		*why = QStringLiteral( "a GL error in setup" );
	return ok;
}

bool ProbeRelightGpu::upload( const ProbeRelightOps & X, QString * why )
{
	if ( !X.built ) {
		*why = QStringLiteral( "no operators" );
		return false;
	}
	QElapsedTimer clock;
	clock.start();
	if ( !d->current() ) {
		*why = QStringLiteral( "makeCurrent failed" );
		return false;
	}
	d->ops = &X;
	d->nS = X.surfels;
	d->nP = X.probes;
	d->nSlot = int( X.slotVox.size() );
	d->nGrow = int( X.growSlot.size() );
	d->nDoor = int( X.doorRefs.size() );
	auto fbits = []( int v ) { float f; std::memcpy( &f, &v, 4 ); return f; };
	// the door records, one table for every kind of entry
	std::vector<float> rec;
	auto addRec = [&]( const int * door, const float * T ) -> int {
		if ( door[0] < 0 )
			return -1;
		const int r = int( rec.size() / 12 );
		rec.insert( rec.end(), { fbits( door[0] ), fbits( door[1] ), 0.0f, 0.0f } );
		rec.insert( rec.end(), T, T + 8 );
		return r;
	};
	std::vector<float> surf( size_t( d->nS ) * 16, 0.0f );
	for ( int i = 0; i < d->nS; i++ ) {
		float * s = &surf[size_t( i ) * 16];
		for ( int c = 0; c < 3; c++ ) {
			s[c] = float( X.nrm[size_t( i ) * 3 + size_t( c )] );
			s[4 + c] = float( X.alb[size_t( i ) * 3 + size_t( c )] );
			s[8 + c] = float( X.le[size_t( i ) * 3 + size_t( c )] );
		}
		s[3] = X.hasDir ? float( X.dirK[size_t( i )] ) : 0.0f;
		s[7] = X.sunOn ? float( X.sunK[size_t( i )] ) : 0.0f;
	}
	std::vector<float> pairs( X.pairLight.size() * 8, 0.0f );
	for ( size_t k = 0; k < X.pairLight.size(); k++ ) {
		float * p = &pairs[k * 8];
		p[0] = fbits( X.pairLight[k] );
		p[1] = float( X.pairD[k] );
		p[2] = float( X.pairNL[k] );
		p[3] = float( X.pairDL[k] );
		p[4] = fbits( addRec( &X.pairDoor[k * 2], &X.pairT[k * 8] ) );
	}
	std::vector<float> links( X.linkSurf.size() * 12, 0.0f );
	for ( size_t k = 0; k < X.linkSurf.size(); k++ ) {
		float * l = &links[k * 12];
		l[0] = fbits( X.linkSurf[k] );
		l[1] = float( X.linkOmega[k] );
		for ( int c = 0; c < 3; c++ )
			l[2 + c] = float( X.linkTint[k * 3 + size_t( c )] );
		for ( int a = 0; a < 6; a++ )
			l[5 + a] = float( X.linkCos[k * 6 + size_t( a )] );
		l[11] = fbits( addRec( &X.linkDoor[k * 2], &X.linkT[k * 8] ) );
	}
	std::vector<float> pk( size_t( d->nP ) * 20, 0.0f );
	for ( int j = 0; j < d->nP; j++ ) {
		pk[size_t( j ) * 20] = float( X.kUnl[size_t( j )] );
		for ( int q = 0; q < 18; q++ )
			pk[size_t( j ) * 20 + 2 + size_t( q )] = float( X.skyE[size_t( j ) * 18 + size_t( q )] );
	}
	std::vector<float> feed( X.feedProbe.size() * 4, 0.0f );
	for ( size_t k = 0; k < X.feedProbe.size(); k++ ) {
		feed[k * 4] = fbits( X.feedProbe[k] );
		feed[k * 4 + 1] = float( X.feedW[k] );
		feed[k * 4 + 2] = fbits( addRec( &X.feedDoor[k * 2], &X.feedT[k * 8] ) );
	}
	std::vector<float> blend( X.blendProbe.size() * 4, 0.0f );
	for ( size_t k = 0; k < X.blendProbe.size(); k++ ) {
		blend[k * 4] = fbits( X.blendProbe[k] );
		blend[k * 4 + 1] = float( X.blendW[k] );
		blend[k * 4 + 2] = fbits( addRec( &X.blendDoor[k * 2], &X.blendT[k * 8] ) );
	}
	std::vector<int> slotv( size_t( d->nSlot ) * 2 );
	for ( int s = 0; s < d->nSlot; s++ ) {
		slotv[size_t( s ) * 2] = X.slotVox[size_t( s )];
		slotv[size_t( s ) * 2 + 1] = X.slotWhich[size_t( s )];
	}
	d->data( Impl::bSurf, surf.data(), surf.size() * 4 );
	d->data( Impl::bPairStart, X.pairStart.data(), X.pairStart.size() * 4 );
	d->data( Impl::bPairs, pairs.data(), pairs.size() * 4 );
	d->data( Impl::bDoorRec, rec.data(), rec.size() * 4 );
	d->data( Impl::bLights, nullptr, X.lights.size() * 64 );
	d->data( Impl::bCtl, nullptr, ( 2 + X.doorRefs.size() ) * 4 );
	d->data( Impl::bLinkStart, X.linkStart.data(), X.linkStart.size() * 4 );
	d->data( Impl::bLinks, links.data(), links.size() * 4 );
	d->data( Impl::bProbeK, pk.data(), pk.size() * 4 );
	d->data( Impl::bPE, nullptr, size_t( d->nP ) * 18 * 4 );
	d->data( Impl::bBa, nullptr, size_t( d->nS ) * 16 );
	d->data( Impl::bBb, nullptr, size_t( d->nS ) * 16 );
	d->data( Impl::bFeedStart, X.feedStart.data(), X.feedStart.size() * 4 );
	d->data( Impl::bFeed, feed.data(), feed.size() * 4 );
	d->data( Impl::bSlots, slotv.data(), slotv.size() * 4 );
	{
		std::vector<float> zero( X.nVox * 48, 0.0f );
		d->data( Impl::bGrid, zero.data(), zero.size() * 4 );
	}
	d->data( Impl::bBlendStart, X.blendStart.data(), X.blendStart.size() * 4 );
	d->data( Impl::bBlend, blend.data(), blend.size() * 4 );
	d->data( Impl::bGrowStart, X.growStart.data(), X.growStart.size() * 4 );
	d->data( Impl::bGrowSlot, X.growSlot.data(), X.growSlot.size() * 4 );
	d->data( Impl::bGrowSrc, X.growSrc.data(), X.growSrc.size() * 4 );
	d->gl->glFinish();
	const bool ok = d->gl->glGetError() == GL_NO_ERROR;
	d->restore();
	d->msUp = clock.nsecsElapsed() / 1e6;
	if ( !ok )
		*why = QStringLiteral( "a GL error in the upload (out of memory?)" );
	return ok;
}

bool ProbeRelightGpu::run( const ProbeRelightState & st, ProbeRelightOut * out, QString * why, bool readGrid )
{
	const ProbeRelightOps * Xp = d->ops;
	if ( !Xp || !d->gl ) {
		*why = QStringLiteral( "nothing uploaded" );
		return false;
	}
	const ProbeRelightOps & X = *Xp;
	QElapsedTimer clock, k;
	clock.start();
	ProbeRelightOut & O = *out;
	O = ProbeRelightOut();
	if ( !d->current() ) {
		*why = QStringLiteral( "makeCurrent failed" );
		return false;
	}
	auto * gl = d->gl;
	const size_t nL = X.lights.size();
	std::vector<float> lights( nL * 16, 0.0f );
	for ( size_t l = 0; l < nL; l++ ) {
		const ProbeRelightOps::Light & L = X.lights[l];
		float * p = &lights[l * 16];
		for ( int c = 0; c < 3; c++ ) {
			p[c] = L.pos[c];
			p[4 + c] = st.color.size() == nL * 3 ? st.color[l * 3 + size_t( c )] : ( L.onAtStart ? L.color[c] : 0.0f );
		}
		float r = L.radius;
		if ( st.radius.size() == nL ) {
			r = st.radius[l];
			if ( r > L.radius ) {
				r = L.radius;
				O.radiusClamped++;
			}
		}
		p[3] = r;
		p[7] = L.bias;
		p[8] = L.scale;
		p[9] = L.exponent;
		p[10] = L.cone;
		p[11] = L.cosOuter;
		p[12] = L.spot ? 1.0f : 0.0f;
	}
	gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, d->buf[Impl::bLights] );
	if ( !lights.empty() )
		gl->glBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, GLsizeiptr( lights.size() * 4 ), lights.data() );
	std::vector<quint32> ctl( 2 + X.doorRefs.size(), 0u );
	for ( size_t i = 0; i < X.doorRefs.size() && i < st.doorClosed.size(); i++ )
		ctl[2 + i] = st.doorClosed[i] ? 1u : 0u;
	gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, d->buf[Impl::bCtl] );
	gl->glBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, GLsizeiptr( ctl.size() * 4 ), ctl.data() );
	const int red = redMode( st.red );
	float sun[3];
	for ( int c = 0; c < 3; c++ )
		sun[c] = st.sun[0] >= 0.0f ? st.sun[c] : float( X.sun[c] );
	auto common = [&]( int p, int n ) {
		gl->glUseProgram( d->prog[p] );
		gl->glUniform1i( d->loc( p, "uN" ), n );
		gl->glUniform1i( d->loc( p, "uRed" ), red );
		gl->glUniform1i( d->loc( p, "uHasDir" ), X.hasDir ? 1 : 0 );
		gl->glUniform3f( d->loc( p, "uDirColor" ), float( X.dirColor[0] ), float( X.dirColor[1] ), float( X.dirColor[2] ) );
		gl->glUniform1i( d->loc( p, "uSunOn" ), X.sunOn ? 1 : 0 );
		gl->glUniform3f( d->loc( p, "uSun" ), sun[0], sun[1], sun[2] );
		gl->glUniform1i( d->loc( p, "uSkyOn" ), X.skyOn ? 1 : 0 );
		gl->glUniform1i( d->loc( p, "uRedGrow" ), X.redGrow ? 1 : 0 );
		gl->glUniform1i( d->loc( p, "uNVox" ), int( X.nVox ) );
		gl->glUniform1i( d->loc( p, "uFirst" ), 0 );
	};
	auto lap = [&]( int slot ) {
		gl->glFinish();
		O.msKernel[slot] += k.nsecsElapsed() / 1e6;
		k.restart();
	};
	k.start();
	// 1. direct -> surf.B1, Ba
	d->bind( 0, Impl::bSurf );
	d->bind( 1, Impl::bPairStart );
	d->bind( 2, Impl::bPairs );
	d->bind( 3, Impl::bDoorRec );
	d->bind( 4, Impl::bLights );
	d->bind( 5, Impl::bCtl );
	d->bind( 10, Impl::bBa );
	common( 0, d->nS );
	d->dispatch( d->nS );
	lap( 0 );
	// 2. gather from Ba
	d->bind( 6, Impl::bLinkStart );
	d->bind( 7, Impl::bLinks );
	d->bind( 8, Impl::bProbeK );
	d->bind( 9, Impl::bPE );
	common( 1, d->nP );
	d->dispatch( d->nP );
	lap( 1 );
	// 3. passes
	bool fixed = X.fixedPasses;
	int cap = X.maxPasses;
	if ( st.passes > 0 ) {
		fixed = true;
		cap = st.passes;
	}
	if ( hasToken( st.red, "nobounce" ) ) {
		fixed = true;
		cap = 1;
	}
	O.passes = 1;
	O.settled = fixed;
	int src = Impl::bBa, dst = Impl::bBb;
	d->bind( 12, Impl::bFeedStart );
	d->bind( 13, Impl::bFeed );
	for ( int pass = 2; pass <= cap; pass++ ) {
		const quint32 zero2[2] = { 0u, 0u };
		gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, d->buf[Impl::bCtl] );
		gl->glBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, 8, zero2 );
		d->bind( 10, src );
		d->bind( 11, dst );
		common( 2, d->nS );
		d->dispatch( d->nS );
		lap( 2 );
		std::swap( src, dst );
		d->bind( 10, src );
		common( 1, d->nP );
		d->dispatch( d->nP );
		lap( 1 );
		O.passes = pass;
		if ( !fixed ) {
			quint32 r2[2] = { 0, 0 };
			gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, d->buf[Impl::bCtl] );
			gl->glGetBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, 8, r2 );
			float ch, mx;
			std::memcpy( &ch, &r2[0], 4 );
			std::memcpy( &mx, &r2[1], 4 );
			O.passLog.insert( O.passLog.end(), { double( ch ), 0.0, double( mx ) } );
			lap( 2 );
			if ( double( ch ) <= X.settle * double( mx ) ) {
				O.settled = true;
				break;
			}
		}
	}
	// 4. the grid: blend (the feed bindings hold the blend lists), then the two rings
	d->bind( 12, Impl::bBlendStart );
	d->bind( 13, Impl::bBlend );
	d->bind( 14, Impl::bSlots );
	d->bind( 15, Impl::bGrid );
	common( 3, d->nSlot );
	d->dispatch( d->nSlot );
	d->bind( 1, Impl::bGrowStart );
	d->bind( 6, Impl::bGrowSlot );
	d->bind( 12, Impl::bGrowSrc );
	for ( int ring = 0; ring < 2; ring++ ) {
		const int g0 = ring ? X.growRing0 : 0, g1 = ring ? d->nGrow : X.growRing0;
		if ( g1 <= g0 )
			continue;
		common( 4, g1 - g0 );
		gl->glUniform1i( d->loc( 4, "uFirst" ), g0 );
		d->dispatch( g1 - g0 );
	}
	lap( 3 );
	// readback
	std::vector<float> s16( size_t( d->nS ) * 16 ), b4( size_t( d->nS ) * 4 );
	gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, d->buf[Impl::bSurf] );
	gl->glGetBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, GLsizeiptr( s16.size() * 4 ), s16.data() );
	gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, d->buf[src] );
	gl->glGetBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, GLsizeiptr( b4.size() * 4 ), b4.data() );
	O.B1.resize( size_t( d->nS ) * 3 );
	O.B.resize( size_t( d->nS ) * 3 );
	for ( int i = 0; i < d->nS; i++ )
		for ( int c = 0; c < 3; c++ ) {
			O.B1[size_t( i ) * 3 + size_t( c )] = s16[size_t( i ) * 16 + 12 + size_t( c )];
			O.B[size_t( i ) * 3 + size_t( c )] = b4[size_t( i ) * 4 + size_t( c )];
		}
	O.E.resize( size_t( d->nP ) * 18 );
	gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, d->buf[Impl::bPE] );
	gl->glGetBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, GLsizeiptr( O.E.size() * 4 ), O.E.data() );
	if ( readGrid ) {
		std::vector<float> g( X.nVox * 48 );
		gl->glBindBuffer( GL_SHADER_STORAGE_BUFFER, d->buf[Impl::bGrid] );
		gl->glGetBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, GLsizeiptr( g.size() * 4 ), g.data() );
		O.grid.assign( g.begin(), g.begin() + std::ptrdiff_t( X.nVox * 24 ) );
		O.grid2.assign( g.begin() + std::ptrdiff_t( X.nVox * 24 ), g.end() );
	}
	lap( 4 );
	const GLenum e = gl->glGetError();
	d->restore();
	O.ms = clock.nsecsElapsed() / 1e6;
	if ( e != GL_NO_ERROR ) {
		*why = QStringLiteral( "GL error 0x%1" ).arg( e, 0, 16 );
		return false;
	}
	return true;
}

// ======================================================================== the shared light record

namespace {

struct Bytes
{
	QByteArray b;
	void raw( const void * p, int n ) { b.append( reinterpret_cast<const char *>( p ), n ); }
	void u8( quint8 v ) { raw( &v, 1 ); }
	void u16( quint16 v ) { v = qToLittleEndian( v ); raw( &v, 2 ); }
	void u32( quint32 v ) { v = qToLittleEndian( v ); raw( &v, 4 ); }
	void i32( qint32 v ) { u32( quint32( v ) ); }
	void u64( quint64 v ) { v = qToLittleEndian( v ); raw( &v, 8 ); }
	void f32( float v ) { quint32 u; std::memcpy( &u, &v, 4 ); u32( u ); }
	void f16( float v ) { const qfloat16 h( v ); quint16 u; std::memcpy( &u, &h, 2 ); u16( u ); }
	void zero( int n ) { b.append( n, '\0' ); }
};

quint64 fnv1a( const QByteArray & b, quint64 h = 1469598103934665603ull )
{
	for ( char c : b ) {
		h ^= quint8( c );
		h *= 1099511628211ull;
	}
	return h;
}

//! 2 x snorm16 octahedral (x low, y high)
quint32 octDir( const float d[3] )
{
	double x = d[0], y = d[1], z = d[2];
	const double s = std::fabs( x ) + std::fabs( y ) + std::fabs( z );
	if ( s <= 0.0 )
		return 0;
	x /= s;
	y /= s;
	z /= s;
	if ( z < 0.0 ) {
		const double ox = ( 1.0 - std::fabs( y ) ) * ( x >= 0.0 ? 1.0 : -1.0 );
		const double oy = ( 1.0 - std::fabs( x ) ) * ( y >= 0.0 ? 1.0 : -1.0 );
		x = ox;
		y = oy;
	}
	auto sn = []( double v ) { return quint32( quint16( qint16( std::lround( std::min( std::max( v, -1.0 ), 1.0 ) * 32767.0 ) ) ) ); };
	return sn( x ) | ( sn( y ) << 16 );
}

quint8 tByte( float t ) { return quint8( std::lround( std::min( std::max( double( t ), 0.0 ), 1.0 ) * 255.0 ) ); }

bool writeFile( const QString & path, const QByteArray & b, QString * err )
{
	QFile fo( path );
	if ( !fo.open( QIODevice::WriteOnly ) || fo.write( b ) != b.size() ) {
		*err = QStringLiteral( "cannot write %1" ).arg( path );
		return false;
	}
	return true;
}

}	// namespace

bool probeRelightWriteRecords( const ProbeRelightOps & X, const QString & dir, const QStringList & plugins, QString * err,
	QString * census )
{
	if ( !X.built ) {
		*err = QStringLiteral( "no operators recorded" );
		return false;
	}
	QDir().mkpath( dir );
	const QRegularExpression re( QStringLiteral( "sector_([+-]?\\d+)_([+-]?\\d+)\\.tbk$" ) );
	qint64 pairsAll = 0, bytesAll = 0;
	int sectors = 0;
	QStringList lines;
	for ( int f = 0; f < X.files.size(); f++ ) {
		const QRegularExpressionMatch m = re.match( X.files[f] );
		if ( !m.hasMatch() )
			continue;
		const int sx = m.captured( 1 ).toInt(), sy = m.captured( 2 ).toInt();
		// the sector's surfels and the lights that reach them (or stand in it)
		std::vector<quint8> inSec( X.lights.size(), 0 );
		int owned = 0;
		for ( int i = 0; i < X.surfels; i++ ) {
			if ( X.sid[size_t( i ) * 2] != f )
				continue;
			owned++;
			for ( int k = X.pairStart[size_t( i )]; k < X.pairStart[size_t( i ) + 1]; k++ )
				inSec[size_t( X.pairLight[size_t( k )] )] = 1;
		}
		for ( size_t l = 0; l < X.lights.size(); l++ )
			if ( int( std::floor( X.lights[l].pos[0] / 4096.0 ) ) == sx && int( std::floor( X.lights[l].pos[1] / 4096.0 ) ) == sy )
				inSec[l] = 1;
		// groups (key ascending: ALWAYS = 0 first), lights by group, then form id, then their order
		std::vector<quint64> keys;
		std::vector<int> lit;
		for ( size_t l = 0; l < X.lights.size(); l++ )
			if ( inSec[l] ) {
				lit.push_back( int( l ) );
				keys.push_back( X.lights[l].groupKey );
			}
		std::sort( keys.begin(), keys.end() );
		keys.erase( std::unique( keys.begin(), keys.end() ), keys.end() );
		auto gOf = [&]( int l ) {
			return int( std::lower_bound( keys.begin(), keys.end(), X.lights[size_t( l )].groupKey ) - keys.begin() );
		};
		std::stable_sort( lit.begin(), lit.end(), [&]( int a, int b ) {
			const int ga = gOf( a ), gb = gOf( b );
			if ( ga != gb )
				return ga < gb;
			return ( X.lights[size_t( a )].ref & 0xFFFFFFu ) < ( X.lights[size_t( b )].ref & 0xFFFFFFu );
		} );
		std::vector<int> wltOf( X.lights.size(), -1 );
		for ( size_t k = 0; k < lit.size(); k++ )
			wltOf[size_t( lit[k] )] = int( k );
		Bytes groups, lights, table;
		for ( size_t g = 0; g < keys.size(); g++ ) {
			int first = -1, count = 0;
			bool on = false;
			for ( size_t k = 0; k < lit.size(); k++ )
				if ( gOf( lit[k] ) == int( g ) ) {
					if ( first < 0 ) {
						first = int( k );
						on = X.lights[size_t( lit[k] )].onAtStart;
					}
					count++;
				}
			groups.u64( keys[g] );
			groups.u16( quint16( first ) );
			groups.u16( quint16( count ) );
			groups.u8( quint8( keys[g] >> 62 ) );
			groups.u8( on ? 1 : 0 );
			groups.u16( 0 );
		}
		for ( int l : lit ) {
			const ProbeRelightOps::Light & L = X.lights[size_t( l )];
			const quint64 refKey = quint64( L.ref & 0xFFFFFFu ) | ( quint64( L.ref >> 24 ) << 24 );
			lights.u64( refKey );
			for ( float p : L.pos )
				lights.f32( p );
			lights.f32( L.radius );
			for ( float c : L.color )
				lights.f32( c );
			lights.f32( L.bias );
			lights.f32( L.scale );
			lights.f32( L.exponent );
			lights.u32( L.spot ? octDir( L.dir ) : 0u );
			lights.f16( L.cosOuter );
			lights.f16( L.cone );
			lights.u16( quint16( gOf( l ) ) );
			lights.u16( L.flags );
			lights.u32( 0 );   // dot RGB9E5: no dot here (flag 32 unset)
		}
		for ( const QString & p : plugins ) {
			QByteArray n = p.toUtf8().left( 63 );
			n.append( 64 - n.size(), '\0' );
			table.b.append( n );
		}
		const quint64 hash = fnv1a( lights.b, fnv1a( groups.b, fnv1a( table.b ) ) );
		Bytes wlt;
		wlt.raw( "WLT1", 4 );
		wlt.u32( 1 );
		wlt.i32( sx );
		wlt.i32( sy );
		wlt.u32( quint32( lit.size() ) );
		wlt.u32( quint32( keys.size() ) );
		wlt.u32( quint32( plugins.size() ) );
		wlt.u32( quint32( 64 + groups.b.size() + lights.b.size() ) );
		wlt.u64( hash );
		wlt.u32( 1u );   // interior (the relight runs interiors)
		wlt.zero( 20 );
		wlt.b += groups.b + lights.b + table.b;
		// the pairs, by .wlt light, then surfel id
		struct P { int l; quint32 sid; int k; };
		std::vector<P> ps;
		for ( int i = 0; i < X.surfels; i++ ) {
			if ( X.sid[size_t( i ) * 2] != f )
				continue;
			for ( int k = X.pairStart[size_t( i )]; k < X.pairStart[size_t( i ) + 1]; k++ )
				ps.push_back( { wltOf[size_t( X.pairLight[size_t( k )] )], quint32( X.sid[size_t( i ) * 2 + 1] ), k } );
		}
		std::sort( ps.begin(), ps.end(), []( const P & a, const P & b ) { return a.l != b.l ? a.l < b.l : a.sid < b.sid; } );
		Bytes wlp;
		wlp.raw( "WLP1", 4 );
		wlp.u32( 1 );
		wlp.i32( sx );
		wlp.i32( sy );
		wlp.u32( quint32( lit.size() ) );
		wlp.u32( quint32( ps.size() ) );
		wlp.u64( hash );
		wlp.u32( quint32( owned ) );
		wlp.u32( X.doorRefs.empty() ? 0u : 1u );
		wlp.u32( quint32( X.doorRefs.size() ) );
		wlp.zero( 20 );
		size_t at = 0;
		for ( size_t l = 0; l <= lit.size(); l++ ) {
			while ( at < ps.size() && size_t( ps[at].l ) < l )
				at++;
			wlp.u32( quint32( at ) );
		}
		for ( const P & p : ps ) {
			const size_t k = size_t( p.k );
			wlp.u32( p.sid );
			wlp.f32( float( X.pairD[k] ) );
			wlp.f16( float( X.pairNL[k] ) );
			wlp.f16( float( X.pairDL[k] ) );
			for ( int s = 0; s < 2; s++ )
				wlp.u16( X.pairDoor[k * 2 + size_t( s )] < 0 ? 0xFFFFu : quint16( X.pairDoor[k * 2 + size_t( s )] ) );
			for ( int s = 0; s < 2; s++ )
				for ( int c = 0; c < 3; c++ )
					wlp.u8( tByte( X.pairT[k * 8 + size_t( s ) * 4 + size_t( c )] ) );
			wlp.zero( 2 );
		}
		for ( quint32 r : X.doorRefs )
			wlp.u32( r );
		char name[64];
		std::snprintf( name, sizeof name, "%+05d_%+05d", sx, sy );
		const QString base = QString::fromLatin1( name );
		if ( !writeFile( QDir( dir ).filePath( QStringLiteral( "lights_%1.wlt" ).arg( base ) ), wlt.b, err )
			|| !writeFile( QDir( dir ).filePath( QStringLiteral( "relight_%1.wlp" ).arg( base ) ), wlp.b, err ) )
			return false;
		sectors++;
		pairsAll += qint64( ps.size() );
		bytesAll += wlt.b.size() + wlp.b.size();
		lines << QStringLiteral( "%1: %2 lights in %3 groups, %4 pairs over %5 surfels" ).arg( base ).arg( lit.size() )
					 .arg( keys.size() ).arg( ps.size() ).arg( owned );
	}
	*census = QStringLiteral( "gpurelight records: %1 sectors, %2 pairs, %3 bytes (%4)" ).arg( sectors ).arg( pairsAll )
				  .arg( bytesAll ).arg( lines.join( QStringLiteral( "; " ) ) );
	return true;
}

// ======================================================================== the gate: `gpurelight`

namespace {

//! a box's 12 triangles, each wound to face out of the box
void addBox( std::vector<float> & out, const double lo[3], const double hi[3] )
{
	const double c[3] = { ( lo[0] + hi[0] ) * 0.5, ( lo[1] + hi[1] ) * 0.5, ( lo[2] + hi[2] ) * 0.5 };
	for ( int ax = 0; ax < 3; ax++ )
		for ( int side = 0; side < 2; side++ ) {
			const int u = ( ax + 1 ) % 3, v = ( ax + 2 ) % 3;
			double q[4][3];
			const double uu[4] = { lo[u], hi[u], hi[u], lo[u] }, vv[4] = { lo[v], lo[v], hi[v], hi[v] };
			for ( int k = 0; k < 4; k++ ) {
				q[k][ax] = side ? hi[ax] : lo[ax];
				q[k][u] = uu[k];
				q[k][v] = vv[k];
			}
			const int tri[2][3] = { { 0, 1, 2 }, { 0, 2, 3 } };
			for ( const auto & t : tri ) {
				const double * a = q[t[0]], * b = q[t[1]], * d = q[t[2]];
				const double e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] }, e2[3] = { d[0] - a[0], d[1] - a[1], d[2] - a[2] };
				const double n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
				const double out_ = ( a[0] - c[0] ) * n[0] + ( a[1] - c[1] ) * n[1] + ( a[2] - c[2] ) * n[2];
				const double * order[3] = { a, b, d };
				if ( out_ < 0.0 )
					std::swap( order[1], order[2] );
				for ( const double * p : order )
					for ( int k = 0; k < 3; k++ )
						out.push_back( float( p[k] ) );
			}
		}
}

void addBox( std::vector<float> & out, double x0, double y0, double z0, double x1, double y1, double z1 )
{
	const double lo[3] = { x0, y0, z0 }, hi[3] = { x1, y1, z1 };
	addBox( out, lo, hi );
}

/*! The synthetic scene: two rooms (A: x 0..512, B: x 528..1040; y 0..768, z 0..256) behind a 16-unit middle wall
 *  with three doorways (96 wide, 192 high, centred at y 128, 384, 640). Each doorway holds a door (closed, as
 *  placed): 0 a solid slab, 1 a slab with an alpha-tested hole (the ALPHATEST1 mask on its two broad faces),
 *  2 a frame around a glass pane (tint 60, 200, 110). */
struct RelightScene
{
	ProbeSoup soup;
	double holeLo[2], holeHi[2];   //!< door 1's hole: y, z
	quint8 tint[3] = { 60, 200, 110 };
};

const double kDoorY[3] = { 128.0, 384.0, 640.0 };

void buildScene( RelightScene & S )
{
	ProbeSoup & s = S.soup;
	std::vector<float> t;
	addBox( t, -16, -16, -16, 1056, 784, 0 );     // floor
	addBox( t, -16, -16, 256, 1056, 784, 272 );   // ceiling
	addBox( t, -16, -16, 0, 0, 784, 256 );        // west
	addBox( t, 1040, -16, 0, 1056, 784, 256 );    // east
	addBox( t, 0, -16, 0, 1040, 0, 256 );         // south
	addBox( t, 0, 768, 0, 1040, 784, 256 );       // north
	double y = 0.0;
	for ( double c : kDoorY ) {   // the middle wall between the doorways, and the lintels
		addBox( t, 512, y, 0, 528, c - 48.0, 256 );
		addBox( t, 512, c - 48.0, 192, 528, c + 48.0, 256 );
		y = c + 48.0;
	}
	addBox( t, 512, y, 0, 528, 768, 256 );
	const quint8 grey[3] = { 170, 170, 170 };
	for ( size_t i = 0; i * 9 < t.size(); i++ )
		s.addTri( &t[i * 9], &t[i * 9 + 3], &t[i * 9 + 6], grey );
	for ( int d = 0; d < 3; d++ ) {
		ProbeSoup::Door door;
		door.ref = 0x0100A001u + quint32( d );
		door.lo[0] = 512.0f;
		door.hi[0] = 528.0f;
		door.lo[1] = float( kDoorY[d] - 48.0 );
		door.hi[1] = float( kDoorY[d] + 48.0 );
		door.lo[2] = 0.0f;
		door.hi[2] = 192.0f;
		s.doors.push_back( door );
	}
	ProbeSoup::DoorGeom & g = s.doorGeom;
	auto addDoorBox = [&]( int d, double x0, double y0, double z0, double x1, double y1, double z1 ) {
		std::vector<float> b;
		addBox( b, x0, y0, z0, x1, y1, z1 );
		g.tris.insert( g.tris.end(), b.begin(), b.end() );
		g.door.insert( g.door.end(), b.size() / 9, d );
	};
	// door 0: solid
	addDoorBox( 0, 517, kDoorY[0] - 48.0, 0, 523, kDoorY[0] + 48.0, 192 );
	// door 1: a slab whose two broad faces carry the hole's mask (u = (y - y0) / 96, v = 1 - z / 192)
	{
		const double y0 = kDoorY[1] - 48.0;
		const size_t first = g.tris.size() / 9;
		addDoorBox( 1, 517, y0, 0, 523, y0 + 96.0, 192 );
		probebvh::AlphaMask::Map map;
		map.w = map.h = 64;
		map.a.assign( 64 * 64, 255 );
		for ( int yy = 0; yy < 64; yy++ )
			for ( int xx = 0; xx < 64; xx++ ) {
				const double u = ( xx + 0.5 ) / 64.0, v = ( yy + 0.5 ) / 64.0;
				if ( u > 0.2 && u < 0.8 && v > 0.1 && v < 0.55 )
					map.a[size_t( yy * 64 + xx )] = 0;
			}
		g.amask.maps.push_back( map );
		g.amask.mapNames.push_back( "synthetic/door_hole_a.dds" );
		g.amask.models.push_back( "synthetic/door_hole.nif" );
		g.amask.triOf.assign( g.tris.size() / 9, -1 );
		for ( size_t i = first; i * 9 < g.tris.size(); i++ ) {
			const float * p = &g.tris[i * 9];
			if ( !( p[0] == p[3] && p[3] == p[6] ) )   // the broad faces only (constant x)
				continue;
			probebvh::AlphaMask::Tri mt;
			mt.map = 0;
			mt.model = 0;
			mt.thr = 128;
			for ( int v = 0; v < 3; v++ ) {
				mt.uv[v * 2] = float( ( p[v * 3 + 1] - y0 ) / 96.0 );
				mt.uv[v * 2 + 1] = float( 1.0 - p[v * 3 + 2] / 192.0 );
			}
			g.amask.triOf[i] = int( g.amask.tris.size() );
			g.amask.tris.push_back( mt );
		}
		S.holeLo[0] = y0 + 0.2 * 96.0;
		S.holeHi[0] = y0 + 0.8 * 96.0;
		S.holeLo[1] = ( 1.0 - 0.55 ) * 192.0;
		S.holeHi[1] = ( 1.0 - 0.1 ) * 192.0;
	}
	// door 2: a frame around a pane
	{
		const double y0 = kDoorY[2] - 48.0, y1 = kDoorY[2] + 48.0;
		addDoorBox( 2, 517, y0, 0, 523, 612, 192 );
		addDoorBox( 2, 517, 668, 0, 523, y1, 192 );
		addDoorBox( 2, 517, 612, 0, 523, 668, 90 );
		addDoorBox( 2, 517, 612, 170, 523, 668, 192 );
		const float q[4][3] = { { 520, 612, 90 }, { 520, 668, 90 }, { 520, 668, 170 }, { 520, 612, 170 } };
		const int tri[2][3] = { { 0, 1, 2 }, { 0, 2, 3 } };
		for ( const auto & tt : tri ) {
			for ( int v : tt )
				g.glass.insert( g.glass.end(), q[v], q[v] + 3 );
			g.glassT.insert( g.glassT.end(), S.tint, S.tint + 3 );
			g.glassDoor.push_back( 2 );
		}
	}
	g.amask.triOf.resize( g.tris.size() / 9, -1 );
}

//! the soup with the closed doors' solid faces merged in (the D1 reference: the rays trace them; panes left out)
ProbeSoup mergedClosed( const ProbeSoup & s0, const std::vector<quint8> & closed )
{
	ProbeSoup s = s0;
	const ProbeSoup::DoorGeom & g = s0.doorGeom;
	const int map0 = int( s.amask.maps.size() ), model0 = int( s.amask.models.size() );
	s.amask.maps.insert( s.amask.maps.end(), g.amask.maps.begin(), g.amask.maps.end() );
	s.amask.mapNames.insert( s.amask.mapNames.end(), g.amask.mapNames.begin(), g.amask.mapNames.end() );
	s.amask.models.insert( s.amask.models.end(), g.amask.models.begin(), g.amask.models.end() );
	const quint8 grey[3] = { 170, 170, 170 };
	for ( size_t i = 0; i * 9 < g.tris.size(); i++ ) {
		const int d = g.door[i];
		if ( d < 0 || size_t( d ) >= closed.size() || !closed[size_t( d )] )
			continue;
		s.addTri( &g.tris[i * 9], &g.tris[i * 9 + 3], &g.tris[i * 9 + 6], grey );
		if ( i < g.amask.triOf.size() && g.amask.triOf[i] >= 0 ) {
			const probebvh::AlphaMask::Tri & t = g.amask.tris[size_t( g.amask.triOf[i] )];
			s.markLastMasked( map0 + t.map, t.thr, t.uv, model0 + t.model );
		}
	}
	if ( !s.amask.tris.empty() )
		s.amask.triOf.resize( size_t( s.triCount() ), -1 );
	s.doorGeom = ProbeSoup::DoorGeom();
	return s;
}

//! max |a - b| over max |b| (0 when both are empty; 1e30 on a size mismatch)
double relDiff( const std::vector<float> & a, const std::vector<float> & b, size_t * where = nullptr )
{
	if ( a.size() != b.size() )
		return 1e30;
	double d = 0.0, m = 0.0;
	for ( size_t i = 0; i < a.size(); i++ ) {
		const double e = std::fabs( double( a[i] ) - double( b[i] ) );
		if ( e > d ) {
			d = e;
			if ( where )
				*where = i;
		}
		m = std::max( m, std::fabs( double( b[i] ) ) );
	}
	return m > 0.0 ? d / m : d;
}

std::vector<float> column( const std::vector<float> & v, size_t stride, size_t first, size_t n )
{
	std::vector<float> o;
	for ( size_t i = 0; i + stride <= v.size(); i += stride )
		o.insert( o.end(), v.begin() + std::ptrdiff_t( i + first ), v.begin() + std::ptrdiff_t( i + first + n ) );
	return o;
}

bool putF32( const QString & path, const std::vector<float> & v )
{
	QFile f( path );
	return f.open( QIODevice::WriteOnly )
		&& f.write( reinterpret_cast<const char *>( v.data() ), qint64( v.size() * 4 ) ) == qint64( v.size() * 4 );
}

template <class T> bool putRaw( const QString & path, const std::vector<T> & v )
{
	QFile f( path );
	return f.open( QIODevice::WriteOnly )
		&& f.write( reinterpret_cast<const char *>( v.data() ), qint64( v.size() * sizeof( T ) ) ) == qint64( v.size() * sizeof( T ) );
}

double median( std::vector<double> v )
{
	if ( v.empty() )
		return 0.0;
	std::sort( v.begin(), v.end() );
	return v[v.size() / 2];
}

}	// namespace

int probeRelightCli( const QStringList & args )
{
	QString outDir;
	ProbePlaceSpec ps;
	ProbeBakeSpec bs;
	int repeats = 5;
	for ( int i = 0; i < args.size(); i++ ) {
		const QString & a = args[i];
		const QString nx = i + 1 < args.size() ? args[i + 1] : QString();
		if ( a == QLatin1String( "--out" ) ) { outDir = nx; i++; }
		else if ( a == QLatin1String( "--spacing" ) ) { ps.spacing = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--rays" ) ) { bs.rays = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--repeats" ) ) { repeats = std::max( 1, nx.toInt() ); i++; }
	}
	if ( outDir.isEmpty() ) {
		std::fprintf( stderr, "usage: gpurelight --out <dir> [--spacing s] [--rays n] [--repeats n]\n"
			"  the synthetic three-door scene: bake, record, relight on the CPU and the GPU, the gates (gates.txt;\n"
			"  exit 3 on a red that passed or a green that failed, 4 without a GPU)\n" );
		return 2;
	}
	QDir().mkpath( outDir );
	QDir out( outDir );
	QStringList G;   // gates.txt
	int failures = 0;
	auto gate = [&]( const QString & name, bool pass, const QString & detail, bool red = false ) {
		const bool ok = red ? !pass : pass;
		if ( !ok )
			failures++;
		G << QStringLiteral( "%1 %2 %3: %4" ).arg( ok ? QStringLiteral( "OK  " ) : QStringLiteral( "FAIL" ) )
				 .arg( red ? QStringLiteral( "red  " ) : QStringLiteral( "green" ) ).arg( name ).arg( detail );
		std::printf( "%s\n", qPrintable( G.back() ) );
		std::fflush( stdout );
	};
	QString err;
	// ---- the scene, through the soup file (DRG1 both ways)
	RelightScene S;
	buildScene( S );
	const QString soupPath = out.filePath( QStringLiteral( "soup.psp" ) ), soupBare = out.filePath( QStringLiteral( "soup_nodoors.psp" ) );
	ProbeSoup bare = S.soup;
	bare.doorGeom = ProbeSoup::DoorGeom();
	if ( !probeSoupWrite( soupPath, S.soup, &err ) || !probeSoupWrite( soupBare, bare, &err ) ) {
		std::fprintf( stderr, "gpurelight: %s\n", qPrintable( err ) );
		return 1;
	}
	{
		QFile a( soupPath ), b( soupBare );
		a.open( QIODevice::ReadOnly );
		b.open( QIODevice::ReadOnly );
		const QByteArray A = a.readAll(), B = b.readAll();
		const bool prefix = A.size() > B.size() && A.startsWith( B ) && A.mid( B.size(), 4 ) == QByteArray( "DRG1", 4 );
		gate( QStringLiteral( "OFF soup" ), prefix,
			QStringLiteral( "without door geometry the soup is %1 B; with it the same bytes + a DRG1 tail of %2 B" )
				.arg( B.size() ).arg( A.size() - B.size() ) );
	}
	ProbeSoup soup;
	if ( !probeSoupRead( soupPath, &soup, &err ) ) {
		std::fprintf( stderr, "gpurelight: %s\n", qPrintable( err ) );
		return 1;
	}
	{
		const ProbeSoup::DoorGeom & a = S.soup.doorGeom, & b = soup.doorGeom;
		bool same = a.tris == b.tris && a.door == b.door && a.glass == b.glass && a.glassT == b.glassT && a.glassDoor == b.glassDoor
			&& a.amask.triOf == b.amask.triOf && a.amask.tris.size() == b.amask.tris.size() && a.amask.maps.size() == b.amask.maps.size();
		for ( size_t i = 0; same && i < a.amask.maps.size(); i++ )
			same = a.amask.maps[i].a == b.amask.maps[i].a;
		for ( size_t i = 0; same && i < a.amask.tris.size(); i++ )
			same = std::memcmp( a.amask.tris[i].uv, b.amask.tris[i].uv, 24 ) == 0 && a.amask.tris[i].thr == b.amask.tris[i].thr;
		gate( QStringLiteral( "DRG1 round trip" ), same, QStringLiteral( "%1 door triangles (%2 masked), %3 panes read back equal" )
			.arg( b.tris.size() / 9 ).arg( b.amask.tris.size() ).arg( b.glass.size() / 9 ) );
	}
	// ---- place and bake (the doors open: the bake never sees doorGeom)
	ps.minX = -16;
	ps.minY = -16;
	ps.maxX = 1056;
	ps.maxY = 784;
	ProbePlaceResult pr;
	if ( !probePlace( soup, ps, &pr ) ) {
		std::fprintf( stderr, "gpurelight: placement: %s\n", qPrintable( pr.error ) );
		return 1;
	}
	const QString bakeDir = out.filePath( QStringLiteral( "bake" ) );
	ProbeBakeResult br;
	if ( !probeBake( soup, pr.probes, bs, bakeDir, &br, &pr.roomBoxes ) ) {
		std::fprintf( stderr, "gpurelight: %s\n", qPrintable( br.error ) );
		return 1;
	}
	std::fputs( qPrintable( probeCensusText( pr ) ), stdout );
	std::fputs( qPrintable( probeBakeCensusText( br ) ), stdout );
	// ---- the lights: two on, one a switch's (off at the start)
	auto mk = []( double x, double y, double z, double r, double cr, double cg, double cb ) {
		WwCellLight l;
		l.pos[0] = float( x );
		l.pos[1] = float( y );
		l.pos[2] = float( z );
		l.radius = float( r );
		l.color[0] = float( cr );
		l.color[1] = float( cg );
		l.color[2] = float( cb );
		return l;
	};
	const WwCellLight L0 = mk( 256, 384, 200, 1100, 2.0, 1.8, 1.5 ), L1 = mk( 900, 600, 220, 500, 0.3, 0.3, 0.4 ),
		L2 = mk( 800, 150, 200, 600, 1.5, 0.6, 0.3 ), L3 = mk( 400, 640, 130, 700, 1.0, 1.0, 1.0 );   // L3: before the pane
	WwCellLighting lit;
	lit.interior = true;
	lit.lights = { L0, L1, L3 };
	ProbeGiSpec gs;
	// ---- OFF: the relight with the recorder on is the relight without it, bit for bit
	ProbeGiResult r0, rRec;
	QElapsedTimer clock;
	clock.start();
	if ( !probeGiRelight( soup, bakeDir, lit, gs, &r0 ) ) {
		std::fprintf( stderr, "gpurelight: %s\n", qPrintable( r0.error ) );
		return 1;
	}
	const double msRef = clock.nsecsElapsed() / 1e6;
	ProbeRelightOps X;
	ProbeGiSpec gsRec = gs;
	gsRec.record = &X;
	gsRec.recordExtra = { L2 };
	gsRec.recordRef = { 0x0100B001u, 0x0100B002u, 0x0100B004u, 0x0100B003u };
	gsRec.recordGroup = { 0ull, 0ull, 0ull, ( 2ull << 62 ) | 0x0100B003ull };
	if ( !probeGiRelight( soup, bakeDir, lit, gsRec, &rRec ) || !X.built ) {
		std::fprintf( stderr, "gpurelight: record: %s\n", qPrintable( rRec.error ) );
		return 1;
	}
	gate( QStringLiteral( "OFF relight" ), r0.surfelOut == rRec.surfelOut && r0.probeCube == rRec.probeCube && r0.grid == rRec.grid
		&& r0.grid2 == rRec.grid2 && r0.surfelBounce == rRec.surfelBounce && r0.passes == rRec.passes,
		QStringLiteral( "recording leaves surfels, probes, grid and the %1 passes identical (bit for bit)" ).arg( r0.passes ) );
	std::printf( "%s\n%s\n", qPrintable( probeGiCensusText( r0 ) ), qPrintable( probeRelightCensusText( X ) ) );
	G << probeRelightCensusText( X );
	gate( QStringLiteral( "doors re-traced" ), X.doorGeometry && X.doorEntries[0] > 0 && X.doorEntries[1] > 0 && X.doorEntries[2] > 0
		&& X.doorEntries[3] > 0, QStringLiteral( "pairs %1, links %2, feed %3, blend %4 entries cross a door box; geometry %5" )
		.arg( X.doorEntries[0] ).arg( X.doorEntries[1] ).arg( X.doorEntries[2] ).arg( X.doorEntries[3] ).arg( X.doorGeometry ) );
	// ---- the states
	struct St
	{
		QString name;
		ProbeRelightState st;
		QVector<WwCellLight> live;   // the reference's lights
		bool reference = true;       // an exact reference exists (no door closed)
	};
	std::vector<St> states;
	auto colors = []( std::initializer_list<WwCellLight> ls ) {
		std::vector<float> c;
		for ( const WwCellLight & l : ls )
			c.insert( c.end(), l.color, l.color + 3 );
		return c;
	};
	WwCellLight off0 = L0, off2 = L2, rec0 = L0, dim1 = L1, rad0 = L0, rad1 = L1;
	for ( float & c : off0.color )
		c = 0.0f;
	for ( float & c : off2.color )
		c = 0.0f;
	rec0.color[0] = 0.4f;
	rec0.color[1] = 1.2f;
	rec0.color[2] = 2.2f;
	for ( float & c : dim1.color )
		c *= 0.3f;
	rad0.radius *= 0.6f;
	rad1.radius *= 0.6f;
	{
		St s;
		s.name = QStringLiteral( "baked" );
		s.st.color = colors( { L0, L1, L3, off2 } );
		s.live = { L0, L1, L3 };
		states.push_back( s );
		s.name = QStringLiteral( "light0off" );
		s.st.color = colors( { off0, L1, L3, off2 } );
		s.live = { off0, L1, L3 };
		states.push_back( s );
		s.name = QStringLiteral( "recolor" );
		s.st.color = colors( { rec0, dim1, L3, off2 } );
		s.live = { rec0, dim1, L3 };
		states.push_back( s );
		s.name = QStringLiteral( "radius" );
		s.st.color = colors( { L0, L1, L3, off2 } );
		s.st.radius = { rad0.radius, rad1.radius, L3.radius, L2.radius };
		s.live = { rad0, rad1, L3 };
		states.push_back( s );
		s.st.radius.clear();
		s.name = QStringLiteral( "switchon" );
		s.st.color = colors( { L0, L1, L3, L2 } );
		s.live = { L0, L1, L3, L2 };
		states.push_back( s );
		s.name = QStringLiteral( "doorsclosed" );
		s.st.color = colors( { L0, L1, L3, off2 } );
		s.st.doorClosed = { 1, 1, 1 };
		s.live = { L0, L1, L3 };
		s.reference = false;
		states.push_back( s );
	}
	// ---- the GPU
	ProbeRelightGpu gpu;
	QString why;
	bool gpuOk = gpu.init( &why ) && gpu.upload( X, &why );
	G << QStringLiteral( "gpu: %1 (upload %2 ms)" ).arg( gpuOk ? gpu.renderer() : QStringLiteral( "UNAVAILABLE: " ) + why )
			 .arg( gpu.msUpload(), 0, 'f', 2 );
	std::printf( "%s\n", qPrintable( G.back() ) );
	// ---- the twin's dumps
	const QString tw = out.filePath( QStringLiteral( "twin" ) );
	QDir().mkpath( tw );
	{
		std::vector<float> surf;
		for ( int i = 0; i < X.surfels; i++ )
			for ( const std::vector<double> * v : { &X.pos, &X.nrm, &X.alb } )
				for ( int c = 0; c < 3; c++ )
					surf.push_back( float( ( *v )[size_t( i ) * 3 + size_t( c )] ) );
		putF32( QDir( tw ).filePath( QStringLiteral( "surf.f32" ) ), surf );
		putRaw( QDir( tw ).filePath( QStringLiteral( "sid.i32" ) ), X.sid );
		putF32( QDir( tw ).filePath( QStringLiteral( "soup_tris.f32" ) ), soup.tris );
		putF32( QDir( tw ).filePath( QStringLiteral( "door_tris.f32" ) ), soup.doorGeom.tris );
		putRaw( QDir( tw ).filePath( QStringLiteral( "door_id.i32" ) ), soup.doorGeom.door );
		std::vector<float> dm;
		for ( size_t i = 0; i < soup.doorGeom.door.size(); i++ ) {
			const int m = i < soup.doorGeom.amask.triOf.size() ? soup.doorGeom.amask.triOf[i] : -1;
			if ( m < 0 ) {
				dm.insert( dm.end(), { -1, 0, 0, 0, 0, 0, 0, 0 } );
				continue;
			}
			const probebvh::AlphaMask::Tri & t = soup.doorGeom.amask.tris[size_t( m )];
			dm.push_back( float( t.map ) );
			dm.push_back( float( t.thr ) );
			dm.insert( dm.end(), t.uv, t.uv + 6 );
		}
		putF32( QDir( tw ).filePath( QStringLiteral( "door_mask.f32" ) ), dm );
		for ( size_t m = 0; m < soup.doorGeom.amask.maps.size(); m++ )
			putRaw( QDir( tw ).filePath( QStringLiteral( "map%1.u8" ).arg( m ) ), soup.doorGeom.amask.maps[m].a );
		putF32( QDir( tw ).filePath( QStringLiteral( "glass.f32" ) ), soup.doorGeom.glass );
		putRaw( QDir( tw ).filePath( QStringLiteral( "glass_t.u8" ) ), soup.doorGeom.glassT );
		putRaw( QDir( tw ).filePath( QStringLiteral( "glass_door.i32" ) ), soup.doorGeom.glassDoor );
	}
	QStringList stateJson;
	// ---- run every state
	std::vector<double> msCpu, msGpu, msRefs { msRef };
	std::map<QString, ProbeRelightOut> cpuOut;
	for ( St & s : states ) {
		ProbeRelightOut co;
		std::vector<double> t;
		for ( int r = 0; r < repeats; r++ ) {
			if ( !probeRelightCpu( X, s.st, &co, &err ) ) {
				std::fprintf( stderr, "gpurelight: %s\n", qPrintable( err ) );
				return 1;
			}
			t.push_back( co.ms );
		}
		msCpu.push_back( median( t ) );
		cpuOut[s.name] = co;
		putF32( QDir( tw ).filePath( QStringLiteral( "b1_cpu_%1.f32" ).arg( s.name ) ), co.B1 );
		putF32( QDir( tw ).filePath( QStringLiteral( "b_cpu_%1.f32" ).arg( s.name ) ), co.B );
		{
			QStringList c, r, d;
			for ( float v : s.st.color )
				c << QString::number( double( v ), 'g', 9 );
			for ( size_t l = 0; l < X.lights.size(); l++ )
				r << QString::number( double( s.st.radius.size() == X.lights.size() ? s.st.radius[l] : X.lights[l].radius ), 'g', 9 );
			for ( size_t k = 0; k < X.doorRefs.size(); k++ )
				d << QString::number( k < s.st.doorClosed.size() ? s.st.doorClosed[k] : 0 );
			stateJson << QStringLiteral( "\"%1\": {\"color\": [%2], \"radius\": [%3], \"closed\": [%4], \"passes\": %5}" )
				.arg( s.name, c.join( ", " ), r.join( ", " ), d.join( ", " ) ).arg( co.passes );
		}
		if ( s.reference ) {   // P: the CPU relight from the operators = the relight with rays
			WwCellLighting L = lit;
			L.lights = s.live;
			ProbeGiResult ref;
			clock.restart();
			probeGiRelight( soup, bakeDir, L, gs, &ref );
			msRefs.push_back( clock.nsecsElapsed() / 1e6 );
			const std::vector<float> rB1 = column( ref.surfelOut, 12, 9, 3 ), rE = column( ref.probeCube, 21, 3, 18 );
			const double dB1 = relDiff( co.B1, rB1 ), dB = relDiff( co.B, ref.surfelBounce ), dE = relDiff( co.E, rE ),
				dG = std::max( relDiff( co.grid, ref.grid ), relDiff( co.grid2, ref.grid2 ) );
			const double worst = std::max( std::max( dB1, dB ), std::max( dE, dG ) );
			gate( QStringLiteral( "P %1" ).arg( s.name ), worst <= 1e-6 && co.passes == ref.passes,
				QStringLiteral( "CPU vs the ray relight: B1 %1, B %2, E %3, grid %4 (max rel; gate 1e-6), passes %5/%6" )
					.arg( dB1, 0, 'g', 3 ).arg( dB, 0, 'g', 3 ).arg( dE, 0, 'g', 3 ).arg( dG, 0, 'g', 3 ).arg( co.passes ).arg( ref.passes ) );
			putF32( QDir( tw ).filePath( QStringLiteral( "b1_ref_%1.f32" ).arg( s.name ) ), rB1 );
			if ( s.name == QLatin1String( "baked" ) ) {   // red: one bounce only must miss the reference
				ProbeRelightState rs = s.st;
				rs.red = QStringLiteral( "nobounce" );
				ProbeRelightOut ro;
				probeRelightCpu( X, rs, &ro, &err );
				const double d = relDiff( ro.B, ref.surfelBounce );
				gate( QStringLiteral( "P red nobounce" ), d <= 1e-6, QStringLiteral( "B %1" ).arg( d, 0, 'g', 3 ), true );
			}
		}
		if ( gpuOk ) {   // G: the GPU = the CPU (float32 against double), the same pass count
			ProbeRelightState gst = s.st;
			gst.passes = co.passes;
			ProbeRelightOut go;
			std::vector<double> tg;
			for ( int r = 0; r < repeats && gpuOk; r++ ) {
				gpuOk = gpu.run( gst, &go, &why, true );
				tg.push_back( go.ms );
			}
			if ( !gpuOk ) {
				G << QStringLiteral( "gpu run failed: %1" ).arg( why );
				break;
			}
			msGpu.push_back( median( tg ) );
			const double dB1 = relDiff( go.B1, co.B1 ), dB = relDiff( go.B, co.B ), dE = relDiff( go.E, co.E ),
				dG = std::max( relDiff( go.grid, co.grid ), relDiff( go.grid2, co.grid2 ) );
			const double worst = std::max( std::max( dB1, dB ), std::max( dE, dG ) );
			gate( QStringLiteral( "G %1" ).arg( s.name ), worst <= 1e-4,
				QStringLiteral( "GPU vs CPU: B1 %1, B %2, E %3, grid %4 (gate 1e-4); %5 ms GPU (direct %6, gather %7, feed %8, "
					"grid %9, readback %10) vs %11 ms CPU" )
					.arg( dB1, 0, 'g', 3 ).arg( dB, 0, 'g', 3 ).arg( dE, 0, 'g', 3 ).arg( dG, 0, 'g', 3 ).arg( median( tg ), 0, 'f', 2 )
					.arg( go.msKernel[0], 0, 'f', 2 ).arg( go.msKernel[1], 0, 'f', 2 ).arg( go.msKernel[2], 0, 'f', 2 )
					.arg( go.msKernel[3], 0, 'f', 2 ).arg( go.msKernel[4], 0, 'f', 2 ).arg( msCpu.back(), 0, 'f', 2 ) );
			putF32( QDir( tw ).filePath( QStringLiteral( "b1_gpu_%1.f32" ).arg( s.name ) ), go.B1 );
			putF32( QDir( tw ).filePath( QStringLiteral( "b_gpu_%1.f32" ).arg( s.name ) ), go.B );
			if ( s.name == QLatin1String( "doorsclosed" ) ) {   // red: a GPU that ignores the closed doors
				ProbeRelightState rs = gst;
				rs.red = QStringLiteral( "dooropen" );
				ProbeRelightOut ro;
				gpu.run( rs, &ro, &why, true );
				const double d = relDiff( ro.B1, co.B1 );
				gate( QStringLiteral( "G red dooropen" ), d <= 1e-4, QStringLiteral( "B1 %1" ).arg( d, 0, 'g', 3 ), true );
			}
		}
	}
	// ---- D1: doors closed = the relight with rays through the merged doors (direct light; panes read clear)
	{
		const St & s = states.back();
		const ProbeSoup merged = mergedClosed( soup, s.st.doorClosed );
		WwCellLighting L = lit;
		L.lights = s.live;
		ProbeGiResult ref;
		probeGiRelight( merged, bakeDir, L, gs, &ref );
		const std::vector<float> rB1 = column( ref.surfelOut, 12, 9, 3 );
		putF32( QDir( tw ).filePath( QStringLiteral( "b1_ref_doorsclosed_clear.f32" ) ), rB1 );
		putF32( QDir( tw ).filePath( QStringLiteral( "b_ref_doorsclosed.f32" ) ), ref.surfelBounce );
		const char * modes[4] = { "glassclear", "glassclear+doorblanket", "glassclear+glassopaque", "glassclear+dooropen" };
		for ( int m = 0; m < 4; m++ ) {
			ProbeRelightState rs = s.st;
			rs.red = QString::fromLatin1( modes[m] );
			ProbeRelightOut ro;
			probeRelightCpu( X, rs, &ro, &err );
			size_t at = 0;
			const double d = relDiff( ro.B1, rB1, &at );
			gate( QStringLiteral( "D1 %1" ).arg( m ? QString::fromLatin1( modes[m] + 11 ) : QStringLiteral( "closed" ) ), d <= 1e-6,
				QStringLiteral( "B1 vs the merged-door ray relight %1 (gate 1e-6; worst surfel %2)%3" ).arg( d, 0, 'g', 3 ).arg( at / 3 )
					.arg( m ? QString() : QStringLiteral( "; B %1, E reported only (the reference's feed lists see the closed doors)" )
						.arg( relDiff( ro.B, ref.surfelBounce ), 0, 'g', 3 ) ), m > 0 );
		}
		const ProbeRelightOut & co = cpuOut[s.name];
		ProbeRelightState rs = s.st;
		rs.red = QStringLiteral( "glassopaque" );
		ProbeRelightOut ro;
		probeRelightCpu( X, rs, &ro, &err );
		putF32( QDir( tw ).filePath( QStringLiteral( "b1_cpu_doorsclosed_glassopaque.f32" ) ), ro.B1 );
		rs.red = QStringLiteral( "doorblanket" );
		probeRelightCpu( X, rs, &ro, &err );
		putF32( QDir( tw ).filePath( QStringLiteral( "b1_cpu_doorsclosed_doorblanket.f32" ) ), ro.B1 );
		G << QStringLiteral( "doorsclosed: %1 slots lost every probe (valid, 0)" ).arg( co.slotsEmptied );
	}
	// ---- D2: what each door lets through, per pair (room A's lights -> surfels in room B: the ray passes the slab)
	{
		struct Tally { int n = 0, through = 0, tinted = 0, other = 0; };
		Tally t[3];
		const float tg[3] = { S.tint[0] / 255.0f, S.tint[1] / 255.0f, S.tint[2] / 255.0f };
		for ( int i = 0; i < X.surfels; i++ ) {
			if ( X.pos[size_t( i ) * 3] < 530.0 )
				continue;
			for ( int k = X.pairStart[size_t( i )]; k < X.pairStart[size_t( i ) + 1]; k++ ) {
				if ( X.lights[size_t( X.pairLight[size_t( k )] )].pos[0] > 512.0f )   // the lights of room A
					continue;
				const int d = X.pairDoor[size_t( k ) * 2];
				if ( d < 0 || d > 2 )
					continue;
				const float * T = &X.pairT[size_t( k ) * 8];
				Tally & y = t[d];
				y.n++;
				const bool zero = T[0] == 0.0f && T[1] == 0.0f && T[2] == 0.0f;
				const bool one = T[0] == 1.0f && T[1] == 1.0f && T[2] == 1.0f;
				const bool tint = T[0] == tg[0] && T[1] == tg[1] && T[2] == tg[2];
				if ( !zero )
					y.through++;
				if ( tint )
					y.tinted++;
				if ( !zero && !( d == 2 ? tint : one ) )
					y.other++;
			}
		}
		gate( QStringLiteral( "D2 solid" ), t[0].n > 0 && t[0].through == 0,
			QStringLiteral( "%1 rays from room A, %2 pass the solid door" ).arg( t[0].n ).arg( t[0].through ) );
		const double fh = t[1].n ? double( t[1].through ) / t[1].n : 0.0;
		gate( QStringLiteral( "D2 hole" ), t[1].n > 0 && fh > 0.05 && fh < 0.95 && t[1].other == 0,
			QStringLiteral( "%1 rays, %2 through the alpha-tested hole (%3), %4 partial" ).arg( t[1].n ).arg( t[1].through )
				.arg( fh, 0, 'f', 3 ).arg( t[1].other ) );
		const double fg = t[2].n ? double( t[2].through ) / t[2].n : 0.0;
		gate( QStringLiteral( "D2 glass" ), t[2].n > 0 && fg > 0.05 && fg < 0.95 && t[2].tinted == t[2].through && t[2].other == 0,
			QStringLiteral( "%1 rays, %2 through the pane, %3 of them tinted exactly (%4, %5, %6)/255, %7 otherwise" ).arg( t[2].n )
				.arg( t[2].through ).arg( t[2].tinted ).arg( S.tint[0] ).arg( S.tint[1] ).arg( S.tint[2] ).arg( t[2].other ) );
	}
	// ---- the shared record
	{
		QString census;
		if ( !probeRelightWriteRecords( X, out.filePath( QStringLiteral( "records" ) ), { QStringLiteral( "Fallout4.esm" ),
			QStringLiteral( "Synthetic.esp" ) }, &err, &census ) ) {
			std::fprintf( stderr, "gpurelight: %s\n", qPrintable( err ) );
			return 1;
		}
		G << census;
		std::printf( "%s\n", qPrintable( census ) );
	}
	// ---- the twin's scene
	{
		QStringList ls;
		for ( const ProbeRelightOps::Light & l : X.lights )
			ls << QStringLiteral( "{\"pos\": [%1, %2, %3], \"radius\": %4, \"bias\": %5, \"scale\": %6, \"exponent\": %7}" )
				.arg( l.pos[0] ).arg( l.pos[1] ).arg( l.pos[2] ).arg( l.radius ).arg( l.bias ).arg( l.scale ).arg( l.exponent );
		const QString js = QStringLiteral( "{\n\"surfels\": %1,\n\"fixtureClear\": %2,\n\"pairs\": %3,\n\"lights\": [%4],\n\"states\": {%5},\n"
			"\"tint\": [%6, %7, %8],\n\"files\": [\"%9\"]\n}\n" ).arg( X.surfels ).arg( double( gs.fixtureClear ) ).arg( X.pairLight.size() )
			.arg( ls.join( ", " ) ).arg( stateJson.join( ",\n" ) ).arg( S.tint[0] ).arg( S.tint[1] ).arg( S.tint[2] )
			.arg( X.files.join( QStringLiteral( "\", \"" ) ) );
		writeFile( QDir( tw ).filePath( QStringLiteral( "scene.json" ) ), js.toUtf8(), &err );
	}
	const QString timing = QStringLiteral( "timings (median of %1): relight with rays %2 ms; record %3 ms; CPU from the operators %4 ms; "
		"GPU %5 ms (upload once %6 ms)" ).arg( repeats ).arg( median( msRefs ), 0, 'f', 1 ).arg( X.msRecord, 0, 'f', 1 )
		.arg( median( msCpu ), 0, 'f', 2 ).arg( msGpu.empty() ? -1.0 : median( msGpu ), 0, 'f', 2 ).arg( gpu.msUpload(), 0, 'f', 2 );
	G << timing;
	std::printf( "%s\n", qPrintable( timing ) );
	G << QStringLiteral( "%1 failures" ).arg( failures );
	writeFile( out.filePath( QStringLiteral( "gates.txt" ) ), ( G.join( QStringLiteral( "\n" ) ) + QStringLiteral( "\n" ) ).toUtf8(), &err );
	std::printf( "gpurelight: %d failures\n", failures );
	if ( failures )
		return 3;
	return msGpu.empty() ? 4 : 0;
}

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

bool probeRelightGpuWantedForArgs( int argc, char ** argv )
{
	bool want = false;
	for ( int i = 1; i < argc; i++ ) {
		if ( !qstrcmp( argv[i], "--no-gpu" ) )
			return false;
		if ( !qstrcmp( argv[i], "gpurelight" ) )
			want = true;
	}
	if ( !want || argc < 1 )
		return false;
	// without the platform plugin a QGuiApplication aborts the process (lodgenGpuWantedForArgs's rule)
	QString dir;
#ifdef Q_OS_WIN
	wchar_t buf[4096];
	const DWORD n = GetModuleFileNameW( nullptr, buf, 4096 );
	if ( n > 0 && n < 4096 )
		dir = QFileInfo( QString::fromWCharArray( buf, int( n ) ) ).absolutePath();
#endif
	if ( dir.isEmpty() )
		dir = QFileInfo( QString::fromLocal8Bit( argv[0] ) ).absolutePath();
	return QFileInfo::exists( dir + QStringLiteral( "/platforms/qwindows.dll" ) );
}
