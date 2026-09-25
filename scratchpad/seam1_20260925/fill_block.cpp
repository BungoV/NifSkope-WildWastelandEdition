/*! THE VANILLA-COLOUR FILL (lane SEAM1, 2026-09-25; docs/LODGEN_TERRAIN_VT.md §2.6).
 *
 *  bungo, on the whole-map overview: "blend the colors of the terrain not
 *  covered by the in-game terrain blended tiles, to the vanilla color on those
 *  tiles, from the bakes bethesda did" -- and "it needs to be done in a proper
 *  way". Measured on the Commonwealth: 2,023 of its 2,304 dim-4 chunks carry
 *  LAND with no BTXT on any quadrant and no ATXT layer at all. The §2.5 law
 *  paints those with the engine's one default texture; Bethesda's own LOD
 *  diffuse for the same cells carries the region's colour. This stage takes
 *  that colour, at bake time only (the vanilla sheets are READ, never written
 *  or shipped), and blends it in on the UNPAINTED side only:
 *
 *    painted cell   a LAND quadrant with a BTXT or any ATXT layer (a NULL-LTEX
 *                   layer counts: it is paint intent). w = 0, so his MO2 LAND
 *                   layers win and every painted texel is byte-identical.
 *    d              world distance from the texel to the nearest painted cell
 *    w              smoothstep( 0, band, d )
 *    colour         colour + ( T( V ) - colour ) * w
 *    V              Bethesda's dim-4 LOD diffuse,
 *                   Textures\Terrain\<WS>\<WS>.4.<x>.<y>.dds, (x, y) the chunk's
 *                   SW cell, 512 texels = 32 units a texel, row 0 = north;
 *                   read with a Mitchell-Netravali bicubic (B = C = 1/3), which
 *                   neither rings nor blurs a 2x upsample the way bilinear does
 *    T              the tone + saturation match, FITTED once per bake on the
 *                   OVERLAP (painted cells with an unpainted cell within
 *                   LODGEN_VT_FILL_RING cells): luminance offset (gain capped
 *                   at 1 -- the vanilla sheet carries baked relief light and
 *                   a gain above 1 amplifies it past its own steps, measured),
 *                   chroma scaled by the RMS ratio and shifted by the
 *                   mean-chroma difference. Cell means on both sides, because
 *                   one side is a point-sampled texture and the other a 32-unit
 *                   LOD sheet: their per-texel spreads are different quantities.
 *    band           MEASURED: ceil( p95 over the unpainted ring of
 *                   |lum ours - lum T(V)| / bar ) cells, at least 1, at most 8;
 *                   bar = p99 of vanilla's own adjacent cell-mean steps over
 *                   the overlap and the ring.
 *
 *  The fit reads OUR side from a low-resolution bake of the overlap tiles
 *  through lodgenBakeVtTile itself (16 texels a tile side), so the tone it
 *  matches includes everything the composite does -- VCLR, the grass tint,
 *  roads, the grade -- with no second copy of the law to drift.
 *
 *  Applied to every FINEST-level tile after its bake; the coarser levels, their
 *  mips and the assembled chunk sheets inherit it through the existing box
 *  filter. A missing vanilla tile leaves its texels alone (counted). */
static const int LODGEN_VT_FILL_RING = 3;
static const int LODGEN_VT_FILL_MAX_BAND = 8;

struct LodgenVtFill
{
	bool on = false;
	QString ws;
	int x0 = 0, y0 = 0, w = 0, h = 0;           //!< painted bitmap, world cells
	std::vector<quint8> painted;
	float gain = 1.0f, offset = 0.0f, sat = 1.0f, cshift[3] = { 0, 0, 0 };
	float bar = 0.0f, p95 = 0.0f, rawGain = 1.0f;
	int bandCells = 1;
	float band = 4096.0f;
	qint64 overlapCells = 0, ringCells = 0, fitTiles = 0;
	qint64 tilesTouched = 0, texelsFilled = 0, texelsNoVanilla = 0;
	QSet<qint64> vanillaMissing;

