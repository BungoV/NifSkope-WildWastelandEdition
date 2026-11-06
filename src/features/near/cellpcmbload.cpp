#include "features/near/cellpcmbload.h"

#include "wwswitch.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <memory>

/* Lane PCMB1: the CSG store. One entry per data folder: the *.csg files in it by file hash, each with its directory
 * read once and its chunks decompressed on demand. A chunk is 64 KiB uncompressed; the cache keeps the newest
 * kChunkCap of them (a vanilla exterior cell's meshes share a few hundred). */

namespace WwPcmb
{
namespace
{
constexpr int kChunkCap = 1024;   // 64 MiB of decompressed geometry

struct CsgFile
{
	QString path;
	CsgDir dir;
	bool ok = false;
	QMutex m;
	QHash<quint32, QByteArray> chunks;
	QList<quint32> order;   // oldest first
};

struct Store
{
	QMutex m;
	QHash<QString, QHash<quint32, std::shared_ptr<CsgFile>>> roots;
};
Store & store()
{
	static Store s;
	return s;
}

std::atomic<qint64> sShapes{ 0 }, sRefused{ 0 }, sChunksRead{ 0 }, sChunkHits{ 0 }, sVerts{ 0 }, sTris{ 0 };

std::shared_ptr<CsgFile> openCsg( const QString & path )
{
	auto f = std::make_shared<CsgFile>();
	f->path = path;
	QFile in( path );
	if ( !in.open( QIODevice::ReadOnly ) )
		return f;
	const QByteArray head = in.read( 12 );
	const std::size_t need = csgHeaderBytes( reinterpret_cast<const std::uint8_t *>( head.constData() ), std::size_t( head.size() ) );
	if ( !need || qint64( need ) > in.size() )
		return f;
	in.seek( 0 );
	const QByteArray hdr = in.read( qint64( need ) );
	f->ok = parseCsgHeader( reinterpret_cast<const std::uint8_t *>( hdr.constData() ), std::size_t( hdr.size() ),
		std::uint64_t( in.size() ), f->dir );
	if ( !f->ok )
		fprintf( stderr, "cell pcmb: %s: the CSG header does not parse\n", path.toLocal8Bit().constData() );
	return f;
}

std::shared_ptr<CsgFile> csgFor( const QString & dataRoot, quint32 hash )
{
	Store & s = store();
	QMutexLocker l( &s.m );
	auto it = s.roots.find( dataRoot );
	if ( it == s.roots.end() ) {
		QHash<quint32, std::shared_ptr<CsgFile>> files;
		const QDir d( dataRoot );
		for ( const QString & nm : d.entryList( { QStringLiteral( "*.csg" ) }, QDir::Files ) )
			files.insert( csgNameHash( nm.toStdString() ), openCsg( d.filePath( nm ) ) );
		it = s.roots.insert( dataRoot, files );
	}
	return it->value( hash );
}

//! `len` bytes of the uncompressed stream from `off`, chunk by chunk through the cache.
bool streamBytes( CsgFile & f, quint64 off, std::size_t len, QByteArray & out )
{
	out.clear();
	out.reserve( int( len ) );
	QFile in;
	while ( std::size_t( out.size() ) < len ) {
		const quint64 at = off + quint64( out.size() );
		const quint32 ci = quint32( at >> 16 );
		const int inChunk = int( at & 0xFFFF );
		if ( ci >= quint32( f.dir.chunks.size() ) )
			return false;
		QByteArray chunk;
		{
			QMutexLocker l( &f.m );
			const auto it = f.chunks.constFind( ci );
			if ( it != f.chunks.constEnd() ) {
				chunk = *it;
				sChunkHits++;
			}
		}
		if ( chunk.isEmpty() ) {
			if ( !in.isOpen() ) {
				in.setFileName( f.path );
				if ( !in.open( QIODevice::ReadOnly ) )
					return false;
			}
			const CsgChunk & c = f.dir.chunks[ci];
			if ( !in.seek( c.fileOffset ) )
				return false;
			const QByteArray z = in.read( c.compSize );
			if ( z.size() != int( c.compSize ) )
				return false;
			// qUncompress wants the uncompressed size first, big-endian; 65,536 is the chunk size (the last is shorter)
			QByteArray sized( 4, '\0' );
			sized[1] = '\x01';
			chunk = qUncompress( sized + z );
			if ( chunk.isEmpty() || chunk.size() > 65536 )
				return false;
			sChunksRead++;
			QMutexLocker l( &f.m );
			if ( !f.chunks.contains( ci ) ) {
				f.chunks.insert( ci, chunk );
				f.order.append( ci );
				while ( f.order.size() > kChunkCap )
					f.chunks.remove( f.order.takeFirst() );
			}
		}
		if ( inChunk >= chunk.size() )
			return false;
		const int take = int( std::min<std::size_t>( len - std::size_t( out.size() ), std::size_t( chunk.size() - inChunk ) ) );
		out.append( chunk.constData() + inChunk, take );
	}
	return true;
}

bool refuse( QString * why, const QString & text )
{
	if ( why )
		*why = text;
	sRefused++;
	return false;
}
} // namespace

bool redRefs()
{
	return wwEnv( "WW_CELL_PCMB_RED" ).trimmed() == "refs";
}

LoadStats loadStats()
{
	LoadStats s;
	s.shapes = sShapes;
	s.refused = sRefused;
	s.chunksRead = sChunksRead;
	s.chunkHits = sChunkHits;
	s.vertices = sVerts;
	s.triangles = sTris;
	return s;
}

bool loadShape( const QString & dataRoot, const QByteArray & nif, const QList<qint32> & extraBlocks, Geometry & g,
	QString * why )
{
	g = Geometry();
	const auto * d = reinterpret_cast<const std::uint8_t *>( nif.constData() );
	const std::size_t n = std::size_t( nif.size() );
	std::vector<BlockSpan> spans;
	if ( !nifBlockSpans( d, n, spans ) )
		return refuse( why, QStringLiteral( "the NIF header does not walk" ) );
	int packs = 0;
	for ( qint32 b : extraBlocks ) {
		if ( b < 0 || std::size_t( b ) >= spans.size() || spans[b].type != "BSPackedCombinedSharedGeomDataExtra" )
			continue;
		packs++;
		Packed p;
		if ( !parsePackedShared( d, n, spans[b].off, spans[b].size, p ) )
			return refuse( why, QStringLiteral( "block %1 does not end at its size" ).arg( b ) );
		for ( const Object & ob : p.objs ) {
			const std::shared_ptr<CsgFile> f = csgFor( dataRoot, ob.fileHash );
			if ( !f || !f->ok )
				return refuse( why, QStringLiteral( "no readable CSG with file hash %1 in %2" ).arg( ob.fileHash ).arg( dataRoot ) );
			// the object must be a directory start with the same vertex desc and count
			const auto &objs = f->dir.objs;
			const auto it = std::lower_bound( objs.begin(), objs.end(), ob.dataOffset,
				[]( const CsgObject & a, std::uint32_t off ) { return a.streamOffset < off; } );
			if ( it == objs.end() || it->streamOffset != ob.dataOffset || it->vdesc != ob.vdesc || it->nv != ob.nv
				|| it->numIndices != 3 * ( ob.triCount[0] + ob.triCount[1] + ob.triCount[2] ) )
				return refuse( why, QStringLiteral( "object at %1 disagrees with %2's directory" ).arg( ob.dataOffset ).arg( f->path ) );
			const std::size_t need = objectBytes( ob.vdesc, ob.nv, ob.triOffset[0] + 3 * ob.triCount[0] );
			QByteArray bytes;
			if ( !streamBytes( *f, ob.dataOffset, need, bytes ) )
				return refuse( why, QStringLiteral( "object at %1: the CSG stream does not read" ).arg( ob.dataOffset ) );
			if ( !expandObject( ob, reinterpret_cast<const std::uint8_t *>( bytes.constData() ), std::size_t( bytes.size() ), g ) )
				return refuse( why, QStringLiteral( "object at %1 does not expand (layout or index range)" ).arg( ob.dataOffset ) );
		}
	}
	if ( !packs )
		return refuse( why, QStringLiteral( "no packed block on the shape" ) );
	sShapes++;
	sVerts += qint64( g.vertexCount() );
	sTris += qint64( g.tris.size() / 3 );
	return true;
}

} // namespace WwPcmb
