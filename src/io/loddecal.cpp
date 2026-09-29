/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "loddecal.h"

#include "io/lodvfile.h"   // lodvCrc32

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{

template <typename T> void put( QByteArray & b, int at, T v )
{
	std::memcpy( b.data() + at, &v, sizeof( T ) );
}
template <typename T> T get( const QByteArray & b, qint64 at )
{
	T v;
	std::memcpy( &v, b.constData() + at, sizeof( T ) );
	return v;
}

quint32 crcOf( const QByteArray & b, qint64 at = 0, qint64 n = -1 )
{
	if ( n < 0 )
		n = b.size() - at;
	return lodvCrc32( reinterpret_cast<const unsigned char *>( b.constData() ) + at, n );
}

void putEdid( QByteArray & h, int at, const QString & ws )
{
	const QByteArray e = ws.toLatin1().left( 31 );
	std::memcpy( h.data() + at, e.constData(), size_t( e.size() ) );
}

QString getEdid( const QByteArray & h, int at )
{
	const char * p = h.constData() + at;
	int n = 0;
	while ( n < 32 && p[n] )
		n++;
	return QString::fromLatin1( p, n );
}

qint16 snorm16( float v )
{
	return qint16( qBound( -32767, int( std::lround( double( v ) * 32767.0 ) ), 32767 ) );
}

// BC3 block -> 16 texels 0xAARRGGBB
void decodeBc3Block( const unsigned char * b, quint32 out[16] )
{
	// alpha
	quint8 a[8];
	a[0] = b[0];
	a[1] = b[1];
	if ( a[0] > a[1] ) {
		for ( int i = 1; i <= 6; i++ )
			a[i + 1] = quint8( ( ( 7 - i ) * a[0] + i * a[1] + 3 ) / 7 );
	} else {
		for ( int i = 1; i <= 4; i++ )
			a[i + 1] = quint8( ( ( 5 - i ) * a[0] + i * a[1] + 2 ) / 5 );
		a[6] = 0;
		a[7] = 255;
	}
	quint64 abits = 0;
	for ( int i = 0; i < 6; i++ )
		abits |= quint64( b[2 + i] ) << ( 8 * i );
	// colour, always 4-colour in BC3
	const quint16 c0 = quint16( b[8] | ( b[9] << 8 ) ), c1 = quint16( b[10] | ( b[11] << 8 ) );
	int r[4], g[4], bl[4];
	auto unpack = []( quint16 c, int & R, int & G, int & B ) {
		R = ( ( c >> 11 ) & 31 ) * 255 / 31;
		G = ( ( c >> 5 ) & 63 ) * 255 / 63;
		B = ( c & 31 ) * 255 / 31;
	};
	unpack( c0, r[0], g[0], bl[0] );
	unpack( c1, r[1], g[1], bl[1] );
	r[2] = ( 2 * r[0] + r[1] + 1 ) / 3; g[2] = ( 2 * g[0] + g[1] + 1 ) / 3; bl[2] = ( 2 * bl[0] + bl[1] + 1 ) / 3;
	r[3] = ( r[0] + 2 * r[1] + 1 ) / 3; g[3] = ( g[0] + 2 * g[1] + 1 ) / 3; bl[3] = ( bl[0] + 2 * bl[1] + 1 ) / 3;
	const quint32 idx = quint32( b[12] ) | ( quint32( b[13] ) << 8 ) | ( quint32( b[14] ) << 16 ) | ( quint32( b[15] ) << 24 );
	for ( int i = 0; i < 16; i++ ) {
		const int ci = int( ( idx >> ( 2 * i ) ) & 3 );
		const int ai = int( ( abits >> ( 3 * i ) ) & 7 );
		out[i] = ( quint32( a[ai] ) << 24 ) | ( quint32( r[ci] ) << 16 ) | ( quint32( g[ci] ) << 8 ) | quint32( bl[ci] );
	}
}

int mipDim( int d, int m )
{
	return qMax( 1, d >> m );
}

} // namespace

qint64 loddChainBytes( int w, int h, int mips )
{
	qint64 n = 0;
	for ( int m = 0; m < mips; m++ )
		n += qint64( ( mipDim( w, m ) + 3 ) / 4 ) * qint64( ( mipDim( h, m ) + 3 ) / 4 ) * 16;
	return n;
}

