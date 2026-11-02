// Lane ESPWRITE1: the standalone gate tool for src/io/espwrite.cpp (QtCore only, no exe; skill
// ww-standalone-writer-gate). Modes:
//   --build <plan.json> <out.esp> <Data dir>   gate 1's C++ side: resolve the plan in the master, write the plugin
//   --yaw <niftypes.cpp> <cellview.cpp>        gate 3: yaw toward a target, against the cell view's draw rule
//   --reemit <plugin> [--drop]                 gate 4: rebuild a shipped file's CELL/WRLD trees with derived labels
#include "espwrite.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>

#include <cmath>
#include <cstdio>
#include <cstring>

using namespace EspWrite;

static int checks = 0, failures = 0;

static void check( bool ok, const QString & what )
{
	++checks;
	if ( !ok ) {
		++failures;
		std::printf( "FAIL %s\n", qPrintable( what ) );
	}
}

static quint32 hexForm( const QJsonValue & v )
{
	return v.toString().toUInt( nullptr, 16 );
}

static Copied copiedFrom( const PluginIndex & idx, quint32 form )
{
	Copied c;
	c.raw = idx.recordBytes( form );
	c.source = idx.name();
	c.sourceMasters = idx.masters();
	c.sourceLocalized = ( idx.tes4Flags() & kTes4Localized ) != 0;
	return c;
}

static int doBuild( const QString & planPath, const QString & outPath, const QString & dataDir )
{
	QFile pf( planPath );
	if ( !pf.open( QIODevice::ReadOnly ) ) {
		std::printf( "FAIL cannot read %s\n", qPrintable( planPath ) );
		return 2;
	}
	QJsonObject j = QJsonDocument::fromJson( pf.readAll() ).object();
	Plan plan;
	for ( const QJsonValue & m : j.value( "masters" ).toArray() )
		plan.masters << m.toString();
	plan.light = j.value( "light" ).toBool();
	plan.author = j.value( "author" ).toString( QStringLiteral( "NifSkope WW" ) );
	if ( plan.masters.size() != 1 ) {
		std::printf( "FAIL the fixture resolves records in one master\n" );
		return 2;
	}
	PluginIndex idx;
	QString err;
	if ( !idx.open( dataDir + QLatin1Char( '/' ) + plan.masters.first(), &err ) ) {
		std::printf( "FAIL %s\n", qPrintable( err ) );
		return 2;
	}
	for ( const QJsonValue & cv : j.value( "cells" ).toArray() ) {
		QJsonObject c = cv.toObject();
		CellEdit e;
		quint32 cf = hexForm( c.value( "cell" ) );
		e.cell = copiedFrom( idx, cf );
		if ( c.value( "world" ).isString() ) {
			e.world = hexForm( c.value( "world" ) );
			e.worldRec = copiedFrom( idx, e.world );
			e.persistentCell = copiedFrom( idx, idx.persistentCell( e.world ) );
		}
		for ( const QJsonValue & av : c.value( "adds" ).toArray() ) {
			QJsonObject a = av.toObject();
			RefAdd r;
			r.base = hexForm( a.value( "base" ) );
			QJsonArray p = a.value( "pos" ).toArray(), q = a.value( "rot" ).toArray();
			for ( int i = 0; i < 3; ++i ) {
				r.pos[i] = float( p.at( i ).toDouble() );
				r.rot[i] = float( q.at( i ).toDouble() );
			}
			r.scale = float( a.value( "scale" ).toDouble( 1.0 ) );
			r.persistent = a.value( "persistent" ).toBool();
			e.adds.append( r );
		}
		for ( const QJsonValue & dv : c.value( "disable" ).toArray() ) {
			quint32 f = hexForm( dv );
			RefOverride o;
			o.rec = copiedFrom( idx, f );
			o.rec.raw = withFlags( o.rec.raw, kRefDisabled );
			QVector<PluginIndex::Group> ch = idx.chain( f );
			o.persistent = !ch.isEmpty() && ch.first().type == 8;
			e.overrides.append( o );
		}
		plan.cells.append( e );
	}
	Report rep;
	if ( !EspWrite::write( outPath, plan, &rep, &err ) ) {
		std::printf( "REFUSED %s\n", qPrintable( err ) );
		return 1;
	}
	std::printf( "%s\n", qPrintable( rep.line( QFileInfo( outPath ).fileName() ) ) );
	for ( const CellEdit & e : plan.cells )
		for ( const RefAdd & a : e.adds )
			std::printf( "  new REFR %08X base %08X\n", a.formOut, a.base );
	return 0;
}

