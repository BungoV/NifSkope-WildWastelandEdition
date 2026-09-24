# LANE CLAMP: give the direct terrain chunk bake the tile baker's one-cell ring,
# through ONE shared filler and ONE shared sampler.
#
# Run from the repo root:  python scratchpad/clamp_20260910/p01_ring.py
import sys, io

PATH = 'src/lodgen.cpp'
raw = open(PATH, 'rb').read()
cr_before = raw.count(b'\r')
s = raw.decode('utf-8')
n_before = len(s.split('\n'))
TAB = chr(9)


def one(marker, start=0):
    if s.count(marker) != 1 and start == 0:
        print('MARKER NOT UNIQUE (%d): %r' % (s.count(marker), marker[:70]))
        sys.exit(1)
    i = s.find(marker, start)
    if i < 0:
        print('MARKER MISSING: %r' % marker[:70])
        sys.exit(1)
    return i


def cut(a_marker, b_marker, new, start=0, unique_b=True):
    """Replace the slice [a_marker, b_marker) with `new`."""
    global s
    a = one(a_marker, start)
    b = s.find(b_marker, a + len(a_marker))
    if b < 0:
        print('END MARKER MISSING after %r: %r' % (a_marker[:40], b_marker[:60]))
        sys.exit(1)
    if unique_b and s.count(b_marker) != 1:
        # allowed when we searched forward from a; just report
        pass
    s = s[:a] + new + s[b:]
    return a


T = TAB

# ---------------------------------------------------------------- A: helpers
anchor_a = 'constexpr quint32 LODGEN_MSN_FLAT = 0xFF80FF80U;\n'
helpers = anchor_a + '\n' + '\n'.join([
    '/*! ONE HOME for the plain-bilinear tap into a terrain sample grid.',
    ' *',
    ' *  `f` is an (n x n) grid of samples 128 world units apart, `lx`/`ly` are',
    ' *  world units from the GRID\'S OWN south-west corner, and the tap clamps at',
    ' *  the grid edge. Five byte-for-byte copies of these seven lines lived in the',
    ' *  two terrain bakers (each one\'s AO `heightAt` and channel samplers); they',
    ' *  are one function now, which is the only reason the two paths can be ASKED',
    ' *  for byte identity instead of told they agree.',
    ' *',
    ' *  Deliberately NOT the reconstruction `lodgenTerrainHeightAt` above uses:',
    ' *  that one eases the blend parameter because it feeds a NORMAL map, where a',
    ' *  kink at every height sample shows as a square lattice (2026-09-09). This',
    ' *  one feeds the AO march and the byte channels, where the plain blend is what',
    ' *  every shipped sheet was measured with.',
    ' */',
    'template <typename T>',
    'static float lodgenTerrainGridSample( const std::vector<T> & f, int n,',
    T + 'float lx, float ly )',
    '{',
    T + 'const float fx = qBound( 0.0f, lx / 128.0f, float( n - 1 ) );',
    T + 'const float fy = qBound( 0.0f, ly / 128.0f, float( n - 1 ) );',
    T + 'const int x0 = int( fx ), y0 = int( fy );',
    T + 'const int x1 = qMin( x0 + 1, n - 1 ), y1 = qMin( y0 + 1, n - 1 );',
    T + 'const float tx = fx - float( x0 ), ty = fy - float( y0 );',
    T + 'const float a = float( f[size_t( y0 ) * n + x0] );',
    T + 'const float b = float( f[size_t( y0 ) * n + x1] );',
    T + 'const float c = float( f[size_t( y1 ) * n + x0] );',
    T + 'const float d = float( f[size_t( y1 ) * n + x1] );',
    T + 'const float top = a + ( b - a ) * tx, bot = c + ( d - c ) * tx;',
    T + 'return top + ( bot - top ) * ty;',
    '}',
    '',
    '/*! The ring the terrain sheets are baked on: the chunk (or the tile) plus ONE',
    ' *  CELL on every side.',
    ' *',
    ' *  4,096 world units covers both neighbourhood operators the sheets use -- the',
    ' *  normal\'s one-step central difference (128 units) and the AO march (2,048)',
    ' *  -- so no texel of the chunk itself is ever computed against a clamped edge.',
    ' *  The tile baker has had it since the pyramid was written; the chunk baker',
    ' *  got it on 2026-09-10, which is what made the two paths\' sheets the same',
    ' *  bytes rather than the same within a bounded band.',
    ' */',
    'constexpr int LODGEN_TERRAIN_RING_CELLS = 1;',
    'constexpr float LODGEN_TERRAIN_RING_UNITS =',
    T + 'float( LODGEN_TERRAIN_RING_CELLS ) * 4096.0f;',
    '',
    '/*! ONE HOME for filling that ring\'s height grid.',
    ' *',
    ' *  `fetch( cx, cy )` is given RING-LOCAL cell coordinates (0..rdim-1) and',
    ' *  returns the LAND record there or null; it is the callers\' one difference --',
    ' *  the chunk baker reads the plugin directly and keeps the chunk\'s own cells,',
    ' *  the tile baker goes through its row cache. THE ITERATION ORDER IS PART OF',
    ' *  THE CONTRACT: cells are visited south to north then west to east and a later',
    ' *  cell overwrites the VHGT sample it shares with an earlier one, so both',
    ' *  callers resolve a shared cell edge the same way and the two grids agree',
    ' *  exactly where they overlap.',
    ' */',
    'template <typename Fetch>',
    'static void lodgenTerrainFillRing( std::vector<float> & hgt, int hn, int rdim,',
    T + 'float empty, Fetch fetch )',
    '{',
    T + 'hgt.assign( size_t( hn ) * hn, empty );',
    T + 'for ( int cy = 0; cy < rdim; cy++ ) {',
    T + T + 'for ( int cx = 0; cx < rdim; cx++ ) {',
    T + T + T + 'const EsmLand * land = fetch( cx, cy );',
    T + T + T + 'if ( !land )',
    T + T + T + T + 'continue;',
    T + T + T + 'for ( int row = 0; row < 33; row++ )',
    T + T + T + T + 'for ( int col = 0; col < 33; col++ )',
    T + T + T + T + T + 'hgt[size_t( cy * 32 + row ) * hn + size_t( cx * 32 + col )] =',
    T + T + T + T + T + T + 'land->heights[row][col];',
    T + T + '}',
    T + '}',
    '}',
    '']) + '\n'