bool loddWrite( const QString & path, const QString & worldEdid, float unitsPerTexel, int maxSide,
	QVector<LoddPiece> & pieces, quint32 * tableCrcOut, QString * error )
{
	auto fail = [error]( const QString & m ) { if ( error ) *error = m; return false; };
	const qint64 tableAt = LODD_HEADER_BYTES;
	const qint64 tableBytes = qint64( pieces.size() ) * LODD_PIECE_BYTES;
	QByteArray names;
	QVector<quint32> nameAt( pieces.size() ), nameLen( pieces.size() );
	for ( int i = 0; i < pieces.size(); i++ ) {
		const QByteArray n = pieces[i].name.toUtf8();
		nameAt[i] = quint32( names.size() );
		nameLen[i] = quint32( n.size() );
		names += n;
	}
	const qint64 namesAt = tableAt + tableBytes;
	qint64 payloadAt = ( namesAt + names.size() + 15 ) & ~qint64( 15 );
	qint64 at = payloadAt;
	for ( LoddPiece & p : pieces ) {
		if ( p.colourBc3.size() != loddChainBytes( p.width, p.height, p.mips )
			|| p.normalBc3.size() != loddChainBytes( p.width, p.height, p.mips ) )
			return fail( QString( "decal piece %1: its chains are not %2x%3 with %4 mips" )
				.arg( p.name ).arg( p.width ).arg( p.height ).arg( p.mips ) );
		p.colourOffset = quint64( at );
		p.colourBytes = quint32( p.colourBc3.size() );
		p.colourCrc = crcOf( p.colourBc3 );
		at += p.colourBc3.size();
		p.normalOffset = quint64( at );
		p.normalBytes = quint32( p.normalBc3.size() );
		p.normalCrc = crcOf( p.normalBc3 );
		at += p.normalBc3.size();
	}
	QByteArray table( int( tableBytes ), '\0' );
	for ( int i = 0; i < pieces.size(); i++ ) {
		const LoddPiece & p = pieces[i];
		const int o = i * LODD_PIECE_BYTES;
		put<float>( table, o + 0x00, p.x0 );
		put<float>( table, o + 0x04, p.y0 );
		put<float>( table, o + 0x08, p.x1 );
		put<float>( table, o + 0x0C, p.y1 );
		put<float>( table, o + 0x10, p.z0 );
		put<float>( table, o + 0x14, p.z1 );
		put<quint16>( table, o + 0x18, quint16( p.width ) );
		put<quint16>( table, o + 0x1A, quint16( p.height ) );
		put<quint8>( table, o + 0x1C, quint8( p.mips ) );
		put<quint8>( table, o + 0x1D, quint8( p.cls ) );
		put<quint16>( table, o + 0x1E, quint16( p.flags ) );
		put<quint64>( table, o + 0x20, p.colourOffset );
		put<quint32>( table, o + 0x28, p.colourBytes );
		put<quint32>( table, o + 0x2C, p.colourCrc );
		put<quint64>( table, o + 0x30, p.normalOffset );
		put<quint32>( table, o + 0x38, p.normalBytes );
		put<quint32>( table, o + 0x3C, p.normalCrc );
		put<quint32>( table, o + 0x40, nameAt[i] );
		put<quint32>( table, o + 0x44, nameLen[i] );
		put<quint32>( table, o + 0x48, p.covered );
	}
	const quint32 tcrc = lodvCrc32( reinterpret_cast<const unsigned char *>( names.constData() ),
		names.size(), crcOf( table ) );
	QByteArray h( LODD_HEADER_BYTES, '\0' );
	put<quint32>( h, 0x00, LODD_MAGIC );
	put<quint32>( h, 0x04, LODD_VERSION );
	put<quint32>( h, 0x08, quint32( LODD_HEADER_BYTES ) );
	put<quint32>( h, 0x0C, 0u );
	put<quint64>( h, 0x10, quint64( at ) );
	put<quint64>( h, 0x18, quint64( tableAt ) );
	put<quint64>( h, 0x20, quint64( namesAt ) );
	put<quint64>( h, 0x28, quint64( payloadAt ) );
	put<quint32>( h, 0x30, quint32( pieces.size() ) );
	put<quint32>( h, 0x34, quint32( names.size() ) );
	put<float>( h, 0x38, unitsPerTexel );
	put<quint32>( h, 0x3C, quint32( maxSide ) );
	put<quint32>( h, 0x40, LODD_DXGI_BC3_UNORM_SRGB );
	put<quint32>( h, 0x44, LODD_DXGI_BC3_UNORM );
	put<quint32>( h, 0x48, tcrc );
	putEdid( h, 0x50, worldEdid );
	put<quint32>( h, 0x4C, crcOf( h, 0, 0x4C ) );
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		return fail( QString( "cannot write %1" ).arg( path ) );
	bool ok = f.write( h ) == h.size() && f.write( table ) == table.size() && f.write( names ) == names.size();
	const qint64 pad = payloadAt - ( namesAt + names.size() );
	if ( ok && pad > 0 )
		ok = f.write( QByteArray( int( pad ), '\0' ) ) == pad;
	for ( const LoddPiece & p : pieces ) {
		if ( !ok )
			break;
		ok = f.write( p.colourBc3 ) == p.colourBc3.size() && f.write( p.normalBc3 ) == p.normalBc3.size();
	}
	f.close();
	if ( !ok || QFileInfo( path ).size() != at )
		return fail( QString( "short write to %1" ).arg( path ) );
	if ( tableCrcOut )
		*tableCrcOut = tcrc;
	return true;
}