// ------------------------------------------------------------------ gate 3
static void fromEuler( float x, float y, float z, float m[3][3] )
{
	// niftypes.cpp Matrix::fromEuler, verbatim (doYaw checks the source still reads this way)
	float sinX = std::sin( x ), cosX = std::cos( x ), sinY = std::sin( y ), cosY = std::cos( y );
	float sinZ = std::sin( z ), cosZ = std::cos( z );
	m[0][0] = cosY * cosZ;
	m[0][1] = -cosY * sinZ;
	m[0][2] = sinY;
	m[1][0] = sinX * sinY * cosZ + sinZ * cosX;
	m[1][1] = cosX * cosZ - sinX * sinY * sinZ;
	m[1][2] = -sinX * cosY;
	m[2][0] = sinX * sinZ - cosX * sinY * cosZ;
	m[2][1] = cosX * sinY * sinZ + sinX * cosZ;
	m[2][2] = cosX * cosY;
}

static int doYaw( const QString & niftypes, const QString & cellview )
{
	QFile a( niftypes ), b( cellview );
	QByteArray nt = a.open( QIODevice::ReadOnly ) ? a.readAll() : QByteArray();
	QByteArray cv = b.open( QIODevice::ReadOnly ) ? b.readAll() : QByteArray();
	const char * pins[] = { "m[0][0] = cosY * cosZ;", "m[0][1] = -cosY * sinZ;", "m[0][2] = sinY;",
		"m[1][0] = sinX * sinY * cosZ + sinZ * cosX;", "m[1][1] = cosX * cosZ - sinX * sinY * sinZ;",
		"m[1][2] = -sinX * cosY;", "m[2][0] = sinX * sinZ - cosX * sinY * cosZ;",
		"m[2][1] = cosX * sinY * sinZ + sinX * cosZ;", "m[2][2] = cosX * cosY;" };
	for ( const char * p : pins )
		check( nt.contains( p ), QStringLiteral( "pin: niftypes.cpp fromEuler no longer reads \"%1\"" ).arg( p ) );
	check( cv.contains( "fromEuler( -r.rot[0], -r.rot[1], -r.rot[2] )" ),
		QStringLiteral( "pin: cellview.cpp no longer places a ref with fromEuler( -rot )" ) );
	struct Case { const char * name; float to[3]; double want; };
	const double pi = 3.14159265358979323846;
	Case cases[] = { { "yaw 0", { 0, 100, 0 }, 0.0 }, { "yaw 90", { 100, 0, 0 }, pi / 2 },
		{ "yaw 180", { 0, -100, 0 }, pi }, { "yaw -45 toward a target", { -70.7106781f, 70.7106781f, 0 }, -pi / 4 },
		{ "yaw 30 off-origin", { 1050.0f, 1000.0f + 86.6025404f, 37.0f }, pi / 6 } };
	for ( const Case & c : cases ) {
		float from[3] = { 0, 0, 0 };
		if ( std::strcmp( c.name, "yaw 30 off-origin" ) == 0 ) {
			from[0] = 1000.0f;
			from[1] = 1000.0f;
			from[2] = 37.0f;
		}
		float rot[3];
		yawToward( from, c.to, rot );
		float m[3][3];
		fromEuler( -rot[0], -rot[1], -rot[2], m );
		double fx = m[0][1], fy = m[1][1], fz = m[2][1];    // M * (0, 1, 0)
		double dx = c.to[0] - from[0], dy = c.to[1] - from[1];
		double len = std::sqrt( dx * dx + dy * dy );
		double ex = std::fabs( fx - dx / len ) + std::fabs( fy - dy / len ) + std::fabs( fz );
		check( ex < 1e-5, QStringLiteral( "yaw: %1 faces (%2, %3, %4), the target is (%5, %6, 0) (rz %7)" )
			.arg( c.name ).arg( fx ).arg( fy ).arg( fz ).arg( dx / len ).arg( dy / len ).arg( rot[2] ) );
		double dw = std::fabs( std::remainder( double( rot[2] ) - c.want, 2 * pi ) );
		check( dw < 1e-5 && rot[0] == 0.0f && rot[1] == 0.0f,
			QStringLiteral( "yaw: %1 wrote (%2, %3, %4), want (0, 0, %5)" ).arg( c.name ).arg( rot[0] ).arg( rot[1] )
			.arg( rot[2] ).arg( c.want ) );
	}
	std::printf( "gate 3 yaw: %d checks, %d failures\n", checks, failures );
	return failures ? 1 : 0;
}

