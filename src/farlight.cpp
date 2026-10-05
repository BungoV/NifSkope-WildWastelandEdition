/* lane FARVIEW1 (2026-10-04): distant light from the surfels, in switchable layers. The design is in farlight.h. */

#include "farlight.h"

#include "probeplace.h"
#include "proberelight.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

namespace {

struct Out
{
	QByteArray b;
	void raw( const void * p, int n ) { b.append( reinterpret_cast<const char *>( p ), n ); }
	void u8( quint8 v ) { raw( &v, 1 ); }
	void u16( quint16 v ) { v = qToLittleEndian( v ); raw( &v, 2 ); }
	void u32( quint32 v ) { v = qToLittleEndian( v ); raw( &v, 4 ); }
	void i32( qint32 v ) { u32( quint32( v ) ); }
	void u64( quint64 v ) { v = qToLittleEndian( v ); raw( &v, 8 ); }
	void f32( float v ) { quint32 u; std::memcpy( &u, &v, 4 ); u32( u ); }
	void zero( int n ) { b.append( n, '\0' ); }
};

template <typename T> T rd( const QByteArray & b, qsizetype at )
{
	T v;
	std::memcpy( &v, b.constData() + at, sizeof v );
	return qFromLittleEndian( v );
}

float rdf( const QByteArray & b, qsizetype at )
{
	const quint32 u = rd<quint32>( b, at );
	float f;
	std::memcpy( &f, &u, 4 );
	return f;
}

double lum( const double * c )
{
	return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2];
}

double lumf( const float * c )
{
	return 0.2126 * double( c[0] ) + 0.7152 * double( c[1] ) + 0.0722 * double( c[2] );
}

//! the side the normal faces most: 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z (FARVIEW1 section 3)
int sideOf( const double * n )
{
	int a = 0;
	for ( int k = 1; k < 3; k++ )
		if ( std::fabs( n[k] ) > std::fabs( n[a] ) )
			a = k;
	return 2 * a + ( n[a] >= 0.0 ? 0 : 1 );
}

//! the key cell: floor((p - 0.5 axis(side)) / cell) (the twin's tab.key)
void keyOf( const double * p, int side, double cell, int k[3] )
{
	for ( int a = 0; a < 3; a++ ) {
		const double nudge = ( a == side / 2 ) ? ( side & 1 ? -0.5 : 0.5 ) : 0.0;
		k[a] = int( std::floor( ( p[a] - nudge ) / cell ) );
	}
}

double p99Of( std::vector<double> v )
{
	if ( v.empty() )
		return 0.0;
	const size_t at = std::min( v.size() - 1, size_t( double( v.size() ) * 0.99 ) );
	std::nth_element( v.begin(), v.begin() + qsizetype( at ), v.end() );
	return v[at];
}

bool writeFile( const QString & path, const QByteArray & b, QString * err )
{
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) || f.write( b ) != b.size() ) {
		*err = QStringLiteral( "cannot write %1" ).arg( path );
		return false;
	}
	return true;
}

QString sectorName( int sx, int sy )
{
	char name[64];
	std::snprintf( name, sizeof name, "%+05d_%+05d", sx, sy );
	return QString::fromLatin1( name );
}

} // namespace

// ======================================================================== the bake

