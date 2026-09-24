import hashlib

p = 'docs/LODGEN_TERRAIN_VT.md'
s = open(p, 'r', encoding='utf-8', newline='').read()


def rep(a, b, n=1):
    global s
    assert s.count(a) == n, (a[:80], s.count(a))
    s = s.replace(a, b)


rep("""**Status: WRITER AND VALIDATOR SHIPPED, off by default** (`--vt`); **NO
CONSUMER** — no `.lodt` reader outside this tree, no tile streamer, no residency
manager. FO4CS's *Improved LOD* module is the first planned one.
**Verified against the writer 2026-09-09** (lane CONTRACTS): every offset,
stride, flag and refusal in §3 was re-read at `src/io/lodvfile.cpp` and matched;
the provenance footer names the lines. No correction was needed.""",
    """**Status: WRITER AND VALIDATOR SHIPPED, off by default** (`--vt`); **NO
CONSUMER** — no `.lodt` reader outside this tree, no tile streamer, no residency
manager. FO4CS's *Improved LOD* module is the first planned one.
**Re-derived against the writer 2026-09-11** (lane TERRAIN-R): §2.2, §2.5, §3 and
§4 were rewritten for version 2 and every line number in the provenance footer
was found again from its own anchor text against the sources stamped there.""")

# the provenance footer: replace the whole trailing block
i = s.index('## Provenance')
head = s[:i]

files = ['src/io/lodvfile.cpp', 'src/io/lodvfile.h', 'src/lodgen.cpp',
         'src/lodgen.h', 'src/esmdata.cpp', 'src/esmdata.h']
rows = []
for f in files:
    b = open(f, 'rb').read()
    rows.append('| `%s` | `%s` | %s | %d |'
                % (f, hashlib.sha256(b).hexdigest()[:16], format(len(b), ','), b.count(b'\n')))

anchors = [
    ('magic `LDTX`, header 256 B, payload alignment 4096', '`lodvfile.h:67`, `lodvfile.cpp:32`',
     '`constexpr quint32 LODTEX_MAGIC = 0x5854444CU;`'),
    ('**version 2**, and that a v1 file is refused by name',
     '`lodvfile.h:91`, `lodvfile.cpp:29, 468`',
     '`constexpr quint32 LODTEX_VERSION = 2;` / `refused: version 1 container -- four sheets whose `'),
    ('the role set: mask 5, emissive 6, data 3 retired',
     '`lodvfile.h:101-102`', '`LODV_ROLE_MASK = 5,` / `LODV_ROLE_EMISSIVE = 6`'),
    ('six sheet descriptors at 0xA0, reserved tail from 0xD0',
     '`lodvfile.h:107, 163`, `lodvfile.cpp:148, 197`',
     '`constexpr int LODV_MAX_SHEETS = 6;` / `LodvSheetDesc sheets[LODV_MAX_SHEETS];` (the two 0xA0 loops are the write and the read side, in that order)'),
    ('the cover carrier is whichever sheet declares two formats',
     '`lodvfile.cpp:220`',
     '`const quint16 fmt = ( cover && sd.dxgiFormatCover != sd.dxgiFormat )`'),
    ('rule 13: role 3 refused by name', '`lodvfile.cpp:555`',
     '`refused: sheet %1 has role 3 \\`data\\` (R AO, G wetness, `'),
    ('rule 13: colour, msn and mask are all required', '`lodvfile.cpp:591`',
     '`refused: a version 2 container must carry the colour `'),
    ('rule 13: exactly one cover carrier', '`lodvfile.cpp:581`',
     '`refused: sheets %1 and %2 both declare a `'),
    ('§2.2 the three mask rules and their words', '`lodgen.h:376`, `lodgen.cpp:1787`',
     '`enum LodgenMaskRule` / `void lodgenResolveMaterialMask( const QString & dataRoot, const QString & matName,`'),
    ('§2.2 the gloss law, shared with the object arrays pass',
     '`lodgen.h:392`, `lodgen.cpp:1782, 4657`',
     '`float lodgenLegacyGloss( float smoothness, float specGreen );` / `r = b8( lodgenLegacyGloss( smooth, sG ) );`'),
    ('§2.2 the per-LTEX resolution and its rule census', '`lodgen.cpp:6768`',
     '`struct LodgenVtMaskCache`'),
    ('§2.2 the mask texel: cover, roughness, metallic, AO', '`lodgen.cpp:7200`',
     '`out.mask[size_t( j ) * S + i] =`'),
    ('§2.3 the mask filters plainly on all four channels', '`lodgen.cpp:7318`',
     '`out.mask[o] = ( ( ( acc[3][3] + 2 ) >> 2 ) << 24 )`'),
    ('§2.2 the emissive sheet is decided before any container opens',
     '`lodgen.cpp:7532`', '`wantEmissive = maskCache.withEmissive > 0;`'),
    ('§3.1 the sheet descriptors the writer emits', '`lodgen.cpp:7580`',
     '`h.sheets[2] = { LODV_DXGI_BC1_UNORM, maskCoverFmt, LODV_ROLE_MASK, 0 };`'),
    ('§4 `family: "pbr"`', '`lodgen.cpp:7776`',
     '`root.insert( QStringLiteral( "family" ), QStringLiteral( "pbr" ) );`'),
    ('§4 `dropped` and `maskRules`', '`lodgen.cpp:7846, 7865`',
     '`t.insert( QStringLiteral( "dropped" ), dropped );` / `t.insert( QStringLiteral( "maskRules" ), rules );`'),
    ('§2.2 the layer\'s material is MNAM and its `_s` map is TX07',
     '`esmdata.h:138`, `esmdata.cpp:501`',
     '`struct EsmLtexTextureSet` / `const EsmLtexTextureSet & EsmWorld::ltexTextureSet( quint32 ltexForm ) const`'),
    ('§5 `--vt-cover-in-color`', '`lodgen.h:483`', '`bool coverInColor = false;`'),
]

foot = ['## Provenance', '',
        'Sections 2.2, 2.2a, 2.5, 3 and 4 were rewritten for container version 2 on',
        '**2026-09-11** by lane TERRAIN-R. Every line number below was found again from',
        'its own ANCHOR TEXT against the sources stamped here, never shifted by a delta',
        '(`ww-contract-provenance` step 3, script',
        '`scratchpad/terrain_r_20260911/anchors.txt`); each anchor was found exactly',
        'once. The measured numbers in 2.2a and 2.5 were re-derived from the artefacts',
        'they describe rather than copied forward.', '',
        '| file | sha256 (16) | bytes | lines |', '|---|---|---|---|']
foot += rows
foot += ['', '| claim | line | anchor |', '|---|---|---|']
for c, l, a in anchors:
    foot.append('| %s | %s | %s |' % (c, l, a))
foot.append('')

s = head + '\n'.join(foot)
open(p, 'w', encoding='utf-8', newline='').write(s)
b = open(p, 'rb').read()
print('CR', b.count(b'\r'), 'LF', b.count(b'\n'))