i = one(anchor_a)
s = s[:i] + helpers + s[i + len(anchor_a):]

# ---------------------------------------------- B: the chunk baker's grid load
newB = '\n'.join([
    T + '/* Per-cell land data, loaded once -- and the HEIGHTS on the one-cell ring.',
    T + ' *',
    T + ' * The PAINT (`cells`, `haveLand`, `dominantBase`, the quadrant cover',
    T + ' * constants) stays scoped to the chunk: every texel this bake writes lands',
    T + ' * inside it, so a neighbour\'s paint has nothing to contribute and widening',
    T + ' * that scope would move the dominant base. The HEIGHTS are different. Both',
    T + ' * neighbourhood operators below reach outside the chunk, and until',
    T + ' * 2026-09-10 they were served a CLAMP there -- the chunk\'s own edge sample',
    T + ' * repeated outwards, a plateau that is not the ground. That clamp is what',
    T + ' * made this bake and the pyramid\'s disagree on the same chunk (V9a: 43',
    T + ' * colour texels and 5,524 msn texels on Commonwealth.4.-24.24, every one of',
    T + ' * them within 4 texels of the chunk edge) and it is what put a 7.312 step',
    T + ' * across a chunk seam where the ringed bake has 4.955. The ring here is the',
    T + ' * tile baker\'s ring, through the shared filler, not a second copy of it. */',
    T + 'const int rdim = dim + 2 * LODGEN_TERRAIN_RING_CELLS;',
    T + 'const int rx0 = chunkX - LODGEN_TERRAIN_RING_CELLS;',
    T + 'const int ry0 = chunkY - LODGEN_TERRAIN_RING_CELLS;',
    T + 'const int hn = rdim * 32 + 1;',
    T + 'std::vector<EsmLand> cells( size_t( dim ) * dim );',
    T + 'std::vector<bool> haveLand( size_t( dim ) * dim, false );',
    T + 'std::vector<float> hgt;',
    T + '{',
    T + T + 'EsmLand ringLand;',
    T + T + 'lodgenTerrainFillRing( hgt, hn, rdim, world.defaultLandHeight(),',
    T + T + T + '[&]( int cx, int cy ) -> const EsmLand * {',
    T + T + T + T + 'const int ix = cx - LODGEN_TERRAIN_RING_CELLS;',
    T + T + T + T + 'const int iy = cy - LODGEN_TERRAIN_RING_CELLS;',
    T + T + T + T + 'const bool inChunk = ( ix >= 0 && ix < dim && iy >= 0 && iy < dim );',
    T + T + T + T + 'EsmLand & land = inChunk ? cells[size_t( iy ) * dim + ix] : ringLand;',
    T + T + T + T + 'if ( !world.land( rx0 + cx, ry0 + cy, land ) )',
    T + T + T + T + T + 'return nullptr;',
    T + T + T + T + 'if ( inChunk )',
    T + T + T + T + T + 'haveLand[size_t( iy ) * dim + ix] = true;',
    T + T + T + T + 'return &land;',
    T + T + T + '} );',
    T + '}',
    T + '/* The CHUNK\'S OWN view of that grid, for the per-sample channels.',
    T + ' *',
    T + ' * They are held to the chunk deliberately and it is not an oversight: the',
    T + ' * wetness channel is a flow accumulation over the WHOLE grid it is handed,',
    T + ' * so widening the grid moves the sheet\'s interior, not its edge. That is a',
    T + ' * different defect from the clamp, it cannot be gated by byte identity',
    T + ' * against the pyramid (whose tiles accumulate over a tile-sized grid, not a',
    T + ' * chunk-sized one), and it is named as owed rather than changed here. */',
    T + 'const int cn = dim * 32 + 1;',
    T + 'std::vector<float> chgt( size_t( cn ) * cn );',
    T + 'for ( int row = 0; row < cn; row++ )',
    T + T + 'for ( int col = 0; col < cn; col++ )',
    T + T + T + 'chgt[size_t( row ) * cn + col] =',
    T + T + T + T + 'hgt[size_t( row + 32 * LODGEN_TERRAIN_RING_CELLS ) * hn',
    T + T + T + T + T + '+ size_t( col + 32 * LODGEN_TERRAIN_RING_CELLS )];',
    '', '']) + ''
