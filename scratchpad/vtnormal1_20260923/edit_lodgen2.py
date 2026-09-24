"""VTNORMAL1 edit 2: the sheet cache, the per-tile override, the driver, the
header's mipSkip, the index and the census."""
import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/src/lodgen.cpp'
H = 'E:/Projects/NifskopeWildWastelandEdition/src/lodgen.h'
s = open(P, 'rb').read().decode('utf-8')


def sub(old, new, count=1):
	global s
	n = s.count(old)
	if n != count:
		sys.exit('anchor found %d times, want %d:\n%s' % (n, count, old[:300]))
	s = s.replace(old, new)


# ---- 1. the sheet cache and the per-tile override ---------------------------
sub("""bool lodgenBakeTerrainVt( const EsmWorld & world, const QString & dataRoot,
	const QString & outDir, const LodgenVtOptions & opts, LodgenBakeCaches * caches,
	QString * report, QString * error )
{""", """/*! THE PYRAMID'S NORMAL FROM bungo's UPSCALED SHEETS (lane VTNORMAL1,
 *  2026-09-23, his ruling: "use my upscaled normal and slope sheets,
 *  downsampled").
 *
 *  The `--msn-cache` directory (panel: the cleaned-normal folder) holds one
 *  `<ws>.4.<x>.<y>_msn.DDS` per dim-4 chunk -- his are 2048 px, 8 world units a
 *  texel, R east G up B north, row 0 NORTH (measured: flipped, every r against
 *  the heights normal and vanilla's own sheet drops from 0.66-0.97 to 0.13).
 *  Each sheet is read ONCE, reduced to the pyramid's finest density by a box
 *  filter in VECTOR space (decode, sum, renormalise, encode), and kept while a
 *  tile row can still reach it. At 8 units a texel the reduction is a copy.
 *  A texel with no sheet under it keeps the heights normal. Coarser levels
 *  come from the pyramid's own filter, which already averages the msn as a
 *  vector and renormalises it, so every level is his sheet box-filtered to
 *  that level's texel size. */
struct LodgenVtMsnSheets
{
	QString ws;
	int upt = 32;       //!< world units a texel at the finest level
	int res = 512;      //!< reduced texels on a dim-4 chunk's side, 16384 / upt
	struct Chunk
	{
		bool present = false;
		std::vector<quint32> px;    //!< res x res, row 0 north, encoded msn
	};
	QMap<QPair<int, int>, Chunk> chunks;
	qint64 sheetsRead = 0, sheetsMissing = 0;

	const Chunk & get( int cx, int cy )
	{
		const QPair<int, int> key( cx, cy );
		auto it = chunks.find( key );
		if ( it != chunks.end() )
			return *it;
		Chunk c;
		std::vector<float> v;
		int w = 0, h = 0;
		const QString name = QString( "%1.4.%2.%3" ).arg( ws ).arg( cx ).arg( cy );
		if ( lodgenMsnCacheRead( name, nullptr, &v, w, h ) && w == h && w > 0 ) {
			c.present = true;
			c.px.assign( size_t( res ) * size_t( res ), LODGEN_MSN_FLAT );
			for ( int b = 0; b < res; b++ ) {
				const int y0 = int( qint64( b ) * h / res );
				const int y1 = qMax( y0 + 1, int( qint64( b + 1 ) * h / res ) );
				for ( int a = 0; a < res; a++ ) {
					const int x0 = int( qint64( a ) * w / res );
					const int x1 = qMax( x0 + 1, int( qint64( a + 1 ) * w / res ) );
					double sx = 0.0, sy = 0.0, sz = 0.0;
					for ( int y = y0; y < y1; y++ ) {
						const float * p = v.data() + ( size_t( y ) * size_t( w ) + size_t( x0 ) ) * 3;
						for ( int x = x0; x < x1; x++, p += 3 ) {
							sx += p[0];
							sy += p[1];
							sz += p[2];
						}
					}
					const double len = std::sqrt( sx * sx + sy * sy + sz * sz );
					if ( len > 1e-9 )
						c.px[size_t( b ) * size_t( res ) + size_t( a )] = lodgenTerrainMsnPixel(
							Vector3( float( sx / len ), float( sy / len ), float( sz / len ) ) );
				}
			}
			sheetsRead++;
		} else {
			sheetsMissing++;
		}
		return *chunks.insert( key, std::move( c ) );
	}

	//! A chunk whose southmost cell lies north of `cellY` is behind the bake.
	void dropNorthOf( int cellY )
	{
		for ( auto it = chunks.begin(); it != chunks.end(); ) {
			if ( it.key().second > cellY )
				it = chunks.erase( it );
			else
				++it;
		}
	}
};

/*! Overwrite one finest tile's msn from the sheets, border included (a border
 *  texel reads the neighbouring chunk's sheet, so seams match by construction).
 *  Returns the number of CONTENT texels taken from a sheet, of content^2. */
static qint64 lodgenVtMsnFromSheets( LodgenVtMsnSheets & sh, int cellX0, int cellY0, int dim,
	int content, int border, bool keepHeights, LodgenVtStage & st )
{
	const int stored = content + 2 * border;
	const int perCell = 4096 / sh.upt;                  // texels a cell
	const int res = sh.res;
	const int tileW = cellX0 * perCell;                 // global texel column of the west edge
	const int tileN = ( cellY0 + dim ) * perCell;       // global texel row line of the north edge
	auto floorDiv = []( int a, int b ) { return a >= 0 ? a / b : -( ( -a + b - 1 ) / b ); };
	if ( keepHeights )
		st.msnHeights = st.msn;
	qint64 fromSheet = 0;
	for ( int j = 0; j < stored; j++ ) {
		const int gy = tileN - ( j - border ) - 1;      // texel spans [gy, gy+1) * upt north of 0
		const int cr = floorDiv( gy, res );
		const int b = ( cr + 1 ) * res - 1 - gy;        // row inside the chunk, 0 = north
		const LodgenVtMsnSheets::Chunk * c = nullptr;
		int cc = INT_MIN;
		for ( int i = 0; i < stored; i++ ) {
			const int gx = tileW + i - border;
			const int ccol = floorDiv( gx, res );
			if ( ccol != cc ) {
				cc = ccol;
				c = &sh.get( ccol * 4, cr * 4 );
			}
			if ( !c->present )
				continue;
			st.msn[size_t( j ) * size_t( stored ) + size_t( i )] =
				c->px[size_t( b ) * size_t( res ) + size_t( gx - ccol * res )];
			if ( j >= border && j < border + content && i >= border && i < border + content )
				fromSheet++;
		}
	}
	return fromSheet;
}

bool lodgenBakeTerrainVt( const EsmWorld & world, const QString & dataRoot,
	const QString & outDir, const LodgenVtOptions & opts, LodgenBakeCaches * caches,
	QString * report, QString * error )
{""")

