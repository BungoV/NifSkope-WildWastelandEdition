#!/usr/bin/env python3
"""The HORIZON2 fix, applied as a byte splice so the file's CRLF line endings
survive (root MISTAKES: line endings are measured with Python byte counts).
It reproduces exactly the rule candidate.py measured."""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/src/lodghorizon.h'
b = open(P, 'rb').read()
s = b.decode('utf-8')
crlf, lf = b.count(b'\r\n'), b.count(b'\n')
print('line endings before: CRLF %d of %d LF' % (crlf, lf))
assert crlf == 0 or crlf == lf, 'mixed line endings; refusing to splice'
NL = '\r\n' if crlf else '\n'


def sub(old, new):
    global s
    o, n = old.replace('\n', NL), new.replace('\n', NL)
    assert o in s, 'not found:\n' + old[:200]
    assert s.count(o) == 1, 'not unique'
    s = s.replace(o, n)


sub(""" *    * a step does not sample a POINT, it covers the whole SEGMENT from d to
 *      d * growth. A point-sampled 1.5x ladder skips half a kilometre of
 *      ground at 1 km out and walks straight past a tower standing in it;
 *      `maxAlong` instead walks the segment at the mip level whose square is
 *      as wide as it may be, so the march cannot step over an occluder.
 *    * HOW WIDE IT MAY BE is the design's one resolution invariant: the
 *      footprint never exceeds the AZIMUTH BIN'S OWN width at that distance.
 *      A bin is 2 sin(180/A) d wide -- 0.39 d at A = 16 -- so the level is
 *      chosen against `binWidthFraction * d`, the taps are spaced one square
 *      apart, and each tap takes the 2x2 block about it. The smear the lattice
 *      adds is therefore never larger than the smear 22.5-degree bins already
 *      carry, and it errs toward MORE occlusion, never less, which is the safe
 *      side for a shadow. About four taps fall in a step, so a bin costs some
 *      16 array reads a step and a receiver A x steps x 16 -- 5,632 at A = 16
 *      and 22 steps, with no triangle intersection anywhere in it.""",
""" *    * a step does not sample a POINT, it covers the whole SEGMENT from d to
 *      d * growth. A point-sampled ladder skips half a kilometre of ground at
 *      1 km out and walks straight past a tower standing in it; `maxAlong`
 *      instead walks the segment square by square, so the march cannot step
 *      over an occluder.
 *    * HOW WIDE THE FOOTPRINT IS comes from THE SEGMENT AND THE TAP BUDGET,
 *      not from the azimuth bin: the level is chosen so the segment fits in
 *      `LODGEN_HORIZON_MAX_TAPS` taps, the taps are spaced HALF a square so
 *      nothing is stepped over, and each tap reads THE ONE SQUARE IT LANDS IN.
 *
 *      IT DID NOT USED TO. Until lane HORIZON2 the level was chosen against
 *      `binWidthFraction * d` and each tap read the 2x2 block about it, so the
 *      footprint was deliberately as wide as the whole 22.5-degree bin and the
 *      march was a MAXIMUM OVER THE BIN'S SECTOR. That was written down here
 *      as a resolution invariant that "errs toward MORE occlusion, never less,
 *      which is the safe side for a shadow", and its price was never measured.
 *      HORIZON2 measured it, against a third witness that shares no code with
 *      this file -- 1-degree pencil rays over the raw BTD heightmap plus the
 *      .lodo/.lodi placements as exact world boxes -- on ten receivers of
 *      chunk 4.4.-12: a systematic bias of +8.52 degrees. Over 576 real texels
 *      of that chunk the sheet said 0.0% lit at a sun of azimuth 240 elevation
 *      15 where the third witness says 10.2%. The viewer does not read a
 *      sector maximum: `BtdTerrain` takes the sun's azimuth, finds the two
 *      bins either side of it and BLENDS them, which is only meaningful if
 *      each stored byte is the skyline in ITS OWN DIRECTION. Dropping the 2x2
 *      block to the one square the sample lands in takes the bias to +2.17;
 *      picking the level from the segment instead of the bin takes it to
 *      +1.83; `growth` 1.5 -> 1.3 takes it to +0.86.
 *      (scratchpad/horizon2_20260918/lane_horizon2_report.md, sections 1-2.)
 *    * A step therefore costs about `LODGEN_HORIZON_MAX_TAPS` array reads
 *      rather than four times that, and a receiver A x steps x taps, with no
 *      triangle intersection anywhere in it.""")

