// OPENCELL1 unit test: File > Open Cell... against synthetic plugins built here (no game data).
// Prints one line per check, then PASS or FAIL; exit code 0 only on PASS.
// Red: WW_OPENCELL_RED=writer swaps x and y in the writer, and this test must then FAIL.
// Red: WW_OPENCELL_RED=strings (and the off switch WW_OPENCELL_STRINGS=0) leave localized names blank: FAIL.
// Red: WW_ESM_LIGHT_RED=full numbers light plugins as full ones (top byte = file index): FAIL.
// Reds (OPENCELLFIX2): WW_OPENCELL_RED=lang (the saved box value, no en retry), =dblclick, WW_DROPDOWN_RED=1: FAIL.

#include "opencellcatalog.h"
#include "opencelldialog.h"
#include "widgets/wwdropdown.h"
#include "esmfile.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTreeWidget>

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
QString shown( int v ) { return QString::number( v ); }
QString shown( bool v ) { return v ? QStringLiteral( "true" ) : QStringLiteral( "false" ); }
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
	// lane CELLSPAWN1: it also edits Base.esm's CellB (a name), so CellB is added by Base.esm and last edited by Mod.esp
	const QByteArray children = group( u32( 0xF01 ), 1, exteriorBlock( cell( 0x01000800, "ModCell", "", 5, -3 )
		+ cell( 0xF04, "CellB", "Cell B Edited", 1, 0 ) ) );
	const QByteArray own = record( "WRLD", 0x01000801, zstring( "EDID", "ModWorld" ) );
	return header( 0, { QStringLiteral( "Base.esm" ) } ) + group( "WRLD", 0, world + children + own );
}

// Localized: FULL is a string-table index, read from Strings/Loc_<lang>.STRINGS.
QByteArray localizedPlugin()
{
	const QByteArray world = record( "WRLD", 0xA01, zstring( "EDID", "LocWorld" ) + field( "FULL", u32( 0x1234 ) ) );
	const QByteArray locCell = record( "CELL", 0xA03, zstring( "EDID", "LocCell" ) + field( "FULL", u32( 0x55 ) )
		+ field( "XCLC", i32( 2 ) + i32( 3 ) + u32( 0 ) ) );
	const QByteArray children = group( u32( 0xA01 ), 1, exteriorBlock( locCell ) );
	const QByteArray interiors = group( "CELL", 0, group( u32( 0 ), 2, group( u32( 0 ), 3,
		record( "CELL", 0xA10, zstring( "EDID", "LocRoom" ) + field( "FULL", u32( 0x66 ) ) + field( "DATA", u16( 1 ) ) ) ) ) );
	return header( 0x81, {} ) + group( "WRLD", 0, world + children ) + interiors;
}

// Localized too; its tables are only inside "Loc2 - Localization.ba2" (and a loose German one beside it).
QByteArray localized2Plugin()
{
	return header( 0x81, {} ) + group( "WRLD", 0, record( "WRLD", 0xB01, zstring( "EDID", "Loc2World" ) + field( "FULL", u32( 7 ) ) ) );
}

// A light master (.esl) and a light-flagged .esp (TES4 0x200).
QByteArray lightMaster()
{
	return header( 0x201, {} ) + group( "WRLD", 0, record( "WRLD", 0xC01, zstring( "EDID", "LightWorld" ) ) );
}

QByteArray lightFlagged()
{
	return header( 0x200, { QStringLiteral( "Base.esm" ) } )
		+ group( "WRLD", 0, record( "WRLD", 0x01000C02, zstring( "EDID", "LightFlagWorld" ) ) );
}

// A string table: count, data size, (id, offset) pairs, data. DL/IL strings carry a length before the text.
QByteArray stringTable( const QList<QPair<quint32, QByteArray>> & entries, bool lengthPrefixed )
{
	QByteArray dir, data;
	for ( const auto & e : entries ) {
		dir += u32( e.first ) + u32( quint32( data.size() ) );
		if ( lengthPrefixed )
			data += u32( quint32( e.second.size() + 1 ) );
		data += e.second + '\0';
	}
	return u32( quint32( entries.size() ) ) + u32( quint32( data.size() ) ) + dir + data;
}