	bool isPainted( int cx, int cy ) const
	{
		if ( cx < x0 || cy < y0 || cx >= x0 + w || cy >= y0 + h )
			return false;
		return painted[size_t( cy - y0 ) * w + ( cx - x0 )] != 0;
	}

	static void lumChroma( const float * c, float & l )
	{
		l = 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
	}

	void tone( const float * v, float * out ) const
	{
		float l;
		lumChroma( v, l );
		const float lt = gain * l + offset;
		for ( int k = 0; k < 3; k++ )
			out[k] = qBound( 0.0f, lt + sat * ( v[k] - l ) + cshift[k], 1.0f );
	}

	static qint64 key( int cx, int cy )
	{
		return ( qint64( cx ) << 32 ) ^ qint64( quint32( cy ) );
	}

	QString vanillaPath( int chunkX, int chunkY ) const
	{
		return QString( "Terrain\\%1\\%1.4.%2.%3.dds" ).arg( ws ).arg( chunkX ).arg( chunkY );
	}

	/*! Vanilla texels over the global texel rectangle [gx0, gx1] x [gy0, gy1]
	 *  (global texel k covers world [32k, 32k + 32) on x; on y the index runs
	 *  NORTH-down: texel row r covers world y [-(r + 1) * 32, -r * 32)), as RGB
	 *  floats; `have` marks the texels whose tile exists. */
	void window( LodgenBakeCaches & bc, const QString & dataRoot, int gx0, int gy0, int gx1, int gy1,
		std::vector<float> & rgb, std::vector<quint8> & have )
	{
		const int ww = gx1 - gx0 + 1, wh = gy1 - gy0 + 1;
		rgb.assign( size_t( ww ) * wh * 3, 0.0f );
		have.assign( size_t( ww ) * wh, 0 );
		// the dim-4 chunks the window touches: chunk columns by 512 texels
		auto floorDiv = []( int a, int b ) { return ( a >= 0 ) ? a / b : -( ( -a + b - 1 ) / b ); };
		for ( int cr = floorDiv( gy0, 512 ); cr <= floorDiv( gy1, 512 ); cr++ ) {
			for ( int cc = floorDiv( gx0, 512 ); cc <= floorDiv( gx1, 512 ); cc++ ) {
				const int chunkX = cc * 4;
				// row index r runs north-down from world y = 0: chunk row cr spans
				// world y [-(cr + 1) * 16384, -cr * 16384), SW cell y = -(cr + 1) * 4
				const int chunkY = -( cr + 1 ) * 4;
				const qint64 k = key( chunkX, chunkY );
				if ( vanillaMissing.contains( k ) )
					continue;
				const DDSTexture16 * t = lodgenCachedTexture( bc, dataRoot, vanillaPath( chunkX, chunkY ) );
				if ( !t || t->getWidth() != 512 || t->getHeight() != 512 ) {
					vanillaMissing.insert( k );
					continue;
				}
				const int tx0 = qMax( gx0, cc * 512 ), tx1 = qMin( gx1, cc * 512 + 511 );
				const int ty0 = qMax( gy0, cr * 512 ), ty1 = qMin( gy1, cr * 512 + 511 );
				for ( int gy = ty0; gy <= ty1; gy++ ) {
					for ( int gx = tx0; gx <= tx1; gx++ ) {
						const FloatVector4 c = FloatVector4::convertFloat16(
							t->getPixelC( gx - cc * 512, gy - cr * 512, 0 ) );
						const size_t o = size_t( gy - gy0 ) * ww + ( gx - gx0 );
						rgb[o * 3 + 0] = c[0];
						rgb[o * 3 + 1] = c[1];
						rgb[o * 3 + 2] = c[2];
						have[o] = 1;
					}
				}
			}
		}
	}

