/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lodgenaggregate.h"
#include "lodgenlayout.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <vector>

/* THE AGGREGATE COMPOSITE.
 *
 * Every geometric law here is the CARD contract's, applied to a cell instead of
 * a model, and the header says why the photograph is a composite rather than a
 * viewport render. What is new is only this: the thing being photographed is a
 * cluster of quads whose pictures we already hold, and an orthographic camera
 * makes that a resample rather than a render.
 */

namespace
{

constexpr float CELL_UNITS = 4096.0f;
//! docs/LODGEN_CARD_SHEETS.md 4, the coverage contract this bake WRITES.
constexpr int COV_FLOOR = 16, COV_TEST = 128, COV_BASE = 160;

int evenUp( int v )
{
	return v + ( v & 1 );
}

//! gap(side) = max(2, side/16) rounded UP to even (docs/LODGEN_CARD_SHEETS.md 3.1).
int gapOf( int side )
{
	return std::max( 2, evenUp( ( side + 15 ) / 16 ) );
}

//! mips = max(1, log2(min(gapX, gapY))) -- the gap decides it (3.4).
int mipsOf( int gapX, int gapY )
{
	int unit = std::min( gapX, gapY ), m = 0;
	for ( int g = unit; g >= 2; g /= 2 )
		m++;
	return std::max( 1, m );
}

struct Vec3
{
	float v[3] = { 0, 0, 0 };
	float operator[]( int i ) const { return v[i]; }
	float & operator[]( int i ) { return v[i]; }
};

Vec3 mulT( const float m[9], const Vec3 & a )   // R^T * a
{
	Vec3 o;
	for ( int c = 0; c < 3; c++ )
		o[c] = m[c] * a[0] + m[3 + c] * a[1] + m[6 + c] * a[2];
	return o;
}
Vec3 mul( const float m[9], const Vec3 & a )    // R * a
{
	Vec3 o;
	for ( int r = 0; r < 3; r++ )
		o[r] = m[r * 3] * a[0] + m[r * 3 + 1] * a[1] + m[r * 3 + 2] * a[2];
	return o;
}
float dot( const Vec3 & a, const Vec3 & b )
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/*! The card grid's own mapping (docs/LODGEN_CARD_SHEETS.md 2, and the bake's
 *  `viewDir` lambda at nifskope_ui.cpp:22657): frame (i, j) looks from this
 *  direction, model space, Z up. It is the direction from the MODEL toward the
 *  CAMERA -- the bake turns it into an elevation and an azimuth and places the
 *  camera there. */
Vec3 cardFrameDir( int oct, int i, int j )
{
	const float u = float( i ) / float( oct - 1 ) * 2.0f - 1.0f;
	const float v = float( j ) / float( oct - 1 ) * 2.0f - 1.0f;
	Vec3 d;
	d[0] = ( u + v ) * 0.5f;
	d[1] = ( u - v ) * 0.5f;
	d[2] = 1.0f - std::fabs( d[0] ) - std::fabs( d[1] );
	const float l = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
	for ( int k = 0; k < 3; k++ )
		d[k] /= l;
	return d;
}

/*! Decode the colour sheet's alpha into the COVERAGE fraction, under whichever
 *  vintage the set is (docs/LODGEN_CARD_SHEETS.md 4). A set that states the
 *  contract wrote its floor at `base`, so the fraction is recovered exactly;
 *  one that states nothing carries the raw fraction and is tested at its floor
 *  of 16/255. Getting this wrong is not subtle -- reading a contract sheet as a
 *  raw one makes every partly covered twig four times too opaque. */
float coverageOf( const LodgenAggCard & c, int a )
{
	if ( c.covBase > c.covTest && c.covTest > c.covFloor ) {
		if ( a < c.covTest )
			return 0.0f;
		const float cov = float( c.covFloor )
			+ float( a - c.covBase ) * float( 255 - c.covFloor ) / float( 255 - c.covBase );
		return std::clamp( cov, float( c.covFloor ), 255.0f ) / 255.0f;
	}
	return a < 16 ? 0.0f : float( a ) / 255.0f;
}

//! The inverse, for the sheet this bake writes: coverage -> the alpha a
//! consumer's own 0.5 test selects exactly (4, "THE COVERAGE CONTRACT").
int coverageEncode( float cov )
{
	const int a = int( std::lround( cov * 255.0f ) );
	if ( a < COV_FLOOR )
		return 0;
	return std::clamp( COV_BASE + int( std::lround( double( a - COV_FLOOR )
		* double( 255 - COV_BASE ) / double( 255 - COV_FLOOR ) ) ), COV_BASE, 255 );
}

//! The three sheets of one card set, loaded once and shared by every cell.
struct CardImages
{
	QImage albedo, normal, mask;
	bool ok = false;
};

const CardImages & loadCard( const LodgenAggCard & c, QHash<quint32, CardImages> & cache )
{
	auto it = cache.constFind( c.formId );
	if ( it != cache.constEnd() )
		return *it;
	CardImages im;
	const QString id = QString( "%1" ).arg( c.formId, 8, 16, QChar( '0' ) );
	const QString stem = c.dir + QLatin1Char( '/' ) + id;
	im.albedo = QImage( stem + QStringLiteral( "_oct_albedo.png" ) );
	im.normal = QImage( stem + QStringLiteral( "_oct_normal.png" ) );
	im.mask = QImage( stem + ( c.pbr ? QStringLiteral( "_oct_rmaos.png" )
		: QStringLiteral( "_oct_gsaos.png" ) ) );
	if ( !im.albedo.isNull() )
		im.albedo = im.albedo.convertToFormat( QImage::Format_ARGB32 );
	if ( !im.normal.isNull() )
		im.normal = im.normal.convertToFormat( QImage::Format_ARGB32 );
	if ( !im.mask.isNull() )
		im.mask = im.mask.convertToFormat( QImage::Format_ARGB32 );
	/* The colour sheet is the only one an aggregate cannot do without: its
	 * alpha IS the silhouette. A missing normal or mask sheet is carried as a
	 * neutral plane rather than refusing the tree, and the census says so. */
	im.ok = !im.albedo.isNull()
		&& im.albedo.width() == c.oct * c.frameW && im.albedo.height() == c.oct * c.frameH;
	return *cache.insert( c.formId, im );
}

//! One target texel's running composite, premultiplied by coverage.
struct Accum
{
	float a = 0.0f;             //!< coverage already accumulated
	float rgb[3] = { 0, 0, 0 };
	float nxy[2] = { 0, 0 };
	float sway = 0.0f;
	float mask[4] = { 0, 0, 0, 0 };
	float depth = 0.0f;         //!< world units toward the camera from the card plane
};

} // namespace