// ------------------------------------------------------------------ gate 4
static int labelMismatch[16];

static bool firstCell( const Node & n, QByteArray & out )
{
	for ( const Node & k : n.kids ) {
		if ( !k.isGroup && recordSig( k.raw ) == "CELL" ) {
			out = k.raw;
			return true;
		}
		if ( k.isGroup && firstCell( k, out ) )
			return true;
	}
	return false;
}

//! Rebuild every label from the records under it (the writer's rules), counting where the file disagrees.
static bool relabel( Node & g, QString * err )
{
	quint32 prevForm = 0;
	for ( Node & k : g.kids ) {
		if ( !k.isGroup ) {
			prevForm = recordForm( k.raw );
			continue;
		}
		quint32 want = k.label;
		QByteArray cell;
		int x = 0, y = 0;
		switch ( k.type ) {
		case 1: case 6: want = prevForm; break;
		case 8: case 9: want = g.label; break;
		case 2: case 3:
			if ( !firstCell( k, cell ) ) {
				*err = QStringLiteral( "block without a CELL" );
				return false;
			}
			want = k.type == 2 ? interiorBlockLabel( recordForm( cell ) ) : interiorSubLabel( recordForm( cell ) );
			break;
		case 4: case 5:
			if ( !firstCell( k, cell ) || !cellGrid( cell, x, y, err ) )
				return false;
			want = k.type == 4 ? exteriorBlockLabel( x, y ) : exteriorSubLabel( x, y );
			break;
		default: break;
		}
		if ( want != k.label )
			++labelMismatch[k.type & 15];
		k.label = want;
		if ( !relabel( k, err ) )
			return false;
	}
	return true;
}

static int orderBreaks( const Node & g )
{
	int n = 0;
	quint32 lastLab[16] = {};
	bool seen[16] = {};
	quint32 lastCell = 0;
	bool cellSeen = false;
	for ( const Node & k : g.kids ) {
		if ( !k.isGroup ) {
			if ( recordSig( k.raw ) == "CELL" ) {
				if ( cellSeen && recordForm( k.raw ) < lastCell )
					++n;
				lastCell = recordForm( k.raw );
				cellSeen = true;
			}
			continue;
		}
		int t = k.type & 15;
		if ( t >= 2 && t <= 5 ) {
			if ( seen[t] && k.label < lastLab[t] )
				++n;
			lastLab[t] = k.label;
			seen[t] = true;
		}
		n += orderBreaks( k );
	}
	return n;
}

static void zeroStamps( QByteArray & b, qsizetype p, qsizetype end )
{
	while ( p < end ) {
		quint32 size = qFromLittleEndian<quint32>( b.constData() + p + 4 );
		if ( std::memcmp( b.constData() + p, "GRUP", 4 ) == 0 ) {
			std::memset( b.data() + p + 16, 0, 8 );
			zeroStamps( b, p + 24, p + size );
			p += size;
		} else {
			p += 24 + size;
		}
	}
}

