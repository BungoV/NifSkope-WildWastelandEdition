/* Standalone gate for src/hkxfile.{h,cpp} (lane HKXEDIT1, 2026-09-10): links
   the packfile object model against Qt6Core alone, so the round trip, the
   edit->save->reload and the mutation gates run without NifSkope.exe
   (ww-standalone-writer-gate). Built by scratchpad/hkxedit1_20260910/build_gate.sh
   into release/hkxfile_gate.exe.

     hkxfile_gate dump <file.hkx>                      the object tree, one field per line
     hkxfile_gate roundtrip <file.hkx> [--out x.hkx]   exit 0 = byte-identical, 1 = differs, 2 = refused
     hkxfile_gate census <dir> [--report out.tsv]      every .hkx under dir: identical / mismatch / refused
     hkxfile_gate edit <in.hkx> <out.hkx> --set <path>=<value> ... [--resize <path>=<n>]
                                                       set scalars/strings/enums by path, resize arrays, write
     hkxfile_gate get <file.hkx> <path> ...            print each value (for the reload side of the edit gate)
     hkxfile_gate classdb                              the class database's provenance line and a count

   Every option accepts --db <hkclasses_fo4.json>; the default search is
   Hkx::ClassDb::defaultPaths() (WW_HKCLASSDB, beside the exe, ../res, ./res). */

#include "hkxfile.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <cstdio>

using namespace Hkx;

static QTextStream & out()
{
	static QTextStream s( stdout );
	return s;
}

static QByteArray readAll( const QString & p )
{
	QFile f( p );
	if ( !f.open( QIODevice::ReadOnly ) )
		return QByteArray();
	return f.readAll();
}

static bool writeAll( const QString & p, const QByteArray & b )
{
	QFile f( p );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		return false;
	return f.write( b ) == b.size();
}

static QString locate( const File & f, qint64 off )
{
	for ( const File::Chunk & c : f.chunks )
		if ( c.offset <= off && off < c.offset + qMax( c.size, 1 ) )
			return QStringLiteral( "%1 %2 (+0x%3)" ).arg( c.kind, c.label ).arg( off - c.offset, 0, 16 );
	return QStringLiteral( "outside every chunk" );
}

static int cmdRoundtrip( const ClassDb & db, const QString & path, const QString & outPath )
{
	const QByteArray blob = readAll( path );
	if ( blob.isEmpty() ) {
		out() << "REFUSED: cannot read " << path << Qt::endl;
		return 2;
	}
	File f;
	if ( !f.read( blob, db ) ) {
		out() << "REFUSED: " << f.error << Qt::endl;
		return 2;
	}
	const QByteArray w = f.write( db );
	if ( w.isEmpty() ) {
		out() << "REFUSED (write): " << f.error << Qt::endl;
		return 2;
	}
	if ( !outPath.isEmpty() )
		writeAll( outPath, w );
	const int n = qMin( blob.size(), w.size() );
	for ( int i = 0; i < n; i++ ) {
		if ( blob[i] != w[i] ) {
			out() << QStringLiteral( "MISMATCH first diff at 0x%1 in %2; sizes %3 -> %4" ).arg( i, 0, 16 ).arg( locate( f, i ) ).arg( blob.size() ).arg( w.size() ) << Qt::endl;
			return 1;
		}
	}
	if ( blob.size() != w.size() ) {
		out() << QStringLiteral( "MISMATCH sizes %1 -> %2 (common prefix identical)" ).arg( blob.size() ).arg( w.size() ) << Qt::endl;
		return 1;
	}
	out() << "IDENTICAL " << blob.size() << " bytes, " << f.objects.size() << " objects, " << f.classesUsed().size() << " classes" << Qt::endl;
	return 0;
}

