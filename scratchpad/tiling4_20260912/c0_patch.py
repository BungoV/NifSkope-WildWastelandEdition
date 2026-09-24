"""TILING4 step 3 -- write the picked sampler into the tree, by REFUSING anchors.

Every insertion below is an exact-once (or exact-twice, asserted) anchor carried
with the file's own line ending, and --check writes nothing.  The reason it is a
script and not four hand edits is ww-anchored-hookup: two of the four edits have
to be made at BOTH sampling sites and TILING2 lost a relink by editing one and
not the other (src/lodgen.cpp's own comment at line 6388 says so).

    python c0_patch.py --check      # counts every anchor, writes nothing
    python c0_patch.py              # applies, after the same counts

What it lands:
  1. src/lodgen.cpp   g_landHexSize, lodgenLandHexCell, lodgenLandHexOffset,
                      lodgenLandHexTap, and the size getter/setter.
  2. src/lodgen.cpp   both sampling sites: the single tap becomes one call to
                      lodgenLandHexTap, which OFF returns the identical
                      expression (`tex->getPixelT( u, v, mip )`).
  3. src/lodgen.h     the declarations and what they were measured at.
  4. src/nifcli.cpp   `--land-sample stochastic` = the hex tiling (256, bias
                      -0.22), `--land-sample warp` = TILING3's warp for the
                      record, `--land-hex <units>` individually, and the usage
                      block.
"""
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                    '..', '..'))

