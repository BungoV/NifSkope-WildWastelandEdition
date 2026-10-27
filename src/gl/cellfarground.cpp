/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "gl/cellfarground.h"
#include "gl/cellfgsched.h"
#include "gl/cellpasstime.h"
#include "gl/cellunits.h"
#include "gl/celluniform.h"	// lane CELLUNI3: the units bound without a unit ask
#include "gl/glscene.h"
#include "gl/renderer.h"
#include "wwswitch.h"

#include "esmdata.h"
#include "lodgen.h"
#include "lodtfile.h"
#include "io/loddecal.h"

#include <QElapsedTimer>
#include <QHash>
#include <QOpenGLContext>
#include <QRegularExpression>
#include <QSettings>
#include <QtDebug>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace
{

constexpr float kCell = 4096.0f, kSpacing = 128.0f, kBin = 512.0f;
constexpr int kLtexSide = 512;	// the terrain preview's LTEX side (ww-terrain-preview)
constexpr int kPageSide[3] = { 256, 512, 1024 };	// red oldpages: FARDRAW1's decal-only pages
constexpr int kAtlasW = 4096, kAtlasMaxH = 8192, kPad = 4;
constexpr GLenum kBc1 = 0x83F0;	// GL_COMPRESSED_RGB_S3TC_DXT1_EXT
constexpr int kSettleFrames = 240;	// frames with no paint before the paint kit (layers, decals) leaves VRAM

struct State
{
	bool loaded = false;
	quint64 liveGen = 0;
	int mode = 0, quality = 1;
	bool pinM = false, pinQ = false, redNoDecals = false, redBaked = false, redOldPages = false, redNoCompress = false;
};

State & st()
{
	static State s;
	if ( !s.loaded || wwSwitchMovedFor( s.liveGen, { "WW_CELL_FARGROUND" } ) ) {
		s.loaded = true;
		s.liveGen = wwSwitchGen();
		const QByteArray m = wwEnv( "WW_CELL_FARGROUND" ).trimmed().toLower();
		const QByteArray q = wwEnv( "WW_CELL_FARGROUND_QUALITY" ).trimmed().toLower();
		const QByteArray red = wwEnv( "WW_CELL_FARGROUND_RED" ).trimmed().toLower();
		const int mi = m == "baked" ? 0 : m == "lookup" ? 1 : m == "cached" ? 2 : -1;
		s.pinM = mi >= 0;
		s.mode = s.pinM ? mi : std::clamp( QSettings().value( QStringLiteral( "WW/CellFarGround" ), 0 ).toInt(), 0, 2 );
		// FARPAGES1: Ultra (2048) is gone -- finer than the screen resolves at ring 0; an old pin reads as High
		const int qi = q == "low" ? 0 : q == "medium" ? 1 : ( q == "high" || q == "ultra" ) ? 2 : -1;
		s.pinQ = qi >= 0;
		s.quality = s.pinQ ? qi : std::clamp( QSettings().value( QStringLiteral( "WW/CellFarGroundQuality" ), 1 ).toInt(), 0, 2 );
		const QList<QByteArray> words = red.split( ',' );
		s.redNoDecals = words.contains( "nodecals" );
		s.redBaked = words.contains( "baked" );
		s.redOldPages = words.contains( "oldpages" );
		s.redNoCompress = words.contains( "nocompress" );
	}
	return s;
}

struct Gpu
{
	QOpenGLContext * ctx = nullptr;
	GLuint grid = 0, height = 0, ltex = 0, dbuf = 0, dtex = 0, atlas = 0, pages = 0, fbo = 0;
	int ready = 0, pageSide = 0;
	qint64 bGrid = 0, bHeight = 0, bLtex = 0, bDec = 0, bAtlas = 0, bPages = 0;
	double stampMs = 0.0;
	// FARPAGES1: the two page pools, the cell table, the paint kit's scratch (a page and its BC1 blocks)
	GLuint pagesN = 0, pagesF = 0, cellTex = 0, scratchN = 0, scratchF = 0, blocks = 0, pbo = 0, fboBc = 0;
	qint64 bPagesN = 0, bPagesF = 0, bCells = 0, bScratch = 0;
	int sideN = 0, sideF = 0, nNear = 0;
	bool compressed = true, decalsOn = true;
	float pixMrad = 0.0f, fovyDeg = 0.0f;
	int viewH = 0;
	FgSched::Sched sched;
	std::vector<quint16> cellTab;
	int idle = 0, fillFrames = 0, painted = 0;
	bool settledSaid = false, busy = false;
};

struct Doc
{
	int first = 0, last = 0;
	int gx0 = 0, gy0 = 0, ncx = 0, ncy = 0, spc = 32, nx = 0, ny = 0;
	int cutX0 = 0, cutY0 = 0, cutX1 = -1, cutY1 = -1;
	float ox = 0.0f, oy = 0.0f;
	std::vector<quint32> grid;	// 4 words a sample: ids 0|1, ids 2|3, weights unorm8 x4, vertex colour RGB8 + page
	std::vector<float> height;
	QStringList ltexPaths;		// a compact layer's diffuse
	int cells = 0, pages = 0;
	std::vector<int> pageCell;	// a page's cell (index into the grid's cell rect)
	// the decals
	QString loddPath, decalWhy;
	std::vector<float> recs;	// 20 floats a record (the terrain preview's layout); the atlas rect filled with the pictures
	std::vector<int> recPiece;
	int binNx = 0, binNy = 0, maxList = 0;
	double meanList = 0.0;
	std::vector<float> binTex, ids;
	// the pictures, decoded on first use
	bool pics = false;
	int ltexMissing = 0, atlasW = 0, atlasH = 0, drop = 0;
	float maxLod = 3.0f;
	double picMs = 0.0, ltexMs = 0.0;
	std::vector<quint32> ltexPix, atlasPix;
	Gpu gpu;
	QString said;
};

QHash<const void *, Doc> & docs()
{
	static QHash<const void *, Doc> d;
	return d;
}

std::vector<Gpu> & graveyard()
{
	static std::vector<Gpu> g;
	return g;
}

void delTex( NifSkopeOpenGLContext::GLFunctions * fn, GLuint & t, qint64 & bytes )
{
	if ( t )
		fn->glDeleteTextures( 1, &t );
	t = 0;
	bytes = 0;
}

void freeDecals( NifSkopeOpenGLContext::GLFunctions * fn, Gpu & g )
{
	delTex( fn, g.dtex, g.bDec );
	if ( g.dbuf )
		fn->glDeleteBuffers( 1, &g.dbuf );
	g.dbuf = 0;
	delTex( fn, g.atlas, g.bAtlas );
}

//! The page paint's inputs and scratch: what a settled set of pages no longer needs.
void freeKit( NifSkopeOpenGLContext::GLFunctions * fn, Gpu & g )
{
	delTex( fn, g.grid, g.bGrid );
	delTex( fn, g.height, g.bHeight );
	delTex( fn, g.ltex, g.bLtex );
	freeDecals( fn, g );
	qint64 none = 0;
	delTex( fn, g.scratchN, none );
	delTex( fn, g.scratchF, none );
	delTex( fn, g.blocks, none );
	g.bScratch = 0;
	if ( g.pbo )
		fn->glDeleteBuffers( 1, &g.pbo );
	g.pbo = 0;
}

void freePools( NifSkopeOpenGLContext::GLFunctions * fn, Gpu & g )
{
	delTex( fn, g.pagesN, g.bPagesN );
	delTex( fn, g.pagesF, g.bPagesF );
	delTex( fn, g.cellTex, g.bCells );
	qint64 none = 0;
	delTex( fn, g.scratchN, none );
	delTex( fn, g.scratchF, none );
	delTex( fn, g.blocks, none );
	g.bScratch = 0;
	if ( g.pbo )
		fn->glDeleteBuffers( 1, &g.pbo );
	g.pbo = 0;
	if ( g.fboBc )
		fn->glDeleteFramebuffers( 1, &g.fboBc );
	g.fboBc = 0;
	g.sideN = g.sideF = 0;
	g.sched.cells.clear();
	g.cellTab.clear();
}

void freeAll( NifSkopeOpenGLContext::GLFunctions * fn, Gpu & g )
{
	delTex( fn, g.grid, g.bGrid );
	delTex( fn, g.height, g.bHeight );
	delTex( fn, g.ltex, g.bLtex );
	freeDecals( fn, g );
	freePools( fn, g );
	delTex( fn, g.pages, g.bPages );
	if ( g.fbo )
		fn->glDeleteFramebuffers( 1, &g.fbo );
	g.fbo = 0;
	g.ready = 0;
	g.pageSide = 0;
}

inline void qrot( const float q[4], const float v[3], float o[3] )
{
	// v + 2 q.xyz x ( q.xyz x v + w v )
	const float t[3] = { q[1] * v[2] - q[2] * v[1] + q[3] * v[0], q[2] * v[0] - q[0] * v[2] + q[3] * v[1],
		q[0] * v[1] - q[1] * v[0] + q[3] * v[2] };
	o[0] = v[0] + 2.0f * ( q[1] * t[2] - q[2] * t[1] );
	o[1] = v[1] + 2.0f * ( q[2] * t[0] - q[0] * t[2] );
	o[2] = v[2] + 2.0f * ( q[0] * t[1] - q[1] * t[0] );
}

/* The grid: the terrain preview's buildGrid sample (terrainpreview.cpp, TERRLIVE1/2) at stride 1 -- the quadrant
 * slots composited base-up, the quadrant cross-fade, the four heaviest renormalised, the 5-5-5 vertex colour. */
void buildGrid( const LodtFile & L, Doc & d, std::vector<int> & remap, QVector<quint32> & forms )
{
	const int spc = d.spc, nl = L.ltexCount();
	const int baseX = ( d.gx0 - L.cellMinX() ) * spc, baseY = ( d.gy0 - L.cellMinY() ) * spc;
	const int lastX = L.cellsX() * spc - 1, lastY = L.cellsY() * spc - 1;
	const size_t N = size_t( d.nx ) * size_t( d.ny );
	d.grid.assign( N * 4, 0u );
	d.height.assign( N, 0.0f );
	remap.assign( size_t( nl ) + 1, -1 );
	auto layerOf = [&]( quint16 id ) -> quint32 {
		if ( id > nl )
			return 0xFFFFu;
		int & r = remap[id];
		if ( r < 0 ) {
			r = forms.size();
			forms.append( id < nl ? L.ltexForm( id ) : ESM_LTEX_ENGINE_DEFAULT );
		}
		return quint32( r );
	};
	const float margin = lodgenBlendMargin();
	const bool edges = lodgenBlendEdges() == 1;
	auto ease = []( float t ) -> float {
		const float u2 = qBound( 0.0f, t, 1.0f );
		return 0.5f * u2 * u2 * u2 * ( u2 * ( u2 * 6.0f - 15.0f ) + 10.0f );
	};
	for ( int j = 0; j < d.ny; j++ ) {
		const int gy = qMin( baseY + j, lastY );
		const int pcy = qMin( j / spc, d.ncy - 1 );
		for ( int i = 0; i < d.nx; i++ ) {
			const int gx = qMin( baseX + i, lastX );
			const int pcx = qMin( i / spc, d.ncx - 1 );
			const size_t s = size_t( j ) * d.nx + i;
			d.height[s] = L.height( gx, gy );
			quint16 id[24];
			float w[24];
			int k = 0;
			auto addQuad = [&]( int sx_, int sy_, float f ) {
				const int qcx = L.cellMinX() + sx_ / spc, qcy = L.cellMinY() + sy_ / spc;
				const int quad = ( ( sy_ % spc ) >= spc / 2 ? 2 : 0 ) | ( ( sx_ % spc ) >= spc / 2 ? 1 : 0 );
				quint16 q[6];
				L.quadrantSlots( qcx, qcy, quad, q );
				quint16 qi[6];
				float qw[6];
				int qk = 0;
				qi[qk] = ( q[5] < nl ) ? q[5] : quint16( nl );	// an empty base = the engine's default land set
				qw[qk++] = 1.0f;
				const quint16 aw = L.alphaWord( sx_, sy_ );
				for ( int layer = 4; layer >= 0; layer-- ) {
					const int a = ( aw >> ( 3 * layer ) ) & 7;
					if ( !a || q[layer] >= nl )
						continue;
					const float t = float( a ) / 7.0f;
					for ( int m = 0; m < qk; m++ )
						qw[m] *= 1.0f - t;
					int m = 0;
					while ( m < qk && qi[m] != q[layer] )
						m++;
					if ( m == qk ) {
						qi[qk] = q[layer];
						qw[qk++] = 0.0f;
					}
					qw[m] += t;
				}
				for ( int u = 0; u < qk; u++ ) {
					int m = 0;
					while ( m < k && id[m] != qi[u] )
						m++;
					if ( m == k ) {
						id[k] = qi[u];
						w[k++] = 0.0f;
					}
					w[m] += f * qw[u];
				}
			};
			const int hq = spc / 2, lxs = gx % hq, lys = gy % hq;
			int nx_ = gx, ny_ = gy;
			float wxN = 0.0f, wyN = 0.0f;
			if ( edges ) {
				const float lxq = float( lxs ) * kSpacing, lyq = float( lys ) * kSpacing;
				if ( lxq < margin && gx - lxs - 1 >= 0 ) {
					nx_ = gx - lxs - 1;
					wxN = ease( 1.0f - lxq / margin );
				} else if ( 2048.0f - lxq < margin && gx + ( hq - lxs ) <= lastX ) {
					nx_ = gx + ( hq - lxs );
					wxN = ease( 1.0f - ( 2048.0f - lxq ) / margin );
				}
				if ( lyq < margin && gy - lys - 1 >= 0 ) {
					ny_ = gy - lys - 1;
					wyN = ease( 1.0f - lyq / margin );
				} else if ( 2048.0f - lyq < margin && gy + ( hq - lys ) <= lastY ) {
					ny_ = gy + ( hq - lys );
					wyN = ease( 1.0f - ( 2048.0f - lyq ) / margin );
				}
			}
			addQuad( gx, gy, ( 1.0f - wxN ) * ( 1.0f - wyN ) );
			if ( wxN > 0.0f )
				addQuad( nx_, gy, wxN * ( 1.0f - wyN ) );
			if ( wyN > 0.0f )
				addQuad( gx, ny_, ( 1.0f - wxN ) * wyN );
			if ( wxN > 0.0f && wyN > 0.0f )
				addQuad( nx_, ny_, wxN * wyN );
			for ( int a = 0; a < k; a++ )
				for ( int b = a + 1; b < k; b++ )
					if ( w[b] > w[a] ) {
						std::swap( w[a], w[b] );
						std::swap( id[a], id[b] );
					}
			float sum = 0.0f;
			for ( int a = 0; a < qMin( k, 4 ); a++ )
				sum += w[a];
			quint32 L4[4] = { 0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu }, W4 = 0;
			for ( int a = 0; a < qMin( k, 4 ); a++ ) {
				L4[a] = layerOf( id[a] );
				W4 |= quint32( qBound( 0, int( w[a] / qMax( sum, 1e-6f ) * 255.0f + 0.5f ), 255 ) ) << ( 8 * a );
			}
			quint32 rgb = 0xFFFFFFu;
			const quint16 cw = L.colourWord( gx, gy );
			if ( cw != 0xFFFFu ) {
				const quint32 r = ( ( cw >> 11 ) & 31 ) * 255 / 31, gg = ( ( cw >> 6 ) & 31 ) * 255 / 31, b = ( cw & 31 ) * 255 / 31;
				rgb = r | ( gg << 8 ) | ( b << 16 );
			}
			int page = 255;
			for ( int p = 0; p < int( d.pageCell.size() ); p++ )
				if ( d.pageCell[size_t( p )] == pcy * d.ncx + pcx ) {
					page = p;
					break;
				}
			d.grid[s * 4 + 0] = L4[0] | ( L4[1] << 16 );
			d.grid[s * 4 + 1] = L4[2] | ( L4[3] << 16 );
			d.grid[s * 4 + 2] = W4;
			d.grid[s * 4 + 3] = rgb | ( quint32( page ) << 24 );
		}
	}
}

//! The ring's decal records (in draw order), their world boxes binned at 512 units.
void buildDecals( const QString & lodlPath, Doc & d, int cx0, int cy0, int cx1, int cy1 )
{
	QString base = lodlPath;
	base.replace( QRegularExpression( QStringLiteral( "\\.lodl$" ), QRegularExpression::CaseInsensitiveOption ), QString() );
	d.loddPath = base + QStringLiteral( ".lodd" );
	LoddFile lodd;
	LodgFile lodg;
	QString why;
	if ( !lodd.open( d.loddPath, &why ) || !lodg.open( base + QStringLiteral( ".lodg" ), &why ) ) {
		d.decalWhy = why;
		return;
	}
	if ( lodg.pieceCount() != lodd.pieceCount() || lodg.loddTableCrc() != lodd.tableCrc() ) {
		d.decalWhy = QStringLiteral( "the .lodg does not pair with the .lodd" );
		return;
	}
	std::vector<char> seen( size_t( lodg.recordCount() ), 0 );
	for ( int cy = 0; cy < d.ncy; cy++ )
		for ( int cx = 0; cx < d.ncx; cx++ ) {
			const int wx = d.gx0 + cx, wy = d.gy0 + cy;
			if ( wx >= cx0 && wx <= cx1 && wy >= cy0 && wy <= cy1 )
				continue;	// the cut: the loaded block draws its own ground
			for ( quint32 r : lodg.cellRecords( wx, wy ) )
				if ( r < seen.size() )
					seen[r] = 1;
		}
	d.binNx = d.ncx * int( kCell / kBin );
	d.binNy = d.ncy * int( kCell / kBin );
	std::vector<std::vector<int>> lists( size_t( d.binNx ) * size_t( d.binNy ) );
	const float pad = lodg.boxPad();
	for ( int r = 0; r < lodg.recordCount(); r++ ) {
		if ( !seen[size_t( r )] )
			continue;
		const LodgRecord & R = lodg.record( r );
		const LoddPiece & P = lodd.piece( R.piece );
		const int k = int( d.recPiece.size() );
		const float rec[20] = { R.pos[0], R.pos[1], R.pos[2], R.scale, R.quat[0], R.quat[1], R.quat[2], R.quat[3],
			P.x0, P.y0, P.x1, P.y1, P.z0 - pad, P.z1 + pad, 4.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
		d.recs.insert( d.recs.end(), rec, rec + 20 );
		d.recPiece.push_back( R.piece );
		float lo[2] = { 1e30f, 1e30f }, hi[2] = { -1e30f, -1e30f };
		for ( int c = 0; c < 8; c++ ) {
			const float l[3] = { ( c & 1 ? P.x1 : P.x0 ) * R.scale, ( c & 2 ? P.y1 : P.y0 ) * R.scale,
				( c & 4 ? P.z1 + pad : P.z0 - pad ) * R.scale };
			float o[3];
			qrot( R.quat, l, o );
			for ( int a = 0; a < 2; a++ ) {
				lo[a] = std::min( lo[a], R.pos[a] + o[a] );
				hi[a] = std::max( hi[a], R.pos[a] + o[a] );
			}
		}
		const int bx0 = std::max( 0, int( std::floor( ( lo[0] - d.ox ) / kBin ) ) );
		const int by0 = std::max( 0, int( std::floor( ( lo[1] - d.oy ) / kBin ) ) );
		const int bx1 = std::min( d.binNx - 1, int( std::floor( ( hi[0] - d.ox ) / kBin ) ) );
		const int by1 = std::min( d.binNy - 1, int( std::floor( ( hi[1] - d.oy ) / kBin ) ) );
		for ( int by = by0; by <= by1; by++ )
			for ( int bx = bx0; bx <= bx1; bx++ )
				lists[size_t( by ) * d.binNx + bx].push_back( k );
	}
	size_t total = 0, used = 0;
	d.binTex.reserve( lists.size() * 4 );
	for ( const std::vector<int> & l : lists ) {
		d.binTex.push_back( float( total ) );
		d.binTex.push_back( float( l.size() ) );
		d.binTex.push_back( 0.0f );
		d.binTex.push_back( 0.0f );
		for ( int k : l )
			d.ids.push_back( float( k ) );
		total += l.size();
		d.maxList = std::max( d.maxList, int( l.size() ) );
		used += l.empty() ? 0 : 1;
	}
	while ( d.ids.size() % 4 )
		d.ids.push_back( 0.0f );
	d.meanList = used ? double( total ) / double( used ) : 0.0;
}

//! The used pieces' colour atlas, decoded once (the first time a live mode draws).
void loadPictures( Doc & d )
{
	QElapsedTimer t;
	t.start();
	d.pics = true;
	if ( !d.recPiece.empty() ) {
		LoddFile lodd;
		QString why;
		if ( !lodd.open( d.loddPath, &why ) ) {
			d.decalWhy = why;
			d.recs.clear();
			d.recPiece.clear();
		} else {
			// the terrain preview's shelf atlas, over the pieces this ring uses
			std::vector<int> pieces( d.recPiece );
			std::sort( pieces.begin(), pieces.end() );
			pieces.erase( std::unique( pieces.begin(), pieces.end() ), pieces.end() );
			const size_t n = pieces.size();
			std::vector<int> order( n ), px( n ), py( n ), pw( n ), ph( n );
			int AH = 0;
			for ( d.drop = 0;; d.drop++ ) {
				for ( size_t i = 0; i < n; i++ ) {
					const LoddPiece & p = lodd.piece( pieces[i] );
					const int m = qMin( d.drop, qMax( 0, p.mips - 1 ) );
					pw[i] = qMax( 1, p.width >> m );
					ph[i] = qMax( 1, p.height >> m );
					order[i] = int( i );
				}
				std::stable_sort( order.begin(), order.end(), [&]( int a, int b ) { return ph[size_t( a )] > ph[size_t( b )]; } );
				int x = 0, y = 0, shelf = 0;
				for ( int i : order ) {
					if ( x + pw[size_t( i )] + kPad > kAtlasW ) {
						x = 0;
						y += shelf + kPad;
						shelf = 0;
					}
					px[size_t( i )] = x;
					py[size_t( i )] = y;
					x += pw[size_t( i )] + kPad;
					shelf = qMax( shelf, ph[size_t( i )] );
				}
				AH = y + shelf;
				if ( AH <= kAtlasMaxH || d.drop >= 4 )
					break;
			}
			AH = qMin( kAtlasMaxH, qMax( 4, ( AH + 3 ) & ~3 ) );
			d.atlasW = kAtlasW;
			d.atlasH = AH;
			d.atlasPix.assign( size_t( kAtlasW ) * AH, 0u );
			QHash<int, int> slot;
			std::vector<quint32> pic;
			for ( size_t i = 0; i < n; i++ ) {
				const LoddPiece & p = lodd.piece( pieces[i] );
				const int m = qMin( d.drop, qMax( 0, p.mips - 1 ) );
				int w = 0, h = 0;
				if ( lodd.picture( pieces[i], false, m, pic, &w, &h, &why ) )
					for ( int y = 0; y < h && py[i] + y < AH; y++ )
						std::memcpy( &d.atlasPix[size_t( py[i] + y ) * kAtlasW + size_t( px[i] )], &pic[size_t( y ) * w],
							size_t( qMin( w, kAtlasW - px[i] ) ) * 4 );
				slot.insert( pieces[i], int( i ) );
			}
			for ( size_t k = 0; k < d.recPiece.size(); k++ ) {
				const size_t i = size_t( slot.value( d.recPiece[k] ) );
				const LoddPiece & p = lodd.piece( d.recPiece[k] );
				float * r = &d.recs[k * 20];
				r[14] = ( p.x1 - p.x0 ) / float( pw[i] );	// local units a texel at the atlas level
				r[16] = ( float( px[i] ) + 0.5f ) / float( kAtlasW );	// half-texel inset: the clamp never reads the neighbour
				r[17] = ( float( py[i] ) + 0.5f ) / float( AH );
				r[18] = ( float( px[i] + pw[i] ) - 0.5f ) / float( kAtlasW );
				r[19] = ( float( py[i] + ph[i] ) - 0.5f ) / float( AH );
			}
			d.maxLod = 3.0f;
		}
	}
	d.picMs = double( t.nsecsElapsed() ) / 1e6;
}

GLuint tex2D( NifSkopeOpenGLContext::GLFunctions * fn, GLenum ifmt, int w, int h, GLenum fmt, GLenum type, const void * data,
	bool linear, bool mips )
{
	GLuint t = 0;
	fn->glGenTextures( 1, &t );
	fn->glBindTexture( GL_TEXTURE_2D, t );
	fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	fn->glTexImage2D( GL_TEXTURE_2D, 0, GLint( ifmt ), w, h, 0, fmt, type, data );
	if ( mips )
		fn->glGenerateMipmap( GL_TEXTURE_2D );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mips ? GL_LINEAR_MIPMAP_LINEAR : linear ? GL_LINEAR : GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	return t;
}

qint64 mipBytes( qint64 b, bool mips )
{
	return mips ? b * 4 / 3 : b;
}

void uploadBase( NifSkopeOpenGLContext::GLFunctions * fn, Doc & d )
{
	Gpu & g = d.gpu;
	if ( !g.grid ) {
		g.grid = tex2D( fn, GL_RGBA32UI, d.nx, d.ny, GL_RGBA_INTEGER, GL_UNSIGNED_INT, d.grid.data(), false, false );
		g.bGrid = qint64( d.grid.size() ) * 4;
	}
	if ( !g.ltex ) {
		const int nl = std::max<int>( 1, int( d.ltexPaths.size() ) );
		fn->glGenTextures( 1, &g.ltex );
		fn->glBindTexture( GL_TEXTURE_2D_ARRAY, g.ltex );
		fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );
		fn->glTexImage3D( GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, kLtexSide, kLtexSide, nl, 0, GL_BGRA, GL_UNSIGNED_BYTE, d.ltexPix.data() );
		fn->glGenerateMipmap( GL_TEXTURE_2D_ARRAY );
		fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT );
		fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT );
		fn->glTexParameterf( GL_TEXTURE_2D_ARRAY, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY */, 8.0f );
		g.bLtex = mipBytes( qint64( kLtexSide ) * kLtexSide * 4 * nl, true );
	}
}