bool farLightBake( const ProbeRelightOps & X, const QString & dir, const FarLightBakeSpec & spec, FarLightBakeOut * out, QString * err )
{
	QElapsedTimer clock;
	clock.start();
	FarLightBakeOut & O = *out;
	O = FarLightBakeOut();
	if ( !X.built ) {
		*err = QStringLiteral( "no operators recorded" );
		return false;
	}
	const size_t nS = size_t( X.surfels ), nL = X.lights.size();
	// the groups: ALWAYS (0) first, then the switched keys ascending
	std::vector<quint64> keys;
	for ( const ProbeRelightOps::Light & L : X.lights )
		keys.push_back( L.groupKey );
	std::sort( keys.begin(), keys.end() );
	keys.erase( std::unique( keys.begin(), keys.end() ), keys.end() );
	O.groups = int( keys.size() );
	O.switched = int( std::count_if( keys.begin(), keys.end(), []( quint64 k ) { return k != 0; } ) );
	auto colorsFor = [&]( const std::function<bool( size_t )> & on ) {
		std::vector<float> c( nL * 3, 0.0f );
		for ( size_t l = 0; l < nL; l++ )
			if ( on( l ) )
				for ( int k = 0; k < 3; k++ )
					c[l * 3 + size_t( k )] = X.lights[l].color[k];
		return c;
	};
	auto run = [&]( const std::vector<float> & color, int passes, ProbeRelightOut * ro ) {
		ProbeRelightState st;
		st.color = color;
		st.passes = passes;
		st.placedOnly = true;
		st.wantE = true;
		return probeRelightCpu( X, st, ro, err );
	};
	// 1. the pass count: the all-on relight's settled count; 2. the all-on light at exactly that count
	const std::vector<float> allOn = colorsFor( []( size_t ) { return true; } );
	ProbeRelightOut settle, full;
	if ( !run( allOn, 0, &settle ) )
		return false;
	O.passes = settle.passes;
	if ( !run( allOn, O.passes, &full ) )
		return false;
	// 3. a layer per group at the same count (red "settle": each by its own stop rule)
	const bool redSettle = spec.red == QLatin1String( "settle" ), redDrop = spec.red == QLatin1String( "drop" );
	std::vector<std::vector<double>> layer( keys.size() );
	std::vector<int> layerPasses( keys.size(), 0 );
	for ( size_t g = 0; g < keys.size(); g++ ) {
		ProbeRelightOut lo;
		if ( !run( colorsFor( [&]( size_t l ) { return X.lights[l].groupKey == keys[g]; } ), redSettle ? 0 : O.passes, &lo ) )
			return false;
		layer[g] = std::move( lo.Es );
		layerPasses[g] = lo.passes;
	}
	// 4. gate L: the sum of the layers against the all-on light
	int dropped = -1;
	if ( redDrop ) {
		double best = -1.0;
		for ( size_t g = 0; g < keys.size(); g++ ) {
			if ( keys[g] == 0 && O.switched > 0 )
				continue;
			double s = 0.0;
			for ( double v : layer[g] )
				s += v;
			if ( s > best ) {
				best = s;
				dropped = int( g );
			}
		}
	}
	std::vector<double> lumAll( nS );
	double maxAll = 0.0, worst = 0.0;
	for ( size_t i = 0; i < nS; i++ ) {
		lumAll[i] = lum( &full.Es[i * 3] );
		for ( int c = 0; c < 3; c++ ) {
			double s = 0.0;
			for ( size_t g = 0; g < keys.size(); g++ )
				if ( int( g ) != dropped )
					s += layer[g][i * 3 + size_t( c )];
			worst = std::max( worst, std::fabs( s - full.Es[i * 3 + size_t( c )] ) );
			maxAll = std::max( maxAll, full.Es[i * 3 + size_t( c )] );
		}
	}
	const double p99All = p99Of( lumAll );
	O.gateL = p99All > 0.0 ? worst / p99All : worst;
	O.gateLmax = maxAll > 0.0 ? worst / maxAll : worst;
	QString passText;
	{
		int lo = 1 << 30, hi = 0;
		for ( int p : layerPasses ) {
			lo = std::min( lo, p );
			hi = std::max( hi, p );
		}
		passText = keys.empty() ? QStringLiteral( "-" ) : lo == hi ? QString::number( lo ) : QStringLiteral( "%1..%2" ).arg( lo ).arg( hi );
	}
	// 5. the files, per sector of the bake (the lights_X_Y.wlt beside carries the hash)
	QDir().mkpath( dir );
	const QRegularExpression re( QStringLiteral( "sector_([+-]?\\d+)_([+-]?\\d+)\\.tbk$" ) );
	const double cell = spec.cell;
	// the start state, for the checker's dump
	ProbeRelightOut start;
	if ( spec.dump && !run( colorsFor( [&]( size_t l ) { return X.lights[l].onAtStart; } ), O.passes, &start ) )
		return false;
	QStringList lines;
	for ( int f = 0; f < X.files.size(); f++ ) {
		const QRegularExpressionMatch m = re.match( X.files[f] );
		if ( !m.hasMatch() )
			continue;
		const int sx = m.captured( 1 ).toInt(), sy = m.captured( 2 ).toInt();
		const QString base = sectorName( sx, sy );
		QFile wf( QDir( dir ).filePath( QStringLiteral( "lights_%1.wlt" ).arg( base ) ) );
		if ( !wf.open( QIODevice::ReadOnly ) ) {
			*err = QStringLiteral( "no %1 beside (the light record writes it first)" ).arg( wf.fileName() );
			return false;
		}
		const QByteArray wlt = wf.readAll();
		if ( wlt.size() < 64 || !wlt.startsWith( "WLT1" ) ) {
			*err = QStringLiteral( "%1 is not a light table" ).arg( wf.fileName() );
			return false;
		}
		const quint64 hash = rd<quint64>( wlt, 32 );
		std::vector<int> own;
		std::vector<double> lumS;
		for ( size_t i = 0; i < nS; i++ )
			if ( X.sid[i * 2] == f ) {
				own.push_back( int( i ) );
				lumS.push_back( lumAll[i] );
			}
		const double p99 = p99Of( lumS );
		const double trimA = spec.trimAlways * p99, trimS = spec.trimSwitched * p99;
		const int g0 = ( !keys.empty() && keys[0] == 0 ) ? 0 : -1;
		struct R
		{
			int s;
			int k[3];
			int side;
			bool alwaysZero;
		};
		std::vector<R> recs;
		for ( int i : own ) {
			const size_t si = size_t( i );
			const double * Ea = g0 >= 0 ? &layer[size_t( g0 )][si * 3] : nullptr;
			const bool aOn = Ea && lum( Ea ) > trimA;
			bool sOn = false;
			for ( size_t g = 0; g < keys.size() && !sOn; g++ )
				sOn = keys[g] != 0 && lum( &layer[g][si * 3] ) > trimS;
			if ( !aOn && !sOn )
				continue;
			R r;
			r.s = i;
			r.side = sideOf( &X.nrm[si * 3] );
			keyOf( &X.pos[si * 3], r.side, cell, r.k );
			r.alwaysZero = !aOn;
			recs.push_back( r );
		}
		// the key bases: the smallest key of the sector's records (a probe links surfels cells past its sector)
		int kx0 = int( std::floor( sx * 4096.0 / cell ) ), ky0 = int( std::floor( sy * 4096.0 / cell ) ), kz0 = 0;
		if ( !recs.empty() ) {
			kx0 = recs[0].k[0];
			ky0 = recs[0].k[1];
			kz0 = recs[0].k[2];
			for ( const R & r : recs ) {
				kx0 = std::min( kx0, r.k[0] );
				ky0 = std::min( ky0, r.k[1] );
				kz0 = std::min( kz0, r.k[2] );
			}
		}
		std::sort( recs.begin(), recs.end(), []( const R & a, const R & b ) {
			for ( int k = 0; k < 3; k++ )
				if ( a.k[k] != b.k[k] )
					return a.k[k] < b.k[k];
			return a.side != b.side ? a.side < b.side : a.s < b.s;
		} );
		Out fvl, fvg, dump;
		bool range = true;
		for ( const R & r : recs ) {
			const int c[3] = { r.k[0] - kx0, r.k[1] - ky0, r.k[2] - kz0 };
			for ( int v : c )
				range = range && v >= 0 && v <= 65535;
			for ( int v : c )
				fvl.u16( quint16( v ) );
			fvl.u8( quint8( r.side ) );
			for ( int a = 0; a < 3; a++ ) {
				const double q = ( X.pos[size_t( r.s ) * 3 + size_t( a )] / cell - r.k[a] ) * 256.0;
				fvl.u8( quint8( std::min( std::max( std::floor( q ), 0.0 ), 255.0 ) ) );
			}
			float Ea[3] = { 0, 0, 0 };
			if ( g0 >= 0 && !r.alwaysZero )
				for ( int c2 = 0; c2 < 3; c2++ )
					Ea[c2] = float( layer[size_t( g0 )][size_t( r.s ) * 3 + size_t( c2 )] );
			fvl.u32( probeRgb9e5( Ea ) );
			fvl.u8( 0 );
			fvl.u8( r.alwaysZero ? 1 : 0 );
			if ( spec.dump ) {
				for ( int c2 = 0; c2 < 3; c2++ )
					dump.f32( float( full.Es[size_t( r.s ) * 3 + size_t( c2 )] ) );
				for ( int c2 = 0; c2 < 3; c2++ )
					dump.f32( float( start.Es[size_t( r.s ) * 3 + size_t( c2 )] ) );
			}
		}
		if ( !range ) {
			*err = QStringLiteral( "%1: a record's cell lies outside u16 from the sector's corner" ).arg( base );
			return false;
		}
		// the switched layers
		Out groups, entries;
		int nGroups = 0, nEntries = 0;
		for ( size_t g = 0; g < keys.size(); g++ ) {
			if ( keys[g] == 0 )
				continue;
			const int first = nEntries;
			for ( size_t r = 0; r < recs.size(); r++ ) {
				const double * E = &layer[g][size_t( recs[r].s ) * 3];
				if ( lum( E ) <= trimS )
					continue;
				const float Ef[3] = { float( E[0] ), float( E[1] ), float( E[2] ) };
				entries.u32( quint32( r ) );
				entries.u32( probeRgb9e5( Ef ) );
				nEntries++;
			}
			if ( nEntries == first )
				continue;
			groups.u64( keys[g] );
			groups.u32( quint32( first ) );
			groups.u32( quint32( nEntries - first ) );
			nGroups++;
		}
		Out head;
		head.raw( "FVL1", 4 );
		head.u32( 3 );
		head.i32( sx );
		head.i32( sy );
		head.f32( float( cell ) );
		head.u32( quint32( recs.size() ) );
		head.u64( hash );
		head.u32( nGroups > 0 ? 1u : 0u );
		head.i32( kz0 );
		head.u32( quint32( O.passes ) );
		head.f32( float( trimA ) );
		head.f32( float( trimS ) );
		head.i32( kx0 );
		head.i32( ky0 );
		head.zero( 4 );
		const QByteArray fvlBytes = head.b + fvl.b;
		if ( !writeFile( QDir( dir ).filePath( QStringLiteral( "farlight_%1.fvl" ).arg( base ) ), fvlBytes, err ) )
			return false;
		qint64 bytes = fvlBytes.size();
		const QString fvgPath = QDir( dir ).filePath( QStringLiteral( "farlight_%1.fvg" ).arg( base ) );
		if ( nGroups > 0 ) {
			Out gh;
			gh.raw( "FVG1", 4 );
			gh.u32( 1 );
			gh.i32( sx );
			gh.i32( sy );
			gh.u32( quint32( nGroups ) );
			gh.u32( quint32( nEntries ) );
			gh.u64( hash );
			const QByteArray fvgBytes = gh.b + groups.b + entries.b;
			if ( !writeFile( fvgPath, fvgBytes, err ) )
				return false;
			bytes += fvgBytes.size();
		} else {
			QFile::remove( fvgPath );
		}
		if ( spec.dump && !writeFile( QDir( dir ).filePath( QStringLiteral( "farcheck_%1.bin" ).arg( base ) ), dump.b, err ) )
			return false;
		O.sectors++;
		O.records += int( recs.size() );
		O.entries += nEntries;
		O.bytes += bytes;
		lines << QStringLiteral( "%1: %2 of %3 surfels lit, %4 switched groups %5 entries, %6 B, p99 %7" ).arg( base ).arg( recs.size() )
					 .arg( own.size() ).arg( nGroups ).arg( nEntries ).arg( bytes ).arg( p99, 0, 'g', 4 );
	}
	O.ms = clock.nsecsElapsed() / 1e6;
	O.census = QStringLiteral( "farlight (FARVIEW1): %1 groups (%2 switched), %3 passes fixed (layers ran %4), gate L %5 of the p99 "
		"(%6 of the max)%7; %8 sectors, %9 records, %10 switched entries, %11 B, %12 ms (%13)" )
		.arg( O.groups ).arg( O.switched ).arg( O.passes ).arg( passText ).arg( O.gateL, 0, 'g', 3 ).arg( O.gateLmax, 0, 'g', 3 )
		.arg( spec.red.isEmpty() ? QString() : QStringLiteral( " [red %1%2]" ).arg( spec.red )
			.arg( dropped >= 0 ? QStringLiteral( ": dropped %1" ).arg( keys[size_t( dropped )], 0, 16 ) : QString() ) )
		.arg( O.sectors ).arg( O.records ).arg( O.entries ).arg( O.bytes ).arg( O.ms, 0, 'f', 0 ).arg( lines.join( QStringLiteral( "; " ) ) );
	return true;
}

