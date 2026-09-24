# Lane PBRR3 -- PBR renderer stage R3: BRDF + v6 specular; PBR display turns ON (BUILD lane)

Director brief, 2026-09-24 11:2x. Model: Opus 5.5. Folder: scratchpad/pbrr3_20260924/ ; progress.md INCREMENTALLY.
Chain rules: scratchpad/brief_pbrchain.md. Skills: nifskope-ww-pbr-shade-ab, the render-shot skill.
PRECHECK: free space on E: >= 2 GB, else DONE.md "PENDING disk".

## Read first
docs/NIFSKOPE_PBR_RENDERER.md: row R3 (~742), the numbered shading items (~250-290: decode fixes, F0/F82, DFG +
multiscatter, energy split Q6 = the FO4CS law, EON), the RULINGS. Lane text of PBRR2A + PBRR2B.
The v6 specular reference: E:\Projects\Fo4CommunityShaders\PBRMaterialEditorQt\docs\PBRM-v6-Specular.md and PBRM-v6.md
(specular weight, specular colour = tint over the IOR F0 of dielectrics and the F82 edge tint of metals, specular IOR
constant or A-channel map over [0, iorMax]); FO4CS wave 89 (SPECV6) is the runtime twin -- match it.

## Scope = the R3 row
`evalSurface`; decode fixes; v6 F0 + F82; Lazarov DFG + multiscatter; energy split (Q6, the FO4CS law); EON diffuse.
Specular weight / colour / IOR must visibly drive the result (bungo will test them in NifSkope and in game).
WHEN ALL R3 GATES PASS: the PBR display default flips ON (Q9) -- a .pbrm-backed shape renders PBR by default; the
Scene window Mode row still offers Legacy. Say in the lane text exactly what flipped.

## Gates = the R3 row, plus
White furnace (metal 1 at rough 0.1/0.5/1.0 -> 1.00 +-0.02 centre and 60 deg; fails without multiscatter at rough 1;
dielectric F0 0.04 -> <= 1.02, fails without the split); v5/v6 twins `zero`; each with its red control.
PLUS specular: (s1) weight 0 -> specular lobe gone (dielectric = diffuse only, measured), (s2) IOR 1.5 vs 2.0 -> F0 0.040
vs 0.111 read back, (s3) specular colour red on a dielectric -> the reflection is red, the diffuse is not; each with a red.
Legacy mode still `zero` on the pbr_shade_ab set.

## Side jobs (small, owed by PBRR2B)
- Ambient direction: PBRR2B ASSUMED the sky's Z- colour lights upward faces. Vanilla research = Todd's treat first: read
  Sky::SetDirectionalAmbientBlend (RVA 0x652F30, 1.10.155 -- quote the build) with the Todd's treat tooling
  (kept outside this repo). Fix if wrong, with a gate.
- docs/CLI.md: add the `weather` command section (match the source).
- tests/spells/pbr_r2a_gates.sh: the relative-output-folder trap PBRR2B fixed in its own driver -- fix it the same way.
Rung before_pbrr3 from b6c79569. DONE.md + DELIVERABLE_TEXT.md as in the chain rules.
