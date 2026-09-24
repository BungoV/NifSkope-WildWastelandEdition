## HANDOFF text

PBRR4 (R4 row: tint mask, emission, opacity composition, plus two director additions) -- built, gated, NOT committed. Exe release/NifSkope.exe sha1 b6d37f73365514ca75cae8af8d71faf1158d563f (built 2026-09-24 12:43:47, shaders copied at link). Rung release/before_pbrr4 = db5ccaf4.
- Tint mask: the editor's law ported verbatim (ED:2310-2316) -- four masks R/G/B/A, each with its own raw-sRGB colour (Q13), overlap Normalize (default) / Add / Priority R>G>B>A. Slot "primaryTintMask" read from the .pbrm (overrideMask<c>, legacy use<c> read as !use).
- Emission: luminance/100 (100 nits = linear 1.00 before exposure); a textured emission replaces the constant colour/mask unless that channel is overridden (FO4CS PBRM.cpp:896-897). Legacy "intensity" still read as is.
- Opacity composition from the .pbrm (Transparency/Composition + Depth): Opaque, Alpha Test (discard in shader), Alpha Blend, Premultiplied, Additive, Multiply with the editor's blend functions (ED:5447-5450); it replaces the NIF alpha property for PBR shapes (as FO4CS PBRM.cpp:679-705 does).
- Specular weight now follows OpenPBR (IOR remap): F0' = clamp(weight x tint x F0(ior)), F90' = saturate(50 F0'); metals scale by weight.
- Diffuse matches the game (Burley): the frag already carried FO4CS's Burley since PBRR3 (wt-spec1/F4FX/Lighting/truepbr_brdf.hlsli:57-66, 1 - F keep :74-83); EON kept when diffuseRoughness > 0. New gate d1 plus a Lambert red now prove it.
- Gates: tests/spells/pbr_r4_gates.sh (tint, emission, comp, s1b, d1), 7 reds all fail their target. R1 48/48, R2a, R2b, R3 (+6 reds), zero set 10/10 vs before_pbrr4 all PASS on the new exe.
- OWED: FO4CS reads "intensity", not "luminance", and defaults its overrides to true -- FO4CS reader must follow (FO4CS built last); constant base colour not decoded (editor uses pow 2.2) -- untouched here; emission constant uses pow 2.2 while the map uses the sRGB format decode; blended draws are not re-sorted back-to-front; twoSided is read but not applied; for PBR shapes the NIF alpha property and lsp->alpha are ignored; the tint fixture is 5 regions (top-left split into pure R + overlap) rather than a pure 2x2.
- bungo's open NifSkope window (pid 7644, inuse copy) needs a restart to pick this up once it lands.

## WW_CHANGES text

### PBR renderer R4 -- tint masks, emission in nits, opacity modes (lane PBRR4, 2026-09-24)
- Tint masks render: up to four mask channels, each with its own colour, blended the way the PBR Material Editor blends them (Normalize, Add or Priority).
- Emission is now in nits: 100 nits shows as full white before exposure. An emission texture replaces the constant colour unless the material overrides it.
- The .pbrm's transparency settings now drive the viewport: Opaque, Alpha Test, Alpha Blend, Premultiplied, Additive and Multiply, with its own depth-write switch.
- Specular weight now follows OpenPBR (IOR remap): lowering the weight dims head-on reflection in proportion but keeps the grazing edge bright.
- Diffuse matches the game (Burley), now proven by its own gate.
- New gate script tests/spells/pbr_r4_gates.sh with fixtures tests/spells/pbr_r4_fixtures.py; every gate has a red control.

## MISTAKES text

- 2026-09-24 PBRR4: the composition judge measured the background as "everything not the plane", which swept in the plane's filtered rim (std 8 against a limit of 2) and failed a correct Alpha Test picture. Fix: sample the background in UV space clear of the plane (outside [-0.05, 1.05]). Rule: a "background" sample for a uniformity check must exclude a margin around the object, never just the object's mask.
- 2026-09-24 PBRR4: a Python edit sent through a bash heredoc died with "unexpected EOF" on quoting. Rule (already in nifskope-ww-build-verify): multi-line edit scripts go through the Write tool, then run.

## Skill review

- nifskope-ww-build-verify: worked as written (make's own RC, no rename needed because bungo's window runs the inuse copy). No change.
- The PBR gate family (pbr_r1..r3 gates, pbr_shade_ab) has no single skill that lists them; nifskope-ww-render-shot covers the hook only. Proposal: extend the PBR shade skill (nifskope-ww-pbr-shade-ab, or create it) with the R4 harness -- the gate order R4 -> R3 (+reds) -> R1 -> R2a -> R2b -> zero set, the WW_R4_RED / WW_R3_RED pins, the WW_R3_TERM=fresnel|diffuse views, and the background-margin rule above.
- ww-analytic-fixture-gate: fitted the tint/emission gates (numpy law, red control). No change.
