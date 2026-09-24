/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

***** END LICENSE BLOCK *****/

#include "lodbfile.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QTextStream>

#include "lodgen.h"


/* ==========================================================================
 * THE BAKE RECORD `.lodb`, version 2 -- lane BAKEREC1, 2026-09-17.
 *
 * The block comment in lodbfile.h says WHY there is one file and not two, and
 * which three lines are deliberately volatile. This file is the container and
 * nothing here decides policy.
 * ======================================================================== */

static const int    LODB_TEXT_VERSION = 2;
static const char * LODB_V1_MAGIC     = "LODB";   //!< the v1 binary container


/* ---- FNV-1a 64, the same constants loadOrderHash folds with --------------
 * The plugin hash is the ONE thing loadOrderHash cannot see: it folds the file
 * NAME and the file SIZE, so a plugin edited in place to the same size passes
 * it. This folds the bytes. Streamed, because Fallout4.esm is 330 MB and a bake
 * that had to hold it twice would be a regression nobody asked for. */
bool lodbFileFnv1a64( const QString & path, quint64 * out )
{
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) )
		return false;
	quint64 h = Q_UINT64_C( 0xCBF29CE484222325 );
	QByteArray buf;
	while ( !( buf = f.read( 1 << 20 ) ).isEmpty() ) {
		const unsigned char * p = reinterpret_cast<const unsigned char *>( buf.constData() );
		for ( int i = 0; i < buf.size(); i++ ) {
			h ^= quint64( p[i] );
			h *= Q_UINT64_C( 0x100000001B3 );
		}
	}
	if ( out )
		*out = h;
	return true;
}


/* ---- the census recorder ------------------------------------------------
 * REGISTERED BY KEYWORD, in one place, from docs/LODGEN_CENSUS.md section 6.1.
 * A keyword is matched against the START of the line, so `native:` and
 * `native-ladder:` are two entries and neither swallows the other. This list is
 * the only thing to add to when a lane ships a new census line, and the gate's
 * floor greps the bake log for exactly these keywords, so forgetting to add one
 * fails a check rather than shortening a record in silence.
 *
 * WHAT IS DELIBERATELY NOT HERE, and why. Section 6.1 also lists the per-CHUNK
 * diagnostics -- `cover ...`, `roads ...`, `bake x,y: ...`, `terrainObjectAo
 * ...`. Those are printed to stderr from inside the chunk WORKERS
 * (`src/lodgen.cpp:9720` and neighbours), so their ORDER is the thread
 * scheduler's, and a record that carried them would differ between two bakes of
 * one tree -- which is the one property the format may not lose (lodbfile.h).
 * The record carries what the bake says ONCE, in a fixed order; what it says
 * per chunk is already in the record as that chunk's input digest and its
 * outputs' digests. Named here rather than silently dropped. */
static const char * const g_censusKeywords[] = {
	"native:", "native-ladder:", "native-ladder-refused:", "native-library:",
	/* `native-library-build:` (lane PERF1) is a SECOND keyword rather than a
	 * second line under `native-library:`, because every other keyword in
	 * this list appears exactly once in a record and a reader greps by
	 * keyword. It says whether the object library was built or kept. */
	"native-library-build:",
	"native-casters:", "native-occluders:",
	"vt:",
	"arrays written:", "card arrays written:", "merged:", "far rings:",
	"bake census:", "stage times:", "incremental:", "bto scratch:",
	/* `bake-record:` is NOT here, and cannot be: it is printed after the record
	 * is closed and it describes the record, read back off the disk. A file
	 * cannot contain the sentence that counts its own bytes. The gate accounts
	 * for it by name rather than by a tolerance. */
	nullptr
};

static QMutex       g_censusMutex;
static QStringList  g_censusLines;

bool lodbIsCensusLine( const QString & line )
{
	const QString t = line.trimmed();
	if ( t.isEmpty() )
		return false;
	for ( const char * const * k = g_censusKeywords; *k; k++ )
		if ( t.startsWith( QLatin1String( *k ) ) )
			return true;
	return false;
}

