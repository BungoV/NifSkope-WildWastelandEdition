/* lane AIRZONE1 (2026-10-29): the hand-placed volumes' files (src/airzone.cpp) through the real recx reader
 * (src/recx.cpp), no GL and no window.
 *
 *   airzone_recx_gate <out dir>        (fixtures are written under it; never into a game Data folder)
 *
 * Checks (each prints PASS/FAIL <name>: ...; exit 1 on any FAIL):
 *   roundtrip   40 random volumes (every block, odd floats) written and read back through wwRecxFind: every number
 *               equal (a -0 reads 0), no warning; writing what was read gives the same bytes
 *   keep        writing a cell's entry into a file that holds another cell's entry and a field we do not know keeps both
 *   gi-all      the converter (airZoneFromGi) of a 3x3 exterior block's GI set, read back and merged: the GI's list equal
 *               to today's, number for number and in order (and the census hash); 4 of 9 cells converted: the same;
 *               an interior: the same
 *   gi-rooms    a "rooms" file naming room 3 (a box) and the opening 3|0: today's room-3 shapes and 3|0 thresholds go,
 *               the rest keep their places, the hand ones follow
 *   novol       with WW_CELL_AIRZONE_RED=novol the same files are ignored: no volumes, the set untouched
 *   refuse      a bad volume (no size, unknown shape) is skipped with a warning; clamped values say so
 * Red: the whole gate run with WW_CELL_AIRZONE_RED=novol must FAIL (roundtrip, keep, gi-*): tests/airzone/gate.sh. */

#include "airzone.h"
#include "proberooms.h"
#include "recx.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>
#include <cstdio>
#include <random>

