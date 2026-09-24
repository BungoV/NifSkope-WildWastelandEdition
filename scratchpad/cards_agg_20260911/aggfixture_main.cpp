/* Lane CARDS-AGG -- the STANDALONE gate for the .lodi v4 aggregate layout
 * (ww-standalone-writer-gate).
 *
 * Links src/lodifile.cpp, src/lodofile.cpp and src/io/lodvfile.cpp against
 * Qt6Core alone, builds a hand-written known-answer set, reads it back with
 * every check on, writes it twice for byte identity, and then MUTATES one byte
 * at a time -- re-signing the CRCs that cover the byte, so the ROW RULE is what
 * refuses and not the checksum -- and requires each refusal to NAME its rule.
 *
 * Modes:
 *   <dir>                       write Agg.lodi and Agg.expect.txt
 *   --verify <dir>              read back, print every check
 *   --mutate <in> <out> <off> <xor> [--resign-header|--resign-all]
 */

#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QString>
#include <QTextStream>

#include <cmath>
#include <cstring>

#include "lodifile.h"
#include "lodofile.h"

extern quint32 lodvCrc32( const unsigned char * p, qsizetype n, quint32 seed );

static QTextStream & out()
{
	static QTextStream s( stdout );
	return s;
}

static int gChecks = 0, gFails = 0;
static void check( bool ok, const QString & what )
{
	gChecks++;
	if ( !ok ) {
		gFails++;
		out() << "FAIL " << what << Qt::endl;
	} else {
		out() << "ok   " << what << Qt::endl;
	}
}

/* THE FIXTURE. Six instances in ONE chunk, spread over two cells, so that:
 *  - cell (1,1) holds four trees and is the aggregate's cell;
 *  - cell (2,1) holds two, which the second aggregate stands for;
 *  - an in-cell order rule has something to be violated by;
 *  - a covered-instance-in-the-wrong-cell mutation has a wrong cell to name.
 * Every number below is written down BEFORE the run, in Agg.expect.txt. */
static bool writeFixture( const QString & dir, QString * err )
{
	LodiSrcSet set;
	set.worldspaceEdid = QStringLiteral( "AggTest" );
	set.flags = LODI_FLAG_ROW_ORDER_NORTH_UP | LODI_FLAG_NOLIB;
	set.lodoIdentity = 0;
	set.pluginCorpusHash = 0x1122334455667788ULL;
	set.objectCorpusHash = 0x99AABBCCDDEEFF00ULL;
	set.loadOrderHash = 0x0102030405060708ULL;

	struct Seed { float x, y, z; quint32 ref; };
	// cell (1,1) spans x,y in [4096, 8192); cell (2,1) spans x in [8192, 12288)
	const Seed seeds[6] = {
		{ 4200.0f, 4300.0f, 100.0f, 0x00000101U },
		{ 5200.0f, 4400.0f, 110.0f, 0x00000102U },
		{ 6200.0f, 5400.0f, 120.0f, 0x00000103U },
		{ 7200.0f, 6400.0f, 130.0f, 0x00000104U },
		{ 8300.0f, 4500.0f, 140.0f, 0x00000105U },
		{ 9300.0f, 5500.0f, 150.0f, 0x00000106U },
	};
	for ( int i = 0; i < 6; i++ ) {
		LodiSrcInstance r;
		r.pos[0] = seeds[i].x; r.pos[1] = seeds[i].y; r.pos[2] = seeds[i].z;
		r.scale = 1.0f;
		r.baseId = quint32( i % 2 );
		r.refFormId = seeds[i].ref;
		r.scolPart = -1;
		r.drawKey = quint16( i % 2 );
		r.identity = quint16( i );
		r.boundRadius = 512.0f;
		r.baseName = QString( "fixture base %1" ).arg( i % 2 );
		set.instances.push_back( r );
	}

	/* The two aggregates. `covered` names SOURCE indices; the writer remaps
	 * them to written indices, which is the whole point of the field. */
	LodiSrcAggregate a1;
	a1.centre[0] = 6144.0f; a1.centre[1] = 6144.0f; a1.centre[2] = 400.0f;
	a1.half[0] = 3000.0f; a1.half[1] = 1000.0f;
	a1.depthSpan = 9000.0f;
	a1.boundRadius = 3200.0f;
	a1.cellX = 1; a1.cellY = 1;
	a1.views = 8;
	a1.flags = LODI_AGG_HEIGHT | LODI_AGG_MIRRORED;
	a1.covered = { 0, 1, 2, 3 };
	LodiSrcAggregate a2 = a1;
	a2.centre[0] = 10240.0f;
	a2.cellX = 2; a2.cellY = 1;
	a2.flags = LODI_AGG_HEIGHT;
	a2.covered = { 4, 5 };
	set.aggregates.push_back( a2 );      // deliberately out of cell order
	set.aggregates.push_back( a1 );      // the writer must sort them

	LodiHeader h;
	LodiWriteStats st;
	const QString path = dir + QStringLiteral( "/Agg.lodi" );
	if ( !lodiWrite( path, set, &h, &st, err ) )
		return false;

	QFile e( dir + QStringLiteral( "/Agg.expect.txt" ) );
	if ( !e.open( QIODevice::WriteOnly | QIODevice::Text ) )
		return false;
	QTextStream es( &e );
	es << "expect version 4\n";
	es << "expect instanceCount 6\n";
	es << "expect aggregateCount 2\n";
	es << "expect coveredCount 6\n";
	es << "expect aggregateViews 8\n";
	es << "expect aggregateStride 48\n";
	es << "expect agg0.cell 1 1\n";
	es << "expect agg0.identity 80000000\n";
	es << "expect agg0.coveredCount 4\n";
	es << "expect agg1.cell 2 1\n";
	es << "expect agg1.identity 80000001\n";
	es << "expect agg1.coveredCount 2\n";
	es << "expect agg0.flags 3\n";
	es << "expect agg1.flags 1\n";
	e.close();
	out() << "wrote " << path << " " << h.fileBytes << " bytes, version " << st.version
		  << ", aggregates " << st.aggregates << ", covered " << st.coveredInstances << Qt::endl;
	return true;
}

