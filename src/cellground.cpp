/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellground.h"

#include "esmdata.h"

#include <QFileInfo>
#include <QHash>

#include <cmath>

namespace {

constexpr float CELL_UNITS = 4096.0f;
constexpr int LAND_GRID = 33;       //!< heights per side
constexpr int QUAD_GRID = 17;       //!< a quadrant's opacity grid per side

/*! Which quadrant a 33x33 grid point falls in, and where inside it.
 *  docs/LODGEN_ESM_LAYOUTS.md: 0 BL, 1 BR, 2 TL, 3 TR, each 17x17 with the
 *  middle row and column SHARED, which is why the local index is the grid
 *  index minus 16 and not minus 17. */
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

} // namespace


bool cellBuildGround( EsmWorld & world, int x0, int y0, int x1, int y1,
	float originX, float originY, float tiling, CellGroundBuild & out, QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};

	out = CellGroundBuild();
	if ( x1 < x0 || y1 < y0 )
		return fail( QString( "the cell rectangle %1,%2 .. %3,%4 is empty" )
			.arg( x0 ).arg( y0 ).arg( x1 ).arg( y1 ) );
	const float T = ( tiling > 1.0f ) ? tiling : CELL_GROUND_TILING;

	// bucket 0 is always the bare one, so a quad that resolves nothing still
	// has somewhere to go and the count of bare quads is a real number
	CellGroundBucket bare;
	out.buckets.append( bare );

	QHash<quint32, int> bucketOfLtex;
	QHash<quint32, bool> ltexTried;

	auto bucketFor = [&]( quint32 ltex ) -> int {
		if ( !ltex )
			return 0;
		auto it = bucketOfLtex.constFind( ltex );
		if ( it != bucketOfLtex.constEnd() )
			return it.value();
		QString diffuse, normal;
		world.ltexTextures( ltex, diffuse, normal );
		if ( !ltexTried.contains( ltex ) ) {
			ltexTried.insert( ltex, !diffuse.isEmpty() );
			if ( diffuse.isEmpty() )
				out.ltexUnresolved++;
		}
		if ( diffuse.isEmpty() ) {
			bucketOfLtex.insert( ltex, 0 );
			return 0;
		}
		CellGroundBucket b;
		b.ltex = ltex;
		b.diffuse = diffuse;
		b.normal = normal;
		const int idx = out.buckets.size();
		out.buckets.append( b );
		bucketOfLtex.insert( ltex, idx );
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
					/* THE LAYER CHOICE, at this quad's own SW corner.  Highest
					 * opacity wins, and only above CELL_GROUND_LAYER_MIN; the
					 * quadrant's BTXT is what everything else falls back to.
					 * Ties go to the LATER layer, which is the one the engine
					 * would paint last.
					 *
					 * TWO REPAIRS, 2026-09-19, lane CELLVIEW3, both measured on
					 * Sanctuary -20,7 with an independent LAND decoder
					 * (scratchpad/cellview3_20260919/ground_probe.py) before a
					 * line of this was written.  133 of that cell's 1024 quads
					 * came out bare, and they were bare for two DIFFERENT
					 * reasons that the single rule above could not tell apart.
					 *
					 * (1) A LAYER WHOSE LTEX IS THE NULL FORM MAY NOT WIN.
					 * Quadrants 0, 1 and 2 of that cell each carry an ATXT whose
					 * LTEX is formid 00000000 -- no record, therefore no TXST,
					 * therefore no texture.  Wherever such a layer's opacity
					 * reached 0.5 it WON, replaced a perfectly good BTXT
					 * (0001F78C on all three quadrants) with nothing, and the
					 * quad went bare: 7 + 13 + 39 = 59 of the 133.  A layer that
					 * names no texture is not a choice, so it is no longer
					 * offered as one; the quad falls back to the BTXT that was
					 * there all along.
					 *
					 * (2) WITH NO BTXT, THE FLOOR IS ANY PAINT AT ALL.
					 * Quadrant 3 of that cell has NO BTXT subrecord, and 74
					 * quads there had no layer reaching 0.5.  The 0.5 floor is
					 * only meaningful as "beat the base": it decides whether a
					 * layer is dominant enough to replace the base texture.
					 * Where there is no base there is nothing to beat, and the
					 * strongest layer present IS the best answer the data has.
					 * There is also nothing else to inherit -- the Commonwealth
					 * WRLD record names no default landscape texture at all
					 * (every subrecord dumped in
					 * scratchpad/mountains_20260907/report_mountains.md R6: DNAM
					 * is two floats, land and water height, not formids), so
					 * "what the game does there" cannot be "use the worldspace
					 * default"; there is none.
					 *
					 * What survives both repairs is a quad whose quadrant has no
					 * BTXT and whose every layer is at ZERO opacity at its
					 * corner -- genuinely unpainted in the plugin.  Those are
					 * counted separately (`quadsUnpainted`) and are the whole of
					 * the remaining bare count; that identity is what the gate
					 * now asserts, and it is an invariant, not a threshold. */
					int qd = 0, lrow = 0, lcol = 0;
					quadrantOf( row, col, qd, lrow, lcol );
					const quint32 baseLtex = land.baseTex[qd];
					quint32 ltex = baseLtex;
					// with no base texture there is nothing to beat: any paint wins
					float best = baseLtex ? CELL_GROUND_LAYER_MIN : 0.0f;
					bool fromLayer = false;
					bool anyPaint = false;
					const QVector<EsmLandLayer> & layers = land.layers[qd];
					for ( int li = 0; li < layers.size(); li++ ) {
						const float o = layers[li].opacity[lrow][lcol];
						if ( o > 0.0f && layers[li].ltex )
							anyPaint = true;
						// a layer naming the null form is not a texture and cannot win
						if ( !layers[li].ltex )
							continue;
						if ( o >= best && o > 0.0f ) {
							best = o;
							ltex = layers[li].ltex;
							fromLayer = true;
						}
					}
					const int bucket = bucketFor( ltex );
					if ( fromLayer && bucket )
						out.quadsFromLayer++;
					if ( !baseLtex && !anyPaint )
						out.quadsUnpainted++;
					/* The OTHER way a quad can end up bare: it chose a real
					 * LTEX form and that form named no texture.  Counted apart
					 * so the two reasons add up to the bare count exactly --
					 * see the gate's invariant. */
					if ( ltex && !bucket )
						out.quadsLtexNoTexture++;

					CellGroundQuad qq;
					qq.bucket = bucket;
					const float xs[2] = { ox + float( col ) * step, ox + float( col + 1 ) * step };
					const float ys[2] = { oy + float( row ) * step, oy + float( row + 1 ) * step };
					const int rr[4] = { row, row, row + 1, row + 1 };
					const int cc[4] = { col, col + 1, col + 1, col };
					const float xx[4] = { xs[0], xs[1], xs[1], xs[0] };
					const float yy[4] = { ys[0], ys[0], ys[1], ys[1] };
					for ( int k = 0; k < 4; k++ ) {
						qq.v[k].p[0] = xx[k] - originX;
						qq.v[k].p[1] = yy[k] - originY;
						qq.v[k].p[2] = land.heights[rr[k]][cc[k]];
						/* WORLD-SPACE UVs, so the paint does not swim when the
						 * block grows: a texel belongs to a place, not to a
						 * quad.  v is flipped because a NIF's V runs down. */
						qq.v[k].uv[0] = xx[k] / T;
						qq.v[k].uv[1] = -yy[k] / T;
						if ( land.hasColors ) {
							for ( int c = 0; c < 3; c++ )
								qq.v[k].rgb[c] = float( land.colors[rr[k]][cc[k]][c] ) / 255.0f;
						}
					}
					// the face normal, from the two in-plane edges of the quad
					const float e1[3] = { qq.v[1].p[0] - qq.v[0].p[0],
						qq.v[1].p[1] - qq.v[0].p[1], qq.v[1].p[2] - qq.v[0].p[2] };
					const float e2[3] = { qq.v[3].p[0] - qq.v[0].p[0],
						qq.v[3].p[1] - qq.v[0].p[1], qq.v[3].p[2] - qq.v[0].p[2] };
					float n[3] = { e1[1] * e2[2] - e1[2] * e2[1],
						e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
					const float len = std::sqrt( n[0] * n[0] + n[1] * n[1] + n[2] * n[2] );
					if ( len > 1.0e-6f ) {
						for ( int k = 0; k < 3; k++ )
							qq.nrm[k] = n[k] / len;
					} else {
						qq.nrm[0] = qq.nrm[1] = 0.0f;
						qq.nrm[2] = 1.0f;
					}

					out.quads.push_back( qq );
					out.quadsTotal++;
					if ( bucket ) {
						out.quadsTextured++;
						out.buckets[bucket].quads++;
					} else {
						out.quadsBare++;
						out.buckets[0].quads++;
					}
				}
			}
		}
	}

	out.notes << cellGroundLegend( out );
	return true;
}

