/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellsplat.h"

#include "esmdata.h"

#include <QHash>
#include <QMutex>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace {

constexpr float CELL_UNITS = 4096.0f;
constexpr int LAND_GRID = 33;       //!< heights per side
constexpr int QUAD_GRID = 17;       //!< a quadrant's opacity grid per side

/*! Which quadrant a 33x33 grid point falls in, and where inside it.
 *  docs/LODGEN_ESM_LAYOUTS.md: 0 BL, 1 BR, 2 TL, 3 TR, each 17x17 with the
 *  middle row and column SHARED, which is why the local index is the grid
 *  index minus 16 and not minus 17.
 *
 *  THIS SHARING IS THE WHOLE REASON THE BLEND NEEDS NO INTERPOLATION. The
 *  land vertex at grid (16, c) is the LAST row of quadrant 0/1 and the FIRST
 *  row of quadrant 2/3, and the record stores an opacity for it in both. A
 *  quad reads its four corners straight out of the grids; two quads that
 *  share an edge read the same two numbers for it; so the weights are
 *  continuous across quadrant boundaries without any code saying so. */
void quadrantOf( int row, int col, int & q, int & lrow, int & lcol )
{
	const int top = row >= QUAD_GRID - 1 ? 1 : 0;
	const int right = col >= QUAD_GRID - 1 ? 1 : 0;
	q = top * 2 + right;
	lrow = row - top * ( QUAD_GRID - 1 );
	lcol = col - right * ( QUAD_GRID - 1 );
	lrow = qBound( 0, lrow, QUAD_GRID - 1 );
	lcol = qBound( 0, lcol, QUAD_GRID - 1 );
}

/*! THE PAINT ORDER, or an honest admission that it is not known.
 *
 *  The order the engine composites a quadrant's layers in is the ATXT LAYER
 *  INDEX -- the int16 at ATXT payload offset 6, after the LTEX form (4) and
 *  the quadrant byte (1) and one unknown byte. `src/esmdata.cpp` reads the
 *  first five bytes and stops, so until the hook-up adds the field there is
 *  nothing to sort BY and the record's own order is used instead. That is a
 *  guess; `layersInRecordOrder` counts every layer composited under it, so a
 *  picture built without the hook-up says so in its own legend rather than
 *  looking merely slightly wrong. */
int layerOrder( const EsmLandLayer & l, int slot )
{
#ifdef WW_CELLSPLAT_LAYER_INDEX
	(void) slot;
	return l.index;
#else
	(void) l;
	return slot;
#endif
}

bool layerOrderKnown()
{
#ifdef WW_CELLSPLAT_LAYER_INDEX
	return true;
#else
	return false;
#endif
}

//! A pass over one quad: which texture, in what order, opaque or blended.
struct Pass
{
	quint32 ltex = 0;
	int order = -1;
	bool blend = false;
	float w[4] = { 1, 1, 1, 1 };
};

/*! THE RED CONTROL (lane TERRBLEND1). Set WW_TERRBLEND_RED=nearest and every
 *  pass takes ONE weight for the whole 128-unit quad -- its SW corner's --
 *  instead of the four stored corners. That is the per-quad switching this
 *  lane removes, put back on purpose, so the ground-rebuild gate is shown to
 *  FAIL on it. It forces the state a measurement needs; it is not a feature. */
bool redNearest()
{
	static const bool on = qgetenv( "WW_TERRBLEND_RED" ) == "nearest";
	return on;
}