	static float mitchell( float t )
	{
		t = std::fabs( t );
		const float b = 1.0f / 3.0f, c = 1.0f / 3.0f;
		if ( t < 1.0f )
			return ( ( 12 - 9 * b - 6 * c ) * t * t * t + ( -18 + 12 * b + 6 * c ) * t * t + ( 6 - 2 * b ) ) / 6.0f;
		if ( t < 2.0f )
			return ( ( -b - 6 * c ) * t * t * t + ( 6 * b + 30 * c ) * t * t + ( -12 * b - 48 * c ) * t
				+ ( 8 * b + 24 * c ) ) / 6.0f;
		return 0.0f;
	}
};

//! The Mitchell tap at world (wx, wy) out of a window from LodgenVtFill::window.
static bool lodgenVtFillSample( const std::vector<float> & rgb, const std::vector<quint8> & have,
	int gx0, int gy0, int ww, int wh, float wx, float wy, float * out )
{
	// continuous texel coordinates, texel centres at k + 0.5
	const float u = wx / 32.0f - 0.5f;
	const float v = -wy / 32.0f - 0.5f;
	const int iu = int( std::floor( u ) ), iv = int( std::floor( v ) );
	float acc[3] = { 0, 0, 0 }, ws = 0.0f;
	for ( int dv = -1; dv <= 2; dv++ ) {
		const int gy = iv + dv;
		if ( gy < gy0 || gy >= gy0 + wh )
			continue;
		const float wv = LodgenVtFill::mitchell( v - float( gy ) );
		for ( int du = -1; du <= 2; du++ ) {
			const int gx = iu + du;
			if ( gx < gx0 || gx >= gx0 + ww )
				continue;
			const size_t o = size_t( gy - gy0 ) * ww + ( gx - gx0 );
			if ( !have[o] )
				continue;
			const float wgt = LodgenVtFill::mitchell( u - float( gx ) ) * wv;
			for ( int k = 0; k < 3; k++ )
				acc[k] += rgb[o * 3 + k] * wgt;
			ws += wgt;
		}
	}
	if ( ws < 0.5f )     // under half the kernel had a tile: no vanilla colour here
		return false;
	for ( int k = 0; k < 3; k++ )
		out[k] = qBound( 0.0f, acc[k] / ws, 1.0f );
	return true;
}

/*! Build the painted bitmap over the bake's finest-level rectangle plus a margin
 *  that covers the ring and the widest band. */
static void lodgenVtFillPainted( const EsmWorld & world, LodgenVtFill & F, int west, int south,
	int east, int north )
{
	const int m = LODGEN_VT_FILL_RING + LODGEN_VT_FILL_MAX_BAND + 1;
	F.x0 = west - m;
	F.y0 = south - m;
	F.w = east - west + 1 + 2 * m;
	F.h = north - south + 1 + 2 * m;
	F.painted.assign( size_t( F.w ) * F.h, 0 );
	for ( int cy = F.y0; cy < F.y0 + F.h; cy++ ) {
		for ( int cx = F.x0; cx < F.x0 + F.w; cx++ ) {
			EsmLand land;
			if ( !world.land( cx, cy, land ) )
				continue;
			bool p = false;
			for ( int q = 0; q < 4 && !p; q++ )
				p = land.baseTex[q] != 0 || !land.layers[q].isEmpty();
			F.painted[size_t( cy - F.y0 ) * F.w + ( cx - F.x0 )] = p ? 1 : 0;
		}
	}
}

/*! The painted cells within `r` cells (Chebyshev) of cell (cx, cy), as their
 *  SW corners in world units; empty when (cx, cy) is itself painted (d = 0). */
static void lodgenVtFillNeighbours( const LodgenVtFill & F, int cx, int cy, int r,
	std::vector<std::pair<float, float>> & out )
{
	out.clear();
	for ( int dy = -r; dy <= r; dy++ )
		for ( int dx = -r; dx <= r; dx++ )
			if ( F.isPainted( cx + dx, cy + dy ) )
				out.emplace_back( float( cx + dx ) * 4096.0f, float( cy + dy ) * 4096.0f );
}