namespace {

int g_fail = 0, g_pass = 0;

void check( const char * name, bool ok, const QString & msg )
{
	std::printf( "%s %s: %s\n", ok ? "PASS" : "FAIL", name, qPrintable( msg ) );
	( ok ? g_pass : g_fail )++;
}

float oddFloat( std::mt19937 & rng, float lo, float hi )
{
	std::uniform_real_distribution<double> U( 0.0, 1.0 );
	const double u = U( rng );
	if ( u < 0.05 )
		return 0.1f;
	if ( u < 0.10 )
		return 1.0f / 3.0f;
	if ( u < 0.13 )
		return -0.0f;
	if ( u < 0.16 )
		return 1e-6f;
	return float( lo + ( hi - lo ) * U( rng ) );
}

az::Volume randomVolume( std::mt19937 & rng, int i )
{
	std::uniform_real_distribution<double> U( 0.0, 1.0 );
	az::Volume v;
	v.name = QStringLiteral( "zone %1" ).arg( i ).toStdString();
	v.shape.type = i % 5;
	for ( int c = 0; c < 3; c++ ) {
		v.shape.center[c] = oddFloat( rng, -40000.0f, 40000.0f );
		v.shape.size[c] = std::max( 1.0f, std::abs( oddFloat( rng, 1.0f, 5000.0f ) ) );
		v.shape.rotation[c] = oddFloat( rng, -180.0f, 180.0f );
	}
	v.edge = std::abs( oddFloat( rng, 0.0f, 300.0f ) );
	v.priority = int( U( rng ) * 20.0 ) - 10;
	if ( U( rng ) < 0.6 ) {
		v.gi.on = true;
		v.gi.threshold = U( rng ) < 0.3;
		v.gi.room = 1 + int( U( rng ) * 40.0 );
		v.gi.rooms[0] = int( U( rng ) * 40.0 );
		v.gi.rooms[1] = int( U( rng ) * 40.0 );
		v.gi.blendWidth = v.gi.threshold ? std::abs( oddFloat( rng, 0.0f, 200.0f ) ) : 0.0f;
		v.gi.seq = U( rng ) < 0.5 ? -1 : int( U( rng ) * 500.0 );
	}
	if ( U( rng ) < 0.7 ) {
		v.fog.on = true;
		v.fog.scale = std::abs( oddFloat( rng, 0.0f, 8.0f ) );
		v.fog.add = std::abs( oddFloat( rng, 0.0f, 50.0f ) );
		v.fog.heightFalloff = std::abs( oddFloat( rng, 0.0f, 2000.0f ) );
		for ( int c = 0; c < 3; c++ ) {
			v.fog.colour[c] = std::abs( oddFloat( rng, 0.0f, 4.0f ) );
			v.fog.noiseDrift[c] = oddFloat( rng, -50.0f, 50.0f );
		}
		v.fog.colourStrength = float( U( rng ) );
		v.fog.noise = float( U( rng ) );
		v.fog.noiseScale = std::max( 1.0f, std::abs( oddFloat( rng, 1.0f, 2000.0f ) ) );
	}
	if ( U( rng ) < 0.6 ) {
		v.part.on = true;
		v.part.density = std::abs( oddFloat( rng, 0.0f, 8.0f ) );
		v.part.light = std::abs( oddFloat( rng, 0.0f, 8.0f ) );
		for ( int c = 0; c < 3; c++ ) {
			v.part.tint[c] = std::abs( oddFloat( rng, 0.0f, 4.0f ) );
			v.part.drift[c] = oddFloat( rng, -50.0f, 50.0f );
		}
		v.part.tintStrength = float( U( rng ) );
		v.part.type = i % 2 ? "dust" : "";
		v.part.size = std::abs( oddFloat( rng, 0.0f, 8.0f ) );
	}
	if ( !v.gi.on && !v.fog.on && !v.part.on )
		v.fog.on = true;	// a volume with no block is legal but warns ("it does nothing"); the roundtrip wants none
	return v;
}

bool eqf( float a, float b )
{
	return a == b;	// -0 == 0: the writer writes a zero as 0
}

bool sameVolume( const az::Volume & a, const az::Volume & b, QString * why )
{
	auto bad = [&]( const char * what ) {
		*why = QStringLiteral( "%1 differs (%2)" ).arg( QLatin1String( what ), QString::fromStdString( a.name ) );
		return false;
	};
	if ( a.name != b.name || a.shape.type != b.shape.type )
		return bad( "name/shape" );
	for ( int c = 0; c < 3; c++ )
		if ( !eqf( a.shape.center[c], b.shape.center[c] ) || !eqf( a.shape.size[c], b.shape.size[c] ) || !eqf( a.shape.rotation[c], b.shape.rotation[c] ) )
			return bad( "transform" );
	if ( !eqf( a.edge, b.edge ) || a.priority != b.priority )
		return bad( "edge/priority" );
	if ( a.gi.on != b.gi.on || ( a.gi.on && ( a.gi.threshold != b.gi.threshold || a.gi.seq != b.gi.seq
		|| ( a.gi.threshold ? ( a.gi.rooms[0] != b.gi.rooms[0] || a.gi.rooms[1] != b.gi.rooms[1] || !eqf( a.gi.blendWidth, b.gi.blendWidth ) )
			: a.gi.room != b.gi.room ) ) ) )
		return bad( "gi" );
	if ( a.fog.on != b.fog.on )
		return bad( "fog on" );
	if ( a.fog.on ) {
		bool ok = eqf( a.fog.scale, b.fog.scale ) && eqf( a.fog.add, b.fog.add ) && eqf( a.fog.heightFalloff, b.fog.heightFalloff )
			&& eqf( a.fog.colourStrength, b.fog.colourStrength ) && eqf( a.fog.noise, b.fog.noise ) && eqf( a.fog.noiseScale, b.fog.noiseScale );
		for ( int c = 0; c < 3; c++ )
			ok = ok && eqf( a.fog.colour[c], b.fog.colour[c] ) && eqf( a.fog.noiseDrift[c], b.fog.noiseDrift[c] );
		if ( !ok )
			return bad( "fog" );
	}
	if ( a.part.on != b.part.on )
		return bad( "particles on" );
	if ( a.part.on ) {
		bool ok = eqf( a.part.density, b.part.density ) && eqf( a.part.light, b.part.light ) && eqf( a.part.tintStrength, b.part.tintStrength )
			&& eqf( a.part.size, b.part.size ) && a.part.type == b.part.type;
		for ( int c = 0; c < 3; c++ )
			ok = ok && eqf( a.part.tint[c], b.part.tint[c] ) && eqf( a.part.drift[c], b.part.drift[c] );
		if ( !ok )
			return bad( "particles" );
	}
	return true;
}

GiVolumeSet randomSet( std::mt19937 & rng, bool exterior )
{
	std::uniform_real_distribution<double> U( 0.0, 1.0 );
	GiVolumeSet s;
	s.source = QStringLiteral( "gate" );
	s.census = QStringLiteral( "gate set" );
	const double lo = exterior ? -4096.0 - 1500.0 : -2000.0, hi = exterior ? 8192.0 + 1500.0 : 2000.0;	// some past the 3x3
	for ( int i = 0; i < 120; i++ ) {
		GiVolumeShape v;
		v.type = int( U( rng ) * 5.0 ) % 5;
		for ( int c = 0; c < 3; c++ ) {
			v.center[c] = c < 2 ? float( lo + ( hi - lo ) * U( rng ) ) : float( U( rng ) * 600.0 );
			v.size[c] = float( 16.0 + U( rng ) * 900.0 );
			v.rotation[c] = U( rng ) < 0.7 ? 0.0f : float( ( U( rng ) - 0.5 ) * 360.0 );
		}
		v.room = 1 + int( U( rng ) * 12.0 );
		s.shapes.push_back( v );
	}
	for ( int i = 0; i < 30; i++ ) {
		GiThreshold t;
		t.shape.type = 0;
		for ( int c = 0; c < 3; c++ ) {
			t.shape.center[c] = c < 2 ? float( lo + ( hi - lo ) * U( rng ) ) : float( U( rng ) * 300.0 );
			t.shape.size[c] = float( 8.0 + U( rng ) * 200.0 );
			t.shape.rotation[c] = c == 2 ? float( U( rng ) * 360.0 ) : 0.0f;
		}
		t.rooms[0] = i % 7 == 0 ? 3 : int( U( rng ) * 12.0 );
		t.rooms[1] = i % 7 == 0 ? 0 : 1 + int( U( rng ) * 12.0 );
		t.blendWidth = U( rng ) < 0.5 ? 0.0f : float( U( rng ) * 96.0 );
		s.thresholds.push_back( t );
	}
	return s;
}

bool sameSet( const GiVolumeSet & a, const GiVolumeSet & b, QString * why )
{
	if ( a.shapes.size() != b.shapes.size() || a.thresholds.size() != b.thresholds.size() ) {
		*why = QStringLiteral( "sizes %1/%2 vs %3/%4" ).arg( a.shapes.size() ).arg( a.thresholds.size() ).arg( b.shapes.size() ).arg( b.thresholds.size() );
		return false;
	}
	for ( size_t i = 0; i < a.shapes.size(); i++ ) {
		const GiVolumeShape & x = a.shapes[i], & y = b.shapes[i];
		bool ok = x.type == y.type && x.room == y.room;
		for ( int c = 0; c < 3; c++ )
			ok = ok && eqf( x.center[c], y.center[c] ) && eqf( x.size[c], y.size[c] ) && eqf( x.rotation[c], y.rotation[c] );
		if ( !ok ) {
			*why = QStringLiteral( "shape %1 differs" ).arg( i );
			return false;
		}
	}
	for ( size_t i = 0; i < a.thresholds.size(); i++ ) {
		const GiThreshold & x = a.thresholds[i], & y = b.thresholds[i];
		bool ok = x.shape.type == y.shape.type && x.rooms[0] == y.rooms[0] && x.rooms[1] == y.rooms[1] && eqf( x.blendWidth, y.blendWidth );
		for ( int c = 0; c < 3; c++ )
			ok = ok && eqf( x.shape.center[c], y.shape.center[c] ) && eqf( x.shape.size[c], y.shape.size[c] )
				&& eqf( x.shape.rotation[c], y.shape.rotation[c] );
		if ( !ok ) {
			*why = QStringLiteral( "threshold %1 differs" ).arg( i );
			return false;
		}
	}
	return true;
}

QByteArray readAll( const QString & p )
{
	QFile f( p );
	return f.open( QIODevice::ReadOnly ) ? f.readAll() : QByteArray();
}

QString freshRoot( const QString & out, const QString & name )
{
	const QString r = QDir( out ).filePath( name );
	QDir( r ).removeRecursively();
	QDir().mkpath( r );
	return r;
}

std::vector<AirZoneCell> block3x3()
{
	std::vector<AirZoneCell> cells;
	for ( int y = -1; y <= 1; y++ )
		for ( int x = -1; x <= 1; x++ ) {
			AirZoneCell c;
			c.interior = false;
			c.plugin = QStringLiteral( "Fallout4.esm" );
			c.localId = 0x0000A000u + quint32( ( y + 1 ) * 3 + ( x + 1 ) );
			c.worldPlugin = QStringLiteral( "Fallout4.esm" );
			c.worldLocalId = 0x0000003Cu;
			c.worldEdid = QStringLiteral( "Commonwealth" );
			c.gx = x;
			c.gy = y;
			cells.push_back( c );
		}
	return cells;
}

} // namespace

