// Standalone driver for the known-answer control, so the .lodo/.lodi writers
// can be exercised WITHOUT release/NifSkope.exe (held by another lane's gates
// while this ran). Links src/lodofile.cpp + src/lodifile.cpp + src/io/lodvfile.cpp
// against Qt6Core only; no NifSkope object is touched.
//
//   fixture_tool <dir>                 write Synthetic.lodo/.lodi/.expect.txt
//   fixture_tool --verify <lodo> <lodi> read both back with every check on
//   fixture_tool --mutate <in> <out> <off> <xor>   flip one byte (refusal control)
#include "lodifile.h"
#include "lodofile.h"

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>

int main( int argc, char ** argv )
{
	QCoreApplication app( argc, argv );
	QTextStream out( stdout );
	QStringList args = app.arguments();
	args.removeFirst();
	if ( args.size() == 3 && args[0] == QLatin1String( "--verify" ) ) {
		LodoHeader lh; LodoLibrary lib; LodiHeader ih; LodiTable tab;
		QString err;
		if ( !lodoRead( args[1], &lh, &lib, true, &err ) ) { out << "REFUSED " << err << "\n"; return 1; }
		for ( const QString & l : lodoDescribe( lh, &lib ) ) out << "lodo " << l << "\n";
		if ( !lodiRead( args[2], &ih, &tab, true, &err ) ) { out << "REFUSED " << err << "\n"; return 1; }
		for ( const QString & l : lodiDescribe( ih, &tab ) ) out << "lodi " << l << "\n";
		out << "identity " << ( ih.lodoIdentity == lodoIdentityOf( lh.headerCrc32, lh.modelCorpusHash, lh.objectCorpusHash ) ? "matches" : "MISMATCH" ) << "\n";
		return 0;
	}
	if ( args.size() == 5 && args[0] == QLatin1String( "--mutate" ) ) {
		QFile f( args[1] );
		if ( !f.open( QIODevice::ReadOnly ) ) return 2;
		QByteArray b = f.readAll();
		const int off = args[3].toInt();
		const int x = args[4].toInt( nullptr, 0 );
		if ( off < 0 || off >= b.size() ) return 2;
		b[off] = char( quint8( b[off] ) ^ quint8( x ) );
		QFile g( args[2] );
		if ( !g.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) return 2;
		g.write( b );
		out << "mutated byte " << off << " of " << args[1] << "\n";
		return 0;
	}
	if ( args.size() != 1 ) {
		out << "usage: fixture_tool <dir> | --verify <lodo> <lodi> | --mutate <in> <out> <off> <xor>\n";
		return 2;
	}
	QStringList report;
	QString err;
	if ( !lodNativeFixtureWrite( args[0], &report, &err ) ) {
		out << "FAIL " << err << "\n";
		return 1;
	}
	for ( const QString & l : report )
		out << l << "\n";
	return 0;
}