/*! One quadrant's layers as the ENGINE builds them (lane TERRBLEND1, read out
 *  of Todd's treat, 1.10.155, and cross-checked against Nomad's 1.10.163
 *  decompile -- notes/terrblend1/STATUS.md has the addresses):
 *
 *   - LoadVerticesIntoArrays: an ATXT's slot is its LAYER INDEX, clamped to
 *     11; each VTXT opacity is stored as a BYTE, (int)(opacity * 255),
 *     truncated.
 *   - CreateGeometry: the base is the BTXT, or -- when the quadrant has none --
 *     the engine's default land texture set (CommonwealthDefault01, here
 *     ESM_LTEX_ENGINE_DEFAULT). It is NEVER "no base". Null slots are
 *     compacted away (RemoveTextureFromArrays).
 *   - MergeMatchingTextures: two slots with the same texture have their bytes
 *     summed into the first; a slot equal to the base is dropped (its weight
 *     goes to the base). Then the base weight is clamp(255 - sum(layers), 0,
 *     255).
 *   - The vertex carries the base and the FIRST FIVE layers; a sixth is read
 *     into the base sum and then not drawn (the ground goes darker there, as
 *     in the game).
 *   - The pixel shader (Landscape PS) is colour = sum(w_i * tex_i), NOT
 *     normalised, then times VCLR.
 *
 *  So the weights are SHARES of one sum, not alpha-over opacities. Nothing
 *  here composites "over" anything. */
struct QuadLayers
{
	quint32 base = 0;
	bool defaultBase = false;
	int n = 0;                          //!< layers kept (<= 5)
	quint32 ltex[5] = {};
	int order[5] = {};
	float b[6][QUAD_GRID][QUAD_GRID];   //!< [0] = base bytes, [1..n] = layer bytes, 0..255
	int dropped = 0;                    //!< layers past the fifth
	int merged = 0;                     //!< slots folded into another or into the base
	int formZero = 0;                   //!< ATXT layers naming form 0 = the default set
};

void buildQuadLayers( const EsmLand & land, int q, QuadLayers & out )
{
	out = QuadLayers();
	out.base = land.baseTex[q] ? land.baseTex[q] : ESM_LTEX_ENGINE_DEFAULT;
	out.defaultBase = !land.baseTex[q];

	// slots by layer index (clamped to 11); a later ATXT on the same slot replaces it
	const QVector<EsmLandLayer> & ls = land.layers[q];
	int slotOf[12];
	for ( int s = 0; s < 12; s++ )
		slotOf[s] = -1;
	for ( int li = 0; li < ls.size(); li++ ) {
		const int s = qBound( 0, layerOrder( ls.at( li ), li ), 11 );
		slotOf[s] = li;
	}

	struct L { quint32 ltex; int order; float b[QUAD_GRID][QUAD_GRID]; };
	std::vector<L> merged;
	for ( int s = 0; s < 12; s++ ) {
		if ( slotOf[s] < 0 )
			continue;
		const EsmLandLayer & l = ls.at( slotOf[s] );
		/* AN ATXT WITH LTEX 0 IS THE DEFAULT TEXTURE, NOT "NOTHING". The engine
		 * loads form 0 as pDefText (LoadVerticesIntoArrays), which is a real
		 * texture set and survives RemoveTextureFromArrays. Read as "paints
		 * nothing" (this file until TERRBLEND1) its share went missing and the
		 * quads it covered came out as dark, hard-edged squares: measured on
		 * Sanctuary -17,23, where 3 of 4 quadrants carry a form-0 layer. */
		const quint32 lt = l.ltex ? l.ltex : ESM_LTEX_ENGINE_DEFAULT;
		if ( !l.ltex )
			out.formZero++;
		if ( lt == out.base ) {
			out.merged++;               // its weight is the base's, implicitly
			continue;
		}
		L * into = nullptr;
		for ( L & m : merged ) {
			if ( m.ltex == lt )
				into = &m;
		}
		if ( into )
			out.merged++;
		else {
			merged.push_back( L() );
			into = &merged.back();
			into->ltex = lt;
			into->order = s;
			for ( int r = 0; r < QUAD_GRID; r++ )
				for ( int c = 0; c < QUAD_GRID; c++ )
					into->b[r][c] = 0.0f;
		}
		for ( int r = 0; r < QUAD_GRID; r++ )
			for ( int c = 0; c < QUAD_GRID; c++ )
				into->b[r][c] += std::floor( qBound( 0.0f, l.opacity[r][c], 1.0f ) * 255.0f );
	}

	for ( int r = 0; r < QUAD_GRID; r++ ) {
		for ( int c = 0; c < QUAD_GRID; c++ ) {
			float sum = 0.0f;
			for ( const L & m : merged )
				sum += m.b[r][c];
			out.b[0][r][c] = qBound( 0.0f, 255.0f - sum, 255.0f );
		}
	}
	out.n = int( qMin<size_t>( merged.size(), 5 ) );
	out.dropped = int( merged.size() ) - out.n;
	for ( int i = 0; i < out.n; i++ ) {
		out.ltex[i] = merged[size_t( i )].ltex;
		out.order[i] = merged[size_t( i )].order;
		for ( int r = 0; r < QUAD_GRID; r++ )
			for ( int c = 0; c < QUAD_GRID; c++ )
				out.b[i + 1][r][c] = qMin( merged[size_t( i )].b[r][c], 255.0f );
	}
}