// ======================================================================== the bulb dots

int farLightDots( const ProbeSoup & soup, ProbeRelightOps & ops, float clear, QString * census )
{
	struct G
	{
		float c[3];
		double I[3];
	};
	std::vector<G> glow;
	const size_t nT = soup.tris.size() / 9;
	for ( size_t t = 0; t < nT; t++ ) {
		if ( !soup.glow.emits( int( t ) ) )
			continue;
		const float * v = &soup.tris[t * 9];
		double e1[3], e2[3];
		for ( int k = 0; k < 3; k++ ) {
			e1[k] = double( v[3 + k] ) - v[k];
			e2[k] = double( v[6 + k] ) - v[k];
		}
		const double cx = e1[1] * e2[2] - e1[2] * e2[1], cy = e1[2] * e2[0] - e1[0] * e2[2], cz = e1[0] * e2[1] - e1[1] * e2[0];
		const double area = 0.5 * std::sqrt( cx * cx + cy * cy + cz * cz );
		double le[3];
		soup.glow.le( int( t ), 1.0 / 3.0, 1.0 / 3.0, le );
		G g;
		for ( int k = 0; k < 3; k++ ) {
			g.c[k] = ( v[k] + v[3 + k] + v[6 + k] ) / 3.0f;
			g.I[k] = le[k] * area * 0.25;
		}
		glow.push_back( g );
	}
	int with = 0;
	double sumI = 0.0;
	const double c2 = double( clear ) * clear;
	for ( ProbeRelightOps::Light & L : ops.lights ) {
		double I[3] = { 0, 0, 0 };
		for ( const G & g : glow ) {
			const double dx = g.c[0] - L.pos[0], dy = g.c[1] - L.pos[1], dz = g.c[2] - L.pos[2];
			if ( dx * dx + dy * dy + dz * dz > c2 )
				continue;
			for ( int k = 0; k < 3; k++ )
				I[k] += g.I[k];
		}
		for ( int k = 0; k < 3; k++ )
			L.dot[k] = float( I[k] );
		if ( I[0] + I[1] + I[2] > 0.0 ) {
			with++;
			sumI += lum( I );
		}
	}
	*census = QStringLiteral( "farlight dots: %1 of %2 lights have a bulb (glowing triangles within %3 u; %4 glowing triangles), "
		"mean I_dot %5" ).arg( with ).arg( ops.lights.size() ).arg( double( clear ), 0, 'f', 0 ).arg( glow.size() )
		.arg( with ? sumI / with : 0.0, 0, 'g', 4 );
	return with;
}