int main( int argc, char ** argv )
{
	QCoreApplication app( argc, argv );
	if ( argc < 2 ) {
		std::printf( "usage: airzone_recx_gate <out dir>\n" );
		return 2;
	}
	const QString out = QDir( QString::fromLocal8Bit( argv[1] ) ).absolutePath();
	QDir().mkpath( out );
	const QString red = wwAirZoneRed();	// a red run (WW_CELL_AIRZONE_RED=novol) must fail the checks below
	std::printf( "airzone recx gate: out %s red '%s'\n", qPrintable( out ), qPrintable( red ) );
	std::mt19937 rng( 29 );

	// ---- roundtrip
	{
		const QString root = freshRoot( out, QStringLiteral( "rt" ) );
		const QStringList plugins = { root + QStringLiteral( "/Fallout4.esm" ), root + QStringLiteral( "/ExampleMod.esp" ) };
		AirZoneCell c;
		c.plugin = QStringLiteral( "ExampleMod.esp" );
		c.localId = 0x00000801u;
		c.edid = QStringLiteral( "ExampleCell" );
		std::vector<az::Volume> v;
		for ( int i = 0; i < 40; i++ )
			v.push_back( randomVolume( rng, i ) );
		const QString path = airZoneFilePath( root, QStringLiteral( "FO4CS" ), c );
		QString err;
		const bool wrote = airZoneWriteEntry( path, c, v, QStringLiteral( "rooms" ), &err );
		const std::vector<AirZoneCellVolumes> got = airZoneLoad( { root }, plugins, { c }, red );
		QString why;
		bool same = wrote && got.size() == 1 && got[0].volumes.size() == v.size() && got[0].warnings.isEmpty();
		for ( size_t i = 0; same && i < v.size(); i++ )
			same = sameVolume( v[i], got[0].volumes[i], &why );
		if ( !same && why.isEmpty() )
			why = QStringLiteral( "wrote %1 (%2); read %3 volume(s), %4 warning(s) %5" ).arg( wrote ).arg( err )
				.arg( got.empty() ? 0 : int( got[0].volumes.size() ) ).arg( got.empty() ? 0 : int( got[0].warnings.size() ) )
				.arg( got.empty() ? QString() : got[0].warnings.join( QStringLiteral( "; " ) ) );
		const QString path2 = QDir( out ).filePath( QStringLiteral( "rt_again.recx" ) );
		QFile::remove( path2 );
		bool again = false;
		if ( same ) {
			airZoneWriteEntry( path2, c, got[0].volumes, got[0].giMode, &err );
			again = readAll( path2 ) == readAll( path );
		}
		check( "roundtrip", same && again && got[0].file == path,
			same ? QStringLiteral( "40 volumes, every number back; rewrite %1 the same bytes; file %2" ).arg( again ? QStringLiteral( "gives" ) : QStringLiteral( "does NOT give" ), got[0].file )
				 : why );
	}

	// ---- keep
	{
		const QString root = freshRoot( out, QStringLiteral( "keep" ) );
		AirZoneCell a, b;
		a.plugin = b.plugin = QStringLiteral( "Fallout4.esm" );
		a.localId = 0x00001111u;
		b.localId = 0x00002222u;
		a.edid = QStringLiteral( "CellA" );
		b.edid = QStringLiteral( "CellB" );
		const QString path = QDir( root ).filePath( QStringLiteral( "Plugins/FO4CS/Cells/Shared.recx" ) );
		QDir().mkpath( QFileInfo( path ).absolutePath() );
		QJsonObject other = airZoneEntryJson( b, { randomVolume( rng, 1 ) }, QStringLiteral( "all" ) );
		other.insert( QStringLiteral( "futureBlock" ), QJsonObject{ { QStringLiteral( "x" ), 1 } } );
		QJsonObject rootObj{ { QStringLiteral( "entries" ), QJsonArray{ other } }, { QStringLiteral( "comment" ), QStringLiteral( "kept" ) } };
		QFile f( path );
		f.open( QIODevice::WriteOnly );
		f.write( QJsonDocument( rootObj ).toJson() );
		f.close();
		QString err;
		const std::vector<az::Volume> mine = { randomVolume( rng, 2 ), randomVolume( rng, 3 ) };
		airZoneWriteEntry( path, a, mine, QStringLiteral( "rooms" ), &err );
		airZoneWriteEntry( path, a, mine, QStringLiteral( "rooms" ), &err );	// twice: replaced, not appended
		const QJsonObject back = QJsonDocument::fromJson( readAll( path ) ).object();
		const QJsonArray es = back.value( QStringLiteral( "entries" ) ).toArray();
		const std::vector<AirZoneCellVolumes> got = airZoneLoad( { root }, { root + QStringLiteral( "/Fallout4.esm" ) }, { a, b }, red );
		const bool ok = es.size() == 2 && back.value( QStringLiteral( "comment" ) ).toString() == QLatin1String( "kept" )
			&& es.at( 0 ).toObject().contains( QStringLiteral( "futureBlock" ) ) && got.size() == 2 && got[0].volumes.size() == 2
			&& got[1].volumes.size() == 1 && got[1].giMode == QLatin1String( "all" );
		check( "keep", ok, QStringLiteral( "entries %1 (want 2), the other entry's unknown block %2, cell A %3 volume(s), cell B %4" )
			.arg( es.size() ).arg( es.at( 0 ).toObject().contains( QStringLiteral( "futureBlock" ) ) ? QStringLiteral( "kept" ) : QStringLiteral( "LOST" ) )
			.arg( got.size() > 0 ? int( got[0].volumes.size() ) : -1 ).arg( got.size() > 1 ? int( got[1].volumes.size() ) : -1 ) );
	}

	// ---- gi-all: the converter, read back, merged = today's list
	{
		const GiVolumeSet today = randomSet( rng, true );
		const std::vector<AirZoneCell> cells = block3x3();
		QStringList lines;
		bool ok = true;
		for ( int arm = 0; arm < 3; arm++ ) {	// 0 every cell, 1 four of nine, 2 an interior
			const QString root = freshRoot( out, QStringLiteral( "giall%1" ).arg( arm ) );
			const GiVolumeSet base = arm == 2 ? randomSet( rng, false ) : today;
			std::vector<AirZoneCell> cs = cells;
			if ( arm == 2 ) {
				AirZoneCell c;
				c.plugin = QStringLiteral( "Fallout4.esm" );
				c.localId = 0x0001F00Du;
				c.edid = QStringLiteral( "GateInterior" );
				cs = { c };
			}
			int written = 0, vols = 0;
			for ( size_t k = 0; k < cs.size(); k++ ) {
				if ( arm == 1 && k >= 4 )
					break;
				const std::vector<az::Volume> v = airZoneFromGi( base, cs[k] );
				vols += int( v.size() );
				QString err;
				if ( airZoneWriteEntry( airZoneFilePath( root, QStringLiteral( "FO4CS" ), cs[k] ), cs[k], v, QStringLiteral( "all" ), &err ) )
					written++;
			}
			const std::vector<AirZoneCellVolumes> got = airZoneLoad( { root }, { root + QStringLiteral( "/Fallout4.esm" ) }, cs, red );
			GiVolumeSet merged = base;
			const QString line = airZoneGiMerge( merged, got );
			QString why;
			const bool same = sameSet( merged, base, &why ) && airZoneGiHash( merged ) == airZoneGiHash( base ) && !line.isEmpty();
			ok = ok && same;
			lines << QStringLiteral( "arm %1: %2 file(s), %3 volume(s), %4" ).arg( arm ).arg( written ).arg( vols )
				.arg( same ? QStringLiteral( "equal, hash %1" ).arg( airZoneGiHash( merged ) ) : QStringLiteral( "DIFFERS: %1 (%2)" ).arg( why, line.isEmpty() ? QStringLiteral( "nothing read" ) : line ) );
		}
		check( "gi-all", ok, lines.join( QStringLiteral( "; " ) ) );
	}

	// ---- gi-rooms: a "rooms" file over today's set
	{
		const QString root = freshRoot( out, QStringLiteral( "girooms" ) );
		const GiVolumeSet today = randomSet( rng, false );
		AirZoneCell c;
		c.plugin = QStringLiteral( "Fallout4.esm" );
		c.localId = 0x0001F00Eu;
		c.edid = QStringLiteral( "GateRooms" );
		az::Volume bx;
		bx.name = "room 3 by hand";
		bx.shape.size[0] = bx.shape.size[1] = bx.shape.size[2] = 333.0f;
		bx.gi.on = true;
		bx.gi.room = 3;
		az::Volume op = bx;
		op.name = "opening 3|0 by hand";
		op.gi.threshold = true;
		op.gi.rooms[0] = 0;
		op.gi.rooms[1] = 3;
		op.gi.blendWidth = 40.0f;
		QString err;
		airZoneWriteEntry( airZoneFilePath( root, QStringLiteral( "FO4CS" ), c ), c, { bx, op }, QStringLiteral( "rooms" ), &err );
		const std::vector<AirZoneCellVolumes> got = airZoneLoad( { root }, { root + QStringLiteral( "/Fallout4.esm" ) }, { c }, red );
		GiVolumeSet merged = today;
		const QString line = airZoneGiMerge( merged, got );
		GiVolumeSet want;
		for ( const GiVolumeShape & s : today.shapes )
			if ( s.room != 3 )
				want.shapes.push_back( s );
		GiVolumeShape hs;
		hs.type = 0;
		hs.size[0] = hs.size[1] = hs.size[2] = 333.0f;
		hs.room = 3;
		want.shapes.push_back( hs );
		for ( const GiThreshold & t : today.thresholds )
			if ( !( ( t.rooms[0] == 3 && t.rooms[1] == 0 ) || ( t.rooms[0] == 0 && t.rooms[1] == 3 ) ) )
				want.thresholds.push_back( t );
		GiThreshold ht;
		ht.shape = hs;
		ht.rooms[0] = 0;
		ht.rooms[1] = 3;
		ht.blendWidth = 40.0f;
		want.thresholds.push_back( ht );
		QString why;
		const bool ok = sameSet( merged, want, &why );
		check( "gi-rooms", ok, ok ? QStringLiteral( "%1 shapes, %2 thresholds as ruled; %3" ).arg( merged.shapes.size() ).arg( merged.thresholds.size() ).arg( line )
			: QStringLiteral( "%1 (%2)" ).arg( why, line ) );
	}

	// ---- novol: the red ignores the same files (only meaningful in a green run)
	if ( red.isEmpty() ) {
		const QString root = QDir( out ).filePath( QStringLiteral( "giall0" ) );
		const std::vector<AirZoneCell> cells = block3x3();
		qputenv( "WW_CELL_AIRZONE_RED", "novol" );
		const std::vector<AirZoneCellVolumes> got = airZoneLoad( { root }, { root + QStringLiteral( "/Fallout4.esm" ) }, cells, wwAirZoneRed() );
		qunsetenv( "WW_CELL_AIRZONE_RED" );
		int vols = 0;
		for ( const AirZoneCellVolumes & cv : got )
			vols += int( cv.volumes.size() );
		std::mt19937 r2( 5 );
		GiVolumeSet s = randomSet( r2, true );
		const QString before = airZoneGiHash( s );
		const QString line = airZoneGiMerge( s, got );
		check( "novol", vols == 0 && line.isEmpty() && airZoneGiHash( s ) == before,
			QStringLiteral( "%1 volume(s) read with the red (want 0), the set %2" ).arg( vols ).arg( airZoneGiHash( s ) == before ? QStringLiteral( "untouched" ) : QStringLiteral( "CHANGED" ) ) );
	}

	// ---- refuse: bad values are skipped or clamped, and said
	{
		const QJsonObject entry{ { QStringLiteral( "cell" ), QStringLiteral( "Fallout4.esm|0x00000001" ) },
			{ QStringLiteral( "volumes" ), QJsonArray{
				QJsonObject{ { QStringLiteral( "shape" ), QStringLiteral( "torus" ) }, { QStringLiteral( "center" ), QJsonArray{ 0, 0, 0 } }, { QStringLiteral( "size" ), QJsonArray{ 1, 1, 1 } } },
				QJsonObject{ { QStringLiteral( "shape" ), QStringLiteral( "box" ) }, { QStringLiteral( "center" ), QJsonArray{ 0, 0, 0 } }, { QStringLiteral( "size" ), QJsonArray{ 0, 1, 1 } } },
				QJsonObject{ { QStringLiteral( "shape" ), QStringLiteral( "Sphere" ) }, { QStringLiteral( "center" ), QJsonArray{ 1, 2, 3 } }, { QStringLiteral( "size" ), QJsonArray{ 10, 10, 10 } },
					{ QStringLiteral( "fog" ), QJsonObject{ { QStringLiteral( "scale" ), 500 } } } } } } };
		std::vector<az::Volume> v;
		QString gm;
		QStringList warn;
		airZoneParse( entry, &v, &gm, &warn );
		const bool ok = v.size() == 1 && v[0].shape.type == az::Sphere && v[0].fog.scale == 64.0f && warn.size() == 3;
		check( "refuse", ok, QStringLiteral( "%1 volume(s) kept (want 1), %2 warning(s) (want 3): %3" ).arg( v.size() ).arg( warn.size() )
			.arg( warn.join( QStringLiteral( " | " ) ) ) );
	}

	if ( g_fail ) {
		std::printf( "AIRZONE_RECX FAIL %d of %d\n", g_fail, g_fail + g_pass );
		return 1;
	}
	std::printf( "AIRZONE_RECX PASS %d\n", g_pass );
	return 0;
}