# ---- 2. half-aux needs a second mip -----------------------------------------
sub("""			.arg( border ).arg( mips ).arg( 4 << ( mips - 1 ) ) );

	LodgenBakeCaches * ownCaches""", """			.arg( border ).arg( mips ).arg( 4 << ( mips - 1 ) ) );
	if ( opts.halfAux && mips < 2 )
		return fail( QStringLiteral( "--vt-half-aux drops each aux sheet's top mip and keeps the "
			"rest, so it needs --vt-mips 2 or more" ) );

	LodgenBakeCaches * ownCaches""")

# ---- 3. the header declares the half sheets ---------------------------------
sub("""		if ( wantEmissive )
			h.sheets[nextSheet++] = { LODV_DXGI_BC1_UNORM, LODV_DXGI_BC1_UNORM, LODV_ROLE_EMISSIVE, 0 };
		const QString path = QString( "%1/%2.VT.%3.lodt" ).arg( dir ).arg( ws ).arg( levels[l].dim );""",
"""		if ( wantEmissive )
			h.sheets[nextSheet++] = { LODV_DXGI_BC1_UNORM, LODV_DXGI_BC1_UNORM, LODV_ROLE_EMISSIVE, 0 };
		/* HALF-RESOLUTION AUX SHEETS: every sheet but the colour one says, in
		 * descriptor byte 6, that it stores no mip 0 (lane VTNORMAL1). Zero --
		 * byte-identical to every earlier file -- when the switch is off. */
		if ( opts.halfAux )
			for ( int i = 1; i < nextSheet; i++ )
				h.sheets[i].mipSkip = 1;
		const QString path = QString( "%1/%2.VT.%3.lodt" ).arg( dir ).arg( ws ).arg( levels[l].dim );""")