bool lodgWrite( const QString & path, const QString & worldEdid, int cx0, int cy0, int cx1, int cy1,
	float boxPad, int pieceCount, quint32 loddTableCrc, const QVector<LodgRecord> & records,
	const QVector<QVector<float>> & boxes, QString * error )
{
	auto fail = [error]( const QString & m ) { if ( error ) *error = m; return false; };
	if ( boxes.size() != records.size() )
		return fail( QStringLiteral( "lodgWrite: one box a record" ) );
	const int cw = cx1 - cx0 + 1, ch = cy1 - cy0 + 1;
	if ( cw <= 0 || ch <= 0 )
		return fail( QStringLiteral( "lodgWrite: empty cell rectangle" ) );
	std::vector<std::vector<quint32>> cells( size_t( cw ) * size_t( ch ) );
	for ( int r = 0; r < records.size(); r++ ) {
		const QVector<float> & b = boxes[r];
		const int x0 = qMax( cx0, int( std::floor( b[0] / 4096.0f ) ) );
		const int y0 = qMax( cy0, int( std::floor( b[1] / 4096.0f ) ) );
		const int x1 = qMin( cx1, int( std::floor( b[2] / 4096.0f ) ) );
		const int y1 = qMin( cy1, int( std::floor( b[3] / 4096.0f ) ) );
		for ( int y = y0; y <= y1; y++ )
			for ( int x = x0; x <= x1; x++ )
				cells[size_t( y - cy0 ) * size_t( cw ) + size_t( x - cx0 )].push_back( quint32( r ) );
	}
	QByteArray recs( records.size() * LODG_RECORD_BYTES, '\0' );
	for ( int r = 0; r < records.size(); r++ ) {
		const LodgRecord & e = records[r];
		const int o = r * LODG_RECORD_BYTES;
		put<float>( recs, o + 0, e.pos[0] );
		put<float>( recs, o + 4, e.pos[1] );
		put<float>( recs, o + 8, e.pos[2] );
		for ( int k = 0; k < 4; k++ )
			put<qint16>( recs, o + 12 + 2 * k, snorm16( e.quat[k] ) );
		put<float>( recs, o + 20, e.scale );
		put<quint16>( recs, o + 24, quint16( e.piece ) );
		put<quint8>( recs, o + 26, quint8( e.cls ) );
		put<quint8>( recs, o + 27, quint8( e.flags ) );
		put<quint32>( recs, o + 28, e.refr );
	}
	QByteArray table( int( cells.size() * 8 ), '\0' );
	QByteArray idx;
	quint32 run = 0;
	for ( size_t c = 0; c < cells.size(); c++ ) {
		put<quint32>( table, int( c * 8 ), run );
		put<quint32>( table, int( c * 8 + 4 ), quint32( cells[c].size() ) );
		for ( quint32 v : cells[c] ) {
			const int o = idx.size();
			idx.resize( o + 4 );
			put<quint32>( idx, o, v );
		}
		run += quint32( cells[c].size() );
	}
	const qint64 recAt = LODD_HEADER_BYTES;
	const qint64 tabAt = recAt + recs.size();
	const qint64 idxAt = tabAt + table.size();
	const qint64 end = idxAt + idx.size();
	quint32 dcrc = crcOf( recs );
	dcrc = lodvCrc32( reinterpret_cast<const unsigned char *>( table.constData() ), table.size(), dcrc );
	dcrc = lodvCrc32( reinterpret_cast<const unsigned char *>( idx.constData() ), idx.size(), dcrc );
	QByteArray h( LODD_HEADER_BYTES, '\0' );
	put<quint32>( h, 0x00, LODG_MAGIC );
	put<quint32>( h, 0x04, LODG_VERSION );
	put<quint32>( h, 0x08, quint32( LODD_HEADER_BYTES ) );
	put<quint64>( h, 0x10, quint64( end ) );
	put<quint64>( h, 0x18, quint64( recAt ) );
	put<quint64>( h, 0x20, quint64( tabAt ) );
	put<quint64>( h, 0x28, quint64( idxAt ) );
	put<quint32>( h, 0x30, quint32( records.size() ) );
	put<quint32>( h, 0x34, quint32( LODG_RECORD_BYTES ) );
	put<qint16>( h, 0x38, qint16( cx0 ) );
	put<qint16>( h, 0x3A, qint16( cy0 ) );
	put<qint16>( h, 0x3C, qint16( cx1 ) );
	put<qint16>( h, 0x3E, qint16( cy1 ) );
	put<quint32>( h, 0x40, run );
	put<quint32>( h, 0x44, quint32( pieceCount ) );
	put<quint32>( h, 0x48, loddTableCrc );
	put<quint32>( h, 0x4C, dcrc );
	putEdid( h, 0x50, worldEdid );
	put<float>( h, 0x70, boxPad );
	put<quint32>( h, 0x74, crcOf( h, 0, 0x74 ) );
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
		return fail( QString( "cannot write %1" ).arg( path ) );
	const bool ok = f.write( h ) == h.size() && f.write( recs ) == recs.size()
		&& f.write( table ) == table.size() && f.write( idx ) == idx.size();
	f.close();
	if ( !ok || QFileInfo( path ).size() != end )
		return fail( QString( "short write to %1" ).arg( path ) );
	return true;
}

