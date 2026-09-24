/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lodtsheets.h"

#include "gamemanager.h"
#include "io/lodvfile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QStandardPaths>

#include <vector>

namespace
{

//! The classic DDS header plus DDS_HEADER_DXT10, 148 bytes, for ONE mip of a
//! block-compressed sheet. The same shape src/lodgen.cpp writes (~6870); the
//! only differences are the block-format flags and the linear size.
QByteArray ddsDx10Header( int width, int height, quint32 dxgiFormat, quint32 linearBytes )
{
	quint32 hdr[32] = { 0 };
	hdr[0] = 0x20534444U;           // 'DDS '
	hdr[1] = 124;
	hdr[2] = 0x000A1007U;           // caps|height|width|linearsize|pixelformat|mipcount
	hdr[3] = quint32( height );
	hdr[4] = quint32( width );
	hdr[5] = linearBytes;           // LINEAR SIZE, the block format's own rule
	hdr[7] = 1;                     // one mip: the viewer takes the sheet's mip 0
	hdr[19] = 32;                   // DDS_PIXELFORMAT size
	hdr[20] = 0x4;                  // fourCC
	hdr[21] = 0x30315844U;          // 'DX10'
	hdr[27] = 0x1000;               // caps: texture
	const quint32 dx10[5] = { dxgiFormat, 3U, 0U, 1U, 0U };
	QByteArray out( reinterpret_cast<const char *>( hdr ), 128 );
	out.append( reinterpret_cast<const char *>( dx10 ), 20 );
	return out;
}

} // namespace

struct LodtSheets::Impl
{
	bool open = false;
	QString path;
	QString cache;
	QString stem;               //!< the container's own base name, for file names
	LodvHeaderFields h;
	std::vector<LodvTileEntry> table;
	int colourSheet = -1, msnSheet = -1, maskSheet = -1;
	QStringList notes;
};

LodtSheets::LodtSheets() : d( new Impl ) {}
LodtSheets::~LodtSheets() = default;

bool LodtSheets::isOpen() const { return d->open; }
int LodtSheets::levelDim() const { return int( d->h.levelDim ); }
int LodtSheets::tilesX() const { return int( d->h.tilesX ); }
int LodtSheets::tilesY() const { return int( d->h.tilesY ); }
int LodtSheets::west() const { return int( d->h.west ); }
int LodtSheets::south() const { return int( d->h.south ); }
int LodtSheets::north() const { return int( d->h.north ); }
int LodtSheets::east() const { return int( d->h.east ); }
int LodtSheets::contentTexels() const { return int( d->h.contentTexels ); }
int LodtSheets::borderTexels() const { return int( d->h.borderTexels ); }
int LodtSheets::storedTexels() const { return int( d->h.storedTexels ); }
QString LodtSheets::containerPath() const { return d->path; }
QString LodtSheets::cacheDir() const { return d->cache; }
QStringList LodtSheets::notes() const { return d->notes; }

float LodtSheets::uvBias() const
{
	return d->h.storedTexels ? float( d->h.borderTexels ) / float( d->h.storedTexels ) : 0.0f;
}

float LodtSheets::uvScale() const
{
	return d->h.storedTexels ? float( d->h.contentTexels ) / float( d->h.storedTexels ) : 1.0f;
}

bool LodtSheets::open( const QString & lodlPath, QString * why )
{
	return openFiltered( lodlPath, -1, why );
}

bool LodtSheets::openForRole( const QString & lodlPath, int role, QString * why )
{
	return openFiltered( lodlPath, role, why );
}

