/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "nifparsestress.h"

#include "model/nifmodel.h"

#include <QBuffer>
#include <QByteArray>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QThread>

#include <atomic>
#include <vector>

/* See nifparsestress.h for why this exists. In one line: a bake crash cannot
 * tell the parser apart from the plugin reader, the texture cache, the archive
 * layer and the message sink, and this can. */

namespace {

//! FNV-1a, 64 bit. Order-sensitive on purpose: the walk order IS part of what
//! is under test, so a tree that comes back correct but differently shaped
//! fails.
inline void digestBytes( quint64 & h, const char * p, int n )
{
	for ( int i = 0; i < n; i++ ) {
		h ^= quint64( (unsigned char) p[i] );
		h *= Q_UINT64_C( 0x100000001b3 );
	}
}

inline void digestString( quint64 & h, const QString & s )
{
	const QByteArray b = s.toUtf8();
	digestBytes( h, b.constData(), b.size() );
	h ^= quint64( b.size() ) + 1;
	h *= Q_UINT64_C( 0x100000001b3 );
}

inline void digestInt( quint64 & h, qint64 v )
{
	for ( int i = 0; i < 8; i++ ) {
		h ^= quint64( ( v >> ( i * 8 ) ) & 0xFF );
		h *= Q_UINT64_C( 0x100000001b3 );
	}
}

/*! The item cap. A 38k-vertex shape is ~1.5M items and hashing every one of
 *  them turns a two-second gate into a two-minute one. The cap is a COUNT, not
 *  a depth, and the walk order is fixed, so the same cap always digests the
 *  same items -- a truncated digest is still a deterministic function of the
 *  document. The count that was actually reached is reported beside the digest,
 *  so "it only hashed three items" cannot hide behind a green line. */
constexpr int kItemCap = 200000;

/*! Walk the document the way the LOD generator reads it -- through
 *  `index`/`rowCount`, `itemName`, `itemStrType` and `getValue` -- NOT through
 *  `data( …, Qt::DisplayRole )`.
 *
 *  That is deliberate. `data()` is the EDITOR's path and it reaches
 *  `nifmodel.cpp`'s array-pseudonym tables, which a separate defect writes into
 *  (BAKEPERF1 R3). Mixing the two would mean a red here could be either fault
 *  and the harness would not be a discriminator. The editor path gets its own
 *  switch below. */
void digestIndex( const NifModel & nif, const QModelIndex & idx, quint64 & h, int & budget, bool viewPath )
{
	if ( budget <= 0 )
		return;

	if ( idx.isValid() ) {
		budget--;
		digestString( h, nif.itemName( idx ) );
		digestString( h, nif.itemStrType( idx ) );
		digestString( h, nif.getValue( idx ).toString() );
		if ( viewPath ) {
			// the editor's own read, statics and all -- only when asked for
			digestString( h, nif.data( idx.sibling( idx.row(), 0 ), Qt::DisplayRole ).toString() );
			digestString( h, nif.data( idx, Qt::DisplayRole ).toString() );
		}
	}

	const int rows = nif.rowCount( idx );
	digestInt( h, rows );
	for ( int r = 0; r < rows && budget > 0; r++ )
		digestIndex( nif, nif.index( r, 0, idx ), h, budget, viewPath );
}

struct FileFixture
{
	QString name;
	QByteArray bytes;
	quint64 refDigest = 0;
	int refItems = 0;
	int refBlocks = 0;
	bool refLoaded = false;
};

/*! One load, start to finish, exactly as `lodgenLoadModel` does it: a model on
 *  the stack, bytes through a `QBuffer`, `resetState()` after `load()` (without
 *  it the document answers as it does mid-load and every index lookup comes
 *  back empty), then the walk, then destruction. The destructor is where the
 *  fault was, so it is inside the measured region. */
quint64 loadAndDigest( const QByteArray & bytes, const QString & name,
	int * items, int * blocks, bool * loaded, bool viewPath )
{
	quint64 h = Q_UINT64_C( 0xcbf29ce484222325 );
	NifModel nif;
	QByteArray copy( bytes );	// each worker parses its OWN buffer
	QBuffer dev( &copy );
	const bool ok = dev.open( QIODevice::ReadOnly )
		&& nif.load( dev, name.toLocal8Bit().constData() );
	nif.resetState();
	if ( loaded )
		*loaded = ok;
	digestInt( h, ok ? 1 : 0 );
	const int nb = nif.getBlockCount();
	digestInt( h, nb );
	if ( blocks )
		*blocks = nb;
	int budget = kItemCap;
	for ( int b = 0; b < nb && budget > 0; b++ )
		digestIndex( nif, nif.getBlockIndex( b ), h, budget, viewPath );
	if ( items )
		*items = kItemCap - budget;
	return h;
}

class StressThread final : public QThread
{
public:
	StressThread( std::vector<FileFixture> * f, int reps, int id, bool viewPath, NifModel * shared )
		: fixtures( f ), reps( reps ), id( id ), viewPath( viewPath ), shared( shared ) {}