void uploadDecals( NifSkopeOpenGLContext::GLFunctions * fn, Doc & d )
{
	Gpu & g = d.gpu;
	if ( g.dtex )
		return;
	std::vector<float> buf( d.recs );
	buf.insert( buf.end(), d.binTex.begin(), d.binTex.end() );
	buf.insert( buf.end(), d.ids.begin(), d.ids.end() );
	if ( buf.empty() )
		buf.assign( 4, 0.0f );
	fn->glGenBuffers( 1, &g.dbuf );
	fn->glBindBuffer( GL_TEXTURE_BUFFER, g.dbuf );
	fn->glBufferData( GL_TEXTURE_BUFFER, GLsizeiptr( buf.size() * 4 ), buf.data(), GL_STATIC_DRAW );
	fn->glGenTextures( 1, &g.dtex );
	fn->glBindTexture( GL_TEXTURE_BUFFER, g.dtex );
	fn->glTexBuffer( GL_TEXTURE_BUFFER, GL_RGBA32F, g.dbuf );
	fn->glBindTexture( GL_TEXTURE_BUFFER, 0 );
	fn->glBindBuffer( GL_TEXTURE_BUFFER, 0 );
	g.bDec = qint64( buf.size() ) * 4;
	if ( d.atlasW > 0 ) {
		g.atlas = tex2D( fn, GL_RGBA8, d.atlasW, d.atlasH, GL_BGRA, GL_UNSIGNED_BYTE, d.atlasPix.data(), true, true );
		g.bAtlas = mipBytes( qint64( d.atlasW ) * d.atlasH * 4, true );
	} else {
		const quint32 none = 0u;
		g.atlas = tex2D( fn, GL_RGBA8, 1, 1, GL_BGRA, GL_UNSIGNED_BYTE, &none, true, false );
		g.bAtlas = 4;
	}
}