/* ---- the readers ---------------------------------------------------------- */

bool LoddFile::open( const QString & path, QString * error )
{
	auto fail = [error, &path]( const QString & m ) {
		if ( error ) *error = QString( "%1: %2" ).arg( path, m );
		return false;
	};
	filePath = path;
	pcs.clear();
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) )
		return fail( QStringLiteral( "cannot open" ) );
	bytes = f.size();
	const QByteArray h = f.read( LODD_HEADER_BYTES );
	if ( h.size() != LODD_HEADER_BYTES )
		return fail( QStringLiteral( "shorter than its header" ) );
	const quint32 magic = get<quint32>( h, 0 );
	if ( magic == LODG_MAGIC )
		return fail( QStringLiteral( "this is a decal PLACEMENT file (.lodg), not the library" ) );
	if ( magic != LODD_MAGIC )
		return fail( QStringLiteral( "not a decal library (magic %1)" ).arg( magic, 8, 16, QChar( '0' ) ) );
	if ( get<quint32>( h, 4 ) != LODD_VERSION )
		return fail( QString( "version %1; this reader knows %2" ).arg( get<quint32>( h, 4 ) ).arg( LODD_VERSION ) );
	if ( get<quint32>( h, 0x4C ) != crcOf( h, 0, 0x4C ) )
		return fail( QStringLiteral( "header CRC mismatch" ) );
	if ( qint64( get<quint64>( h, 0x10 ) ) != bytes )
		return fail( QString( "header says %1 bytes, the file has %2" ).arg( get<quint64>( h, 0x10 ) ).arg( bytes ) );
	const qint64 tableAt = qint64( get<quint64>( h, 0x18 ) ), namesAt = qint64( get<quint64>( h, 0x20 ) );
	const int n = int( get<quint32>( h, 0x30 ) ), nameBytes = int( get<quint32>( h, 0x34 ) );
	upt = get<float>( h, 0x38 );
	side = int( get<quint32>( h, 0x3C ) );
	tcrc = get<quint32>( h, 0x48 );
	ws = getEdid( h, 0x50 );
	if ( get<quint32>( h, 0x40 ) != LODD_DXGI_BC3_UNORM_SRGB || get<quint32>( h, 0x44 ) != LODD_DXGI_BC3_UNORM )
		return fail( QStringLiteral( "unknown sheet formats" ) );
	if ( !f.seek( tableAt ) )
		return fail( QStringLiteral( "table offset past the end" ) );
	const QByteArray t = f.read( qint64( n ) * LODD_PIECE_BYTES );
	if ( !f.seek( namesAt ) )
		return fail( QStringLiteral( "name offset past the end" ) );
	const QByteArray names = f.read( nameBytes );
	if ( t.size() != n * LODD_PIECE_BYTES || names.size() != nameBytes )
		return fail( QStringLiteral( "truncated table" ) );
	const quint32 c = lodvCrc32( reinterpret_cast<const unsigned char *>( names.constData() ), names.size(), crcOf( t ) );
	if ( c != tcrc )
		return fail( QStringLiteral( "piece table CRC mismatch" ) );
	pcs.resize( n );
	for ( int i = 0; i < n; i++ ) {
		LoddPiece & p = pcs[i];
		const int o = i * LODD_PIECE_BYTES;
		p.x0 = get<float>( t, o + 0x00 ); p.y0 = get<float>( t, o + 0x04 );
		p.x1 = get<float>( t, o + 0x08 ); p.y1 = get<float>( t, o + 0x0C );
		p.z0 = get<float>( t, o + 0x10 ); p.z1 = get<float>( t, o + 0x14 );
		p.width = get<quint16>( t, o + 0x18 );
		p.height = get<quint16>( t, o + 0x1A );
		p.mips = get<quint8>( t, o + 0x1C );
		p.cls = get<quint8>( t, o + 0x1D );
		p.flags = get<quint16>( t, o + 0x1E );
		p.colourOffset = get<quint64>( t, o + 0x20 );
		p.colourBytes = get<quint32>( t, o + 0x28 );
		p.colourCrc = get<quint32>( t, o + 0x2C );
		p.normalOffset = get<quint64>( t, o + 0x30 );
		p.normalBytes = get<quint32>( t, o + 0x38 );
		p.normalCrc = get<quint32>( t, o + 0x3C );
		const quint32 na = get<quint32>( t, o + 0x40 ), nl = get<quint32>( t, o + 0x44 );
		if ( qint64( na ) + nl > names.size() )
			return fail( QString( "piece %1's name leaves the name blob" ).arg( i ) );
		p.name = QString::fromUtf8( names.constData() + na, int( nl ) );
		p.covered = get<quint32>( t, o + 0x48 );
		const qint64 want = loddChainBytes( p.width, p.height, p.mips );
		if ( p.width <= 0 || p.height <= 0 || ( p.width & 3 ) || ( p.height & 3 ) || p.mips < 1
			|| p.colourBytes != want || p.normalBytes != want
			|| qint64( p.colourOffset ) + want > bytes || qint64( p.normalOffset ) + want > bytes )
			return fail( QString( "piece %1 (%2) has an impossible layout" ).arg( i ).arg( p.name ) );
		if ( p.cls < LODD_CLASS_ROAD || p.cls > LODD_CLASS_FLAT_OVER )
			return fail( QString( "piece %1 has draw class %2" ).arg( i ).arg( p.cls ) );
	}
	return true;
}

