p = 'docs/LODGEN_TERRAIN_VT.md'
s = open(p, 'r', encoding='utf-8', newline='').read()


def rep(a, b, n=1):
    global s
    assert s.count(a) == n, (a[:80], s.count(a))
    s = s.replace(a, b)


# ---- 3.4 reader rules ----------------------------------------------------
rep("""3. `version != 1`""",
    """3. `version != 2`. **A version 1 file is named for what it is**, not lumped
   into "bad version": its third sheet is role 3 `data` — R sky AO, G flow
   wetness, B shore proximity, A ground cover — and three of those four channels
   no longer exist. There is no converter and nothing to convert; re-bake""")

rep("""13. `sheetCount` outside 1..4; a used sheet with `role == 0` or a duplicated role; a role-1/2/3 sheet whose format is not one of {71, 72, 77, 78} or a role-4 sheet whose format is not 56; `colorSpace > 1`; `dxgiFormatCover != dxgiFormat` on a sheet whose role is not 3; a sheet past `sheetCount` that is not all zero""",
    """13. `sheetCount` outside 1..6; a used sheet carrying the **retired role 3**
    (refused by name, saying what role 3 used to mean); a used sheet with
    `role == 0` or `role > 6`; a duplicated role; a **missing colour, msn or
    mask** sheet — a pyramid without one of those three is a broken bake, not a
    cheaper one, and a reader that discovers the absence at sample time cannot
    say so; a role-1/2/5/6 sheet whose format is not one of {71, 72, 77, 78} or a
    role-4 sheet whose format is not 56; `colorSpace > 1`; **more than one sheet
    declaring `dxgiFormatCover != dxgiFormat`**, or such a sheet whose role is
    neither 5 (mask) nor 1 (colour) — exactly one sheet carries the ground-cover
    alpha and the header says which by declaring the pair; a sheet past
    `sheetCount` that is not all zero""")

# ---- 4 the index ---------------------------------------------------------
rep("""No parser change was needed. The envelope stays version 1, `lodm` stays 1, and
`family` is `"legacy"` because the parser hard-rejects a third family word and a
new one would split the corpus. **`family` is vestigial for
`kind: "terrainVT"`; `kind` is the discriminator**, and `src/io/lodmfile.h` says
so, which is where that collision gets resolved rather than discovered.""",
    """No parser change was needed. The envelope stays version 1 and `lodm` stays 1.
**`family` is `"pbr"` since 2026-09-11 and it is no longer vestigial**: the
sheets ARE the object family's, so the word describes them. A legacy source is
converted at bake — gloss inverted into roughness, metallic 0 — and
`terrain.maskRules` below counts how many landscape textures came by which road,
so the word is auditable rather than asserted. `kind` remains the discriminator
for which payload object to read.""")

rep("""      { "role": "data",   "dxgi": 71, "dxgiWithCover": 77, "colorSpace": "linear",
        "channels": "R sky-free AO, G flow wetness, B shore proximity, A ground cover" },
      { "role": "height", "dxgi": 56, "dxgiWithCover": 56, "colorSpace": "linear",
        "channels": "R16_UNORM, height/8 + 32767, the shadow heightmap's own encoding" }
    ],""",
    """      { "role": "mask",   "dxgi": 71, "dxgiWithCover": 77, "colorSpace": "linear",
        "channels": "rmaos: R roughness, G metallic, B sky-free AO, A ground cover" },
      { "role": "height", "dxgi": 56, "dxgiWithCover": 56, "colorSpace": "linear",
        "channels": "R16_UNORM, height/8 + 32767, the shadow heightmap's own encoding" }
    ],
    "emissive": "none",
    "dropped": {
      "shoreProximity": "runtime: subtract the .lodl water body plane from the height at the sample",
      "wetness": "not baked: a close-up effect; far wetness is a weather state the runtime owns"
    },
    "maskRules": { "pbrm": 0, "legacyInverted": 14, "noneDefault": 0,
                   "withRoughnessMap": 14, "withMetallicMap": 0, "withEmissiveMap": 0,
                   "distinctLtex": 14, "roughnessDefault": 1.0, "metallicDefault": 0.0 },""")

rep('  "lodm": 1, "family": "legacy", "kind": "terrainVT",',
    '  "lodm": 1, "family": "pbr", "kind": "terrainVT",')

rep("""`worldUnitsPerTile` and `contentTexels` are **stated per level, not implied**:
a consumer picking clipmap rings reads those two numbers at load time rather
than deriving them from `dim` and a constant it has to know.""",
    """`worldUnitsPerTile` and `contentTexels` are **stated per level, not implied**:
a consumer picking clipmap rings reads those two numbers at load time rather
than deriving them from `dim` and a constant it has to know.

**`emissive` is `"none"` or `"present"`, in words.** A consumer must be able to
tell "this worldspace emits nothing" from "the writer forgot", and a black sheet
says neither. **`dropped`** names what version 1 carried and version 2 does not,
and where to get it instead, so a reader looking for a channel that was removed
on purpose finds the answer rather than a gap. **`maskRules`** is the per-layer
rule census: `pbrm + legacyInverted + noneDefault == distinctLtex`, and a
worldspace served entirely by `noneDefault` is a pyramid whose roughness is the
1.0 floor everywhere — which the number says out loud instead of shipping as a
measurement. The numbers above are the ones measured on the Sanctuary region
(cells −20..−17 x 24..27) on 2026-09-11.""")

# ---- 5 the CLI -----------------------------------------------------------
rep("""| `--vt-mips N` | 2 | stored mips per tile |""",
    """| `--vt-mips N` | 2 | stored mips per tile |
| `--vt-cover-in-color` / `--vt-cover-in-mask` | mask | which sheet's alpha carries the ground cover (§2.2a). The two cost the same bytes, measured; the default is the mask because the colour sheet's alpha is the object family's OPACITY slot |""")

# ---- 6 what is deliberately not done -------------------------------------
rep("""* **No water filter on cover.** §1.3; the B channel already carries shore
  proximity.""",
    """* **No water filter on cover.** §1.3. Version 1 pointed at the data sheet's B
  channel for shore proximity; that channel is gone, and a consumer that wants to
  suppress cover near water subtracts the `.lodl`'s water body plane from the
  height at the sample instead.
* **No shore proximity and no baked wetness.** Both dropped by bungo's ruling of
  2026-09-11 09:5x. Shore is a runtime subtraction the `.lodl` already supports;
  wetness is a close-up effect at a distance nobody views this pyramid from, and
  far wetness is a weather state. Neither is deprecated-but-written: the channels
  do not exist.
* **The object bake does not yet consume the shared mask resolver.** The law
  lives in one place (`lodgenResolveMaterialMask`) and the GLOSS is genuinely
  shared code (`lodgenLegacyGloss`, called by the arrays pass and inverted here),
  but the object path still takes its PBR answer from a `.lodm` sidecar rather
  than from a `.pbrm`. Wiring it is a separate lane; until then a model whose
  material is a PBRM bakes its objects legacy and its terrain PBR.
* **No PBRM has been exercised end to end on terrain.** The vanilla corpus
  contains none: 14 of 14 landscape textures on the Sanctuary region resolved
  `legacy-inverted`, 0 `pbrm`. The PBRM arm is gated on a fixture, not on
  shipped data, and that is stated rather than implied by a green suite.""")

open(p, 'w', encoding='utf-8', newline='').write(s)
b = open(p, 'rb').read()
print('CR', b.count(b'\r'), 'LF', b.count(b'\n'))
