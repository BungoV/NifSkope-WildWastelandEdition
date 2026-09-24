# Impostor lighting: what was wrong, and the fix (lane IMPOSTORLIGHT1, 2026-09-22)

You asked "Normals issue?" Yes, but the normals were in the wrong space, not
the wrong sign. The card lit a normal in the tree's own (model) space with a
light in camera (view) space: `impostor_oct.frag` (bf6aa749) line ~410,
`abs( dot( normal, normalize( L ) ) )`. The result was lit by the tree's
up-axis instead of its surface. That made the card about half as bright as
the mesh, and it went dark where the mesh is lit.

The fix is in the shader only, with no rebake and no format change:
- the normal is taken into view space before the light touches it;
- the card is then lit by the mesh's own rule: light 0, Oren-Nayar times
  (1 - Fresnel), one-sided.

## 1. Before | after, beside the mesh (cardRes 512, one ordinary view)

![headline](pics/headline_before_after.png)

`pics/headline_before_after.png`

Card brightness / mesh brightness at the bake directions:

| subject | before | after |
|---|---|---|
| blasted maple N4 | 0.475 | 0.953 |
| blasted maple N8 | 0.481 | 0.954 |
| forest maple N4 | 0.541 | 0.966 |
| destroyed tree N4 | 0.513 | 0.984 |
| cliff rock N4 | 0.877 | 0.967 |

## 2. The light walked round the rock: the lit side now turns with it

![side light](pics/sidelight_rock_n4.png)

`pics/sidelight_rock_n4.png` (also `sidelight_blast_n4.png`, `sidelight_dead_n4.png`)

Mean brightness at light angles 0 / 90 / 180 / 270:

| | 0 | 90 | 180 | 270 | follows the mesh? |
|---|---|---|---|---|---|
| mesh | 83 | 119 | 65 | 46 | |
| card before | 130 | 80 | 114 | 81 | r = -0.15 (the old card was brightest where the mesh was dark) |
| card after | 78 | 116 | 57 | 39 | r = 1.00 |

The two trees read r = -0.58 and -0.41 before, and 0.99 after.

## Still owed, from you

- **Gloss.** On a glossy test cube the card's shading range is compressed:
  - it is 18 % too bright on faces turned away from the light;
  - it is 13 % too dark on faces facing the light.

  The reason is that the card ignores the gloss in its sheet, by your
  2026-09-19 ruling of no gloss until the material renderer exists. Letting
  it read its own gloss brings the cube to within about 10 %. Vanilla trees
  and rock are rough, so the five subjects barely move either way. Your call.
- The off-bake silhouettes (holes, the thin maple crown) are the known
  coverage problem. Lighting cannot fix those.