bool LoddFile::picture( int i, bool normal, int mip, std::vector<quint32> & out, int * w, int * h,
	QString * error ) const
{
	if ( i < 0 || i >= pcs.size() || mip < 0 || mip >= pcs[i].mips ) {
		if ( error ) *error = QStringLiteral( "no such piece or mip" );
		return false;
	}
	const LoddPiece & p = pcs[i];
	qint64 at = qint64( normal ? p.normalOffset : p.colourOffset );
	for ( int m = 0; m < mip; m++ )
		at += loddChainBytes( mipDim( p.width, m ), mipDim( p.height, m ), 1 );
	const int mw = mipDim( p.width, mip ), mh = mipDim( p.height, mip );
	const int bw = ( mw + 3 ) / 4, bh = ( mh + 3 ) / 4;
	QFile f( filePath );
	if ( !f.open( QIODevice::ReadOnly ) || !f.seek( at ) ) {
		if ( error ) *error = QStringLiteral( "cannot read %1" ).arg( filePath );
		return false;
	}
	const QByteArray blk = f.read( qint64( bw ) * bh * 16 );
	if ( blk.size() != bw * bh * 16 ) {
		if ( error ) *error = QStringLiteral( "truncated picture" );
		return false;
	}
	out.assign( size_t( mw ) * size_t( mh ), 0U );
	quint32 px[16];
	for ( int by = 0; by < bh; by++ )
		for ( int bx = 0; bx < bw; bx++ ) {
			decodeBc3Block( reinterpret_cast<const unsigned char *>( blk.constData() ) + ( by * bw + bx ) * 16, px );
			for ( int k = 0; k < 16; k++ ) {
				const int x = bx * 4 + ( k & 3 ), y = by * 4 + ( k >> 2 );
				if ( x < mw && y < mh )
					out[size_t( y ) * mw + size_t( x )] = px[k];
			}
		}
	if ( w ) *w = mw;
	if ( h ) *h = mh;
	return true;
}