// A general BA2 (version 1, files stored uncompressed).
QByteArray ba2( const QList<QPair<QByteArray, QByteArray>> & files )
{
	const quint32 n = quint32( files.size() );
	quint64 at = 24 + 36 * quint64( n );
	QByteArray recs, data, names;
	for ( const auto & f : files ) {
		recs += u32( 0 ) + QByteArray( "str", 4 ) + u32( 0 ) + u32( 0x00100100 )
			+ u32( quint32( at ) ) + u32( quint32( at >> 32 ) ) + u32( 0 ) + u32( quint32( f.second.size() ) ) + u32( 0xBAADF00D );
		data += f.second;
		at += quint64( f.second.size() );
		names += u16( quint16( f.first.size() ) ) + f.first;
	}
	return QByteArray( "BTDX" ) + u32( 1 ) + QByteArray( "GNRL" ) + u32( n ) + u32( quint32( at ) ) + u32( quint32( at >> 32 ) )
		+ recs + data + names;
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

// A full plugin over Base.esm and Light.esl: overrides the light world and adds a record of its own.
QByteArray usesLightPlugin()
{
	return header( 0, { QStringLiteral( "Base.esm" ), QStringLiteral( "Light.esl" ) } )
		+ group( "WRLD", 0, record( "WRLD", 0x01000C01, zstring( "EDID", "LightWorld" ) )
			+ record( "WRLD", 0x02000800, zstring( "EDID", "UsesLightWorld" ) ) );
}

QString hex( quint32 v ) { return QStringLiteral( "%1" ).arg( v, 8, 16, QLatin1Char( '0' ) ).toUpper(); }

// lane OPENCELLFIX1: the LOD panel's MO2 profile as the application finds it; the test sets it.
OpenCellFallback g_fallback;
OpenCellFallback openCellFallback() { return g_fallback; }

QStringList sortedCopy( QStringList v )
{
	v.sort( Qt::CaseInsensitive );
	return v;
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );   // a crash still leaves the last line
	qputenv( "QT_QPA_PLATFORM", "offscreen" );
	QStandardPaths::setTestModeEnabled( true );   // keep QSettings away from the real profile
	QApplication app( argc, argv );
	QApplication::setOrganizationName( QStringLiteral( "opencelltest" ) );
	QApplication::setApplicationName( QStringLiteral( "opencelltest" ) );
	// the test's own settings scope: an ini beside the exe, never the registry
	QSettings::setDefaultFormat( QSettings::IniFormat );
	QSettings::setPath( QSettings::IniFormat, QSettings::UserScope, QCoreApplication::applicationDirPath() + QStringLiteral( "/settings" ) );

	// --real <Data> <plugin> [lang]: the user's own installed plugin (nothing of it is in the repo). PASS when the
	// plugin is localized, its tables are found and its worldspaces carry names.
	if ( argc >= 4 && QByteArray( argv[1] ) == "--real" ) {
		const QString dataDir = QString::fromLocal8Bit( argv[2] );
		const QString path = QDir( dataDir ).filePath( QString::fromLocal8Bit( argv[3] ) );
		OpenCellReadOptions opt;
		opt.lang = argc >= 5 ? QString::fromLocal8Bit( argv[4] ) : QStringLiteral( "en" );
		opt.dataDir = dataDir;
		bool localized = false;
		openCellReadHeader( path, nullptr, &localized, nullptr );
		OpenCellCatalog cat;
		QString err;
		const bool read = openCellReadCatalog( { path }, cat, &err, opt );
		int wn = 0, cn = 0, ct = 0, in = 0;
		for ( const OpenCellWorld & w : cat.worlds )
			wn += !w.name.isEmpty();
		for ( const auto & list : cat.cells )
			for ( const OpenCellCell & c : list ) {
				ct++;
				cn += !c.name.isEmpty();
			}
		for ( const OpenCellCell & c : cat.interiors )
			in += !c.name.isEmpty();
		std::printf( "opencell real: localized %d, worlds %d named %d, exterior cells %d named %d, interiors %d named %d, "
			"no strings [%s]%s\n", int( localized ), int( cat.worlds.size() ), wn, ct, cn, int( cat.interiors.size() ), in,
			qPrintable( cat.noStrings.join( QLatin1Char( ',' ) ) ), read ? "" : qPrintable( QStringLiteral( " error " ) + err ) );
		const bool pass = read && localized && cat.noStrings.isEmpty() && wn > 0;
		std::printf( "%s\n", pass ? "PASS" : "FAIL" );
		return pass ? 0 : 1;
	}

	// lane CELLSPAWN1 --census <Data> <plugin,plugin,...>: the real load order through the dialog. Per worldspace (and
	// Interior): catalog rows with an editor ID / a name / neither, against what the list shows. PASS when every row is
	// listed, no exterior's Editor ID cell reads blank (Wilderness stands in), and every row shows an 8-digit FormID.
	// WW_OPENCELL_RED=list (the old list) must FAIL.
	if ( argc >= 4 && QByteArray( argv[1] ) == "--census" ) {
		OpenCellPicker cd;
		cd.setDataDir( QString::fromLocal8Bit( argv[2] ) );
		cd.setPluginsTicked( QString::fromLocal8Bit( argv[3] ).split( QLatin1Char( ',' ), Qt::SkipEmptyParts ) );
		cd.readNow();
		bool pass = !cd.worldItems().isEmpty();
		long long tot[6] = {};
		for ( const QString & item : cd.worldItems() ) {
			// the combo's text is "EDID (name)"; select by the EDID
			const QString edid = item.section( QStringLiteral( " (" ), 0, 0 );
			if ( edid == QLatin1String( OpenCellPicker::allItem ) )
				continue;
			cd.selectWorld( edid );
			const QStringList heads = cd.cellHeaders();
			const int ce = heads.indexOf( QStringLiteral( "Editor ID" ) ), cn = heads.indexOf( QStringLiteral( "Name" ) );
			const int cf = heads.indexOf( QStringLiteral( "FormID" ) );
			int rows = 0, withEdid = 0, withName = 0, neither = 0, blankShown = 0, wild = 0, formOk = 0;
			for ( const QString & line : cd.cellItems() ) {
				const QStringList c = line.split( QLatin1Char( '\t' ) );
				const QString e = c.value( ce ), n = c.value( cn );
				const bool isWild = e == QLatin1String( "Wilderness" );
				rows++;
				withEdid += !e.isEmpty() && !isWild;
				withName += !n.isEmpty();
				neither += ( e.isEmpty() || isWild ) && n.isEmpty();
				if ( e.isEmpty() && blankShown < 5 )
					std::printf( "  blank editor ID: %s\n", qPrintable( QString( line ).replace( QLatin1Char( '\t' ), QLatin1Char( '|' ) ) ) );
				blankShown += e.isEmpty();
				wild += isWild;
				bool ok = false;
				formOk += cf >= 0 && c.value( cf ).size() == 8 && ( c.value( cf ).toUInt( &ok, 16 ), ok );
			}
			// An interior with no editor ID reads blank, as in the CK (it is listed by its FormID); an exterior reads Wilderness.
			const bool interiors = edid == QLatin1String( OpenCellPicker::interiorItem );
			const bool ok = rows == cd.cellTotal() && ( interiors || blankShown == 0 ) && formOk == rows;
			pass = pass && ok;
			std::printf( "census %-28s rows %6d listed %6d  edid %6d  name %6d  neither %6d  wilderness %6d  blank %6d  formid %6d  %s\n",
				qPrintable( edid ), cd.cellTotal(), rows, withEdid, withName, neither, wild, blankShown, formOk, ok ? "ok" : "FAIL" );
			const long long v[6] = { rows, withEdid, withName, neither, blankShown, formOk };
			for ( int i = 0; i < 6; i++ )
				tot[i] += v[i];
		}
		// [<world>=<search> ...]: the hits of a search, with the first hit's row (a known EDID, name, FormID)
		for ( int i = 4; i < argc; i++ ) {
			const QString q = QString::fromLocal8Bit( argv[i] );
			cd.selectWorld( q.section( QLatin1Char( '=' ), 0, 0 ) );
			cd.setSearch( q.section( QLatin1Char( '=' ), 1 ) );
			std::printf( "search %s: %d of %d [%s]\n", qPrintable( q ), cd.cellCount(), cd.cellTotal(),
				qPrintable( cd.cellItems().value( 0 ).replace( QLatin1Char( '\t' ), QLatin1Char( '|' ) ) ) );
			pass = pass && cd.cellCount() > 0;
			cd.setSearch( QString() );
		}
		std::printf( "census total rows %lld edid %lld name %lld neither %lld blank %lld formid %lld\n%s\n",
			tot[0], tot[1], tot[2], tot[3], tot[4], tot[5], pass ? "PASS" : "FAIL" );
		return pass ? 0 : 1;
	}

	// lane OPENCELLFIX1 --spec <Data> <plugins.txt> <world> <x,y> <n> <out dir>: the cell file the Cells tab's Open
	// writes for that load order (the user's own install; nothing of it is in the repo). nifskope-cli cellrefs reads it.
	if ( argc >= 8 && QByteArray( argv[1] ) == "--spec" ) {
		OpenCellPicker p;
		p.setDataDir( QString::fromLocal8Bit( argv[2] ) );
		p.importLoadOrder( QString::fromLocal8Bit( argv[3] ) );
		p.readNow();
		int named = 0;	// OPENCELLFIX2: names never blank ("EDID (name)" in the combo)
		for ( const QString & w : p.worldItems() )
			named += w.contains( QStringLiteral( " (" ) );
		std::printf( "spec: language %s, worldspaces %d named %d\n", qPrintable( p.stringsLanguage() ),
			int( p.worldItems().size() ), named );
		p.selectWorld( QString::fromLocal8Bit( argv[4] ) );
		const QStringList xy = QString::fromLocal8Bit( argv[5] ).split( QLatin1Char( ',' ) );
		p.selectCell( xy.value( 0 ).toInt(), xy.value( 1 ).toInt(), QByteArray( argv[6] ).toInt() );
		const QStringList ticked = p.tickedPlugins();
		std::printf( "spec: plugins %d (first %s), data %s, resources %d\n", int( p.loadOrderPaths().size() ),
			qPrintable( ticked.value( 0 ) ), qPrintable( p.loadOrderSources().dataDir ), int( p.loadOrderSources().resources.size() ) );
		if ( p.specLine().isEmpty() ) {
			std::printf( "spec: refused: %s\nFAIL\n", qPrintable( p.refusal() ) );
			return 1;
		}
		const bool ok = p.writeSpec( QString::fromLocal8Bit( argv[7] ) );
		std::printf( "spec: %s\n%s\n", qPrintable( p.specPath() ), ok ? "PASS" : "FAIL" );
		return ok ? 0 : 1;
	}

	QSettings().clear();	// every run starts from nothing saved
	QTemporaryDir tmp;
	qputenv( "WW_MODPICK_MYGAMES", QFile::encodeName( tmp.path() + QStringLiteral( "/MyGames" ) ) );	// never the real inis
	const QString data = tmp.path() + QStringLiteral( "/Data" );
	writeFile( data + QStringLiteral( "/Base.esm" ), basePlugin() );
	writeFile( data + QStringLiteral( "/Mod.esp" ), modPlugin() );
	writeFile( data + QStringLiteral( "/Loc.esm" ), localizedPlugin() );
	writeFile( data + QStringLiteral( "/Loc2.esm" ), localized2Plugin() );
	writeFile( data + QStringLiteral( "/Light.esl" ), lightMaster() );
	writeFile( data + QStringLiteral( "/LightFlag.esp" ), lightFlagged() );
	writeFile( data + QStringLiteral( "/Strings/Loc_en.STRINGS" ), stringTable( { { 0x1234, "Localized World" },
		{ 0x55, "Caf\xc3\xa9 Cell" }, { 0x66, "Loc Room" } }, false ) );
	writeFile( data + QStringLiteral( "/Strings/Loc_en.DLSTRINGS" ), stringTable( { { 0x9001, "a description" } }, true ) );
	writeFile( data + QStringLiteral( "/Loc2 - Localization.ba2" ), ba2( {
		{ "Strings\\Loc2_en.STRINGS", stringTable( { { 7, "Archived World" } }, false ) },
		{ "Strings\\Loc2_de.STRINGS", stringTable( { { 7, "Archiv-Welt" } }, false ) } } ) );
	writeFile( data + QStringLiteral( "/Strings/Loc2_de.STRINGS" ), stringTable( { { 7, "Lose Welt" } }, false ) );
	const QString base = data + QStringLiteral( "/Base.esm" ), mod = data + QStringLiteral( "/Mod.esp" );

	std::printf( "catalog\n" );
	OpenCellSources src;
	src.dataDir = data;
	QStringList missing;
	checkEq( openCellWithMasters( { QStringLiteral( "Mod.esp" ) }, src, &missing ),
		QStringList{ QStringLiteral( "Base.esm" ), QStringLiteral( "Mod.esp" ) }, QStringLiteral( "a master is added before its plugin" ) );
	check( missing.isEmpty(), QStringLiteral( "no master missing" ) );
	checkEq( openCellDataPlugins( data ), QStringList{ QStringLiteral( "Base.esm" ), QStringLiteral( "Light.esl" ),
		QStringLiteral( "Loc.esm" ), QStringLiteral( "Loc2.esm" ), QStringLiteral( "LightFlag.esp" ), QStringLiteral( "Mod.esp" ) },
		QStringLiteral( "Data folder plugins: masters and .esl first, a light-flagged .esp with the plugins" ) );
	{	// lane GAPFIX1: an ESM-flagged .esp ranks with the masters (the flag decides, not the extension)
		QTemporaryDir tmp2;
		const QString d2 = tmp2.path() + QStringLiteral( "/Data" );
		writeFile( d2 + QStringLiteral( "/Base.esm" ), basePlugin() );
		writeFile( d2 + QStringLiteral( "/Aaa.esp" ), modPlugin() );
		writeFile( d2 + QStringLiteral( "/Zed.esp" ), header( 0x1, {} ) );
		checkEq( openCellDataPlugins( d2 ), QStringList{ QStringLiteral( "Base.esm" ), QStringLiteral( "Zed.esp" ),
			QStringLiteral( "Aaa.esp" ) }, QStringLiteral( "an ESM-flagged .esp sorts with the masters" ) );
	}
	checkEq( openCellParsePluginsTxt( QStringLiteral( "*Light.esl\n*LightFlag.esp\n" ) ),
		QStringList{ QStringLiteral( "Light.esl" ), QStringLiteral( "LightFlag.esp" ) }, QStringLiteral( "plugins.txt keeps .esl entries" ) );
	{
		OpenCellCatalog light;
		openCellReadCatalog( { base, data + QStringLiteral( "/Light.esl" ), data + QStringLiteral( "/LightFlag.esp" ) }, light, nullptr );
		QStringList w;
		for ( const OpenCellWorld & x : light.worlds )
			w.append( x.edid );
		checkEq( w, QStringList{ QStringLiteral( "TestWorld" ), QStringLiteral( "LightWorld" ), QStringLiteral( "LightFlagWorld" ) },
			QStringLiteral( "an .esl and a light-flagged .esp are read" ) );
	}
	std::printf( "light numbering\n" );
	{
		// (UsesLight.esp lives outside Data so the later Data-folder checks stay as they are)
		// the game's rule: a light plugin's IDs are FE + its slot among the light plugins in load order + 12-bit local ID
		writeFile( tmp.path() + QStringLiteral( "/Elsewhere/UsesLight.esp" ), usesLightPlugin() );
		const QStringList order{ base, data + QStringLiteral( "/Light.esl" ), data + QStringLiteral( "/LightFlag.esp" ),
			tmp.path() + QStringLiteral( "/Elsewhere/UsesLight.esp" ) };
		QStringList native;
		for ( const QString & f : order )
			native.append( QDir::toNativeSeparators( f ) );
		ESMFile esm( native.join( QLatin1Char( ',' ) ).toLocal8Bit().constData() );
		const ESMFile::ESMRecord * lw = esm.findRecord( 0xFE000C01U );
		check( lw && lw->formID == 0xFE000C01U, QStringLiteral( "Light.esl (light slot 0): WRLD 0xC01 is FE000C01" ) );
		const ESMFile::ESMRecord * fw = esm.findRecord( 0xFE001C02U );
		check( fw != nullptr, QStringLiteral( "LightFlag.esp (flag 0x200, light slot 1): its own 01000C02 is FE001C02" ) );
		check( !esm.findRecord( 0x01000C01U ) && !esm.findRecord( 0x02000C02U ), QStringLiteral( "no full-slot copy of a light record" ) );
		const ESMFile::ESMRecord * uw = esm.findRecord( 0x03000800U );
		check( uw != nullptr, QStringLiteral( "a full plugin after two light ones keeps its load-order byte (03000800)" ) );
		checkEq( hex( uw ? esm.mapFormID( *uw, 0x01000C01U ) : 0U ), hex( 0xFE000C01U ),
			QStringLiteral( "a full plugin's reference to its light master resolves to FE000C01" ) );
		checkEq( hex( esm.getFormIDFile( 0xFE001C02U ) ) + QLatin1Char( ' ' ) + hex( ESMFile::getFormIDLocal( 0xFE001C02U ) )
			+ QLatin1Char( ' ' ) + hex( esm.getFormIDFile( 0x03000800U ) ),
			hex( 2 ) + QLatin1Char( ' ' ) + hex( 0xC02 ) + QLatin1Char( ' ' ) + hex( 3 ),
			QStringLiteral( "FE ids find their plugin and local ID" ) );
		check( esm.findRecord( 0x00000F01U ) != nullptr, QStringLiteral( "the master keeps 00000F01" ) );
		OpenCellCatalog light;
		openCellReadCatalog( order, light, nullptr );
		QStringList forms;
		for ( const OpenCellWorld & x : light.worlds )
			forms.append( x.edid + QLatin1Char( '=' ) + hex( x.form ) );
		checkEq( forms, QStringList{ QStringLiteral( "TestWorld=00000F01" ), QStringLiteral( "LightWorld=FE000C01" ),
			QStringLiteral( "LightFlagWorld=FE001C02" ), QStringLiteral( "UsesLightWorld=03000800" ) },
			QStringLiteral( "the catalog lists light worlds under their FE ids" ) );
	}
	checkEq( openCellParsePluginsTxt( QStringLiteral( "# Fallout 4\r\n*Mod.esp\r\nOff.esp\r\n" ) ), QStringList{ QStringLiteral( "Mod.esp" ) },
		QStringLiteral( "plugins.txt: only the active entries" ) );

	std::printf( "strings\n" );
	OpenCellStrings dl;
	check( openCellParseStrings( readFile( data + QStringLiteral( "/Strings/Loc_en.DLSTRINGS" ) ), true, dl )
		&& dl.value( 0x9001 ) == QLatin1String( "a description" ), QStringLiteral( "a length-prefixed table parses" ) );
	OpenCellStrings bad;
	check( !openCellParseStrings( QByteArray( "\x05\0\0\0\x10\0\0\0", 8 ), false, bad ), QStringLiteral( "a truncated table is refused" ) );
	OpenCellReadOptions en;
	en.lang = QStringLiteral( "en" );
	OpenCellCatalog loc;
	openCellReadCatalog( { data + QStringLiteral( "/Loc.esm" ) }, loc, nullptr, en );
	checkEq( loc.worlds.value( 0 ).name, QStringLiteral( "Localized World" ), QStringLiteral( "a localized world's name from Strings/" ) );
	checkEq( loc.cells.value( 0xA01 ).value( 0 ).name, QString::fromUtf8( "Caf\xc3\xa9 Cell" ), QStringLiteral( "a localized cell's name (UTF-8)" ) );
	checkEq( loc.interiors.value( 0 ).name, QStringLiteral( "Loc Room" ), QStringLiteral( "a localized interior's name" ) );
	check( loc.noStrings.isEmpty(), QStringLiteral( "no plugin reported without strings" ) );
	OpenCellReadOptions fr;
	fr.lang = QStringLiteral( "fr" );
	OpenCellCatalog locFr;
	openCellReadCatalog( { data + QStringLiteral( "/Loc.esm" ) }, locFr, nullptr, fr );
	// OPENCELLFIX2: no table for the language falls back to the en tables (never a blank list)
	checkEq( locFr.worlds.value( 0 ).name, QStringLiteral( "Localized World" ), QStringLiteral( "no table for the language: the en names" ) );
	check( locFr.noStrings.isEmpty(), QStringLiteral( "no plugin reported when the en tables stand in" ) );
	OpenCellCatalog arch;
	openCellReadCatalog( { data + QStringLiteral( "/Loc2.esm" ) }, arch, nullptr, en );
	checkEq( arch.worlds.value( 0 ).name, QStringLiteral( "Archived World" ), QStringLiteral( "a table inside <plugin> - *.ba2" ) );
	OpenCellReadOptions de;
	de.lang = QStringLiteral( "de" );
	OpenCellCatalog loose;
	openCellReadCatalog( { data + QStringLiteral( "/Loc2.esm" ) }, loose, nullptr, de );
	checkEq( loose.worlds.value( 0 ).name, QStringLiteral( "Lose Welt" ), QStringLiteral( "a loose table beats the archive's" ) );
	OpenCellCatalog plain;
	openCellReadCatalog( { base }, plain, nullptr, en );
	checkEq( plain.worlds.value( 0 ).name, QStringLiteral( "Test World" ), QStringLiteral( "a plugin that is not localized keeps its FULL text" ) );

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
	OpenCellPicker dlg;
	dlg.setDataDir( data );
	dlg.setPluginsTicked( { QStringLiteral( "Mod.esp" ) } );
	dlg.readNow();
	checkEq( dlg.loadOrderPaths(), QStringList{ QDir::cleanPath( base ), QDir::cleanPath( mod ) }, QStringLiteral( "load order paths" ) );
	checkEq( dlg.worldItems(), QStringList{ QStringLiteral( "TestWorld (Test World)" ), QStringLiteral( "ModWorld" ),
		QStringLiteral( "Interior" ), QStringLiteral( "All Cells" ) },
		QStringLiteral( "worldspace list (overridden TestWorld listed once, then Interior and All Cells)" ) );
	dlg.selectWorld( QStringLiteral( "TestWorld" ) );
	checkEq( dlg.cellHeaders(), QStringList{ QStringLiteral( "Editor ID" ), QStringLiteral( "Name" ), QStringLiteral( "X" ),
		QStringLiteral( "Y" ), QStringLiteral( "FormID" ), QStringLiteral( "Plugin" ), QStringLiteral( "Last Edit" ) },
		QStringLiteral( "exterior columns as the CK's (no Worldspace column inside one worldspace)" ) );
	checkEq( dlg.cellItems(), QStringList{ QStringLiteral( "ModCell\t\t5\t-3\t01000800\tMod.esp\tMod.esp" ),
		QStringLiteral( "CellA\tCell A\t0\t0\t00000F03\tBase.esm\tBase.esm" ),
		QStringLiteral( "CellB\tCell B Edited\t1\t0\t00000F04\tBase.esm\tMod.esp" ),
		QStringLiteral( "Wilderness\t\t-1\t2\t00000F05\tBase.esm\tBase.esm" ) },
		QStringLiteral( "cell list: winning names, FormIDs, owner and last edit; a cell with no editor ID reads Wilderness" ) );
	dlg.selectWorld( QStringLiteral( "Interior" ) );
	checkEq( dlg.cellItems(), QStringList{ QStringLiteral( "AnotherRoom\t\t00000F11\tBase.esm\tBase.esm" ),
		QStringLiteral( "TestInterior\tTest Interior\t00000F10\tBase.esm\tBase.esm" ) },
		QStringLiteral( "interior list (no grid columns)" ) );

	/* lane OPENCELLFIX2 notes 1, 2, 7: no language box, no Open button (a double-click opens), every dropdown is the
	 * toolbar's Object Mode selector. Reds: WW_OPENCELL_RED=dblclick, WW_DROPDOWN_RED=1. */
	std::printf( "dock look\n" );
	{
		check( !dlg.findChild<QWidget *>( QStringLiteral( "OpenCellLanguage" ) ), QStringLiteral( "no language box" ) );
		check( !dlg.findChild<QWidget *>( QStringLiteral( "OpenCellOpen" ) ), QStringLiteral( "no Open button" ) );
		checkEq( int( dlg.findChildren<QComboBox *>().size() ), 0, QStringLiteral( "no plain combo box left" ) );
		checkEq( int( dlg.findChildren<QPushButton *>().size() ), 0, QStringLiteral( "no push button left" ) );
		const auto drops = dlg.findChildren<WwDropdown *>();
		int styled = 0;
		for ( const WwDropdown * d : drops )
			styled += WwDropdown::hasStyle( d );
		check( drops.size() >= 2 && styled == int( drops.size() ),
			QStringLiteral( "every dropdown has the Object Mode look (%1 of %2)" ).arg( styled ).arg( drops.size() ) );
		check( WwDropdown::hasStyle( dlg.findChild<QToolButton *>( QStringLiteral( "OpenCellFilters" ) ) ),
			QStringLiteral( "the Filters button has the same look" ) );

		dlg.selectWorld( QStringLiteral( "TestWorld" ) );
		QTreeWidget * tree = dlg.findChild<QTreeWidget *>( QStringLiteral( "OpenCellList" ) );
		QTreeWidgetItem * row = tree ? tree->topLevelItem( 1 ) : nullptr;	// CellA, not the current row
		dlg.selectCell( 5, -3, 1 );
		dlg.writeSpec( tmp.path() + QStringLiteral( "/specs" ) );	// the recent folder: Open writes beside it
		QString opened;
		auto conn = QObject::connect( &dlg, &OpenCellPicker::openRequested, [&]( const QString & p ) { opened = p; } );
		if ( row )
			emit tree->itemDoubleClicked( row, 0 );
		QObject::disconnect( conn );
		check( !opened.isEmpty() && QFile::exists( opened ), QStringLiteral( "a double-click opens the cell (openRequested)" ) );
		checkEq( dlg.specLine().section( QLatin1Char( '|' ), 1, 2 ), QStringLiteral( "TestWorld|0,0" ),
			QStringLiteral( "the double-clicked row is the cell opened" ) );
	}

	std::printf( "search and filters\n" );
	{
		dlg.selectWorld( QStringLiteral( "TestWorld" ) );
		auto hits = [&]( const QString & s ) { dlg.setSearch( s ); return dlg.cellCount(); };
		checkEq( hits( QStringLiteral( "cella" ) ), 1, QStringLiteral( "search: an editor ID (any case)" ) );
		checkEq( hits( QStringLiteral( "Edited" ) ), 1, QStringLiteral( "search: a name" ) );
		checkEq( hits( QStringLiteral( "01000800" ) ), 1, QStringLiteral( "search: a FormID" ) );
		checkEq( hits( QStringLiteral( "0x00000F05" ) ), 1, QStringLiteral( "search: a FormID with 0x" ) );
		checkEq( hits( QStringLiteral( "-1, 2" ) ), 1, QStringLiteral( "search: x, y" ) );
		checkEq( hits( QStringLiteral( "Wilderness" ) ), 1, QStringLiteral( "search: Wilderness finds the cell with no editor ID" ) );
		checkEq( hits( QStringLiteral( "cell b" ) ), 1, QStringLiteral( "search: every term must match" ) );
		checkEq( hits( QStringLiteral( "nothing-like-this" ) ), 0, QStringLiteral( "search: no match lists nothing" ) );
		checkEq( dlg.cellTotal(), 4, QStringLiteral( "the total stays the worldspace's" ) );
		auto act = [&]( const char * name ) {
			QAction * a = dlg.findChild<QAction *>( QLatin1String( name ) );
			check( a != nullptr, QStringLiteral( "filter action %1 exists" ).arg( QLatin1String( name ) ) );
			if ( a )
				a->trigger();
		};
		dlg.setSearch( QStringLiteral( "Edited" ) );
		act( "OpenCellSearchName" );   // Search In: name off
		checkEq( dlg.cellCount(), 0, QStringLiteral( "Search In without Name: a name no longer hits" ) );
		act( "OpenCellSearchName" );
		act( "OpenCellSearchEditorId" );
		act( "OpenCellSearchFormId" );
		dlg.setSearch( QStringLiteral( "00000F03" ) );
		checkEq( dlg.cellCount(), 0, QStringLiteral( "Search In without FormID: a FormID no longer hits" ) );
		act( "OpenCellSearchName" );   // the last field cannot be turned off
		checkEq( dlg.findChild<QAction *>( QStringLiteral( "OpenCellSearchName" ) )->isChecked(), true,
			QStringLiteral( "Search In keeps one field" ) );
		act( "OpenCellResetSearchFilters" );
		checkEq( dlg.cellCount(), 4, QStringLiteral( "Reset Search and Filters lists everything" ) );

		dlg.selectWorld( QStringLiteral( "All Cells" ) );
		checkEq( dlg.cellTotal(), 6, QStringLiteral( "All Cells = 4 exterior + 0 + 2 interior" ) );
		checkEq( dlg.cellHeaders().contains( QStringLiteral( "Worldspace" ) ), true, QStringLiteral( "All Cells shows the Worldspace column" ) );
		act( "OpenCellCellsInterior" );
		checkEq( dlg.cellCount(), 2, QStringLiteral( "filter interior only" ) );
		act( "OpenCellCellsExterior" );
		checkEq( dlg.cellCount(), 4, QStringLiteral( "filter exterior only" ) );
		act( "OpenCellCellsAll" );
		act( "OpenCellNamed" );
		checkEq( dlg.cellCount(), 3, QStringLiteral( "filter with a name (Cell A, Cell B Edited, Test Interior)" ) );
		act( "OpenCellUnnamed" );
		checkEq( dlg.cellCount(), 3, QStringLiteral( "filter without a name" ) );
		act( "OpenCellNameAny" );
		act( "OpenCellPlugin_Mod.esp" );
		checkEq( dlg.cellCount(), 1, QStringLiteral( "filter added by Mod.esp" ) );
		act( "OpenCellPluginLastEdit" );
		checkEq( dlg.cellCount(), 2, QStringLiteral( "filter last edited by Mod.esp" ) );
		act( "OpenCellPlugin_Base.esm" );
		checkEq( dlg.cellCount(), 4, QStringLiteral( "filter last edited by Base.esm" ) );
		act( "OpenCellResetSearchFilters" );
		checkEq( dlg.cellCount(), 6, QStringLiteral( "reset again lists all 6" ) );
		dlg.selectWorld( QStringLiteral( "ModWorld" ) );
		checkEq( dlg.cellTotal(), 0, QStringLiteral( "a worldspace lists only its own cells" ) );

		dlg.selectWorld( QStringLiteral( "TestWorld" ) );
		dlg.sortCells( 2, false );   // X
		checkEq( dlg.cellItems().value( 0 ).section( QLatin1Char( '\t' ), 0, 0 ), QStringLiteral( "Wilderness" ),
			QStringLiteral( "sort by X: -1 first" ) );
		dlg.sortCells( 4, true );    // FormID, descending
		checkEq( dlg.cellItems().value( 0 ).section( QLatin1Char( '\t' ), 0, 0 ), QStringLiteral( "ModCell" ),
			QStringLiteral( "sort by FormID descending: 01000800 first" ) );
		dlg.sortCells( -1, false );
		checkEq( hex( openCellShownForm( 0x03000800U, 3, { false, true, true, false } ) ), hex( 0x01000800U ),
			QStringLiteral( "load-order FormID counts full plugins only" ) );
		checkEq( hex( openCellShownForm( 0xFE001C02U, 2, { false, true, true } ) ), hex( 0xFE001C02U ),
			QStringLiteral( "a light plugin's FE FormID stays" ) );
	}

	std::printf( ".wwcell\n" );
	const QString cells = tmp.path() + QStringLiteral( "/cells" );
	dlg.selectWorld( QStringLiteral( "All Cells" ) );
	dlg.selectInterior( QStringLiteral( "TestInterior" ) );
	checkEq( dlg.specLine().section( QLatin1Char( '|' ), 1 ), QStringLiteral( "interior|TestInterior" ),
		QStringLiteral( "an interior picked under All Cells opens as an interior" ) );
	dlg.selectWorld( QStringLiteral( "Interior" ) );
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

	/* lane OPENCELLFIX1, the bug: an MO2 plugins.txt lists the mods only; the game loads Fallout4.esm, the DLC and the
	 * Creation Club files first whatever the file says. Without them the cell opened with 0 references.
	 * Red: WW_OPENCELL_RED=masters (the old import; a missing master no longer refused) must FAIL. */
	std::printf( "game load order\n" );
	const QString game = tmp.path() + QStringLiteral( "/Game" );
	const QString data2 = game + QStringLiteral( "/Data" );
	writeFile( data2 + QStringLiteral( "/Fallout4.esm" ), header( 1, {} )
		+ group( "WRLD", 0, record( "WRLD", 0x3C, zstring( "EDID", "Commonwealth" ) ) ) );
	writeFile( data2 + QStringLiteral( "/DLCRobot.esm" ), header( 1, { QStringLiteral( "Fallout4.esm" ) } ) );
	writeFile( data2 + QStringLiteral( "/ccTest.esl" ), header( 0x201, { QStringLiteral( "Fallout4.esm" ) } ) );
	writeFile( game + QStringLiteral( "/Fallout4.ccc" ), "ccTest.esl\n" );
	writeFile( data2 + QStringLiteral( "/Mod2.esp" ), header( 0, { QStringLiteral( "Fallout4.esm" ) } ) );
	const QString gameTxt = game + QStringLiteral( "/plugins.txt" );
	writeFile( gameTxt, "*Mod2.esp\n" );
	const QStringList gameOrder{ QStringLiteral( "Fallout4.esm" ), QStringLiteral( "DLCRobot.esm" ),
		QStringLiteral( "ccTest.esl" ), QStringLiteral( "Mod2.esp" ) };
	{
		OpenCellSources gs;
		gs.dataDir = data2;
		QStringList got;
		openCellImportLoadOrder( gameTxt, got, gs, nullptr );
		checkEq( got, gameOrder, QStringLiteral( "a plugins.txt import leads with Fallout4.esm, the DLC and the CC files" ) );

		const QString mo = tmp.path() + QStringLiteral( "/MO2b" );
		writeFile( mo + QStringLiteral( "/profiles/P/plugins.txt" ), "*Mod3.esp\n" );
		writeFile( mo + QStringLiteral( "/profiles/P/modlist.txt" ), "+M3\n" );
		writeFile( mo + QStringLiteral( "/mods/M3/Mod3.esp" ), header( 0, { QStringLiteral( "Fallout4.esm" ) } ) );
		OpenCellSources ms;
		ms.dataDir = data2;
		openCellImportLoadOrder( mo + QStringLiteral( "/profiles/P/plugins.txt" ), got, ms, nullptr );
		checkEq( got, QStringList{ QStringLiteral( "Fallout4.esm" ), QStringLiteral( "DLCRobot.esm" ),
			QStringLiteral( "ccTest.esl" ), QStringLiteral( "Mod3.esp" ) },
			QStringLiteral( "an MO2 profile import leads with the masters, as the LOD panel reads it" ) );
		bool hasMod = false;
		for ( const QString & r : ms.resources )
			hasMod = hasMod || QDir::cleanPath( r ).endsWith( QStringLiteral( "/mods/M3" ) );
		checkEq( hasMod, true, QStringLiteral( "the profile's mod folders become the cell's resources" ) );
	}
	writeFile( data2 + QStringLiteral( "/Orphan.esp" ), header( 0, { QStringLiteral( "Gone.esm" ) } )
		+ group( "WRLD", 0, record( "WRLD", 0x01000801, zstring( "EDID", "OrphanWorld" ) ) ) );
	{
		OpenCellPicker p;
		p.setDataDir( data2 );
		p.setPluginsTicked( { QStringLiteral( "Orphan.esp" ) } );
		p.readNow();
		p.selectWorld( QStringLiteral( "OrphanWorld" ) );
		p.selectCell( 0, 0, 1 );
		checkEq( p.specLine(), QString(), QStringLiteral( "a load order missing a master opens nothing (no 0-reference cell)" ) );
		checkEq( p.refusal().contains( QStringLiteral( "Gone.esm" ) ), true, QStringLiteral( "the refusal names the missing master" ) );
	}

	/* lane OPENCELLFIX1 (bungo: "make sure the plugin list is saved between sessions"): every choice comes back on the
	 * next start. Red: WW_OPENCELL_RED=restore must FAIL. */
	std::printf( "saved between starts\n" );
	QStringList aTicked;
	{
		OpenCellPicker a;
		a.setDataDir( data );
		a.setPluginsTicked( { QStringLiteral( "Mod.esp" ) } );
		a.readNow();
		a.selectWorld( QStringLiteral( "TestWorld" ) );
		a.selectCell( 5, -3, 3 );
		aTicked = a.tickedPlugins();
	}
	{
		OpenCellPicker b;
		checkEq( b.loadOrderSources().dataDir, QDir::cleanPath( data ), QStringLiteral( "the Data folder comes back" ) );
		checkEq( b.tickedPlugins(), aTicked, QStringLiteral( "the ticked plugins come back" ) );
		b.readNow();
		checkEq( b.specLine().section( QLatin1Char( '|' ), 1 ), QStringLiteral( "TestWorld|5,-3|3" ),
			QStringLiteral( "the worldspace, X,Y and size come back" ) );
	}
	{	// the saved Data folder is gone: the LOD panel's profile (here the game's plugins.txt) fills the tab
		QSettings().setValue( QStringLiteral( "WW/OpenCell/DataDir" ), tmp.path() + QStringLiteral( "/NoSuchData" ) );
		g_fallback = { data2, gameTxt };
		OpenCellPicker c;
		checkEq( c.loadOrderSources().dataDir, QDir::cleanPath( data2 ), QStringLiteral( "a gone Data folder falls back to the profile's" ) );
		checkEq( sortedCopy( c.tickedPlugins() ), sortedCopy( gameOrder ), QStringLiteral( "the fallback's load order is ticked, masters too" ) );
		checkEq( c.loadOrderFile(), QDir::cleanPath( gameTxt ), QStringLiteral( "the fallback's plugins.txt is the load-order file" ) );
	}
	{	// plugins.txt changed since: read again
		writeFile( data2 + QStringLiteral( "/Extra.esp" ), header( 0, { QStringLiteral( "Fallout4.esm" ) } ) );
		writeFile( gameTxt, "*Mod2.esp\n*Extra.esp\n" );
		OpenCellPicker d;
		checkEq( d.tickedPlugins().contains( QStringLiteral( "Extra.esp" ) ), true,
			QStringLiteral( "a plugins.txt changed since the last start is read again" ) );
	}

	/* lane OPENCELLFIX2 note 3: the strings language is the game's [General] sLanguage (Fallout4.ini, Prefs, Custom; a
	 * later one wins; an MO2 profile's own inis first); default en. A saved "e" never blanks names. Red: WW_OPENCELL_RED=lang. */
	std::printf( "strings language\n" );
	{
		const QString mg = tmp.path() + QStringLiteral( "/MyGames" ), prof = tmp.path() + QStringLiteral( "/Profile" );
		qputenv( "WW_MODPICK_MYGAMES", QFile::encodeName( mg ) );
		QSettings().setValue( QStringLiteral( "WW/OpenCell/Language" ), QStringLiteral( "e" ) );	// the old box's bad value
		checkEq( openCellStringsLanguage(), QStringLiteral( "en" ), QStringLiteral( "no game ini (and a saved \"e\"): en" ) );
		OpenCellCatalog saved;
		openCellReadCatalog( { data + QStringLiteral( "/Loc.esm" ) }, saved, nullptr, OpenCellReadOptions() );
		checkEq( saved.worlds.value( 0 ).name, QStringLiteral( "Localized World" ), QStringLiteral( "a saved \"e\" still reads en names" ) );
		writeFile( mg + QStringLiteral( "/Fallout4.ini" ), "[Display]\nsLanguage=fr\n[General]\nsLanguage=de\n" );
		checkEq( openCellStringsLanguage(), QStringLiteral( "de" ), QStringLiteral( "Fallout4.ini [General] sLanguage" ) );
		writeFile( mg + QStringLiteral( "/Fallout4Custom.ini" ), "[General]\r\nsLanguage = FR\r\n" );
		checkEq( openCellStringsLanguage(), QStringLiteral( "fr" ), QStringLiteral( "Fallout4Custom.ini wins (case, spaces, CRLF)" ) );
		writeFile( mg + QStringLiteral( "/Fallout4Custom.ini" ), "[General]\nsLanguage=e\n" );
		checkEq( openCellStringsLanguage(), QStringLiteral( "en" ), QStringLiteral( "an invalid sLanguage reads en" ) );
		writeFile( prof + QStringLiteral( "/Fallout4.ini" ), "[General]\nsLanguage=pl\n" );
		checkEq( openCellStringsLanguage( prof ), QStringLiteral( "pl" ), QStringLiteral( "the profile's own ini wins" ) );
		QFile::remove( mg + QStringLiteral( "/Fallout4.ini" ) );
		QFile::remove( mg + QStringLiteral( "/Fallout4Custom.ini" ) );
	}

	std::printf( "%d checks, %d failed\n%s\n", checks, fails, fails ? "FAIL" : "PASS" );
	return fails ? 1 : 0;
}