//! Distance, world units, from (wx, wy) to the nearest of those cells; 1e30 if none.
static float lodgenVtFillDistance( const std::vector<std::pair<float, float>> & cells, float wx, float wy )
{
	float best = 1e30f;
	for ( const auto & c : cells ) {
		const float ex = qMax( qMax( c.first - wx, wx - ( c.first + 4096.0f ) ), 0.0f );
		const float ey = qMax( qMax( c.second - wy, wy - ( c.second + 4096.0f ) ), 0.0f );
		best = qMin( best, std::sqrt( ex * ex + ey * ey ) );
	}
	return best;
}

//! Apply the fill to one finest-level tile (colour plane only).
static void lodgenVtFillTile( LodgenVtFill & F, LodgenBakeCaches & bc, const QString & dataRoot,
	int cellX0, int cellY0, int dim, int content, int border, LodgenVtStage & out )
{
	const int S = content + 2 * border;
	const float upt = float( dim ) * 4096.0f / float( content );
	const float tileW = float( cellX0 ) * 4096.0f;
	const float tileN = float( cellY0 + dim ) * 4096.0f;
	// fast outs: every cell the tile (with its border) reaches is painted
	const int bc0 = int( std::floor( ( tileW - border * upt ) / 4096.0f ) );
	const int bc1 = int( std::floor( ( tileW + ( content + border ) * upt - 1.0f ) / 4096.0f ) );
	const int br0 = int( std::floor( ( tileN - ( content + border ) * upt ) / 4096.0f ) );
	const int br1 = int( std::floor( ( tileN + border * upt - 1.0f ) / 4096.0f ) );
	bool anyUnpainted = false;
	for ( int cy = br0; cy <= br1 && !anyUnpainted; cy++ )
		for ( int cx = bc0; cx <= bc1 && !anyUnpainted; cx++ )
			anyUnpainted = !F.isPainted( cx, cy );
	if ( !anyUnpainted )
		return;
	// the vanilla window: the tile's world rectangle plus the kernel's reach
	const float wx0 = tileW - border * upt, wx1 = tileW + ( content + border ) * upt;
	const float wyN = tileN + border * upt, wyS = tileN - ( content + border ) * upt;
	const int gx0 = int( std::floor( wx0 / 32.0f ) ) - 2, gx1 = int( std::floor( wx1 / 32.0f ) ) + 2;
	const int gy0 = int( std::floor( -wyN / 32.0f ) ) - 2, gy1 = int( std::floor( -wyS / 32.0f ) ) + 2;
	std::vector<float> rgb;
	std::vector<quint8> have;
	F.window( bc, dataRoot, gx0, gy0, gx1, gy1, rgb, have );
	const int ww = gx1 - gx0 + 1, wh = gy1 - gy0 + 1;
	const int r = F.bandCells + 1;
	// per cell of the tile's reach: painted -> skipped; else its painted neighbours
	const int ncx = bc1 - bc0 + 1, ncy = br1 - br0 + 1;
	std::vector<std::vector<std::pair<float, float>>> nb( size_t( ncx ) * ncy );
	std::vector<quint8> cellPainted( size_t( ncx ) * ncy );
	for ( int cy = br0; cy <= br1; cy++ )
		for ( int cx = bc0; cx <= bc1; cx++ ) {
			const size_t o = size_t( cy - br0 ) * ncx + ( cx - bc0 );
			cellPainted[o] = F.isPainted( cx, cy ) ? 1 : 0;
			if ( !cellPainted[o] )
				lodgenVtFillNeighbours( F, cx, cy, r, nb[o] );
		}
	bool touched = false;
	for ( int j = 0; j < S; j++ ) {
		const float wy = tileN - ( float( j ) - float( border ) + 0.5f ) * upt;
		const int cy = qBound( br0, int( std::floor( wy / 4096.0f ) ), br1 );
		for ( int i = 0; i < S; i++ ) {
			const float wx = tileW + ( float( i ) - float( border ) + 0.5f ) * upt;
			const int cx = qBound( bc0, int( std::floor( wx / 4096.0f ) ), bc1 );
			const size_t co = size_t( cy - br0 ) * ncx + ( cx - bc0 );
			if ( cellPainted[co] )
				continue;       // his LAND paint: byte-identical
			const float d = lodgenVtFillDistance( nb[co], wx, wy );
			const float t = qBound( 0.0f, d / F.band, 1.0f );
			const float wgt = t * t * ( 3.0f - 2.0f * t );
			if ( wgt <= 0.0f )
				continue;
			float v[3], tv[3];
			if ( !lodgenVtFillSample( rgb, have, gx0, gy0, ww, wh, wx, wy, v ) ) {
				F.texelsNoVanilla++;
				continue;
			}
			F.tone( v, tv );
			quint32 & px = out.colour[size_t( j ) * S + i];
			float c[3] = { float( ( px >> 16 ) & 0xFF ) / 255.0f, float( ( px >> 8 ) & 0xFF ) / 255.0f,
				float( px & 0xFF ) / 255.0f };
			for ( int k = 0; k < 3; k++ )
				c[k] = c[k] + ( tv[k] - c[k] ) * wgt;
			px = ( px & 0xFF000000U )
				| ( quint32( qBound( 0, int( c[0] * 255.0f + 0.5f ), 255 ) ) << 16 )
				| ( quint32( qBound( 0, int( c[1] * 255.0f + 0.5f ), 255 ) ) << 8 )
				| quint32( qBound( 0, int( c[2] * 255.0f + 0.5f ), 255 ) );
			F.texelsFilled++;
			touched = true;
		}
	}
	if ( touched )
		F.tilesTouched++;
}

