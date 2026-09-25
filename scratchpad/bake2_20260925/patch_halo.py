"""BAKE2 halo fix: a no-LAND cell has no colour of ours (only the generator's placeholder grey), so the
vanilla fill takes it whole (w = 1) and it stays out of the band's p95. Anchors asserted once; CR count kept."""
import os
R = 'E:/Projects/NifskopeWWE-bake2/'
DRY = os.environ.get('DRY') == '1'

def patch(path, pairs):
    b = open(R + path, 'rb').read(); cr = b.count(b'\r'); s = b.decode('utf-8')
    for old, new in pairs:
        n = s.count(old); assert n == 1, (path, n, old[:80]); s = s.replace(old, new)
    out = s.encode('utf-8'); assert out.count(b'\r') == cr, path
    if not DRY: open(R + path, 'wb').write(out)
    print('%s: %d edits%s' % (path, len(pairs), ' (dry)' if DRY else ''))

patch('src/lodgen.cpp', [
("""                   layers win and every painted texel is byte-identical.
 *    d              world distance from the texel to the nearest painted cell
 *    w              smoothstep( 0, band, d )
""",
"""                   layers win and every painted texel is byte-identical.
 *    d              world distance from the texel to the nearest painted cell
 *    w              smoothstep( 0, band, d ); 1 on a cell with NO LAND record
 *                   (lane BAKE2, 2026-09-25): such a cell has no colour of ours,
 *                   only the generator's flat placeholder grey, and blending FROM
 *                   it painted a pale halo 1..3 cells wide round pre-war's
 *                   playable block (+38 luminance over vanilla one cell out).
 *                   Those cells stay out of the band's p95 for the same reason.
"""),
("""	std::vector<quint8> painted;
	float gain = 1.0f,""",
"""	std::vector<quint8> painted;
	std::vector<quint8> noLand;                 //!< 1 = no LAND record: no colour of ours, filled whole
	float gain = 1.0f,"""),
("""	qint64 tilesTouched = 0, texelsFilled = 0, texelsNoVanilla = 0;
	QSet<qint64> vanillaMissing;""",
"""	qint64 tilesTouched = 0, texelsFilled = 0, texelsNoVanilla = 0;
	qint64 noLandCells = 0, noLandRingCells = 0, texelsNoLand = 0;
	QSet<qint64> vanillaMissing;"""),
("""		return painted[size_t( cy - y0 ) * w + ( cx - x0 )] != 0;
	}
""",
"""		return painted[size_t( cy - y0 ) * w + ( cx - x0 )] != 0;
	}

	bool isNoLand( int cx, int cy ) const
	{
		if ( cx < x0 || cy < y0 || cx >= x0 + w || cy >= y0 + h )
			return false;
		return noLand[size_t( cy - y0 ) * w + ( cx - x0 )] != 0;
	}
"""),
("""	F.painted.assign( size_t( F.w ) * F.h, 0 );
	for ( int cy = F.y0; cy < F.y0 + F.h; cy++ ) {
		for ( int cx = F.x0; cx < F.x0 + F.w; cx++ ) {
			EsmLand land;
			if ( !world.land( cx, cy, land ) )
				continue;
""",
"""	F.painted.assign( size_t( F.w ) * F.h, 0 );
	F.noLand.assign( size_t( F.w ) * F.h, 0 );
	for ( int cy = F.y0; cy < F.y0 + F.h; cy++ ) {
		for ( int cx = F.x0; cx < F.x0 + F.w; cx++ ) {
			EsmLand land;
			if ( !world.land( cx, cy, land ) ) {
				F.noLand[size_t( cy - F.y0 ) * F.w + ( cx - F.x0 )] = 1;
				if ( cx >= west && cx <= east && cy >= south && cy <= north )
					F.noLandCells++;
				continue;
			}
"""),
("""	std::vector<quint8> cellPainted( size_t( ncx ) * ncy );
	for ( int cy = br0; cy <= br1; cy++ )
		for ( int cx = bc0; cx <= bc1; cx++ ) {
			const size_t o = size_t( cy - br0 ) * ncx + ( cx - bc0 );
			cellPainted[o] = F.isPainted( cx, cy ) ? 1 : 0;
			if ( !cellPainted[o] )
				lodgenVtFillNeighbours( F, cx, cy, r, nb[o] );
		}
""",
"""	std::vector<quint8> cellPainted( size_t( ncx ) * ncy ), cellNoLand( size_t( ncx ) * ncy );
	for ( int cy = br0; cy <= br1; cy++ )
		for ( int cx = bc0; cx <= bc1; cx++ ) {
			const size_t o = size_t( cy - br0 ) * ncx + ( cx - bc0 );
			cellPainted[o] = F.isPainted( cx, cy ) ? 1 : 0;
			cellNoLand[o] = F.isNoLand( cx, cy ) ? 1 : 0;
			if ( !cellPainted[o] && !cellNoLand[o] )
				lodgenVtFillNeighbours( F, cx, cy, r, nb[o] );
		}
"""),
("""			const float d = lodgenVtFillDistance( nb[co], wx, wy );
			const float t = qBound( 0.0f, d / F.band, 1.0f );
			const float wgt = t * t * ( 3.0f - 2.0f * t );
			if ( wgt <= 0.0f )
				continue;
""",
"""			float wgt = 1.0f;       // no LAND: only the placeholder grey here, filled whole
			if ( !cellNoLand[co] ) {
				const float d = lodgenVtFillDistance( nb[co], wx, wy );
				const float t = qBound( 0.0f, d / F.band, 1.0f );
				wgt = t * t * ( 3.0f - 2.0f * t );
			}
			if ( wgt <= 0.0f )
				continue;
"""),
("""			F.texelsFilled++;
			touched = true;""",
"""			F.texelsFilled++;
			if ( cellNoLand[co] )
				F.texelsNoLand++;
			touched = true;"""),
("""		if ( it.value() != 2 || !ours.contains( it.key() ) || !van.contains( it.key() ) )
			continue;
		const auto v = van.value( it.key() );""",
"""		if ( it.value() != 2 || !ours.contains( it.key() ) || !van.contains( it.key() ) )
			continue;
		if ( F.isNoLand( LodgenVtFill::keyX( it.key() ), LodgenVtFill::keyY( it.key() ) ) ) {
			F.noLandRingCells++;    // filled whole, so it does not size the band
			continue;
		}
		const auto v = van.value( it.key() );"""),
("""		"texelsNoVanilla=%16 vanillaChunksMissing=%17 vanillaSheetsRead=%18 root=%19" )""",
"""		"texelsNoVanilla=%16 vanillaChunksMissing=%17 vanillaSheetsRead=%18 noLandCells=%19 "
		"noLandRingCells=%20 texelsNoLand=%21 root=%22" )"""),
("""		.arg( F.vanillaLoaded ).arg( lodgenVanillaLodRoot() );""",
"""		.arg( F.vanillaLoaded ).arg( F.noLandCells ).arg( F.noLandRingCells ).arg( F.texelsNoLand )
		.arg( lodgenVanillaLodRoot() );"""),
])

