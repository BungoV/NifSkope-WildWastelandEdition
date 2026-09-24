import os
R = r'E:\Projects\NifskopeWWE-incrgate1'


def rep(s, a, b):
    assert s.count(a) == 1, (a[:70], s.count(a))
    return s.replace(a, b)


def patch(rel, pairs):
    p = os.path.join(R, rel)
    src = open(p, 'rb').read()
    cr0 = src.count(b'\r')
    for a, b in pairs:
        src = rep(src, a.encode('utf-8'), b.encode('utf-8'))
    assert src.count(b'\r') == cr0
    open(p + '.tmp', 'wb').write(src)
    os.replace(p + '.tmp', p)
    print('patched', rel)


patch('docs/FO4CS_IMPROVED_LOD_PLAN.md', [
    ('| 13 | **the asymmetric-drop proof on the (-32,0) dim-32 chunk has never been run** |',
     '| 13 | ~~**the asymmetric-drop proof on the (-32,0) dim-32 chunk has never been run**~~ \u2014 '
     '**DONE 2026-09-24, lane INCRGATE1**: `tests/spells/lodgen_native_baseline.sh --drop-proof` bakes the chunk '
     'ONCE (`--slot-fallback --identity --keep-bto --native`, 84 s). The stock `.BTO` has no geometry for '
     '**2,628 of 42,560** placements (**6.17 %**, NATIVE 9\'s figure exactly: manifest rows on no '
     '`--dump-geometry` identity line); the native pair holds **all 42,560** (the decoder finds every manifest '
     'row, `instanceCount` 42,560, `--native-verify` `instancesWithNeitherGeometryNorCard 0`). Red controls: '
     'the stock count must be above 0, and a manifest row in no table must fail by name |'),
    ('| 25 | **the census checker\'s 59 / 0 / 31 has not been re-run on a v4 pair**, and the cited pairs are v3, '
     'which the decoder refuses (added 2026-09-23) |',
     '| 25 | ~~**the census checker\'s 59 / 0 / 31 has not been re-run on a v4 pair**, and the cited pairs are v3, '
     'which the decoder refuses (added 2026-09-23)~~ \u2014 **DONE 2026-09-24, lane INCRGATE1**: re-run on row 26\'s '
     'default pair (`.lodo` 4 / `.lodi` 7) by `tests/spells/lodgen_sanctuary_pair.sh`: **38 ok / 0 RED / 32 '
     'not-derivable**, its own floor caught (2 of 3; the third, `level1.triangles`, is skipped by name because '
     'an authored-only pair has no level 1). The first run read 37 / **1 RED** / 31: `lodo.ladderGroup` 4 '
     'against 0. The file was right. The ladder is OFF by default, the census line prints the target a ladder '
     'would be built at, and the file writes 0 by contract (NATIVE 3, 0xCD). The checker is fixed, and a red leg '
     'shows ladderGroup doctored to 4 is still caught. The fall from 59 is the ladder\'s own words; the v3 '
     'figure stays historical |'),
    ('| 26 | **no default-settings Sanctuary pair on disk** (authored-only library, `.lodi` v7) (added 2026-09-23) |',
     '| 26 | ~~**no default-settings Sanctuary pair on disk** (authored-only library, `.lodi` v7) (added 2026-09-23)~~ '
     '\u2014 **DONE 2026-09-24, lane INCRGATE1**: `tests/spells/lodgen_sanctuary_pair.sh` bakes (-20,24)..(-9,35) '
     'dim 4 with `--native` and no other switch (20 s) into a scratch folder that stays out of git (the spell '
     're-makes it; nothing is committed): `.lodo` **v4, 6,204,388 B** (2,970 bases, 10,634 clusters), `.lodi` '
     '**v7, 527,989 B**, **3,526** placements in 10 chunks = the bake\'s own manifest rows, 0 occluders, 0 '
     'cell-line ambiguities; the decoder 54 / 0. Red control: a `.lodi` cut 4,096 B short is refused by name |'),
    ('gate needs a re-bake first (\u00a75 row 26).\n',
     'gate needs a re-bake first (\u00a75 row 26). **Re-baked 2026-09-24 (lane INCRGATE1, \u00a75 row 26):** '
     'the default pair\'s `.lodi` is v7, **527,989 B**, 3,526 placements in 10 chunks, beside the same '
     '6,204,388 B `.lodo`; `tests/spells/lodgen_sanctuary_pair.sh` makes it.\n'),
])
patch('docs/LODGEN_CENSUS.md', [
    ('needs a re-bake at `.lodo` 4 / `.lodi` 7 and a fresh run; until then 59/0/31 is a\n'
     '2026-09-11 measurement, not a gate.\n',
     'needs a re-bake at `.lodo` 4 / `.lodi` 7 and a fresh run; until then 59/0/31 is a\n'
     '2026-09-11 measurement, not a gate.\n\n'
     '**Re-run 2026-09-24 (lane INCRGATE1) on a v4 pair:** the default-settings Sanctuary\n'
     'pair (`.lodo` 4, 6,204,388 B / `.lodi` 7, 527,989 B), made by\n'
     '`tests/spells/lodgen_sanctuary_pair.sh`, reads **38 ok / 0 RED / 32 not-derivable**,\n'
     'the floor caught. The first run was 37 / 1 RED / 31. The one RED was the checker:\n'
     'with the ladder OFF (the default) the `native-ladder:` line still prints\n'
     '`(group 4, ...)`, the target of a ladder that was not built, and the file writes\n'
     '`ladderGroup` 0 by contract. The checker now claims 0 for the file in that case and\n'
     'carries the printed target as the not-derivable `bake.ladderGroupTarget`. A red leg\n'
     'in the spell shows a doctored 4 is still caught. There are fewer checks than 59\n'
     'because an authored-only pair has no ladder levels to compare.\n'),
])
patch('docs/LODGEN_NATIVE_LODO_LODI.md', [
    ('geometry nor a card** (`--native-verify`). The asymmetric drop proof on\n'
     '(\u221232,0) dim 32 is **still owed** \u2014 it needs a `--slot-fallback --native` bake of\n'
     'that one chunk and has not been run by NATIVE1a or NATIVE1b.\n',
     'geometry nor a card** (`--native-verify`). **The asymmetric drop proof on\n'
     '(\u221232,0) dim 32 has been run** (lane INCRGATE1, 2026-09-24,\n'
     '`tests/spells/lodgen_native_baseline.sh --drop-proof`). One bake of that chunk with\n'
     '`--slot-fallback --identity --keep-bto --native` gives both halves. The stock `.BTO`\n'
     'has no geometry for **2,628 of 42,560** placements (6.17 %, the figure above).\n'
     'The `.lodi` holds **all 42,560**: every manifest row is in its table, and\n'
     '`--native-verify` reads `instancesWithNeitherGeometryNorCard 0`.\n'),
])