bool LoddFile::verify( QString * report, QString * error ) const
{
	QFile f( filePath );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		if ( error ) *error = QStringLiteral( "cannot open %1" ).arg( filePath );
		return false;
	}
	int bad = 0, stamped = 0;
	qint64 texels = 0, covered = 0, pictureBytes = 0;
	int cls[3] = { 0, 0, 0 };
	QString first;
	for ( int i = 0; i < pcs.size(); i++ ) {
		const LoddPiece & p = pcs[i];
		f.seek( qint64( p.colourOffset ) );
		const QByteArray c = f.read( p.colourBytes );
		f.seek( qint64( p.normalOffset ) );
		const QByteArray nrm = f.read( p.normalBytes );
		if ( crcOf( c ) != p.colourCrc || crcOf( nrm ) != p.normalCrc ) {
			if ( !bad )
				first = p.name;
			bad++;
		}
		texels += qint64( p.width ) * p.height;
		covered += p.covered;
		pictureBytes += p.colourBytes + p.normalBytes;
		cls[qBound( 0, p.cls, 2 )]++;
		if ( p.flags & 1u )
			stamped++;
	}
	if ( report )
		*report = QString( "lodd: %1 piece(s) (roads %2, flat %3, flat-over %4), %5 mip-0 texel(s) "
			"(%6 covered), %7 picture byte(s), %8 byte(s) on disk, %9 u a texel, %10 with a normal stamp, "
			"%11 CRC mismatch(es)" )
			.arg( pcs.size() ).arg( cls[0] ).arg( cls[1] ).arg( cls[2] ).arg( texels ).arg( covered )
			.arg( pictureBytes ).arg( bytes ).arg( double( upt ) ).arg( stamped ).arg( bad );
	if ( bad ) {
		if ( error ) *error = QString( "%1 picture(s) fail their CRC, first %2" ).arg( bad ).arg( first );
		return false;
	}
	return true;
}

