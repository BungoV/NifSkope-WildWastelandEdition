"""FARRING1 step 1: src/lodgen.h -- the atlas BC1 flag and the far-ring
simplification options.  LF-only file; the CR count must stay 0."""
import io
import sys

P = 'src/lodgen.h'
b = open(P, 'rb').read()
assert b.count(b'\r') == 0, 'lodgen.h must be LF-only'
s = b.decode('utf-8')


def once(hay, needle):
    n = hay.count(needle)
    assert n == 1, 'anchor matched %d times, want 1:\n%s' % (n, needle[:200])


# ---- 1. the atlas gains a BC1 flag ------------------------------------------
A1 = """ * so the output is self-contained; pass an empty looseRoot to skip. */
bool lodgenBuildAtlas( const QStringList & btoPaths, const QString & dataRoot,
\tconst QString & atlasFileBase, const QString & atlasGameBase,
\tconst QString & looseRoot, QString * error );
"""
once(s, A1)
B1 = """ * so the output is self-contained; pass an empty looseRoot to skip.
 *
 * bc1 writes the DIFFUSE sheet as BC1 (DXT1) with one-bit alpha for the
 * cut-outs instead of BC3, which is what vanilla's own sheet is: measured,
 * `Commonwealth.Objects.DDS` is 4096x2048 DXT1, 13 mips, 5,592,552 bytes.
 * Half the memory for a sheet whose alpha is only ever a cut-out mask, so it
 * is the stock target's default; FO4CS keeps BC3 for its eight-bit alpha.
 * The normal sheet stays BC3 and `_s` stays BC5. */
bool lodgenBuildAtlas( const QStringList & btoPaths, const QString & dataRoot,
\tconst QString & atlasFileBase, const QString & atlasGameBase,
\tconst QString & looseRoot, bool bc1, QString * error );
"""
s = s.replace(A1, B1)

# ---- 2. far-ring simplification --------------------------------------------
A2 = """bool lodgenMergeChunkShapes( const QStringList & btoPaths, QString * report, QString * error );
"""
once(s, A2)
B2 = A2 + """
/*! Far-ring proxy simplification: how much of a chunk's geometry survives at
 *  each ring.  Every engine since 2017 replaces a far cluster with one
 *  simplified mesh; ours simplifies the MERGED shape in place, which is the
 *  same thing once the merge has already made one shape per material.
 *
 *  A ratio of 1 leaves the ring untouched.  Ring 0 (dim 4) is what the player
 *  walks up to and is never simplified at all, whatever is set here -- the
 *  byte-identity gate for the near chunk depends on that.
 */
struct LodgenSimplifyOptions
{
\tbool enabled = true;
\tfloat ratio8 = 1.0f;        //!< ring 1 (dim 8): off by default
\tfloat ratio16 = 0.35f;      //!< ring 2 (dim 16)
\tfloat ratio32 = 0.20f;      //!< ring 3 (dim 32)
\t/*! Tolerated deviation in WORLD units at ring 0, scaled by the ring's dim
\t *  (ring 2 tolerates 4x it, ring 3 8x): the simplifier stops early rather
\t *  than exceed it, so the ratio is a target and this is the rail.  Note
\t *  that a chunk shape's vertices are miniatures divided by the ring's dim,
\t *  so a bound that grows with the ring is a CONSTANT in the file's own
\t *  units -- which is the point: the same on-screen error at every ring. */
\tfloat errorWorld = 32.0f;
\t//! A group of triangles this small keeps every one of them.
\tint minTris = 8;
};

//! The ratio for one ring; 1 (untouched) for ring 0 and anything unknown.
float lodgenSimplifyRatio( const LodgenSimplifyOptions & opts, int dim );

/*! Simplify each far chunk's merged shapes, AFTER the merge.
 *
 *  Per shape, triangles are grouped by (object identity index, texture-array
 *  layer) and each group is simplified on its own, so a collapse can never
 *  weld two objects together, never interpolates the identity index in the
 *  vertex colours (the surviving vertices are a SUBSET of the originals --
 *  meshoptimizer creates no new ones), and never crosses an array layer.
 *  Every other channel of docs/LODGEN_VERTEX_PACKING.md rides along as a
 *  weighted attribute so the metric keeps it meaningful: normal, UV, sky
 *  visibility (UV2.x), baked AO (colour B), sway (colour A) and the
 *  ground-contact blend (Eye Data).
 *
 *  Shapes with an alpha property are left ALONE -- a cut-out card's four
 *  vertices cannot lose one -- and so is any group whose object index appears
 *  on a `C` (impostor card) manifest line.  Segments are regrouped after the
 *  cut by the cell of each triangle's CENTROID, bounds and multi-bounds are
 *  recomputed, and the manifest's rows are untouched. */
bool lodgenSimplifyFarRings( const QStringList & btoPaths, const LodgenSimplifyOptions & opts,
\tQString * report, QString * error );
"""
s = s.replace(A2, B2)

out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('lodgen.h: %d -> %d bytes, CR %d' % (len(b), len(out), out.count(b'\r')))
