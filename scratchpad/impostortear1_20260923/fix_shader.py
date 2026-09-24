# IMPOSTORTEAR1 -- fix (2) of IMPOSTORAA1's ranked list: the alpha cut is
# decided on the STRONGEST contributing frame's coverage (the frame with the
# largest blend weight), not on the 3-frame mean. Colour/normal/height still
# blend. Way back: uniform `cutOnMean` true = the old mean cut, byte for byte in
# behaviour (an exe that never sets it gets the NEW rule, GL's default 0).
#
#   python fix_shader.py <in.frag> <out.frag>
import sys

src, dst = sys.argv[1], sys.argv[2]
b = open(src, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')


def once(s, a, n):
    c = s.count(a)
    if c != 1:
        sys.exit('REFUSED: anchor %r matches %d times' % (a[:60], c))
    return s.replace(a, n)


s = once(s, '''uniform float alphaThreshold;      // default 128/255, vanilla's LOD alpha test (IMPOSTORFIN1)
''', '''uniform float alphaThreshold;      // default 128/255, vanilla's LOD alpha test (IMPOSTORFIN1)
/* THE CUT'S COVERAGE (lane IMPOSTORTEAR1, 2026-09-23). false -- GL's default,
 * so also what an exe that never sets it gets -- cuts on the coverage of the
 * STRONGEST contributing frame (the largest blend weight); true is the way
 * back, the 3-frame weighted mean the cut used before. See section 1b. */
uniform bool  cutOnMean;
''')

s = once(s, '''	float wsum     = 0.0;
	float depthOffset = 0.0;
''', '''	float wsum     = 0.0;
	float depthOffset = 0.0;
	float cutCov   = 0.0;              // the strongest frame's coverage (1b)
	float cutW     = -1.0;
''')

s = once(s, '''		float wc = w * cov;
''', '''		float wc = w * cov;

		/* 1b. THE STRONGEST FRAME DECIDES THE CUT (lane IMPOSTORTEAR1). bungo:
		 * "like somebody ripped out a piece of paper". Measured (IMPOSTORAA1,
		 * numpy reference card, torn views el 20): the three frames do not
		 * register -- one-step parallax cannot move a thin trunk by more than
		 * its own width, and N=4 blends a 0 deg rim frame with a 63.4 deg top
		 * frame -- so the MEAN of their coverages falls under the cut where
		 * any single frame is solid. Torn share 72 / 81 / 36 % (blast / maple
		 * / rock), best single frame in the torn pixels 0.97 / 0.88 / 1.00,
		 * the blend 0.34 / 0.32 / 0.40. The cut now reads the coverage of the
		 * frame with the largest weight (ties: the lower k); colour, normal,
		 * height and the material sheets still blend over all three.
		 * Reference-card IoU vs mesh at the torn views, shipped -> this rule:
		 * 0.462 -> 0.705, 0.170 -> 0.340, 0.646 -> 0.778. The price is that
		 * the silhouette follows ONE frame and switches when the weights
		 * cross; the popping row of tests/spells/impostor_draw.sh measures it. */
		if ( w > cutW ) {
			cutW = w;
			cutCov = cov;
		}
''')

s = once(s, '''	if ( colour.a < alphaThreshold )
		discard;
''', '''	if ( cutOnMean )
		cutCov = colour.a;
	if ( cutCov < alphaThreshold )
		discard;
''')

s = once(s, '''		else if ( debugChannel == 2 ) dbg = vec3( colour.a );           // coverage
''', '''		else if ( debugChannel == 2 ) dbg = vec3( cutCov );             // coverage the cut read (1b)
''')

out = s.encode('utf-8')
assert out.count(b'\r') == cr0, 'CR count moved'
open(dst, 'wb').write(out)
print('wrote', dst, 'CR', cr0)
