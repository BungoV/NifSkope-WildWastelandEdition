// OPENCELL1 unit test: File > Open Cell... against synthetic plugins built here (no game data).
// Prints one line per check, then PASS or FAIL; exit code 0 only on PASS.
// Red: WW_OPENCELL_RED=writer swaps x and y in the writer, and this test must then FAIL.

#include "opencellcatalog.h"
#include "opencelldialog.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <cstdio>

namespace {

int checks = 0, fails = 0;

void check( bool ok, const QString & what )
{
	checks++;
	fails += !ok;
	std::printf( "  %s %s\n", ok ? "ok  " : "FAIL", qPrintable( what ) );
}

QString shown( const QStringList & v ) { return v.join( QStringLiteral( " | " ) ); }
QString shown( const QString & v ) { return v; }
QString shown( const QByteArray & v ) { return QString::fromUtf8( v ).replace( QLatin1Char( '\n' ), QLatin1String( "\\n" ) ); }

template <typename T>
void checkEq( const T & got, const T & want, const QString & what )
{
	check( got == want, what );
	if ( got != want )
		std::printf( "       got:  %s\n       want: %s\n", qPrintable( shown( got ) ), qPrintable( shown( want ) ) );
}

// ---- a minimal plugin writer: only the fields the reader needs

QByteArray u16( quint16 v ) { return QByteArray( reinterpret_cast<const char *>( &v ), 2 ); }
QByteArray u32( quint32 v ) { return QByteArray( reinterpret_cast<const char *>( &v ), 4 ); }
QByteArray i32( qint32 v ) { return QByteArray( reinterpret_cast<const char *>( &v ), 4 ); }

QByteArray field( const char * tag, const QByteArray & data )
{
	return QByteArray( tag, 4 ) + u16( quint16( data.size() ) ) + data;
}

QByteArray zstring( const char * tag, const char * s )
{
	return field( tag, QByteArray( s ) + '\0' );
}

QByteArray record( const char * type, quint32 form, const QByteArray & fields, quint32 flags = 0 )
{
	// type, size, flags, form, version control, form version 131 (Fallout 4), unknown
	return QByteArray( type, 4 ) + u32( quint32( fields.size() ) ) + u32( flags ) + u32( form ) + u32( 0 )
		+ u16( 131 ) + u16( 0 ) + fields;
}

QByteArray group( const QByteArray & label, quint32 type, const QByteArray & body )
{
	return QByteArray( "GRUP" ) + u32( quint32( 24 + body.size() ) ) + label + u32( type ) + u32( 0 ) + u32( 0 ) + body;
}

QByteArray header( quint32 flags, const QStringList & masters )
{
	QByteArray f = field( "HEDR", u32( 0x3F800000 ) + u32( 0 ) + u32( 0x800 ) );
	for ( const QString & m : masters )
		f += zstring( "MAST", qPrintable( m ) ) + field( "DATA", QByteArray( 8, '\0' ) );
	return record( "TES4", 0, f, flags );
}

QByteArray cell( quint32 form, const char * edid, const char * full, int x, int y )
{
	QByteArray f;
	if ( *edid )
		f += zstring( "EDID", edid );
	if ( *full )
		f += zstring( "FULL", full );
	f += field( "XCLC", i32( x ) + i32( y ) + u32( 0 ) );
	return record( "CELL", form, f );
}

QByteArray interior( quint32 form, const char * edid, const char * full )
{
	QByteArray f = zstring( "EDID", edid );
	if ( *full )
		f += zstring( "FULL", full );
	return record( "CELL", form, f + field( "DATA", u16( 1 ) ) );
}

QByteArray exteriorBlock( const QByteArray & cells )
{
	return group( u32( 0 ), 4, group( u32( 0 ), 5, cells ) );
}

QByteArray basePlugin()
{
	const QByteArray world = record( "WRLD", 0xF01, zstring( "EDID", "TestWorld" ) + zstring( "FULL", "Test World" ) );
	const QByteArray children = group( u32( 0xF01 ), 1,
		record( "CELL", 0xF02, zstring( "EDID", "PersistentCell" ) )   // the persistent cell: not a grid cell
		+ exteriorBlock( cell( 0xF03, "CellA", "Cell A", 0, 0 ) + cell( 0xF04, "CellB", "", 1, 0 )
			+ cell( 0xF05, "", "", -1, 2 ) ) );
	const QByteArray interiors = group( "CELL", 0, group( u32( 0 ), 2, group( u32( 0 ), 3,
		interior( 0xF10, "TestInterior", "Test Interior" ) + interior( 0xF11, "AnotherRoom", "" ) ) ) );
	return header( 0x1, {} ) + group( "WRLD", 0, world + children ) + interiors;
}

// Overrides TestWorld, adds a cell to it and a worldspace of its own.
QByteArray modPlugin()
{
	const QByteArray world = record( "WRLD", 0xF01, zstring( "EDID", "TestWorld" ) + zstring( "FULL", "Test World" ) );
	const QByteArray children = group( u32( 0xF01 ), 1, exteriorBlock( cell( 0x01000800, "ModCell", "", 5, -3 ) ) );
	const QByteArray own = record( "WRLD", 0x01000801, zstring( "EDID", "ModWorld" ) );
	return header( 0, { QStringLiteral( "Base.esm" ) } ) + group( "WRLD", 0, world + children + own );
}

// Localized: FULL is a string-table index and must not show as a name.
QByteArray localizedPlugin()
{
	const QByteArray world = record( "WRLD", 0xA01, zstring( "EDID", "LocWorld" ) + field( "FULL", u32( 0x1234 ) ) );
	return header( 0x81, {} ) + group( "WRLD", 0, world );
}

bool writeFile( const QString & path, const QByteArray & data )
{
	QDir().mkpath( QFileInfo( path ).absolutePath() );
	QFile f( path );
	return f.open( QIODevice::WriteOnly ) && f.write( data ) == data.size();
}

QByteArray readFile( const QString & path )
{
	QFile f( path );
	return f.open( QIODevice::ReadOnly ) ? f.readAll() : QByteArray();
}

} // namespace