QString mb( qint64 b )
{
	return QString::number( double( b ) / ( 1024.0 * 1024.0 ), 'f', 2 );
}

void bindUnit( NifSkopeOpenGLContext::GLFunctions * fn, int unit, GLenum target, GLuint t )
{
	fn->glActiveTexture( GLenum( GL_TEXTURE0 + unit ) );
	fn->glBindTexture( target, t );
}

// the decal uniforms the lookup twin and the stamp share
void decalUniforms( NifSkopeOpenGLContext::Program * p, const Doc & d )
{
	const int nb = d.binNx * d.binNy;
	const int binOff = int( d.recs.size() / 4 ), idOff = binOff + nb;
	p->uni1i( "fgDecal", WwCellUnit::FarGroundDecal );
	p->uni1i( "fgAtlas", WwCellUnit::FarGroundAtlas );
	p->uni4f( "fgBinMap", FloatVector4( d.ox, d.oy, 1.0f / kBin, d.maxLod ) );
	p->uni4f( "fgBinN", FloatVector4( float( d.binNx ), float( d.binNy ), float( binOff ), float( idOff ) ) );
}

void baseUniforms( NifSkopeOpenGLContext::Program * p, const Doc & d )
{
	p->uni1i( "fgGrid", WwCellUnit::FarGroundGrid );
	p->uni1i( "fgLtex", WwCellUnit::FarGroundLtex );
	p->uni4f( "fgGridMap", FloatVector4( d.ox, d.oy, 1.0f / kSpacing, lodgenLandTiling() ) );
	p->uni2f( "fgGridN", float( d.nx ), float( d.ny ) );
}