void lodbClearCensus()
{
	QMutexLocker lock( &g_censusMutex );
	g_censusLines.clear();
}

void lodbNoteCensus( const QString & block )
{
	QMutexLocker lock( &g_censusMutex );
	const QStringList lines = block.split( QChar( '\n' ) );
	for ( const QString & l : lines ) {
		const QString t = l.trimmed();
		/* Tabs would break `key<TAB>fields` open, so a census line carrying one
		 * has it folded to a space and the fact is not hidden: the gate's floor
		 * compares against the log with the same fold. */
		QString flat = t;
		flat.replace( QChar( '\t' ), QChar( ' ' ) );
		if ( !flat.isEmpty() && lodbIsCensusLine( flat ) )
			g_censusLines.append( flat );
	}
}

QStringList lodbCensusLines()
{
	QMutexLocker lock( &g_censusMutex );
	return g_censusLines;
}


/* ---- the exe that baked it ----------------------------------------------
 * Composed the way src/main.cpp:109-125 composes the window title, and for the
 * same reason it gives there: the compiled-in NIFSKOPE_REVISION is baked when
 * qmake runs and after an incremental build it names a commit the binary is
 * not, so build_rev.txt (written by QMAKE_PRE_LINK at every link) wins. */
QString lodbExeStamp()
{
	QString rev;
	QFile revFile( QCoreApplication::applicationDirPath() + QStringLiteral( "/build_rev.txt" ) );
	if ( revFile.open( QIODevice::ReadOnly | QIODevice::Text ) )
		rev = QString::fromLatin1( revFile.readLine() ).trimmed();
#ifdef NIFSKOPE_REVISION
	if ( rev.isEmpty() )
		rev = QStringLiteral( NIFSKOPE_REVISION );
#endif
#ifdef WW_EDITION_VERSION
	const QString ed = QStringLiteral( WW_EDITION_VERSION );
#else
	const QString ed = QStringLiteral( "unknown" );
#endif
	return rev.isEmpty() ? ed : ( ed + QChar( '+' ) + rev );
}

qint64 lodbExeSize()
{
	return QFileInfo( QCoreApplication::applicationFilePath() ).size();
}