// ======================================================================== the runtime set

bool farLightLoad( const QString & dir, FarLightSet * S, QString * err )
{
	*S = FarLightSet();
	const QDir d( dir );
	const QStringList fvls = d.entryList( { QStringLiteral( "farlight_*.fvl" ) }, QDir::Files, QDir::Name );
	if ( fvls.isEmpty() ) {
		*err = QStringLiteral( "no farlight_*.fvl in %1" ).arg( dir );
		return false;
	}
	std::map<quint64, size_t> groupAt;
	QHash<quint64, bool> startOf;   // group key -> on at the start (from the .wlt's)
	QStringList refused;
	for ( const QString & name : fvls ) {
		const QString base = name.mid( 9, name.size() - 13 );
		QFile ff( d.filePath( name ) ), fw( d.filePath( QStringLiteral( "lights_%1.wlt" ).arg( base ) ) );
		if ( !ff.open( QIODevice::ReadOnly ) || !fw.open( QIODevice::ReadOnly ) ) {
			refused << base + QStringLiteral( " (no .wlt)" );
			continue;
		}
		const QByteArray fvl = ff.readAll(), wlt = fw.readAll();
		if ( fvl.size() < 64 || !fvl.startsWith( "FVL1" ) || rd<quint32>( fvl, 4 ) != 3 || wlt.size() < 64 || !wlt.startsWith( "WLT1" ) ) {
			refused << base + QStringLiteral( " (not v2)" );
			continue;
		}
		const quint64 hash = rd<quint64>( fvl, 24 );
		QByteArray fvg;
		if ( rd<quint32>( fvl, 32 ) & 1u ) {
			QFile fg( d.filePath( QStringLiteral( "farlight_%1.fvg" ).arg( base ) ) );
			if ( !fg.open( QIODevice::ReadOnly ) ) {
				refused << base + QStringLiteral( " (.fvg missing)" );
				continue;
			}
			fvg = fg.readAll();
			if ( fvg.size() < 32 || !fvg.startsWith( "FVG1" ) || rd<quint64>( fvg, 24 ) != hash ) {
				refused << base + QStringLiteral( " (.fvg hash)" );
				continue;
			}
		}
		if ( rd<quint64>( wlt, 32 ) != hash ) {
			refused << base + QStringLiteral( " (.wlt hash)" );
			continue;
		}
		const int sx = rd<qint32>( fvl, 8 ), sy = rd<qint32>( fvl, 12 );
		const float cell = rdf( fvl, 16 );
		const quint32 n = rd<quint32>( fvl, 20 );
		const int kz0 = rd<qint32>( fvl, 36 );
		if ( fvl.size() < 64 + qsizetype( n ) * 16 ) {
			refused << base + QStringLiteral( " (short)" );
			continue;
		}
		S->cell = cell;
		const int kx0 = rd<qint32>( fvl, 52 ), ky0 = rd<qint32>( fvl, 56 );
		const size_t first = S->recs.size();
		for ( quint32 i = 0; i < n; i++ ) {
			const qsizetype at = 64 + qsizetype( i ) * 16;
			FarLightSet::Rec r;
			r.k[0] = kx0 + rd<quint16>( fvl, at );
			r.k[1] = ky0 + rd<quint16>( fvl, at + 2 );
			r.k[2] = kz0 + rd<quint16>( fvl, at + 4 );
			r.side = quint8( fvl[at + 6] );
			for ( int a = 0; a < 3; a++ )
				r.pos[a] = ( float( r.k[a] ) + ( float( quint8( fvl[at + 7 + a] ) ) + 0.5f ) / 256.0f ) * cell;
			probeRgb9e5Decode( rd<quint32>( fvl, at + 10 ), r.Ea );
			S->recs.push_back( r );
		}
		// the groups' start state and the dots, from the .wlt
		const quint32 nl = rd<quint32>( wlt, 16 ), ng = rd<quint32>( wlt, 20 );
		std::vector<quint64> gkey( ng );
		std::vector<bool> gon( ng );
		for ( quint32 g = 0; g < ng; g++ ) {
			gkey[g] = rd<quint64>( wlt, 64 + qsizetype( g ) * 16 );
			gon[g] = quint8( wlt[64 + qsizetype( g ) * 16 + 13] ) != 0;
			startOf.insert( gkey[g], gon[g] );
		}
		for ( quint32 l = 0; l < nl; l++ ) {
			const qsizetype at = 64 + qsizetype( ng ) * 16 + qsizetype( l ) * 64;
			const quint16 fl = rd<quint16>( wlt, at + 58 ), gi = rd<quint16>( wlt, at + 56 );
			if ( !( fl & 32 ) || gi >= ng )
				continue;
			FarLightSet::Dot dt;
			for ( int a = 0; a < 3; a++ )
				dt.pos[a] = rdf( wlt, at + 8 + a * 4 );
			// only the lights standing in this sector (a .wlt also lists the lights that merely reach it)
			if ( int( std::floor( dt.pos[0] / 4096.0f ) ) != sx || int( std::floor( dt.pos[1] / 4096.0f ) ) != sy )
				continue;
			probeRgb9e5Decode( rd<quint32>( wlt, at + 60 ), dt.I );
			dt.group = gkey[gi];
			dt.onAtStart = gon[gi];
			S->dots.push_back( dt );
		}
		if ( !fvg.isEmpty() ) {
			const quint32 nG = rd<quint32>( fvg, 16 ), nE = rd<quint32>( fvg, 20 );
			const qsizetype eAt = 32 + qsizetype( nG ) * 16;
			for ( quint32 g = 0; g < nG; g++ ) {
				const qsizetype at = 32 + qsizetype( g ) * 16;
				const quint64 key = rd<quint64>( fvg, at );
				const quint32 e0 = rd<quint32>( fvg, at + 8 ), ec = rd<quint32>( fvg, at + 12 );
				auto it = groupAt.find( key );
				if ( it == groupAt.end() ) {
					it = groupAt.emplace( key, S->groups.size() ).first;
					S->groups.emplace_back();
					S->groups.back().key = key;
				}
				FarLightSet::Group & G = S->groups[it->second];
				for ( quint32 e = e0; e < e0 + ec && e < nE; e++ ) {
					const qsizetype a2 = eAt + qsizetype( e ) * 8;
					const quint32 ri = rd<quint32>( fvg, a2 ) & 0xFFFFFFu;
					if ( ri >= n )
						continue;
					float E[3];
					probeRgb9e5Decode( rd<quint32>( fvg, a2 + 4 ), E );
					G.rec.push_back( int( first + ri ) );
					G.E.insert( G.E.end(), E, E + 3 );
				}
			}
		}
		S->sectors++;
	}
	for ( FarLightSet::Group & G : S->groups )
		G.onAtStart = startOf.value( G.key, true );
	S->refused = int( refused.size() );
	S->census = QStringLiteral( "far light: %1 sectors (%2 refused%3), %4 records, %5 switched groups, %6 dots" ).arg( S->sectors )
		.arg( refused.size() ).arg( refused.isEmpty() ? QString() : QStringLiteral( ": " ) + refused.join( QStringLiteral( ", " ) ) )
		.arg( S->recs.size() ).arg( S->groups.size() ).arg( S->dots.size() );
	if ( S->sectors == 0 ) {
		*err = S->census;
		return false;
	}
	return true;
}