QString lodgenAggregateStem( const QString & worldspace, int cellX, int cellY )
{
	/* ONE ROOT (lane LAYOUT1, 2026-09-16, bungo 19:3x). The aggregate sheets
	 * are a FO4CS-target output, so they live with the rest of them:
	 * Data\FO4CSLOD\<ws>\Aggregate\. ONE function composes that
	 * prefix (src/lodgenlayout.cpp) and this is the only place the game
	 * string for a set is made. */
	return QStringLiteral( "Data\\" ) + lodgenFo4csGameWorldPath( worldspace )
		+ QStringLiteral( "\\Aggregate\\" ) + QString::number( cellX ) + QStringLiteral( "_" )
		+ QString::number( cellY ) + QStringLiteral( "_agg" );
}

bool lodgenAggregateBuild( const QVector<LodgenAggTree> & trees,
	const QHash<quint32, LodgenAggCard> & cards, const LodgenAggOptions & opts,
	QVector<LodgenAggSet> * out, LodgenAggStats * stats, QString * error )
{
	if ( !out )
		return false;
	LodgenAggStats st;
	if ( opts.views < 2 || opts.tile < 16 || opts.minTrees < 1 ) {
		if ( error )
			*error = QString( "aggregate options refuse themselves: views %1 (needs 2+), tile %2 (needs 16+), minTrees %3" )
				.arg( opts.views ).arg( opts.tile ).arg( opts.minTrees );
		return false;
	}

	// --- bin the trees by cell -------------------------------------------
	QHash<qint64, QVector<int>> perCell;
	auto key = []( int cx, int cy ) { return ( qint64( cx ) << 32 ) ^ quint32( cy ); };
	for ( int i = 0; i < trees.size(); i++ ) {
		const int cx = int( std::floor( trees[i].pos[0] / CELL_UNITS ) );
		const int cy = int( std::floor( trees[i].pos[1] / CELL_UNITS ) );
		perCell[key( cx, cy )].append( i );
	}
	st.cellsSeen = int( perCell.size() );

	QHash<quint32, CardImages> images;
	QSet<quint32> refusedBases;
	QList<qint64> keys = perCell.keys();
	std::sort( keys.begin(), keys.end() );

	for ( qint64 k : keys ) {
		const QVector<int> & members = perCell[k];
		if ( members.size() < opts.minTrees )
			continue;
		st.cellsForested++;
		const int cellX = int( k >> 32 );
		const int cellY = int( qint32( quint32( k & 0xFFFFFFFFU ) ) );

		/* Which trees this cell can actually photograph. A base with no card,
		 * or a card from before the orthographic camera, is REFUSED IN WORDS
		 * and its tree stays per-tree: the aggregate's fallback floor is the
		 * behaviour that already ships (CONSTITUTION 10), never a silent drop. */
		struct Member
		{
			int treeIndex;
			const LodgenAggCard * card;
			const CardImages * img;
		};
		QVector<Member> mem;
		for ( int ti : members ) {
			const LodgenAggTree & t = trees[ti];
			auto ci = cards.constFind( t.baseId );
			if ( ci == cards.constEnd() ) {
				st.treesRefusedNoCard++;
				if ( !refusedBases.contains( t.baseId ) ) {
					refusedBases.insert( t.baseId );
					st.refusals << QString( "base %1 has no card set in the bake tree, so its trees stay per-tree" )
						.arg( t.baseId, 8, 16, QChar( '0' ) );
				}
				continue;
			}
			if ( !ci->ortho ) {
				st.treesRefusedNotOrtho++;
				if ( !refusedBases.contains( t.baseId ) ) {
					refusedBases.insert( t.baseId );
					st.refusals << QString( "base %1 has a card baked through a PERSPECTIVE camera "
						"(no `projection ortho`), whose half extents describe no picture; re-bake it" )
						.arg( t.baseId, 8, 16, QChar( '0' ) );
				}
				continue;
			}
			const CardImages & im = loadCard( *ci, images );
			if ( !im.ok ) {
				st.treesRefusedNoImage++;
				if ( !refusedBases.contains( t.baseId ) ) {
					refusedBases.insert( t.baseId );
					st.refusals << QString( "base %1: the card's colour sheet PNG is missing or is not "
						"%2 x %3, so nothing can be composited from it" ).arg( t.baseId, 8, 16, QChar( '0' ) )
						.arg( ci->oct * ci->frameW ).arg( ci->oct * ci->frameH );
				}
				continue;
			}
			mem.append( Member{ ti, &*ci, &im } );
		}
		if ( mem.isEmpty() ) {
			st.cellsRefusedAllTreesLost++;
			st.refusals << QString( "cell (%1, %2): every one of its %3 trees was refused, so no aggregate "
				"is written and the cell keeps its per-tree cards" ).arg( cellX ).arg( cellY ).arg( members.size() );
			continue;
		}

		// --- the view basis, one per azimuth -----------------------------
		const int V = opts.views;
		std::vector<Vec3> eye( V ), right( V );
		for ( int v = 0; v < V; v++ ) {
			const float phi = 6.28318530717958647692f * float( v ) / float( V );
			eye[v][0] = std::cos( phi ); eye[v][1] = std::sin( phi ); eye[v][2] = 0.0f;
			/* right = up x eye, with up = +Z: the basis a look-at with forward
			 * = -eye and up = +Z produces, which is the basis the card frames
			 * were photographed in. If it were the other handedness the
			 * one-tree control (the aggregate of a single tree must reproduce
			 * that tree's own frame) would come out mirrored, which is exactly
			 * what that control is for. */
			right[v][0] = -eye[v][1]; right[v][1] = eye[v][0]; right[v][2] = 0.0f;
		}

		/* --- pass 1, ANALYTIC: where every tree's quad lands, per view.
		 *
		 * Nothing is drawn here. Each tree's quad is a rectangle of known world
		 * size at a known place, so the silhouette box of a view is the union
		 * of those rectangles -- exact, and without the render the card bake
		 * needs for the same number. */
		struct Quad
		{
			int mem;
			int frame;          //!< the card frame index j*oct+i
			float qr, qu;       //!< quad centre in the view's right/up, from the cell centre
			float hw, hh;       //!< quad half extents, world units
			float depth;        //!< toward the camera, from the cell centre
		};
		std::vector<std::vector<Quad>> quads( V );
		const float ccx = ( float( cellX ) + 0.5f ) * CELL_UNITS;
		const float ccy = ( float( cellY ) + 0.5f ) * CELL_UNITS;
		float zLo = 3.4e38f, zHi = -3.4e38f;
		std::vector<float> vMinR( V, 3.4e38f ), vMaxR( V, -3.4e38f );
		std::vector<float> vMinU( V, 3.4e38f ), vMaxU( V, -3.4e38f );
		bool anyMirror = false;
		for ( int m = 0; m < mem.size(); m++ ) {
			const LodgenAggTree & t = trees[mem[m].treeIndex];
			const LodgenAggCard & c = *mem[m].card;
			anyMirror = anyMirror || t.mirrorU;
			Vec3 cen;
			for ( int i = 0; i < 3; i++ )
				cen[i] = c.center[i];
			const Vec3 rc = mul( t.rot, cen );
			const Vec3 cw{ { t.pos[0] + t.scale * rc[0], t.pos[1] + t.scale * rc[1],
				t.pos[2] + t.scale * rc[2] } };
			for ( int v = 0; v < V; v++ ) {
				// the frame whose own direction best matches this view, in the tree's space
				const Vec3 nm = mulT( t.rot, eye[v] );
				int best = 0;
				float bestDot = -2.0f;
				for ( int j = 0; j < c.oct; j++ )
					for ( int i = 0; i < c.oct; i++ ) {
						const float d = dot( cardFrameDir( c.oct, i, j ), nm );
						if ( d > bestDot ) { bestDot = d; best = j * c.oct + i; }
					}
				float fx = 0.0f, fy = 0.0f;
				if ( c.frameOff.size() >= 2 * ( best + 1 ) ) {
					fx = c.frameOff[2 * best];
					fy = c.frameOff[2 * best + 1];
				}
				if ( t.mirrorU )
					fx = -fx;       // the U mirror flips the frame about its own centre
				Quad q;
				q.mem = m;
				q.frame = best;
				const float dx = cw[0] - ccx, dy = cw[1] - ccy;
				q.qr = dx * right[v][0] + dy * right[v][1] + t.scale * fx;
				q.qu = cw[2] + t.scale * fy;            // absolute Z for now; re-based below
				q.depth = dx * eye[v][0] + dy * eye[v][1];
				q.hw = t.scale * c.halfW;
				q.hh = t.scale * c.halfH;
				vMinR[v] = std::min( vMinR[v], q.qr - q.hw );
				vMaxR[v] = std::max( vMaxR[v], q.qr + q.hw );
				vMinU[v] = std::min( vMinU[v], q.qu - q.hh );
				vMaxU[v] = std::max( vMaxU[v], q.qu + q.hh );
				zLo = std::min( zLo, q.qu - q.hh );
				zHi = std::max( zHi, q.qu + q.hh );
				quads[v].push_back( q );
			}
		}

		/* The frame holds the WIDEST SINGLE VIEW, and each view shifts its own
		 * silhouette to its own centre -- docs/LODGEN_CARD_SHEETS.md 3.6, the
		 * same law and the same `frameOffset` key. */
		const float centreZ = 0.5f * ( zLo + zHi );
		/* Pass 1 measured the vertical in ABSOLUTE world Z, because the cluster
		 * centre is only known once every quad is placed. Re-base now, so every
		 * number below this line is relative to the card's own centre exactly
		 * as `frameOffset` and `half` are. */
		for ( int v = 0; v < V; v++ ) {
			vMinU[v] -= centreZ;
			vMaxU[v] -= centreZ;
			for ( Quad & q : quads[v] )
				q.qu -= centreZ;
		}
		float measHalfW = 0.0f, measHalfH = 0.0f;
		QVector<float> frameOff( 2 * V, 0.0f );
		for ( int v = 0; v < V; v++ ) {
			const float ox = 0.5f * ( vMinR[v] + vMaxR[v] );
			const float oy = 0.5f * ( vMinU[v] + vMaxU[v] );
			frameOff[2 * v] = ox;
			frameOff[2 * v + 1] = oy;
			measHalfW = std::max( measHalfW, 0.5f * ( vMaxR[v] - vMinR[v] ) );
			measHalfH = std::max( measHalfH, 0.5f * ( vMaxU[v] - vMinU[v] ) );
		}
		if ( !( measHalfW > 0.0f ) || !( measHalfH > 0.0f ) ) {
			st.refusals << QString( "cell (%1, %2): the cluster measures no width or no height" )
				.arg( cellX ).arg( cellY );
			continue;
		}

		/* The frame's two sides: the LONG one is the tile rung, the SHORT one
		 * the smallest multiple of 16 whose INNER rect is not narrower than the
		 * measured silhouette (3.2). A cell is wider than it is tall in every
		 * case the census measured, but the rule is written both ways so a
		 * narrow gorge does not get a cropped frame. */
		const bool wideFrame = measHalfW >= measHalfH;
		const int longSide = opts.tile;
		const float measLong = wideFrame ? measHalfW : measHalfH;
		const float measShort = wideFrame ? measHalfH : measHalfW;
		const int gapLong = gapOf( longSide );
		const float unitsPerTexel = 2.0f * measLong / float( longSide - gapLong );
		int shortSide = 16;
		while ( shortSide < longSide ) {
			const int g = gapOf( shortSide );
			if ( float( shortSide - g ) * unitsPerTexel >= 2.0f * measShort )
				break;
			shortSide += 16;
		}
		shortSide = std::min( shortSide, longSide );
		const int frameW = wideFrame ? longSide : shortSide;
		const int frameH = wideFrame ? shortSide : longSide;
		const int gapX = gapOf( frameW ), gapY = gapOf( frameH );
		// half spans the WHOLE frame, padding included: the quad IS the frame
		const float halfW = unitsPerTexel * float( frameW ) * 0.5f;
		const float halfH = unitsPerTexel * float( frameH ) * 0.5f;

		// the cloud's radius, and the height channel's span, by the card's law
		float radius = 0.0f;
		for ( const Quad & q : quads[0] )
			radius = std::max( radius, std::sqrt( q.qr * q.qr + q.depth * q.depth )
				+ std::max( q.hw, q.hh ) );
		radius = std::max( radius, std::max( halfW, halfH ) );
		const float depthSpan = 3.0f * std::max( radius, 1024.0f );

		// --- pass 2, the composite ---------------------------------------
		const int sheetW = V * frameW, sheetH = frameH;
		QImage colour( sheetW, sheetH, QImage::Format_ARGB32 );
		QImage normal( sheetW, sheetH, QImage::Format_ARGB32 );
		QImage maskImg( sheetW, sheetH, QImage::Format_ARGB32 );
		colour.fill( 0 ); normal.fill( 0 ); maskImg.fill( 0 );
		std::vector<Accum> acc( size_t( frameW ) * frameH );
		std::vector<float> lw( size_t( frameW ) * frameH );
		std::vector<Accum> layer( size_t( frameW ) * frameH );
		long long coveredTexels = 0;
		double coverageSum = 0.0;
		int hMin = 255, hMax = 0;
		double hSum = 0.0;
		long long hN = 0;

		for ( int v = 0; v < V; v++ ) {
			std::fill( acc.begin(), acc.end(), Accum() );
			// farthest first: `depth` grows toward the camera
			std::vector<Quad> order = quads[v];
			std::stable_sort( order.begin(), order.end(),
				[]( const Quad & a, const Quad & b ) { return a.depth < b.depth; } );
			const float ox = frameOff[2 * v], oy = frameOff[2 * v + 1];
			const float left = ox - halfW, top = oy + halfH;
			for ( const Quad & q : order ) {
				const LodgenAggCard & c = *mem[q.mem].card;
				const CardImages & im = *mem[q.mem].img;
				const LodgenAggTree & t = trees[mem[q.mem].treeIndex];
				const int fi = q.frame % c.oct, fj = q.frame / c.oct;
				const int sx0 = fi * c.frameW, sy0 = fj * c.frameH;
				/* Source samples per target texel. Under 2 the splat would
				 * leave holes, so the source is supersampled until it cannot --
				 * the aggregate is always a heavy DOWNsample in practice, and
				 * this is the guard that says so rather than assuming it. */
				const float txPerSrcX = ( 2.0f * q.hw / float( c.frameW ) ) / unitsPerTexel;
				const float txPerSrcY = ( 2.0f * q.hh / float( c.frameH ) ) / unitsPerTexel;
				const int ssx = std::clamp( int( std::ceil( txPerSrcX * 2.0f ) ), 1, 8 );
				const int ssy = std::clamp( int( std::ceil( txPerSrcY * 2.0f ) ), 1, 8 );
				/* THE AREA WEIGHT, and the defect gate A3's own ceiling found
				 * (2026-09-11). A sample's contribution to a target texel is the share
				 * of the TEXEL'S AREA it stands for, not one vote. The first run
				 * normalised each tree's layer by the NUMBER of samples that landed in
				 * the texel, so a tree covering a tenth of a coarse texel composited as
				 * if it covered all of it: the aggregate carried 94 percent MORE
				 * coverage mass than the same cluster composited finely, against a
				 * measured ceiling of 1.4 percent. The weight is sampleArea/texelArea
				 * and the layer's alpha is its sum, clamped -- exact when a tree covers
				 * the whole texel, proportional when it does not. */
				const float sampleArea = ( 2.0f * q.hw / float( c.frameW ) / float( ssx ) )
					* ( 2.0f * q.hh / float( c.frameH ) / float( ssy ) );
				const float texelArea = unitsPerTexel * unitsPerTexel;
				const float wgt = texelArea > 0.0f ? sampleArea / texelArea : 1.0f;
				// the target rect this quad can touch
				const int tx0 = std::max( 0, int( std::floor( ( q.qr - q.hw - left ) / unitsPerTexel ) ) );
				const int tx1 = std::min( frameW - 1, int( std::ceil( ( q.qr + q.hw - left ) / unitsPerTexel ) ) );
				const int ty0 = std::max( 0, int( std::floor( ( top - ( q.qu + q.hh ) ) / unitsPerTexel ) ) );
				const int ty1 = std::min( frameH - 1, int( std::ceil( ( top - ( q.qu - q.hh ) ) / unitsPerTexel ) ) );
				if ( tx1 < tx0 || ty1 < ty0 )
					continue;
				const int lwW = tx1 - tx0 + 1, lwH = ty1 - ty0 + 1;
				std::fill_n( layer.begin(), size_t( lwW ) * lwH, Accum() );
				std::fill_n( lw.begin(), size_t( lwW ) * lwH, 0.0f );
				const bool haveN = !im.normal.isNull() && im.normal.size() == im.albedo.size();
				const bool haveM = !im.mask.isNull() && im.mask.size() == im.albedo.size();
				for ( int sy = 0; sy < c.frameH; sy++ ) {
					const QRgb * rowA = reinterpret_cast<const QRgb *>( im.albedo.constScanLine( sy0 + sy ) );
					const QRgb * rowN = haveN ? reinterpret_cast<const QRgb *>( im.normal.constScanLine( sy0 + sy ) ) : nullptr;
					const QRgb * rowM = haveM ? reinterpret_cast<const QRgb *>( im.mask.constScanLine( sy0 + sy ) ) : nullptr;
					for ( int sx = 0; sx < c.frameW; sx++ ) {
						const QRgb pa = rowA[sx0 + sx];
						const float cov = coverageOf( c, qAlpha( pa ) );
						if ( cov <= 0.0f )
							continue;
						const QRgb pn = rowN ? rowN[sx0 + sx] : qRgba( 128, 128, 128, 0 );
						const QRgb pm = rowM ? rowM[sx0 + sx] : qRgba( 0, 0, 255, 0 );
						/* The texel's own depth: the card's height channel is
						 * WINDOW depth about the card plane, so a value above
						 * 0.5 is FURTHER from the camera. */
						const float behind = ( float( qBlue( pn ) ) / 255.0f - 0.5f ) * c.depthSpan * t.scale;
						const float texelDepth = q.depth - behind;
						for ( int jy = 0; jy < ssy; jy++ ) {
							const float ly = ( float( sy ) + ( float( jy ) + 0.5f ) / float( ssy ) ) / float( c.frameH );
							const float pu = q.qu + ( 1.0f - 2.0f * ly ) * q.hh;
							const int ty = int( std::floor( ( top - pu ) / unitsPerTexel ) );
							if ( ty < ty0 || ty > ty1 )
								continue;
							for ( int jx = 0; jx < ssx; jx++ ) {
								float lx = ( float( sx ) + ( float( jx ) + 0.5f ) / float( ssx ) ) / float( c.frameW );
								if ( t.mirrorU )
									lx = 1.0f - lx;
								const float pr = q.qr + ( 2.0f * lx - 1.0f ) * q.hw;
								const int tx = int( std::floor( ( pr - left ) / unitsPerTexel ) );
								if ( tx < tx0 || tx > tx1 )
									continue;
								Accum & L = layer[size_t( ty - ty0 ) * lwW + ( tx - tx0 )];
								float & W = lw[size_t( ty - ty0 ) * lwW + ( tx - tx0 )];
								const float cw = cov * wgt;
								W += wgt;
								L.a += cw;
								L.rgb[0] += cw * float( qRed( pa ) );
								L.rgb[1] += cw * float( qGreen( pa ) );
								L.rgb[2] += cw * float( qBlue( pa ) );
								L.nxy[0] += cw * float( qRed( pn ) );
								L.nxy[1] += cw * float( qGreen( pn ) );
								L.sway += cw * float( qAlpha( pn ) );
								L.mask[0] += cw * float( qRed( pm ) );
								L.mask[1] += cw * float( qGreen( pm ) );
								L.mask[2] += cw * float( qBlue( pm ) );
								L.mask[3] += cw * float( qAlpha( pm ) );
								L.depth += cw * texelDepth;
							}
						}
					}
				}
				// the layer over what is already there, back to front
				for ( int ty = ty0; ty <= ty1; ty++ ) {
					for ( int tx = tx0; tx <= tx1; tx++ ) {
						const size_t li = size_t( ty - ty0 ) * lwW + ( tx - tx0 );
						const float W = lw[li];
						if ( W <= 0.0f )
							continue;
						const Accum & L = layer[li];
						const float sa = std::clamp( L.a, 0.0f, 1.0f );   // L.a is already area-weighted
						if ( sa <= 0.0f )
							continue;
						Accum & D = acc[size_t( ty ) * frameW + tx];
						const float keep = 1.0f - sa;
						auto over = []( float & d, float srcPremul, float srcA, float keep2 ) {
							// srcPremul is sum(cov * value); divide by sum(cov) to get the value
							d = srcPremul * srcA + d * keep2;
						};
						const float inv = 1.0f / L.a;   // L.a is sum(cov) over the samples
						for ( int ch = 0; ch < 3; ch++ )
							over( D.rgb[ch], L.rgb[ch] * inv, sa, keep );
						for ( int ch = 0; ch < 2; ch++ )
							over( D.nxy[ch], L.nxy[ch] * inv, sa, keep );
						over( D.sway, L.sway * inv, sa, keep );
						for ( int ch = 0; ch < 4; ch++ )
							over( D.mask[ch], L.mask[ch] * inv, sa, keep );
						over( D.depth, L.depth * inv, sa, keep );
						D.a = sa + D.a * keep;
					}
				}
			}
			// encode this view's frame
			for ( int ty = 0; ty < frameH; ty++ ) {
				QRgb * rc = reinterpret_cast<QRgb *>( colour.scanLine( ty ) );
				QRgb * rn = reinterpret_cast<QRgb *>( normal.scanLine( ty ) );
				QRgb * rm = reinterpret_cast<QRgb *>( maskImg.scanLine( ty ) );
				for ( int tx = 0; tx < frameW; tx++ ) {
					const Accum & D = acc[size_t( ty ) * frameW + tx];
					const int X = v * frameW + tx;
					if ( D.a <= 0.0f ) {
						rc[X] = qRgba( 0, 0, 0, 0 );
						rn[X] = qRgba( 128, 128, 128, 0 );
						rm[X] = qRgba( 0, 0, 255, 0 );
						continue;
					}
					const float inv = 1.0f / D.a;
					auto b8 = [inv]( float v2 ) {
						return std::clamp( int( std::lround( v2 * inv ) ), 0, 255 );
					};
					const int a8 = coverageEncode( std::clamp( D.a, 0.0f, 1.0f ) );
					rc[X] = qRgba( b8( D.rgb[0] ), b8( D.rgb[1] ), b8( D.rgb[2] ), a8 );
					/* B = 0.5 + (distance BEHIND the card plane) / depthSpan,
					 * exactly the card's own encoding, so the same reader
					 * decodes both without a branch. */
					const float behind = -( D.depth * inv );
					const int h8 = std::clamp( int( std::lround( ( 0.5f + behind / depthSpan ) * 255.0f ) ), 0, 255 );
					rn[X] = qRgba( b8( D.nxy[0] ), b8( D.nxy[1] ), h8, b8( D.sway ) );
					rm[X] = qRgba( b8( D.mask[0] ), b8( D.mask[1] ), b8( D.mask[2] ), b8( D.mask[3] ) );
					if ( a8 >= COV_TEST ) {
						hMin = std::min( hMin, h8 );
						hMax = std::max( hMax, h8 );
						hSum += h8;
						hN++;
						coveredTexels++;
						coverageSum += double( D.a );
					}
				}
			}
		}

		LodgenAggSet s;
		s.cellX = cellX; s.cellY = cellY;
		s.colour = colour; s.normal = normal; s.mask = maskImg;
		s.views = V; s.frameW = frameW; s.frameH = frameH;
		s.gapX = gapX; s.gapY = gapY; s.mips = mipsOf( gapX, gapY );
		s.half[0] = halfW; s.half[1] = halfH;
		s.centre[0] = ccx; s.centre[1] = ccy; s.centre[2] = centreZ;
		s.depthSpan = depthSpan;
		s.boundRadius = radius;
		s.frameOffset = frameOff;
		s.pbr = mem.first().card->pbr;
		s.anyMirrored = anyMirror;
		s.heightMin = hN ? hMin : 0;
		s.heightMax = hN ? hMax : 0;
		s.heightMean = hN ? hSum / double( hN ) : 0.0;
		s.heightSpanUnits = hN ? float( double( hMax - hMin ) / 255.0 * double( depthSpan ) ) : 0.0f;
		s.coveredTexels = int( coveredTexels );
		s.coverageMean = coveredTexels ? coverageSum / double( coveredTexels ) : 0.0;
		for ( const Member & m : mem )
			s.covered.push_back( trees[m.treeIndex].srcIndex );
		std::sort( s.covered.begin(), s.covered.end() );
		st.treesPhotographed += int( s.covered.size() );
		st.cellsAggregated++;
		out->append( s );
	}

	if ( stats )
		*stats = st;
	return true;
}