HEX_BLOCK = r'''/* --- THE HISTOGRAM-PRESERVING HEX TILING (lane TILING4) ---------------------
 *
 * See lodgen.h for what this is and what it was measured at.  OFF IS A RETURN:
 * at size 0 `lodgenLandHexTap` evaluates the identical expression the rung
 * compiled, so the default sheet is the same bytes.
 *
 * Heitz & Neyret 2018.  The plane is covered by a triangle lattice of `size`
 * world units; each lattice VERTEX carries one random offset into the texture;
 * a position takes the three offsets of the triangle it falls in and blends
 * them with its barycentric weights, variance-preserved:
 *
 *     result = mean + sum_k w_k (s_k - mean) / sqrt( sum_k w_k^2 )
 *
 * Without that denominator, blending three decorrelated samples of the same
 * texture with weights that sum to one drops the contrast by up to sqrt(1/3) --
 * a soft mottling exactly where the operator is needed.  The offline sweep
 * measured it: the no-varnorm variant reads the swirl instrument HIGHER than
 * the variance-preserved one (2.24 against 1.93 on the worst sheet).
 *
 * WHY THIS RATHER THAN TILING3'S WARP.  A smooth warp removes the repeat only
 * by straining the texture, and the strain IS the swirl bungo saw.  A
 * piecewise-constant offset has a strain of exactly zero away from the lattice
 * edges, so it cannot have that defect by construction -- measured, the swirl
 * gate goes from 2 of 7 sheets (the warp) to 7 of 7 and 7 of 7 (this).
 *
 * THREE PROPERTIES BY CONSTRUCTION, NOT BY TESTING, the same three the warp
 * has: it is a pure function of world position, so there is no seam at any
 * quadrant, cell or chunk line; it reads nothing per-chunk and no evaluation
 * order, so the bake is byte-identical at 1 chunk thread and at 16; and it is a
 * RESAMPLING of the texture, so the grain's spectrum and histogram survive. */
static float g_landHexSize = 0.0f;            // world units; 0 == off

/* the skew and scale that turn a unit square lattice into an equilateral
 * triangle one -- 1/sqrt(3) and 2/sqrt(3), spelled out to the same 17 digits as
 * the offline prototype (scratchpad/tiling4_20260912/h_cand.py) */
static const double LODGEN_HEX_SKEW  = 0.57735026918962576;
static const double LODGEN_HEX_SCALE = 1.15470053837925152;

/*! The triangle a world position falls in: its three lattice vertices and the
 *  three barycentric weights.
 *
 *  In double for the same reason the warp is: the lattice index comes from a
 *  floor of a world coordinate that reaches +-2,000,000 units and float carries
 *  24 bits of mantissa, so the weights would quantise at the far edge of the
 *  worldspace.  Nothing here reads any state. */
static void lodgenLandHexCell( double wx, double wy, double size,
                               qint32 * vi, qint32 * vj, double * w )
{
	const double px = wx / size;
	const double py = wy / size;
	const double sx = px - LODGEN_HEX_SKEW * py;
	const double sy = LODGEN_HEX_SCALE * py;
	const double bi = std::floor( sx );
	const double bj = std::floor( sy );
	const double tx = sx - bi;
	const double ty = sy - bj;
	const double tz = 1.0 - tx - ty;
	/* tz > 0 is the "up" triangle of the rhombus; the other half is its mirror,
	 * and the weights below are the prototype's `tri_grid` term for term. */
	const bool up = tz > 0.0;
	const double o = up ? 0.0 : 1.0;
	w[0] = up ? tz : -tz;
	w[1] = up ? ty : 1.0 - ty;
	w[2] = up ? tx : 1.0 - tx;
	vi[0] = qint32( bi + o );        vj[0] = qint32( bj + o );
	vi[1] = qint32( bi + o );        vj[1] = qint32( bj + 1.0 - o );
	vi[2] = qint32( bi + 1.0 - o );  vj[2] = qint32( bj + o );
}

/*! One lattice vertex's offset into the texture, in [0,1) of one repeat.
 *  The SAME hash the warp is built on, so there is one hash in this file. */
static inline double lodgenLandHexOffset( qint32 i, qint32 j, quint32 k )
{
	return double( lodgenWarpHash( i, j, k ) ) / 4294967296.0;
}

/*! The land diffuse tap: one texel off it when the hex tiling is off, the
 *  three-tap variance-preserving blend when it is on.
 *
 *  `swx`/`swy` are the (possibly warp-offset) world position; `u`/`v` are the
 *  wrapped coordinates the caller already computed from them, so that OFF costs
 *  one branch and returns the caller's own expression unchanged. */
static FloatVector4 lodgenLandHexTap( const DDSTexture16 * tex,
                                      float swx, float swy, float tile,
                                      float u, float v, float mip, float maxMip )
{
	if ( g_landHexSize <= 0.0f )
		return tex->getPixelT( u, v, mip );
	qint32 vi[3], vj[3];
	double w[3];
	lodgenLandHexCell( double( swx ), double( swy ), double( g_landHexSize ),
	                   vi, vj, w );
	/* the texture's own mean over one whole repeat -- its 1x1 mip, the same
	 * value the `average` path reads */
	const FloatVector4 mean = tex->getPixelT( 0.5f, 0.5f, maxMip );
	FloatVector4 acc( 0.0f, 0.0f, 0.0f, 0.0f );
	double wsq = 0.0;
	float bestW = -1.0f;
	float bestA = mean[3];
	for ( int k = 0; k < 3; k++ ) {
		const double ox = lodgenLandHexOffset( vi[k], vj[k], 0 ) * double( tile );
		const double oy = lodgenLandHexOffset( vi[k], vj[k], 1 ) * double( tile );
		/* wrap by hand exactly as the caller does: getPixelT clamps and the
		 * tiling is ours */
		double tu = std::fmod( ( double( swx ) + ox ) / double( tile ), 1.0 );
		double tv = std::fmod( ( double( swy ) + oy ) / double( tile ), 1.0 );
		if ( tu < 0.0 ) tu += 1.0;
		if ( tv < 0.0 ) tv += 1.0;
		const FloatVector4 s = tex->getPixelT( float( tu ), float( tv ), mip );
		acc += ( s - mean ) * float( w[k] );
		wsq += w[k] * w[k];
		if ( float( w[k] ) > bestW ) {
			bestW = float( w[k] );
			bestA = s[3];
		}
	}
	if ( wsq > 1e-12 )
		acc *= float( 1.0 / std::sqrt( wsq ) );
	FloatVector4 r = mean + acc;
	/* ALPHA IS NEVER BLENDED.  Weights that sum to one over a variance-
	 * preserving denominator would push a constant 1.0 alpha to about 1.07, and
	 * a land diffuse's alpha is not a colour to be decorrelated -- it is taken
	 * from the tap with the largest weight, which is a tap, not an average. */
	r[3] = bestA;
	return r;
}

float lodgenLandHexSize()
{
	return g_landHexSize;
}

void lodgenSetLandHexSize( float units )
{
	/* Clamped at zero, not refused, like the warp's amplitude: a negative size
	 * is a typo and the meaningful floor of this control is "off". */
	g_landHexSize = units > 0.0f ? units : 0.0f;
}

'''

