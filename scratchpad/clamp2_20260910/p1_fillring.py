#!/usr/bin/env python
"""CLAMP2 patch 1 -- the cell owns its own boundary rows.

Rewrites lodgenTerrainFillRing in src/lodgen.cpp so a RING cell never writes a
grid sample that belongs to the inner unit (the chunk, or the VT tile), and adds
the env-gated known-answer self-test that proves it with the OLD fill order as
the refuter.

Written as a file and run, never through a heredoc (nifskope-ww-lodgen).
"""
import io, sys

P = 'E:/Projects/NifskopeWildWastelandEdition/src/lodgen.cpp'
s = io.open(P, encoding='utf-8', newline='').read()
assert s.count('\r') == 0, 'lodgen.cpp is LF-only'

OLD = '''/*! ONE HOME for filling that ring's height grid.
 *
 *  `fetch( cx, cy )` is given RING-LOCAL cell coordinates (0..rdim-1) and
 *  returns the LAND record there or null; it is the callers' one difference --
 *  the chunk baker reads the plugin directly and keeps the chunk's own cells,
 *  the tile baker goes through its row cache. THE ITERATION ORDER IS PART OF
 *  THE CONTRACT: cells are visited south to north then west to east and a later
 *  cell overwrites the VHGT sample it shares with an earlier one, so both
 *  callers resolve a shared cell edge the same way and the two grids agree
 *  exactly where they overlap.
 */
template <typename Fetch>
static void lodgenTerrainFillRing( std::vector<float> & hgt, int hn, int rdim,
\tfloat empty, Fetch fetch )
{
\thgt.assign( size_t( hn ) * hn, empty );
\tfor ( int cy = 0; cy < rdim; cy++ ) {
\t\tfor ( int cx = 0; cx < rdim; cx++ ) {
\t\t\tconst EsmLand * land = fetch( cx, cy );
\t\t\tif ( !land )
\t\t\t\tcontinue;
\t\t\tfor ( int row = 0; row < 33; row++ )
\t\t\t\tfor ( int col = 0; col < 33; col++ )
\t\t\t\t\thgt[size_t( cy * 32 + row ) * hn + size_t( cx * 32 + col )] =
\t\t\t\t\t\tland->heights[row][col];
\t\t}
\t}
}
'''