/*! Every pass one 128-unit quad needs: the base ALWAYS (opaque -- it writes
 *  the depth and carries its own share, w0, in the vertex alpha), then each
 *  kept layer with any share at any of the four corners. The shares sum to 1
 *  at every corner (less where a sixth layer was dropped), so the passes are
 *  ADDED (src/gl: landSplat), and the order they are drawn in does not
 *  matter. */
int passesFor( const QuadLayers ql[4], int row, int col, Pass * out, int maxOut )
{
	/* The quadrant is the one the quad's OWN SW corner falls in, and every one
	 * of its four corners is then read out of THAT quadrant's grids. A 128-unit
	 * quad never straddles two quadrants: the boundary runs along grid line 16,
	 * which is a quad EDGE, not a quad. */
	int q = 0, lrow = 0, lcol = 0;
	quadrantOf( row, col, q, lrow, lcol );
	const QuadLayers & L = ql[q];
	const int r0 = row - ( q >= 2 ? QUAD_GRID - 1 : 0 );
	const int c0 = col - ( ( q & 1 ) ? QUAD_GRID - 1 : 0 );
	// the quad's four corners in the quadrant grid, SW CCW, as the geometry below
	const int rr[4] = { r0, r0, r0 + 1, r0 + 1 };
	const int cc[4] = { c0, c0 + 1, c0 + 1, c0 };
	const bool red = redNearest();

	int n = 0;
	for ( int i = 0; i <= L.n && n < maxOut; i++ ) {
		Pass p;
		p.ltex = i ? L.ltex[i - 1] : L.base;
		p.order = i ? L.order[i - 1] : -1;
		p.blend = i > 0;
		float top = 0.0f;
		for ( int k = 0; k < 4; k++ ) {
			const int kr = red ? rr[0] : rr[k];
			const int kc = red ? cc[0] : cc[k];
			p.w[k] = L.b[i][qBound( 0, kr, QUAD_GRID - 1 )][qBound( 0, kc, QUAD_GRID - 1 )] / 255.0f;
			top = qMax( top, p.w[k] );
		}
		if ( i && top < CELL_SPLAT_WEIGHT_MIN )
			continue;                   // a layer with no share on this quad adds nothing
		out[n++] = p;
	}
	return n;
}

//! The passes a cell would emit, without building any of them.
qint64 countCell( const EsmLand & land )
{
	QuadLayers ql[4];
	for ( int q = 0; q < 4; q++ )
		buildQuadLayers( land, q, ql[q] );
	qint64 n = 0;
	Pass passes[8];
	for ( int row = 0; row + 1 < LAND_GRID; row++ ) {
		for ( int col = 0; col + 1 < LAND_GRID; col++ )
			n += passesFor( ql, row, col, passes, 8 );
	}
	return n;
}

} // namespace


qint64 cellSplatCountVerts( EsmWorld & world, int x0, int y0, int x1, int y1 )
{
	if ( x1 < x0 || y1 < y0 )
		return -1;
	qint64 n = 0;
	for ( int y = y0; y <= y1; y++ ) {
		for ( int x = x0; x <= x1; x++ ) {
			EsmLand land;
			if ( !world.land( x, y, land ) )
				continue;
			n += countCell( land ) * 4;
		}
	}
	return n;
}