bool LodtSheets::openFiltered( const QString & lodlPath, int wantRole, QString * why )
{
	auto no = [why]( const QString & m ) {
		if ( why )
			*why = m;
		return false;
	};

	const QFileInfo li( lodlPath );
	const QString stem = li.completeBaseName();

	QString dir = qEnvironmentVariable( "WW_LODL_SHEETS" );
	if ( !dir.isEmpty() ) {
		if ( !QFileInfo( dir ).isDir() )
			return no( QString( "WW_LODL_SHEETS=%1 is not a directory" ).arg( dir ) );
	} else {
		dir = li.absolutePath();
	}

	/* `<worldspace>.VT.<dim>.lodt`, the name the bake writes. The FINEST level
	 * is the smallest dim; WW_LODL_SHEET_DIM asks for one by name. */
	/* WW_LODL_SHEET_DIM names a level, and is IGNORED when a role was asked
	 * for: the horizon is on the level the bake authored it on, and a dial
	 * pointing elsewhere would turn "here it is" into "absent". */
	const int wantDim = ( wantRole >= 0 ) ? 0 : qEnvironmentVariableIntValue( "WW_LODL_SHEET_DIM" );
	QDir qd( dir );
	const QStringList found = qd.entryList(
		QStringList() << ( stem + QStringLiteral( ".VT.*.lodt" ) ), QDir::Files, QDir::Name );
	if ( found.isEmpty() )
		return no( QString( "no %1.VT.*.lodt beside %2" ).arg( stem, dir ) );

	QString pick;
	int pickDim = 0;
	for ( const QString & f : found ) {
		const QStringList parts = f.split( QChar( '.' ) );
		if ( parts.size() < 4 )
			continue;
		bool ok = false;
		const int dim = parts.at( parts.size() - 2 ).toInt( &ok );
		if ( !ok || dim <= 0 )
			continue;
		if ( wantDim > 0 ) {
			if ( dim != wantDim )
				continue;
		} else if ( pickDim && dim >= pickDim ) {
			continue;
		}
		const QString cand = qd.absoluteFilePath( f );
		if ( wantRole >= 0 ) {
			/* The header is read to ANSWER the question, not to guess it: a
			 * level carries the role or it does not, and the file says so. */
			LodvHeaderFields ch;
			std::vector<LodvTileEntry> ct;
			QString ce;
			if ( !lodvValidate( cand, &ch, &ct, false, &ce ) )
				continue;
			bool has = false;
			for ( int i = 0; i < int( ch.sheetCount ); i++ )
				if ( ch.sheets[i].role == quint32( wantRole ) )
					has = true;
			if ( !has )
				continue;
		}
		pick = cand;
		pickDim = dim;
	}
	if ( pick.isEmpty() )
		return no( wantRole >= 0
			? QString( "no %1.VT.<dim>.lodt in %2 carries a sheet with role %3" )
				.arg( stem, dir ).arg( wantRole )
			: wantDim > 0
			? QString( "no %1.VT.%2.lodt in %3" ).arg( stem ).arg( wantDim ).arg( dir )
			: QString( "no usable %1.VT.<dim>.lodt in %2" ).arg( stem, dir ) );

	QString err;
	if ( !lodvValidate( pick, &d->h, &d->table, false, &err ) )
		return no( QString( "%1: %2" ).arg( QFileInfo( pick ).fileName(), err ) );

	if ( d->h.compression != 0 )
		return no( QString( "%1 stores its tiles compressed (compression %2); this "
			"reader unpacks RAW payloads only" )
			.arg( QFileInfo( pick ).fileName() ).arg( int( d->h.compression ) ) );

	for ( int i = 0; i < LODV_MAX_SHEETS && i < int( d->h.sheetCount ); i++ ) {
		if ( d->h.sheets[i].role == LODV_ROLE_COLOR )
			d->colourSheet = i;
		else if ( d->h.sheets[i].role == LODV_ROLE_MSN )
			d->msnSheet = i;
		else if ( d->h.sheets[i].role == LODV_ROLE_MASK )
			d->maskSheet = i;
	}
	if ( d->colourSheet < 0 )
		return no( QString( "%1 carries no colour sheet" ).arg( QFileInfo( pick ).fileName() ) );

	/* The cache, and the folder the renderer will resolve against. Under the
	 * system temporary directory by default; WW_LODL_SHEET_CACHE puts it where
	 * a lane can look at the files afterwards. */
	QString cache = qEnvironmentVariable( "WW_LODL_SHEET_CACHE" );
	if ( cache.isEmpty() )
		cache = QDir( QStandardPaths::writableLocation( QStandardPaths::TempLocation ) )
			.absoluteFilePath( QStringLiteral( "nifskope_ww_lodl_sheets" ) );
	if ( !QDir().mkpath( cache + QStringLiteral( "/Textures/LODLSheets" ) ) )
		return no( QString( "could not create the sheet cache at %1" ).arg( cache ) );

	/* Session-only resource root, exactly as WW_LODGEN_RESOURCES does it in
	 * src/main.cpp: prepend, then drop the open archives so the next lookup
	 * re-scans. GameManager::save() is what persists a folder list and only the
	 * Settings dialog calls it, so bungo's own Resources page is untouched. */
	QStringList view = Game::GameManager::folders( Game::FALLOUT_4 );
	if ( !view.contains( cache, Qt::CaseInsensitive ) ) {
		view.prepend( cache );
		Game::GameManager::update_folders( Game::FALLOUT_4, view );
		Game::GameManager::close_resources();
	}

	d->path = pick;
	d->cache = cache;
	d->stem = QFileInfo( pick ).completeBaseName();
	d->open = true;

	int present = 0;
	for ( const LodvTileEntry & e : d->table )
		if ( e.flags & LODV_TILE_PRESENT )
			present++;
	d->notes << QString( "sheets %1: level dim %2 cells, %3x%4 tiles (%5 present), "
			"cells [%6,%7]..[%8,%9], content %10 border %11 stored %12, %13 sheets" )
		.arg( QFileInfo( pick ).fileName() )
		.arg( d->h.levelDim ).arg( d->h.tilesX ).arg( d->h.tilesY ).arg( present )
		.arg( d->h.west ).arg( d->h.south ).arg( d->h.east ).arg( d->h.north )
		.arg( d->h.contentTexels ).arg( d->h.borderTexels ).arg( d->h.storedTexels )
		.arg( int( d->h.sheetCount ) );
	d->notes << QString( "sheet cache %1 (loose DDS, session resource root)" ).arg( cache );
	return true;
}

