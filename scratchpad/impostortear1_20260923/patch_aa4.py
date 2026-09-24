# IMPOSTORTEAR1 job 2 -- the offscreen card bake's supersample 2x -> 4x.
# The factor becomes `aaK` (default 4); WW_IMPOSTOR_AA=K picks K in 1..8
# (2 = IMPOSTORAA1's bake exactly), WW_IMPOSTOR_AA=0 is still the window
# photograph. The sidecar's `aa` line carries K.
#   python patch_aa4.py            (edits src/nifskope_ui.cpp in place)
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/src/nifskope_ui.cpp'
b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')


def once(s, a, n):
    c = s.count(a)
    if c != 1:
        sys.exit('REFUSED: anchor %r matches %d times' % (a[:70], c))
    return s.replace(a, n)


s = once(s, '''const bool aaOn = !( qEnvironmentVariableIsSet( "WW_IMPOSTOR_AA" )
							&& qEnvironmentVariableIntValue( "WW_IMPOSTOR_AA" ) == 0 );
''', '''/* THE FACTOR (lane IMPOSTORTEAR1, 2026-09-23): 4 x 4 samples a texel,
						 * raised from IMPOSTORAA1's 2 x 2. Measured on the 512-unit cube
						 * (tests/spells/lodgen_octahedral.sh F1, N 8, tile 64): the 2x
						 * bake reproduces its own ideal 2x2 estimator exactly and that
						 * estimator reads 1.27-1.32 texels against F1's bar of 1.0 -- the
						 * DESIGN could not pass, whatever the code did -- while 3x3 and
						 * 4x4 point sampling read 0.68, the area truth's own number. The
						 * ruled rules are unchanged: offscreen, MSAA off, coverage-
						 * weighted, window-size independent. WW_IMPOSTOR_AA=K picks K
						 * (1..8; 2 is IMPOSTORAA1's bake exactly), =0 the window
						 * photograph. The fill is K^2 per texel: 4x the 2x arm's. */
						const int aaK = qEnvironmentVariableIsSet( "WW_IMPOSTOR_AA" )
							? qBound( 0, qEnvironmentVariableIntValue( "WW_IMPOSTOR_AA" ), 8 ) : 4;
						const bool aaOn = aaK > 0;
''')

s = once(s, '''									/* THE 2x ARM: the frame's inner rect, exactly, at 2 x iw by
									 * 2 x ih samples -- halfW / halfH == iw / ih, so the samples
									 * are square -- centred on this view's silhouette, then 2:1. */
									const int RW = 2 * iw, RH = 2 * ih;
''', '''									/* THE OFFSCREEN ARM: the frame's inner rect, exactly, at aaK x iw
									 * by aaK x ih samples -- halfW / halfH == iw / ih, so the samples
									 * are square -- centred on this view's silhouette, then aaK:1
									 * (a K x K box; K = 4 since IMPOSTORTEAR1, 2 before). */
									const int RW = aaK * iw, RH = aaK * ih, KK = aaK * aaK;
''')

s = once(s, '''											for ( int q = 0; q < 4; q++ ) {
												const int sx = 2 * x + ( q & 1 ), sy = 2 * y + ( q >> 1 );
''', '''											for ( int q = 0; q < KK; q++ ) {
												const int sx = aaK * x + q % aaK, sy = aaK * y + q / aaK;
''')

s = once(s, '''											const int cov = ( sa + 2 ) / 4;
''', '''											const int cov = ( sa + KK / 2 ) / KK;
''')

s = once(s, '''aaCentreErr = qMax( aaCentreErr, 0.5f * qMax( ex, ey ) );	// samples -> texels
''', '''aaCentreErr = qMax( aaCentreErr, qMax( ex, ey ) / float( aaK ) );	// samples -> texels
''')

s = once(s, '''						 * <worst centre miss in texels>` for the 2x offscreen bake, `aa 0
''', '''						 * <worst centre miss in texels>` for the 2x offscreen bake (`aa 4 ...`
						 * since IMPOSTORTEAR1: the first number is the factor K), `aa 0
''')

s = once(s, '''ms << "aa 2 " << p1Size << " " << aaCentreErr << "\\n";
''', '''ms << "aa " << aaK << " " << p1Size << " " << aaCentreErr << "\\n";
''')

out = s.encode('utf-8')
assert out.count(b'\r') == cr0, 'CR count moved %d -> %d' % (cr0, out.count(b'\r'))
open(P, 'wb').write(out)
print('patched', P, 'CR', cr0)
