#include "probegifine.h"

#include "proberooms.h"

#include <QElapsedTimer>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <thread>

namespace
{
const double kAxes[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
const double kPi = 3.14159265358979;

double lum( const double c[3] )
{
	return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2];
}

void runParallel( size_t n, const std::function<void( size_t )> & fn )
{
	const int nt = std::max( 1, int( std::thread::hardware_concurrency() ) );
	std::atomic<size_t> next( 0 );
	std::vector<std::thread> pool;
	for ( int t = 0; t < nt; t++ )
		pool.emplace_back( [&]() {
			for ( size_t i; ( i = next++ ) < n; )
				fn( i );
		} );
	for ( std::thread & t : pool )
		t.join();
}

struct Box
{
	double lo[3], hi[3];
	double power;
	int clusters;
};
}

void probeGiFineBuild( const ProbeGiFineInput & in, ProbeGiFine * out )
{
	ProbeGiFine & F = *out;
	F = ProbeGiFine();
	QElapsedTimer clock;
	clock.start();
	if ( in.red == QLatin1String( "1" ) || in.red == QLatin1String( "old" ) ) {
		F.note = QStringLiteral( "red: not built (the old path)" );
		return;
	}
	const QByteArray ethEnv = qgetenv( "WW_CELL_EMISSIVEGRID_ETH" );
	F.eth = ethEnv.isEmpty() ? 0.02 : std::max( ethEnv.toDouble(), 1e-4 );
	const double v = in.coarseVoxel;
	const int W = in.coarseDims[0], H = in.coarseDims[1];
	F.width = W;
	F.height = H;
	F.emitters = int( in.glow.size() );
	const size_t ne = in.glow.size();
	if ( ne == 0 || W <= 0 || H <= 0 ) {
		F.note = QStringLiteral( "no glowing surfels" );
		return;
	}

	// 1. clusters: glowing surfels within one coarse voxel of each other (single linkage)
	std::vector<int> root( ne );
	for ( size_t i = 0; i < ne; i++ )
		root[i] = int( i );
	auto find = [&]( int i ) {
		while ( root[size_t( i )] != i )
			i = root[size_t( i )] = root[size_t( root[size_t( i )] )];
		return i;
	};
	for ( size_t i = 0; i < ne; i++ )
		for ( size_t j = i + 1; j < ne; j++ ) {
			double d2 = 0.0;
			for ( int c = 0; c < 3; c++ )
				d2 += ( in.glow[i].p[c] - in.glow[j].p[c] ) * ( in.glow[i].p[c] - in.glow[j].p[c] );
			if ( d2 <= v * v )
				root[size_t( find( int( i ) ) )] = find( int( j ) );
		}
	// 2. a box round each cluster bright enough to light past 1.5 coarse voxels: reach = sqrt( power / eth )
	std::vector<Box> boxes;
	{
		std::vector<int> idx( ne, -1 );
		std::vector<Box> cl;
		for ( size_t i = 0; i < ne; i++ ) {
			const int r = find( int( i ) );
			if ( idx[size_t( r )] < 0 ) {
				idx[size_t( r )] = int( cl.size() );
				Box b;
				for ( int c = 0; c < 3; c++ )
					b.lo[c] = b.hi[c] = in.glow[i].p[c];
				b.power = 0.0;
				b.clusters = 1;
				cl.push_back( b );
			}
			Box & b = cl[size_t( idx[size_t( r )] )];
			for ( int c = 0; c < 3; c++ ) {
				b.lo[c] = std::min( b.lo[c], in.glow[i].p[c] );
				b.hi[c] = std::max( b.hi[c], in.glow[i].p[c] );
			}
			b.power += lum( in.glow[i].Le ) * in.glow[i].area;
		}
		F.clusters = int( cl.size() );
		for ( Box b : cl ) {
			const double reach = std::sqrt( b.power / F.eth );
			if ( reach < 1.5 * v )
				continue;
			const double r = std::min( reach, 4.0 * v );
			for ( int c = 0; c < 3; c++ ) {
				b.lo[c] -= r;
				b.hi[c] += r;
			}
			boxes.push_back( b );
		}
	}
	// overlapping boxes merge (their union's box)
	for ( bool merged = true; merged; ) {
		merged = false;
		for ( size_t i = 0; i < boxes.size() && !merged; i++ )
			for ( size_t j = i + 1; j < boxes.size() && !merged; j++ ) {
				bool over = true;
				for ( int c = 0; c < 3; c++ )
					over = over && boxes[i].lo[c] <= boxes[j].hi[c] && boxes[j].lo[c] <= boxes[i].hi[c];
				if ( !over )
					continue;
				for ( int c = 0; c < 3; c++ ) {
					boxes[i].lo[c] = std::min( boxes[i].lo[c], boxes[j].lo[c] );
					boxes[i].hi[c] = std::max( boxes[i].hi[c], boxes[j].hi[c] );
				}
				boxes[i].power += boxes[j].power;
				boxes[i].clusters += boxes[j].clusters;
				boxes.erase( boxes.begin() + std::ptrdiff_t( j ) );
				merged = true;
			}
	}
	std::sort( boxes.begin(), boxes.end(), []( const Box & a, const Box & b ) { return a.power > b.power; } );
	if ( boxes.size() > 8 )
		boxes.resize( 8 );	// the shader's uniform arrays

	// 3. the boxes' voxels: a third of the coarse voxel, coarser if a box would not fit one atlas layer
	const qint64 kMaxVoxels = 600000;
	struct BoxDim { double o[3], fv; int n[3], z0; };
	std::vector<BoxDim> bd;
	int layers = 0;
	for ( const Box & b : boxes ) {
		BoxDim d;
		d.fv = std::max( { v / 3.0, ( b.hi[0] - b.lo[0] ) / W, ( b.hi[1] - b.lo[1] ) / H } );
		for ( int c = 0; c < 3; c++ ) {
			d.n[c] = std::max( 2, int( std::ceil( ( b.hi[c] - b.lo[c] ) / d.fv ) ) );
			d.o[c] = 0.5 * ( b.lo[c] + b.hi[c] ) - 0.5 * d.n[c] * d.fv;
		}
		d.n[0] = std::min( d.n[0], W );
		d.n[1] = std::min( d.n[1], H );
		const qint64 nv = qint64( d.n[0] ) * d.n[1] * d.n[2];
		if ( F.voxels + nv > kMaxVoxels )
			break;
		d.z0 = layers;
		layers += 12 * d.n[2];
		F.voxels += nv;
		bd.push_back( d );
	}
	F.boxes = int( bd.size() );
	F.layers = layers;
	if ( bd.empty() ) {
		F.note = QStringLiteral( "no cluster bright enough" );
		F.ms = clock.nsecsElapsed() / 1e6;
		return;
	}
	F.data.assign( size_t( W ) * size_t( H ) * size_t( layers ) * 4, 0.0f );
	std::atomic<qint64> rays( 0 ), blockedN( 0 ), air( 0 );
	const double rad = in.radius;
	for ( const BoxDim & d : bd ) {
		double lo[3], hi[3];
		for ( int c = 0; c < 3; c++ ) {
			lo[c] = d.o[c];
			hi[c] = d.o[c] + d.n[c] * d.fv;
		}
		// the glowing surfels in the box: D sums them, C takes only their links (the coarse share D replaces)
		std::vector<char> inBox( ne, 0 );
		std::vector<int> set;
		for ( size_t e = 0; e < ne; e++ ) {
			bool ins = true;
			for ( int c = 0; c < 3; c++ )
				ins = ins && in.glow[e].p[c] >= lo[c] && in.glow[e].p[c] <= hi[c];
			if ( ins ) {
				inBox[e] = 1;
				set.push_back( int( e ) );
			}
		}
		// the probes within the radius of the box, each with its cube of the set's light (as step 2 gathers it)
		struct PC { int probe; double C[6][3]; };
		std::vector<PC> pcs;
		for ( size_t j = 0; j < in.probes.size(); j++ ) {
			const ProbeGiFineProbe & P = in.probes[j];
			double d2 = 0.0;
			for ( int c = 0; c < 3; c++ ) {
				const double e = std::max( std::max( lo[c] - P.p[c], P.p[c] - hi[c] ), 0.0 );
				d2 += e * e;
			}
			if ( d2 >= rad * rad )
				continue;
			PC pc;
			pc.probe = int( j );
			std::fill( &pc.C[0][0], &pc.C[0][0] + 18, 0.0 );
			for ( int k = P.linkStart; k < P.linkEnd; k++ ) {
				const int e = in.linkEmit[size_t( k )];
				if ( !inBox[size_t( e )] )
					continue;
				for ( int a = 0; a < 6; a++ )
					for ( int c = 0; c < 3; c++ )
						pc.C[a][c] += in.glow[size_t( e )].Le[c] * in.linkTint[size_t( k ) * 3 + size_t( c )] * in.linkW[size_t( k ) * 7 + size_t( a )];
			}
			pcs.push_back( pc );
		}
		const size_t nv = size_t( d.n[0] ) * size_t( d.n[1] ) * size_t( d.n[2] );
		runParallel( nv, [&]( size_t i ) {
			const int x = int( i % size_t( d.n[0] ) ), y = int( ( i / size_t( d.n[0] ) ) % size_t( d.n[1] ) ),
				z = int( i / ( size_t( d.n[0] ) * size_t( d.n[1] ) ) );
			const double c[3] = { d.o[0] + ( x + 0.5 ) * d.fv, d.o[1] + ( y + 0.5 ) * d.fv, d.o[2] + ( z + 0.5 ) * d.fv };
			int room[2] = { -1, -1 };
			if ( in.rooms && in.rooms->ok && in.rooms->at( c, room ) && room[0] < 0 )
				return;	// solid: no light lives here (a = 0, the shader skips it)
			air++;
			qint64 nr = 0, nb = 0;
			double D[6][3] = {}, C[6][3] = {};
			// D: each glowing surfel a small area light, A cos / ( d^2 + A / pi ) (finite near its face), shadowed
			for ( int e : set ) {
				const ProbeGiFineEmitter & E = in.glow[size_t( e )];
				double dir[3], d2 = 0.0;
				for ( int k = 0; k < 3; k++ ) {
					dir[k] = E.p[k] - c[k];
					d2 += dir[k] * dir[k];
				}
				const double len = std::sqrt( std::max( d2, 1e-6 ) );
				for ( double & k : dir )
					k /= len;
				const double cosS = -( E.n[0] * dir[0] + E.n[1] * dir[1] + E.n[2] * dir[2] );
				if ( cosS <= 0.0 )
					continue;
				const double om = E.area * cosS / ( d2 + E.area / kPi );
				const double q[3] = { E.p[0] + E.n[0] * 2.0, E.p[1] + E.n[1] * 2.0, E.p[2] + E.n[2] * 2.0 };
				nr++;
				if ( in.blocked( c, q ) ) {
					nb++;
					continue;
				}
				for ( int a = 0; a < 6; a++ ) {
					const double ca = kAxes[a][0] * dir[0] + kAxes[a][1] * dir[1] + kAxes[a][2] * dir[2];
					if ( ca > 0.0 )
						for ( int k = 0; k < 3; k++ )
							D[a][k] += E.Le[k] * om * ca;
				}
			}
			// C: the coarse grid's blend of the same light, (1 - d^2/r^2)^2 over the probes of the room it sees
			double ws = 0.0;
			for ( const PC & pc : pcs ) {
				const ProbeGiFineProbe & P = in.probes[size_t( pc.probe )];
				if ( room[0] >= 0 && P.room[0] >= 0 && P.room[0] != room[0] && P.room[1] != room[0]
					&& ( room[1] < 0 || ( P.room[0] != room[1] && P.room[1] != room[1] ) ) )
					continue;
				double d2 = 0.0;
				for ( int k = 0; k < 3; k++ )
					d2 += ( P.p[k] - c[k] ) * ( P.p[k] - c[k] );
				if ( d2 >= rad * rad )
					continue;
				nr++;
				if ( in.blocked( c, P.p ) ) {
					nb++;
					continue;
				}
				const double t = 1.0 - d2 / ( rad * rad );
				ws += t * t;
				for ( int a = 0; a < 6; a++ )
					for ( int k = 0; k < 3; k++ )
						C[a][k] += t * t * pc.C[a][k];
			}
			rays += nr;
			blockedN += nb;
			for ( int s = 0; s < 12; s++ ) {
				const size_t L = size_t( d.z0 + s * d.n[2] + z );
				float * o = &F.data[( ( L * size_t( H ) + size_t( y ) ) * size_t( W ) + size_t( x ) ) * 4];
				for ( int k = 0; k < 3; k++ )
					o[k] = float( s < 6 ? D[s][k] : ws > 0.0 ? C[s - 6][k] / ws : 0.0 );
				o[3] = 1.0f;
			}
		} );
		F.box.insert( F.box.end(), { float( d.o[0] ), float( d.o[1] ), float( d.o[2] ), float( d.fv ),
			float( d.n[0] ), float( d.n[1] ), float( d.n[2] ), float( d.z0 ) } );
	}
	F.voxelsAir = air;
	F.rays = rays;
	F.raysBlocked = blockedN;
	F.ms = clock.nsecsElapsed() / 1e6;
	F.note = QStringLiteral( "on" );
}