bool LodgFile::open( const QString & path, QString * error )
{
	auto fail = [error, &path]( const QString & m ) {
		if ( error ) *error = QString( "%1: %2" ).arg( path, m );
		return false;
	};
	recs.clear();
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) )
		return fail( QStringLiteral( "cannot open" ) );
	const QByteArray all = f.readAll();
	bytes = all.size();
	if ( bytes < LODD_HEADER_BYTES )
		return fail( QStringLiteral( "shorter than its header" ) );
	const quint32 magic = get<quint32>( all, 0 );
	if ( magic == LODD_MAGIC )
		return fail( QStringLiteral( "this is the decal LIBRARY (.lodd), not the placements" ) );
	if ( magic != LODG_MAGIC )
		return fail( QStringLiteral( "not a decal placement file (magic %1)" ).arg( magic, 8, 16, QChar( '0' ) ) );
	if ( get<quint32>( all, 4 ) != LODG_VERSION )
		return fail( QString( "version %1; this reader knows %2" ).arg( get<quint32>( all, 4 ) ).arg( LODG_VERSION ) );
	if ( get<quint32>( all, 0x74 ) != crcOf( all, 0, 0x74 ) )
		return fail( QStringLiteral( "header CRC mismatch" ) );
	if ( qint64( get<quint64>( all, 0x10 ) ) != bytes )
		return fail( QStringLiteral( "file size does not match its header" ) );
	const qint64 recAt = qint64( get<quint64>( all, 0x18 ) ), tabAt = qint64( get<quint64>( all, 0x20 ) );
	const qint64 idxAt = qint64( get<quint64>( all, 0x28 ) );
	const int n = int( get<quint32>( all, 0x30 ) );
	if ( get<quint32>( all, 0x34 ) != quint32( LODG_RECORD_BYTES ) )
		return fail( QStringLiteral( "unknown record stride" ) );
	minX = get<qint16>( all, 0x38 ); minY = get<qint16>( all, 0x3A );
	maxX = get<qint16>( all, 0x3C ); maxY = get<qint16>( all, 0x3E );
	const quint32 nIdx = get<quint32>( all, 0x40 );
	nPieces = int( get<quint32>( all, 0x44 ) );
	loddCrc = get<quint32>( all, 0x48 );
	pad = get<float>( all, 0x70 );
	const qint64 nCells = qint64( maxX - minX + 1 ) * qint64( maxY - minY + 1 );
	if ( nCells <= 0 || recAt + qint64( n ) * LODG_RECORD_BYTES != tabAt || tabAt + nCells * 8 != idxAt
		|| idxAt + qint64( nIdx ) * 4 != bytes )
		return fail( QStringLiteral( "sections do not tile the file" ) );
	if ( crcOf( all, recAt, bytes - recAt ) != get<quint32>( all, 0x4C ) )
		return fail( QStringLiteral( "data CRC mismatch" ) );
	recs.resize( n );
	for ( int r = 0; r < n; r++ ) {
		LodgRecord & e = recs[r];
		const qint64 o = recAt + qint64( r ) * LODG_RECORD_BYTES;
		for ( int k = 0; k < 3; k++ )
			e.pos[k] = get<float>( all, o + 4 * k );
		for ( int k = 0; k < 4; k++ )
			e.quat[k] = float( get<qint16>( all, o + 12 + 2 * k ) ) / 32767.0f;
		e.scale = get<float>( all, o + 20 );
		e.piece = get<quint16>( all, o + 24 );
		e.cls = get<quint8>( all, o + 26 );
		e.flags = get<quint8>( all, o + 27 );
		e.refr = get<quint32>( all, o + 28 );
	}
	cellFirst.resize( size_t( nCells ) );
	cellCount.resize( size_t( nCells ) );
	for ( qint64 c = 0; c < nCells; c++ ) {
		cellFirst[size_t( c )] = get<quint32>( all, tabAt + c * 8 );
		cellCount[size_t( c )] = get<quint32>( all, tabAt + c * 8 + 4 );
	}
	index.resize( nIdx );
	if ( nIdx )
		std::memcpy( index.data(), all.constData() + idxAt, size_t( nIdx ) * 4 );
	return true;
}

QVector<quint32> LodgFile::cellRecords( int cx, int cy ) const
{
	QVector<quint32> r;
	if ( cx < minX || cx > maxX || cy < minY || cy > maxY )
		return r;
	const size_t c = size_t( cy - minY ) * size_t( maxX - minX + 1 ) + size_t( cx - minX );
	for ( quint32 k = 0; k < cellCount[c]; k++ )
		r.append( index[size_t( cellFirst[c] ) + k] );
	return r;
}