	int mismatches = 0;
	int loads = 0;
	QString firstBad;

protected:
	void run() override
	{
		for ( int r = 0; r < reps; r++ ) {
			for ( size_t i = 0; i < fixtures->size(); i++ ) {
				FileFixture & f = (*fixtures)[i];
				QByteArray bytes = f.bytes;
				/* SABOTAGE "digest": worker 0, first rep, first file, one byte
				 * flipped. The digest MUST come back different -- that is what
				 * proves it reaches the parse instead of hashing a constant. */
				if ( sabotageByte && id == 0 && r == 0 && i == 0 && bytes.size() > 256 )
					bytes[200] = char( bytes[200] ^ 0x5A );
				quint64 h;
				if ( shared ) {
					/* SABOTAGE "share": every worker on ONE model. The layer is
					 * not allowed to survive this; it is the floor under
					 * "N threads clean". */
					QBuffer dev( &bytes );
					if ( dev.open( QIODevice::ReadOnly ) )
						shared->load( dev, f.name.toLocal8Bit().constData() );
					shared->resetState();
					h = quint64( shared->getBlockCount() );
				} else {
					h = loadAndDigest( bytes, f.name, nullptr, nullptr, nullptr, viewPath );
				}
				loads++;
				if ( h != f.refDigest ) {
					mismatches++;
					if ( firstBad.isEmpty() )
						firstBad = QStringLiteral( "%1 (rep %2, worker %3): got %4, reference %5" )
							.arg( f.name ).arg( r ).arg( id )
							.arg( h, 16, 16, QLatin1Char( '0' ) )
							.arg( f.refDigest, 16, 16, QLatin1Char( '0' ) );
				}
			}
		}
	}

public:
	bool sabotageByte = false;

private:
	std::vector<FileFixture> * fixtures;
	int reps;
	int id;
	bool viewPath;
	NifModel * shared;
};

}	// namespace

