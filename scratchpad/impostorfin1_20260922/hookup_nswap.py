#!/usr/bin/env python
"""IMPOSTORFIN1 job 4 -- the `_n` HEIGHT <-> SWAY channel swap, PREPARED, NOT
APPLIED. It is NOT RULED (brief 2026-09-22: "PREPARE it only"). Skill
`ww-anchored-hookup`.

    python hookup_nswap.py              --check (default): counts, writes nothing
    python hookup_nswap.py --dry <dir>  runs the apply onto COPIES under <dir>
                                        and prints each copy's deltas
    python hookup_nswap.py --apply      exact-once splice of every edit, or none

WHAT AND WHY (IMPOSTORFIX2 report section 6, measured through the real
encoder): in BC3 the ALPHA channel gets its own BC4 block, while BLUE shares one
RGB565 palette with the normal. Height in alpha is 20-40x more accurate (rock
worst case 1,881 world units -> 99); sway pays 4-8x (mean 1.5 -> 6.9 levels);
silhouette IoU +0.038 on blast_n4 and blast_n8, +0.002..0.005 on the others.

THE DESIGN: SWAP AT THE DDS BOUNDARY, NOWHERE ELSE. Inside the bake the height
stays in BLUE and the sway in ALPHA: the bake's `_oct_normal.png`
(nifskope_ui.cpp ~:23311 writes qRgba(nx, ny, z, sway)), `lodgenDilateFrames`,
`lodgenRepairOctHeight` and the aggregate's source read
(lodgenaggregate.cpp:502, :528) all work on that PNG and are NOT touched. The
three places a `_n.DDS` is ENCODED swap B and A on the way into the encoder:

    per-set sheet     src/lodgen.cpp   `lodgenWriteDds( path, aw, ah, pixels( *s.img ) ...`
    forest aggregate  src/lodgen.cpp   `pixels( normal ), true, set.mips )`
    card array layer  src/lodgen.cpp   `g.n.push_back( pixels( nrm ) );`

so an `_n.DDS` is layout 2 everywhere and a PNG is layout 1 everywhere, and the
nifskope_ui.cpp write IMPOSTORFIX2 named stays as it is (moving it would mean
moving every internal reader of the PNG with it).

THE REFUSAL: every writer of a `_n.DDS` stamps `"nlayout": 2` at the .lodm's
top level (card, cardArray, aggregate), and `impostorCardLoad` refuses a set of
those three kinds whose nlayout is not 2, by name, "rebake". The global
LODM_VERSION is NOT bumped: that would also refuse every SOURCE .lodm, whose
bytes this change does not touch.

THE READERS: impostor_oct.frag (:42 comment, :217 height, :267 sway shear,
:292/:293 the blend); the three gates that read height out of a `_n.DDS`
(impostor_sheet_check.py:87, impostor_draw.sh:887, impostor_height_ref.py:151)
pick the channel from the set's own nlayout, so they gate an old sheet and a
new one alike; spec docs/LODGEN_IMPOSTOR_SPEC.md:45 and the two table pairs in
docs/LODGEN_CARD_SHEETS.md.

NOT COVERED, and owed by whoever applies it: docs/LODGEN_LODM_FORMAT.md gains
the `nlayout` key; the FO4CS runtime's reader of `_n` (FO4CS is built last by
standing order); every sheet already baked is refused until rebaked.
"""
import sys, io, os, shutil

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'

HELPER = """
/*! THE `_n` SHEET'S LAYOUT 2 (lane IMPOSTORFIN1 hook-up, ruled by bungo):
 *  height in ALPHA, sway in BLUE. Inside the bake the height is BLUE and the
 *  sway ALPHA (`_oct_normal.png`); this swaps the two on the way into the
 *  encoder and nowhere else, because BC3's alpha is its own BC4 block and blue
 *  shares the normal's RGB565 palette (IMPOSTORFIX2 section 6: height error
 *  20-40x smaller). A .lodm naming such a sheet says `"nlayout": 2`. */
static std::vector<quint32> lodgenNLayout2( std::vector<quint32> px )
{
	for ( quint32 & p : px )
		p = ( p & 0x00FFFF00u ) | ( ( p & 0x000000FFu ) << 24 ) | ( p >> 24 );
	return px;
}
"""

