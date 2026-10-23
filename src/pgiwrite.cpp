/*! Lane PGI0: the `.pgi` writer (see pgiwrite.h) and its two CLI verbs. */

#include "pgiwrite.h"
#include "pgi_format.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{

constexpr uint32_t kToolVersion = 1;   // lane PGI0
constexpr uint32_t kTbkMagic = 0x314B4254u;   // 'TBK1'

// the `.tbk` v5 header (src/probebake.cpp TbkHeader), read only here
struct TbkHead
{
	uint32_t magic, version, recordKind;
	int32_t cellX, cellY;
	float surfelCell;
	uint32_t surfelCount, probeCount, linkCount, flags;
	uint32_t reserved[6];
};
static_assert( sizeof( TbkHead ) == 64, "tbk head" );

struct Chunk
{
	uint32_t id;
	uint16_t group;
	QByteArray data;
};

constexpr double kPi = 3.14159265358979323846;

/*! The self-plane horizon of a surfel side (HORZ source 1): per azimuth, the elevation of the surfel's own plane.
 *  Up-facing: atan(-t / nz) with t the normal's component along the azimuth; a vertical side: -90 where it faces
 *  the azimuth, +90 behind; a down-facing side: +90 (no sky above it). tests/spells/pgi_read.py computes the same.
 *  |t| below 1e-6 counts as 0: cos/sin at 90/180/270 deg are not exact, and the sign of that residue would
 *  otherwise pick 127 or 128 (and -90 or +90 for a vertical side) differently per maths library. An int16 normal
 *  never gives a real |t| that small (the least nonzero is about 3e-5). */
void selfPlane( const int16_t n[3], int azimuths, uint8_t * elev, uint8_t * open )
{
	for ( int k = 0; k < azimuths; k++ ) {
		const double a = double( k ) * ( 2.0 * kPi / double( azimuths ) );
		double t = double( n[0] ) * std::cos( a ) + double( n[1] ) * std::sin( a );
		if ( std::fabs( t ) < 1e-6 )
			t = 0.0;
		double deg;
		if ( n[2] > 0 )
			deg = std::atan( -t / double( n[2] ) ) * ( 180.0 / kPi );
		else if ( n[2] < 0 )
			deg = 90.0;
		else
			deg = t > 0.0 ? -90.0 : 90.0;
		const double v = std::floor( ( deg + 90.0 ) * ( 255.0 / 180.0 ) + 0.5 );
		elev[k] = uint8_t( std::min( 255.0, std::max( 0.0, v ) ) );
		open[k] = 255;
	}
}

struct Sector
{
	TbkHead h;
	QByteArray bytes;
	QString path;
};

bool readTbk( const QString & path, Sector & s, QString & err )
{
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		err = QStringLiteral( "cannot read %1" ).arg( path );
		return false;
	}
	s.bytes = f.readAll();
	s.path = path;
	if ( s.bytes.size() < 64 ) {
		err = QStringLiteral( "%1: shorter than a header" ).arg( path );
		return false;
	}
	std::memcpy( &s.h, s.bytes.constData(), 64 );
	const TbkHead & h = s.h;
	if ( h.magic != kTbkMagic || h.version != 5 || h.recordKind != 1 ) {
		err = QStringLiteral( "%1: not a .tbk v5 (magic %2 version %3 kind %4); the .pgi needs v5" )
			.arg( path ).arg( h.magic, 8, 16, QLatin1Char( '0' ) ).arg( h.version ).arg( h.recordKind );
		return false;
	}
	if ( h.reserved[0] != 0 ) {
		err = QStringLiteral( "%1: v5 with a back list (%2)" ).arg( path ).arg( h.reserved[0] );
		return false;
	}
	const qint64 need = 64 + 32 * qint64( h.surfelCount ) + 144 * qint64( h.probeCount ) + 12 * qint64( h.linkCount )
		+ 8 * qint64( h.linkCount ) + 32 * qint64( h.probeCount ) + 32 * qint64( h.reserved[1] ) + 16 * qint64( h.reserved[3] );
	if ( need != s.bytes.size() ) {
		err = QStringLiteral( "%1: size %2, the counts say %3" ).arg( path ).arg( s.bytes.size() ).arg( need );
		return false;
	}
	// lane PGIBIT32: side ids are 0..5, or 0..23 (bin + 6 x slot) when the bake split cells by room (content 32)
	const unsigned sides = ( h.reserved[2] & pgi::kContentRoomSplit ) ? 6u * pgi::kRoomSlots : 6u;
	const char * b = s.bytes.constData();
	const qint64 oLx = 64 + 32 * qint64( h.surfelCount ) + 144 * qint64( h.probeCount ) + 12 * qint64( h.linkCount );
	for ( uint32_t i = 0; i < h.surfelCount; i++ )
		if ( uint8_t( b[64 + 32 * qint64( i ) + offsetof( pgi::Surfel, bin )] ) >= sides ) {
			err = QStringLiteral( "%1: surfel %2 side %3, the content flags (%4) allow %5" ).arg( path ).arg( i )
				.arg( uint8_t( b[64 + 32 * qint64( i ) + offsetof( pgi::Surfel, bin )] ) ).arg( h.reserved[2] ).arg( sides );
			return false;
		}
	for ( uint32_t i = 0; i < h.linkCount; i++ )
		if ( uint8_t( b[oLx + 8 * qint64( i )] ) >= sides ) {
			err = QStringLiteral( "%1: link %2 side %3, the content flags (%4) allow %5" ).arg( path ).arg( i )
				.arg( uint8_t( b[oLx + 8 * qint64( i )] ) ).arg( h.reserved[2] ).arg( sides );
			return false;
		}
	return true;
}