NEW = '''/*! ONE HOME for filling that ring's height grid.
 *
 *  `fetch( cx, cy )` is given RING-LOCAL cell coordinates (0..rdim-1) and
 *  returns the LAND record there or null; it is the callers' one difference --
 *  the chunk baker reads the plugin directly and keeps the chunk's own cells,
 *  the tile baker goes through its row cache. Both callers lay the ring out the
 *  same way -- `LODGEN_TERRAIN_RING_CELLS` cells of margin on every side of an
 *  inner unit of `rdim - 2 * LODGEN_TERRAIN_RING_CELLS` cells -- so the inner
 *  unit's grid box is derived here rather than passed, and there is exactly one
 *  description of it.
 *
 *  THE CELL OWNS ITS OWN ROWS (bungo, 2026-09-10, verbatim: "The cell owns it
 *  then"). A RING cell fills ONLY the samples BEYOND the inner unit: it never
 *  writes the inner unit's own boundary row or column, so where a neighbour's
 *  copy of a shared VHGT row disagrees with the cell's own, the cell's copy
 *  stays and Bethesda's hairline disagreement is preserved at the seam instead
 *  of being smeared one row into the chunk.
 *
 *  It is a real disagreement and it was measured, not assumed: over the cells
 *  x = -24..-17 the shared row y=31|32 differs by 2, 1, 4, 6, 9, 8, 7 and 4
 *  VHGT units of 8 (16..72 world units) while y=23|24, y=27|28, y=32|33 and
 *  both east seams differ by 0 (lane BUILD4, 2026-09-10). The old south-to-north
 *  order let the y=32 cell overwrite that row inside the y=28..31 chunk, and a
 *  bilinear tap carried it 7 texels in -- past the 4-texel band the normal's
 *  own central difference can reach.
 *
 *  Inside the inner unit the order is unchanged and still part of the contract:
 *  cells are visited south to north then west to east and a later cell
 *  overwrites the VHGT sample it shares with an earlier one, so both callers
 *  resolve an INTERNAL cell edge the same way and the two grids agree exactly
 *  where they overlap. The same convention as the mesh path's chunk-only grid
 *  (`lodgenWriteLandChunk`), which is why that path needs no change.
 *
 *  A cell with no LAND writes nothing, as before: its samples keep `empty`, and
 *  a neighbour no longer reaches in to fill the shared row of a landless inner
 *  cell. That is the same rule -- the cell owns it -- applied to a cell whose
 *  own answer is the worldspace default.
 */
template <typename Fetch>
static void lodgenTerrainFillRing( std::vector<float> & hgt, int hn, int rdim,
\tfloat empty, Fetch fetch )
{
\t// the inner unit's closed grid box; < 0 when there is no ring at all
\tconst bool haveInner = ( rdim > 2 * LODGEN_TERRAIN_RING_CELLS );
\tconst int innerLo = 32 * LODGEN_TERRAIN_RING_CELLS;
\tconst int innerHi = hn - 1 - innerLo;
\thgt.assign( size_t( hn ) * hn, empty );
\tfor ( int cy = 0; cy < rdim; cy++ ) {
\t\tfor ( int cx = 0; cx < rdim; cx++ ) {
\t\t\tconst EsmLand * land = fetch( cx, cy );
\t\t\tif ( !land )
\t\t\t\tcontinue;
\t\t\tconst bool ringCell = haveInner
\t\t\t\t&& ( cx < LODGEN_TERRAIN_RING_CELLS
\t\t\t\t\t|| cx >= rdim - LODGEN_TERRAIN_RING_CELLS
\t\t\t\t\t|| cy < LODGEN_TERRAIN_RING_CELLS
\t\t\t\t\t|| cy >= rdim - LODGEN_TERRAIN_RING_CELLS );
\t\t\tfor ( int row = 0; row < 33; row++ ) {
\t\t\t\tconst int gr = cy * 32 + row;
\t\t\t\tconst bool rowInside = ( gr >= innerLo && gr <= innerHi );
\t\t\t\tfor ( int col = 0; col < 33; col++ ) {
\t\t\t\t\tconst int gc = cx * 32 + col;
\t\t\t\t\tif ( ringCell && rowInside && gc >= innerLo && gc <= innerHi )
\t\t\t\t\t\tcontinue;   // the inner unit's own sample: it owns it
\t\t\t\t\thgt[size_t( gr ) * hn + size_t( gc )] =
\t\t\t\t\t\tland->heights[row][col];
\t\t\t\t}
\t\t\t}
\t\t}
\t}
}

/*! The known-answer control under that rule, run once per process when
 *  `WW_TERRAIN_RING_TEST` is set in the environment.
 *
 *  A synthetic pair of cells whose shared VHGT row DISAGREES by a known amount
 *  -- 72 world units, the 9-unit worst case measured on the y=31|32 seam -- is
 *  filled through the shipped `lodgenTerrainFillRing`, and every sample of the
 *  inner unit's boundary row and column is asserted to be the inner cell's own
 *  value, exactly, while the samples one step beyond are asserted to be the
 *  neighbour's (so a filler that simply stopped filling the ring would fail).
 *  The bilinear tap the sheets actually read is asserted at the same place, so
 *  the claim is about a texel's operand and not only about a grid cell.
 *
 *  THE REFUTER: the same synthetic pair filled by the OLD south-to-north order,
 *  reproduced verbatim below as a control. It must give the NEIGHBOUR's value
 *  on the inner boundary row -- that is, it must FAIL the bar above. If it
 *  passes, the bar is not discriminating and the self-test says so and fails.
 */
static bool lodgenTerrainRingSelfTest()
{
\tconstexpr int RC = LODGEN_TERRAIN_RING_CELLS;
\tconstexpr int rdim = 1 + 2 * RC;            // one inner cell, one ring
\tconstexpr int hn = rdim * 32 + 1;
\tconst int innerLo = 32 * RC, innerHi = hn - 1 - innerLo;
\tconst float A = 1024.0f;                    // the inner cell's own row
\tconst float B = 1096.0f;                    // the neighbours', 72 units apart

\tEsmLand inner, north, east;
\tfor ( int r = 0; r < 33; r++ )
\t\tfor ( int c = 0; c < 33; c++ ) {
\t\t\tinner.heights[r][c] = A;
\t\t\tnorth.heights[r][c] = B;
\t\t\teast.heights[r][c] = B;
\t\t}
\tauto fetch = [&]( int cx, int cy ) -> const EsmLand * {
\t\tif ( cx == RC && cy == RC )
\t\t\treturn &inner;
\t\tif ( cx == RC && cy == RC + 1 )
\t\t\treturn &north;
\t\tif ( cx == RC + 1 && cy == RC )
\t\t\treturn &east;
\t\treturn nullptr;
\t};

\tstd::vector<float> hgt;
\tlodgenTerrainFillRing( hgt, hn, rdim, 0.0f, fetch );

\tint checks = 0, bad = 0;
\tauto expect = [&]( const char * what, float got, float want ) {
\t\tchecks++;
\t\tif ( got != want ) {
\t\t\tbad++;
\t\t\tfprintf( stderr, "ring:   FAIL %s = %.3f, expected %.3f\\n", what, got, want );
\t\t} else {
\t\t\tfprintf( stderr, "ring:   ok   %s = %.3f\\n", what, got );
\t\t}
\t};
\tauto worst = [&]( int r0, int r1, int c0, int c1, float want ) {
\t\tfloat far = want;
\t\tfor ( int r = r0; r <= r1; r++ )
\t\t\tfor ( int c = c0; c <= c1; c++ ) {
\t\t\t\tconst float v = hgt[size_t( r ) * hn + size_t( c )];
\t\t\t\tif ( qAbs( v - want ) > qAbs( far - want ) )
\t\t\t\t\tfar = v;
\t\t\t}
\t\treturn far;
\t};

\tfprintf( stderr, "ring: self-test the cell owns its own boundary rows"
\t\t" (WW_TERRAIN_RING_TEST)\\n" );
\tfprintf( stderr, "ring:   synthetic pair, inner %.3f, north and east neighbour"
\t\t" %.3f, shared rows disagree by %.3f world units\\n", A, B, B - A );
\texpect( "the inner unit's NORTH boundary row, every sample",
\t\tworst( innerHi, innerHi, innerLo, innerHi, A ), A );
\texpect( "the inner unit's EAST boundary column, every sample",
\t\tworst( innerLo, innerHi, innerHi, innerHi, A ), A );
\texpect( "the inner unit's SOUTH boundary row, every sample",
\t\tworst( innerLo, innerLo, innerLo, innerHi, A ), A );
\texpect( "the inner unit's WEST boundary column, every sample",
\t\tworst( innerLo, innerHi, innerLo, innerLo, A ), A );
\texpect( "one grid step BEYOND the north border, the neighbour's",
\t\tworst( innerHi + 1, hn - 1, innerLo, innerHi, B ), B );
\texpect( "one grid step BEYOND the east border, the neighbour's",
\t\tworst( innerLo, innerHi, innerHi + 1, hn - 1, B ), B );
\t// the texel's own operand, not just the grid: the shared tap at the border
\texpect( "the bilinear tap ON the north border",
\t\tlodgenTerrainGridSample( hgt, hn, float( innerLo + 16 ) * 128.0f,
\t\t\tfloat( innerHi ) * 128.0f ), A );
\texpect( "the bilinear tap half a step beyond it",
\t\tlodgenTerrainGridSample( hgt, hn, float( innerLo + 16 ) * 128.0f,
\t\t\tfloat( innerHi ) * 128.0f + 64.0f ), ( A + B ) * 0.5f );

\t/* THE CONTROL: the fill order this replaced, reproduced verbatim. */
\tstd::vector<float> old( size_t( hn ) * hn, 0.0f );
\tfor ( int cy = 0; cy < rdim; cy++ )
\t\tfor ( int cx = 0; cx < rdim; cx++ ) {
\t\t\tconst EsmLand * land = fetch( cx, cy );
\t\t\tif ( !land )
\t\t\t\tcontinue;
\t\t\tfor ( int row = 0; row < 33; row++ )
\t\t\t\tfor ( int col = 0; col < 33; col++ )
\t\t\t\t\told[size_t( cy * 32 + row ) * hn + size_t( cx * 32 + col )] =
\t\t\t\t\t\tland->heights[row][col];
\t\t}
\tconst float oldNorth = old[size_t( innerHi ) * hn + size_t( innerLo + 16 )];
\tconst float oldEast = old[size_t( innerLo + 16 ) * hn + size_t( innerHi )];
\tchecks++;
\tif ( oldNorth == A || oldEast == A ) {
\t\tbad++;
\t\tfprintf( stderr, "ring:   FAIL CONTROL the old south-to-north order gives"
\t\t\t" %.3f / %.3f on the inner boundary, which the bar above ACCEPTS:"
\t\t\t" the bar does not discriminate\\n", oldNorth, oldEast );
\t} else {
\t\tfprintf( stderr, "ring:   ok   CONTROL the old south-to-north order gives"
\t\t\t" %.3f north and %.3f east on the inner boundary, which the bar above"
\t\t\t" REFUSES\\n", oldNorth, oldEast );
\t}
\tfprintf( stderr, "ring: self-test %d checks, %d failures, %s\\n", checks, bad,
\t\tbad ? "RESULT FAIL" : "RESULT PASS" );
\treturn bad == 0;
}

/*! Runs it once per process, and only when asked. */
static void lodgenTerrainRingSelfTestOnce()
{
\tstatic bool done = false;
\tif ( done || qgetenv( "WW_TERRAIN_RING_TEST" ).isEmpty() )
\t\treturn;
\tdone = true;
\tlodgenTerrainRingSelfTest();
}
'''