cut(T + '// per-cell land data, loaded once',
    T + '/* The texture cache belongs to the CALLER when it has one:', newB)

# --------------------------------------------------- C: the msn grid coordinates
newC = '\n'.join([
    T * 3 + '/* Grid coordinates on the RING, so the central difference below has',
    T * 3 + ' * real ground on both sides at the chunk\'s own edge. The spacing is',
    T * 3 + ' * 128 world units, which is exactly what `span / ( hn - 1 )` used to',
    T * 3 + ' * evaluate to -- ( dim * 4096 ) / ( dim * 32 ) -- and both were exact',
    T * 3 + ' * powers of two, so nothing in the chunk\'s interior moves a bit. */',
    T * 3 + 'const float ngx = ( wx - cwX + LODGEN_TERRAIN_RING_UNITS ) / 128.0f;',
    T * 3 + 'const float ngy = ( wy - cwY + LODGEN_TERRAIN_RING_UNITS ) / 128.0f;',
    T * 3 + 'const float spacing = 128.0f;',
    '']) + ''
cut(T * 3 + 'const float ngx = ( wx - cwX ) / span * float( hn - 1 );',
    T * 3 + '/* The reconstruction and the channel order are SHARED with the', newC)

# ------------------------------------------------------- D: the AO height tap
newD = '\n'.join([
    T * 2 + '/* Bilinear sample of the heightfield in CHUNK-local world units, served',
    T * 2 + ' * from the ring. The march below reaches 2,048 units and until',
    T * 2 + ' * 2026-09-10 everything past the chunk edge was that edge repeated, so',
    T * 2 + ' * every chunk carried a false plateau around itself and its outermost',
    T * 2 + ' * 2,048 units of AO were shadowed by nothing. Same tap as the tile',
    T * 2 + ' * baker\'s, same function. */',
    T * 2 + 'auto heightAt = [&]( float wx, float wy ) {',
    T * 3 + 'return lodgenTerrainGridSample( hgt, hn,',
    T * 4 + 'wx + LODGEN_TERRAIN_RING_UNITS, wy + LODGEN_TERRAIN_RING_UNITS );',
    T * 2 + '};',
    '']) + ''
cut(T * 2 + '// bilinear sample of the chunk heightfield, in chunk-local world units',
    T * 2 + '/* Channel-packed terrain data map, RGBA.', newD)

# ---------------------------------------- E: the channel grids and their sampler
newE = '\n'.join([
    T * 2 + 'lodgenTerrainChannels( world, chunkX, chunkY, dim, chgt,',
    T * 3 + 'tMat, tWet, tAo2, tSky, tMat2, tShore, &tBlend );',
    T * 2 + '// the channel grids are the CHUNK\'s, so these carry no ring offset',
    T * 2 + 'auto sampleU8 = [&]( const std::vector<quint8> & f, float wx, float wy ) {',
    T * 3 + 'return lodgenTerrainGridSample( f, cn, wx, wy );',
    T * 2 + '};',
    '']) + ''
cut(T * 2 + 'lodgenTerrainChannels( world, chunkX, chunkY, dim, hgt,',
    T * 2 + 'std::vector<quint32> aoTex(', newE)