sub("""		/* THE 2x2 TAP IS THE FOOTPRINT, not the square: each tap reads four
		 * neighbouring squares, so a level picked at `c <= wantCell` blurs
		 * `2c` -- twice what the caller asked for and, since the caller asks
		 * for the azimuth bin's own width, one whole bin wider than the bins.
		 * The level is therefore picked at `2c <= wantCell`. */
		int level = 0;
		float c = cell;
		while ( level + 1 < int( mip.size() ) && c * 4.0f <= wantCell ) {
			level++;
			c *= 2.0f;
		}
		const float dx = x1 - x0, dy = y1 - y0;
		const float len = std::sqrt( dx * dx + dy * dy );
		int taps = int( len / c );
		if ( taps > 64 )
			taps = 64;      // a hard stop; at x1.5 growth the walk needs about 3
		float m = NONE;
		for ( int t = 0; t <= taps; t++ ) {
			const float f = taps ? float( t ) / float( taps ) : 0.0f;
			const float px = x0 + dx * f, py = y0 + dy * f;
			const int gx = int( std::floor( ( px - ox ) / c - 0.5f ) );
			const int gy = int( std::floor( ( py - oy ) / c - 0.5f ) );
			for ( int j = 0; j < 2; j++ )
				for ( int i = 0; i < 2; i++ ) {
					const float v = at( level, gx + i, gy + j );
					if ( v > m )
						m = v;
				}
		}
		return m;""",
"""		/* ONE SQUARE A TAP, not a 2x2 block. A 2x2 block is a `2c` wide
		 * dilation of every occluder on the segment, and while the caller set
		 * `wantCell` from the azimuth bin's width -- which is what this march
		 * did until lane HORIZON2 -- that dilation was one whole bin, which
		 * made the walk a maximum over the bin's sector rather than a reading
		 * along its own direction. Measured price of that: +8.52 degrees of
		 * systematic bias against a witness that shares no code with this file.
		 *
		 * Nothing is stepped over, because the taps are spaced HALF a square:
		 * a segment that crosses a square enters and leaves it more than half
		 * a square apart, so at least one tap lands inside it. */
		int level = 0;
		float c = cell;
		while ( level + 1 < int( mip.size() ) && c * 4.0f <= wantCell ) {
			level++;
			c *= 2.0f;
		}
		const float dx = x1 - x0, dy = y1 - y0;
		const float len = std::sqrt( dx * dx + dy * dy );
		int taps = int( 2.0f * len / c );
		if ( taps > 2 * LODGEN_HORIZON_MAX_TAPS )
			taps = 2 * LODGEN_HORIZON_MAX_TAPS;   // a hard stop on one segment
		float m = NONE;
		for ( int t = 0; t <= taps; t++ ) {
			const float f = taps ? float( t ) / float( taps ) : 0.0f;
			const float px = x0 + dx * f, py = y0 + dy * f;
			const int gx = int( std::floor( ( px - ox ) / c ) );
			const int gy = int( std::floor( ( py - oy ) / c ) );
			const float v = at( level, gx, gy );
			if ( v > m )
				m = v;
		}
		return m;""")

sub("""			/* THE RESOLUTION INVARIANT: the lattice square the segment is read
			 * at is never wider than the azimuth bin is at this distance. */
			const float wantCell = std::max( k.binWidthFraction * d, 1.0f );""",
"""			/* THE FOOTPRINT COMES FROM THE SEGMENT, NOT FROM THE BIN. The level
			 * only has to keep the segment inside the tap budget; choosing it
			 * against `binWidthFraction * d` instead, which is what this march
			 * did until lane HORIZON2, widened every sample to the whole
			 * azimuth bin and made the stored byte a sector maximum -- which is
			 * not the quantity the viewer interpolates. See `maxAlong`. */
			const float wantCell = std::max( ( dEnd - d ) / float( LODGEN_HORIZON_MAX_TAPS ), 1.0f );""")

sub("""	float growth = 1.5f;                           //!< each step is this many times the last""",
"""	/*! Each step is this many times the last. The elevation is taken at the
	 *  segment's NEAR end, so a segment whose occluder stands at its FAR end
	 *  over-states `tan(elevation)` by up to `growth`. At 1.5 that is half.
	 *  Lane HORIZON2 measured the residual bias after the footprint fix at
	 *  +2.74 degrees at 1.5 and +0.86 at 1.3, for 32 steps a bin instead of 21;
	 *  1.2 would give -0.46 at 46 steps. */
	float growth = 1.3f;""")

sub("""constexpr float LODGEN_HORIZON_NEAR_CELL = 32.0f;""",
"""constexpr float LODGEN_HORIZON_NEAR_CELL = 32.0f;

/*! Taps a `maxAlong` segment is budgeted. The mip level is chosen so the
 *  segment fits in this many squares and the taps are then spaced half a square,
 *  so a segment costs at most twice this many array reads. This is what bounds
 *  the walk at long range now that the footprint is no longer set by the
 *  azimuth bin's width. */
constexpr int LODGEN_HORIZON_MAX_TAPS = 64;""")

out = s.encode('utf-8')
open(P, 'wb').write(out)
print('line endings after:  CRLF %d of %d LF' % (out.count(b'\r\n'), out.count(b'\n')))
print('bytes %d -> %d' % (len(b), len(out)))