static bool verify( const QString & dir )
{
	LodiHeader h;
	LodiTable t;
	QString err;
	const QString path = dir + QStringLiteral( "/Agg.lodi" );
	if ( !lodiRead( path, &h, &t, true, &err ) ) {
		out() << "FAIL read: " << err << Qt::endl;
		gChecks++; gFails++;
		return false;
	}
	check( h.version == 4, QString( "version is 4 (%1)" ).arg( h.version ) );
	check( h.instanceCount == 6, QString( "instanceCount 6 (%1)" ).arg( h.instanceCount ) );
	check( h.aggregateCount == 2, QString( "aggregateCount 2 (%1)" ).arg( h.aggregateCount ) );
	check( h.coveredCount == 6, QString( "coveredCount 6 (%1)" ).arg( h.coveredCount ) );
	check( h.aggregateStride == 48, QString( "aggregateStride 48 (%1)" ).arg( h.aggregateStride ) );
	check( h.aggregateViews == 8, QString( "aggregateViews 8 (%1)" ).arg( h.aggregateViews ) );
	check( std::fabs( double( h.aggSwitchPx ) - 96.0 ) < 1e-6, QString( "aggSwitchPx 96 (%1)" ).arg( double( h.aggSwitchPx ) ) );
	check( std::fabs( double( h.aggBandRatio ) - 1.2 ) < 1e-6, QString( "aggBandRatio 1.2 (%1)" ).arg( double( h.aggBandRatio ) ) );
	check( t.aggregates.size() == 2, QStringLiteral( "two aggregate rows" ) );
	if ( t.aggregates.size() == 2 ) {
		check( t.aggregates[0].cellX == 1 && t.aggregates[0].cellY == 1,
			QString( "row 0 is cell (1,1) -- the writer SORTED them, they went in as (2,1) first (got %1,%2)" )
				.arg( t.aggregates[0].cellX ).arg( t.aggregates[0].cellY ) );
		check( t.aggregates[1].cellX == 2 && t.aggregates[1].cellY == 1, QStringLiteral( "row 1 is cell (2,1)" ) );
		check( t.aggregates[0].identity == 0x80000000U,
			QString( "row 0 identity 0x80000000 (0x%1)" ).arg( t.aggregates[0].identity, 8, 16, QChar( '0' ) ) );
		check( t.aggregates[1].identity == 0x80000001U,
			QString( "row 1 identity 0x80000001 (0x%1)" ).arg( t.aggregates[1].identity, 8, 16, QChar( '0' ) ) );
		check( t.aggregates[0].coveredCount == 4, QString( "row 0 covers 4 (%1)" ).arg( t.aggregates[0].coveredCount ) );
		check( t.aggregates[1].coveredCount == 2, QString( "row 1 covers 2 (%1)" ).arg( t.aggregates[1].coveredCount ) );
		check( t.aggregates[0].flags == ( LODI_AGG_HEIGHT | LODI_AGG_MIRRORED ),
			QString( "row 0 flags HEIGHT|MIRRORED (%1)" ).arg( t.aggregates[0].flags ) );
		check( t.aggregates[1].flags == LODI_AGG_HEIGHT, QString( "row 1 flags HEIGHT (%1)" ).arg( t.aggregates[1].flags ) );
		check( t.aggregates[0].coveredFirst == 0 && t.aggregates[1].coveredFirst == 4,
			QStringLiteral( "the two covered ranges partition the blob in order" ) );
		bool asc = true;
		for ( size_t i = 1; i < t.covered.size(); i++ )
			if ( i != 4 && t.covered[i] <= t.covered[i - 1] )
				asc = false;
		check( asc, QStringLiteral( "every aggregate's covered list is ascending" ) );
		check( t.covered.size() == 6, QStringLiteral( "the covered blob holds six entries" ) );
		/* THE COUNT IDENTITY, from the bytes: the two rows' coveredCount must
		 * sum to the header's coveredCount and to the blob's length. */
		check( t.aggregates[0].coveredCount + t.aggregates[1].coveredCount == h.coveredCount
			&& h.coveredCount == quint32( t.covered.size() ),
			QStringLiteral( "count identity: the rows, the header and the blob agree" ) );
	}
	return gFails == 0;
}