QString probeGiFineCensusText( const ProbeGiFine & f )
{
	QString boxes;
	for ( size_t b = 0; b + 7 < f.box.size(); b += 8 )
		boxes += QStringLiteral( " [%1,%2,%3 %4x%5x%6 voxel %7]" ).arg( double( f.box[b] ), 0, 'f', 0 )
			.arg( double( f.box[b + 1] ), 0, 'f', 0 ).arg( double( f.box[b + 2] ), 0, 'f', 0 ).arg( int( f.box[b + 4] ) )
			.arg( int( f.box[b + 5] ) ).arg( int( f.box[b + 6] ) ).arg( double( f.box[b + 3] ), 0, 'f', 1 );
	return QStringLiteral( "gi emissive grid: %1 fine boxes from %2 glowing surfels in %3 clusters (eth %4), %5 voxels (%6 air), "
		"atlas %7x%8x%9 = %10 KB (RGBA16F), rays %11 (%12 blocked), build ms %13; %14%15" )
		.arg( f.boxes ).arg( f.emitters ).arg( f.clusters ).arg( f.eth, 0, 'g', 3 ).arg( f.voxels ).arg( f.voxelsAir )
		.arg( f.width ).arg( f.height ).arg( f.layers ).arg( qint64( f.data.size() ) * 2 / 1024 )
		.arg( f.rays ).arg( f.raysBlocked ).arg( f.ms, 0, 'f', 0 ).arg( f.note ).arg( boxes );
}