NL = '\t%sroot.insert( QStringLiteral( "nlayout" ), 2 );\t// `_n`: height in A, sway in B\n'

REFUSE = """	/* `_n` LAYOUT 2 (lane IMPOSTORFIN1 hook-up): height in ALPHA, sway in
	 * BLUE. A set written before it carries the two the other way round, and
	 * drawing it would read the sway as a depth -- refused by name. */
	if ( ( mat.kind == QLatin1String( "card" ) || mat.kind == QLatin1String( "cardArray" )
			|| mat.kind == QLatin1String( "aggregate" ) )
		&& mat.root.value( "nlayout" ).toInt( 1 ) != 2 ) {
		set.error = QString( "%1: _n layout %2, this build reads layout 2 (height in A,"
				" sway in B) -- rebake the set" ).arg( lodmPath )
				.arg( mat.root.value( "nlayout" ).toInt( 1 ) );
		return set;
	}
"""

# (file, mode, key, text, expected count)
#   sub    : replace substring `key` by `text`
#   after  : insert `text` after the whole LINE containing `key`
#   afterI : as `after`, `text` has one %s filled with the line's indentation
EDITS = [
    ('src/lodgen.cpp', 'after', '#include "lodgenao.h"', HELPER, 1),
    ('src/lodgen.cpp', 'sub',
     'ok = lodgenWriteDds( path, aw, ah, pixels( *s.img ), true, auxMips ) && ok;',
     'ok = lodgenWriteDds( path, aw, ah, s.img == &nrmA ? lodgenNLayout2( pixels( *s.img ) )\n'
     '\t\t\t\t\t\t\t: pixels( *s.img ), true, auxMips ) && ok;', 1),
    ('src/lodgen.cpp', 'afterI', 'root.insert( QStringLiteral( "kind" ), QStringLiteral( "card" ) );', NL, 1),
    ('src/lodgen.cpp', 'sub', 'pixels( normal ), true, set.mips )',
     'lodgenNLayout2( pixels( normal ) ), true, set.mips )', 1),
    ('src/lodgen.cpp', 'sub', 'g.n.push_back( pixels( nrm ) );',
     'g.n.push_back( lodgenNLayout2( pixels( nrm ) ) );\t// `_n` layout 2', 1),
    ('src/lodgen.cpp', 'afterI', 'root.insert( QStringLiteral( "kind" ), QStringLiteral( "cardArray" ) );', NL, 1),
    ('src/lodgen.cpp', 'afterI', 'const QByteArray lodm = lodgenAggregateLodm( worldspace, set );',
     '%s// the aggregate .lodm stamps `"nlayout": 2` itself (lodgenaggregate.cpp)\n', 1),
    ('src/lodgenaggregate.cpp', 'afterI',
     'root.insert( QStringLiteral( "kind" ), QStringLiteral( "aggregate" ) );', NL, 1),
    ('src/impostorcard.cpp', 'after', 'set.emissiveScale = mat.emissiveScale;', REFUSE, 1),
    ('res/shaders/impostor_oct.frag', 'sub',
     'uniform sampler2D NormalSheet;     // _n       : R nX, G nY, B height, A sway',
     'uniform sampler2D NormalSheet;     // _n       : R nX, G nY, B sway, A height (layout 2)', 1),
    ('res/shaders/impostor_oct.frag', 'sub',
     'float h = textureLod( NormalSheet, uv, 0.0 ).b;',
     'float h = textureLod( NormalSheet, uv, 0.0 ).a;\t// layout 2: height in A', 1),
    ('res/shaders/impostor_oct.frag', 'sub',
     'float sw = textureLod( NormalSheet, uv, 0.0 ).a;',
     'float sw = textureLod( NormalSheet, uv, 0.0 ).b;\t// layout 2: sway in B', 1),
    ('res/shaders/impostor_oct.frag', 'sub', 'height     += n.b * wc;',
     'height     += n.a * wc;\t// layout 2: height in A', 1),
    ('res/shaders/impostor_oct.frag', 'sub', 'sway       += n.a * wc;',
     'sway       += n.b * wc;\t// layout 2: sway in B', 1),
    ('docs/LODGEN_IMPOSTOR_SPEC.md', 'sub',
     '| `_n` | BC3 | normal X | normal Y | height | sway weight |',
     '| `_n` | BC3 | normal X | normal Y | sway weight | height |', 1),
    ('docs/LODGEN_CARD_SHEETS.md', 'sub', '| normal B (height) |', '| normal A (height) |', 2),
    ('docs/LODGEN_CARD_SHEETS.md', 'sub', '| normal A (sway) |', '| normal B (sway) |', 2),
    ('tests/spells/impostor_sheet_check.py', 'sub', 'h = nsh[..., 2] * 255.0',
     "h = nsh[..., 3 if b'\"nlayout\":2' in raw.replace(b' ', b'') else 2] * 255.0  # _n layout 2 = height in A", 1),
    ('tests/spells/impostor_draw.sh', 'sub', 'h = nsh[..., 2] * 255.0',
     "h = nsh[..., 3 if b'\"nlayout\":2' in open(lodm, 'rb').read().replace(b' ', b'') else 2] * 255.0  # _n layout 2 = height in A", 1),
    ('tests/spells/impostor_height_ref.py', 'sub',
     'got = np.clip(np.rint(np.asarray(arr)[..., 2] * 255.0), 0, 255).astype(np.int32)',
     "got = np.clip(np.rint(np.asarray(arr)[..., 3 if j.get('nlayout', 1) == 2 else 2] * 255.0), 0, 255).astype(np.int32)", 1),
]