bool LodtSheets::tileOfCell( int cellX, int cellY, int * tx, int * ty ) const
{
	if ( !d->open || d->h.levelDim <= 0 )
		return false;
	const int dim = int( d->h.levelDim );
	if ( cellX < int( d->h.west ) || cellX > int( d->h.east )
		|| cellY < int( d->h.south ) || cellY > int( d->h.north ) )
		return false;
	const int x = ( cellX - int( d->h.west ) ) / dim;
	// ty = 0 is the NORTH row (docs/LODGEN_TERRAIN_VT.md 2.2)
	const int y = ( int( d->h.north ) - cellY ) / dim;
	if ( x < 0 || x >= int( d->h.tilesX ) || y < 0 || y >= int( d->h.tilesY ) )
		return false;
	if ( tx )
		*tx = x;
	if ( ty )
		*ty = y;
	return true;
}

/* BC1 colour block -> ONE of R (0), G (1), B (2) for its 16 texels. BC3 is an
 * 8-byte alpha block followed by the same colour block, so this takes the
 * colour half of either. Widened from `bc1BlueBlock` by lane CHANVIEW1: the AO
 * read wanted only B, WW_LODL_CHANNEL wants the roughness and the metallic out
 * of the same blocks, and two decoders of one format is how they drift apart. */