HDR_BLOCK = r'''
/* --- THE HISTOGRAM-PRESERVING HEX TILING (lane TILING4) ---------------------
 *
 * bungo on TILING3's warp, over cmp_tiling3.png: "the proposal looks pretty
 * good, but maybe it could use some improvement".  The improvement is the
 * SWIRLS, and they are the warp's strain: a smooth domain warp removes the
 * repeat only by stretching the texture enough to be seen.  Lane TILING4 built
 * an instrument for that stretch (the structure-tensor orientation coherence of
 * the 1-5 texel grain, the land repeat's own frequency family notched out, read
 * against the same chunk's vanilla sheet + 20 %) and it convicts the shipped
 * warp on 5 of 7 sheets and the isolated warp on 7 of 7.
 *
 * This replaces the warp behind the same switch value.  Heitz & Neyret 2018: a
 * triangle lattice of `--land-hex` world units, one random texture offset per
 * lattice VERTEX, the three offsets of a position's triangle blended with its
 * barycentric weights over a variance-preserving denominator.  The offsets are
 * PIECEWISE CONSTANT, so the strain is exactly zero away from a lattice edge
 * and the swirl cannot exist by construction.
 *
 * MEASURED (SPLAT1's independent offline model; 7 selection sheets frozen by
 * TILING3 and 7 VALIDATION sheets frozen before any candidate was scored; lane
 * TILING4).  Tile 256 world units, mip bias -0.22:
 *   * the swirl: 7 of 7 and 7 of 7, against TILING3's warp at 2 of 7;
 *   * the repeat: 6 of 7 and 6 of 7 -- worst sheet 0.350 where that sheet's own
 *     no-repeat control licenses 0.366, and ONE RED PER SET: (-4,-20) 0.308 and
 *     (-12,-20) 0.279 against the 0.264 absolute ceiling, +17 % and +6 % over.
 *     Both pass the ratio half (0.155 and 0.298 against 0.448) with a wide
 *     margin, so what fails on them is the absolute amplitude against vanilla's
 *     WORST of 22, not a repeat visible over that sheet's own energy;
 *   * the grain: no regression on any sheet (within 20 % of the rung's own hp
 *     SD, 7 of 7 and 7 of 7), and the median 19.9 % under vanilla's median on
 *     the selection seven where the rung is 26.7 % under it.
 * A per-sheet grain gate against each chunk's OWN vanilla sheet is not a gate
 * on the sampler and the numbers say so: vanilla's grain over these sheets
 * spans a factor of 5.1 because it is set by what terrain is there, while
 * anything this compositor can produce spans 1.3.  The three finest radial
 * bands are 46 %, 75 % and 97 % short of vanilla ON THE RUNG, whatever the
 * sampler does.
 *
 * THE REPEAT GATE IS THEREFORE 6 OF 7 AND NOT 7, ON BOTH SETS, WHICH IS WHY
 * THIS SHIPS OFF exactly as TILING3's warp did.  `--land-sample stochastic`
 * turns it on; `--land-hex 0` is the exact way back and the default is the
 * 2026-09-11 bake byte for byte.
 *
 * H3, the warp capped at strain 0.5 on top of this, was built and REFUSED by
 * its own numbers: it buys nothing on the repeat (6 of 7 with it, 6 of 7
 * without) and it costs the swirl -- 7 of 7 at strain 0, 6 of 7 at 0.20, 2 of 7
 * at the 0.50 the brief allowed.  Both are still reachable together, and that
 * combination is what they produce. */
float lodgenLandHexSize();
void lodgenSetLandHexSize( float units );        // 0 = off = the rung's bytes
'''