MARK = 'lodgenNLayout2'


def splice(text, mode, key, new):
    if mode == 'sub':
        return text.replace(key, new)
    i = text.index(key)
    ls = text.rfind('\n', 0, i) + 1
    le = text.index('\n', i) + 1
    line = text[ls:le]
    ind = line[:len(line) - len(line.lstrip('\t '))]
    ins = new % ind[1:] if mode == 'afterI' and '\t%s' in new else (new % ind if mode == 'afterI' else new)
    return text[:le] + ins + text[le:]


def run(outroot):
    files = {}
    for f, *_ in EDITS:
        if f not in files:
            files[f] = io.open(ROOT + f, 'r', encoding='utf-8', newline='').read()
    ok = True
    for f, t in files.items():
        print('%-40s %7d chars, CR %d' % (f, len(t), t.count('\r')))
    if MARK in files['src/lodgen.cpp']:
        print('ALREADY APPLIED: %s is in src/lodgen.cpp. Nothing written.' % MARK)
        return 1
    for f, mode, key, new, want in EDITS:
        n = files[f].count(key)
        print('  %-38s %-6s %d match (want %d)  %r' % (f, mode, n, want, key[:56]))
        if n != want:
            ok = False
    if not ok:
        print('REFUSED: an anchor does not match its count. Nothing written.')
        return 1
    if outroot is None:
        print('OK: %d edits, every anchor at its count. Nothing written (--check).' % len(EDITS))
        return 0
    before = {f: (t.count('\r'), t.count('{') - t.count('}'), t.count('(') - t.count(')')) for f, t in files.items()}
    for f, mode, key, new, want in EDITS:
        t = files[f]
        assert t.count(key) == want, (f, key)   # counted again AT APPLY TIME
        files[f] = splice(t, mode, key, new)
    for f, t in files.items():
        cr, br, pa = t.count('\r'), t.count('{') - t.count('}'), t.count('(') - t.count(')')
        print('  %-38s CR %d->%d  brace delta %d->%d  paren delta %d->%d'
              % (f, before[f][0], cr, before[f][1], br, before[f][2], pa))
        assert cr == before[f][0], 'CR moved in ' + f
        dst = os.path.join(outroot, f)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        io.open(dst, 'w', encoding='utf-8', newline='').write(t)
    print('WROTE %d files under %s' % (len(files), outroot))
    return 0


if __name__ == '__main__':
    if '--apply' in sys.argv:
        sys.exit(run(ROOT))
    if '--dry' in sys.argv:
        sys.exit(run(sys.argv[sys.argv.index('--dry') + 1]))
    sys.exit(run(None))
