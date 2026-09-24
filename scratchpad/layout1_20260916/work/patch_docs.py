"""Lane LAYOUT1 (2026-09-16): the contract pages that spell a written path.

Every changed line carries its provenance inline (ww-contract-provenance): the
date, the lane, and bungo's own words, because a path in a contract page is the
one thing a consumer copies verbatim.
"""
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'
B = chr(92)
RULE = ('bungo 2026-09-16 19:3x, "The folder should be called FO4CSLOD maybe, '
        'so it\'d be Data/FO4CSLOD, sound fine?" (lane LAYOUT1)')


def patch(rel, subs):
    p = ROOT + rel
    s = open(p, encoding='utf-8', newline='').read()
    for a, b in subs:
        n = s.count(a)
        if n != 1:
            print('MISS %d in %s: %r' % (n, rel, a[:70]))
            sys.exit(1)
        s = s.replace(a, b)
    open(p, 'w', encoding='utf-8', newline='').write(s)
    d = open(p, 'rb').read()
    print('ok %-40s CR %d LF %d' % (rel, d.count(b'\r'), d.count(b'\n')))


# ---------------------------------------------------------------- BTD / .lodl
patch('docs/LODGEN_BTD_FORMAT.md', [
    ('## Location\n\n    Data' + B + 'Terrain' + B + '<WorldspaceEditorID>.lodl\n',
     '## Location\n\n    Data' + B + 'FO4CSLOD' + B + '<WorldspaceEditorID>' + B
     + '<WorldspaceEditorID>.lodl\n\n'
     'MOVED 2026-09-16 (' + RULE + '): every FO4CS-target output of a bake now\n'
     'lives under one root inside the mod folder, `Data' + B + 'FO4CSLOD' + B + '`, and each\n'
     "worldspace has its own folder under it. It was `Data" + B + 'Terrain' + B + "<WS>.lodl`\n"
     'until that day. `src/lodgenlayout.cpp` composes the folder and is the only\n'
     'place the name is spelled; `tests/spells/lodgen_layout.sh` is the gate.\n'),

    ('against an EXISTING `<dir>/Terrain/<name>.lodl`: heights (exact for FO4, half',
     'against an EXISTING `<dir>/FO4CSLOD/<name>/<name>.lodl` (moved 2026-09-16,\n'
     'lane LAYOUT1): heights (exact for FO4, half'),
])

# ------------------------------------------------------------------- ledger
patch('docs/LODGEN_LEDGER_FORMAT.md', [
    ('> which this ledger does not describe — so the ledger sits beside the `.BTR` and\n'
     '> `.BTO` files it is a ledger *of*. One word from bungo moves it.\n',
     '> which this ledger does not describe — so the ledger sits beside the `.BTR` and\n'
     '> `.BTO` files it is a ledger *of*. One word from bungo moves it.\n'
     '>\n'
     '> **2026-09-16, lane LAYOUT1.** That word came: ' + RULE + '. The\n'
     "> ledger's new home is `FO4CSLOD/<ws>/<ws>.lodb` and it belongs to lane\n"
     '> BAKEREC1, which owns the ledger. LAYOUT1 moved every OTHER FO4CS-target\n'
     '> output and deliberately wrote nothing into that spot; the ledger is still\n'
     '> at `<out-dir>/<WorldspaceEdid>.lodb` as this page says, and the layout gate\n'
     '> names it as an exemption rather than sweeping it.\n'),
])

# ------------------------------------------------------------------ manifest
patch('docs/LODGEN_MANIFEST_FORMAT.md', [
    ('One UTF-8 text file beside every `.BTO`, same stem plus `.manifest.txt`. LF line',
     'One UTF-8 text file per `.BTO`, same stem plus `.manifest.txt`.\n\n'
     'WHERE IT LANDS, 2026-09-16 (lanes BTOFREE1 then LAYOUT1): the `.BTO` itself is\n'
     'scaffolding under the FO4CS target — built in `<mod>/lodgen_bto_scratch/`, read\n'
     'by the five passes that need it, then removed — and the sidecar is KEPT. It is\n'
     'written beside the files it describes, `FO4CSLOD/<ws>/<stem>.BTO.manifest.txt`\n'
     '(' + RULE + '). Under the stock engine, and under `--keep-bto`, the `.BTO` and\n'
     'its sidecar are where they always were. LF line'),
])

# ----------------------------------------------------------------- native pair
patch('docs/LODGEN_NATIVE_LODO_LODI.md', [
    ('| `Data' + B + 'Terrain' + B + '<WS>' + B + 'Objects' + B + '<WS>.lodo` | the **geometry library**',
     '| `Data' + B + 'FO4CSLOD' + B + '<WS>' + B + '<WS>.lodo` | the **geometry library**'),
    ('| `Data' + B + 'Terrain' + B + '<WS>' + B + 'Objects' + B + '<WS>.lodi` | the **instance tables**',
     '| `Data' + B + 'FO4CSLOD' + B + '<WS>' + B + '<WS>.lodi` | the **instance tables**'),
])

# ------------------------------------------------------------------------- VT
patch('docs/LODGEN_TERRAIN_VT.md', [
    ('**Name:** `Data' + B + 'Terrain' + B + '<EDID>.VT.<dim>.lodt`, one per level.',
     '**Name:** `Data' + B + 'FO4CSLOD' + B + '<EDID>' + B + '<EDID>.VT.<dim>.lodt`, one per level.'),

    ('`Data' + B + 'Terrain' + B + '` and not `Data' + B + 'Textures' + B + 'Terrain' + B + '<WS>' + B + '`, because the latter is\n'
     'enumerated by name with a cap that a level\'s worth of tiles would blow past; and\n'
     '`Data' + B + 'Terrain' + B + '` is read by exact name and is where the `.lodl` precedent already\n'
     'lives.',
     'NOT `Data' + B + 'Textures' + B + 'Terrain' + B + '<WS>' + B + '`, because that folder is enumerated by name\n'
     "with a cap that a level's worth of tiles would blow past; the containers are read\n"
     'by exact name, off the index.\n\n'
     'MOVED 2026-09-16: it was `Data' + B + 'Terrain' + B + '<EDID>.VT.<dim>.lodt` until that day,\n'
     'beside the `.lodl`. ' + RULE + ' — so the `.lodl` precedent moved with it and both\n'
     'now sit in the worldspace\'s own folder under the one root. The `"container"`\n'
     'strings inside the index moved with the files (lane LAYOUT1;\n'
     '`tests/spells/lodgen_layout.sh` legs (a) and (b)).'),

    ('"container": "Terrain' + B + B + 'Commonwealth.VT.2.lodt", "tiles": 9216, "present": 9216 }',
     '"container": "FO4CSLOD' + B + B + 'Commonwealth' + B + B + 'Commonwealth.VT.2.lodt",\n'
     '        "tiles": 9216, "present": 9216 }'),
])

# ----------------------------------------------------------------- card sheets
patch('docs/LODGEN_CARD_SHEETS.md', [
    ('Data' + B + 'Textures' + B + 'Terrain' + B + '<ws>' + B + 'Objects' + B + '<ws>.LodgenCards.legacy.<SW>x<SH>_d.DDS',
     'Data' + B + 'FO4CSLOD' + B + '<ws>' + B + 'Objects' + B + '<ws>.LodgenCards.legacy.<SW>x<SH>_d.DDS'),
])

print('done')
