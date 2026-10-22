/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellsplat.h"

#include "esmdata.h"

#include <QHash>

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

/*! The opacity one layer OF QUADRANT `q` carries at a 33x33 grid point.
 *
 *  THE QUADRANT IS THE CALLER'S, NOT THE POINT'S, and that distinction is the
 *  whole correctness of the blend. `quadrantOf` answers "which quadrant OWNS
 *  this point", and on the shared middle row and column it answers with the
 *  top/right one. But a quad in quadrant 0 has corners at row 16 and col 16,
 *  and those corners' weights must come from QUADRANT 0's grid -- where they
 *  are the last row and column -- not from quadrant 2's, whose layer list is a
 *  different list of different textures in a different order. Asking the point
 *  would silently index layer 2 of the wrong quadrant along every quad that
 *  touches the cell's centre lines: 124 of the 1024 quads of every cell. */
float weightAt( const EsmLand & land, int q, int row, int col, int layerSlot )
{
	const QVector<EsmLandLayer> & ls = land.layers[q];
	if ( layerSlot < 0 || layerSlot >= ls.size() )
		return 0.0f;
	const int lrow = qBound( 0, row - ( q >= 2 ? QUAD_GRID - 1 : 0 ), QUAD_GRID - 1 );
	const int lcol = qBound( 0, col - ( ( q & 1 ) ? QUAD_GRID - 1 : 0 ), QUAD_GRID - 1 );
	return ls.at( layerSlot ).opacity[lrow][lcol];
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

/*! Every pass one 128-unit quad needs, in draw order.
 *
 *  The base first when the quadrant has a BTXT; then each layer that has any
 *  weight at any of the four corners, ascending by paint order. A quadrant
 *  with NO BTXT has no opaque base, and the void would show through every
 *  layer's transparency -- so the FIRST layer with weight is promoted to
 *  opaque there, which is the same thing the simulation does and the reason
 *  its blended picture has no holes. */
int passesFor( const EsmLand & land, int row, int col, Pass * out, int maxOut,
	bool & promoted )
{
	/* The quadrant is the one the quad's OWN SW corner falls in, and every one
	 * of its four corners is then read out of THAT quadrant's grids -- see
	 * weightAt. A 128-unit quad never straddles two quadrants: the quadrant
	 * boundary runs along grid line 16, which is a quad EDGE, not a quad. */
	int q = 0, lrow = 0, lcol = 0;
	quadrantOf( row, col, q, lrow, lcol );
	(void) lrow;
	(void) lcol;
	const QVector<EsmLandLayer> & ls = land.layers[q];
	promoted = false;

	int n = 0;
	if ( land.baseTex[q] && n < maxOut ) {
		Pass p;
		p.ltex = land.baseTex[q];
		p.order = -1;
		p.blend = false;
		out[n++] = p;
	}

	// the quad's four corners on the 33x33 grid, SW CCW, as the geometry below
	const int rr[4] = { row, row, row + 1, row + 1 };
	const int cc[4] = { col, col + 1, col + 1, col };

	// collect, then sort by the engine's paint order
	struct Cand { int order; int slot; float w[4]; };
	std::vector<Cand> cand;
	cand.reserve( size_t( ls.size() ) );
	for ( int li = 0; li < ls.size(); li++ ) {
		if ( !ls.at( li ).ltex )
			continue;               // a null form paints nothing
		Cand c;
		c.order = layerOrder( ls.at( li ), li );
		c.slot = li;
		float top = 0.0f;
		for ( int k = 0; k < 4; k++ ) {
			c.w[k] = weightAt( land, q, rr[k], cc[k], li );
			top = qMax( top, c.w[k] );
		}
		if ( top < CELL_SPLAT_WEIGHT_MIN )
			continue;               // contributes nothing anywhere on this quad
		cand.push_back( c );
	}
	std::stable_sort( cand.begin(), cand.end(),
		[]( const Cand & a, const Cand & b ) { return a.order < b.order; } );

	for ( size_t i = 0; i < cand.size() && n < maxOut; i++ ) {
		Pass p;
		p.ltex = ls.at( cand[i].slot ).ltex;
		p.order = cand[i].order;
		/* The promotion: with no BTXT the first pass on this quad has nothing
		 * underneath it, so it is drawn opaque. Its own weights are kept for
		 * the census but the pass writes 1.0 -- see the emit below. */
		p.blend = !( n == 0 );
		if ( n == 0 && !land.baseTex[q] )
			promoted = true;
		for ( int k = 0; k < 4; k++ )
			p.w[k] = cand[i].w[k];
		out[n++] = p;
	}
	return n;
}

//! The passes a cell would emit, without building any of them.
qint64 countCell( const EsmLand & land )
{
	qint64 n = 0;
	Pass passes[16];
	bool promoted = false;
	for ( int row = 0; row + 1 < LAND_GRID; row++ ) {
		for ( int col = 0; col + 1 < LAND_GRID; col++ )
			n += passesFor( land, row, col, passes, 16, promoted );
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
			for ( int q = 0; q < 4; q++ )
				out.layersRead += land.layers[q].size();

			const float ox = float( x ) * CELL_UNITS;
			const float oy = float( y ) * CELL_UNITS;
			const float step = CELL_UNITS / float( LAND_GRID - 1 );

			for ( int row = 0; row + 1 < LAND_GRID; row++ ) {
				for ( int col = 0; col + 1 < LAND_GRID; col++ ) {
					out.quadsTotal++;

					Pass passes[16];
					bool promoted = false;
					const int np = passesFor( land, row, col, passes, 16, promoted );
					if ( promoted )
						out.quadsPromotedBase++;
					if ( !np ) {
						out.quadsBare++;
						out.quadsBareUnpainted++;
						out.buckets[0].quads++;
						continue;
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
						 * v is flipped because a NIF's V runs down. Every pass on
						 * the quad shares them, which is what makes the layers
						 * line up with the base instead of sliding over it. */
						proto.v[k].uv[0] = xx[k] / T;
						proto.v[k].uv[1] = -yy[k] / T;
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
						const int bucket = bucketFor( p.ltex, p.order, p.blend );
						if ( !bucket )
							continue;   // the LTEX named no texture: not drawn, counted
						CellSplatQuad qq = proto;
						qq.bucket = bucket;
						for ( int k = 0; k < 4; k++ )
							qq.v[k].w = p.blend ? qBound( 0.0f, p.w[k], 1.0f ) : 1.0f;
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
		s += QString( "; of the bare quads %L1 are unpainted and %L2 chose an LTEX "
		"that named no texture" ).arg( b.quadsBareUnpainted ).arg( b.quadsBareNoTexture );
	if ( b.quadsPromotedBase )
		s += QString( "; %L1 quads had no BTXT on the quadrant and the first layer "
			"with any weight was drawn opaque under the rest" ).arg( b.quadsPromotedBase );
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