//! Stamps every page: the ring's decals over each cell, premultiplied (ground x (1 - a) + rgb is the composite).
bool stamp( Renderer * r, Doc & d, int side, QString * why )
{
	auto fn = r->fn;
	Gpu & g = d.gpu;
	delTex( fn, g.pages, g.bPages );
	fn->glGenTextures( 1, &g.pages );
	fn->glBindTexture( GL_TEXTURE_2D_ARRAY, g.pages );
	fn->glTexImage3D( GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, side, side, d.pages, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
	fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	fn->glTexParameterf( GL_TEXTURE_2D_ARRAY, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY */, 8.0f );
	g.bPages = mipBytes( qint64( side ) * side * 4 * d.pages, true );
	if ( !g.height ) {
		g.height = tex2D( fn, GL_R32F, d.nx, d.ny, GL_RED, GL_FLOAT, d.height.data(), true, false );
		g.bHeight = qint64( d.height.size() ) * 4;
	}
	uploadDecals( fn, d );
	if ( !g.fbo )
		fn->glGenFramebuffers( 1, &g.fbo );

	GLint prevDraw = 0, prevRead = 0, prevActive = 0, vp[4], sb[4];
	fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw );
	fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prevRead );
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glGetIntegerv( GL_VIEWPORT, vp );
	fn->glGetIntegerv( GL_SCISSOR_BOX, sb );
	const bool wasDepth = fn->glIsEnabled( GL_DEPTH_TEST ), wasBlend = fn->glIsEnabled( GL_BLEND );
	const bool wasCull = fn->glIsEnabled( GL_CULL_FACE ), wasScissor = fn->glIsEnabled( GL_SCISSOR_TEST );
	const bool wasStencil = fn->glIsEnabled( GL_STENCIL_TEST );
	GLboolean depthMask = GL_TRUE, cmask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
	fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
	fn->glGetBooleanv( GL_COLOR_WRITEMASK, cmask );

	bool ok = true;
	NifSkopeOpenGLContext::Program * p = r->useProgram( "cell_fgstamp.prog" );
	if ( !p ) {
		*why = QStringLiteral( "cell_fgstamp.prog did not link" );
		ok = false;
	} else {
		QElapsedTimer t;
		t.start();
		fn->glFinish();
		fn->glDisable( GL_DEPTH_TEST );
		fn->glDisable( GL_BLEND );
		fn->glDisable( GL_CULL_FACE );
		fn->glDisable( GL_SCISSOR_TEST );
		fn->glDisable( GL_STENCIL_TEST );
		fn->glDepthMask( GL_FALSE );
		fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		bindUnit( fn, WwCellUnit::FarGroundDecal, GL_TEXTURE_BUFFER, g.dtex );
		bindUnit( fn, WwCellUnit::FarGroundAtlas, GL_TEXTURE_2D, g.atlas );
		bindUnit( fn, WwCellUnit::FarGroundHeight, GL_TEXTURE_2D, g.height );
		decalUniforms( p, d );
		baseUniforms( p, d );
		p->uni1i( "fgHeight", WwCellUnit::FarGroundHeight );
		p->uni1f( "fgStampSide", float( side ) );
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.fbo );
		fn->glViewport( 0, 0, side, side );
		static const float q[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
		static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
		for ( int pg = 0; pg < d.pages && ok; pg++ ) {
			fn->glFramebufferTextureLayer( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, g.pages, 0, pg );
			const GLenum one = GL_COLOR_ATTACHMENT0;
			fn->glDrawBuffers( 1, &one );
			if ( fn->glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
				*why = QStringLiteral( "the page target is incomplete" );
				ok = false;
				break;
			}
			const int c = d.pageCell[size_t( pg )];
			p->uni2f( "fgStampCell", float( d.gx0 + c % d.ncx ) * kCell, float( d.gy0 + c / d.ncx ) * kCell );
			const float * attrs = q;
			r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
		}
		r->stopProgram();
		fn->glBindTexture( GL_TEXTURE_2D_ARRAY, g.pages );
		fn->glGenerateMipmap( GL_TEXTURE_2D_ARRAY );
		fn->glFinish();
		g.stampMs = double( t.nsecsElapsed() ) / 1e6;
	}
	for ( int u : { int( WwCellUnit::FarGroundDecal ), int( WwCellUnit::FarGroundAtlas ), int( WwCellUnit::FarGroundHeight ) } ) {
		fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
		fn->glBindTexture( GL_TEXTURE_BUFFER, 0 );
	}
	fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( prevDraw ) );
	fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prevRead ) );
	fn->glActiveTexture( GLenum( prevActive ) );
	fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
	fn->glScissor( sb[0], sb[1], sb[2], sb[3] );
	( wasDepth ? fn->glEnable( GL_DEPTH_TEST ) : fn->glDisable( GL_DEPTH_TEST ) );
	( wasBlend ? fn->glEnable( GL_BLEND ) : fn->glDisable( GL_BLEND ) );
	( wasCull ? fn->glEnable( GL_CULL_FACE ) : fn->glDisable( GL_CULL_FACE ) );
	( wasScissor ? fn->glEnable( GL_SCISSOR_TEST ) : fn->glDisable( GL_SCISSOR_TEST ) );
	( wasStencil ? fn->glEnable( GL_STENCIL_TEST ) : fn->glDisable( GL_STENCIL_TEST ) );
	fn->glDepthMask( depthMask );
	fn->glColorMask( cmask[0], cmask[1], cmask[2], cmask[3] );
	// VRAM: the cached option keeps only its pages after the stamp
	freeDecals( fn, g );
	delTex( fn, g.height, g.bHeight );
	return ok;
}