QString cellGroundLegend( const CellGroundBuild & b )
{
	int used = 0;
	for ( const CellGroundBucket & k : b.buckets ) {
		if ( k.ltex && k.quads )
			used++;
	}
	QString s = QString( "ground: %L1 LAND cells (%L2 with VCLR), %L3 quads over %L4 "
		"landscape textures -- %L5 textured, %L6 bare; %L7 quads took an ATXT layer "
		"over the quadrant's BTXT, out of %L8 layers read" )
		.arg( b.cells ).arg( b.cellsWithColour ).arg( b.quadsTotal ).arg( used )
		.arg( b.quadsTextured ).arg( b.quadsBare ).arg( b.quadsFromLayer )
		.arg( b.layersRead );
	if ( b.ltexUnresolved )
		s += QString( "; %1 LTEX forms named no texture" ).arg( b.ltexUnresolved );
	/* WHY EACH BARE QUAD IS BARE (lane CELLVIEW3). Without this the number
	 * "133 bare" was unreadable: it mixed quads the plugin never painted with
	 * quads a null-form layer had blanked, and only the second kind was a
	 * defect. The two numbers must sum to the bare count. */
	s += QString( "; of the bare quads %1 are unpainted in the plugin (no BTXT on "
		"the quadrant and no layer opacity at the corner -- and the worldspace "
		"names no default landscape texture to inherit) and %2 chose an LTEX that "
		"named no texture" ).arg( b.quadsUnpainted ).arg( b.quadsLtexNoTexture );
	s += QStringLiteral( ". The layers are NOT blended: each quad takes the strongest "
		"layer at its own corner, so the ground is a hard-edged mosaic of the real "
		"textures and not the engine's composite" );
	return s;
}