QByteArray lodgenAggregateLodm( const QString & worldspace, const LodgenAggSet & set )
{
	QJsonObject root;
	root.insert( QStringLiteral( "lodm" ), 1 );
	root.insert( QStringLiteral( "family" ), set.pbr ? QStringLiteral( "pbr" ) : QStringLiteral( "legacy" ) );
	root.insert( QStringLiteral( "kind" ), QStringLiteral( "aggregate" ) );
	const QString stem = lodgenAggregateStem( worldspace, set.cellX, set.cellY );
	QJsonObject tex;
	tex.insert( set.pbr ? QStringLiteral( "baseColor" ) : QStringLiteral( "diffuse" ),
		stem + ( set.pbr ? QStringLiteral( "_bc.DDS" ) : QStringLiteral( "_d.DDS" ) ) );
	tex.insert( QStringLiteral( "normal" ), stem + QStringLiteral( "_n.DDS" ) );
	tex.insert( set.pbr ? QStringLiteral( "rmaos" ) : QStringLiteral( "gsaos" ),
		stem + ( set.pbr ? QStringLiteral( "_rmaos.DDS" ) : QStringLiteral( "_gsaos.DDS" ) ) );
	/* NO EMISSIVE SHEET. A forest emits nothing, and `emissiveScale` 0 is how a
	 * set says so in one number (docs/LODGEN_LODM_FORMAT.md 2). The key is
	 * absent rather than naming a black sheet nobody wrote. */
	root.insert( QStringLiteral( "textures" ), tex );
	root.insert( QStringLiteral( "emissiveScale" ), 0 );

	QJsonObject a;
	a.insert( QStringLiteral( "cell" ), QJsonArray{ set.cellX, set.cellY } );
	a.insert( QStringLiteral( "views" ), set.views );
	a.insert( QStringLiteral( "grid" ), QJsonArray{ set.views, 1 } );
	a.insert( QStringLiteral( "frame" ), QJsonArray{ set.frameW, set.frameH } );
	a.insert( QStringLiteral( "pad" ), QJsonArray{ set.gapX / 2, set.gapY / 2 } );
	a.insert( QStringLiteral( "gap" ), QJsonArray{ set.gapX, set.gapY } );
	a.insert( QStringLiteral( "mips" ), set.mips );
	a.insert( QStringLiteral( "half" ), QJsonArray{ double( set.half[0] ), double( set.half[1] ) } );
	a.insert( QStringLiteral( "center" ), QJsonArray{ double( set.centre[0] ),
		double( set.centre[1] ), double( set.centre[2] ) } );
	a.insert( QStringLiteral( "depthSpan" ), double( set.depthSpan ) );
	a.insert( QStringLiteral( "boundRadius" ), double( set.boundRadius ) );
	a.insert( QStringLiteral( "trees" ), int( set.covered.size() ) );
	/* The identity law, in words so a reader cannot mistake a constant for a
	 * missing feature (bungo 2026-09-11 08:4x). */
	a.insert( QStringLiteral( "identity" ), QStringLiteral( "per-aggregate" ) );
	a.insert( QStringLiteral( "projection" ), QStringLiteral( "ortho" ) );
	a.insert( QStringLiteral( "coverage" ), QJsonObject{
		{ QStringLiteral( "floor" ), COV_FLOOR },
		{ QStringLiteral( "test" ), COV_TEST },
		{ QStringLiteral( "base" ), COV_BASE } } );
	QJsonArray fo;
	for ( float f : set.frameOffset )
		fo.append( double( f ) );
	a.insert( QStringLiteral( "frameOffset" ), fo );
	root.insert( QStringLiteral( "aggregate" ), a );

	const QByteArray payload = QJsonDocument( root ).toJson( QJsonDocument::Compact );
	QByteArray file;
	file.append( "LODM", 4 );
	auto putLE32 = [&file]( quint32 v ) {
		for ( int i = 0; i < 4; i++ )
			file.append( char( ( v >> ( 8 * i ) ) & 0xFF ) );
	};
	putLE32( 1 );
	putLE32( quint32( payload.size() ) );
	file.append( payload );
	return file;
}