static int cmdCensus( const ClassDb & db, const QString & dir, const QString & report )
{
	QDirIterator it( dir, QStringList() << QStringLiteral( "*.hkx" ), QDir::Files, QDirIterator::Subdirectories );
	const QDir base( QFileInfo( dir ).absoluteFilePath() );	// rows are relative to the census root, whatever `dir` was
	int n = 0, ident = 0, mism = 0, refused = 0;
	QHash<QString, int> reasons;
	QStringList rows;
	QStringList mismatches;
	while ( it.hasNext() ) {
		const QString p = it.next();
		const QString rel = base.relativeFilePath( QFileInfo( p ).absoluteFilePath() );
		n++;
		const QByteArray blob = readAll( p );
		File f;
		if ( !f.read( blob, db ) ) {
			refused++;
			QString key = f.error.section( QLatin1Char( ':' ), 0, 0 ).left( 90 );
			reasons[key]++;
			rows << rel + QLatin1String( "\tREFUSED\t" ) + f.error;
			continue;
		}
		const QByteArray w = f.write( db );
		int d = -1;
		const int m = qMin( blob.size(), w.size() );
		for ( int i = 0; i < m; i++ ) if ( blob[i] != w[i] ) { d = i; break; }
		if ( d < 0 && blob.size() != w.size() ) d = m;
		if ( d < 0 ) {
			ident++;
			rows << rel + QLatin1String( "\tIDENTICAL\t" );
		} else {
			mism++;
			const QString why = QStringLiteral( "first diff at 0x%1 in %2; sizes %3 -> %4" ).arg( d, 0, 16 ).arg( locate( f, d ) ).arg( blob.size() ).arg( w.size() );
			rows << rel + QLatin1String( "\tMISMATCH\t" ) + why;
			if ( mismatches.size() < 30 )
				mismatches << rel + QLatin1String( " " ) + why;
		}
	}
	out() << QStringLiteral( "census %1: %2 files, %3 byte-identical, %4 mismatched, %5 refused" ).arg( dir ).arg( n ).arg( ident ).arg( mism ).arg( refused ) << Qt::endl;
	for ( auto r = reasons.constBegin(); r != reasons.constEnd(); ++r )
		out() << QStringLiteral( "  refused %1  %2" ).arg( r.value(), 5 ).arg( r.key() ) << Qt::endl;
	for ( const QString & s : mismatches )
		out() << "  MISMATCH " << s << Qt::endl;
	if ( !report.isEmpty() ) {
		QFile rf( report );
		if ( rf.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
			rf.write( "file\tverdict\tdetail\n" );
			for ( const QString & r : rows ) rf.write( ( r + QLatin1Char( '\n' ) ).toUtf8() );
		}
	}
	return mism == 0 ? 0 : 1;
}

static bool applySet( File & f, const QString & spec, QString * err )
{
	const int eq = spec.indexOf( QLatin1Char( '=' ) );
	if ( eq < 0 ) { *err = QStringLiteral( "--set needs path=value, got %1" ).arg( spec ); return false; }
	const QString path = spec.left( eq ), val = spec.mid( eq + 1 );
	Value * v = f.find( path, err );
	if ( !v ) return false;
	switch ( v->kind ) {
	case Value::Scalar:
		if ( v->type == Type::Real ) {
			bool ok = false;
			const float x = val.toFloat( &ok );
			if ( !ok ) { *err = QStringLiteral( "%1: '%2' is not a float" ).arg( path, val ); return false; }
			v->setFloat( x );
		} else if ( ( v->type == Type::Enum || v->type == Type::Flags ) && v->member && v->member->en && !val[0].isDigit() && val[0] != QLatin1Char( '-' ) ) {
			qint64 x = 0;
			if ( !v->member->en->valueOf( val, &x ) ) { *err = QStringLiteral( "%1: '%2' is not an item of enum %3" ).arg( path, val, v->member->en->name ); return false; }
			v->setInt( x );
		} else {
			bool ok = false;
			const qint64 x = val.toLongLong( &ok );
			if ( !ok ) { *err = QStringLiteral( "%1: '%2' is not an integer" ).arg( path, val ); return false; }
			v->setInt( x );
		}
		return true;
	case Value::Str:
		v->setStringValue( val );
		return true;
	case Value::Ptr: {
		bool ok = false;
		const int o = val.startsWith( QLatin1Char( '#' ) ) ? val.mid( 1 ).toInt( &ok ) : val.toInt( &ok );
		if ( !ok || o < -1 || o >= f.objects.size() ) { *err = QStringLiteral( "%1: '%2' is not an object index" ).arg( path, val ); return false; }
		v->object = o;
		return true;
	}
	default:
		*err = QStringLiteral( "%1: a value of kind %2 cannot be set from text" ).arg( path ).arg( int( v->kind ) );
		return false;
	}
}

static QString getText( const File & f, const QString & path, QString * err )
{
	const Value * v = f.find( path, err );
	if ( !v ) return QString();
	if ( v->kind == Value::Scalar && v->type == Type::Real )
		return QString::number( v->toFloat(), 'g', 9 );
	if ( v->kind == Value::Scalar )
		return QString::number( v->toInt() );
	if ( v->kind == Value::Str )
		return v->isNull ? QStringLiteral( "null" ) : v->toStringValue();
	if ( v->kind == Value::Array || v->kind == Value::RelArray )
		return QStringLiteral( "n=%1" ).arg( v->count );
	return describe( *v, &f );
}

