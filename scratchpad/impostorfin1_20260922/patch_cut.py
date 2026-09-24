#!/usr/bin/env python
"""IMPOSTORFIN1 job 3a -- the impostor draw's DEFAULT coverage cut becomes
vanilla's LOD alpha test, 128/255 (cutoff_census.txt: every alpha-tested shape
in every vanilla .BTO chunk carries NiAlphaProperty flags 0x12EC, threshold
128). bungo 2026-09-22: "what vanilla game had worked pretty well".

    python patch_cut.py            --check (default): counts, writes nothing
    python patch_cut.py --apply    exact-once splice, CR asserted
"""
import sys, io

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'

H_OLD = """	/*! The coverage FRACTION below which a texel is not drawn. Negative means
	 *  ASK THE SET, which is the default and the only answer that is the
	 *  bake's rather than this viewer's.
"""
H_NEW = """	/*! The coverage FRACTION below which a texel is not drawn. Negative means
	 *  THE DEFAULT, which is VANILLA'S LOD ALPHA TEST: `kImpostorVanillaCut`,
	 *  128/255 (lane IMPOSTORFIN1, 2026-09-22, on bungo's word "what vanilla
	 *  game had worked pretty well"). Census of the whole vanilla corpus
	 *  (scratchpad/impostorfin1_20260922/cutoff_census.txt): every
	 *  alpha-tested shape in every Fallout 4 .BTO chunk -- 461 of 980, the rest
	 *  opaque, no other value anywhere -- carries NiAlphaProperty flags 0x12EC
	 *  (test ON, GREATER, NO blend) at threshold 128. The per-model tree LOD
	 *  sources vary (105, 80, 82, 127, 90, 100, 65, 110, 45 beside 128); the
	 *  drawn chunks do not. Vanilla's alpha is the leaf texture's coverage, so
	 *  its 128 is a cut on the coverage FRACTION at one half, which is the
	 *  domain this number is in.
	 *
	 *  The text below is the older rule this default replaced, kept because
	 *  its consequence still holds: the set's extents were measured at
	 *  `floor`, so a card cut at one half draws INSIDE its own `halfW` /
	 *  `halfH` (IMPOSTORFIX4 measured 0.50 below 0.063 on all five subjects).
"""

H2_OLD = """	float alphaThreshold = -1.0f;
"""
H2_NEW = """	float alphaThreshold = -1.0f;
"""

# the constant goes above the struct: anchor on the struct line itself is
# unsafe (ends in `{`), so it is inserted AFTER the include guard's last
# include -- found at run time below.

C_OLD = """	/* The cut, in the DECODED fraction domain the shader now works in. See
	 * `ImpostorDrawOptions::alphaThreshold` for why the set's own number is
	 * `floor` and not `test`. */
	const float thr = ( opt.alphaThreshold >= 0.0f ) ? opt.alphaThreshold
			: ( set.covOk() ? float( set.covFloor ) / 255.0f : 0.5f );
"""
C_NEW = """	/* The cut, in the DECODED fraction domain the shader now works in. The
	 * default is VANILLA'S LOD alpha test, 128/255, for every set -- see
	 * `ImpostorDrawOptions::alphaThreshold` (lane IMPOSTORFIN1). The set's
	 * own `floor` is no longer the default; `WW_IMPOSTOR_ALPHA=0.0627` draws
	 * the old picture. */
	const float thr = ( opt.alphaThreshold >= 0.0f ) ? opt.alphaThreshold
			: kImpostorVanillaCut;
"""

F1_OLD = """uniform float alphaThreshold;      // spec 317..321: 0.5 full crowns, lower bare
"""
F1_NEW = """uniform float alphaThreshold;      // default 128/255, vanilla's LOD alpha test (IMPOSTORFIN1)
"""