# ---- 4. the sheet cache is set up beside the land cache ---------------------
sub("""	LodgenVtLandCache landCache;

	/* ROADS. Gathered ONCE""", """	LodgenVtLandCache landCache;

	/* bungo's UPSCALED SHEETS AS THE PYRAMID'S NORMAL (lane VTNORMAL1): on
	 * whenever the cleaned-normal folder is set, exactly as the chunk sheets
	 * already were. The heights normal is kept beside it only when the .btr
	 * chunk sheets are assembled here, so those bytes do not move. */
	std::unique_ptr<LodgenVtMsnSheets> msnSheets;
	const int finestUpt = levels[0].dim * 4096 / content;
	if ( !lodgenMsnCacheDir().isEmpty() ) {
		msnSheets.reset( new LodgenVtMsnSheets );
		msnSheets->ws = ws;
		msnSheets->upt = finestUpt;
		msnSheets->res = 16384 / finestUpt;
	}
	const bool keepHeightsNormal = msnSheets && !opts.btrTexDir.isEmpty();
	qint64 normalTilesSheet = 0, normalTilesHeights = 0, normalTilesMixed = 0;

	/* ROADS. Gathered ONCE""")

# ---- 5. the encoder is told about half-aux ---------------------------------
sub("""		const QByteArray raw = lodgenVtEncodeTile( st, stored, mips, opts.height,
			wantEmissive, opts.coverInColor );""", """		const QByteArray raw = lodgenVtEncodeTile( st, stored, mips, opts.height,
			wantEmissive, opts.coverInColor, opts.halfAux );""")

# ---- 6. the chunk-sheet assembly reads the heights normal --------------------
sub("""							col[dst] = st.colour[src];
							nrm[dst] = st.msn[src];""", """							col[dst] = st.colour[src];
							// the heights normal when the sheets replaced the
							// pyramid's (lane VTNORMAL1): this path does not move
							nrm[dst] = st.msnHeights.empty() ? st.msn[src] : st.msnHeights[src];""")

# ---- 7. the driver ------------------------------------------------------------
sub("""	for ( int ty = 0; ty < levels[0].tilesY; ty++ ) {
		std::vector<LodgenVtStage> row( size_t( levels[0].tilesX ) );
		for ( int tx = 0; tx < levels[0].tilesX; tx++ ) {
			const int cellX0 = levels[0].west + tx * levels[0].dim;
			const int cellY0 = levels[0].north - ( ty + 1 ) * levels[0].dim + 1;
			if ( !lodgenBakeVtTile( world, dataRoot, bc, opts.cover, landCache, maskCache,
				wantEmissive, cellX0, cellY0, levels[0].dim, content, border, row[size_t( tx )],
				roadSet.get(), &roadCensus, objField.get(), &objCensus ) )
				return fail( QString( "could not bake tile (%1,%2)" ).arg( tx ).arg( ty ) );
		}""", """	for ( int ty = 0; ty < levels[0].tilesY; ty++ ) {
		std::vector<LodgenVtStage> row( size_t( levels[0].tilesX ) );
		// a tile row's border reaches one cell past its north edge, no further
		if ( msnSheets )
			msnSheets->dropNorthOf( levels[0].north - ty * levels[0].dim + 1 );
		for ( int tx = 0; tx < levels[0].tilesX; tx++ ) {
			const int cellX0 = levels[0].west + tx * levels[0].dim;
			const int cellY0 = levels[0].north - ( ty + 1 ) * levels[0].dim + 1;
			if ( !lodgenBakeVtTile( world, dataRoot, bc, opts.cover, landCache, maskCache,
				wantEmissive, cellX0, cellY0, levels[0].dim, content, border, row[size_t( tx )],
				roadSet.get(), &roadCensus, objField.get(), &objCensus ) )
				return fail( QString( "could not bake tile (%1,%2)" ).arg( tx ).arg( ty ) );
			if ( msnSheets ) {
				const qint64 got = lodgenVtMsnFromSheets( *msnSheets, cellX0, cellY0,
					levels[0].dim, content, border, keepHeightsNormal, row[size_t( tx )] );
				if ( got == qint64( content ) * content )
					normalTilesSheet++;
				else if ( got == 0 )
					normalTilesHeights++;
				else
					normalTilesMixed++;
			} else {
				normalTilesHeights++;
			}
		}""")