static bool dropOne( Node & g )
{
	for ( Node & k : g.kids ) {
		if ( k.isGroup && k.type == 6 && !k.kids.isEmpty() ) {
			k.kids.removeLast();
			return true;
		}
		if ( k.isGroup && dropOne( k ) )
			return true;
	}
	return false;
}

static int doReemit( const QString & path, bool drop )
{
	PluginIndex idx;
	QString err;
	if ( !idx.open( path, &err ) ) {
		std::printf( "FAIL %s\n", qPrintable( err ) );
		return 2;
	}
	const QByteArray & file = idx.bytes();
	QVector<Node> top;
	if ( !parseNodes( file, idx.tes4End(), file.size(), top, &err ) ) {
		std::printf( "FAIL parse: %s\n", qPrintable( err ) );
		return 2;
	}
	qsizetype p = idx.tes4End();
	int trees = 0;
	for ( Node & t : top ) {
		quint32 size = qFromLittleEndian<quint32>( file.constData() + p + 4 );
		if ( t.isGroup && ( t.label == label4( "CELL" ) || t.label == label4( "WRLD" ) ) ) {
			++trees;
			std::memset( labelMismatch, 0, sizeof labelMismatch );
			Node n = t;
			bool ok = relabel( n, &err );
			check( ok, QStringLiteral( "relabel: %1" ).arg( err ) );
			if ( drop )
				dropOne( n );
			int recs = 0, grps = 0;
			QByteArray mine = emitNode( n, &recs, &grps );
			QByteArray theirs = file.mid( p, size );
			zeroStamps( theirs, 0, theirs.size() );
			const char * lab = t.label == label4( "CELL" ) ? "CELL" : "WRLD";
			check( mine.size() == theirs.size(), QStringLiteral( "size: %1 tree rebuilt %2 bytes, the file %3" )
				.arg( lab ).arg( mine.size() ).arg( theirs.size() ) );
			check( mine == theirs, QStringLiteral( "bytes: %1 tree differs from the file" ).arg( lab ) );
			QStringList mm;
			for ( int i = 0; i < 16; ++i )
				if ( labelMismatch[i] )
					mm << QStringLiteral( "type %1: %2" ).arg( i ).arg( labelMismatch[i] );
			check( mm.isEmpty(), QStringLiteral( "label: %1 labels the rules do not derive (%2)" ).arg( lab, mm.join( ", " ) ) );
			int ob = orderBreaks( t );
			check( ob == 0, QStringLiteral( "order: %1 %2 siblings out of the sort rule" ).arg( lab ).arg( ob ) );
			std::printf( "  %s tree: %d records, %d groups, %lld bytes%s\n", lab, recs, grps,
				(long long)mine.size(), mine == theirs ? ", byte-identical" : "" );
		}
		p += t.isGroup ? qsizetype( size ) : 24 + qsizetype( size );
	}
	check( trees > 0, QStringLiteral( "no CELL / WRLD tree in %1" ).arg( path ) );
	std::printf( "gate 4 %s%s: %d checks, %d failures\n", qPrintable( QFileInfo( path ).fileName() ),
		drop ? " (red: one group dropped)" : "", checks, failures );
	return failures ? 1 : 0;
}

// ESPWRITE2: the body of tests/espwrite/fixture_main.cpp and of `NifSkope -no-gui espwrite build|yaw|reemit`
// (a[0] = the program, a[1] = the mode)
int espwriteGatesMain( const QStringList & a )
{
	checks = failures = 0;
	if ( a.size() >= 5 && a[1] == "--build" )
		return doBuild( a[2], a[3], a[4] );
	if ( a.size() >= 4 && a[1] == "--yaw" )
		return doYaw( a[2], a[3] );
	if ( a.size() >= 3 && a[1] == "--reemit" )
		return doReemit( a[2], a.size() >= 4 && a[3] == "--drop" );
	std::printf( "usage: --build <plan> <out> <data> | --yaw <niftypes.cpp> <cellview.cpp> | --reemit <plugin> [--drop]\n" );
	return 2;
}