F2_OLD = """	/* CLAUSE C28. The spec gives 0.5 for full crowns and says a consumer
	 * "tests lower, or blends, for bare trees" without naming the number --
"""
F2_NEW = """	/* THE DEFAULT IS VANILLA'S (lane IMPOSTORFIN1, 2026-09-22): the viewer
	 * sends 128/255, the threshold on every alpha-tested shape of every
	 * vanilla .BTO chunk (flags 0x12EC: a hard GREATER test, never a blend),
	 * and the colour is then written opaque below -- the same hard cut-out.
	 *
	 * CLAUSE C28. The spec gives 0.5 for full crowns and says a consumer
	 * "tests lower, or blends, for bare trees" without naming the number --
"""

T_OLD = """			: QStringLiteral( "coverage cut: from the set -- %1" )
					.arg( s.set.covOk()
						? QStringLiteral( "floor %1/255 = %2" ).arg( s.set.covFloor )
								.arg( double( s.set.covFloor ) / 255.0, 0, 'f', 4 )
						: QStringLiteral( "no contract declared, so spec 311's 0.5" ) ) );
"""
T_NEW = """			: QStringLiteral( "coverage cut: vanilla's LOD alpha test 128/255 = %1 (the set's floor would be %2)" )
					.arg( double( kImpostorVanillaCut ), 0, 'f', 4 )
					.arg( s.set.covOk()
						? QStringLiteral( "%1/255 = %2" ).arg( s.set.covFloor )
								.arg( double( s.set.covFloor ) / 255.0, 0, 'f', 4 )
						: QStringLiteral( "undeclared" ) ) );
"""

EDITS = [
    ('src/gl/impostordraw.h', 'replace', H_OLD, H_NEW),
    ('src/gl/impostordraw.cpp', 'replace', C_OLD, C_NEW),
    ('res/shaders/impostor_oct.frag', 'replace', F1_OLD, F1_NEW),
    ('res/shaders/impostor_oct.frag', 'replace', F2_OLD, F2_NEW),
    ('src/impostorpreviewtest.cpp', 'replace', T_OLD, T_NEW),
]

CONST = ("\n/*! Vanilla Fallout 4's LOD alpha test as a coverage fraction: NiAlphaProperty\n"
         " *  threshold 128 (flags 0x12EC) on every alpha-tested shape of every vanilla\n"
         " *  .BTO chunk. The impostor draw's default cut (lane IMPOSTORFIN1). */\n"
         "constexpr float kImpostorVanillaCut = 128.0f / 255.0f;\n")


def main():
    apply = '--apply' in sys.argv
    files = {}
    ok = True
    for f, how, old, new in EDITS:
        if f not in files:
            files[f] = io.open(ROOT + f, 'r', encoding='utf-8', newline='').read()
    for f, t in files.items():
        print('%-34s %7d chars, CR %d' % (f, len(t), t.count('\r')))
        if t.count('\r'):
            print('  REFUSE: CR in an LF-only file'); ok = False
    for f, how, old, new in EDITS:
        n = files[f].count(old)
        print('  %-32s %-8s %d match' % (f, how, n))
        if n != 1:
            ok = False
    # the constant: after the LAST #include of impostordraw.h
    h = files['src/gl/impostordraw.h']
    lines = h.split('\n')
    inc = [i for i, l in enumerate(lines) if l.startswith('#include')]
    print('  impostordraw.h last #include at line %d: %r' % (inc[-1] + 1, lines[inc[-1]]))
    print('  kImpostorVanillaCut already present: %d' % h.count('kImpostorVanillaCut'))
    if h.count('kImpostorVanillaCut'):
        ok = False
    if not ok:
        print('REFUSED: an anchor does not match once (or already applied). Nothing written.')
        return 1
    if not apply:
        print('OK: every anchor matches once. Nothing written (--check).')
        return 0
    for f, how, old, new in EDITS:
        t = files[f]
        assert t.count(old) == 1
        files[f] = t.replace(old, new)
    lines = files['src/gl/impostordraw.h'].split('\n')
    inc = [i for i, l in enumerate(lines) if l.startswith('#include')]
    lines.insert(inc[-1] + 1, CONST.rstrip('\n'))
    files['src/gl/impostordraw.h'] = '\n'.join(lines)
    for f, t in files.items():
        assert '\r' not in t
        io.open(ROOT + f, 'w', encoding='utf-8', newline='').write(t)
        print('WROTE %s (%d chars)' % (f, len(t)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