# ---- 8. the index ---------------------------------------------------------------
sub("""		if ( wantEmissive )
			sheets.append( sheet( "emissive", 71, 71, "linear",
				"RGB emissive colour, no alpha" ) );
		t.insert( QStringLiteral( "sheets" ), sheets );""", """		if ( wantEmissive )
			sheets.append( sheet( "emissive", 71, 71, "linear",
				"RGB emissive colour, no alpha" ) );
		/* HALF-RESOLUTION AUX SHEETS (lane VTNORMAL1): said per sheet and once
		 * at the top, and only when on, so an index with the switch off is
		 * byte-identical to every earlier one. The container says the same in
		 * descriptor byte 6. */
		if ( opts.halfAux ) {
			for ( int i = 0; i < sheets.size(); i++ ) {
				QJsonObject o = sheets[i].toObject();
				const int skipTop = i == 0 ? 0 : 1;
				o.insert( QStringLiteral( "mipSkip" ), skipTop );
				o.insert( QStringLiteral( "texels" ), stored >> skipTop );
				sheets[i] = o;
			}
			t.insert( QStringLiteral( "halfAux" ), true );
		}
		t.insert( QStringLiteral( "sheets" ), sheets );
		/* WHERE THE NORMAL CAME FROM (lane VTNORMAL1), written when the
		 * cleaned-normal folder was set; absent means every finest tile's
		 * normal is the heights normal, as it was before. */
		if ( msnSheets ) {
			QJsonObject ns;
			ns.insert( QStringLiteral( "rule" ), normalTilesHeights == 0 && normalTilesMixed == 0
				? QStringLiteral( "msnCache" ) : ( normalTilesSheet == 0 && normalTilesMixed == 0
					? QStringLiteral( "heights" ) : QStringLiteral( "mixed" ) ) );
			ns.insert( QStringLiteral( "filter" ),
				QStringLiteral( "vector box to each level's texel size, renormalised" ) );
			ns.insert( QStringLiteral( "tilesMsnCache" ), double( normalTilesSheet ) );
			ns.insert( QStringLiteral( "tilesHeights" ), double( normalTilesHeights ) );
			ns.insert( QStringLiteral( "tilesMixed" ), double( normalTilesMixed ) );
			ns.insert( QStringLiteral( "sheetsRead" ), double( msnSheets->sheetsRead ) );
			ns.insert( QStringLiteral( "sheetsMissing" ), double( msnSheets->sheetsMissing ) );
			t.insert( QStringLiteral( "normalSource" ), ns );
		}""")

# ---- 9. the census line --------------------------------------------------------
sub("""			r << QString( "msnCacheRenorm %1" ).arg( vr.msnCacheRenorm );
		}""", """			r << QString( "msnCacheRenorm %1" ).arg( vr.msnCacheRenorm );
		}
		/* THE PYRAMID'S NORMAL SOURCE (lane VTNORMAL1), written whether or not
		 * the folder is set: `normalMsnCache 0 normalHeights N` is a bake whose
		 * every finest tile took the heights normal. */
		r << QString( "normalMsnCache %1" ).arg( normalTilesSheet );
		r << QString( "normalHeights %1" ).arg( normalTilesHeights );
		r << QString( "normalMixed %1" ).arg( normalTilesMixed );
		r << QString( "msnSheetsRead %1" ).arg( msnSheets ? msnSheets->sheetsRead : 0 );
		r << QString( "msnSheetsMissing %1" ).arg( msnSheets ? msnSheets->sheetsMissing : 0 );
		r << QString( "unitsPerTexel %1" ).arg( finestUpt );
		r << QString( "halfAux %1" ).arg( opts.halfAux ? 1 : 0 );""")

open(P, 'wb').write(s.encode('utf-8'))

h = open(H, 'rb').read().decode('utf-8')
old = """	bool coverInColor = false;
	/* THE TERRAIN HORIZON SHEET (lane HORIZON1, 2026-09-18), `.lodt` role 7,"""
assert h.count(old) == 1
h = h.replace(old, """	bool coverInColor = false;
	/*! HALF-RESOLUTION AUX SHEETS (lane VTNORMAL1, `--vt-half-aux`, panel row
	 *  "Half-resolution normal, mask, height and emissive tiles"). The colour
	 *  sheet stays at the chosen texel density; msn, mask, height and emissive
	 *  store half the texels a side (their mip 0 is dropped, descriptor byte 6
	 *  says so). OFF by default and byte-identical when off. */
	bool halfAux = false;
	/* THE TERRAIN HORIZON SHEET (lane HORIZON1, 2026-09-18), `.lodt` role 7,""")
open(H, 'wb').write(h.encode('utf-8'))
print('lodgen edit 2 ok')
