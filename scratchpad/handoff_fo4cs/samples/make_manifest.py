"""Write MANIFEST.md for the FO4CS sample set.

Every size is READ from the file; nothing is typed. The command column and the
contract column are keyed off the path shape, so a file that appears without a
rule is listed with a loud `???` rather than quietly inheriting a neighbour's
provenance.

  python make_manifest.py
"""
import datetime
import os
import struct

HERE = os.path.dirname(os.path.abspath(__file__))

# path pattern -> (what it is, the command that made it, the contract document)
RULES = [
    ('.BTO.manifest.txt', 'per-object manifest, version 2',
     'CHUNKS', 'docs/LODGEN_MANIFEST_FORMAT.md'),
    ('.BTO', 'object chunk (stock bake)',
     'CHUNKS', 'docs/LODGEN_VERTEX_PACKING.md'),
    ('.BTR', 'terrain chunk (stock bake)',
     'CHUNKS', 'docs/LODGEN_VERTEX_PACKING.md'),
    ('LodgenCards.', 'card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm',
     'CHUNKS', 'docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md'),
    ('LodgenArrays.txt', 'array sidecar index: family, class, layer, .lodm, colour, normal, mask, source',
     'CHUNKS', 'docs/LODGEN_TEXTURE_ARRAYS.md'),
    ('LodgenArrays.', 'mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm',
     'CHUNKS', 'docs/LODGEN_TEXTURE_ARRAYS.md'),
    ('_data.DDS', 'terrain data sheet: R sky-free AO, G flow wetness, B shore proximity, A ground cover',
     'CHUNKS', 'docs/LODGEN_TERRAIN_VT.md section 1'),
    ('_msn.DDS', 'terrain model-space normal sheet',
     'CHUNKS', 'docs/LODGEN_TERRAIN_VT.md section 2.2'),
    ('.VT.lodm', 'the pyramid index, kind:"terrainVT"',
     'VT', 'docs/LODGEN_TERRAIN_VT.md section 4 + docs/LODGEN_LODM_FORMAT.md section 5'),
    ('.lodt', 'ONE LEVEL of the terrain virtual texture: bordered 256-texel tiles, four sheets each (colour, model-space normal, data, R16 height)',
     'VT', 'docs/LODGEN_TERRAIN_VT.md'),
    ('.DDS', 'terrain albedo sheet (grass tint folded in)',
     'CHUNKS', 'docs/LODGEN_TERRAIN_VT.md section 1'),
    ('make_samples.sh', 'the script that made everything here', '-', '-'),
    ('make_samples.log', 'its whole stdout, every run', '-', '-'),
    ('make_manifest.py', 'the script that made this file', '-', '-'),
    ('MANIFEST.md', 'this file', '-', '-'),
]

CMD = {
    'CHUNKS': 'C<dim>',
    'VT': 'V',
    '-': '-',
}


def classify(rel):
    for pat, what, cmd, contract in RULES:
        if pat in rel:
            return what, cmd, contract
    return '??? UNCLASSIFIED', '???', '???'


def dim_of(rel):
    head = rel.split('/')[0]
    if head.startswith('L') and head[1:].isdigit():
        return head[1:]
    if head == 'vt':
        return 'V'
    return '-'


