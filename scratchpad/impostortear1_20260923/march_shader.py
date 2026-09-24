# IMPOSTORTEAR1 -- the smallest step toward fix (1): a SHORT depth march per
# frame, shader-only (constants, no uniforms, so no build is needed to A/B it).
#   python march_shader.py <in.frag (run_fix's)> <out.frag> <taps> <tolUnits|'step'> <mean|strong>
# taps T: sample the view ray at T points over [-Rm, +Rm], Rm = 1.1 x max card
# half extent (IMPOSTORAA1's tear_variants.py march, K=320 there); the first tap
# whose frame-k coverage passes the cut and lies behind frame k's own surface by
# 0..tol units is that frame's hit; its uv serves every sheet and its coverage
# is the frame's coverage; no hit = coverage 0. tol 'step' = max(48, tap spacing).
import sys

src, dst, taps, tol, rule = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4], sys.argv[5]
s = open(src, 'rb').read().decode('utf-8')


def once(s, a, n):
    c = s.count(a)
    if c != 1:
        sys.exit('REFUSED: anchor %r matches %d times' % (a[:60], c))
    return s.replace(a, n)


tolx = 'max( 48.0, mStep )' if tol == 'step' else '%s' % float(tol)
s = once(s, '''		/* The sway shear, if the viewer asked for one:''', '''		bool mFound = true;
#if MARCH_TAPS > 0
		if ( useHeightBlend ) {
			float mR = 1.1 * max( cardHalf.x, cardHalf.y );
			float mStep = 2.0 * mR / float( MARCH_TAPS - 1 );
			mFound = false;
			for ( int t = 0; t < MARCH_TAPS; t++ ) {
				vec3 mp = cardPosModel + ray * ( -mR + float( t ) * mStep );
				float md;
				vec2 muv = frameUvOf( k, mp, md );
				float mc = coverageOf( textureLod( ColourSheet, muv, 0.0 ).a );
				float mh = textureLod( NormalSheet, muv, 0.0 ).b;
				float mdz = -( mh - 0.5 ) * cardDepthSpan - md;
				if ( mc >= alphaThreshold && mdz >= 0.0 && mdz <= MARCH_TOL ) {
					uv = muv;
					mFound = true;
					break;
				}
			}
		}
#endif
		/* The sway shear, if the viewer asked for one:''')
s = once(s, '''		float cov = coverageOf( c.a );
''', '''		float cov = coverageOf( c.a );
		if ( !mFound )
			cov = 0.0;
''')
if rule == 'mean':
    s = once(s, '''	if ( cutOnMean )
		cutCov = colour.a;''', '''	if ( true )
		cutCov = colour.a;''')
# the defines go after the #version line
lines = s.split('\n')
vi = [i for i, l in enumerate(lines) if l.startswith('#version')]
assert len(vi) == 1
lines.insert(vi[0] + 1, '#define MARCH_TAPS %d\n#define MARCH_TOL %s' % (taps, tolx))
s = '\n'.join(lines)
open(dst, 'wb').write(s.encode('utf-8'))
print('wrote', dst, 'taps', taps, 'tol', tolx, 'rule', rule)
