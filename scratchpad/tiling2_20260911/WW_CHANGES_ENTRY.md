## 2026-09-11 — The tiling pattern, the quadrant grid, and the blur that has no source

bungo looked at the fixed-tiling bake and said *"there's a few issues here …
Yes, you can see the tiling pattern of each texture, which is not good, hard
blend edges also appear in some places, and yeah, it's muddy or blurry
looking"*. Three complaints. All three were measured against **Bethesda's own
22 shipped dim-4 colour sheets** before a line of code was written, so that
"too much" and "too little" mean a number and not an opinion. Two of the three
now have a switch that puts us inside vanilla's own range; the third is
**refused with the evidence**, because vanilla's fine detail does not come from
anything this bake can compute.

**Nothing changed by default.** A bake made with the new exe and no new flags is
the previous exe's bytes exactly — 9 files a tile, 0 differ, on both test tiles,
with the switches' off values also spelled out explicitly. Everything below is
opt-in until bungo says otherwise.

**The repeat, measured.** The landscape texture repeat (341.3333 world units,
the engine's own) is visible in our sheets and is not visible in Bethesda's: **0
of 22** shipped sheets read it above their own null floor, and the worst of the
22 reads an amplitude of **0.264** of 255. Ours read **1.037** and **1.261** —
four times vanilla's worst. The tiling fix of the same morning did not create
that; it made an existing repeat about 1.7x more prominent (the old 2,048-unit
bake read 0.502 and 0.450 on the dimensionless scale).

**`--land-sample footprint|average`** (default `footprint`, the old behaviour).
`average` reads the landscape diffuse's 1x1 mip. A landscape texture ships a
full mip chain down to 1x1 and one repeat IS the whole texture, so that texel is
the **exact average over one repeat** — not a blur of it, the average — and no
term at the repeat's period can enter the sheet at all. The repeat falls to
**0.092**, inside vanilla's law by a factor of 2.9. There is a byte-exact proof
that it is really gone: with `average` the sheet is **identical at
`--land-tiling 341.3333` and at `--land-tiling 2048`**, so the land term no
longer depends on the tiling constant.

**The hard edges were positional, not wide.** Our sheets have FEWER hard edges
than vanilla's (52 and 190 of 6,000 strong edges, against a median of 523) and
they are marginally wider (5.00 and 4.00 texels against a median of 4.12, inside
vanilla's 2.50..6.50 range). What was wrong is WHERE they are: on chunk
(-20,24) all 14 interior quadrant lines — the 2,048-unit grid where the layer
set changes — carried **23.6% more gradient** than the sheet's own average,
where vanilla's worst of 22 carries 10.0%, and 13 of 52 sub-texel edges sat on a
line (25.0% against a 9.2% chance, p = 0.00066). On (-20,20) the same statistic
is inside vanilla's range, which is exactly why bungo said "in some places".

**`--blend-edges off|quadrant`** (default `off`), with **`--blend-margin`**
(default 128 world units = 4 texels, the opacity grid's own spacing).
`quadrant` cross-fades the neighbouring quadrant's composite — its own layer
set, its own opacities, evaluated at the same world point — over that margin
either side of every line, with a quintic ease that is exactly 0.5 ON the line
so both sides meet on one value. The grid statistic falls from 1.236 to
**0.955** and from 1.065 to **0.847**, **below vanilla's own median of 1.041 on
both tiles**, and it costs nothing measurable: local variance -0.9%, spectrum
distance 1.227 -> 1.226, the repeat +1.3%. One limitation by name: a quadrant
line lying on the chunk's own edge is not blended, because the neighbouring
cell's paint is not loaded there — the adjacent chunk does not blend it from its
side either, so no new seam is made, but that one line stays as hard as it is.

**The blur is REFUSED, and here is why that is a result.** Below 4 texels
vanilla carries 22.6% of its variance and we carry 2.7% — a factor of eight.
So: where does vanilla GET it? Ten candidates were correlated against vanilla's
own fine-detail residual, each with its own phase twin beside it as the floor:
the land diffuse at the footprint mip and at one and two mips finer and one
coarser, the exact footprint box average, the fully averaged texture, VCLR, the
slope out of Bethesda's own shipped `_msn`, the best of eight directional
shadings of that slope, and our own sheet. **Every one reads |r| <= 0.006
against floors of the same size.** And the control that makes that mean
something: the same offline model, run against OUR bake instead of vanilla's,
reads **r = +0.79 and +0.70 with the best alignment at exactly zero shift**
(an upside-down copy reads 0.05). The instrument works, the model is right, the
alignment is right — and vanilla's fine detail has **no source in this
composite's operands**. It was not baked from these textures. Nothing is
invented to imitate it, and there is nothing to hand to the grading work
either, because VCLR and the lighting planes read zero too.

**`--land-detail k`** (default 0) exists so the trade is priced rather than
argued: it lerps back k of the footprint sample's departure from the average,
and it buys the repeat back at the same rate. k = 0/0.15/0.25/0.35/0.50 reads a
repeat of 0.092/0.175/0.268/0.366/0.532 for a local variance of
4.84/5.00/5.22/5.58/6.45 against vanilla's 19.81. Vanilla's repeat ceiling is
crossed at **k = 0.246**, by which point the knob has recovered **0.37 of the
14.97** local-variance levels the average bake is missing. The repeat and the
texture's own detail are one signal; you cannot keep one and drop the other.

**One thing to know if a default ever changes.** The chunk colour sheet is
written by the virtual-texture pyramid pass, so both switches are implemented at
both colour sites, and the pyramid copy is **colour only**: roughness,
metalness, emissive, the cover opacities, the normals and the heights are
untouched, which is why `_msn`, `_data`, the `.lodm`, the BTO, the BTR and the
manifest are byte-identical at every setting. Each switch moves exactly the
chunk colour DDS and the two `.lodt` containers that carry its tiles. And if
`--land-sample average` ever becomes the default, the runtime's ring-0 blend
must average too, or ring 0 and ring 1 will differ by exactly the detail the
switch removes.

The whole measurement is in `scratchpad/lane_tiling2_report.md` with three
pictures beside it — `images/cmp_tiling2.png`, `cmp_edges.png`,
`cmp_detail_spectrum.png`, every panel a DDS file off disk with its numbers
burned in — and the colour law's new section is `docs/LODGEN_TERRAIN_VT.md`
§2.5a.