QByteArray raw( const void * p, size_t n )
{
	return QByteArray( static_cast<const char *>( p ), int( n ) );
}

void groupChunks( const Sector & s, uint16_t g, const PgiWriteSpec & spec, std::vector<Chunk> & out, PgiWriteResult & R )
{
	const TbkHead & h = s.h;
	const char * b = s.bytes.constData();
	const qint64 oS = 64, oP = oS + 32 * qint64( h.surfelCount ), oL = oP + 144 * qint64( h.probeCount );
	const qint64 oLx = oL + 12 * qint64( h.linkCount ), oPx = oLx + 8 * qint64( h.linkCount );
	const qint64 oB = oPx + 32 * qint64( h.probeCount ), oE = oB + 32 * qint64( h.reserved[1] );

	pgi::Sector sec{};
	sec.x = h.cellX;
	sec.y = h.cellY;
	sec.bakeVersion = h.version;
	sec.content = h.reserved[2];
	sec.bakeFlags = h.flags;
	sec.recordKind = h.recordKind;
	out.push_back( { pgi::kSECT, g, raw( &sec, sizeof sec ) } );

	out.push_back( { pgi::kSURF, g, QByteArray( b + oS, int( 32 * qint64( h.surfelCount ) ) ) } );

	const int az = spec.red == QLatin1String( "horz8" ) ? 8 : pgi::kAzimuths;
	pgi::HorizonHead hh{};
	hh.azimuths = uint16_t( az );
	hh.source = pgi::HorizonSelfPlane;
	hh.count = h.surfelCount;
	QByteArray hz = raw( &hh, sizeof hh );
	hz.reserve( int( sizeof hh + size_t( h.surfelCount ) * 2 * size_t( az ) ) );
	std::vector<uint8_t> rec( size_t( 2 * az ) );
	for ( uint32_t i = 0; i < h.surfelCount; i++ ) {
		pgi::Surfel sf;
		std::memcpy( &sf, b + oS + 32 * qint64( i ), sizeof sf );
		selfPlane( sf.normal, az, rec.data(), rec.data() + az );
		hz.append( reinterpret_cast<const char *>( rec.data() ), int( rec.size() ) );
	}
	out.push_back( { pgi::kHORZ, g, hz } );

	QByteArray pr;
	pr.reserve( int( 176 * qint64( h.probeCount ) ) );
	for ( uint32_t i = 0; i < h.probeCount; i++ ) {
		pr.append( b + oP + 144 * qint64( i ), 144 );
		pr.append( b + oPx + 32 * qint64( i ), 32 );
	}
	out.push_back( { pgi::kPROB, g, pr } );

	const bool shuffle = spec.red == QLatin1String( "shuffle" );
	QByteArray lk;
	lk.reserve( int( 20 * qint64( h.linkCount ) ) );
	for ( uint32_t i = 0; i < h.linkCount; i++ ) {
		pgi::Link l;
		std::memcpy( &l, b + oL + 12 * qint64( i ), 12 );
		std::memcpy( reinterpret_cast<char *>( &l ) + 12, b + oLx + 8 * qint64( i ), 8 );
		if ( shuffle )   // the next facing bin, same room slot
			l.side = uint8_t( ( l.side % 6 + 1 ) % 6 + 6 * ( l.side / 6 ) );
		lk.append( raw( &l, sizeof l ) );
	}
	out.push_back( { pgi::kLINK, g, lk } );

	out.push_back( { pgi::kROOM, g, QByteArray( b + oB, int( 32 * qint64( h.reserved[1] ) ) ) } );
	out.push_back( { pgi::kEMIT, g, QByteArray( b + oE, int( 16 * qint64( h.reserved[3] ) ) ) } );

	R.sectors++;
	R.roomSplit += ( h.reserved[2] & pgi::kContentRoomSplit ) ? 1 : 0;
	R.surfels += h.surfelCount;
	R.probes += h.probeCount;
	R.links += h.linkCount;
	R.rooms += h.reserved[1];
	R.emits += h.reserved[3];
}