int main( int argc, char ** argv )
{
	QCoreApplication app( argc, argv );
	QStringList args = app.arguments();
	args.removeFirst();
	QString dbPath;
	for ( int i = 0; i + 1 < args.size(); i++ ) {
		if ( args[i] == QLatin1String( "--db" ) ) {
			dbPath = args[i + 1];
			args.removeAt( i ); args.removeAt( i );
			break;
		}
	}
	QString err;
	const ClassDb * db = dbPath.isEmpty() ? ClassDb::instance( &err ) : ClassDb::load( dbPath, &err );
	if ( !db ) {
		out() << "REFUSED: " << err << Qt::endl;
		return 2;
	}
	if ( args.isEmpty() ) {
		out() << "usage: hkxfile_gate dump|roundtrip|census|edit|get|classdb ..." << Qt::endl;
		return 3;
	}
	const QString cmd = args.takeFirst();
	if ( cmd == QLatin1String( "classdb" ) ) {
		out() << db->sourceSummary() << Qt::endl;
		int registered = 0;
		for ( const QString & n : db->names() ) if ( db->find( n )->registryIndex >= 0 ) registered++;
		out() << db->count() << " classes, " << registered << " registered" << Qt::endl;
		return 0;
	}
	if ( cmd == QLatin1String( "dump" ) ) {
		if ( args.isEmpty() ) return 3;
		File f;
		if ( !f.read( readAll( args[0] ), *db ) ) { out() << "REFUSED: " << f.error << Qt::endl; return 2; }
		out() << dump( f );
		return 0;
	}
	if ( cmd == QLatin1String( "roundtrip" ) ) {
		if ( args.isEmpty() ) return 3;
		QString o;
		const int k = args.indexOf( QLatin1String( "--out" ) );
		if ( k >= 0 && k + 1 < args.size() ) o = args[k + 1];
		return cmdRoundtrip( *db, args[0], o );
	}
	if ( cmd == QLatin1String( "census" ) ) {
		if ( args.isEmpty() ) return 3;
		QString rep;
		const int k = args.indexOf( QLatin1String( "--report" ) );
		if ( k >= 0 && k + 1 < args.size() ) rep = args[k + 1];
		return cmdCensus( *db, args[0], rep );
	}
	if ( cmd == QLatin1String( "get" ) ) {
		if ( args.size() < 2 ) return 3;
		File f;
		if ( !f.read( readAll( args[0] ), *db ) ) { out() << "REFUSED: " << f.error << Qt::endl; return 2; }
		for ( int i = 1; i < args.size(); i++ ) {
			QString e;
			const QString t = getText( f, args[i], &e );
			out() << args[i] << " = " << ( e.isEmpty() ? t : QLatin1String( "ERROR " ) + e ) << Qt::endl;
		}
		return 0;
	}
	if ( cmd == QLatin1String( "edit" ) ) {
		if ( args.size() < 2 ) return 3;
		File f;
		if ( !f.read( readAll( args[0] ), *db ) ) { out() << "REFUSED: " << f.error << Qt::endl; return 2; }
		for ( int i = 2; i + 1 < args.size(); i += 2 ) {
			QString e;
			if ( args[i] == QLatin1String( "--set" ) ) {
				if ( !applySet( f, args[i + 1], &e ) ) { out() << "REFUSED: " << e << Qt::endl; return 2; }
				out() << "set " << args[i + 1] << Qt::endl;
			} else if ( args[i] == QLatin1String( "--resize" ) ) {
				const int eq = args[i + 1].indexOf( QLatin1Char( '=' ) );
				Value * v = f.find( args[i + 1].left( eq ), &e );
				if ( !v || !f.resizeArray( *v, args[i + 1].mid( eq + 1 ).toInt(), &e ) ) { out() << "REFUSED: " << e << Qt::endl; return 2; }
				out() << "resized " << args[i + 1] << Qt::endl;
			} else {
				out() << "unknown option " << args[i] << Qt::endl;
				return 3;
			}
		}
		const QByteArray w = f.write( *db );
		if ( w.isEmpty() ) { out() << "REFUSED (write): " << f.error << Qt::endl; return 2; }
		if ( !writeAll( args[1], w ) ) { out() << "cannot write " << args[1] << Qt::endl; return 2; }
		out() << "wrote " << args[1] << " " << w.size() << " bytes" << Qt::endl;
		return 0;
	}
	out() << "unknown command " << cmd << Qt::endl;
	return 3;
}
