# IMPOSTORTEAR1 -- write the chosen cut into res/shaders/impostor_oct.frag (LF-only file).
#   python final_shader.py <shipped.frag> <out.frag>
import sys

src, dst = sys.argv[1], sys.argv[2]
s = open(src, 'rb').read().decode('utf-8')
assert '\r' not in s


def once(s, a, n):
    c = s.count(a)
    if c != 1:
        sys.exit('REFUSED: anchor %r matches %d times' % (a[:50], c))
    return s.replace(a, n)


s = once(s, '''uniform float alphaThreshold;      // default 128/255, vanilla's LOD alpha test (IMPOSTORFIN1)
''', '''uniform float alphaThreshold;      // default 128/255, vanilla's LOD alpha test (IMPOSTORFIN1)
/* WHICH COVERAGE THE CUT READS (lane IMPOSTORTEAR1, 2026-09-23). 0 -- GL's
 * default, so also what an exe that never sets it gets -- is the STIPPLED
 * cut of section 1b; 1 is the way back, the 3-frame weighted mean the cut
 * read before; 2 is the strongest frame alone (the rule the brief named first,
 * kept ONLY as impostor_draw.sh row 18's red control: it pops). */
uniform int   cutRule;
''')

s = once(s, '''	float depthOffset = 0.0;
''', '''	float depthOffset = 0.0;
	float cutCov   = 0.0;              // what the cut reads (1b)
	float cutW     = -1.0;
''')

s = once(s, '''	vec3 ray = normalize( cardViewRay );
''', '''	vec3 ray = normalize( cardViewRay );

	/* The stipple's noise (1b): one value per texel of the CARD, not of the
	 * screen, so it is fixed to the object and does not swim when the camera
	 * moves. Frame 0's texel size stands for all three (every frame of a set
	 * has the same size). */
	vec2  stipTexels = vec2( textureSize( ColourSheet, 0 ) ) * frameRect[0].zw;
	vec2  stipCell   = floor( cardQuadUv * stipTexels );
	float stipNoise  = fract( sin( dot( stipCell, vec2( 12.9898, 78.233 ) ) ) * 43758.5453 );
''')

s = once(s, '''		float cov = coverageOf( c.a );
''', '''		float cov = coverageOf( c.a );

		/* 1b. THE TEAR (lane IMPOSTORTEAR1). bungo: "like somebody ripped out
		 * a piece of paper". The three frames do not register -- one parallax
		 * step cannot move a thin trunk by more than its own width, and N=4
		 * blends frames up to 63 degrees apart -- so where ONE frame is solid
		 * the MEAN of the three falls under the cut and the card tears.
		 *
		 * THE RULE: a frame's own silhouette counts at a pixel whose card-texel
		 * noise is below min(1, 2 w) -- in full once its weight reaches 0.5,
		 * as a stipple of density 2w below that -- and the 3-frame mean is kept
		 * as the floor, so the card is never less covered than it was.
		 *
		 * WHY A STIPPLE, measured by the 1-degree sweeps of
		 * scratchpad/impostortear1_20260923 (bar: worst per-step change in
		 * covered pixels <= 1.5 x the shipped drawer's, azimuth and elevation,
		 * blast / maple / rock; tear clause: IoU vs the mesh at the torn view
		 * beats the shipped drawer and the torn share falls to <= 0.6 x):
		 *   strongest frame only      tear repaired, pops 1.8 .. 2.6 x  FAIL
		 *   depth march 4..128 taps   pops OK from 16 taps, the rock's tear
		 *                             never repaired (27.7 % at 128)     FAIL
		 *   max_k cov * min(1, 2w)    azimuth OK, elevation 1.72 x       FAIL
		 *   this stipple              pops 0.31 / 1.04 / 0.31 (az),
		 *                             0.49 / 1.26 / 0.08 (el); torn share
		 *                             44.9 / 68.3 / 27.3 % -> 5.9 / 19.0 / 5.4 %
		 * A hard threshold on ANY weight-scaled coverage flips every pixel only
		 * one frame covers in the same degree, because they all share that
		 * frame's weight -- the shipped mean did it at w = 0.5, which is where
		 * its own worst steps were. The noise spreads that one flip over the
		 * whole weight range. The price is a steady small change instead of a
		 * rare large one (mean step blast 3841 -> 5703 px) and a visible
		 * stipple where only a weak frame covers. */
		if ( cutRule == 0 ) {
			if ( cov >= alphaThreshold && stipNoise < min( 1.0, 2.0 * w ) )
				cutCov = max( cutCov, cov );
		} else if ( cutRule == 2 && w > cutW ) {
			cutW = w;
			cutCov = cov;
		}
''')

s = once(s, '''	if ( colour.a < alphaThreshold )
		discard;''', '''	if ( cutRule == 0 )
		cutCov = max( cutCov, colour.a );
	else if ( cutRule == 1 )
		cutCov = colour.a;
	if ( cutCov < alphaThreshold )
		discard;''')

s = once(s, '''		else if ( debugChannel == 2 ) dbg = vec3( colour.a );           // coverage''',
         '''		else if ( debugChannel == 2 ) dbg = vec3( cutCov );             // coverage the cut read (1b)''')
open(dst, 'wb').write(s.encode('utf-8'))
print('wrote', dst)