assert s.count(OLD) == 1, 'lodgenTerrainFillRing anchor count %d' % s.count(OLD)
s = s.replace(OLD, NEW)

# the call site: once, at the top of the chunk baker, before anything is read
CALL_OLD = '''bool lodgenBakeTerrainTextures( const EsmWorld & world, int chunkX, int chunkY,
\tint dim, const QString & dataRoot, const QString & outDir,
\tconst LodgenCoverOptions & coverOpts, LodgenBakeCaches * caches, QString * error )
{
\tauto fail = [error]( const QString & message ) {'''
CALL_NEW = '''bool lodgenBakeTerrainTextures( const EsmWorld & world, int chunkX, int chunkY,
\tint dim, const QString & dataRoot, const QString & outDir,
\tconst LodgenCoverOptions & coverOpts, LodgenBakeCaches * caches, QString * error )
{
\tlodgenTerrainRingSelfTestOnce();
\tauto fail = [error]( const QString & message ) {'''
assert s.count(CALL_OLD) == 1, 'bake anchor count %d' % s.count(CALL_OLD)
s = s.replace(CALL_OLD, CALL_NEW)

io.open(P, 'w', encoding='utf-8', newline='').write(s)
b = open(P, 'rb').read()
print('written, CR %d LF %d bytes %d' % (b.count(b'\r'), b.count(b'\n'), len(b)))