static void bc1ColourBlock( const unsigned char * b, int channel, quint8 out[16] )
{
	const unsigned c0 = b[0] | ( unsigned( b[1] ) << 8 ), c1 = b[2] | ( unsigned( b[3] ) << 8 );
	auto comp = [channel]( unsigned c ) {
		if ( channel == 0 ) {              // R: 5 bits
			const unsigned v = ( c >> 11 ) & 31;
			return int( ( v << 3 ) | ( v >> 2 ) );
		}
		if ( channel == 1 ) {              // G: 6 bits
			const unsigned v = ( c >> 5 ) & 63;
			return int( ( v << 2 ) | ( v >> 4 ) );
		}
		const unsigned v = c & 31;         // B: 5 bits
		return int( ( v << 3 ) | ( v >> 2 ) );
	};
	const int b0 = comp( c0 ), b1 = comp( c1 );
	int pal[4] = { b0, b1, 0, 0 };
	if ( c0 > c1 ) {
		pal[2] = ( 2 * b0 + b1 ) / 3;
		pal[3] = ( b0 + 2 * b1 ) / 3;
	} else {
		pal[2] = ( b0 + b1 ) / 2;
		pal[3] = 0;
	}
	const quint32 bits = b[4] | ( quint32( b[5] ) << 8 ) | ( quint32( b[6] ) << 16 ) | ( quint32( b[7] ) << 24 );
	for ( int i = 0; i < 16; i++ )
		out[i] = quint8( pal[( bits >> ( 2 * i ) ) & 3] );
}

/* BC3 ALPHA block (the FIRST 8 bytes of a 16-byte block) -> the A of its 16
 * texels. On the mask sheet that is the GROUND COVER, which nothing in the tree
 * had ever decoded because the AO read only wanted B. */
static void bc3AlphaBlock( const unsigned char * b, quint8 out[16] )
{
	const int a0 = b[0], a1 = b[1];
	int pal[8] = { a0, a1, 0, 0, 0, 0, 0, 0 };
	if ( a0 > a1 ) {
		for ( int i = 0; i < 6; i++ )
			pal[2 + i] = ( ( 6 - i ) * a0 + ( 1 + i ) * a1 ) / 7;
	} else {
		for ( int i = 0; i < 4; i++ )
			pal[2 + i] = ( ( 4 - i ) * a0 + ( 1 + i ) * a1 ) / 5;
		pal[6] = 0;
		pal[7] = 255;
	}
	quint64 bits = 0;
	for ( int i = 0; i < 6; i++ )
		bits |= quint64( b[2 + i] ) << ( 8 * i );
	for ( int i = 0; i < 16; i++ )
		out[i] = quint8( pal[( bits >> ( 3 * i ) ) & 7] );
}

bool LodtSheets::hasRole( int role ) const
{
	if ( !d->open )
		return false;
	for ( int i = 0; i < int( d->h.sheetCount ); i++ )
		if ( d->h.sheets[i].role == quint32( role ) )
			return true;
	return false;
}

int LodtSheets::roleCount( int role ) const
{
	if ( !d->open )
		return 0;
	int n = 0;
	for ( int i = 0; i < int( d->h.sheetCount ); i++ )
		if ( d->h.sheets[i].role == quint32( role ) )
			n++;
	return n;
}

bool LodtSheets::maskAo( int tx, int ty, std::vector<quint8> & out, QString * why )
{
	// the AO read IS the mask sheet's B: one decoder, one code path (hotfix 7b)
	return sheetChannel( LODV_ROLE_MASK, tx, ty, 2, out, why );
}

