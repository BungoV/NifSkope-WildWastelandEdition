#!/usr/bin/env python
"""TERRAINFIX step 2: a cell with no LAND record stops reading as flat zero.

Measured against the shadow heightmaps of the same worldspaces (which reproduce
Bethesda's own Commonwealth_fine byte for byte): DiamondCity 167,936 texels,
NukaWorldAmphitheater 97, DLC03FarHarbor 62. Two causes, both here."""

P = 'src/lodtfile.cpp'
b = open(P, 'rb').read().decode('utf-8')
cr0 = b.count('\r')


def sub(old, new, n=1):
    global b
    c = b.count(old)
    assert c == n, 'anchor count %d != %d for %r' % (c, n, old[:70])
    b = b.replace(old, new)


# --- the one height encoding, in one place ------------------------------
sub('''//! Little-endian appenders. Everything in the file is LE regardless of host.''',
    '''/*! World units -> the stored word. ONE encoding, used by every plane the
 *  writer emits and by the landless-cell fallback, because they were two
 *  expressions and they disagreed: the fallback used a bare 32767, which is
 *  height ZERO, where the rest of the file (and the shadow heightmap) use the
 *  worldspace's default land height. */
static inline quint16 lodtHeightWord( double h, double quantum )
{
\treturn quint16( qBound( 0.0, std::floor( h / quantum + 32767.0 + 0.5 ), 65535.0 ) );
}

//! Little-endian appenders. Everything in the file is LE regardless of host.''')

# --- the source reports what a landless cell inherits -------------------
sub('''\t//! cell water height and type form; false = none
\tstd::function<bool( int, int, float &, quint32 & )> water;''',
    '''\t//! cell water height and type form; false = none
\tstd::function<bool( int, int, float &, quint32 & )> water;
\t/*! The range of the samples a cell with NO landscape of its own still
\t *  inherits from a neighbour across the shared VHGT edge; false = it
\t *  inherits nothing. Optional -- a .btd has no shared edge. */
\tstd::function<bool( int, int, float &, float & )> edgeRange;''')

# --- pass one: a landless cell's range must cover the row it inherits ---
sub('''\t\t\t} else {
\t\t\t\tlo = hi = src.defaultLand;
\t\t\t}''',
    '''\t\t\t} else {
\t\t\t\t/* No LAND record: the surface here is the worldspace's default
\t\t\t\t * land height -- what the shadow heightmap writes for exactly
\t\t\t\t * these texels -- except on the row and column this cell shares
\t\t\t\t * with a neighbour that does have terrain. Without those in the
\t\t\t\t * range, a renderer culling on it would cull the inherited row. */
\t\t\t\tlo = hi = src.defaultLand;
\t\t\t\tfloat elo = 0.0f, ehi = 0.0f;
\t\t\t\tif ( src.edgeRange && src.edgeRange( cx, cy, elo, ehi ) ) {
\t\t\t\t\tlo = qMin( lo, elo );
\t\t\t\t\thi = qMax( hi, ehi );
\t\t\t\t}
\t\t\t}''')

# --- the plane fallback is the default height, not zero -----------------
sub('''\t\tif ( !cp )
\t\t\treturn plane == 0 ? quint16( 32767 )
\t\t\t\t: ( plane == 2 ? quint16( 0xFFFFU ) : quint16( 0 ) );''',
    '''\t\tif ( !cp )
\t\t\treturn plane == 0 ? lodtHeightWord( src.defaultLand, quantum )
\t\t\t\t: ( plane == 2 ? quint16( 0xFFFFU ) : quint16( 0 ) );''')