patch('docs/LODGEN_TERRAIN_VT.md', [
("""    w             smoothstep(0, band, d); w = 0 on a painted cell, so its texels are untouched
""",
"""    w             smoothstep(0, band, d); w = 0 on a painted cell, so its texels are untouched;
                  w = 1 on a cell with NO LAND record (lane BAKE2, 2026-09-25, below)
"""),
("""  **The band** = ceil(p95 over the ring of |lum ours - lum T(V)| / bar) cells,
  at least 1, at most 8.
""",
"""  **The band** = ceil(p95 over the ring of |lum ours - lum T(V)| / bar) cells,
  at least 1, at most 8. No-LAND ring cells are left out of the p95.
* **A cell with no LAND record is filled whole** (lane BAKE2, 2026-09-25). It has no
  colour of ours: the generator writes its flat placeholder grey (luminance 129.6,
  chroma 2) there. Blending FROM that grey over the band painted a pale halo round
  pre-war Sanctuary's playable block, which bungo saw on the top-down picture: no-LAND
  cells one, two and three cells off the LAND edge read +38, +30 and +19 luminance
  over vanilla, against +11 (the tone match) six cells out, while vanilla's own texels
  there are flat. Those cells were also in the band's p95, where the grey set the band
  (4 cells on pre-war). Both are removed: w = 1 on a no-LAND cell, and it does not size
  the band. The Commonwealth has LAND on every cell of -96..95, so its fill is unchanged
  by construction. Gate: `scratchpad/bake2_20260925/halo_gate.py` (RED on the exe
  before the change: 100 of 180 near cells over vanilla + offset + 8).
"""),
("""  texelsFilled= texelsNoVanilla= vanillaChunksMissing= vanillaSheetsRead= root=`.""",
"""  texelsFilled= texelsNoVanilla= vanillaChunksMissing= vanillaSheetsRead= noLandCells=
  noLandRingCells= texelsNoLand= root=`."""),
])