int nifParseStressRun( const NifParseStressOptions & opts, QTextStream & out )
{
	int failures = 0;
	const bool viewPath = false;	// the editor path is a separate lane's gate

	if ( opts.paths.isEmpty() ) {
		out << "FAIL no input files - a stress run over nothing passes trivially" << Qt::endl;
		out << "FAIL" << Qt::endl;
		return 1;
	}

	// ---- read every file ONCE, on this thread ----------------------------
	std::vector<FileFixture> fixtures;
	for ( const QString & p : opts.paths ) {
		QFile f( p );
		if ( !f.open( QIODevice::ReadOnly ) ) {
			out << "FAIL cannot read " << p << Qt::endl;
			failures++;
			continue;
		}
		FileFixture fx;
		fx.name = p;
		fx.bytes = f.readAll();
		f.close();
		if ( fx.bytes.size() < 64 ) {
			out << "FAIL " << p << " is " << fx.bytes.size() << " bytes - not a NIF" << Qt::endl;
			failures++;
			continue;
		}
		fixtures.push_back( fx );
	}
	if ( fixtures.empty() ) {
		out << "FAIL nothing to parse" << Qt::endl;
		out << "FAIL" << Qt::endl;
		return failures + 1;
	}

	// ---- the single-threaded reference -----------------------------------
	out << "== reference pass, one thread ==" << Qt::endl;
	for ( FileFixture & fx : fixtures ) {
		fx.refDigest = loadAndDigest( fx.bytes, fx.name, &fx.refItems, &fx.refBlocks, &fx.refLoaded, viewPath );
		out << QStringLiteral( "  %1  loaded=%2 blocks=%3 items=%4 digest=%5" )
			.arg( QFileInfo( fx.name ).fileName(), -40 )
			.arg( fx.refLoaded ? QStringLiteral( "yes" ) : QStringLiteral( "NO" ) )
			.arg( fx.refBlocks ).arg( fx.refItems )
			.arg( fx.refDigest, 16, 16, QLatin1Char( '0' ) ) << Qt::endl;
		/* THE FLOOR ON THE FIXTURES THEMSELVES. A file that did not load, or a
		 * walk that reached one item, would make every later comparison pass
		 * over nothing. Both are named as failures here, not silently allowed. */
		if ( !fx.refLoaded ) {
			out << "FAIL fixture did not load: " << fx.name << Qt::endl;
			failures++;
		}
		if ( fx.refItems < 32 ) {
			out << "FAIL fixture digested only " << fx.refItems << " items: " << fx.name << Qt::endl;
			failures++;
		}
	}

	// twice on one thread: the reference must be reproducible before N threads
	// are asked to reproduce it
	for ( FileFixture & fx : fixtures ) {
		const quint64 again = loadAndDigest( fx.bytes, fx.name, nullptr, nullptr, nullptr, viewPath );
		if ( again != fx.refDigest ) {
			out << "FAIL the digest is not deterministic on ONE thread: " << fx.name << Qt::endl;
			failures++;
		}
	}

	// ---- the parallel pass -----------------------------------------------
	const int nThreads = qMax( 1, opts.threads );
	const int reps = qMax( 1, opts.reps );
	NifModel * shared = nullptr;
	if ( opts.sabotage == QLatin1String( "share" ) ) {
		shared = new NifModel();
		out << "SABOTAGE share: every worker on ONE NifModel - expected to go red" << Qt::endl;
	}
	if ( opts.sabotage == QLatin1String( "digest" ) )
		out << "SABOTAGE digest: one byte flipped for worker 0 - expected to go red" << Qt::endl;

	out << "== parallel pass: " << nThreads << " threads x " << reps << " reps x "
		<< int( fixtures.size() ) << " files ==" << Qt::endl;

	QElapsedTimer timer;
	timer.start();
	std::vector<StressThread *> workers;
	workers.reserve( size_t( nThreads ) );
	for ( int i = 0; i < nThreads; i++ ) {
		StressThread * t = new StressThread( &fixtures, reps, i, viewPath, shared );
		t->sabotageByte = ( opts.sabotage == QLatin1String( "digest" ) );
		workers.push_back( t );
	}
	for ( StressThread * t : workers )
		t->start();
	for ( StressThread * t : workers )
		t->wait();
	const qint64 ms = timer.elapsed();

	int mismatches = 0, loads = 0;
	QString firstBad;
	for ( StressThread * t : workers ) {
		mismatches += t->mismatches;
		loads += t->loads;
		if ( firstBad.isEmpty() )
			firstBad = t->firstBad;
		delete t;
	}
	delete shared;

	out << "  loads " << loads << ", mismatches " << mismatches
		<< ", wall " << ms << " ms" << Qt::endl;
	if ( !firstBad.isEmpty() )
		out << "  first mismatch: " << firstBad << Qt::endl;

	/* THE FLOOR ON THE PARALLEL PASS. A run that performed no loads passes the
	 * mismatch test trivially; say so as a failure. */
	const int expected = nThreads * reps * int( fixtures.size() );
	if ( loads != expected ) {
		out << "FAIL expected " << expected << " loads, counted " << loads << Qt::endl;
		failures++;
	}

	if ( opts.sabotage.isEmpty() ) {
		if ( mismatches )
			failures += mismatches;
	} else {
		// the floor: the sabotaged run MUST go red, or the check cannot fail
		if ( !mismatches ) {
			out << "FAIL sabotage '" << opts.sabotage
				<< "' produced no mismatch - this check cannot go red" << Qt::endl;
			failures++;
		} else {
			out << "  floor fired as it must: " << mismatches << " mismatch(es)" << Qt::endl;
			mismatches = 0;
		}
	}

	out << ( failures ? QStringLiteral( "FAIL" ) : QStringLiteral( "PASS" ) ) << Qt::endl;
	return failures;
}