bool writeFile( const QString & path, pgi::Header h, const std::vector<Chunk> & chunks, PgiWriteResult & R )
{
	h.chunkCount = uint32_t( chunks.size() );
	std::vector<pgi::DirEntry> dir( chunks.size() );
	uint32_t off = uint32_t( sizeof( pgi::Header ) + chunks.size() * sizeof( pgi::DirEntry ) );
	for ( size_t i = 0; i < chunks.size(); i++ ) {
		const QByteArray & d = chunks[i].data;
		pgi::DirEntry & e = dir[i];
		e.id = chunks[i].id;
		e.group = chunks[i].group;
		e.flags = 0;
		e.offset = off;
		e.storedSize = e.rawSize = uint32_t( d.size() );
		e.crc = pgi::crc32( d.constData(), size_t( d.size() ) );
		off += ( uint32_t( d.size() ) + 3u ) & ~3u;
	}
	h.headerCrc = pgi::headerCrc( h, dir.data() );
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
		R.error = QStringLiteral( "cannot write %1: %2" ).arg( path, f.errorString() );
		return false;
	}
	f.write( reinterpret_cast<const char *>( &h ), sizeof h );
	f.write( reinterpret_cast<const char *>( dir.data() ), qint64( dir.size() * sizeof( pgi::DirEntry ) ) );
	static const char zero[4] = { 0, 0, 0, 0 };
	for ( const Chunk & c : chunks ) {
		f.write( c.data );
		const int pad = ( 4 - ( c.data.size() & 3 ) ) & 3;
		if ( pad )
			f.write( zero, pad );
	}
	f.close();
	R.files.append( path );
	R.bytes += off;
	return true;
}

} // namespace

bool pgiWriteFromTbk( const QStringList & tbkFiles, const PgiWriteSpec & spec, const QString & outDir, PgiWriteResult * out )
{
	PgiWriteResult & R = *out;
	R = PgiWriteResult();
	std::vector<Sector> secs;
	QStringList sorted = tbkFiles;
	std::sort( sorted.begin(), sorted.end() );
	for ( const QString & p : sorted ) {
		Sector s;
		if ( !readTbk( p, s, R.error ) )
			return false;
		if ( !secs.empty() && s.h.surfelCell != secs.front().h.surfelCell ) {
			R.error = QStringLiteral( "%1: surfel cell %2, the first file's %3" ).arg( p ).arg( s.h.surfelCell ).arg( secs.front().h.surfelCell );
			return false;
		}
		secs.push_back( std::move( s ) );
	}
	if ( secs.empty() ) {
		R.error = QStringLiteral( "no .tbk files" );
		return false;
	}
	if ( secs.size() > 65535 ) {
		R.error = QStringLiteral( "too many sectors for one file" );
		return false;
	}
	if ( !QDir().mkpath( outDir ) ) {
		R.error = QStringLiteral( "cannot create %1" ).arg( outDir );
		return false;
	}
	pgi::Header h{};
	h.magic = pgi::kMagic;
	h.version = pgi::kVersion;
	h.surfelCell = secs.front().h.surfelCell;
	h.voxelEdge = 0.0f;   // no GRID yet (lane PGI1)
	h.toolVersion = kToolVersion;
	h.inputHash = 0;      // not recorded yet (lane PGI1)
	h.optionsHash = pgi::fnv1a64( spec.options.constData(), size_t( spec.options.size() ) );
	if ( spec.interior ) {
		h.kind = pgi::KindInterior;
		h.interiorForm = spec.interiorForm;
		std::vector<Chunk> chunks;
		for ( size_t i = 0; i < secs.size(); i++ )
			groupChunks( secs[i], uint16_t( i ), spec, chunks, R );
		QString name = spec.interiorName.isEmpty() ? QStringLiteral( "interior" ) : spec.interiorName;
		return writeFile( QDir( outDir ).filePath( name + QStringLiteral( ".pgi" ) ), h, chunks, R );
	}
	h.kind = pgi::KindExterior;
	const QByteArray w = spec.world.toUtf8();
	h.worldHash = w.isEmpty() ? 0u : pgi::fnv1a32( w.constData(), size_t( w.size() ) );
	for ( const Sector & s : secs ) {
		std::vector<Chunk> chunks;
		groupChunks( s, 0, spec, chunks, R );
		h.cellX = s.h.cellX;
		h.cellY = s.h.cellY;
		if ( !writeFile( QDir( outDir ).filePath( QStringLiteral( "%1_%2.pgi" ).arg( s.h.cellX ).arg( s.h.cellY ) ), h, chunks, R ) )
			return false;
	}
	return true;
}

