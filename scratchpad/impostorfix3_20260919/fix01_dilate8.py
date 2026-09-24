"""IMPOSTORFIX3 fix 01 -- the height fill OUTSIDE coverage becomes an 8-texel
dilation of the object's own height instead of a jump to the card plane.

src/lodgen.cpp, lodgenRepairOctHeight. Asserted exact-once, LF preserved.
"""
import sys, io

P = 'E:/Projects/NifskopeWildWastelandEdition/src/lodgen.cpp'
b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')

def once(anchor):
    n = s.count(anchor)
    if n != 1:
        print('ANCHOR COUNT %d (want 1) for:\n%s' % (n, anchor[:160]))
        sys.exit(2)

# ---------------------------------------------------------------- 1. the law
A1 = """ *  THE REPAIR, in the frame's own coordinates and nowhere else:
 *    coverage >= 250   the texel is whole; its height stands.
 *    16 <= cov < 250   partial; take the height of the nearest FULLY covered
 *                      texel, which is a depth some surface actually had.
 *    coverage < 16     outside the object; the card plane, 128, which makes
 *                      the parallax step an exact no-op and leaves the pixel
 *                      where it belongs.
"""
B1 = """ *  THE REPAIR, in the frame's own coordinates and nowhere else:
 *    coverage >= 250   the texel is whole; its height stands.
 *    16 <= cov < 250   partial; take the height of the nearest FULLY covered
 *                      texel, which is a depth some surface actually had.
 *    coverage < 16,    outside the object but WITHIN 8 RINGS of a whole texel:
 *      ring <= 8       the same dilated height. A neighbouring frame's ray
 *                      lands just outside this frame's silhouette constantly,
 *                      and out there the object's own depth is the only honest
 *                      answer; the card plane is a claim that the surface is
 *                      at z = 0, which for a fat solid object is the one place
 *                      it certainly is not.
 *    coverage < 16,    far outside; the card plane, 128, which makes the
 *      ring > 8        parallax step an exact no-op. Nothing samples out
 *                      there, so the plane is harmless.
 *
 *  THE RING METRIC IS CHEBYSHEV, NOT EUCLIDEAN, and that is a divergence from
 *  the simulation this was chosen on: the dilation below grows by 8-connected
 *  passes, so "ring 8" is a square of radius 8, not a disc. The square's
 *  corners reach 11.3 texels. The simulation measured a Euclidean disc of 8
 *  (R2d8) AND one of 16 (R2d16) and both beat the card plane on all five
 *  subjects, so the answer does not turn on which of the two metrics is used;
 *  it is named here so nobody reads "8" as the simulation's 8.
"""
once(A1); s = s.replace(A1, B1)

# ------------------------------------------- 2. the candidate table, extended
A2 = """ *      as shipped                                  0.3585
 *      card plane on every non-full texel          0.4563
 *      dilate from fully covered, nothing else     0.4288
 *      THIS ONE (dilate, then card plane outside)  0.4587
 *      card plane outside coverage only            0.4504
"""
B2 = """ *      as shipped                                  0.3585
 *      card plane on every non-full texel          0.4563
 *      dilate from fully covered, nothing else     0.4288
 *      dilate, then card plane OUTSIDE coverage    0.4587
 *      card plane outside coverage only            0.4504
 *
 *  THAT TABLE WAS SCORED ON ONE SUBJECT (blast_n4 against the N=12 set) AND
 *  IT NEVER TESTED A DILATION THAT REACHED OUTSIDE THE COVERAGE FLOOR, which
 *  is why the rule it picked cost the one fat solid subject in the fixture set
 *  2.8 per cent of its silhouette. Lane IMPOSTORFIX2 (2026-09-19) re-scored
 *  six fills on all FIVE subjects, 24 orbit views each, through the real BC3
 *  round trip, with the registration frozen:
 *
 *      fill outside coverage      blast_n4 blast_n8  maple  dead_n4  rock_n4
 *      as baked (frame mean)        0.3599  0.3794  0.3569  0.4187  0.7894
 *      the card plane everywhere    0.4550  0.5943  0.3232  0.5391  0.7596
 *      card plane outside (above)   0.4978  0.6639  0.3609  0.5826  0.7777
 *      DILATE 8, plane beyond       0.5646  0.7200  0.3685  0.6153  0.8331
 *      dilate 16, plane beyond      0.5515  0.7165  0.3690  0.5907  0.8416
 *      dilate to the whole frame    0.5302  0.7163  0.3694  0.5898  0.8380
 *
 *  The 8-ring dilation is better than the card plane on every subject, and on
 *  the rock it also clears the number the regression was measured against
 *  (0.7944 before any of this) by +0.039, so no subject is traded for another.
 *  Dilating to the WHOLE frame is worse than 8 on three of the five: far from
 *  the object the nearest whole texel's height is not that pixel's depth
 *  either, and the plane is the better default there.
"""
once(A2); s = s.replace(A2, B2)