bool cellBuildSplat( EsmWorld & world, int x0, int y0, int x1, int y1,
	float originX, float originY, float tiling, CellSplatBuild & out, QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};

	out = CellSplatBuild();
	if ( x1 < x0 || y1 < y0 )
		return fail( QString( "the cell rectangle %1,%2 .. %3,%4 is empty" )
			.arg( x0 ).arg( y0 ).arg( x1 ).arg( y1 ) );
	const float T = ( tiling > 1.0f ) ? tiling : CELL_GROUND_TILING;

	// bucket 0 is always the bare one, exactly as the mosaic's is, so a quad
	// that resolves nothing still has somewhere to go and the bare count is real
	out.buckets.append( CellSplatBucket() );

	/* A bucket is a (texture, pass kind) pair, NOT a texture: the same sand may
	 * be a quadrant's opaque base here and a blended layer two quadrants away,
	 * and those are two different shapes with two different alpha properties. */
	QHash<quint64, int> bucketOf;
	QHash<quint32, bool> ltexTried;

	auto bucketFor = [&]( quint32 ltex, int order, bool blend ) -> int {
		if ( !ltex )
			return 0;
		const quint64 key = ( quint64( ltex ) << 32 )
			| ( quint64( quint32( order + 1 ) ) << 1 ) | quint64( blend ? 1 : 0 );
		auto it = bucketOf.constFind( key );
		if ( it != bucketOf.constEnd() )
			return it.value();
		QString diffuse, normal;
		world.ltexTextures( ltex, diffuse, normal );
		if ( !ltexTried.contains( ltex ) ) {
			ltexTried.insert( ltex, !diffuse.isEmpty() );
			if ( diffuse.isEmpty() )
				out.ltexUnresolved++;
		}
		if ( diffuse.isEmpty() ) {
			bucketOf.insert( key, 0 );
			return 0;
		}
		CellSplatBucket b;
		b.ltex = ltex;
		b.diffuse = diffuse;
		b.normal = normal;
		b.order = order;
		b.blend = blend;
		const int idx = out.buckets.size();
		out.buckets.append( b );
		bucketOf.insert( key, idx );
		return idx;
	};

	for ( int y = y0; y <= y1; y++ ) {
		for ( int x = x0; x <= x1; x++ ) {
			EsmLand land;
			if ( !world.land( x, y, land ) )
				continue;
			out.cells++;
			if ( land.hasColors )
				out.cellsWithColour++;
			QuadLayers ql[4];
			for ( int q = 0; q < 4; q++ ) {
				out.layersRead += land.layers[q].size();
				buildQuadLayers( land, q, ql[q] );
				out.layersDropped += ql[q].dropped;
				out.layersMerged += ql[q].merged;
				out.layersFormZero += ql[q].formZero;
				if ( ql[q].defaultBase )
					out.quadrantsDefaultBase++;
			}

			const float ox = float( x ) * CELL_UNITS;
			const float oy = float( y ) * CELL_UNITS;
			const float step = CELL_UNITS / float( LAND_GRID - 1 );

			for ( int row = 0; row + 1 < LAND_GRID; row++ ) {
				for ( int col = 0; col + 1 < LAND_GRID; col++ ) {
					out.quadsTotal++;

					Pass passes[8];
					const int np = passesFor( ql, row, col, passes, 8 );
					{
						int q = 0, lr = 0, lc = 0;
						quadrantOf( row, col, q, lr, lc );
						if ( ql[q].defaultBase )
							out.quadsDefaultBase++;
					}

					// the geometry, shared by every pass on this quad
					const float xs[2] = { ox + float( col ) * step, ox + float( col + 1 ) * step };
					const float ys[2] = { oy + float( row ) * step, oy + float( row + 1 ) * step };
					const int rr[4] = { row, row, row + 1, row + 1 };
					const int cc[4] = { col, col + 1, col + 1, col };
					const float xx[4] = { xs[0], xs[1], xs[1], xs[0] };
					const float yy[4] = { ys[0], ys[0], ys[1], ys[1] };

					CellSplatQuad proto;
					for ( int k = 0; k < 4; k++ ) {
						proto.v[k].p[0] = xx[k] - originX;
						proto.v[k].p[1] = yy[k] - originY;
						proto.v[k].p[2] = land.heights[rr[k]][cc[k]];
						/* WORLD-SPACE UVs, so the paint does not swim when the
						 * block grows: a texel belongs to a place, not to a quad.
						 * v GROWS NORTHWARD, as the game's: InitSDM (0x3a7180)
						 * writes uv = (col, row) * 0.375 with row running north,
						 * and the Landscape VS passes it through.  The old -y
						 * mirrored every texture north-south.  Every pass on
						 * the quad shares them, which is what makes the layers
						 * line up with the base instead of sliding over it. */
						proto.v[k].uv[0] = xx[k] / T;
						proto.v[k].uv[1] = yy[k] / T;
						if ( land.hasColors ) {
							for ( int c = 0; c < 3; c++ )
								proto.v[k].rgb[c] = float( land.colors[rr[k]][cc[k]][c] ) / 255.0f;
						}
					}
					// the face normal, from the two in-plane edges of the quad
					const float e1[3] = { proto.v[1].p[0] - proto.v[0].p[0],
						proto.v[1].p[1] - proto.v[0].p[1], proto.v[1].p[2] - proto.v[0].p[2] };
					const float e2[3] = { proto.v[3].p[0] - proto.v[0].p[0],
						proto.v[3].p[1] - proto.v[0].p[1], proto.v[3].p[2] - proto.v[0].p[2] };
					float n[3] = { e1[1] * e2[2] - e1[2] * e2[1],
						e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
					const float len = std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
					if ( len > 1.0e-6f ) {
						for ( int k = 0; k < 3; k++ )
							proto.nrm[k] = n[k] / len;
					} else {
						proto.nrm[0] = proto.nrm[1] = 0.0f;
						proto.nrm[2] = 1.0f;
					}

					int emitted = 0;
					for ( int pi = 0; pi < np; pi++ ) {
						const Pass & p = passes[pi];
						int bucket = bucketFor( p.ltex, p.order, p.blend );
						if ( !bucket && !p.blend ) {
							/* A base whose LTEX names no texture still has to be drawn:
							 * it is the opaque pass every layer is added onto, and
							 * without it the sky shows through (the holes). The
							 * engine's default set stands in, and is counted. */
							bucket = bucketFor( ESM_LTEX_ENGINE_DEFAULT, p.order, false );
							if ( bucket )
								out.quadsBaseFallback++;
						}
						if ( !bucket )
							continue;   // the LTEX named no texture: not drawn, counted
						CellSplatQuad qq = proto;
						qq.bucket = bucket;
						/* The SHARE, on the base pass too: the base is opaque (it writes
						 * the depth) and the shader scales it by its own share, then
						 * every layer is ADDED at its share (src/gl landSplat). */
						for ( int k = 0; k < 4; k++ )
							qq.v[k].w = qBound( 0.0f, p.w[k], 1.0f );
						out.quads.push_back( qq );
						out.buckets[bucket].quads++;
						out.quadsEmitted++;
						emitted++;
						if ( p.blend )
							out.quadsBlended++;
						else
							out.quadsBase++;
						if ( !layerOrderKnown() && p.blend )
							out.layersInRecordOrder++;
					}
					if ( !emitted ) {
						out.quadsBare++;
						out.quadsBareNoTexture++;
						out.buckets[0].quads++;
					}
				}
			}
		}
	}

	/* DRAW ORDER IS BUCKET ORDER, and the caller relies on it: the opaque base
	 * passes first, then the blended passes by ascending paint order. The
	 * transparent pass sorts by BLOCK NUMBER when the root is a BSOrderedNode,
	 * so whatever order the buckets are emitted in is the order they composite
	 * in. Sorting here is what makes that true; bucket 0 (bare) stays first. */
	QVector<int> remap( out.buckets.size(), 0 );
	QVector<CellSplatBucket> sorted;
	sorted.reserve( out.buckets.size() );
	QVector<int> idx;
	for ( int i = 1; i < out.buckets.size(); i++ )
		idx.append( i );
	std::stable_sort( idx.begin(), idx.end(), [&]( int a, int b ) {
		const CellSplatBucket & A = out.buckets.at( a );
		const CellSplatBucket & B = out.buckets.at( b );
		if ( A.blend != B.blend )
			return !A.blend;        // every opaque base before every blended layer
		return A.order < B.order;
	} );
	sorted.append( out.buckets.at( 0 ) );
	for ( int i = 0; i < idx.size(); i++ ) {
		remap[idx.at( i )] = sorted.size();
		sorted.append( out.buckets.at( idx.at( i ) ) );
	}
	out.buckets = sorted;
	for ( CellSplatQuad & q : out.quads )
		q.bucket = remap.value( q.bucket, 0 );

	out.notes << cellSplatLegend( out );
	return true;
}