//! Saves and restores the GL state a page paint touches (the stamp's list, plus the pixel buffers and unit 0).
struct GlSave
{
	NifSkopeOpenGLContext::GLFunctions * fn;
	GLint draw = 0, read = 0, active = 0, pack = 0, unpack = 0, unit0 = 0, vp[4] = {}, sb[4] = {};
	bool depth, blend, cull, scissor, stencil;
	GLboolean depthMask = GL_TRUE, cmask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
	explicit GlSave( NifSkopeOpenGLContext::GLFunctions * f ) : fn( f )
	{
		fn->glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &draw );
		fn->glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &read );
		fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &active );
		fn->glGetIntegerv( GL_PIXEL_PACK_BUFFER_BINDING, &pack );
		fn->glGetIntegerv( GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack );
		fn->glActiveTexture( GL_TEXTURE0 );
		fn->glGetIntegerv( GL_TEXTURE_BINDING_2D, &unit0 );
		fn->glGetIntegerv( GL_VIEWPORT, vp );
		fn->glGetIntegerv( GL_SCISSOR_BOX, sb );
		depth = fn->glIsEnabled( GL_DEPTH_TEST );
		blend = fn->glIsEnabled( GL_BLEND );
		cull = fn->glIsEnabled( GL_CULL_FACE );
		scissor = fn->glIsEnabled( GL_SCISSOR_TEST );
		stencil = fn->glIsEnabled( GL_STENCIL_TEST );
		fn->glGetBooleanv( GL_DEPTH_WRITEMASK, &depthMask );
		fn->glGetBooleanv( GL_COLOR_WRITEMASK, cmask );
		fn->glDisable( GL_DEPTH_TEST );
		fn->glDisable( GL_BLEND );
		fn->glDisable( GL_CULL_FACE );
		fn->glDisable( GL_SCISSOR_TEST );
		fn->glDisable( GL_STENCIL_TEST );
		fn->glDepthMask( GL_FALSE );
		fn->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
	}
	~GlSave()
	{
		for ( int u : { int( WwCellUnit::FarGroundGrid ), int( WwCellUnit::FarGroundLtex ), int( WwCellUnit::FarGroundDecal ),
				int( WwCellUnit::FarGroundAtlas ), int( WwCellUnit::FarGroundHeight ) } ) {
			fn->glActiveTexture( GLenum( GL_TEXTURE0 + u ) );
			fn->glBindTexture( GL_TEXTURE_2D, 0 );
			fn->glBindTexture( GL_TEXTURE_2D_ARRAY, 0 );
			fn->glBindTexture( GL_TEXTURE_BUFFER, 0 );
		}
		fn->glActiveTexture( GL_TEXTURE0 );
		fn->glBindTexture( GL_TEXTURE_2D, GLuint( unit0 ) );
		fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, GLuint( pack ) );
		fn->glBindBuffer( GL_PIXEL_UNPACK_BUFFER, GLuint( unpack ) );
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, GLuint( draw ) );
		fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( read ) );
		fn->glActiveTexture( GLenum( active ) );
		fn->glViewport( vp[0], vp[1], vp[2], vp[3] );
		fn->glScissor( sb[0], sb[1], sb[2], sb[3] );
		( depth ? fn->glEnable( GL_DEPTH_TEST ) : fn->glDisable( GL_DEPTH_TEST ) );
		( blend ? fn->glEnable( GL_BLEND ) : fn->glDisable( GL_BLEND ) );
		( cull ? fn->glEnable( GL_CULL_FACE ) : fn->glDisable( GL_CULL_FACE ) );
		( scissor ? fn->glEnable( GL_SCISSOR_TEST ) : fn->glDisable( GL_SCISSOR_TEST ) );
		( stencil ? fn->glEnable( GL_STENCIL_TEST ) : fn->glDisable( GL_STENCIL_TEST ) );
		fn->glDepthMask( depthMask );
		fn->glColorMask( cmask[0], cmask[1], cmask[2], cmask[3] );
	}
};

//! A page pool: `layers` pages of `side` with mips down to 4x4, BC1 or (red nocompress) RGBA8.
GLuint allocPool( NifSkopeOpenGLContext::GLFunctions * fn, int side, int layers, bool compressed, qint64 & bytes )
{
	GLuint t = 0;
	fn->glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
	fn->glGenTextures( 1, &t );
	fn->glBindTexture( GL_TEXTURE_2D_ARRAY, t );
	const int nl = FgSched::levels( side );
	for ( int L = 0; L < nl; L++ ) {
		const int s = side >> L;
		if ( compressed )
			fn->glCompressedTexImage3D( GL_TEXTURE_2D_ARRAY, L, kBc1, s, s, layers, 0, ( s / 4 ) * ( s / 4 ) * 8 * layers, nullptr );
		else
			fn->glTexImage3D( GL_TEXTURE_2D_ARRAY, L, GL_RGBA8, s, s, layers, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
	}
	fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0 );
	fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, nl - 1 );
	fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	fn->glTexParameteri( GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	fn->glTexParameterf( GL_TEXTURE_2D_ARRAY, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY */, 8.0f );
	fn->glBindTexture( GL_TEXTURE_2D_ARRAY, 0 );
	bytes = FgSched::pageBytes( side, compressed ) * layers;
	return t;
}

