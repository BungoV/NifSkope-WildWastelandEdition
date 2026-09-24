# Lane PBRR4 -- PBR renderer stage R4: tint mask + emission + opacity (BUILD lane)

Director brief, 2026-09-24 12:2x. Model: Opus 5.5. Folder: scratchpad/pbrr4_20260924/ ; progress.md INCREMENTALLY.
Chain rules: scratchpad/brief_pbrchain.md. Skill: nifskope-ww-pbr-shade-ab. PRECHECK: E: >= 2 GB free.

## Read first
docs/NIFSKOPE_PBR_RENDERER.md row R4 (~743), item 7 (tint masks, ~273) and the emission/opacity items; RULINGS.
PBRR3 lane text (scratchpad/pbrr3_20260924/DELIVERABLE_TEXT.md): PBR display is now ON by default.

## Scope = the R4 row
TINT MASK (bungo: "4 channels R+G+B+A for tinting base color, each channel a mask that fills one area"): port the
editor EXACTLY -- E:\Projects\Fo4CommunityShaders\PBRMaterialEditorQt\src\materialpreviewwidget.cpp:2310-2316 (layer
values :734-735, uniforms :1695): masks R/G/B/A each with its own colour; overlap modes Normalize (default; sum>1 ->
divide) / Add / Priority R>G>B>A; tint = max(0, (1 - sum m) + sum c_i m_i); base *= tint. Tint colours raw sRGB like
the editor (Q13 inconsistency stays filed, not fixed). FO4CS is porting the same law in parallel (lane TINTMASK1) --
same fixture values welcome.
EMISSION: luminance/100, replace semantics; textured emission ignores the constant colour unless overridden.
OPACITY composition per the doc.

## Gates = the R4 row
2x2 tint-mask fixture: each quadrant = base x law (numpy) +-2/255, an overlap texel follows Normalize, plus Add and
Priority fixtures; emission 100 nits -> linear 1.00 pre-exposure (tonemap None); textured-emission rule; each with a
red control. Legacy zero set + R1/R2a/R2b/R3 gates still PASS.
Rung before_pbrr4 from db5ccaf4. DONE.md + DELIVERABLE_TEXT.md as in the chain rules.
