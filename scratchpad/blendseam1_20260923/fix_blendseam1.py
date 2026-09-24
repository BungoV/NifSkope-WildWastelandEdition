"""BLENDSEAM1: the stock chunk colour writer cross-fades across the chunk edge too.

Anchored, exact-once, LF-checked. --check writes nothing."""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/src/lodgen.cpp'
CHECK = '--check' in sys.argv
b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')

EDITS = [
    # 1. the header comment on the chunk-scoped paint
    ("""	 * The PAINT (`cells`, `haveLand`, `dominantBase`, the quadrant cover
	 * constants) stays scoped to the chunk: every texel this bake writes lands
	 * inside it, so a neighbour's paint has nothing to contribute and widening
	 * that scope would move the dominant base. The HEIGHTS are different. Both
""",
     """	 * The PAINT (`cells`, `haveLand`, `dominantBase`, the quadrant cover
	 * constants) stays scoped to the chunk: every texel this bake writes lands
	 * inside it, and widening that scope would move the dominant base. The one
	 * reader of a neighbour's paint is the quadrant cross-fade, which gets the
	 * ring's paint in a separate array (`ringPaint`, below) so that nothing
	 * else can see it. The HEIGHTS are different. Both
"""),
    # 2. the ring paint arrays
    ("""	std::vector<EsmLand> cells( size_t( dim ) * dim );
	std::vector<bool> haveLand( size_t( dim ) * dim, false );
	std::vector<float> hgt;
	{
		EsmLand ringLand;
""",
     """	std::vector<EsmLand> cells( size_t( dim ) * dim );
	std::vector<bool> haveLand( size_t( dim ) * dim, false );
	/* THE RING'S PAINT, for the quadrant cross-fade and for nothing else (lane
	 * BLENDSEAM1, 2026-09-23). A quadrant line on the chunk's own edge is a
	 * quadrant line like the other seven, and the pyramid -- the writer whose
	 * sheet ships -- has always blended it, because its tile grid carries the
	 * same one-cell ring with the paint loaded. This bake fell back to its own
	 * colour there, so with the blend on the two writers disagreed on every
	 * texel within the margin of the chunk edge (6,718 texels over the four V9a
	 * chunks, max 25 levels, every one within 3 px of the edge). Kept apart from
	 * `cells` so the dominant base, the cover constants and every statistic stay
	 * chunk-scoped; filled only when the blend is on, so `--blend-edges off`
	 * does exactly the work it did before. Indexed ring-local, 0..rdim-1. */
	const bool ringPaintWanted = ( lodgenBlendEdges() == 1 );
	std::vector<EsmLand> ringPaint( ringPaintWanted ? size_t( rdim ) * rdim : 0 );
	std::vector<bool> haveRingPaint( ringPaint.size(), false );
	std::vector<float> hgt;
	{
		EsmLand ringLand;
"""),
    # 3. fill it
    ("""				if ( !world.land( rx0 + cx, ry0 + cy, land ) )
					return nullptr;
				if ( inChunk )
					haveLand[size_t( iy ) * dim + ix] = true;
				return &land;
""",
     """				if ( !world.land( rx0 + cx, ry0 + cy, land ) )
					return nullptr;
				if ( inChunk )
					haveLand[size_t( iy ) * dim + ix] = true;
				else if ( ringPaintWanted ) {
					const size_t ri = size_t( cy ) * rdim + cx;
					ringPaint[ri] = land;
					haveRingPaint[ri] = true;
				}
				return &land;
"""),
    # 4. the comment in the cross-fade
    ("""					 * quadrants bilinearly. A neighbour outside the chunk has
					 * no paint loaded and falls back to this quadrant's own
					 * colour; the adjacent chunk's bake falls back the same way
					 * from its side, so no new seam appears at a chunk border.
					 */
""",
     """					 * quadrants bilinearly. A neighbour outside the chunk is
					 * read from the one-cell ring's paint (`ringPaint`), exactly
					 * as the pyramid reads its tile's ring, so the chunk's own
					 * edge is blended like every other quadrant line and the
					 * adjacent chunk meets it on the same 50/50 mix (lane
					 * BLENDSEAM1). Only a cell with no LAND falls back to this
					 * quadrant's own colour, on both writers.
					 */
"""),
    # 5. the neighbour lookup
    ("""							if ( ncx < 0 || ncx >= dim || ncy < 0 || ncy >= dim )
								return color;
							const size_t nci = size_t( ncy ) * dim + ncx;
							if ( !haveLand[nci] )
								return color;
""",
     """							const EsmLand * npl = nullptr;
							if ( ncx >= 0 && ncx < dim && ncy >= 0 && ncy < dim ) {
								const size_t nci = size_t( ncy ) * dim + ncx;
								if ( haveLand[nci] )
									npl = &cells[nci];
							} else {
								const int rcx = ncx + LODGEN_TERRAIN_RING_CELLS;
								const int rcy = ncy + LODGEN_TERRAIN_RING_CELLS;
								const size_t ri = size_t( rcy ) * rdim + rcx;
								if ( rcx >= 0 && rcx < rdim && rcy >= 0 && rcy < rdim
									&& ri < ringPaint.size() && haveRingPaint[ri] )
									npl = &ringPaint[ri];
							}
							if ( !npl )
								return color;
"""),
    ("""							return quadComposite( cells[nci], ( by << 1 ) | bx,
								nlx / 2048.0f, nly / 2048.0f, false );
""",
     """							return quadComposite( *npl, ( by << 1 ) | bx,
								nlx / 2048.0f, nly / 2048.0f, false );
"""),
]

ok = True
for i, (a, _) in enumerate(EDITS):
    n = s.count(a)
    print('edit %d: anchor count %d' % (i + 1, n))
    ok &= (n == 1)
if not ok:
    print('REFUSED: an anchor is not exactly-once')
    sys.exit(1)
if CHECK:
    print('check only, nothing written; CR in file %d' % cr0)
    sys.exit(0)
for a, r in EDITS:
    s = s.replace(a, r)
out = s.encode('utf-8')
assert out.count(b'\r') == cr0, 'CR count moved'
open(P, 'wb').write(out)
print('written; CR %d -> %d, lines +%d' % (cr0, out.count(b'\r'), out.count(b'\n') - b.count(b'\n')))