/* ---- normalising away the five volatile things -------------------------- */
QStringList lodbNormalise( const QStringList & lines )
{
	QStringList out;
	for ( const QString & l : lines ) {
		if ( l.startsWith( QLatin1String( "baked\t" ) ) ) {
			out << QStringLiteral( "baked\t<volatile>" );
			continue;
		}
		if ( l.startsWith( QLatin1String( "resource\t" ) ) ) {
			/* kind is content; path, size and mtime are where the operator
			 * happens to keep the stack. The KIND and the ORDER stay, so a
			 * reordered stack still shows. */
			const QStringList f = l.split( QChar( '\t' ) );
			out << QStringLiteral( "resource\t" ) + ( f.size() > 1 ? f.at( 1 ) : QString() )
				+ QStringLiteral( "\t<volatile>" );
			continue;
		}
		if ( l.startsWith( QLatin1String( "census\tstage times:" ) ) ) {
			/* THE FOURTH VOLATILE THING, found by the gate rather than by
			 * thinking about it (lane BAKEREC1, 2026-09-17, leg (h)): the
			 * `stage times:` census line is a WALL CLOCK. Two bakes of one tree
			 * differ in it by tenths of a second, so a record that carries it
			 * verbatim is not deterministic.
			 *
			 * It is RECORDED and MASKED, not dropped: an operator reading a
			 * record wants to know the bake took a minute, and dropping it
			 * would also put the census completeness floor and the census the
			 * bake printed permanently out of step. */
			out << QStringLiteral( "census\tstage times: <volatile>" );
			continue;
		}
		if ( l.startsWith( QLatin1String( "census\t" ) )
		      && l.contains( QLatin1String( "peak working set: " ) ) ) {
			/* THE FIFTH VOLATILE THING, handed over by lane ARCHLOCK1 and
			 * masked by lane INCR1 (2026-09-17): the chunk-pass census line
			 * carries this process's PEAK WORKING SET
			 * (`lodgenPeakWorkingSetLine()`, src/lodgenparallel.cpp). That is a
			 * measurement of THIS MACHINE AT THIS MOMENT, not of the bake's
			 * inputs: two bakes of one tree differ in it by megabytes, and four
			 * gates were red on this one clause.
			 *
			 * It sits INLINE in a line whose other facts -- thread counts, the
			 * chunk-thread bound, chunk jobs and workers, the bto disposition,
			 * the layout root and its file counts -- are CONTENT and must keep
			 * comparing. (`ww-volatile-field-law` step 1 would have had it on a
			 * line of its own; moving it would move a census line bungo reads,
			 * so the clause is masked where it stands and the divergence is
			 * stated in docs/LODGEN_BAKE_RECORD.md section 3.)
			 *
			 * Exactly the one clause goes: from `peak working set: ` to the next
			 * comma, or to the end of the line when there is none. NEITHER
			 * spelling lodgenPeakWorkingSetLine() produces ("... GB (N bytes)" /
			 * "not available on this platform") contains a comma, so the clause
			 * cannot run past its own end and eat a fact that does not move. */
			const int at = l.indexOf( QLatin1String( "peak working set: " ) )
			               + 18;   /* strlen( "peak working set: " ) */
			int to = l.indexOf( QChar( ',' ), at );
			if ( to < 0 )
				to = l.size();
			out << l.left( at ) + QStringLiteral( "<volatile>" ) + l.mid( to );
			continue;
		}
		if ( l.startsWith( QLatin1String( "plugin\t" ) ) ) {
			/* index, name, size and the byte hash are content. The full path is
			 * informational and is exactly what section 8.1's law says must not
			 * be able to fire a staleness refusal. */
			QStringList f = l.split( QChar( '\t' ) );
			while ( f.size() > 5 )
				f.removeLast();
			f << QStringLiteral( "<volatile>" );
			out << f.join( QChar( '\t' ) );
			continue;
		}
		out << l;
	}
	return out;
}


/* ---- the plugin diff: which one went stale, by name ---------------------- */
QStringList lodbDiffPlugins( const QVector<LodbPlugin> & recorded, const QString & nowPaths )
{
	QStringList msg;
	QVector<LodbPlugin> now;
	const QStringList parts = nowPaths.split( QChar( ',' ), Qt::SkipEmptyParts );
	for ( int i = 0; i < parts.size(); i++ ) {
		const QFileInfo fi( parts.at( i ).trimmed() );
		LodbPlugin p;
		p.index = i;
		p.name  = fi.fileName().toLower();
		p.bytes = fi.size();
		p.path  = fi.absoluteFilePath();
		/* The byte hash is computed ONLY for a plugin whose name and size still
		 * match the record's: that is the only case where the cheap fields
		 * cannot answer, and it is the case worth 330 MB of reading. */
		now.append( p );
	}
	QHash<QString, int> recAt, nowAt;
	for ( const LodbPlugin & p : recorded )
		recAt.insert( p.name, p.index );
	for ( const LodbPlugin & p : now )
		nowAt.insert( p.name, p.index );

	for ( const LodbPlugin & p : now )
		if ( !recAt.contains( p.name ) )
			msg << QString( "plugin %1 %2 was ADDED since the bake" ).arg( p.index ).arg( p.name );
	for ( const LodbPlugin & p : recorded )
		if ( !nowAt.contains( p.name ) )
			msg << QString( "plugin %1 %2 was REMOVED since the bake" ).arg( p.index ).arg( p.name );
	for ( LodbPlugin & p : now ) {
		const auto it = recAt.constFind( p.name );
		if ( it == recAt.constEnd() )
			continue;
		if ( it.value() != p.index )
			msg << QString( "plugin %1 %2 was REORDERED (it was %3 at the bake)" )
				.arg( p.index ).arg( p.name ).arg( it.value() );
		const LodbPlugin * r = nullptr;
		for ( const LodbPlugin & q : recorded )
			if ( q.name == p.name ) { r = &q; break; }
		if ( !r )
			continue;
		if ( r->bytes != p.bytes ) {
			msg << QString( "plugin %1 %2 was RESIZED (%3 bytes at the bake, %4 now) -- it was edited" )
				.arg( p.index ).arg( p.name ).arg( r->bytes ).arg( p.bytes );
			continue;
		}
		/* Same name, same size: the ONE case loadOrderHash is blind to. */
		quint64 h = 0;
		if ( !lodbFileFnv1a64( p.path, &h ) )
			msg << QString( "plugin %1 %2 cannot be read at %3 -- it moved or was deleted" )
				.arg( p.index ).arg( p.name ).arg( p.path );
		else if ( h != r->hash )
			msg << QString( "plugin %1 %2 was EDITED: its bytes hash 0x%3 now and hashed 0x%4 at the bake, "
				"at the same %5 bytes -- the load order cannot see this" )
				.arg( p.index ).arg( p.name )
				.arg( h, 16, 16, QChar( '0' ) ).arg( r->hash, 16, 16, QChar( '0' ) ).arg( p.bytes );
	}
	return msg;
}


