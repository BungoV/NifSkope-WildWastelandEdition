## 2026-09-16 — A terrain texel depends on its world position only: the pyramid sheet and the direct bake agree byte for byte on the ruled land default

Under the land look ruled on 2026-09-12 — hex 256, warp amplitude 341, mip bias
−0.22, guide `flatwarp:1.0` — a terrain colour sheet assembled from pyramid
tiles differed from a direct chunk bake of the same ground by **4 bytes on
`Commonwealth.4.-24.28` and 27 on `Commonwealth.4.-20.28`**, out of 174,888.
The other two chunks of the probe region were identical. Every differing byte
was a BC1 selector: the two paths agreed on which colours were in a block and
disagreed on which of them 33 texels took. The check that noticed it, V9a-1 of
`tests/spells/lodgen_terrain_vt.sh`, had been parked on the four old land
switches since; it is now back on the shipped look.

**The cause.** `lodgenTerrainFillRing` builds the ring height grid both sheet
writers read. Where two Bethesda cells both carry a shared VHGT row, bungo's
2026-09-10 rule — the cell owns its own rows — applies only inside the fill's
INNER UNIT; outside it, in the ring, the fill is plain later-wins and the north
or east cell's copy stands. The inner unit was derived from the caller's own
`rdim`, so it was the dim-4 chunk for the chunk baker and the dim-2 tile for the
tile baker. The same world sample therefore came out as one cell's copy in one
grid and the neighbour's in the other — up to **64 world units apart**, on
Bethesda's own y=31|32 cell line, 31 samples a tile. That would be invisible if
nothing read the ring, but the macro slope does: it is a Sobel at ±512 world
units over exactly those samples, and under the ruled default the warp amplitude
is `341 × (1 − min(1, slope / 0.5))`. A moved ring sample moved the amplitude,
the amplitude moved the land lookup, and a handful of texels crossed a BC1
selector. `--land-warp 0` zeroed the amplitude and `--land-guide off` skipped
computing the gradient at all, which is why both hid it, and why hex and mip
bias — which never read the height grid — were innocent.

**The fix.** The inner unit is now a world-anchored box the caller may name.
The tile baker names the box of the CHUNK it will be assembled into,
`lodgenVtFloorTo( cellX0, 2 × dim )` on both axes; every pyramid level shares
one origin aligned to the coarsest dim, so that chunk is fixed with no option,
no level index and no region rectangle in it. The chunk baker passes nothing and
keeps the derived box, bit for bit. **The direct bake writes exactly the bytes it
wrote before** — measured, all 12 files of the probe region identical between
the rung and the new exe — so no ruled default moved and nothing the gate
compares against moved.

The rule was modelled offline against the dumped Commonwealth VHGT before a line
of it was compiled: 108 disagreeing height samples over the four probe chunks
went to 0, with the old rule kept beside it as the refuter.

**Measured, rung against new, same region, bare default, `--road-detail 1`:**

| chunk | rung: assembled vs direct | new: assembled vs direct | direct: rung vs new |
|---|---|---|---|
| `Commonwealth.4.-24.24` | 0 | 0 | 0 |
| `Commonwealth.4.-20.24` | 0 | 0 | 0 |
| `Commonwealth.4.-24.28` | **4** | **0** | 0 |
| `Commonwealth.4.-20.28` | **27** | **0** | 0 |

What ships changes with it: the `.lodt` containers move by 435 bytes of
2,231,776 at level 2, 101 of 560,608 at level 4 and 52 of 142,816 at level 8,
and the two `_data` sheets by 20 and 47 bytes. The `_msn` sheets do not move on
either path. Picture:
`scratchpad/vt1_20260912/images/vt1_pyramid_sheet_rung_vs_new.png`.

**The gate.** `tests/spells/lodgen_terrain_vt.sh` now bakes on the bare ruled
default, and keeps a second arm that spells the four old land switches; both
must pass, with a floor under them that asserts the two arms really are
different bakes on at least 3 of the 4 chunks, so a fix that worked by reverting
the look would fail rather than pass twice. `lodgenTerrainRingSelfTest` gained
the case as PART TWO — a synthetic grid where the two cells disagree on purpose,
with a control that the derived box and the chunk box must give DIFFERENT
answers there, and a check that naming the derived box explicitly changes not one
sample. It runs inside every bake under `WW_TERRAIN_RING_TEST=1` and the suite
now greps its verdict: 13 checks, 0 failures.

Counts beside the exe (`release/NifSkope.exe`, 22,293,504 bytes, 2026-09-16
12:49:19): `lodgen_terrain_vt.sh` 44 checks, 1 failure (V9c, and only V9c);
`lodgen_terrain.sh` 26 checks, 0 failures; `lodgen_native_baseline --check` 25
files, 0 differ. `lodgen_defaults.sh` did not run — it needs a pre-2026-09-12
rung exe that is not in the tree — and `tests/spells/lodt_write.sh` does not
exist; both are named as skips rather than counted.

**V9c stays red and is not this lane's.** It reads the `_msn` sheets of the
direct bake, which are byte-identical between the rung and the new exe, and
identical again under the old land switches, under `--land-warp 0` and under
`--land-guide off` — five bakes, one set of numbers. Its cause was measured
instead: the check decodes a **DXT5** sheet (349,680 bytes, 10 mips) with a DXT1
reader, which takes every alpha block for a colour block and leaves four fully
white columns in every eight, exactly 50% of the sheet; and its bars were pinned
in 2026-09-09 on our generated normals, while all four of these chunks now get
**vanilla's own `_msn` file, copied byte for byte**, by the rule at
`lodgen.cpp:6680`. Decoded as DXT5, the E/W seam ratio the check calls 14.20
against a bar of 3.20 is **0.99**, and the N/S is 1.31 against 3.30: there is no
seam discontinuity in these sheets. Fixing the reader and re-pinning the bars is
bungo's call and nothing was landed for it.

Doc: `docs/LODGEN_TERRAIN_VT.md` gains section 2.5j, THE RULE — a texel depends
on its WORLD POSITION only — beside the sampler, with what broke it, what the
fix is, and the note that the older "seamless and deterministic" paragraph
measured one writer twice and so could not see this.