//! Paints this frame's pages: the cell's layers + decals into a scratch page, its mips, then BC1 blocks (GPU) through
//! a pixel buffer into the pool's layer, or (red nocompress) the levels copied as they are.
bool paintJobs( Renderer * r, Doc & d, const std::vector<FgSched::Job> & jobs, QString * why )
{
	auto fn = r->fn;
	Gpu & g = d.gpu;
	if ( !d.pics )
		loadPictures( d );
	uploadBase( fn, d );
	uploadDecals( fn, d );
	if ( !g.height ) {
		g.height = tex2D( fn, GL_R32F, d.nx, d.ny, GL_RED, GL_FLOAT, d.height.data(), true, false );
		g.bHeight = qint64( d.height.size() ) * 4;
	}
	if ( !g.fbo )
		fn->glGenFramebuffers( 1, &g.fbo );
	if ( !g.fboBc )
		fn->glGenFramebuffers( 1, &g.fboBc );
	auto scratch = [&]( GLuint & t, int side ) {
		if ( t )
			return;
		t = tex2D( fn, GL_RGBA8, side, side, GL_RGBA, GL_UNSIGNED_BYTE, nullptr, false, true );
		fn->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, FgSched::levels( side ) - 1 );
		g.bScratch += mipBytes( qint64( side ) * side * 4, true );
	};
	scratch( g.scratchN, g.sideN );
	scratch( g.scratchF, g.sideF );
	const int bw0 = g.sideN / 4;
	if ( !g.blocks ) {
		g.blocks = tex2D( fn, GL_RG32UI, bw0, bw0, GL_RG_INTEGER, GL_UNSIGNED_INT, nullptr, false, false );
		g.bScratch += qint64( bw0 ) * bw0 * 8;
	}
	if ( !g.pbo ) {
		fn->glGenBuffers( 1, &g.pbo );
		fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, g.pbo );
		fn->glBufferData( GL_PIXEL_PACK_BUFFER, GLsizeiptr( bw0 ) * bw0 * 8, nullptr, GL_STREAM_COPY );
		fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
		g.bScratch += qint64( bw0 ) * bw0 * 8;
	}

	GlSave save( fn );
	static const float q[12] = { -1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0 };
	static const std::uint16_t idx[6] = { 0, 1, 2, 2, 1, 3 };
	const float * attrs = q;
	const GLenum one = GL_COLOR_ATTACHMENT0;
	for ( const FgSched::Job & j : jobs ) {
		const int side = j.pool == 1 ? g.sideN : g.sideF;
		const GLuint scr = j.pool == 1 ? g.scratchN : g.scratchF;
		fn->glActiveTexture( GL_TEXTURE0 );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );	// the target is never also a bound source
		NifSkopeOpenGLContext::Program * p = r->useProgram( "cell_fgpage.prog" );
		if ( !p ) {
			*why = QStringLiteral( "cell_fgpage.prog did not link" );
			return false;
		}
		bindUnit( fn, WwCellUnit::FarGroundGrid, GL_TEXTURE_2D, g.grid );
		bindUnit( fn, WwCellUnit::FarGroundLtex, GL_TEXTURE_2D_ARRAY, g.ltex );
		bindUnit( fn, WwCellUnit::FarGroundDecal, GL_TEXTURE_BUFFER, g.dtex );
		bindUnit( fn, WwCellUnit::FarGroundAtlas, GL_TEXTURE_2D, g.atlas );
		bindUnit( fn, WwCellUnit::FarGroundHeight, GL_TEXTURE_2D, g.height );
		baseUniforms( p, d );
		decalUniforms( p, d );
		p->uni1i( "fgHeight", WwCellUnit::FarGroundHeight );
		p->uni1f( "fgStampSide", float( side ) );
		p->uni1i( "fgPageDecals", g.decalsOn ? 1 : 0 );
		const FgSched::Cell & c = g.sched.cells[size_t( j.cell )];
		p->uni2f( "fgStampCell", float( c.wx ) * kCell, float( c.wy ) * kCell );
		fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.fbo );
		fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, scr, 0 );
		fn->glDrawBuffers( 1, &one );
		if ( fn->glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
			*why = QStringLiteral( "the page target is incomplete" );
			return false;
		}
		fn->glViewport( 0, 0, side, side );
		r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
		fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0 );
		fn->glActiveTexture( GL_TEXTURE0 );
		fn->glBindTexture( GL_TEXTURE_2D, scr );
		fn->glGenerateMipmap( GL_TEXTURE_2D );

		const int layer = g.sched.commit( j );
		if ( layer < 0 )
			continue;
		const GLuint dst = j.pool == 1 ? g.pagesN : g.pagesF;
		const int nl = FgSched::levels( side );
		if ( g.compressed ) {
			NifSkopeOpenGLContext::Program * e = r->useProgram( "cell_fgbc1.prog" );
			if ( !e ) {
				*why = QStringLiteral( "cell_fgbc1.prog did not link" );
				return false;
			}
			fn->glActiveTexture( GL_TEXTURE0 );
			fn->glBindTexture( GL_TEXTURE_2D, scr );
			e->uni1i( "fgSrc", 0 );
			e->uni1i( "fgBc1Red", 0 );
			fn->glBindFramebuffer( GL_DRAW_FRAMEBUFFER, g.fboBc );
			fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.blocks, 0 );
			fn->glDrawBuffers( 1, &one );
			fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.fboBc );
			fn->glReadBuffer( GL_COLOR_ATTACHMENT0 );
			for ( int L = 0; L < nl; L++ ) {
				const int s = side >> L, bw = s / 4;
				e->uni1i( "fgSrcLevel", L );
				fn->glViewport( 0, 0, bw, bw );
				r->drawShape( 4, 3, 6, GL_TRIANGLES, GL_UNSIGNED_SHORT, &attrs, idx );
				fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, g.pbo );
				fn->glReadPixels( 0, 0, bw, bw, GL_RG_INTEGER, GL_UNSIGNED_INT, nullptr );
				fn->glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
				fn->glBindBuffer( GL_PIXEL_UNPACK_BUFFER, g.pbo );
				fn->glBindTexture( GL_TEXTURE_2D_ARRAY, dst );
				fn->glCompressedTexSubImage3D( GL_TEXTURE_2D_ARRAY, L, 0, 0, layer, s, s, 1, kBc1, bw * bw * 8, nullptr );
				fn->glBindTexture( GL_TEXTURE_2D_ARRAY, 0 );
				fn->glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
			}
			fn->glFramebufferTexture2D( GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0 );
		} else {
			fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, g.fboBc );
			fn->glBindTexture( GL_TEXTURE_2D_ARRAY, dst );
			for ( int L = 0; L < nl; L++ ) {
				const int s = side >> L;
				fn->glFramebufferTexture2D( GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, scr, L );
				fn->glReadBuffer( GL_COLOR_ATTACHMENT0 );
				fn->glCopyTexSubImage3D( GL_TEXTURE_2D_ARRAY, L, 0, 0, layer, 0, 0, s, s );
			}
			fn->glFramebufferTexture2D( GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0 );
			fn->glBindTexture( GL_TEXTURE_2D_ARRAY, 0 );
		}
		g.painted++;
	}
	r->stopProgram();
	return true;
}

//! The cell table the draw reads: per ring cell pool, layer, fade (RGBA16UI; uploaded when it changes).
void uploadCells( NifSkopeOpenGLContext::GLFunctions * fn, Doc & d )
{
	Gpu & g = d.gpu;
	std::vector<quint16> t( size_t( d.ncx ) * size_t( d.ncy ) * 4, 0 );
	for ( const FgSched::Cell & c : g.sched.cells ) {
		const size_t k = ( size_t( c.wy - d.gy0 ) * size_t( d.ncx ) + size_t( c.wx - d.gx0 ) ) * 4;
		t[k] = quint16( c.pool );
		t[k + 1] = quint16( std::max( 0, c.layer ) );
		t[k + 2] = quint16( c.pool ? c.fade : 0 );
	}
	if ( g.cellTex && t == g.cellTab )
		return;
	if ( !g.cellTex ) {
		g.cellTex = tex2D( fn, GL_RGBA16UI, d.ncx, d.ncy, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, t.data(), false, false );
		g.bCells = qint64( t.size() ) * 2;
	} else {
		fn->glBindTexture( GL_TEXTURE_2D, g.cellTex );
		fn->glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
		fn->glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, d.ncx, d.ncy, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, t.data() );
		fn->glBindTexture( GL_TEXTURE_2D, 0 );
	}
	g.cellTab.swap( t );
}

//! The cached pages a frame (FARPAGES1): size the pools by the measured pixel, plan the budget, paint, fade.
bool cachedPass( Scene * scene, Doc & d, const State & s, QString * extra )
{
	Renderer * r = scene->renderer;
	auto fn = r->fn;
	Gpu & g = d.gpu;
	if ( g.sched.cells.empty() )
		for ( int cy = 0; cy < d.ncy; cy++ )
			for ( int cx = 0; cx < d.ncx; cx++ ) {
				FgSched::Cell c;
				c.wx = d.gx0 + cx;
				c.wy = d.gy0 + cy;
				if ( c.wx >= d.cutX0 && c.wx <= d.cutX1 && c.wy >= d.cutY0 && c.wy <= d.cutY1 )
					continue;
				g.sched.cells.push_back( c );
			}
	// the camera (sunshadow.cpp's form: posView = R (sc Q) + t) and the pixel at the screen's centre
	const Transform & vt = scene->view;
	const float sc = vt.scale != 0.0f ? vt.scale : 1.0f;
	FgSched::View v;
	float C[3], F[3];
	for ( int j = 0; j < 3; j++ ) {
		float c = 0;
		for ( int k = 0; k < 3; k++ )
			c -= vt.rotation( k, j ) * vt.translation[k];
		C[j] = c / sc;
		F[j] = -vt.rotation( 2, j );
	}
	v.x = C[0];
	v.y = C[1];
	v.fx = F[0];
	v.fy = F[1];
	const auto & P = r->globalUniforms->projectionMatrix;
	const bool persp = P[3][3] != 1.0f && P[1][1] > 0.0f && P[0][0] > 0.0f;
	const float tanY = persp ? 1.0f / P[1][1] : std::tan( 0.5236f ), tanX = persp ? 1.0f / P[0][0] : tanY * 1.87f;
	const int vh = r->globalUniforms->viewportDimensions[3];
	const int H = vh >= 64 ? vh : 1026;
	const float pix = 2.0f * tanY / float( H );
	const int qu = s.quality;
	v.halfFov = std::atan( tanX );
	v.nearDist = FgSched::nearDistance( qu, pix );
	v.sideN = FgSched::kNearSide[qu];
	v.sideF = FgSched::kFarSide[qu];
	v.budget = 4LL * 512 * 512;
	const float ccx = float( d.cutX0 + d.cutX1 + 1 ) * 0.5f * kCell, ccy = float( d.cutY0 + d.cutY1 + 1 ) * 0.5f * kCell;
	const int nNear = FgSched::nearCount( g.sched.cells, ccx, ccy, v.nearDist );
	const bool comp = !s.redNoCompress;
	if ( !g.pagesN || g.sideN != v.sideN || g.nNear != nNear || g.compressed != comp ) {
		qint64 none = 0;
		delTex( fn, g.pagesN, g.bPagesN );
		delTex( fn, g.pagesF, g.bPagesF );
		delTex( fn, g.scratchN, none );
		delTex( fn, g.scratchF, none );
		delTex( fn, g.blocks, none );
		g.bScratch = 0;
		if ( g.pbo )
			fn->glDeleteBuffers( 1, &g.pbo );
		g.pbo = 0;
		const int n = int( g.sched.cells.size() );
		int capN = 0, capF = 0;
		FgSched::poolCaps( n, nNear, qu, capN, capF );
		g.sideN = v.sideN;
		g.sideF = v.sideF;
		g.nNear = nNear;
		g.compressed = comp;
		g.pagesN = allocPool( fn, g.sideN, capN, comp, g.bPagesN );
		g.pagesF = allocPool( fn, g.sideF, capF, comp, g.bPagesF );
		g.sched.reset( capN, capF );
		g.fillFrames = 0;
		g.painted = 0;
		g.settledSaid = false;
	}
	if ( g.decalsOn != !s.redNoDecals ) {
		g.decalsOn = !s.redNoDecals;
		g.sched.reset( g.sched.capN, g.sched.capF );	// the pages hold the decals: paint them again
		g.fillFrames = 0;
		g.painted = 0;
		g.settledSaid = false;
	}
	g.pixMrad = pix * 1000.0f;
	g.fovyDeg = 2.0f * std::atan( tanY ) * 57.29578f;
	g.viewH = H;
	const std::vector<FgSched::Job> jobs = g.sched.plan( v );
	if ( !jobs.empty() ) {
		PassTime::subBegin( PassTime::FarGroundPaint );
		QString why;
		const bool ok = paintJobs( r, d, jobs, &why );
		PassTime::subEnd( PassTime::FarGroundPaint );
		if ( !ok ) {
			*extra = QStringLiteral( "; cached REFUSED (%1)" ).arg( why );
			return false;
		}
		g.idle = 0;
	} else if ( g.idle < kSettleFrames && ++g.idle == kSettleFrames ) {
		freeKit( fn, g );	// VRAM: settled pages need neither the layers nor the decals
	}
	const bool fading = g.sched.tick();
	if ( fading )
		g.fillFrames++;
	else if ( !g.settledSaid ) {
		g.settledSaid = true;
		qInfo().noquote() << QStringLiteral( "far ground pages: settled after %1 frames, %2 pages painted (budget %3 texels a frame)" )
			.arg( g.fillFrames ).arg( g.painted ).arg( v.budget );
	}
	g.busy = fading || g.idle < kSettleFrames;
	uploadCells( fn, d );
	*extra = QStringLiteral( "; pages near %1 x %2 px + far %3 x %4 px = %5 MB %6 (cell table %7 KB); pixel %8 mrad (fovy %9, %10 px), "
		"near under %11 units: %12 cells for the camera at the cut centre; budget %13 texels a frame" )
		.arg( g.sched.capN ).arg( g.sideN ).arg( g.sched.capF ).arg( g.sideF ).arg( mb( g.bPagesN + g.bPagesF ) )
		.arg( g.compressed ? QStringLiteral( "BC1" ) : QStringLiteral( "RGBA8 (red nocompress)" ) ).arg( double( g.bCells ) / 1024.0, 0, 'f', 1 )
		.arg( double( g.pixMrad ), 0, 'f', 3 ).arg( double( g.fovyDeg ), 0, 'f', 1 ).arg( g.viewH )
		.arg( double( v.nearDist ), 0, 'f', 0 ).arg( g.nNear ).arg( v.budget );
	return true;
}

