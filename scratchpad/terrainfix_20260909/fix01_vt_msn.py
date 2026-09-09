#!/usr/bin/env python
"""TERRAINFIX step 1: the VT pyramid's _msn gets the chunk path's two 2026-09-07
fixes, through SHARED code, and the flat-normal fill constant stops being the
renderer's tangent-space one in the wrong byte order."""
import io, sys

P = 'src/lodgen.cpp'
b = open(P, 'rb').read().decode('utf-8')
cr0 = b.count('\r')


def sub(old, new, n=1):
    global b
    c = b.count(old)
    assert c == n, 'anchor count %d != %d for %r' % (c, n, old[:70])
    b = b.replace(old, new)


# --- 1. the chunk path calls the shared helpers -------------------------
old_chunk_head = '''\t\t\t/* BILINEAR, not nearest. `int( ngx )` truncated, so every texel in a
\t\t\t * 4x4 group read the same grid point and got the same normal: the
\t\t\t * sheet held 129x129 distinct values magnified into 512x512.
\t\t\t * Measured on our own output against vanilla's, same tile, the
\t\t\t * fraction of texels differing from their left neighbour by x mod 4
\t\t\t * was 83.3 / 0.0 / 0.0 / 0.0 for us against 99.8 / 64.4 / 64.7 /
\t\t\t * 64.3 for vanilla - every block of ours was flat.
\t\t\t *
\t\t\t * The central difference spans a full grid step either side of the
\t\t\t * sample, so the gradient is continuous rather than piecewise
\t\t\t * constant and the blocks go. It cannot add detail: vanilla carries
\t\t\t * 6.6-8.2x our high-frequency energy because Bethesda baked from
\t\t\t * finer terrain than the 128-unit VHGT grid the ESM ships. */
\t\t\t/* BILINEAR, and deliberately not smoother.'''
new_chunk_head = '''\t\t\t/* The reconstruction and the channel order are SHARED with the
\t\t\t * virtual-texture tile baker -- lodgenTerrainHeightAt and
\t\t\t * lodgenTerrainMsnPixel at the top of this file. They were twelve
\t\t\t * lines copied into `lodgenBakeVtTile`, and that copy kept BOTH of
\t\t\t * the defects below after this one lost them (nearest sampling and
\t\t\t * up-in-blue), which is the reason there is now one home.
\t\t\t *
\t\t\t * BILINEAR, and deliberately not smoother.'''
sub(old_chunk_head, new_chunk_head)

old_lambda = '''\t\t\tauto heightAt = [&hgt, hn]( float gx, float gy ) {
\t\t\t\tconst float cx2 = qBound( 0.0f, gx, float( hn - 1 ) - 0.001f );
\t\t\t\tconst float cy2 = qBound( 0.0f, gy, float( hn - 1 ) - 0.001f );
\t\t\t\tconst int ix = int( cx2 ), iy = int( cy2 );
\t\t\t\tauto ease = []( float t ) {
\t\t\t\t\treturn t * t * t * ( t * ( t * 6.0f - 15.0f ) + 10.0f );
\t\t\t\t};
\t\t\t\tconst float tx = ease( cx2 - float( ix ) ), ty = ease( cy2 - float( iy ) );
\t\t\t\tconst float h00 = hgt[size_t( iy ) * hn + ix];
\t\t\t\tconst float h10 = hgt[size_t( iy ) * hn + ix + 1];
\t\t\t\tconst float h01 = hgt[size_t( iy + 1 ) * hn + ix];
\t\t\t\tconst float h11 = hgt[size_t( iy + 1 ) * hn + ix + 1];
\t\t\t\treturn ( h00 * ( 1.0f - tx ) + h10 * tx ) * ( 1.0f - ty )
\t\t\t\t\t+ ( h01 * ( 1.0f - tx ) + h11 * tx ) * ty;
\t\t\t};
\t\t\tconst float dzdx = ( heightAt( ngx + 1.0f, ngy )
\t\t\t\t- heightAt( ngx - 1.0f, ngy ) ) / ( 2.0f * spacing );
\t\t\tconst float dzdy = ( heightAt( ngx, ngy + 1.0f )
\t\t\t\t- heightAt( ngx, ngy - 1.0f ) ) / ( 2.0f * spacing );'''
new_lambda = '''\t\t\tconst float dzdx = ( lodgenTerrainHeightAt( hgt, hn, ngx + 1.0f, ngy )
\t\t\t\t- lodgenTerrainHeightAt( hgt, hn, ngx - 1.0f, ngy ) ) / ( 2.0f * spacing );
\t\t\tconst float dzdy = ( lodgenTerrainHeightAt( hgt, hn, ngx, ngy + 1.0f )
\t\t\t\t- lodgenTerrainHeightAt( hgt, hn, ngx, ngy - 1.0f ) ) / ( 2.0f * spacing );'''
sub(old_lambda, new_lambda)