void farLightSum( const FarLightSet & S, const std::function<bool( quint64, bool )> & on, std::vector<float> & E )
{
	E.assign( S.recs.size() * 3, 0.0f );
	if ( on( 0, true ) )
		for ( size_t r = 0; r < S.recs.size(); r++ )
			for ( int c = 0; c < 3; c++ )
				E[r * 3 + size_t( c )] = S.recs[r].Ea[c];
	for ( const FarLightSet::Group & G : S.groups ) {
		if ( !on( G.key, G.onAtStart ) )
			continue;
		for ( size_t e = 0; e < G.rec.size(); e++ )
			for ( int c = 0; c < 3; c++ )
				E[size_t( G.rec[e] ) * 3 + size_t( c )] += G.E[e * 3 + size_t( c )];
	}
}

void farLightTables( const FarLightSet & S, const std::vector<float> & E, std::vector<float> & slotTab, std::vector<float> & recs,
	int * bits, int * maxProbe )
{
	int b = 4;
	while ( ( size_t( 1 ) << b ) < S.recs.size() * 2 )
		b++;
	*bits = b;
	const quint32 mask = ( 1u << b ) - 1u;
	slotTab.assign( ( size_t( 1 ) << b ) * 4, 0.0f );
	recs.assign( S.recs.size() * 8, 0.0f );
	int longest = 0;
	for ( size_t r = 0; r < S.recs.size(); r++ ) {
		const FarLightSet::Rec & R = S.recs[r];
		for ( int a = 0; a < 3; a++ ) {
			recs[r * 8 + size_t( a )] = R.pos[a];
			recs[r * 8 + 4 + size_t( a )] = E[r * 3 + size_t( a )];
		}
		quint32 h = farLightHash( R.k[0], R.k[1], R.k[2], R.side ) & mask;
		int walked = 1;
		while ( slotTab[size_t( h ) * 4 + 3] != 0.0f ) {
			h = ( h + 1u ) & mask;
			walked++;
		}
		slotTab[size_t( h ) * 4 + 0] = float( R.k[0] );
		slotTab[size_t( h ) * 4 + 1] = float( R.k[1] );
		slotTab[size_t( h ) * 4 + 2] = float( R.k[2] * 8 + R.side );
		slotTab[size_t( h ) * 4 + 3] = float( r + 1 );
		longest = std::max( longest, walked );
	}
	*maxProbe = longest;
}
