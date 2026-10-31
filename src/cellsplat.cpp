/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellsplat.h"
#include "cellgroundlegend.h"	// lane DUNESAND3: the TX07 clause a live set rewrites
#include "wwswitch.h"	// lane TESTDRIVER1: the WW_* switch store

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

/*! Which quadrant a 33x33 grid point falls in, and where inside it (0 BL, 1 BR, 2 TL, 3 TR).
 *  The middle row and column are SHARED, so the local index is grid minus 16 and the weights
 *  are continuous across quadrant boundaries without interpolation. */
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

/*! The paint order: the ATXT layer index (int16 at payload offset 6). Until esmdata reads it,
 *  the record's own order is used and counted in `layersInRecordOrder`. */
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

/*! WW_TERRBLEND_RED=nearest: one weight per 128-unit quad (its SW corner), for testing only. */
bool redNearest()
{
	WW_SWITCH_STATIC( bool, on, wwEnv( "WW_TERRBLEND_RED" ) == "nearest" );
	return on;
}

}	// namespace: cellSplatSpecRed is declared in cellsplat.h

bool cellSplatSpecRed()
{
	WW_SWITCH_STATIC( bool, on, wwEnv( "WW_TERRBLEND_RED" ) == "nospec" );
	return on;
}

namespace {

//! WW_TERRBLEND_RED=facenormal: every corner lit with its quad's face normal, for testing only
bool redFaceNormal()
{
	WW_SWITCH_STATIC( bool, on, wwEnv( "WW_TERRBLEND_RED" ) == "facenormal" );
	return on;
}

/*! One quadrant's layers as the engine builds them: slot = layer index clamped to 11, opacities
 *  stored as truncated bytes; base = BTXT or the engine default set, never "no base"; matching
 *  slots summed, a slot equal to the base dropped into it; base weight = clamp(255 - sum, 0, 255);
 *  only the first five layers drawn. Pixel = sum(w_i * tex_i) * VCLR: shares, not alpha-over. */
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
		/* An ATXT with LTEX 0 is the engine's default texture set, not "nothing"; dropping it
		 * leaves dark hard-edged squares. */
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

/*! Every pass one 128-unit quad needs: the base always (opaque, its own share in vertex alpha),
 *  then each kept layer with any share. Passes are ADDED, so draw order does not matter. */
int passesFor( const QuadLayers ql[4], int row, int col, Pass * out, int maxOut )
{
	/* The quad's SW corner picks the quadrant; quads never straddle one (the boundary is
	 * grid line 16, a quad edge). */
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

	/* A bucket is a (texture, pass kind) pair: the same texture can be an opaque base in one
	 * quadrant and a blended layer in another, which need different alpha properties. */
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
		/* lane DUNESAND1: the game's Landscape shader samples each layer's TX07 _s (gloss, spec mask); without it the
		 * ground drew at smoothness 1 / mask 1 and 1.4x brighter than a static with the same sand. A material-backed set's
		 * .bgsm names its own. */
		const EsmLtexTextureSet & set = world.ltexTextureSet( ltex );
		if ( set.material.isEmpty() )
			b.specWant = set.specular;
		if ( !cellSplatSpecRed() )
			b.spec = b.specWant;
		if ( !b.spec.isEmpty() )
			out.bucketsSpec++;
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
						/* World-space UVs so the paint does not swim when the block grows; v grows
						 * northward as in the game. Shared by every pass so layers align with the base. */
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
					/* Each corner is lit with the LAND's vertex normal (VNML, else central differences);
					 * one face normal per quad stepped the light into squares. */
					for ( int k = 0; k < 4; k++ ) {
						float * vn = proto.v[k].n;
						if ( redFaceNormal() ) {
							for ( int c = 0; c < 3; c++ )
								vn[c] = proto.nrm[c];
						} else if ( land.hasNormals ) {
							for ( int c = 0; c < 3; c++ )
								vn[c] = land.normals[rr[k]][cc[k]][c];
						} else {
							const int r = rr[k], c = cc[k];
							const int c0 = qMax( c - 1, 0 ), c1 = qMin( c + 1, LAND_GRID - 1 );
							const int r0 = qMax( r - 1, 0 ), r1 = qMin( r + 1, LAND_GRID - 1 );
							const float gx = ( land.heights[r][c1] - land.heights[r][c0] ) / ( float( c1 - c0 ) * step );
							const float gy = ( land.heights[r1][c] - land.heights[r0][c] ) / ( float( r1 - r0 ) * step );
							const float m = std::sqrt( gx * gx + gy * gy + 1.0f );
							vn[0] = -gx / m;
							vn[1] = -gy / m;
							vn[2] = 1.0f / m;
						}
					}

					int emitted = 0;
					for ( int pi = 0; pi < np; pi++ ) {
						const Pass & p = passes[pi];
						int bucket = bucketFor( p.ltex, p.order, p.blend );
						if ( !bucket && !p.blend ) {
							/* A base whose LTEX names no texture must still draw (else sky shows
							 * through): the engine default set stands in, and is counted. */
							bucket = bucketFor( ESM_LTEX_ENGINE_DEFAULT, p.order, false );
							if ( bucket )
								out.quadsBaseFallback++;
						}
						if ( !bucket )
							continue;   // the LTEX named no texture: not drawn, counted
						CellSplatQuad qq = proto;
						qq.bucket = bucket;
							/* The share, on the base pass too: the shader scales the opaque base by
							 * its share, and every layer is ADDED at its share. */
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

	/* Draw order is bucket order: the transparent pass sorts by block number under a
	 * BSOrderedNode, so opaque bases first, then blended passes by paint order; bucket 0 stays first. */
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
	int used = 0, specWant = 0;
	for ( const CellSplatBucket & k : b.buckets ) {
		if ( k.ltex && k.quads )
			used++;
		if ( !k.specWant.isEmpty() )
			specWant++;
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
	/* the engine's weighted sum, stated in the legend */
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
	s += cellGroundSpecClause( cellSplatSpecRed(), specWant, b.buckets.size(), QStringLiteral( "passes" ) );
	/* Without the ATXT layer index the stacking is a guess; the legend must say so. */
	if ( b.layersInRecordOrder )
		s += QString( "; WITHOUT THE PAINT ORDER -- %L1 layer passes were composited "
			"in RECORD order because the ATXT layer index is not read "
			"(WW_CELLSPLAT_LAYER_INDEX undefined), so the stacking is a guess" )
			.arg( b.layersInRecordOrder );
	else
		s += QStringLiteral( "; layers composited in the ATXT paint order" );
	return s;
}


/* Which blocks of which document are blended-ground shapes, keyed like the water registry
 * (src/gl/cellwater.cpp): NifModel pointer plus block number. */
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
QHash<const void *, QHash<int, QString>> & groundSpecDocs()
{
	static QHash<const void *, QHash<int, QString>> d;
	return d;
}
struct LandOrigin
{
	QString ws;
	float x = 0.0f, y = 0.0f;
	int bx = 0, by = 0, half = -1;	// lane LANDSPEC4: the near block (-1 none)
	bool fixed = false;	// lane LANDSPEC6: a fixed world, its WCTR cell
	int wx = 0, wy = 0;
};
QHash<const void *, LandOrigin> & landOrigins()
{
	static QHash<const void *, LandOrigin> d;
	return d;
}
} // namespace

void wwCellLandOrigin( const void * nif, const QString & ws, float ox, float oy )
{
	QMutexLocker lock( &landMutex() );
	landOrigins().insert( nif, { ws, ox, oy } );
}

void wwCellLandBlock( const void * nif, int cx, int cy, int half )
{
	QMutexLocker lock( &landMutex() );
	LandOrigin & o = landOrigins()[nif];
	o.bx = cx;
	o.by = cy;
	o.half = half;
}

bool wwCellLandBlockOf( const void * nif, int * cx, int * cy, int * half )
{
	QMutexLocker lock( &landMutex() );
	const auto it = landOrigins().constFind( nif );
	if ( it == landOrigins().constEnd() || it->half < 0 )
		return false;
	*cx = it->bx;
	*cy = it->by;
	*half = it->half;
	return true;
}

void wwCellLandFixed( const void * nif, bool fixed, int wx, int wy )
{
	QMutexLocker lock( &landMutex() );
	LandOrigin & o = landOrigins()[nif];
	o.fixed = fixed;
	o.wx = wx;
	o.wy = wy;
}

bool wwCellLandFixedOf( const void * nif, int * wx, int * wy )
{
	QMutexLocker lock( &landMutex() );
	const auto it = landOrigins().constFind( nif );
	if ( it == landOrigins().constEnd() || !it->fixed )
		return false;
	*wx = it->wx;
	*wy = it->wy;
	return true;
}

bool wwCellLandOriginOf( const void * nif, QString * ws, float * ox, float * oy )
{
	QMutexLocker lock( &landMutex() );
	const auto it = landOrigins().constFind( nif );
	if ( it == landOrigins().constEnd() || it->ws.isEmpty() )
		return false;
	*ws = it->ws;
	*ox = it->x;
	*oy = it->y;
	return true;
}

void wwCellLandBegin( const void * nif )
{
	QMutexLocker lock( &landMutex() );
	landDocs().remove( nif );
	groundSpecDocs().remove( nif );
}

void wwCellGroundSpecShape( const void * nif, int block, const QString & spec )
{
	QMutexLocker lock( &landMutex() );
	groundSpecDocs()[nif].insert( block, spec );
}

QHash<int, QString> wwCellGroundSpecs( const void * nif )
{
	QMutexLocker lock( &landMutex() );
	return groundSpecDocs().value( nif );
}

void wwCellLandForget( const void * nif, int block )
{
	QMutexLocker lock( &landMutex() );
	auto d = landDocs().find( nif );
	if ( d != landDocs().end() )
		d->remove( block );
	auto g = groundSpecDocs().find( nif );
	if ( g != groundSpecDocs().end() )
		g->remove( block );
}

void wwCellLandRemap( const void * nif, const WwBlockMap & m )
{
	QMutexLocker lock( &landMutex() );
	auto d = landDocs().find( nif );
	if ( d != landDocs().end() )
		wwRemapKeys( *d, m );
	auto g = groundSpecDocs().find( nif );
	if ( g != groundSpecDocs().end() )
		wwRemapKeys( *g, m );
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
