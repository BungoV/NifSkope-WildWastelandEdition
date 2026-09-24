# IMPOSTORTEAR1 -- a CONTINUOUS version of fix (2), shader-only, zero extra fetches.
#   python soft_shader.py <in.frag (run_fix's)> <out.frag> <K>
# The cut reads max over the contributing frames of cov_k * min(1, K * w_k).
# Fix (2) (cut = the coverage of the frame with the largest weight) jumps when
# two weights cross; this does not: at a crossing both frames carry the same
# factor, and a frame entering the triangle at w = 0 contributes nothing.
# K = 2: every frame with weight >= 0.5 counts in full, so at an edge crossing
# the silhouette is the UNION of the two frames, never one then the other.
import sys

src, dst, K = sys.argv[1], sys.argv[2], float(sys.argv[3])
s = open(src, 'rb').read().decode('utf-8')
a = '''		if ( w > cutW ) {
			cutW = w;
			cutCov = cov;
		}
'''
if s.count(a) != 1:
    sys.exit('REFUSED: anchor matches %d times' % s.count(a))
s = s.replace(a, '''		cutCov = max( cutCov, cov * min( 1.0, w * %.4f ) );
''' % K)
open(dst, 'wb').write(s.encode('utf-8'))
print('wrote', dst, 'soft K', K)