bool LodtSheets::sheetChannel( int role, int tx, int ty, int channel,
	std::vector<quint8> & out, QString * why, int occurrence )
{
	auto no = [why]( const QString & m ) {
		if ( why )
			*why = m;
		return false;
	};
	out.clear();
	if ( !d->open )
		return no( QStringLiteral( "no sheet container open" ) );
	if ( channel < 0 || channel > 3 )
		return no( QString( "channel %1 is not one of R, G, B, A" ).arg( channel ) );
	if ( occurrence < 0 )
		return no( QString( "occurrence %1 is negative" ).arg( occurrence ) );
	int sheet = -1, seenOfRole = 0;
	for ( int i = 0; i < int( d->h.sheetCount ); i++ )
		if ( d->h.sheets[i].role == quint32( role ) ) {
			if ( seenOfRole == occurrence )
				sheet = i;
			seenOfRole++;
		}
	if ( seenOfRole == 0 )
		return no( QString( "the container carries no sheet with role %1" ).arg( role ) );
	if ( sheet < 0 )
		return no( QString( "the container carries %1 sheet(s) with role %2; there is no "
			"occurrence %3" ).arg( seenOfRole ).arg( role ).arg( occurrence ) );
	if ( tx < 0 || ty < 0 || tx >= int( d->h.tilesX ) || ty >= int( d->h.tilesY ) )
		return no( QString( "tile %1,%2 is outside the grid" ).arg( tx ).arg( ty ) );
	const size_t index = size_t( ty ) * size_t( d->h.tilesX ) + size_t( tx );
	if ( index >= d->table.size() )
		return no( QStringLiteral( "tile index past the table" ) );
	const LodvTileEntry & e = d->table[index];
	if ( !( e.flags & LODV_TILE_PRESENT ) )
		return no( QString( "tile %1,%2 is absent" ).arg( tx ).arg( ty ) );
	const bool cover = ( e.flags & LODV_TILE_COVER ) != 0;
	const quint32 fmt = cover ? d->h.sheets[sheet].dxgiFormatCover
		: d->h.sheets[sheet].dxgiFormat;
	/* THE UNCOMPRESSED ROLE, read first because it is not a block format at
	 * all. Role 7 (horizon) is R8G8B8A8: every channel is a byte a texel and
	 * every channel, alpha included, is a real bin -- there is no "this tile is
	 * BC1 so it has no alpha" case here. */
	const int dimU = int( d->h.storedTexels );
	if ( fmt == LODV_DXGI_R8G8B8A8_UNORM ) {
		quint64 offU = 0;
		for ( int sIdx = 0; sIdx < sheet; sIdx++ )
			for ( int m = 0; m < int( d->h.mipCount ); m++ )
				offU += lodvSheetMipBytes( d->h, sIdx, m, cover );
		const quint32 bytesU = lodvSheetMipBytes( d->h, sheet, 0, cover );
		if ( bytesU != quint32( dimU ) * quint32( dimU ) * 4u )
			return no( QString( "mip 0 of the role-%1 sheet is %2 bytes, not %3x%3x4" )
				.arg( role ).arg( bytesU ).arg( dimU ) );
		QFile fu( d->path );
		if ( !fu.open( QIODevice::ReadOnly ) || !fu.seek( qint64( e.offset + offU ) ) )
			return no( QString( "could not read %1" ).arg( d->path ) );
		const QByteArray raw = fu.read( qint64( bytesU ) );
		if ( raw.size() != qint64( bytesU ) )
			return no( QString( "short read of the role-%1 sheet" ).arg( role ) );
		out.assign( size_t( dimU ) * size_t( dimU ), 0 );
		const unsigned char * rp = reinterpret_cast<const unsigned char *>( raw.constData() );
		for ( size_t i = 0; i < out.size(); i++ )
			out[i] = rp[i * 4 + size_t( channel )];
		return true;
	}
	int blockBytes;
	if ( fmt == LODV_DXGI_BC1_UNORM || fmt == LODV_DXGI_BC1_UNORM_SRGB )
		blockBytes = 8;
	else if ( fmt == LODV_DXGI_BC3_UNORM || fmt == LODV_DXGI_BC3_UNORM_SRGB )
		blockBytes = 16;
	else
		return no( QString( "sheet (role %1) format %2 is not BC1, BC3 or R8G8B8A8" )
			.arg( role ).arg( fmt ) );
	/* A is the BC3 alpha block, and a BC1 tile HAS no alpha. Saying so is the
	 * whole point: on the mask sheet A is the ground cover, and a chunk the bake
	 * wrote as BC1 carries none (root MISTAKES 05:0x -- never a proxy for it). */
	if ( channel == 3 && blockBytes == 8 )
		return no( QString( "tile %1,%2 is BC1 (dxgi %3): it carries no alpha" )
			.arg( tx ).arg( ty ).arg( fmt ) );
	quint64 off = 0;
	for ( int s = 0; s < sheet; s++ )
		for ( int m = 0; m < int( d->h.mipCount ); m++ )
			off += lodvSheetMipBytes( d->h, s, m, cover );
	const quint32 bytes = lodvSheetMipBytes( d->h, sheet, 0, cover );
	/* A HALF-RESOLUTION sheet (descriptor byte 6, mipSkip 1) stores its first
	 * mip at storedTexels / 2. It is decoded at its own size and handed back at
	 * storedTexels by nearest texel, so every caller keeps one grid. */
	const int full = int( d->h.storedTexels );
	const int dim = lodvSheetSide( d->h, sheet, 0 );
	const int blocks = ( dim + 3 ) / 4;
	if ( bytes != quint32( blocks * blocks * blockBytes ) )
		return no( QString( "mip 0 of the role-%1 sheet is %2 bytes, not %3 blocks of %4" )
			.arg( role ).arg( bytes ).arg( blocks * blocks ).arg( blockBytes ) );
	QFile f( d->path );
	if ( !f.open( QIODevice::ReadOnly ) || !f.seek( qint64( e.offset + off ) ) )
		return no( QString( "could not read %1" ).arg( d->path ) );
	const QByteArray payload = f.read( qint64( bytes ) );
	if ( payload.size() != qint64( bytes ) )
		return no( QString( "short read of the role-%1 sheet" ).arg( role ) );
	out.assign( size_t( dim ) * size_t( dim ), 255 );
	const unsigned char * p = reinterpret_cast<const unsigned char *>( payload.constData() );
	for ( int by = 0; by < blocks; by++ ) {
		for ( int bx = 0; bx < blocks; bx++ ) {
			quint8 blk[16];
			const unsigned char * block = p + size_t( by * blocks + bx ) * blockBytes;
			if ( channel == 3 )
				bc3AlphaBlock( block, blk );
			else
				bc1ColourBlock( block + ( blockBytes - 8 ), channel, blk );
			for ( int i = 0; i < 16; i++ ) {
				const int x = bx * 4 + ( i & 3 ), y = by * 4 + ( i >> 2 );
				if ( x < dim && y < dim )
					out[size_t( y ) * size_t( dim ) + size_t( x )] = blk[i];
			}
		}
	}
	if ( dim != full && dim > 0 ) {
		std::vector<quint8> small;
		small.swap( out );
		out.assign( size_t( full ) * size_t( full ), 255 );
		for ( int y = 0; y < full; y++ )
			for ( int x = 0; x < full; x++ )
				out[size_t( y ) * size_t( full ) + size_t( x )] =
					small[size_t( y * dim / full ) * size_t( dim ) + size_t( x * dim / full )];
	}
	return true;
}

