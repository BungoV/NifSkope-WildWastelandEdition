#!/usr/bin/env python
"""IMPOSTORLOOK1 -- the ONE source change this lane proposes, written as a
REFUSING anchored script and NOT APPLIED. Skill `ww-anchored-hookup`.

WHAT IT REPAIRS, and it is not a matter of taste.

`res/shaders/impostor_oct.frag` and `res/shaders/fo4_default.{vert,frag}` draw
the same objects into the same framebuffer through two different colour
pipelines, and the divergence is three lines long:

    quantity        FO4 mesh path                        impostor card path
    ambient         sqrt(lightSourceAmbient.rgb)*0.375   lightSourceAmbient.rgb
    diffuse         sqrt(lightSourceDiffuse[0].rgb)      lightSourceDiffuse[i].rgb
    output          tonemap( colour )                    colour, written raw

(fo4_default.vert:61 and :63 are where the two square roots are taken; the
tonemap is fo4_default.frag:238 and is applied at :538. The impostor's whole
output is its last line.) Every other shader in the tree ends in a tonemap --
f76_default, f76_effectshader, sk_effectshader, skybox, stf_default. The
impostor is the only one that does not, so a card and the mesh it stands in for
cannot agree even when the sheet is perfect.

This script gives the card the mesh path's three terms. It does NOT touch the
alpha cut-off, the `_n` channel order or the frame size -- those are owed
rulings and this lane measured their share instead of moving them.

IT IS NOT A CLAIM THAT THIS ALONE MAKES THE CARDS MATCH. This lane is offline
and cannot build, so nobody has drawn a single pixel with it. The
DISCRIMINATOR, for whoever holds the build slot: re-run
`scratchpad/impostorlook1_20260919/look.py` and compare the TRANSFER table. The
card's response currently spans 0.130..0.178 of linear luma where the mesh
spans 0.02..0.78 (blast_n4, 190,708 texels). If the card's span does not widen,
the colour pipeline was not the cause of the flatness and the remaining
suspects are the missing specular lobe and one sheet texel per screen pixel --
both of which are bigger than a shader line.

    python hookup_cardlight.py --check     reads, writes nothing, prints anchors
    python hookup_cardlight.py --apply     exact-once splice, byte-asserted
"""
import sys, os, hashlib

F = 'E:/Projects/NifskopeWildWastelandEdition/res/shaders/impostor_oct.frag'

# Each anchor must occur EXACTLY ONCE. The file is LF-only (asserted below), so
# the anchors carry '\n' and an --apply that found a CR would refuse.
A1_OLD = """	vec3 lit = vec3( 0.0 );
	for ( int i = 0; i < 3; i++ ) {
		vec3 L = lightSourcePosition[i].xyz;
		if ( dot( L, L ) < 1e-8 )
			continue;
"""
A1_NEW = """	/* THE LIGHT TERMS ARE THE MESH PATH'S, NOT THE RAW UNIFORMS (lane
	 * IMPOSTORLOOK1, 2026-09-19). `fo4_default.vert` hands its fragment stage
	 * `A = vec4( sqrt(lightSourceAmbient.rgb) * 0.375, toneMapScale )` and
	 * `D = vec4( sqrt(lightSourceDiffuse[0].rgb), brightnessScale )` -- the
	 * square roots are the FO4 path's gamma convention and the 0.375 is its
	 * ambient scale. This shader used the uniforms raw, so a card and the mesh
	 * it replaces were lit by two different lights in one framebuffer. */
	vec3 lit = vec3( 0.0 );
	for ( int i = 0; i < 3; i++ ) {
		vec3 L = lightSourcePosition[i].xyz;
		if ( dot( L, L ) < 1e-8 )
			continue;
"""

A2_OLD = """		lit += lightSourceDiffuse[i].rgb * ndl;
"""
A2_NEW = """		lit += sqrt( max( lightSourceDiffuse[i].rgb, vec3( 0.0 ) ) ) * ndl;
"""

A3_OLD = """	vec3 ambient = lightSourceAmbient.rgb * ao;

	fragColor = vec4( colour.rgb * ( lit + ambient ) * brightnessScale, 1.0 );
}
"""
A3_NEW = """	vec3 ambient = sqrt( max( lightSourceAmbient.rgb, vec3( 0.0 ) ) ) * 0.375 * ao;

	/* AND THE TONEMAP, which this shader was the only one in the tree without.
	 * Verbatim `fo4_default.frag`'s curve, with `A.a` = toneMapScale and
	 * `D.a` = brightnessScale, which is what its vertex stage packs there. */
	vec3 lc = colour.rgb * ( lit + ambient );
	{
		const float a = 0.15, b = 0.50, c = 0.10, d = 0.20, e = 0.02, f = 0.30;
		vec3 z = lc * lc * brightnessScale * ( toneMapScale * 4.22978723 );
		z = ( z * ( a * z + b * c ) + d * e ) / ( z * ( a * z + b ) + d * f ) - e / f;
		lc = sqrt( z / max( toneMapScale * 0.93333333, 1e-6 ) );
	}
	fragColor = vec4( lc, 1.0 );
}
"""

PAIRS = [('the light loop head', A1_OLD, A1_NEW),
         ('the diffuse term', A2_OLD, A2_NEW),
         ('the ambient term and the output', A3_OLD, A3_NEW)]

def main(argv):
    mode = argv[1] if len(argv) > 1 else '--check'
    if mode not in ('--check', '--apply'):
        print('usage: hookup_cardlight.py [--check|--apply]'); return 2
    raw = open(F, 'rb').read()
    cr, lf = raw.count(b'\r'), raw.count(b'\n')
    print('%s\n  %d bytes, CR %d, LF %d, sha1 %s' % (F, len(raw), cr, lf, hashlib.sha1(raw).hexdigest()))
    if cr != 0:
        print('REFUSED: this file was LF-only when the anchors were written and now holds %d CR' % cr)
        return 1
    txt = raw.decode('utf-8')
    bad = 0
    for name, old, new in PAIRS:
        n = txt.count(old)
        print('  anchor %-36s %d match%s' % (name, n, '' if n == 1 else 'es'))
        if n != 1: bad += 1
        if txt.count(new.strip().split('\n')[0]) and new not in txt:
            pass
    if bad:
        print('REFUSED: %d anchor(s) are not exact-once -- the file moved under this script' % bad)
        return 1
    if mode == '--check':
        print('OK: 3 anchors, 1 match each. Nothing was written.')
        return 0
    for name, old, new in PAIRS:
        txt = txt.replace(old, new, 1)
    out = txt.encode('utf-8')
    if out.count(b'\r') != 0:
        print('REFUSED: the splice introduced %d CR' % out.count(b'\r')); return 1
    open(F, 'wb').write(out)
    print('APPLIED: %d -> %d bytes, CR 0 -> 0, LF %d -> %d' % (len(raw), len(out), lf, out.count(b'\n')))
    print('NOT VERIFIED: nothing has been built or drawn. Run the discriminator in the docstring.')
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