SITE_OLD = ('\t\t\t\t\tconst float mip = qBound( 0.0f, mipRaw, maxMip );\n'
            '\t\t\t\t\tif ( !lodgenLandSampleAverage() )\n'
            '\t\t\t\t\t\treturn tex->getPixelT( u, v, mip );\n')
SITE_NEW = ('\t\t\t\t\tconst float mip = qBound( 0.0f, mipRaw, maxMip );\n'
            '\t\t\t\t\t/* THE HEX TILING (lane TILING4), on the land diffuse\n'
            '\t\t\t\t\t * lookup and on nothing else. ONE call, at BOTH sites:\n'
            '\t\t\t\t\t * off, it evaluates the identical expression this line\n'
            '\t\t\t\t\t * used to hold, so the default is the rung\'s bytes. It\n'
            '\t\t\t\t\t * takes the WARP-offset coordinate, so setting both\n'
            '\t\t\t\t\t * gives warp-then-hex (measured, and refused as a\n'
            '\t\t\t\t\t * default: it costs the swirl and buys no repeat). */\n'
            '\t\t\t\t\tconst FloatVector4 fp =\n'
            '\t\t\t\t\t\tlodgenLandHexTap( tex, swx, swy, TILE, u, v, mip, maxMip );\n'
            '\t\t\t\t\tif ( !lodgenLandSampleAverage() )\n'
            '\t\t\t\t\t\treturn fp;\n')

DET_OLD = '\t\t\t\t\treturn avg + ( tex->getPixelT( u, v, mip ) - avg ) * kDetail;\n'
DET_NEW = '\t\t\t\t\treturn avg + ( fp - avg ) * kDetail;\n'

CLI_OLD = '''		else if ( t == QLatin1String( "--land-sample" ) ) {
			const QString v = next().toLower();
			lodgenSetLandSampleAverage( v == QLatin1String( "average" ) );
			if ( v == QLatin1String( "stochastic" ) ) {
				lodgenSetLandWarpAmp( 683.0f );
				lodgenSetLandWarpLattice( 1024.0f );
				lodgenSetLandWarpOctaves( 1 );
				lodgenSetLandMipBias( -1.0f );
			}
		}
'''
CLI_NEW = '''		else if ( t == QLatin1String( "--land-sample" ) ) {
			const QString v = next().toLower();
			lodgenSetLandSampleAverage( v == QLatin1String( "average" ) );
			/* `stochastic` MEANS THE HEX TILING as of lane TILING4: the warp it
			 * used to mean reads as swirls (the swirl instrument convicts it on
			 * 5 of 7 shipped sheets, and bungo saw it), and the hex tiling is
			 * clean on 7 of 7 and 7 of 7 for the same repeat count. The warp is
			 * still reachable, as `warp`, so the measurement can be repeated.
			 * Each mode turns the OTHER geometry off, so the two words cannot
			 * silently compose into a third thing nobody picked. */
			if ( v == QLatin1String( "stochastic" ) ) {
				lodgenSetLandHexSize( 256.0f );
				lodgenSetLandWarpAmp( 0.0f );
				lodgenSetLandMipBias( -0.22f );
			}
			else if ( v == QLatin1String( "warp" ) ) {
				lodgenSetLandHexSize( 0.0f );
				lodgenSetLandWarpAmp( 683.0f );
				lodgenSetLandWarpLattice( 1024.0f );
				lodgenSetLandWarpOctaves( 1 );
				lodgenSetLandMipBias( -1.0f );
			}
		}
'''

HEX_CLI_OLD = ('\t\telse if ( t == QLatin1String( "--land-mip-bias" ) )'
               ' lodgenSetLandMipBias( next().toFloat() );\n')