def main():
    rows = []
    total = 0
    for root, _dirs, files in os.walk(HERE):
        for f in sorted(files):
            p = os.path.join(root, f)
            rel = os.path.relpath(p, HERE).replace('\\', '/')
            size = os.path.getsize(p)
            total += size
            what, cmd, contract = classify(rel)
            if cmd in ('CHUNKS', 'VT'):
                d = dim_of(rel)
                cmd = 'V' if d == 'V' else 'C%s' % d
            rows.append((rel, size, what, cmd, contract))
    rows.sort()

    land = r'E:/Projects/Fallout 4 Mods/mods/FO4CS/Terrain/Commonwealth.lodl'
    lb = open(land, 'rb').read(8)
    lver = struct.unpack('<I', lb[4:8])[0]
    lsz = os.path.getsize(land)
    lmt = datetime.datetime.fromtimestamp(os.path.getmtime(land)).strftime('%Y-%m-%d %H:%M')

    out = []
    w = out.append
    w('# The FO4CS sample set -- every file, its size, its command, its contract\n')
    w('Written by lane IMAGES5, 2026-09-09, on `release/NifSkope.exe` **19:35:14**')
    w('(the build carrying lane RENAME\'s FINAL FILE NAMES and lane OFFSCREEN2\'s')
    w('invisible headless window). **No build happened.** `Fallout4.exe` and any')
    w('other `NifSkope.exe` were checked absent before the run; the gate is inside')
    w('`make_samples.sh`.\n')
    w('**The region is the one containing cell (0,0)** -- cells 0..3 x 0..3. At each')
    w('far level the sweep bakes every chunk TOUCHING that rectangle, which here is')
    w('exactly the one chunk that CONTAINS it, so the four levels are four views of')
    w('the same ground, nested:\n')
    w('| level | chunk | cells |')
    w('|---|---|---|')
    w('| 4 | `Commonwealth.4.0.0` | 0..3 x 0..3 |')
    w('| 8 | `Commonwealth.8.0.0` | 0..7 x 0..7 |')
    w('| 16 | `Commonwealth.16.0.0` | 0..15 x 0..15 |')
    w('| 32 | `Commonwealth.32.0.0` | 0..31 x 0..31 |\n')
    w('---\n')
    w('## The commands\n')
    w('Both are in `make_samples.sh`, which is in this directory and carries the')
    w('game-down and one-instance gates. `ESM` =')
    w('`X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm`, `DATA` =')
    w('`E:/Tools/Fallout 4/DataUnpacked/Data`, `CARDS` =')
    w('`<repo>/scratchpad/images_20260909/gen/cards_trees19` (the 19-tree octahedral')
    w('library this lane baked), `S` = this directory. Every path absolute.\n')
    w('**`C<dim>` -- the chunk bakes, one per level:**\n')
    w('```')
    w('release/NifSkope.exe -no-gui lodgen "$ESM" --worldspace 3C \\')
    w('  --terrain-region 0 0 3 3 --dim <4|8|16|32> \\')
    w('  --out-dir $S/L<dim> --tex-dir $S/L<dim>/textures/terrain/Commonwealth \\')
    w('  --data-root "$DATA" --arrays --impostors "$CARDS" --cover [--slot-fallback]')
    w('```')
    w('`--slot-fallback` at dim 16 and 32 only. Identity is ON (the default), which')
    w('is what writes the `.BTO` channel contract and the `.manifest.txt`.')
    w('**No `--atlas`**: the atlas sheets are the stock engine\'s draw-call')
    w('optimisation and the native target drops them (README section 1); `--arrays`')
    w('is the FO4CS path and must run before an atlas would.\n')
    w('**`V` -- the terrain virtual texture, one run for the whole pyramid:**\n')
    w('```')
    w('release/NifSkope.exe -no-gui lodgen "$ESM" --worldspace 3C \\')
    w('  --terrain-region 0 0 3 3 --vt $S/vt --vt-height --cover \\')
    w('  --out-dir $S/vt/tex --tex-dir $S/vt/tex/textures/terrain/Commonwealth \\')
    w('  --data-root "$DATA"')
    w('```')
    w('`--vt-height` is OFF by default and is passed deliberately: it adds the fourth')
    w('R16 height sheet per tile (+133% on a tile), and a sample set that omits a')
    w('sheet cannot be used to write a reader for it. `vt/tex/` is that run\'s own')
    w('chunk output, kept because `--vt-btr` (on by default) ASSEMBLES the `.btr`')
    w('sheets from the pyramid rather than baking them again, so it is the one')
    w('artefact that proves the assembly path ran.\n')
    w('---\n')
    w('## Every file\n')
    w('Sizes read from disk by `make_manifest.py`, not typed. `cmd` names which')
    w('command above wrote it.\n')
    w('| file | bytes | cmd | what it is | contract |')
    w('|---|---:|---|---|---|')
    for rel, size, what, cmd, contract in rows:
        w('| `%s` | %s | %s | %s | %s |' % (rel, format(size, ','), cmd, what, contract))
    w('')
    w('**%d files, %s bytes (%.1f MB).**\n' % (len(rows), format(total, ','), total / 1048576.0))
    w('---\n')
    w('## Everything was read back through this tree\'s own validators\n')
    w('Not "it wrote a file". Each of these exited 0 and printed what it parsed;')
    w('the `.lodt` check walks every rule of `docs/LODGEN_TERRAIN_VT.md` section 3.4')
    w('and verifies **every tile\'s CRC**.\n')
    w('```')
    w('release/NifSkope.exe -no-gui lodgen --lodt-check $S/vt/Terrain/Commonwealth.VT.<dim>.lodt')
    w('release/NifSkope.exe -no-gui lodgen --lodm-check <any .lodm above>')
    w('```\n')
    w('| level | present tiles | cover tiles | stored bytes | rc |')
    w('|---|---:|---:|---:|---|')
    w('| 2 | 32 | 30 | 11,744,960 | 0 |')
    w('| 4 | 8 | 8 | 2,959,360 | 0 |')
    w('| 8 | 4 | 4 | 1,479,680 | 0 |')
    w('| 16 | 2 | 2 | 739,840 | 0 |')
    w('| 32 | 1 | 1 | 369,920 | 0 |\n')
    w('Every level reports the same three non-colour sheets -- `role 2` msn and')
    w('`role 3` data at dxgi 71 (BC1), `role 3` turning 77 (BC3) where a tile has')
    w('ground cover, and `role 4` height at dxgi 56 (R16_UNORM).\n')
    w('`--lodm-check` on one of each kind: `Commonwealth.VT.lodm` ->')
    w('`kind terrainVT`, 1,964 bytes; `Commonwealth.LodgenArrays.128x128.lodm` ->')
    w('`kind array`; `Commonwealth.LodgenCards.legacy.1024x1024.lodm` ->')
    w('`kind cardArray`. All three `lodm ok 1`, `version 1`, `family legacy`.\n')
    w('**The pyramid is PARTIAL and says so**: the index carries')
    w('`"partial": true` and `extent {south 0, west 0, north 3, east 3}`, because')
    w('`--terrain-region` was given. A consumer must read that rather than assume a')
    w('whole worldspace.\n')
    w('---\n')
    w('## What is here that the README said did not exist\n')
    w('| README section 5 said missing | now |')
    w('|---|---|')
    w('| a version-2 `.lodl` | the five INSTALLED files are version 2 (below); no copy here |')
    w('| a `.lodt` at any level | five, levels 2/4/8/16/32, all validated |')
    w('| a `<WS>.VT.lodm` index | `vt/Terrain/Commonwealth.VT.lodm` |')
    w('| a version-2 manifest | four, one per level (`# lodgen manifest 2 ...`) |')
    w('| a `kind:"array"` `.lodm` + `LodgenArrays*` | two size classes per level, all four levels |')
    w('| a `kind:"card"` `.lodm` + `<id>_oct_*.DDS` | the 19-tree library, `scratchpad/images_20260909/gen/cards_trees19` |')
    w('| a `kind:"cardArray"` `.lodm` + `LodgenCards.*` | four frame classes at L16 and L32, one at L8 |')
    w('| `LodgenObjects*` atlas sheets | **still absent, deliberately** -- `--atlas` is the stock path and the native target drops it |')
    w('| `.lodo` / `.lodi` | **still absent** -- there is no writer |\n')
    w('**The card arrays hold 18 of the 19 trees.** `000a7206 TreeBlasted01Lichen`')
    w('baked its sheets and its sidecar like the other 18, but `--impostors` wrote no')
    w('`_oct.lodm` and no DDS for it -- 18 `.lodm` and 90 `.DDS` against 19 sidecars.')
    w('Candidate, unverified: no placement in the region stood on that base, and the')
    w('conversion is per placed card.\n')
    w('---\n')
    w('## The whole-worldspace land file is NOT here\n')
    w('It exists already and is 36 MB; copying it would only make a second one to go')
    w('stale. Point at:\n')
    w('```')
    w('E:\\Projects\\Fallout 4 Mods\\mods\\FO4CS\\Terrain\\Commonwealth.lodl')
    w('```')
    w('**%s bytes, header version %d, written %s** -- read from the file by this')
    w('script, not copied from an older page. Contract:')
    w('`docs/LODGEN_BTD_FORMAT.md`.\n' % ())
    w('**That version is the live incompatibility.** FO4CS\'s parser pins')
    w('`kVersion = 1u`; all five installed `.lodl` files are version 2 as of')
    w('2026-09-09 17:27 and would be REFUSED, not misread. The version-1 twins are')
    w('the `*.lodt.bak-20260909` copies beside them (2026-09-05 03:14). The')
    w('zero-effort fallback on our side needs no rebuild: `WW_LODL_VERSION=1`.\n')
    text = '\n'.join(out)
    text = text.replace('**%s bytes, header version %d, written %s** -- read from the file by this',
                        '**%s bytes, header version %d, written %s** -- read from the file by this'
                        % (format(lsz, ','), lver, lmt))
    path = os.path.join(HERE, 'MANIFEST.md')
    with open(path, 'wb') as fh:
        fh.write(text.encode('utf-8'))
    print('%s  %d bytes, %d rows, %s total' % (path, len(text), len(rows), format(total, ',')))
    bad = [r for r in rows if r[3] == '???']
    print('unclassified: %d' % len(bad))
    for r in bad:
        print('   ', r[0])


if __name__ == '__main__':
    main()