# the up-in-green argument now lives on lodgenTerrainMsnPixel
i = b.index('\t\t\t/* FALLOUT 4 PUTS UP IN GREEN.')
j = b.index('\t\t\tFloatVector4 color( 0.5f, 0.5f, 0.5f, 1.0f );', i)
old_tail = b[i:j]
assert 'msn[size_t( py ) * RES + px]' in old_tail and len(old_tail) < 2600
new_tail = '''\t\t\tmsn[size_t( py ) * RES + px] = lodgenTerrainMsnPixel( nrm );

'''
sub(old_tail, new_tail)

# --- 2. the VT tile baker: nearest -> the shared eased bilinear, and the
#        channel order it never had ---------------------------------------
old_vt = '''\t\t\t// the normal first: the cover gate reads its Z
\t\t\tconst float ngx = lx / 128.0f;
\t\t\tconst float ngy = ly / 128.0f;
\t\t\tconst int hx = qBound( 1, int( ngx ), hn - 2 );
\t\t\tconst int hy = qBound( 1, int( ngy ), hn - 2 );
\t\t\tconst float dzdx = ( hgt[size_t( hy ) * hn + hx + 1]
\t\t\t\t- hgt[size_t( hy ) * hn + hx - 1] ) / 256.0f;
\t\t\tconst float dzdy = ( hgt[size_t( hy + 1 ) * hn + hx]
\t\t\t\t- hgt[size_t( hy - 1 ) * hn + hx] ) / 256.0f;
\t\t\tVector3 nrm( -dzdx, -dzdy, 1.0f );
\t\t\tnrm.normalize();
\t\t\tout.msn[size_t( j ) * S + i] = 0xFF000000U
\t\t\t\t| ( quint32( qBound( 0, int( ( nrm[0] * 0.5f + 0.5f ) * 255.0f + 0.5f ), 255 ) ) << 16 )
\t\t\t\t| ( quint32( qBound( 0, int( ( nrm[1] * 0.5f + 0.5f ) * 255.0f + 0.5f ), 255 ) ) << 8 )
\t\t\t\t| quint32( qBound( 0, int( ( nrm[2] * 0.5f + 0.5f ) * 255.0f + 0.5f ), 255 ) );'''
new_vt = '''\t\t\t/* The normal first: the cover gate reads its Z.
\t\t\t *
\t\t\t * THROUGH THE SHARED RECONSTRUCTION AND THE SHARED ENCODER, both at
\t\t\t * the top of this file. Until 2026-09-09 these lines were their own
\t\t\t * copy and carried both of the defects the chunk path lost on
\t\t\t * 2026-09-07: `int( ngx )` -- NEAREST, so all sixteen texels of a
\t\t\t * 4x4 block read one grid point and got one normal -- and north in
\t\t\t * green with up in blue, the conventional order, which on the chunk
\t\t\t * path cost 67.7% of the light and 92.0% of the shading variation
\t\t\t * when it was measured. That matters here and not only in a future
\t\t\t * consumer: with --vt on, the .btr chunk sheets are ASSEMBLED from
\t\t\t * these tiles (docs/LODGEN_TERRAIN_VT.md 2.4), so a --vt bake was
\t\t\t * shipping the pre-2026-09-07 _msn into the stock engine.
\t\t\t *
\t\t\t * The grid step is 128 world units on both paths, so the central
\t\t\t * difference divides by 2*128 exactly as the chunk path's
\t\t\t * 2*spacing does. */
\t\t\tconst float ngx = lx / 128.0f;
\t\t\tconst float ngy = ly / 128.0f;
\t\t\tconst float dzdx = ( lodgenTerrainHeightAt( hgt, hn, ngx + 1.0f, ngy )
\t\t\t\t- lodgenTerrainHeightAt( hgt, hn, ngx - 1.0f, ngy ) ) / 256.0f;
\t\t\tconst float dzdy = ( lodgenTerrainHeightAt( hgt, hn, ngx, ngy + 1.0f )
\t\t\t\t- lodgenTerrainHeightAt( hgt, hn, ngx, ngy - 1.0f ) ) / 256.0f;
\t\t\tVector3 nrm( -dzdx, -dzdy, 1.0f );
\t\t\tnrm.normalize();
\t\t\tout.msn[size_t( j ) * S + i] = lodgenTerrainMsnPixel( nrm );'''
sub(old_vt, new_vt)

