# fix03_docs.py -- lane IMPOSTORDEPTH2: the card `_n` is BC7 in the two contracts.
import io

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'


def patch(path, edits):
    b = open(ROOT + path, 'rb').read()
    cr = b.count(b'\r')
    s = b.decode('utf-8')
    for old, new, n in edits:
        c = s.count(old)
        assert c == n, (path, old[:60], c, n)
        s = s.replace(old, new)
    out = s.encode('utf-8')
    assert out.count(b'\r') == cr, path
    open(ROOT + path, 'wb').write(out)
    print('patched', path)


SHEETS = 'docs/LODGEN_CARD_SHEETS.md'
patch(SHEETS, [
    ('                           <formid8hex>_oct_n.DDS               BC3\n',
     '                           <formid8hex>_oct_n.DDS               BC7 (DX10, DXGI 98)\n', 2),
    ('per-card sets stay on disk beside the arrays for a consumer without arrays.\n',
     'per-card sets stay on disk beside the arrays for a consumer without arrays.\n'
     '\n'
     'The `_n` array is **BC7** (DXGI 98), like the per-card `_n`; every other array\n'
     'keeps its format (§6).\n', 1),
    ('shadows and the model-to-card transition |\n',
     'shadows and the model-to-card transition. Stored **BC7** since 2026-09-23 (§6.1) |\n', 1),
    ('a `DXT1`/`DXT5` fourCC where the format allows it, or the DX10 extension.\n',
     'a `DXT1`/`DXT5` fourCC where the format allows it, or the DX10 extension. The\n'
     'card `_n` is always the DX10 extension: `dxgiFormat` **98** (`BC7_UNORM`),\n'
     '`resourceDimension` 3, `arraySize` 1, the same 124-byte header as the table\n'
     'below, and the payload 20 bytes further on. Every other card sheet keeps its\n'
     'fourCC.\n', 1),
    ('| DXT10 `dxgiFormat` | **77** (`BC3_UNORM`) for the colour, normal and mask sheets; **71** (`BC1_UNORM`) for the emissive |\n',
     '| DXT10 `dxgiFormat` | **98** (`BC7_UNORM`) for the normal sheet (`_n`); **77** (`BC3_UNORM`) for the colour and mask sheets; **71** (`BC1_UNORM`) for the emissive |\n', 1),
    ('is opaque by contract, and the reason the single-sheet writer needed an explicit\n'
     '`bc1Alpha` opt-in for punch-through atlases.\n',
     'is opaque by contract, and the reason the single-sheet writer needed an explicit\n'
     '`bc1Alpha` opt-in for punch-through atlases. The BC7 `_n` keeps its alpha (sway)\n'
     'down the chain, as BC3 does.\n'
     '\n'
     '### 6.1 The `_n` sheet is BC7 (2026-09-23)\n'
     '\n'
     'bungo\'s ruling, on lane IMPOSTORDEPTH1\'s measurement: under DXT5 the height\n'
     'sat in the 5:6:5 colour block with the normal\'s X and Y, and came back 2.81\n'
     'levels wrong on average (34 units), p95 8, with 27 of the bake\'s 59 heights\n'
     'surviving the decode. That is the doubled, thinned trunk on the 8x8 maple.\n'
     '\n'
     '**The encoder is in-tree**: `src/lodgenbc7.h`, header-only, no library, no\n'
     'download. It is deterministic -- the same pixels give the same bytes on any\n'
     'thread count; two bakes of the maple are byte-identical. It tries mode 6, modes\n'
     '5 and 4 under every channel rotation (4 with both index selections), mode 7 on\n'
     'the four best partitions, and modes 3 and 1 on opaque blocks, and keeps the\n'
     'least WEIGHTED squared error: R 1, G 1, **B (height) 32**, A (sway) 1. The\n'
     'weight is the measured knee: 16 left p95 at 3, 64 began to lose heights.\n'
     'Every block was decoded with the vendored detex decoder (`lib/detex`) and\n'
     'its error equals the encoder\'s claim; the gate decodes with Pillow.\n'
     '\n'
     'Measured on the maple (1920x2048, 12 mips, the 1,037,765 covered texels),\n'
     'against the height lodgen was GIVEN to encode (the bake PNG after\n'
     '`lodgenRepairOctHeight`):\n'
     '\n'
     '| | DXT5 (before) | BC7 (now) |\n'
     '|---|---|---|\n'
     '| height error, mean | 2.64 levels | **0.57** |\n'
     '| height error, p95 | 7 | **2** |\n'
     '| heights surviving | 27 of 58 | **57 of 58** (the lost one is the extreme, 5 texels) |\n'
     '| normal X / Y error, mean | 11.3 / 8.8 | 3.3 / 3.3 |\n'
     '| sway (A) error, mean / p95 | 0.02 / 0 | **1.34 / 4** -- the cost |\n'
     '| file | 5,222,528 B | 5,222,548 B (the DX10 header) |\n'
     '| lodgen compress, whole card | 12.2 s | 9.6-10.1 s (single runs; the encode is not slower) |\n'
     '\n'
     'Against the raw bake PNG (the repair included) the height reads 2.81 / p95 8 /\n'
     '27 of 59 before and 1.00 / p95 4 / 58 of 59 now. Gate:\n'
     '`tests/spells/impostor_sheetbar.py`, run by `tests/spells/impostor_trunk.sh`.\n'
     '\n'
     'Not moved, and named here so nobody assumes otherwise: the aggregate `_n`\n'
     '(§10) and every mesh `_n` (the source arrays, the atlas) stay BC3.\n', 1),
    ('6. Impostor cards are **never** decimated by the far-ring simplifier, and are\n'
     '   excluded from it a second time by object index off the `C` lines.\n',
     '6. Impostor cards are **never** decimated by the far-ring simplifier, and are\n'
     '   excluded from it a second time by object index off the `C` lines.\n'
     '7. The card `_n` -- per-card set AND array -- is **BC7_UNORM** (DXGI 98, linear,\n'
     '   not sRGB) under the DX10 header: `arraySize` 1 on a per-card set, the layer\n'
     '   count on an array. A set baked before 2026-09-23 has a **DXT5** `_n` (legacy\n'
     '   header) with the same channel meaning. **A reader picks the decoder from\n'
     '   the header, never from the file name.** The `.lodm` names files, not\n'
     '   formats, and is unchanged; no version moved.\n', 1),
])