int main( int argc, char ** argv )
{
	QCoreApplication app( argc, argv );
	QStringList a;
	for ( int i = 1; i < argc; i++ )
		a << QString::fromLocal8Bit( argv[i] );
	if ( a.isEmpty() ) {
		out() << "usage: aggfixture <dir> | --verify <dir> | --mutate <in> <out> <off> <xor> [--resign-header|--resign-all]" << Qt::endl;
		out().flush();
		return 2;
	}
	if ( a[0] == QLatin1String( "--verify" ) ) {
		const bool ok = verify( a.value( 1 ) );
		out() << gChecks << " checks, " << gFails << " failures, " << ( ok ? "PASS" : "FAIL" ) << Qt::endl;
		out().flush();
		return ok ? 0 : 1;
	}
	if ( a[0] == QLatin1String( "--mutate" ) ) {
		QFile in( a.value( 1 ) );
		if ( !in.open( QIODevice::ReadOnly ) )
			return 2;
		QByteArray b = in.readAll();
		in.close();
		const int off = a.value( 3 ).toInt();
		const int x = a.value( 4 ).toInt( nullptr, 0 );
		if ( off < 0 || off >= b.size() )
			return 2;
		b[off] = char( quint8( b[off] ) ^ quint8( x ) );
		const bool resignAll = a.contains( QStringLiteral( "--resign-all" ) );
		const bool resignHeader = resignAll || a.contains( QStringLiteral( "--resign-header" ) );
		unsigned char * p = reinterpret_cast<unsigned char *>( b.data() );
		auto getLE64 = [p]( int o ) {
			quint64 v = 0;
			for ( int i = 0; i < 8; i++ )
				v |= quint64( p[o + i] ) << ( 8 * i );
			return v;
		};
		auto getLE32 = [p]( int o ) {
			quint32 v = 0;
			for ( int i = 0; i < 4; i++ )
				v |= quint32( p[o + i] ) << ( 8 * i );
			return v;
		};
		auto putLE32 = [&b]( int o, quint32 v ) {
			for ( int i = 0; i < 4; i++ )
				b[o + i] = char( ( v >> ( 8 * i ) ) & 0xFF );
		};
		if ( resignAll ) {
			/* Re-sign indexCrc32 over the five payloads the contract names, in
			 * file order: chunk table, cell ranges, occluders, occluder ranges,
			 * aggregates, covered. Then the affected chunk's own crc32 is left
			 * alone deliberately -- a mutation inside the instance blob should
			 * be answered by THAT, and a case that wants the row rule mutates
			 * a table the chunk crc does not cover. */
			const quint32 chunkCount = getLE32( 0x54 );
			const quint32 present = getLE32( 0x5C );
			const quint32 aggCount = getLE32( 0xC0 );
			const quint32 covCount = getLE32( 0xC4 );
			const quint64 offChunks = getLE64( 0x68 ), offCells = getLE64( 0x70 );
			const quint64 offOcc = getLE64( 0x98 ), offOccR = getLE64( 0xA0 );
			const quint64 offAgg = getLE64( 0xB0 ), offCov = getLE64( 0xB8 );
			const quint32 occCount = getLE32( 0xA8 );
			quint32 crc = lodvCrc32( p + offChunks, qsizetype( chunkCount * 32 ), 0 );
			crc = lodvCrc32( p + offCells, qsizetype( quint64( present ) * 16 * 8 ), crc );
			crc = lodvCrc32( p + offOcc, qsizetype( quint64( occCount ) * 40 ), crc );
			crc = lodvCrc32( p + offOccR, qsizetype( quint64( present ) * 16 * 8 ), crc );
			crc = lodvCrc32( p + offAgg, qsizetype( quint64( aggCount ) * 48 ), crc );
			crc = lodvCrc32( p + offCov, qsizetype( quint64( covCount ) * 4 ), crc );
			putLE32( 0x64, crc );
		}
		if ( resignHeader ) {
			const quint32 hcrc = lodvCrc32( p + 0x10, 256 - 0x10, 0 );
			putLE32( 0x0C, hcrc );
		}
		QFile o( a.value( 2 ) );
		if ( !o.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
			return 2;
		o.write( b );
		o.close();
		return 0;
	}
	QString err;
	if ( !writeFixture( a[0], &err ) ) {
		out() << "FAIL write: " << err << Qt::endl;
		out().flush();
		return 1;
	}
	out().flush();
	return 0;
}