int main( int argc, char ** argv )
{
	qputenv( "QT_QPA_PLATFORM", "offscreen" );
	QStandardPaths::setTestModeEnabled( true );   // keep QSettings away from the real profile
	QApplication app( argc, argv );
	QApplication::setOrganizationName( QStringLiteral( "opencelltest" ) );
	QApplication::setApplicationName( QStringLiteral( "opencelltest" ) );

	QTemporaryDir tmp;
	const QString data = tmp.path() + QStringLiteral( "/Data" );
	writeFile( data + QStringLiteral( "/Base.esm" ), basePlugin() );
	writeFile( data + QStringLiteral( "/Mod.esp" ), modPlugin() );
	writeFile( data + QStringLiteral( "/Loc.esm" ), localizedPlugin() );
	const QString base = data + QStringLiteral( "/Base.esm" ), mod = data + QStringLiteral( "/Mod.esp" );

	std::printf( "catalog\n" );
	OpenCellSources src;
	src.dataDir = data;
	QStringList missing;
	checkEq( openCellWithMasters( { QStringLiteral( "Mod.esp" ) }, src, &missing ),
		QStringList{ QStringLiteral( "Base.esm" ), QStringLiteral( "Mod.esp" ) }, QStringLiteral( "a master is added before its plugin" ) );
	check( missing.isEmpty(), QStringLiteral( "no master missing" ) );
	checkEq( openCellDataPlugins( data ), QStringList{ QStringLiteral( "Base.esm" ), QStringLiteral( "Loc.esm" ), QStringLiteral( "Mod.esp" ) },
		QStringLiteral( "Data folder plugins: masters first" ) );
	checkEq( openCellParsePluginsTxt( QStringLiteral( "# Fallout 4\r\n*Mod.esp\r\nOff.esp\r\n" ) ), QStringList{ QStringLiteral( "Mod.esp" ) },
		QStringLiteral( "plugins.txt: only the active entries" ) );
	OpenCellCatalog loc;
	openCellReadCatalog( { data + QStringLiteral( "/Loc.esm" ) }, loc, nullptr );
	check( loc.worlds.size() == 1 && loc.worlds[0].edid == QLatin1String( "LocWorld" ) && loc.worlds[0].name.isEmpty(),
		QStringLiteral( "a localized FULL is not shown as a name" ) );

	std::printf( "MO2 profile\n" );
	const QString mo2 = tmp.path() + QStringLiteral( "/MO2" );
	writeFile( mo2 + QStringLiteral( "/profiles/Default/plugins.txt" ), "*Mod.esp\n" );
	writeFile( mo2 + QStringLiteral( "/profiles/Default/modlist.txt" ), "# managed\n+MyMod\n-OffMod\n" );
	writeFile( mo2 + QStringLiteral( "/mods/MyMod/Mod.esp" ), modPlugin() );
	OpenCellSources mosrc;
	mosrc.dataDir = data;
	QStringList moPlugins;
	check( openCellImportLoadOrder( mo2 + QStringLiteral( "/profiles/Default/plugins.txt" ), moPlugins, mosrc, nullptr ),
		QStringLiteral( "MO2 plugins.txt imports" ) );
	checkEq( mosrc.resolve( QStringLiteral( "Mod.esp" ) ), QDir::cleanPath( mo2 + QStringLiteral( "/mods/MyMod/Mod.esp" ) ),
		QStringLiteral( "a plugin in an enabled mod resolves to the mod folder" ) );
	checkEq( mosrc.resolve( QStringLiteral( "Base.esm" ) ), QDir::cleanPath( base ), QStringLiteral( "a plugin in no mod resolves to Data" ) );

	std::printf( "dialog\n" );
	OpenCellDialog dlg;
	dlg.setDataDir( data );
	dlg.setPluginsTicked( { QStringLiteral( "Mod.esp" ) } );
	dlg.readNow();
	checkEq( dlg.loadOrderPaths(), QStringList{ QDir::cleanPath( base ), QDir::cleanPath( mod ) }, QStringLiteral( "load order paths" ) );
	checkEq( dlg.worldItems(), QStringList{ QStringLiteral( "TestWorld (Test World)" ), QStringLiteral( "ModWorld" ), QStringLiteral( "Interior" ) },
		QStringLiteral( "worldspace list (overridden TestWorld listed once)" ) );
	dlg.selectWorld( QStringLiteral( "TestWorld" ) );
	checkEq( dlg.cellItems(), QStringList{ QStringLiteral( "ModCell\t\t5\t-3" ), QStringLiteral( "CellA\tCell A\t0\t0" ),
		QStringLiteral( "CellB\t\t1\t0" ), QStringLiteral( "\t\t-1\t2" ) },
		QStringLiteral( "cell list: the mod's new cell joins, the persistent cell does not" ) );
	dlg.selectWorld( QStringLiteral( "Interior" ) );
	checkEq( dlg.cellItems(), QStringList{ QStringLiteral( "AnotherRoom\t\t00000F11" ), QStringLiteral( "TestInterior\tTest Interior\t00000F10" ) },
		QStringLiteral( "interior list" ) );

	std::printf( ".wwcell\n" );
	const QString cells = tmp.path() + QStringLiteral( "/cells" );
	dlg.selectInterior( QStringLiteral( "TestInterior" ) );
	check( dlg.writeSpec( cells ), QStringLiteral( "interior spec written" ) );
	checkEq( readFile( dlg.specPath() ), ( QDir::cleanPath( base ) + QStringLiteral( "," ) + QDir::cleanPath( mod )
		+ QStringLiteral( "|interior|TestInterior\n" ) ).toUtf8(), QStringLiteral( "interior .wwcell bytes" ) );
	dlg.selectWorld( QStringLiteral( "TestWorld" ) );
	dlg.selectCell( 5, -3, 3 );
	check( dlg.writeSpec( cells ), QStringLiteral( "exterior spec written" ) );
	checkEq( QFileInfo( dlg.specPath() ).fileName(), QStringLiteral( "TestWorld_5_-3_3.wwcell" ), QStringLiteral( "spec file name" ) );
	checkEq( readFile( dlg.specPath() ), ( QDir::cleanPath( base ) + QStringLiteral( "," ) + QDir::cleanPath( mod )
		+ QStringLiteral( "|TestWorld|5,-3|3\n" ) ).toUtf8(), QStringLiteral( "exterior .wwcell bytes" ) );
	checkEq( openCellRecent().value( 0 ), QFileInfo( dlg.specPath() ).absoluteFilePath(), QStringLiteral( "newest recent cell" ) );
	for ( int i = 0; i < 12; i++ ) {
		const QString p = cells + QStringLiteral( "/r%1.wwcell" ).arg( i );
		writeFile( p, "x\n" );
		openCellRecentAdd( p );
	}
	check( openCellRecent().size() == openCellRecentMax, QStringLiteral( "Recent Cells keeps the last 10" ) );

	std::printf( "%d checks, %d failed\n%s\n", checks, fails, fails ? "FAIL" : "PASS" );
	return fails ? 1 : 0;
}