static float lodgenVtFillPercentile( std::vector<float> v, float p )
{
	if ( v.empty() )
		return 0.0f;
	std::sort( v.begin(), v.end() );
	// numpy's default (linear) percentile, so the offline model and this agree
	const double pos = double( p ) / 100.0 * double( v.size() - 1 );
	const size_t lo = size_t( std::floor( pos ) ), hi = qMin( lo + 1, v.size() - 1 );
	return float( v[lo] + ( v[hi] - v[lo] ) * ( pos - double( lo ) ) );
}

/*! THE FIT: our cell means from a 16-texel bake of every finest tile that holds
 *  an overlap or ring cell, vanilla's from the mean of its 128 x 128 texels a
 *  cell. Sets gain/offset/sat/cshift, bar, p95 and the band. */
template <typename BakeFn>
static void lodgenVtFillFit( LodgenVtFill & F, LodgenBakeCaches & bc, const QString & dataRoot,
	int west, int south, int east, int north, int dim, BakeFn bakeSmall )
{
	const int R = LODGEN_VT_FILL_RING;
	auto nearOther = [&]( int cx, int cy, bool wantPainted ) {
		for ( int dy = -R; dy <= R; dy++ )
			for ( int dx = -R; dx <= R; dx++ )
				if ( F.isPainted( cx + dx, cy + dy ) == wantPainted )
					return true;
		return false;
	};
	// 0 none, 1 overlap (painted, unpainted near), 2 ring (unpainted, painted near)
	QHash<qint64, int> role;
	for ( int cy = south; cy <= north; cy++ )
		for ( int cx = west; cx <= east; cx++ ) {
			const bool p = F.isPainted( cx, cy );
			if ( nearOther( cx, cy, !p ) )
				role.insert( LodgenVtFill::key( cx, cy ), p ? 1 : 2 );
		}
	QHash<qint64, std::array<float, 3>> ours, van;
	// OUR side: bake each finest tile holding a role cell once, at 16 texels
	QSet<qint64> tilesDone;
	const int C = 16, Bd = 4, S = C + 2 * Bd, perCell = C / dim;
	for ( auto it = role.constBegin(); it != role.constEnd(); ++it ) {
		const int cx = int( it.key() >> 32 ), cy = int( qint32( quint32( it.key() & 0xFFFFFFFF ) ) );
		const int tx = lodgenVtFloorTo( cx - west, dim ) + west, ty = lodgenVtFloorTo( cy - south, dim ) + south;
		if ( tilesDone.contains( LodgenVtFill::key( tx, ty ) ) )
			continue;
		tilesDone.insert( LodgenVtFill::key( tx, ty ) );
		LodgenVtStage st;
		if ( !bakeSmall( tx, ty, dim, C, Bd, st ) || st.colour.size() != size_t( S ) * S )
			continue;
		F.fitTiles++;
		for ( int ly = 0; ly < dim; ly++ )
			for ( int lx = 0; lx < dim; lx++ ) {
				const qint64 k = LodgenVtFill::key( tx + lx, ty + ly );
				if ( !role.contains( k ) )
					continue;
				std::array<float, 3> m = { 0, 0, 0 };
				const int j0 = Bd + ( dim - 1 - ly ) * perCell, i0 = Bd + lx * perCell;
				for ( int j = j0; j < j0 + perCell; j++ )
					for ( int i = i0; i < i0 + perCell; i++ ) {
						const quint32 px = st.colour[size_t( j ) * S + i];
						m[0] += float( ( px >> 16 ) & 0xFF );
						m[1] += float( ( px >> 8 ) & 0xFF );
						m[2] += float( px & 0xFF );
					}
				for ( int k3 = 0; k3 < 3; k3++ )
					m[k3] /= float( perCell * perCell );
				ours.insert( k, m );
			}
	}
	// VANILLA's side: the mean of the cell's 128 x 128 texels
	for ( auto it = role.constBegin(); it != role.constEnd(); ++it ) {
		const int cx = int( it.key() >> 32 ), cy = int( qint32( quint32( it.key() & 0xFFFFFFFF ) ) );
		const int gx0 = cx * 128, gy0 = -( cy + 1 ) * 128;
		std::vector<float> rgb;
		std::vector<quint8> have;
		F.window( bc, dataRoot, gx0, gy0, gx0 + 127, gy0 + 127, rgb, have );
		std::array<float, 3> m = { 0, 0, 0 };
		int n = 0;
		for ( size_t o = 0; o < have.size(); o++ )
			if ( have[o] ) {
				for ( int k3 = 0; k3 < 3; k3++ )
					m[k3] += rgb[o * 3 + k3] * 255.0f;
				n++;
			}
		if ( n < 128 * 128 )
			continue;
		for ( int k3 = 0; k3 < 3; k3++ )
			m[k3] /= float( n );
		van.insert( it.key(), m );
	}
	auto lum = []( const std::array<float, 3> & c ) {
		return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
	};
	// the tone fit on the overlap
	double sLv = 0, sLv2 = 0, sLb = 0, sLb2 = 0, cv2 = 0, cb2 = 0, cvm[3] = { 0, 0, 0 }, cbm[3] = { 0, 0, 0 };
	qint64 n = 0;
	for ( auto it = role.constBegin(); it != role.constEnd(); ++it ) {
		if ( it.value() != 1 || !ours.contains( it.key() ) || !van.contains( it.key() ) )
			continue;
		const auto b = ours.value( it.key() ), v = van.value( it.key() );
		const double lb = lum( b ), lv = lum( v );
		sLv += lv; sLv2 += lv * lv; sLb += lb; sLb2 += lb * lb;
		for ( int k = 0; k < 3; k++ ) {
			cv2 += ( v[k] - lv ) * ( v[k] - lv );
			cb2 += ( b[k] - lb ) * ( b[k] - lb );
			cvm[k] += v[k] - lv;
			cbm[k] += b[k] - lb;
		}
		n++;
	}
	F.overlapCells = n;
	for ( auto it = role.constBegin(); it != role.constEnd(); ++it )
		if ( it.value() == 2 )
			F.ringCells++;
	if ( n < 2 ) {
		F.on = false;       // nothing to fit on: the fill does nothing, and says so
		return;
	}
	const double mLv = sLv / n, mLb = sLb / n;
	const double sdV = std::sqrt( qMax( 0.0, sLv2 / n - mLv * mLv ) );
	const double sdB = std::sqrt( qMax( 0.0, sLb2 / n - mLb * mLb ) );
	F.rawGain = sdV > 0.0 ? float( sdB / sdV ) : 1.0f;
	F.gain = qMin( 1.0f, F.rawGain );
	F.offset = float( mLb - F.gain * mLv ) / 255.0f;
	F.sat = cv2 > 0.0 ? float( std::sqrt( cb2 / cv2 ) ) : 1.0f;
	for ( int k = 0; k < 3; k++ )
		F.cshift[k] = float( cbm[k] / n - F.sat * cvm[k] / n ) / 255.0f;
	// the bar: vanilla's own adjacent cell-mean steps over the overlap and the ring
	std::vector<float> steps, diffs;
	for ( auto it = van.constBegin(); it != van.constEnd(); ++it ) {
		const int cx = int( it.key() >> 32 ), cy = int( qint32( quint32( it.key() & 0xFFFFFFFF ) ) );
		for ( const auto & nb : { LodgenVtFill::key( cx + 1, cy ), LodgenVtFill::key( cx, cy + 1 ) } ) {
			auto jt = van.constFind( nb );
			if ( jt != van.constEnd() )
				steps.push_back( std::fabs( lum( it.value() ) - lum( jt.value() ) ) );
		}
	}
	F.bar = lodgenVtFillPercentile( steps, 99.0f );
	for ( auto it = role.constBegin(); it != role.constEnd(); ++it ) {
		if ( it.value() != 2 || !ours.contains( it.key() ) || !van.contains( it.key() ) )
			continue;
		const auto v = van.value( it.key() );
		float vv[3] = { v[0] / 255.0f, v[1] / 255.0f, v[2] / 255.0f }, tv[3];
		F.tone( vv, tv );
		const std::array<float, 3> t3 = { tv[0] * 255.0f, tv[1] * 255.0f, tv[2] * 255.0f };
		diffs.push_back( std::fabs( lum( ours.value( it.key() ) ) - lum( t3 ) ) );
	}
	F.p95 = lodgenVtFillPercentile( diffs, 95.0f );
	F.bandCells = ( F.bar > 0.0f ) ? int( std::ceil( F.p95 / F.bar ) ) : 1;
	F.bandCells = qBound( 1, F.bandCells, LODGEN_VT_FILL_MAX_BAND );
	F.band = float( F.bandCells ) * 4096.0f;
}