# --------------------------------------------- F0/F1/F2: the tile baker shares it
old_f0 = T + 'const int rdim = dim + 2;\n' + T + 'const int rx0 = cellX0 - 1, ry0 = cellY0 - 1;\n'
new_f0 = (T + 'const int rdim = dim + 2 * LODGEN_TERRAIN_RING_CELLS;\n'
          + T + 'const int rx0 = cellX0 - LODGEN_TERRAIN_RING_CELLS;\n'
          + T + 'const int ry0 = cellY0 - LODGEN_TERRAIN_RING_CELLS;\n')
if s.count(old_f0) != 1:
    print('F0 anchor count %d' % s.count(old_f0)); sys.exit(1)
s = s.replace(old_f0, new_f0)

newF1 = '\n'.join([
    T + 'std::vector<EsmLand> cells( size_t( rdim ) * rdim );',
    T + 'std::vector<bool> haveLand( size_t( rdim ) * rdim, false );',
    T + 'std::vector<float> hgt;',
    T + 'lodgenTerrainFillRing( hgt, hn, rdim, world.defaultLandHeight(),',
    T * 2 + '[&]( int cx, int cy ) -> const EsmLand * {',
    T * 3 + 'const EsmLand * l = landCache.get( world, rx0 + cx, ry0 + cy );',
    T * 3 + 'if ( !l )',
    T * 4 + 'return nullptr;',
    T * 3 + '// copied out at once: the cache\'s pointer dies on the next get()',
    T * 3 + 'const size_t ci = size_t( cy ) * rdim + cx;',
    T * 3 + 'cells[ci] = *l;',
    T * 3 + 'haveLand[ci] = true;',
    T * 3 + 'return &cells[ci];',
    T * 2 + '} );',
    '', '']) + ''
cut(T + 'std::vector<EsmLand> cells( size_t( rdim ) * rdim );',
    T + 'quint32 dominantBase = 0;', newF1)

newF2 = '\n'.join([
    T + '// both taps are the shared one; the tile\'s coordinates are already',
    T + '// RING-local, so they carry no offset of their own',
    T + 'auto heightAt = [&]( float lx, float ly ) {',
    T * 2 + 'return lodgenTerrainGridSample( hgt, hn, lx, ly );',
    T + '};',
    T + 'auto sampleU8 = [&]( const std::vector<quint8> & f, float lx, float ly ) {',
    T * 2 + 'return lodgenTerrainGridSample( f, hn, lx, ly );',
    T + '};',
    '']) + ''
cut(T + 'auto heightAt = [&]( float lx, float ly ) {',
    T + 'static const float dirs[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },',
    newF2)

# ------------------------------------------------------- G: the header comment
old_g = '\n'.join([
    ' *   - it bakes a one-cell RING around every tile, so the border, the msn\'s',
    ' *     central differences and the 2,048-unit AO march all have real data',
    ' *     instead of the chunk path\'s edge clamp;'])
new_g = '\n'.join([
    ' *   - it carries a BORDER on every tile, so a consumer filters and samples one',
    ' *     tile without reaching into its neighbours. The one-cell RING that feeds',
    ' *     the msn\'s central differences and the 2,048-unit AO march is NO LONGER',
    ' *     one of these: since 2026-09-10 the chunk path bakes on the same ring,',
    ' *     through the same lodgenTerrainFillRing and the same',
    ' *     lodgenTerrainGridSample, which is what lets V9a ask the two paths for',
    ' *     byte identity instead of for a bounded band;'])
if s.count(old_g) != 1:
    print('G anchor count %d' % s.count(old_g)); sys.exit(1)
s = s.replace(old_g, new_g)

old_g2 = ' * the tile baker repeats the arithmetic rather than refactoring the gated path'
new_g2 = ' * the tile baker repeats the arithmetic rather than refactoring the gated path'
# amend the "deliberately NOT shared" paragraph's last sentence
old_g3 = ' * than a shared function asserting it.'
new_g3 = '\n'.join([
    ' * than a shared function asserting it. What IS shared, since 2026-09-10, is',
    ' * everything the two paths must not be allowed to drift on: the height',
    ' * reconstruction (lodgenTerrainHeightAt), the msn encoding',
    ' * (lodgenTerrainMsnPixel), the ring fill (lodgenTerrainFillRing) and the',
    ' * bilinear tap (lodgenTerrainGridSample). The per-texel loops stay apart.'])
if s.count(old_g3) != 1:
    print('G3 anchor count %d' % s.count(old_g3)); sys.exit(1)
s = s.replace(old_g3, new_g3)

out = s.encode('utf-8')
if out.count(b'\r') != cr_before:
    print('CR COUNT MOVED: %d -> %d' % (cr_before, out.count(b'\r'))); sys.exit(1)
open(PATH, 'wb').write(out)
print('ok  CR %d (unchanged), lines %d -> %d' % (cr_before, n_before, len(s.split('\n'))))