void status( Doc & d, int mode, const QString & extra )
{
	static const char * const kName[3] = { "baked", "lookup", "cached" };
	const Gpu & g = d.gpu;
	const qint64 total = g.bGrid + g.bLtex + g.bDec + g.bAtlas + g.bHeight + g.bPages + g.bPagesN + g.bPagesF + g.bCells + g.bScratch;
	const QString line = QStringLiteral( "far ground: %1 -- ring 0 %2 cells (%3 pages), grid %4x%5, ltex %6 layers x %7 (%8 missing), "
		"decals %9 records over %10x%11 bins (list max %12, mean %13), atlas %14x%15 drop %16; "
		"VRAM grid %17 + ltex %18 + decals %19 + atlas %20 + pages %21 = %22 MB; ltex %23 ms + atlas %24 ms, stamp %25 ms (%26 px a page)%27" )
		.arg( QLatin1String( kName[mode] ) ).arg( d.cells ).arg( d.pages ).arg( d.nx ).arg( d.ny )
		.arg( d.ltexPaths.size() ).arg( kLtexSide ).arg( d.ltexMissing )
		.arg( d.recPiece.size() ).arg( d.binNx ).arg( d.binNy ).arg( d.maxList ).arg( d.meanList, 0, 'f', 1 )
		.arg( d.atlasW ).arg( d.atlasH ).arg( d.drop )
		.arg( mb( g.bGrid ), mb( g.bLtex ), mb( g.bDec ), mb( g.bAtlas ), mb( g.bPages ), mb( total ) )
		.arg( d.ltexMs, 0, 'f', 1 ).arg( d.picMs, 0, 'f', 1 ).arg( g.stampMs, 0, 'f', 1 ).arg( g.pageSide ).arg( extra );
	if ( line != d.said ) {
		d.said = line;
		qInfo().noquote() << line;
	}
}

} // namespace

