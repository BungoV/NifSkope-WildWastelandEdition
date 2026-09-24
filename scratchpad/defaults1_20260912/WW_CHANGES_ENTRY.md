## 2026-09-12 — The LOD defaults follow bungo's rulings: both identity channels off, his land look, the verge is not road

Four defaults moved, and nothing else. They are bungo's rulings of 2026-09-12,
and each one is reachable back at its old value by one switch.

**Object identity is OFF by default.** `--identity` is the opt-in; `--no-identity`
still turns it off. With it off a `.BTO` carries the plain object vertex
descriptor `474989027590661` — no vertex colours, no UV 2, no Eye Data, no tree
sway in alpha — which is the layout vanilla's own object LOD uses. bungo, 15:56:
"Legacy terrain bakes stay as they were, no extra data for FO4CS to be included
in them. Only the .lod ones have new data in them."

**Terrain identity is OFF by default.** `--terrain-identity` is the opt-in. A
generated `.BTR` now carries vanilla's land descriptor `52776558133763` with
neutral vertex colours, so the blue-purple cast that came from writing material
class, wetness, AO and shore proximity into the colour slot is gone unless you
ask for it. bungo, 15:53: "We don't bake BTR for FO4CS, and so we do not use of
that data for it at all."

**The land sampling default is the look he picked**, panel (c) of LAND1's
`a_land_guide_*.png`: hex tile size **256**, warp amplitude **341**, mip bias
**-0.22**, guide rule **flat warp** at strength 1. The warp lattice (1024), the
octave count (1), the guide scale and the guide slope did not move, and the base
sample rule is still FOOTPRINT — `--land-sample average` stays rejected. The way
back is exact and byte-identical:
`--land-hex 0 --land-warp 0 --land-mip-bias 0 --land-guide off`.

**`--road-ground-paint` defaults to 0.** The `materials/Landscape/Ground/`
shapes that ship inside the road NIFs — the grass planes beside the kerb — are
no longer painted into the road plane. bungo, 16:55: "the grass meshes included
with the road nifs" are excluded. `--road-opacity` and `--road-detail` did not
move. The way back is `--road-ground-paint 1`.

**The sidecars and the far rings no longer depend on the identity flag.** Until
today `--no-identity` suppressed the whole `<chunk>.bto.manifest.txt`, and with
it the texture arrays, the impostor card placements and the native lighting
data that read it. The manifest is a SIDECAR — it is not inside the `.BTO`, so
writing it does not put "extra data" in a legacy file — and it is now written
either way, as are the `--arrays` sheets and the impostor cards, and the native
`.lodo` / `.lodi` / `.lodt` / `.lodl` files carry exactly the same bytes with
identity on or off. Per-VERTEX identity (the R+G object index, AO in B, sway in
A, the layer in UV2.y) is still what the flag switches.

**The panel follows.** Both identity boxes start unticked, Hex tile size 256,
Warp amplitude 341, Mip bias -0.22, Guide rule "Flat warp", Verge painted as
road 0. No row was added or removed. A saved panel whose value is still exactly
the old default is moved to the new one once, guarded by
`LodGeneration/defaults1Applied`; a number you set yourself is not the old
default and is left alone.

**The panel self-test follows too.** One of its 121 checks measured the style of
a TICKED check box and had been borrowing the identity box's state; it now ticks
the box itself and puts it back, which is what a harness is supposed to do. A
saved panel whose five land/road rows still held the old defaults is migrated
once, so the panel and the command line bake the same thing.

**New gate:** `tests/spells/lodgen_defaults.sh` — five phases against the
previous exe: the new defaults equal the old exe with the switches spelled out,
the way back equals the old exe's defaults, every `.BTR`/`.BTO` of a default
bake at dim 4/8/16/32 reads vanilla's descriptor, the arrays and cards still
place with identity off, and the native files do not move.