bool LodgFile::verify( QString * report, QString * error ) const
{
	int badPiece = 0, badCls = 0, badScale = 0, badQuat = 0;
	int cls[3] = { 0, 0, 0 };
	int lastCls = 0;
	int orderBreaks = 0;
	for ( const LodgRecord & e : recs ) {
		if ( e.piece < 0 || e.piece >= nPieces )
			badPiece++;
		if ( e.cls < 0 || e.cls > 2 )
			badCls++;
		else
			cls[e.cls]++;
		if ( !( e.scale > 0.0f ) || !std::isfinite( e.scale ) )
			badScale++;
		const float q2 = e.quat[0] * e.quat[0] + e.quat[1] * e.quat[1] + e.quat[2] * e.quat[2] + e.quat[3] * e.quat[3];
		if ( std::fabs( q2 - 1.0f ) > 1e-3f )
			badQuat++;
		if ( e.cls < lastCls )
			orderBreaks++;
		lastCls = e.cls;
	}
	int badIdx = 0, unsorted = 0, busiest = 0, used = 0;
	quint32 expectFirst = 0;
	for ( size_t c = 0; c < cellFirst.size(); c++ ) {
		if ( cellFirst[c] != expectFirst )
			badIdx++;
		expectFirst += cellCount[c];
		busiest = qMax( busiest, int( cellCount[c] ) );
		if ( cellCount[c] )
			used++;
		for ( quint32 k = 0; k < cellCount[c]; k++ ) {
			const quint32 v = index[size_t( cellFirst[c] ) + k];
			if ( v >= quint32( recs.size() ) )
				badIdx++;
			if ( k && v <= index[size_t( cellFirst[c] ) + k - 1] )
				unsorted++;
		}
	}
	if ( expectFirst != index.size() )
		badIdx++;
	if ( report )
		*report = QString( "lodg: %1 placement(s) (roads %2, flat %3, flat-over %4) over cells %5..%6 x %7..%8, "
			"%9 index entr(ies), %10 cell(s) used, busiest %11, %12 byte(s); bad piece %13, bad class %14, "
			"bad scale %15, unnormalised rotation %16, draw-order breaks %17, bad index %18, unsorted index %19" )
			.arg( recs.size() ).arg( cls[0] ).arg( cls[1] ).arg( cls[2] )
			.arg( minX ).arg( maxX ).arg( minY ).arg( maxY ).arg( index.size() ).arg( used ).arg( busiest )
			.arg( bytes ).arg( badPiece ).arg( badCls ).arg( badScale ).arg( badQuat ).arg( orderBreaks )
			.arg( badIdx ).arg( unsorted );
	const bool ok = !badPiece && !badCls && !badScale && !badQuat && !orderBreaks && !badIdx && !unsorted;
	if ( !ok && error )
		*error = QStringLiteral( "the placement file fails its own checks (see the line)" );
	return ok;
}

bool loddCheckPair( const QString & fileOrDir, QString * report, QString * error )
{
	QString dir = fileOrDir, ws;
	QFileInfo fi( fileOrDir );
	if ( fi.isFile() ) {
		dir = fi.absolutePath();
		ws = fi.completeBaseName();
	} else {
		const QStringList l = QDir( dir ).entryList( { QStringLiteral( "*.lodd" ) }, QDir::Files );
		if ( l.size() != 1 ) {
			if ( error ) *error = QString( "%1 holds %2 .lodd file(s); name one" ).arg( dir ).arg( l.size() );
			return false;
		}
		ws = QFileInfo( l.first() ).completeBaseName();
	}
	LoddFile d;
	LodgFile g;
	QString e;
	if ( !d.open( dir + QChar( '/' ) + ws + QStringLiteral( ".lodd" ), &e )
		|| !g.open( dir + QChar( '/' ) + ws + QStringLiteral( ".lodg" ), &e ) ) {
		if ( error ) *error = e;
		return false;
	}
	QString r1, r2, e1, e2;
	const bool ok1 = d.verify( &r1, &e1 );
	const bool ok2 = g.verify( &r2, &e2 );
	const bool paired = g.loddTableCrc() == d.tableCrc() && g.pieceCount() == d.pieceCount();
	// every picture decodes at mip 0 and its last mip
	int decodeFail = 0;
	for ( int i = 0; i < d.pieceCount(); i++ ) {
		std::vector<quint32> px;
		for ( int m : { 0, d.piece( i ).mips - 1 } )
			for ( bool n : { false, true } )
				if ( !d.picture( i, n, m, px, nullptr, nullptr, nullptr ) )
					decodeFail++;
	}
	if ( report )
		*report = r1 + QChar( '\n' ) + r2 + QString( "\npair: %1 (lodg names table crc %2, lodd has %3); "
			"%4 picture decode failure(s)" )
			.arg( paired ? QStringLiteral( "matched" ) : QStringLiteral( "MISMATCHED" ) )
			.arg( g.loddTableCrc(), 8, 16, QChar( '0' ) ).arg( d.tableCrc(), 8, 16, QChar( '0' ) ).arg( decodeFail );
	if ( !ok1 || !ok2 || !paired || decodeFail ) {
		if ( error )
			*error = !ok1 ? e1 : !ok2 ? e2 : !paired ? QStringLiteral( "the .lodg was written against another .lodd" )
				: QStringLiteral( "a picture does not decode" );
		return false;
	}
	return true;
}