QString cellSplatLegend( const CellSplatBuild & b )
{
	int used = 0;
	for ( const CellSplatBucket & k : b.buckets ) {
		if ( k.ltex && k.quads )
			used++;
	}
	const double mult = b.quadsTotal
		? double( b.quadsEmitted ) / double( b.quadsTotal ) : 0.0;
	QString s = QString( "ground: %L1 LAND cells (%L2 with VCLR), %L3 land quads drawn "
		"as %L4 passes over %L5 landscape textures -- %L6 opaque base, %L7 blended "
		"layer, %L8 bare; %L9 layers read" )
		.arg( b.cells ).arg( b.cellsWithColour ).arg( b.quadsTotal )
		.arg( b.quadsEmitted ).arg( used ).arg( b.quadsBase ).arg( b.quadsBlended )
		.arg( b.quadsBare ).arg( b.layersRead );
	s += QString( "; %1 passes per land quad" ).arg( mult, 0, 'f', 2 );
	/* lane TERRBLEND1: the game's sum, said in the legend so a census line tells
	 * the two builds apart -- the base is never missing, a quadrant without a
	 * BTXT takes the engine's default set, the layers are ADDED at their shares */
	s += QString( "; weighted sum as the engine (base share = 1 - layer shares, layers "
		"added): %L1 quads on %L2 quadrants with no BTXT took the engine default "
		"base, %L3 bases fell back to it because their LTEX named no texture, "
		"%L4 layers named form 0 and drew the default set, %L5 layers merged into a "
		"matching one or the base, %L6 layers past the fifth not drawn" )
		.arg( b.quadsDefaultBase ).arg( b.quadrantsDefaultBase ).arg( b.quadsBaseFallback )
		.arg( b.layersFormZero ).arg( b.layersMerged ).arg( b.layersDropped );
	if ( redNearest() )
		s += QStringLiteral( "; RED CONTROL WW_TERRBLEND_RED=nearest: one share per quad "
			"(its SW corner), the per-quad switching put back on purpose" );
	if ( b.ltexUnresolved )
		s += QString( "; %1 LTEX forms named no texture" ).arg( b.ltexUnresolved );
	/* THE ONE THING A READER MUST NOT HAVE TO ASK. Without the ATXT layer index
	 * the composite is in the order the layers happen to sit in the record,
	 * which is not the engine's paint order and can put gravel under grass that
	 * should be over it. A legend that did not say so would look like a slightly
	 * poor blend instead of a known-unknown. */
	if ( b.layersInRecordOrder )
		s += QString( "; WITHOUT THE PAINT ORDER -- %L1 layer passes were composited "
			"in RECORD order because the ATXT layer index is not read "
			"(WW_CELLSPLAT_LAYER_INDEX undefined), so the stacking is a guess" )
			.arg( b.layersInRecordOrder );
	else
		s += QStringLiteral( "; layers composited in the ATXT paint order" );
	return s;
}


/* lane TERRBLEND1: which blocks of which document are blended-ground shapes.
 * Same keying as the water registry (src/gl/cellwater.cpp): the NifModel
 * pointer the cell build wrote into, the block number the renderer draws. */
namespace {
QMutex & landMutex()
{
	static QMutex m;
	return m;
}
QHash<const void *, QSet<int>> & landDocs()
{
	static QHash<const void *, QSet<int>> d;
	return d;
}
} // namespace

void wwCellLandBegin( const void * nif )
{
	QMutexLocker lock( &landMutex() );
	landDocs().remove( nif );
}

void wwCellLandShape( const void * nif, int block )
{
	QMutexLocker lock( &landMutex() );
	landDocs()[nif].insert( block );
}

bool wwCellLandIs( const void * nif, int block )
{
	QMutexLocker lock( &landMutex() );
	const auto it = landDocs().constFind( nif );
	return it != landDocs().constEnd() && it->contains( block );
}