QString pgiWriteCensusText( const PgiWriteResult & r )
{
	QString s;
	QTextStream t( &s );
	t << "pgi: " << r.files.size() << " files from " << r.sectors << " sectors, " << QString::number( double( r.bytes ) / 1e6, 'f', 2 )
	  << " MB; surfels " << r.surfels << " (horizon 16 azimuths, self-plane), probes " << r.probes << ", links " << r.links
	  << ", room boxes " << r.rooms << ", glowing " << r.emits;
	if ( r.roomSplit )
		t << "; room split sectors " << r.roomSplit;
	if ( !r.error.isEmpty() )
		t << "; REFUSED: " << r.error;
	return s;
}

int pgiWriteCli( const QStringList & args )
{
	QString tbkDir, outDir;
	PgiWriteSpec spec;
	for ( int i = 0; i < args.size(); i++ ) {
		const QString & a = args[i];
		const QString nx = i + 1 < args.size() ? args[i + 1] : QString();
		if ( a == QLatin1String( "--tbk" ) ) { tbkDir = nx; i++; }
		else if ( a == QLatin1String( "--out" ) ) { outDir = nx; i++; }
		else if ( a == QLatin1String( "--world" ) ) { spec.world = nx; i++; }
		else if ( a == QLatin1String( "--interior" ) ) { spec.interior = true; spec.interiorName = nx; i++; }
		else if ( a == QLatin1String( "--options" ) ) { spec.options = nx.toUtf8(); i++; }
		else if ( a == QLatin1String( "--red" ) ) { spec.red = nx; i++; }
	}
	if ( tbkDir.isEmpty() || outDir.isEmpty() ) {
		std::fprintf( stderr, "usage: pgiwrite --tbk <dir of sector_*.tbk> --out <dir> [--interior <name>] [--world <EditorID>] "
			"[--options <text>] [--red shuffle|horz8]\n" );
		return 2;
	}
	QStringList files;
	for ( const QFileInfo & fi : QDir( tbkDir ).entryInfoList( { QStringLiteral( "sector_*.tbk" ) }, QDir::Files, QDir::Name ) )
		files.append( fi.absoluteFilePath() );
	PgiWriteResult r;
	const bool ok = pgiWriteFromTbk( files, spec, outDir, &r );
	std::printf( "%s\n", pgiWriteCensusText( r ).toUtf8().constData() );
	for ( const QString & f : r.files )
		std::printf( "pgi file %s\n", QDir::toNativeSeparators( f ).toUtf8().constData() );
	std::fflush( stdout );
	return ok ? 0 : 1;
}

int pgiCheckCli( const QStringList & args )
{
	bool acceptAll = false;
	QStringList files;
	for ( int i = 0; i < args.size(); i++ ) {
		if ( args[i] == QLatin1String( "--red" ) && i + 1 < args.size() ) {
			acceptAll = args[++i] == QLatin1String( "acceptall" );
		} else {
			files.append( args[i] );
		}
	}
	if ( files.isEmpty() ) {
		std::fprintf( stderr, "usage: pgicheck <file.pgi>... [--red acceptall]\n" );
		return 2;
	}
	int refused = 0;
	for ( const QString & p : files ) {
		QFile f( p );
		QByteArray b;
		if ( f.open( QIODevice::ReadOnly ) )
			b = f.readAll();
		const pgi::Refusal r = acceptAll ? pgi::Ok : pgi::checkFile( b.constData(), size_t( b.size() ) );
		const QByteArray name = QFileInfo( p ).fileName().toUtf8();
		if ( r != pgi::Ok ) {
			refused++;
			std::printf( "pgicheck %s: refused %s\n", name.constData(), pgi::refusalName( r ) );
			continue;
		}
		pgi::Header h{};
		std::memcpy( &h, b.constData(), std::min<size_t>( sizeof h, size_t( b.size() ) ) );
		std::printf( "pgicheck %s: ok kind %u version %u chunks %u bytes %lld\n", name.constData(), unsigned( h.kind ),
			unsigned( h.version ), unsigned( h.chunkCount ), (long long)b.size() );
		for ( uint32_t i = 0; i < h.chunkCount && !acceptAll; i++ ) {
			pgi::DirEntry e;
			std::memcpy( &e, b.constData() + sizeof h + i * sizeof e, sizeof e );
			const char id[5] = { char( e.id & 0xFF ), char( ( e.id >> 8 ) & 0xFF ), char( ( e.id >> 16 ) & 0xFF ), char( e.id >> 24 ), 0 };
			std::printf( "pgicheck chunk %s group %u bytes %u crc %08x\n", id, unsigned( e.group ), unsigned( e.rawSize ), unsigned( e.crc ) );
		}
	}
	std::fflush( stdout );
	return refused ? 1 : 0;
}