HEX_CLI_NEW = ('\t\telse if ( t == QLatin1String( "--land-mip-bias" ) )'
               ' lodgenSetLandMipBias( next().toFloat() );\n'
               '\t\t/* THE HEX TILE SIZE (lane TILING4), individually. 0 is the\n'
               '\t\t * default and is the 2026-09-11 bake byte for byte, so\n'
               '\t\t * `--land-hex 0 --land-mip-bias 0` is the exact way back\n'
               '\t\t * from `--land-sample stochastic`. 256 world units is what\n'
               '\t\t * the lane picked; one land repeat is 341.3333. */\n'
               '\t\telse if ( t == QLatin1String( "--land-hex" ) )'
               ' lodgenSetLandHexSize( next().toFloat() );\n')

USAGE_OLD = '''		  << "  lodgen ... --terrain-region ... [--land-tiling UNITS]\\n"
'''
USAGE_NEW = '''		  << "  lodgen ... --terrain-region ... [--land-sample MODE]\\n"
		  << "                                          how the land texture is read\\n"
		  << "                                          inside one repeat. footprint\\n"
		  << "                                          (DEFAULT) = the shipped bake;\\n"
		  << "                                          average = its mean over a whole\\n"
		  << "                                          repeat; stochastic = the hex\\n"
		  << "                                          tiling (256 units, bias -0.22),\\n"
		  << "                                          which drops the repeat without\\n"
		  << "                                          the warp's swirls; warp = lane\\n"
		  << "                                          TILING3's domain warp, kept for\\n"
		  << "                                          the record\\n"
		  << "  lodgen ... --terrain-region ... [--land-hex UNITS]\\n"
		  << "                                          the hex tile size on its own.\\n"
		  << "                                          DEFAULT 0 = off = the shipped\\n"
		  << "                                          bake byte for byte; --land-hex 0\\n"
		  << "                                          --land-mip-bias 0 is the exact\\n"
		  << "                                          way back from stochastic\\n"
		  << "  lodgen ... --terrain-region ... [--land-tiling UNITS]\\n"
'''

EDITS = [
    ('src/lodgen.cpp', 'float lodgenLandMipBias()\n{\n\treturn g_landMipBias;\n}\n',
     HEX_BLOCK + 'float lodgenLandMipBias()\n{\n\treturn g_landMipBias;\n}\n', 1),
    ('src/lodgen.cpp', SITE_OLD, SITE_NEW, 2),
    ('src/lodgen.cpp', DET_OLD, DET_NEW, 2),
    ('src/lodgen.h',
     'void lodgenSetLandMipBias( float bias );         // 0 = off = the rung\'s bytes\n',
     'void lodgenSetLandMipBias( float bias );         // 0 = off = the rung\'s bytes\n'
     + HDR_BLOCK, 1),
    ('src/nifcli.cpp', CLI_OLD, CLI_NEW, 1),
    ('src/nifcli.cpp', HEX_CLI_OLD, HEX_CLI_NEW, 1),
    ('src/nifcli.cpp', USAGE_OLD, USAGE_NEW, 1),
]


def main():
    check = '--check' in sys.argv
    ok = True
    for rel, old, new, want in EDITS:
        p = os.path.join(ROOT, rel)
        raw = open(p, 'rb').read()
        assert b'\r\n' not in raw, '%s carries CRLF; this patch assumes LF' % rel
        d = raw.decode('utf-8')
        n = d.count(old)
        print('%-16s anchor %-46s found %d, want %d %s'
              % (rel, repr(old[:44]), n, want, 'ok' if n == want else 'REFUSED'))
        if n != want:
            ok = False
    if not ok:
        print('\nREFUSED: an anchor count is wrong; nothing was written.')
        return 1
    if check:
        print('\n--check: every anchor is exact; nothing written.')
        return 0
    for rel, old, new, want in EDITS:
        p = os.path.join(ROOT, rel)
        d = open(p, 'rb').read().decode('utf-8')
        d = d.replace(old, new)
        b = d.encode('utf-8')
        assert b'\r\n' not in b, 'would have written CRLF into %s' % rel
        open(p, 'wb').write(b)
        print('wrote %s' % rel)
    return 0


if __name__ == '__main__':
    sys.exit(main())
