import re
p = 'src/lodgen.cpp'; s = open(p, 'rb').read().decode('utf-8')
old = """				const DDSTexture16 * t = lodgenCachedTexture( c, dataRoot, source );
				if ( t ) {
					const FloatVector4 avg = t->getPixelT( 0.5f, 0.5f,
						float( t->getMaxMipLevel() ) );
					if ( avg[3] >= 0.05f ) {
						for ( int k = 0; k < 3; k++ )
							tint[k] = qBound( 0.0f, avg[k] / avg[3], 1.0f );
						ok = true;
					}
				}"""
new = """				const DDSTexture16 * t = lodgenCachedTexture( c, dataRoot, source );
				if ( t ) {
					/* ALPHA-WEIGHTED over a real mip, not the smallest mip divided
					 * by its alpha (lane SEAM1, GRASSCOL). A DDS mip chain is
					 * STRAIGHT alpha: every shipped grass atlas averages its RGB
					 * over the transparent gaps unweighted, so rgb/a of the 1x1
					 * mip is not the leaves' colour -- at mean alpha 0.07..0.30
					 * it clamps to (1,1,1) on 9 of the 9 alpha-cut grasses under
					 * Sanctuary and -24,-8, and the cover tint went WHITE (the
					 * sandy wash bungo saw). sum(rgb*a)/sum(a) at the first mip no
					 * longer than 1024 is within 2.5/255 of mip 0 on the measured
					 * atlases (scratchpad grass_mipcheck.py). The 0.05 floor on the
					 * MEAN alpha is unchanged. */
					int m = 0;
					while ( m < t->getMaxMipLevel()
						&& qMax( t->getWidth() >> m, t->getHeight() >> m ) > 1024 )
						m++;
					const int mw = qMax( 1, t->getWidth() >> m ), mh = qMax( 1, t->getHeight() >> m );
					double sum[4] = { 0.0, 0.0, 0.0, 0.0 };
					for ( int y = 0; y < mh; y++ )
						for ( int x = 0; x < mw; x++ ) {
							const FloatVector4 px = FloatVector4::convertFloat16( t->getPixelN( x, y, m ) );
							const double a = qBound( 0.0, double( px[3] ), 1.0 );
							for ( int k = 0; k < 3; k++ )
								sum[k] += double( px[k] ) * a;
							sum[3] += a;
						}
					if ( sum[3] >= 0.05 * double( mw ) * double( mh ) ) {
						for ( int k = 0; k < 3; k++ )
							tint[k] = qBound( 0.0f, float( sum[k] / sum[3] ), 1.0f );
						ok = true;
					}
				}"""
ded = lambda t: chr(10).join(l[1:] if l.startswith(chr(9)) else l for l in t.split(chr(10)))
old = ded(old); new = ded(new)
assert s.count(old) == 1; s = s.replace(old, new)
open(p, 'wb').write(s.encode('utf-8')); print('patched')
