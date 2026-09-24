# IMPOSTORTEAR1 -- the soft cut WITHOUT the synchronized flip, shader-only, zero extra fetches.
#   python dither_shader.py <in.frag (run_fix's)> <out.frag> <K>
# WHY: soft2 (cut = max_k cov_k * min(1, 2 w_k)) passed azimuth popping and the tear
# clause but failed elevation popping at 1.72 / 1.52, and the profile says why: every
# pixel covered by ONE frame only carries the same factor 2 w_k, so they all cross the
# threshold in the same degree (el 35 -> 36 on blast: w 0.26 -> 0.24, card area
# 34206 -> 10148). The shipped mean cut has the same flip at w_dom = 0.5 (el 19 -> 20).
# A hard threshold on a weight-scaled coverage is a mass pop however continuous it is.
# THE REPAIR: a frame's own silhouette counts where a per-texel hash n < min(1, K w_k)
# (a stipple whose density follows the weight), so a weight change of dw flips K dw of
# the pixels only that frame covers, not all of them. The hash lives on the CARD'S
# texel grid (cardQuadUv x the frame's texel size), so it is fixed to the object and
# does not swim with the screen. The 3-frame mean is kept as the floor: never less
# covered than the shipped drawer.
import sys

src, dst, K = sys.argv[1], sys.argv[2], float(sys.argv[3])
s = open(src, 'rb').read().decode('utf-8')


def once(s, a, n):
    c = s.count(a)
    if c != 1:
        sys.exit('REFUSED: anchor %r matches %d times' % (a[:50], c))
    return s.replace(a, n)


s = once(s, '''	vec3 ray = normalize( cardViewRay );
''', '''	vec3 ray = normalize( cardViewRay );
	vec2 dTex = vec2( textureSize( ColourSheet, 0 ) ) * frameRect[0].zw;
	vec2 dCell = floor( cardQuadUv * dTex );
	float dn = fract( sin( dot( dCell, vec2( 12.9898, 78.233 ) ) ) * 43758.5453 );
''')
s = once(s, '''		if ( w > cutW ) {
			cutW = w;
			cutCov = cov;
		}
''', '''		if ( cov >= alphaThreshold && dn < min( 1.0, w * %.4f ) )
			cutCov = max( cutCov, cov );
''' % K)
s = once(s, '''	if ( cutOnMean )
		cutCov = colour.a;''', '''	cutCov = max( cutCov, colour.a );
	if ( cutOnMean )
		cutCov = colour.a;''')
open(dst, 'wb').write(s.encode('utf-8'))
print('wrote', dst, 'dither K', K)
