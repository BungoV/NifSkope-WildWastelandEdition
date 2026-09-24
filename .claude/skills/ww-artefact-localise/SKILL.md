---
name: ww-artefact-localise
description: Localise a REPEATING visual artefact bungo saw in a NifSkope Wild Wasteland render — squares, bands, creases, a lattice — to the sheet, the mesh, the encoder or the renderer, and to a period, before anything in src/ is touched. The substitution matrix that separates the four, the phase-conditional statistic that sees a grid-locked artefact where a spectral comb cannot, the uncompressed and constant-sheet controls, and the look-at shift that calibrates screen pixels per texel. Use whenever "ours looks wrong and vanilla does not" and the wrongness is periodic.
---

# Localise a repeating artefact before fixing it

Repo `E:\Projects\NifskopeWildWastelandEdition`. Read `CONSTITUTION.md` (rule 4
measurement, rule 5 proof by picture) and `ww-control-calibration` first. The
render mechanics are `nifskope-ww-render-shot`; staging both sides is section 2
of `nifskope-ww-vanilla-compare`; this skill is the diagnosis in between.

Written 2026-09-09 by lane LATTICE, after bungo's *"You can see the square
pattern on the right in the terrain, which is not good."* Two things cost that
lane rounds and both are avoidable: the first metric it reached for could not
see the artefact at all, and it edited `src/` before it had shown, in a
picture, that the candidate fix removed what he was looking at. It did not.

## 1. Localise BEFORE you measure the data

A render contains a mesh, a material, several sheets, a tangent basis, mips, a
compressor and perspective. Do not reason about which one it is — **substitute
and photograph**. One pinned camera (`WW_RENDER_VIEW`/`CENTER`/`DIST`/`SIZE`),
one crop, and each variant staged as its own miniature data root, so a variant
cannot borrow another's files.

The matrix for a sheet-vs-mesh question is four frames:

| | ours' sheet | vanilla's sheet |
|---|---|---|
| **ours' mesh** | A | B |
| **vanilla's mesh** | C | D |

Hold everything else fixed — use ONE side's diffuse in all four, so only the
mesh and the sheet vary. Then: A and C show it, B and D do not => the sheet.
A and B show it, C and D do not => the mesh. All four => the renderer.

Two more variants answer the next question, and both need only a DDS writer
(`scratchpad/mountains_20260907/ddswrite.py`, 32-bit A8R8G8B8 with a mip chain
box-filtered and RENORMALISED on the normals, not on the bytes):

* **the same sheet UNCOMPRESSED** — if the artefact goes, it is the block
  encoder;
* **a CONSTANT sheet** — if the artefact is still there, it is the mesh or the
  renderer and no amount of generator work will touch it. (Section 7 of
  `nifskope-ww-vanilla-compare` has the BC1 constant-sheet recipe; the
  uncompressed writer is the one this skill adds.)

Controls that make the matrix honest: re-stage the shipped case as variant A
and check its render is byte-size identical to the frame already delivered, and
md5 the files that are supposed to be the same on both sides. Lane LATTICE's
regeneration after the fix had a byte-identical `.BTR` and diffuse — the only
proof that the picture shows the sheet change and nothing else.

## 2. Pick a statistic that can SEE the artefact

**A spectral comb is usually the wrong instrument, and it fails silently.**
A crease at every grid line whose STRENGTH follows the content is a train of
impulses with independent amplitudes, and that has a FLAT spectrum — no comb.
Measured: a known-blocky sheet read comb prominence **0.19** at its own period,
*below* a broadband field, because a zero-order hold has spectral zeros exactly
at the comb bins. A metric that reads a known positive as negative has to be
put down, not argued with.

The instrument that works for anything locked to a known grid of period `p` is
**phase-conditional** — the shape of the x-mod-4 table in WW_CHANGES
2026-09-07:

```python
d2   = abs(F[:, 2:] - 2*F[:, 1:-1] + F[:, :-2])   # local roughness, per column
c    = d2.mean(axis=0)
MOD  = (max over residue classes of x mod p - min) / mean
```

on the field the artefact lives in (for an `_msn`, the slope field
`P = -n_east/n_up`, `R = n_north/n_up`, minus its own 9x9 box mean, edges
cropped). It is scale-free, so it does not reward a sheet for having more
energy — which matters when ours has 6x less than vanilla's. **Which residue
classes are raised tells you where the artefact sits**: a sample line at texel
`4k - 0.5` raises classes 3 and 0 and no others.

Prove it before using it: a fractal field reads ~0.005, the same field creased
every p-th row and column with random amplitude reads ~0.22.

## 3. Separate the candidates with a SYNTHETIC 2x2

Real data has everything in it at once. Build the smallest synthetic field that
has the property under test, put it through the same code path, and vary one
thing at a time. Lane LATTICE settled "is it the 8-unit VHGT staircase or the
reconstruction?" in one table on a smooth analytic height field:

| heights | basis | MOD |
|---|---|---|
| exact | bilinear | 1.254 |
| quantised to 8 units | bilinear | 1.254 |
| exact | eased | 0.217 |
| quantised to 8 units | eased | 0.218 |

The quantisation moves it by 0.000; the reconstruction by 1.04. That is a
verdict, and it took no build.

## 4. Calibrate screen pixels per texel — do not estimate it

Do not convert a screen period into texels by eye or by dividing the bounding
box. Render the SAME staged file twice with `WW_RENDER_CENTER` differing by a
known number of world units, cross-correlate one patch across a shift range,
and read the peak. Lane LATTICE's guess was 2.5 px/unit-of-texel and the
measurement was 1.25 — a factor of two, on which a whole period identification
turned.

The orbit camera moves with its pivot, so the shift is `f*delta/depth` and
differs across the frame: calibrate in the SAME patch you are measuring.

## 5. Prove the candidate fix in a PICTURE before touching src/

This is the rule the lane broke. A sheet-domain number falling is not the
artefact going: the number can fall 78% while the picture barely moves, because
a second cause was in the frame the whole time. With the DDS writer from
section 1 you can render the proposed output before it exists in the generator
— write the candidate sheet from the offline replica, stage it, shoot it, and
compare against the shipped frame with the same crop.

Only when that picture moves does the edit earn its build.

## 6. What to say when it is only partly fixed

Name both causes, give each its number, and do not let the fixed one stand for
the whole. CONSTITUTION 9: no "fixed" before bungo confirms live, and every
unvalidated thing is named in the same breath as what shipped. A before/after
picture whose caption says "reduced, not gone" and why is worth more than one
that quietly implies otherwise.

## 7. Two facts about this renderer that will bite

* **NifSkope discards a terrain `_msn`'s stored blue** and recomputes it as
  `sqrt(max(1 - dot(normal.rg, normal.rg), 0))`, then multiplies by
  `btnMatrix_norm` — it treats FO4's MODEL-space sheet as tangent-space, and it
  does this even though the LAND shader flags set `SLSF1_Model_Space_Normals`
  (bit 12 of 2151682048). Two sheets differing only in blue render identically,
  and a change to the north channel cannot be shown in a NifSkope render.
* **FO4 terrain LOD `.BTR` carries no vertex normals** (Vertex Desc
  52776558133763 = VERTEX + UVs only), ours and vanilla's alike, so no
  per-triangle shading difference can come from the mesh. Check the descriptor
  before blaming a tangent basis.