namespace CellFarGround
{

int mode()
{
	return st().mode;
}

void setMode( int m )
{
	State & s = st();
	if ( s.pinM )
		return;
	s.mode = std::clamp( m, 0, 2 );
	QSettings().setValue( QStringLiteral( "WW/CellFarGround" ), s.mode );
}

bool modePinned()
{
	return st().pinM;
}

int quality()
{
	return st().quality;
}

void setQuality( int q )
{
	State & s = st();
	if ( s.pinQ )
		return;
	s.quality = std::clamp( q, 0, 2 );
	QSettings().setValue( QStringLiteral( "WW/CellFarGroundQuality" ), s.quality );
}

bool qualityPinned()
{
	return st().pinQ;
}

void prepare( const void * nif, const EsmWorld & world, const QString & lodlPath, int blockFirst, int blockLast,
	int rx0, int ry0, int rx1, int ry1, int cx0, int cy0, int cx1, int cy1, QStringList * notes )
{
	forget( nif );
	auto note = [notes]( const QString & l ) {
		if ( notes )
			*notes << l;
	};
	if ( blockLast <= blockFirst || rx1 < rx0 || ry1 < ry0 )
		return;
	QElapsedTimer t;
	t.start();
	LodtFile L;
	QString why;
	if ( !L.open( lodlPath, &why ) ) {
		note( QStringLiteral( "far ground: refused -- %1" ).arg( why ) );
		return;
	}
	L.setBlockCacheSize( 512 );
	Doc d;
	d.first = blockFirst;
	d.last = blockLast;
	d.cutX0 = cx0;
	d.cutY0 = cy0;
	d.cutX1 = cx1;
	d.cutY1 = cy1;
	d.gx0 = std::max( rx0, L.cellMinX() );
	d.gy0 = std::max( ry0, L.cellMinY() );
	const int gx1 = std::min( rx1, L.cellMaxX() ), gy1 = std::min( ry1, L.cellMaxY() );
	if ( gx1 < d.gx0 || gy1 < d.gy0 ) {
		note( QStringLiteral( "far ground: refused -- ring 0 misses the .lodl" ) );
		return;
	}
	d.ncx = gx1 - d.gx0 + 1;
	d.ncy = gy1 - d.gy0 + 1;
	d.spc = L.samplesPerCell();
	d.nx = d.ncx * d.spc + 1;
	d.ny = d.ncy * d.spc + 1;
	d.ox = float( d.gx0 ) * kCell;
	d.oy = float( d.gy0 ) * kCell;
	for ( int cy = 0; cy < d.ncy; cy++ )
		for ( int cx = 0; cx < d.ncx; cx++ ) {
			const int wx = d.gx0 + cx, wy = d.gy0 + cy;
			if ( wx >= cx0 && wx <= cx1 && wy >= cy0 && wy <= cy1 )
				continue;
			d.cells++;
			if ( d.pageCell.size() < 255 )	// the page id is a byte of the grid (255 = none)
				d.pageCell.push_back( cy * d.ncx + cx );
		}
	d.pages = int( d.pageCell.size() );
	std::vector<int> remap;
	QVector<quint32> forms;
	buildGrid( L, d, remap, forms );
	{
		// the terrain preview's LTEX pictures (the default form = the engine's land set), from the session's stack
		QElapsedTimer tp;
		tp.start();
		const size_t layer = size_t( kLtexSide ) * kLtexSide;
		d.ltexPix.assign( layer * size_t( std::max<qsizetype>( 1, forms.size() ) ), 0xFF808080u );
		LodgenBakeCaches * caches = lodgenCreateBakeCaches();
		std::vector<quint32> pic;
		for ( int i = 0; i < forms.size(); i++ ) {
			QString path;
			if ( lodgenLtexPicture( world, QString(), caches, forms.at( i ), kLtexSide, pic, &path ) && pic.size() == layer )
				std::memcpy( &d.ltexPix[layer * size_t( i )], pic.data(), layer * 4 );
			else
				d.ltexMissing++;
			d.ltexPaths << path;
		}
		lodgenDestroyBakeCaches( caches );
		d.ltexMs = double( tp.nsecsElapsed() ) / 1e6;
	}
	buildDecals( lodlPath, d, cx0, cy0, cx1, cy1 );
	note( QStringLiteral( "far ground: prepared ring 0 cells %1..%2 x %3..%4 (%5 drawn, %6 pages), grid %7x%8, %9 LTEX layers, "
		"%10 decal records%11, list max %12 mean %13, %14 ms, CPU %15 MB" )
		.arg( d.gx0 ).arg( gx1 ).arg( d.gy0 ).arg( gy1 ).arg( d.cells ).arg( d.pages ).arg( d.nx ).arg( d.ny )
		.arg( d.ltexPaths.size() ).arg( d.recPiece.size() )
		.arg( d.decalWhy.isEmpty() ? QString() : QStringLiteral( " (no decals: %1)" ).arg( d.decalWhy ) )
		.arg( d.maxList ).arg( d.meanList, 0, 'f', 1 ).arg( double( t.nsecsElapsed() ) / 1e6, 0, 'f', 1 )
		.arg( mb( qint64( d.grid.size() + d.height.size() + d.recs.size() + d.binTex.size() + d.ids.size() ) * 4 ) ) );
	docs().insert( nif, std::move( d ) );
}

void forget( const void * nif )
{
	auto it = docs().find( nif );
	if ( it == docs().end() )
		return;
	if ( it->gpu.ctx )
		graveyard().push_back( it->gpu );	// GL objects go with the next pass on their context
	docs().erase( it );
}

int twinFor( const void * nif, int block )
{
	const auto it = docs().constFind( nif );
	if ( it == docs().constEnd() || block < it->first || block >= it->last )
		return 0;
	if ( it->gpu.ctx != QOpenGLContext::currentContext() )
		return 0;
	return it->gpu.ready;
}

void pass( Scene * scene )
{
	if ( !scene || !scene->renderer )
		return;
	QOpenGLContext * ctx = QOpenGLContext::currentContext();
	auto fn = scene->renderer->fn;
	std::vector<Gpu> & grave = graveyard();
	for ( size_t i = 0; i < grave.size(); ) {
		if ( grave[i].ctx == ctx ) {
			freeAll( fn, grave[i] );
			grave.erase( grave.begin() + qsizetype( i ) );
		} else {
			i++;
		}
	}
	auto it = docs().find( scene->nifModel );
	if ( it == docs().end() )
		return;
	Doc & d = *it;
	if ( d.gpu.ctx && d.gpu.ctx != ctx )
		return;
	const State & s = st();
	int want = s.redBaked ? 0 : s.mode;
	const bool old = want == 2 && s.redOldPages;
	QString extra;
	if ( old && d.pages < d.cells ) {
		want = 0;
		extra = QStringLiteral( "; cached REFUSED (%1 cells, 255 pages at most)" ).arg( d.cells );
	}
	static GLint units = 0;
	if ( !units )
		fn->glGetIntegerv( GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &units );
	if ( want && units <= WwCellUnit::FarGroundPagesFar ) {
		want = 0;
		extra = QStringLiteral( "; REFUSED (too few texture units)" );
	}
	if ( !want ) {
		if ( d.gpu.ctx )
			freeAll( fn, d.gpu );	// VRAM: the baked sheet needs none of it
		d.gpu.ctx = nullptr;
		status( d, 0, extra + ( s.redBaked ? QStringLiteral( "; red baked" ) : QString() ) );
		return;
	}
	d.gpu.ctx = ctx;
	GLint prevActive = 0, prev2D = 0, prevArr = 0;
	fn->glGetIntegerv( GL_ACTIVE_TEXTURE, &prevActive );
	fn->glActiveTexture( GL_TEXTURE0 + WwCellUnit::FarGroundGrid );
	fn->glGetIntegerv( GL_TEXTURE_BINDING_2D, &prev2D );
	fn->glGetIntegerv( GL_TEXTURE_BINDING_2D_ARRAY, &prevArr );
	auto restore = [&]() {
		fn->glActiveTexture( GL_TEXTURE0 + WwCellUnit::FarGroundGrid );
		fn->glBindTexture( GL_TEXTURE_2D, GLuint( prev2D ) );
		fn->glBindTexture( GL_TEXTURE_2D_ARRAY, GLuint( prevArr ) );
		fn->glActiveTexture( GLenum( prevActive ) );
	};
	const QString reds = ( s.redNoDecals ? QStringLiteral( "; red nodecals" ) : QString() )
		+ ( s.redNoCompress ? QStringLiteral( "; red nocompress" ) : QString() ) + ( old ? QStringLiteral( "; red oldpages" ) : QString() );
	if ( want == 2 && !old ) {
		// FARPAGES1: a page a cell (layers + decals), painted a few a frame; nothing is uploaded or blended per frame
		delTex( fn, d.gpu.pages, d.gpu.bPages );
		d.gpu.pageSide = 0;
		QString more;
		const bool ok = cachedPass( scene, d, s, &more );
		restore();
		if ( !ok ) {
			freeAll( fn, d.gpu );
			d.gpu.ctx = nullptr;
			status( d, 0, more );
			return;
		}
		d.gpu.ready = 3;
		status( d, 2, more + reds + extra );
		return;
	}
	freePools( fn, d.gpu );
	if ( !d.pics )
		loadPictures( d );
	uploadBase( fn, d );
	if ( want == 1 ) {
		delTex( fn, d.gpu.pages, d.gpu.bPages );
		d.gpu.pageSide = 0;
		uploadDecals( fn, d );
	} else {
		const int side = kPageSide[s.quality];
		if ( !d.gpu.pages || d.gpu.pageSide != side ) {
			QString why;
			if ( !stamp( scene->renderer, d, side, &why ) ) {
				freeAll( fn, d.gpu );
				d.gpu.ctx = nullptr;
				fn->glActiveTexture( GLenum( prevActive ) );
				status( d, 0, QStringLiteral( "; cached REFUSED (%1)" ).arg( why ) );
				return;
			}
			d.gpu.pageSide = side;
		}
		freeDecals( fn, d.gpu );
	}
	restore();
	d.gpu.ready = want;
	status( d, want, reds + extra );
}

void uniforms( Scene * scene, NifSkopeOpenGLContext::Program * prog )
{
	auto it = docs().constFind( scene ? scene->nifModel : nullptr );
	if ( it == docs().constEnd() || !it->gpu.ready || it->gpu.ctx != QOpenGLContext::currentContext() ) {
		prog->uni1i( "fgMode", 0 );
		return;
	}
	const Doc & d = *it;
	auto fn = scene->renderer->fn;
	WwCellUni::Units<std::remove_pointer_t<decltype( fn )>> u( fn, wwCellUniDsa() );	// lane CELLUNI3: no ask with DSA
	u.section();
	if ( d.gpu.ready == 3 ) {
		u.bind( WwCellUnit::FarGroundGrid, GL_TEXTURE_2D, d.gpu.cellTex );
		u.bind( WwCellUnit::FarGroundPages, GL_TEXTURE_2D_ARRAY, d.gpu.pagesN );
		u.bind( WwCellUnit::FarGroundPagesFar, GL_TEXTURE_2D_ARRAY, d.gpu.pagesF );
		prog->uni1i( "fgCells", WwCellUnit::FarGroundGrid );
		prog->uni1i( "fgPagesN", WwCellUnit::FarGroundPages );
		prog->uni1i( "fgPagesF", WwCellUnit::FarGroundPagesFar );
		prog->uni4f( "fgGridMap", FloatVector4( d.ox, d.oy, 1.0f / kSpacing, lodgenLandTiling() ) );
		prog->uni1i( "fgMode", 3 );
		u.done();
		return;
	}
	u.bind( WwCellUnit::FarGroundGrid, GL_TEXTURE_2D, d.gpu.grid );
	u.bind( WwCellUnit::FarGroundLtex, GL_TEXTURE_2D_ARRAY, d.gpu.ltex );
	baseUniforms( prog, d );
	prog->uni1i( "fgMode", d.gpu.ready );
	prog->uni1i( "fgDecalsOn", st().redNoDecals ? 0 : 1 );
	if ( d.gpu.ready == 1 ) {
		u.bind( WwCellUnit::FarGroundDecal, GL_TEXTURE_BUFFER, d.gpu.dtex );
		u.bind( WwCellUnit::FarGroundAtlas, GL_TEXTURE_2D, d.gpu.atlas );
		decalUniforms( prog, d );
	} else {
		u.bind( WwCellUnit::FarGroundPages, GL_TEXTURE_2D_ARRAY, d.gpu.pages );
		prog->uni1i( "fgPages", WwCellUnit::FarGroundPages );
	}
	u.done();
}

bool ring0( const void * nif, int block )
{
	const auto it = docs().constFind( nif );
	return it != docs().constEnd() && block >= it->first && block < it->last;	// every option, Baked too: the judge's total
}

bool wantsFrames( Scene * scene )
{
	const auto it = docs().constFind( scene ? scene->nifModel : nullptr );
	return it != docs().constEnd() && it->gpu.ready == 3 && it->gpu.busy;
}

}