QString lodgenVtFillReport( const LodgenVtFill & F )
{
	return QString( "vanillaFill overlapCells=%1 ringCells=%2 fitTiles=%3 gain=%4 rawGain=%5 offset=%6 "
		"sat=%7 cshift=%8,%9,%10 bar=%11 p95=%12 bandCells=%13 tilesTouched=%14 texelsFilled=%15 "
		"texelsNoVanilla=%16 vanillaChunksMissing=%17" )
		.arg( F.overlapCells ).arg( F.ringCells ).arg( F.fitTiles )
		.arg( double( F.gain ), 0, 'f', 4 ).arg( double( F.rawGain ), 0, 'f', 4 )
		.arg( double( F.offset * 255.0f ), 0, 'f', 3 ).arg( double( F.sat ), 0, 'f', 4 )
		.arg( double( F.cshift[0] * 255.0f ), 0, 'f', 3 ).arg( double( F.cshift[1] * 255.0f ), 0, 'f', 3 )
		.arg( double( F.cshift[2] * 255.0f ), 0, 'f', 3 )
		.arg( double( F.bar ), 0, 'f', 3 ).arg( double( F.p95 ), 0, 'f', 3 ).arg( F.bandCells )
		.arg( F.tilesTouched ).arg( F.texelsFilled ).arg( F.texelsNoVanilla ).arg( F.vanillaMissing.size() );
}