/* ---- the writer ---------------------------------------------------------- */

//! Every file under `dir`, recursively, EXCEPT `except`. Read off the disk.
static void lodbCountTree( const QString & dir, const QString & except, int * files, qint64 * bytes )
{
	*files = 0;
	*bytes = 0;
	const QString skip = QDir::fromNativeSeparators( QFileInfo( except ).absoluteFilePath() ).toLower();
	QDirIterator it( dir, QDir::Files, QDirIterator::Subdirectories );
	while ( it.hasNext() ) {
		it.next();
		const QString abs = QDir::fromNativeSeparators( it.fileInfo().absoluteFilePath() ).toLower();
		if ( abs == skip )
			continue;
		( *files )++;
		*bytes += it.fileInfo().size();
	}
}

static QString tsv( const QString & s )
{
	/* A tab inside a field would break the format open, so it is folded to a
	 * space -- and a newline with it. Nothing else is escaped: the fields are
	 * paths, hex and census prose, and a reader that had to unescape would be a
	 * second parser. */
	QString t = s;
	t.replace( QChar( '\t' ), QChar( ' ' ) );
	t.replace( QChar( '\n' ), QChar( ' ' ) );
	t.replace( QChar( '\r' ), QChar( ' ' ) );
	return t;
}

bool lodgenWriteLedger( const QString & path, const LodgenLedger & led, QString * error )
{
	QStringList L;
	auto row = [&L]( const QStringList & f ) { L << f.join( QChar( '\t' ) ); };

	/* 1. the version line. An unknown version REFUSES BY NAME below, so this
	 *    number is the whole forward-compatibility contract. */
	row( { QStringLiteral( "lodb" ), QString::number( LODB_TEXT_VERSION ), tsv( led.worldEdid ),
	       tsv( led.exeStamp ), QString::number( led.exeBytes ) } );
	//    THE ONE VOLATILE LINE, on its own so a comparison can drop exactly it.
	row( { QStringLiteral( "baked" ), tsv( led.bakedUtc ) } );
	/*    The algorithms, stated rather than implied: a reader never has to know
	 *    which lane wrote which hash. */
	row( { QStringLiteral( "alg" ), QStringLiteral( "chunk=sha1" ), QStringLiteral( "file=sha1" ),
	       QStringLiteral( "plugin=fnv1a64" ), QStringLiteral( "switches=sha1" ) } );
	row( { QStringLiteral( "shape" ), QString::number( led.worldspace ), QString::number( led.dim ),
	       QString( "%1,%2,%3,%4" ).arg( led.region[0] ).arg( led.region[1] )
	           .arg( led.region[2] ).arg( led.region[3] ),
	       led.fo4csTarget ? QStringLiteral( "fo4cs" ) : QStringLiteral( "stock" ) } );

	/* 2. the five corpus hashes, exactly as the pair's headers carry them. A
	 *    bake that wrote no pair writes no hash line rather than five zeros. */
	auto hashRow = [&]( const char * name, const QString & hex ) {
		if ( !hex.isEmpty() )
			row( { QStringLiteral( "hash" ), QLatin1String( name ), hex } );
	};
	hashRow( "loadOrderHash",     led.loadOrderHashHex );
	hashRow( "pluginCorpusHash",  led.pluginCorpusHashHex );
	hashRow( "objectCorpusHash",  led.objectCorpusHashHex );
	hashRow( "modelCorpusHash",   led.modelCorpusHashHex );
	hashRow( "cardCorpusHash",    led.cardCorpusHashHex );
	//    loadOrderHash again under its v1 name, so a v1 reader's field survives.
	row( { QStringLiteral( "loadorder" ), led.loadOrder } );

	/* 3. the plugins, one a line, IN LOAD ORDER. */
	for ( const LodbPlugin & p : led.plugins )
		row( { QStringLiteral( "plugin" ), QString::number( p.index ), tsv( p.name ),
		       QString::number( p.bytes ), QString( "%1" ).arg( p.hash, 16, 16, QChar( '0' ) ),
		       tsv( p.path ) } );

	/* 4. the resource stack, in the order it was given. */
	for ( const LodbResource & r : led.resources )
		row( { QStringLiteral( "resource" ), tsv( r.kind ), tsv( r.path ),
		       QString::number( r.bytes ), tsv( r.mtimeIso ) } );

	/* 5. the switches: the argument vector VERBATIM, one token a line, which is
	 *    the way back to reproducing this bake exactly, plus the digest the
	 *    incremental path already compares. */
	for ( const QString & t : led.switchTokens )
		row( { QStringLiteral( "switch" ), tsv( t ) } );
	row( { QStringLiteral( "switches" ), led.switches } );

	/* 6. the chunks. The rows are sorted by (cy,cx) -- never by the order the
	 *    pass retired them -- which is what keeps the record deterministic. */
	QVector<LodgenLedgerEntry> sorted = led.chunks;
	std::sort( sorted.begin(), sorted.end(),
		[]( const LodgenLedgerEntry & a, const LodgenLedgerEntry & b ) {
			if ( a.cy != b.cy ) return a.cy < b.cy;
			return a.cx < b.cx;
		} );
	for ( const LodgenLedgerEntry & e : sorted ) {
		row( { QStringLiteral( "chunk" ), QString::number( e.cx ), QString::number( e.cy ),
		       QString::number( e.dim ), e.inputs } );
		for ( int i = 0; i < e.outFiles.size(); i++ )
			row( { QStringLiteral( "out" ), QString::number( e.cx ), QString::number( e.cy ),
			       tsv( e.outFiles.at( i ) ),
			       i < e.outDigests.size() ? e.outDigests.at( i ) : QString() } );
	}

	/* 7. every census line the bake printed, verbatim. */
	for ( const QString & c : led.census )
		row( { QStringLiteral( "census" ), tsv( c ) } );

	/* 8. the end line, READ BACK from the disk -- so a truncated record, or a
	 *    bake that died between two files, is detectable by a reader that has
	 *    only the folder. The record itself is excluded: it is not written yet. */
	int    nf = 0;
	qint64 nb = 0;
	lodbCountTree( QFileInfo( path ).absolutePath(), path, &nf, &nb );
	row( { QStringLiteral( "end" ), QString::number( nf ), QString::number( nb ) } );

	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
		if ( error )
			*error = QStringLiteral( "cannot write the bake record at %1" ).arg( path );
		return false;
	}
	//  LF, always, on every platform: the record is compared byte for byte by
	//  the gates and a CRLF from QTextStream's platform default would make the
	//  same bake differ from itself across machines.
	const QByteArray bytes = ( L.join( QChar( '\n' ) ) + QChar( '\n' ) ).toUtf8();
	const qint64 wrote = f.write( bytes );
	f.flush();
	f.close();
	if ( wrote != bytes.size() ) {
		if ( error )
			*error = QStringLiteral( "the bake record at %1 wrote %2 of %3 bytes" )
				.arg( path ).arg( wrote ).arg( bytes.size() );
		return false;
	}
	return true;
}