bool LodtSheets::tile( int tx, int ty, LodtSheetTile & out, QString * why )
{
	auto no = [why]( const QString & m ) {
		if ( why )
			*why = m;
		return false;
	};
	out = LodtSheetTile();
	if ( !d->open )
		return no( QStringLiteral( "no sheet container open" ) );
	if ( tx < 0 || ty < 0 || tx >= int( d->h.tilesX ) || ty >= int( d->h.tilesY ) )
		return no( QString( "tile %1,%2 is outside the %3x%4 grid" )
			.arg( tx ).arg( ty ).arg( d->h.tilesX ).arg( d->h.tilesY ) );

	const size_t index = size_t( ty ) * size_t( d->h.tilesX ) + size_t( tx );
	if ( index >= d->table.size() )
		return no( QStringLiteral( "tile index past the table" ) );
	const LodvTileEntry & e = d->table[index];
	if ( !( e.flags & LODV_TILE_PRESENT ) )
		return no( QString( "tile %1,%2 is absent" ).arg( tx ).arg( ty ) );

	const bool cover = ( e.flags & LODV_TILE_COVER ) != 0;
	const quint32 raw = lodvTileRawBytes( d->h, cover );
	if ( e.rawBytes != raw || e.storedBytes != raw )
		return no( QString( "tile %1,%2 stores %3 of %4 raw bytes" )
			.arg( tx ).arg( ty ).arg( e.storedBytes ).arg( raw ) );

	// where each sheet's mip 0 starts: sheet-major, mip-minor, tightly packed
	auto sheetMip0Offset = [&]( int sheet ) {
		quint64 off = 0;
		for ( int s = 0; s < sheet; s++ )
			for ( int m = 0; m < int( d->h.mipCount ); m++ )
				off += lodvSheetMipBytes( d->h, s, m, cover );
		return off;
	};

	QFile f( d->path );
	if ( !f.open( QIODevice::ReadOnly ) )
		return no( QString( "could not open %1" ).arg( d->path ) );

	auto unpack = [&]( int sheet, const char * role, QString & name ) -> bool {
		if ( sheet < 0 )
			return true;                     // the container carries no such sheet
		const quint32 bytes = lodvSheetMipBytes( d->h, sheet, 0, cover );
		if ( !bytes )
			return true;
		name = QString( "LODLSheets\\%1.%2.%3.%4.DDS" )
			.arg( d->stem ).arg( tx ).arg( ty ).arg( QLatin1String( role ) );
		const QString disk = d->cache + QStringLiteral( "/Textures/" )
			+ QString( name ).replace( QChar( '\\' ), QChar( '/' ) );
		/* Reused only when it is NEWER than the container it came out of. The
		 * cache directory outlives the session and the name carries no bake
		 * identity, so a bare exists() drew the tiles of an OLDER bake of the
		 * same stem (found 2026-09-18, tiles of 09-12 still in the cache). */
		if ( QFileInfo::exists( disk )
			&& QFileInfo( disk ).lastModified() > QFileInfo( d->path ).lastModified() )
			return true;
		if ( !f.seek( qint64( e.offset + sheetMip0Offset( sheet ) ) ) )
			return false;
		const QByteArray payload = f.read( qint64( bytes ) );
		if ( payload.size() != qint64( bytes ) )
			return false;
		const quint32 fmt = cover ? d->h.sheets[sheet].dxgiFormatCover
			: d->h.sheets[sheet].dxgiFormat;
		QFile o( disk );
		if ( !o.open( QIODevice::WriteOnly ) )
			return false;
		// a half-resolution sheet (mipSkip 1) is its own, smaller picture; the
		// border and content fractions are the same, so the UVs do not move
		const QByteArray head = ddsDx10Header( lodvSheetSide( d->h, sheet, 0 ),
			lodvSheetSide( d->h, sheet, 0 ), fmt, bytes );
		if ( o.write( head ) != head.size() || o.write( payload ) != payload.size() )
			return false;
		return o.flush();
	};

	QString colour, msn, emissive;
	if ( !unpack( d->colourSheet, "c", colour ) )
		return no( QString( "could not unpack tile %1,%2's colour sheet" ).arg( tx ).arg( ty ) );
	if ( !unpack( d->msnSheet, "n", msn ) )
		return no( QString( "could not unpack tile %1,%2's msn sheet" ).arg( tx ).arg( ty ) );
	int emissiveSheet = -1;
	for ( int i = 0; i < int( d->h.sheetCount ); i++ )
		if ( d->h.sheets[i].role == LODV_ROLE_EMISSIVE )
			emissiveSheet = i;
	if ( !unpack( emissiveSheet, "e", emissive ) )
		return no( QString( "could not unpack tile %1,%2's emissive sheet" ).arg( tx ).arg( ty ) );

	out.colour = colour;
	out.msn = msn;
	out.emissive = emissive;
	return !colour.isEmpty();
}