# the pyramid filter re-encodes an averaged normal: same order, same helper
old_filt = '''\t\t\t\tout.msn[o] = 0xFF000000U
\t\t\t\t\t| ( quint32( qBound( 0, int( ( n[0] * 0.5f + 0.5f ) * 255.0f + 0.5f ), 255 ) ) << 16 )
\t\t\t\t\t| ( quint32( qBound( 0, int( ( n[1] * 0.5f + 0.5f ) * 255.0f + 0.5f ), 255 ) ) << 8 )
\t\t\t\t\t| quint32( qBound( 0, int( ( n[2] * 0.5f + 0.5f ) * 255.0f + 0.5f ), 255 ) );'''
new_filt = '''\t\t\t\tout.msn[o] = lodgenTerrainMsnPixel( Vector3( n[0], n[1], n[2] ) );'''
sub(old_filt, new_filt)

# --- 3. the flat fill constant, all five sites --------------------------
sub('std::vector<quint32> msn( size_t( RES ) * RES, 0xFFFF8080U );',
    'std::vector<quint32> msn( size_t( RES ) * RES, LODGEN_MSN_FLAT );')
sub('\tout.msn.assign( size_t( S ) * S, 0xFFFF8080U );',
    '\tout.msn.assign( size_t( S ) * S, LODGEN_MSN_FLAT );')
sub('\t\t\tbgra3[1] = 0xFFFF8080U;', '\t\t\tbgra3[1] = LODGEN_MSN_FLAT;')
sub('\tout.msn.assign( size_t( stored ) * stored, 0xFFFF8080U );',
    '\tout.msn.assign( size_t( stored ) * stored, LODGEN_MSN_FLAT );')
sub('\t\t\tstd::vector<quint32> nrm( size_t( RES ) * RES, 0xFFFF8080U );',
    '\t\t\tstd::vector<quint32> nrm( size_t( RES ) * RES, LODGEN_MSN_FLAT );')

# the only 0xFFFF8080 left is the sentence in LODGEN_MSN_FLAT's comment saying
# what the constant used to be
assert b.count('0xFFFF8080') == 1, 'a flat-normal fill site was missed'
assert b.count('0xFFFF8080U') == 0, 'a flat-normal fill site was missed'
assert b.count('\r') == cr0, 'line endings moved'
open(P, 'wb').write(b.encode('utf-8'))
print('lodgen.cpp patched, CR %d (unchanged)' % cr0)