# ------------------------------------------------- 3. the ring array + counts
A3 = """	const int kFull = 250;		// "the object covers this texel whole"
	const int kFloor = 16;		// the spec's coverage floor
	qint64 partial = 0, outside = 0, whole = 0, violations = 0;
"""
B3 = """	const int kFull = 250;		// "the object covers this texel whole"
	const int kFloor = 16;		// the spec's coverage floor
	const int kOutRings = 8;	// how far the object's own height reaches past its silhouette
	qint64 partial = 0, outside = 0, outsideNear = 0, whole = 0, violations = 0;
"""
once(A3); s = s.replace(A3, B3)

A4 = """			std::vector<quint8> have( size_t( frameW ) * frameH, 0 );
			std::vector<quint8> hgt( size_t( frameW ) * frameH, 128 );
"""
B4 = """			std::vector<quint8> have( size_t( frameW ) * frameH, 0 );
			std::vector<quint8> hgt( size_t( frameW ) * frameH, 128 );
			/* ring[t] = how many dilation passes it took to reach t, i.e. the
			 * Chebyshev distance from t to the nearest FULLY covered texel.
			 * 0 on the seeds themselves. Only the outside-coverage branch
			 * below reads it; the partial branch is unchanged. */
			std::vector<quint16> ring( size_t( frameW ) * frameH, 0 );
"""
once(A4); s = s.replace(A4, B4)

A5 = """						if ( k ) {
							hgt[size_t( y ) * frameW + x] = quint8( s / k );
							next[size_t( y ) * frameW + x] = 1;
							grew = true;
						}
"""
B5 = """						if ( k ) {
							hgt[size_t( y ) * frameW + x] = quint8( s / k );
							next[size_t( y ) * frameW + x] = 1;
							ring[size_t( y ) * frameW + x] = quint16( pass + 1 );
							grew = true;
						}
"""
once(A5); s = s.replace(A5, B5)

A6 = """					} else {
						b = 128;		// the card plane: the parallax step is then an exact no-op
						outside++;
					}
"""
B6 = """					} else if ( ring[size_t( y ) * frameW + x]
								&& ring[size_t( y ) * frameW + x] <= kOutRings ) {
						/* just outside the silhouette: a neighbouring frame's ray
						 * lands here, and the object's own depth is what it should
						 * read. Carried out from the whole texels, not invented. */
						b = hgt[size_t( y ) * frameW + x];
						outsideNear++;
					} else {
						b = 128;		// the card plane: the parallax step is then an exact no-op
						outside++;
					}
"""
once(A6); s = s.replace(A6, B6)

# -------------------------------------------------------------- 4. the census
A7 = """		*report = QStringLiteral( "oct height repaired: %1 whole kept, %2 partial filled from the nearest"
			" whole texel, %3 outside set to the card plane; self-check %4" )
			.arg( whole ).arg( partial ).arg( outside )
"""
B7 = """		*report = QStringLiteral( "oct height repaired: %1 whole kept, %2 partial filled from the nearest"
			" whole texel, %3 outside within 8 rings carried the object's height, %4 outside set to the"
			" card plane; self-check %5" )
			.arg( whole ).arg( partial ).arg( outsideNear ).arg( outside )
"""
once(A7); s = s.replace(A7, B7)

out = s.encode('utf-8')
if out.count(b'\r') != cr0:
    print('CR COUNT MOVED %d -> %d' % (cr0, out.count(b'\r'))); sys.exit(2)
open(P, 'wb').write(out)
print('fix01 applied: %d -> %d bytes, CR %d -> %d' % (len(b), len(out), cr0, out.count(b'\r')))