SPEC = 'docs/LODGEN_IMPOSTOR_SPEC.md'
patch(SPEC, [
    ('| `_n` | BC3 | normal X | normal Y | height | sway weight |\n',
     '| `_n` | BC3; **BC7** on a card | normal X | normal Y | height | sway weight |\n', 1),
    ('third texture\'s colour block with two channels that are nearly material\n'
     'labels.\n',
     'third texture\'s colour block with two channels that are nearly material\n'
     'labels.\n'
     '\n'
     '**The card\'s `_n` is BC7** (2026-09-23, bungo\'s ruling). On a card the\n'
     'height is not a detail: it places every frame at its depth, and under BC3 it\n'
     'came back 2.81 levels (34 units) wrong on average with 27 of 59 heights left,\n'
     'which doubled and thinned the trunk. BC7 spends the same 16 bytes per 4x4 on\n'
     'up to 16 index levels with 7-bit end points, and the in-tree encoder\n'
     '(`src/lodgenbc7.h`) weights the height 32 to 1: 0.57 levels, p95 2, 57 of 58\n'
     'heights. The price is the sway, which leaves its private alpha ramp (0.02 ->\n'
     '1.34 levels mean). Numbers and the gate: `docs/LODGEN_CARD_SHEETS.md` §6.1.\n'
     'Mesh `_n` sheets (source arrays, the atlas) and the aggregate `_n` stay BC3.\n', 1),
    ('the manifest\'s `C` line opens the `.lodm` and draws the sheets instead.\n',
     'the manifest\'s `C` line opens the `.lodm` and draws the sheets instead.\n'
     '\n'
     '**What FO4CS must decode (2026-09-23).** `_oct_n.DDS` is a DX10 DDS:\n'
     '`dxgiFormat` **98** (`DXGI_FORMAT_BC7_UNORM`, linear, not sRGB),\n'
     '`resourceDimension` 3, `arraySize` 1, the full mip chain; the card `_n`\n'
     'ARRAY is the same with `arraySize` = the layer count. Direct3D 11 samples\n'
     'BC7 in hardware, so this is a load change and no shader change: create the\n'
     'texture with the format the header names. The other card sheets keep BC3 /\n'
     'BC1. A set baked before this date carries a DXT5 `_n`; a loader that takes\n'
     'the format from the header (DirectXTex `LoadFromDDSMemory` does) reads both.\n'
     'Channel meaning is unchanged and the `.lodm` names files, not formats, so no\n'
     'version moved.\n', 1),
    ('DX10 array per texture (BC3, and BC1 for the emissive), grouped by FAMILY and\n',
     'DX10 array per texture (BC3; BC7 for `_n`; BC1 for the emissive), grouped by FAMILY and\n', 1),
])