# --- the ESM source: fill a landless cell that inherits an edge ---------
sub('''\tsrc.planes = [get, seams]( int cx, int cy, const quint32 * slotForms, float quantum,
\t\tstd::vector<quint16> & h, std::vector<quint16> & a,
\t\tstd::vector<quint16> & c, std::vector<quint16> & g )
\t{
\t\tif ( !get( cx, cy ) )
\t\t\treturn false;
\t\tSeam sm;
\t\tseams( cx, cy, sm );
\t\tconst EsmLand * l = get( cx, cy );   // after the neighbours: see MemoSet
\t\tconst int spc = 32;
\t\th.assign( size_t( spc ) * spc, 32767 );''',
    '''\t/*! What a landless cell inherits, for the per-cell range. The same three
\t *  neighbours the seam rule reads, and nothing else can reach it. */
\tsrc.edgeRange = [get, seams]( int cx, int cy, float & lo, float & hi ) {
\t\tif ( get( cx, cy ) )
\t\t\treturn false;                    // it has land: not this rule's business
\t\tSeam sm;
\t\tseams( cx, cy, sm );
\t\tif ( !sm.s && !sm.w && !sm.sw )
\t\t\treturn false;
\t\tlo = 3.4e38f;
\t\thi = -3.4e38f;
\t\tauto take = [&lo, &hi]( float v ) { lo = qMin( lo, v ); hi = qMax( hi, v ); };
\t\tfor ( int c = 0; c < 32; c++ )
\t\t\tif ( sm.s )
\t\t\t\ttake( sm.sRow[c] );
\t\tfor ( int r = 0; r < 32; r++ )
\t\t\tif ( sm.w )
\t\t\t\ttake( sm.wCol[r] );
\t\tif ( sm.sw )
\t\t\ttake( sm.swC );
\t\treturn true;
\t};
\tsrc.planes = [get, seams, defaultLand = world.defaultLandHeight()](
\t\tint cx, int cy, const quint32 * slotForms, float quantum,
\t\tstd::vector<quint16> & h, std::vector<quint16> & a,
\t\tstd::vector<quint16> & c, std::vector<quint16> & g )
\t{
\t\tSeam sm;
\t\tseams( cx, cy, sm );
\t\tconst EsmLand * l = get( cx, cy );   // after the neighbours: see MemoSet
\t\tconst int spc = 32;
\t\tif ( !l ) {
\t\t\t/* NO LAND RECORD, and the cell to the south or the west has one.
\t\t\t * VHGT's row 32 / column 32 IS this cell's row 0 / column 0, so
\t\t\t * that row is real terrain -- and it was being written as flat
\t\t\t * ZERO, because the plane was refused outright and the caller
\t\t\t * substituted a bare 32767 sentinel.
\t\t\t *
\t\t\t * MEASURED against the shadow heightmaps, which apply the seam
\t\t\t * maximum whether or not the sample's own cell exists and which
\t\t\t * reproduce Bethesda's Commonwealth_fine byte for byte:
\t\t\t * DLC03FarHarbor 62 texels differed (ONE landless cell ringed by
\t\t\t * eight that have land), NukaWorldAmphitheater 97 over four cells,
\t\t\t * DiamondCity 167,936 of 172,032 -- 164 landless cells whose whole
\t\t\t * interior read 0 against that worldspace's default land height of
\t\t\t * -2048. The Commonwealth is fully dense, has not one landless
\t\t\t * cell, and stayed byte-identical throughout: that is why nothing
\t\t\t * caught this.
\t\t\t *
\t\t\t * The two files are read as ONE surface -- terrain from here, far
\t\t\t * shadows from the heightmap -- so a sample they disagree on is the
\t\t\t * ridge-that-casts-a-shadow-without-being-drawn of 2026-09-05c,
\t\t\t * one row in from a hole in the landscape.
\t\t\t *
\t\t\t * The default height does NOT take part in the maximum on an
\t\t\t * inherited sample, exactly as it does not in the heightmap: Far
\t\t\t * Harbor's default is 0 and its inherited row is around -250, and
\t\t\t * a max against the default would have kept every one of those 62
\t\t\t * texels wrong. */
\t\t\tif ( !sm.s && !sm.w && !sm.sw )
\t\t\t\treturn false;                // nothing inherited: the caller's default
\t\t\th.assign( size_t( spc ) * spc, 0 );
\t\t\ta.assign( size_t( spc ) * spc, 0 );
\t\t\tc.assign( size_t( spc ) * spc, 0xFFFFU );
\t\t\tg.clear();
\t\t\tfor ( int r = 0; r < spc; r++ ) {
\t\t\t\tfor ( int cc = 0; cc < spc; cc++ ) {
\t\t\t\t\tfloat hh = defaultLand;
\t\t\t\t\tbool inherited = false;
\t\t\t\t\tauto take = [&hh, &inherited]( float v ) {
\t\t\t\t\t\thh = inherited ? qMax( hh, v ) : v;
\t\t\t\t\t\tinherited = true;
\t\t\t\t\t};
\t\t\t\t\tif ( r == 0 && sm.s )
\t\t\t\t\t\ttake( sm.sRow[cc] );
\t\t\t\t\tif ( cc == 0 && sm.w )
\t\t\t\t\t\ttake( sm.wCol[r] );
\t\t\t\t\tif ( r == 0 && cc == 0 && sm.sw )
\t\t\t\t\t\ttake( sm.swC );
\t\t\t\t\th[size_t( r ) * size_t( spc ) + size_t( cc )] =
\t\t\t\t\t\tlodtHeightWord( hh, quantum );
\t\t\t\t}
\t\t\t}
\t\t\treturn true;
\t\t}
\t\th.assign( size_t( spc ) * spc, 32767 );''')

# the landed path's own encoding goes through the one helper too
sub('''\t\t\t\tconst double v = double( hh ) / double( quantum ) + 32767.0;
\t\t\t\th[k] = quint16( qBound( 0.0, std::floor( v + 0.5 ), 65535.0 ) );''',
    '''\t\t\t\th[k] = lodtHeightWord( hh, quantum );''')

assert b.count('\r') == cr0, 'line endings moved'
open(P, 'wb').write(b.encode('utf-8'))
print('lodtfile.cpp patched, CR %d (unchanged)' % cr0)