/* ---- the reader ---------------------------------------------------------- */

bool lodgenReadLedger( const QString & path, LodgenLedger * led, QString * error )
{
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		/* NAME THE DIRECTORY IT LOOKED IN (lane ARCHLOCK1, 2026-09-17, from
		 * BAKEREC1's finding). Batch mode does `QDir::setCurrent(
		 * applicationDirPath() )` on purpose -- that is how `nif.xml` is found
		 * (src/nifcli.cpp:179) -- so a RELATIVE `--bake-record scratchpad/.../
		 * x.lodb` is resolved against the NifSkope folder and refused while the
		 * file plainly exists in the shell's own working directory. The refusal
		 * said only the path it was given, which reads as "the file is missing".
		 * One clause, no behaviour change: the absolute directory that was
		 * actually searched. */
		if ( error )
			*error = QStringLiteral( "no bake record at %1 (looked in %2)" )
				.arg( path, QFileInfo( path ).absolutePath() );
		return false;
	}
	const QByteArray all = f.readAll();
	f.close();
	if ( all.size() >= 4 && all.startsWith( QByteArray( LODB_V1_MAGIC ) ) ) {
		/* The v1 binary container. Refused BY NAME rather than mis-parsed: its
		 * JSON has no plugin lines, no corpus hashes and no census, so a reader
		 * that limped on would answer questions it cannot answer. */
		if ( error )
			*error = QStringLiteral( "%1 is a version 1 BINARY bake record and this build writes "
				"version 2 (plain text). Re-bake once without --incremental; every bake writes it" )
				.arg( path );
		return false;
	}
	const QStringList L = QString::fromUtf8( all ).split( QChar( '\n' ) );
	bool sawVersion = false;
	led->chunks.clear();
	led->plugins.clear();
	led->resources.clear();
	led->switchTokens.clear();
	led->census.clear();
	QHash<QString, int> chunkAt;

	for ( const QString & rawLine : L ) {
		const QString line = rawLine.endsWith( QChar( '\r' ) ) ? rawLine.left( rawLine.size() - 1 ) : rawLine;
		if ( line.isEmpty() )
			continue;
		const QStringList f = line.split( QChar( '\t' ) );
		const QString k = f.at( 0 );
		if ( k == QLatin1String( "lodb" ) ) {
			if ( f.size() < 3 ) {
				if ( error )
					*error = QStringLiteral( "%1 has a short version line" ).arg( path );
				return false;
			}
			const int v = f.at( 1 ).toInt();
			if ( v != LODB_TEXT_VERSION ) {
				if ( error )
					*error = QStringLiteral( "%1 is bake record version %2 and this build writes %3 -- "
						"re-bake once without --incremental" ).arg( path ).arg( v ).arg( LODB_TEXT_VERSION );
				return false;
			}
			sawVersion = true;
			led->worldEdid = f.at( 2 );
			if ( f.size() > 3 ) led->exeStamp = f.at( 3 );
			if ( f.size() > 4 ) led->exeBytes = f.at( 4 ).toLongLong();
		} else if ( k == QLatin1String( "baked" ) && f.size() > 1 ) {
			led->bakedUtc = f.at( 1 );
		} else if ( k == QLatin1String( "shape" ) && f.size() > 4 ) {
			led->worldspace = quint32( f.at( 1 ).toUInt() );
			led->dim        = f.at( 2 ).toInt();
			const QStringList r = f.at( 3 ).split( QChar( ',' ) );
			for ( int i = 0; i < 4 && i < r.size(); i++ )
				led->region[i] = r.at( i ).toInt();
			led->fo4csTarget = ( f.at( 4 ) == QLatin1String( "fo4cs" ) );
		} else if ( k == QLatin1String( "hash" ) && f.size() > 2 ) {
			const QString n = f.at( 1 );
			if ( n == QLatin1String( "loadOrderHash" ) )         led->loadOrderHashHex    = f.at( 2 );
			else if ( n == QLatin1String( "pluginCorpusHash" ) ) led->pluginCorpusHashHex = f.at( 2 );
			else if ( n == QLatin1String( "objectCorpusHash" ) ) led->objectCorpusHashHex = f.at( 2 );
			else if ( n == QLatin1String( "modelCorpusHash" ) )  led->modelCorpusHashHex  = f.at( 2 );
			else if ( n == QLatin1String( "cardCorpusHash" ) )   led->cardCorpusHashHex   = f.at( 2 );
			//  an unknown hash NAME is ignored, like an unknown line kind
		} else if ( k == QLatin1String( "loadorder" ) && f.size() > 1 ) {
			led->loadOrder = f.at( 1 );
		} else if ( k == QLatin1String( "plugin" ) && f.size() > 4 ) {
			LodbPlugin p;
			p.index = f.at( 1 ).toInt();
			p.name  = f.at( 2 );
			p.bytes = f.at( 3 ).toLongLong();
			p.hash  = f.at( 4 ).toULongLong( nullptr, 16 );
			if ( f.size() > 5 ) p.path = f.at( 5 );
			led->plugins.append( p );
		} else if ( k == QLatin1String( "resource" ) && f.size() > 2 ) {
			LodbResource r;
			r.kind = f.at( 1 );
			r.path = f.at( 2 );
			if ( f.size() > 3 ) r.bytes = f.at( 3 ).toLongLong();
			if ( f.size() > 4 ) r.mtimeIso = f.at( 4 );
			led->resources.append( r );
		} else if ( k == QLatin1String( "switch" ) && f.size() > 1 ) {
			led->switchTokens.append( f.at( 1 ) );
		} else if ( k == QLatin1String( "switches" ) && f.size() > 1 ) {
			led->switches = f.at( 1 );
		} else if ( k == QLatin1String( "chunk" ) && f.size() > 4 ) {
			LodgenLedgerEntry e;
			e.cx = f.at( 1 ).toInt();
			e.cy = f.at( 2 ).toInt();
			e.dim = f.at( 3 ).toInt();
			e.inputs = f.at( 4 );
			chunkAt.insert( QString( "%1,%2" ).arg( e.cx ).arg( e.cy ), led->chunks.size() );
			led->chunks.append( e );
		} else if ( k == QLatin1String( "out" ) && f.size() > 4 ) {
			const int at = chunkAt.value( QString( "%1,%2" ).arg( f.at( 1 ), f.at( 2 ) ), -1 );
			if ( at >= 0 ) {
				led->chunks[at].outFiles.append( f.at( 3 ) );
				led->chunks[at].outDigests.append( f.at( 4 ) );
			}
		} else if ( k == QLatin1String( "census" ) && f.size() > 1 ) {
			led->census.append( f.at( 1 ) );
		} else if ( k == QLatin1String( "end" ) && f.size() > 2 ) {
			led->endFiles = f.at( 1 ).toInt();
			led->endBytes = f.at( 2 ).toLongLong();
		}
		/* anything else: IGNORED ON PURPOSE. A later lane adds a line kind and
		 * this reader keeps working -- that is the forward-compatibility half
		 * of the contract, and the version line above is the other half. */
	}
	if ( !sawVersion ) {
		if ( error )
			*error = QStringLiteral( "%1 has no `lodb` version line -- it is not a bake record" ).arg( path );
		return false;
	}
	return true;
}


bool lodbReadPlugins( const QString & path, QVector<LodbPlugin> * out, QString * error )
{
	LodgenLedger led;
	if ( !lodgenReadLedger( path, &led, error ) )
		return false;
	if ( out )
		*out = led.plugins;
	return true;
}


QString lodbPathBeside( const QString & lodoPath )
{
	const QFileInfo fi( lodoPath );
	const QString p = fi.absolutePath() + QChar( '/' ) + fi.completeBaseName() + QStringLiteral( ".lodb" );
	return QFileInfo( p ).isFile() ? p : QString();
}
