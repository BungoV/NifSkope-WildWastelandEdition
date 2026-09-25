# CARDFIX1 step 6 (IMPOSTORWIND1 job 3, sway A): the contract text. All three docs are LF-only.
D = 'E:/Projects/NifskopeWWE-cardfix1/docs/'


def patch(name, pairs):
    p = D + name
    b = open(p, 'rb').read()
    assert b.count(b'\r') == 0
    s = b.decode('utf-8')
    for old, new in pairs:
        assert s.count(old) == 1, (name, old[:60], s.count(old))
        s = s.replace(old, new)
    out = s.encode('utf-8')
    assert out.count(b'\r') == 0
    open(p, 'wb').write(out)
    print('patched', name)


SWAY_A = (
    '### 3.3 `lodm` 2 -- the model\'s OWN sway (2026-09-24, lane CARDFIX1 step 6)\n'
    '\n'
    'bungo, 2026-09-23: *"sway from the tree\'s model\'s own wind weights would be neat"*; RULED 2026-09-24\n'
    '21:1x as **sway A**. A card baked from a model that has at least one tree-animation shape (a\n'
    '`BSLightingShaderProperty` with the vertex-alpha-animation flag -- the same test that makes the mask\n'
    '"tree") writes, per texel,\n'
    '\n'
    '    _n.A = W x h\n'
    '\n'
    '`W` the model\'s own vertex-alpha wind weight at that texel (the only wind input the game\'s tree vertex\n'
    'shader reads; 0 on a shape without the flag, which the game never moves), `h` the linear height up from\n'
    'the view\'s own coverage bottom row. A model with NO tree-animation shape keeps the synthetic\n'
    '`h^2 x (0.35 + 0.65 r)`, byte for byte. The bake\'s sidecar says which: `sway model` or `sway synthetic`.\n'
    '\n'
    'Because `_n.A` then MEANS something else, the payload version moves, on the card family only:\n'
    '\n'
    '| key | on | type | meaning |\n'
    '|---|---|---|---|\n'
    '| `lodm` | card, cardArray | int | **2** when the set (any layer of an array) carries model sway; 1 otherwise, with none of the keys below |\n'
    '| `sway` | card | string | `"model"` -- `_n.A` is `W x h` |\n'
    '| `leafAmplitude`, `leafFrequency` | card | number | the placed base\'s own leaf numbers (STAT DNAM / TREE CNAM); both 0 in the record reads as 1 / 1 |\n'
    '| `array.sway`, `array.leafAmplitude`, `array.leafFrequency` | cardArray | list | one per layer, parallel to `layers`; a synthetic layer says `"synthetic"` and 1 / 1 |\n'
    '\n'
    '* **An old reader refuses a v2 card by name** (`payload is not a lodm 1 object`), which is the point:\n'
    '  it would otherwise draw a real wind weight under the synthetic law\'s assumptions.\n'
    '* **A SOURCE (or any kind outside card, cardArray, aggregate) claiming 2 is refused by name**:\n'
    '  `lodm 2 is the card family\'s version (...); a "<kind>" .lodm is lodm 1`.\n'
    '* The envelope version stays 1.\n'
    '* **Owed:** the aggregate (§3a) composites the model weight texel by texel already (§3a.3), but writes\n'
    '  no `sway` key and stays 1; teaching it v2 is owed with the ring (lodgenaggregate.cpp is not this\n'
    '  lane\'s). FO4CS\'s reader is owed (`docs/LODGEN_IMPOSTOR_SPEC.md`, the owed paragraph).\n'
    '\n')

patch('LODGEN_LODM_FORMAT.md', [
    ('7. `lodm` != 1;\n',
     '7. `lodm` is neither 1 nor 2; and `lodm` 2 on a kind outside the card family (card, cardArray,\n'
     '   aggregate), refused naming the version and the kind (§3.3);\n'),
    ('**The `.lodm` stays version 1.** A reader that does not know `views` on a card\n'
     'refuses the set; nothing is misread. Version 2 is reserved for the sway channel\'s\n'
     'model-authored amplitude (lane CARDFIX1 step 6).\n\n',
     '**A ring alone does not move the version.** A reader that does not know `views` on a card\n'
     'refuses the set; nothing is misread. Version 2 is the model sway (§3.3).\n\n' + SWAY_A),
    ('| `emissiveScale` | both | number[] | one per layer, **parallel to `layers`** |\n',
     '| `emissiveScale` | both | number[] | one per layer, **parallel to `layers`** |\n'
     '| `sway`, `leafAmplitude`, `leafFrequency` | `cardArray`, only when a layer carries model sway (the file is then `lodm` 2) | string[], number[], number[] | one per layer, parallel to `layers` (§3.3) |\n'),
    ('| `lodm != 1` refusal | `lodmfile.cpp:47` | `payload is not a lodm 1 object` |\n',
     '| `lodm` 1 or 2 refusals | `lodmfile.cpp:46-63` | `payload is not a lodm 1 object`, `lodm 2 is the card family\'s version` |\n'),
])

patch('LODGEN_IMPOSTOR_SPEC.md', [
    ('- sway: h² × (0.35 + 0.65 × r), h up from the view\'s own bottom row, r the\n'
     '  radius from the silhouette\'s axis; 0 for rigid objects. The chunk builder\'s\n'
     '  law, applied to the picture.\n',
     '- sway: **sway A** (bungo 2026-09-24 21:1x) on a model with a tree-animation\n'
     '  shape: W × h, W the model\'s own vertex-alpha wind weight (0 on a shape the\n'
     '  game never moves), h linear up from the view\'s own bottom row; the `.lodm`\n'
     '  says `lodm` 2, `sway` "model" (`docs/LODGEN_LODM_FORMAT.md` §3.3). Otherwise\n'
     '  h² × (0.35 + 0.65 × r), r the radius from the silhouette\'s axis; 0 for rigid\n'
     '  objects. The chunk builder\'s law, applied to the picture.\n'),
    ('`2·V` long, view `v` at `[2v]`; (5) a `cardArray` carries the same keys.\n',
     '`2·V` long, view `v` at `[2v]`; (5) a `cardArray` carries the same keys;\n'
     '(6) a card or card array at `lodm` 2 (`sway` "model", CARDFIX1 step 6) holds\n'
     'the model\'s own weight × h in `_n.A`, not the synthetic law: FO4CS refuses it\n'
     'by name until its reader drives the card\'s wind from that weight and the\n'
     '`leafAmplitude` / `leafFrequency` beside it.\n'),
])

patch('LODGEN_CARD_SHEETS.md', [
    ('| normal A (sway) | `h² × (0.35 + 0.65·r)`, `h` up from the view\'s own bottom row, `r` the radius from the silhouette\'s axis; **explicit 0 for rigid objects** |\n',
     '| normal A (sway) | a model with a tree-animation shape: **`W × h`**, `W` its own vertex-alpha wind weight (channel 11 G; 0 on a shape the game never moves), `h` linear up from the view\'s own bottom row -- sway A, `lodm` 2 (CARDFIX1 step 6). Otherwise `h² × (0.35 + 0.65·r)`, `r` the radius from the silhouette\'s axis; **explicit 0 for rigid objects** |\n'),
])
