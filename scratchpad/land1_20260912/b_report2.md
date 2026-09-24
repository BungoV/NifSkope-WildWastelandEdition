
## B6 The picture

`scratchpad/land1_20260912/images/b_incremental.png` (2004 x 854), drawn by
`b_pics.py` from the gate's own tree -- it bakes nothing.

**Top row, five panels, each a real `.DDS` off disk** for Commonwealth chunk
(-20,20) dim 4, the chunk gate arm `A/land` moved:

| panel | what it shows |
|---|---|
| `base` | the region as it was, sha1 `54201bfe79a2fa305c8a5ea0` |
| `full bake` | the edited plugin, all 25 chunks rebaked, sha1 `217cc48bfecd3a81e9583434` |
| `\|full - base\| x8` | **the floor** -- not black. The edit reached the output: max delta 20 of 255 |
| `dirty rebake` | the edited plugin, 9 chunks of 25, sha1 `217cc48bfecd3a81e9583434` -- **the same sha1 as the full bake** |
| `\|incr - full\| x8` | **the promise** -- black, `max delta = 0 of 255` |

The x8 gain on both difference panels is the point of them: a difference too
small to see at 1x would still be a difference, so the gain is the honest way to
look at one. The picture is evidence; the sha1s printed under the panels and the
tree-wide byte comparison in B3 are the proof.

**Bottom row: the dirty set of all eight arms**, 5x5 chunk grids. Bright green =
the chunk whose input actually moved (or whose output was deleted); dim green =
dirtied by the one-chunk widening; grey = left exactly as the previous bake wrote
it.

Those grids are **computed, not illustrated, and they are a second check on the
diff.** The first draft shaded `dirty` cells outward from the centre and called
the shading illustrative -- which was the wrong call, because a reader looks at
the picture, not at the disclaimer, and a diamond of nine chunks is not what a
one-cell edit dirties. The grids now come from the seed chunks the exe itself
printed, widened by the rule in `nifcli.cpp:3791`, and the script **refuses to
draw** if its computed set does not have the size the exe reported. It drew on
all eight arms, so the widening rule and the exe's own count agree independently.

Part A's two pictures are unchanged: `images/a_land_guide_flat.png` and
`images/a_land_guide_slope.png`. All three were baked with `--road-detail 1`.

---

## B7 What is red, and what was not measured

**Red: nothing in Part B.** B2 5/5, B3 8/8, B4 0 failures. The inherited red
(`lodgen_roads.sh` R5) was cleared earlier in the lane and the suite reads 11
checks / 0 failures.

Not measured, each named rather than left to inference:

1. **The `assets` row of the dependency map has no gate arm.** Rows 4, 6 and 7 --
   an `LTEX` texture set's bytes, a ref base's LOD `.nif` bytes, a `SCOL` part's
   models -- are digested through `lodgenReadAsset()`, and the LTEX row was added
   this session after reasoning about what a loose-file override could reach.
   **None of them was exercised by an edit.** An arm that drops an overriding
   loose `.dds` or `.nif` into a resource folder and re-runs would close this,
   and it is the single most valuable arm the next lane could add.
2. **No floor arm with a deliberately wrong constant.** B1 promised one -- halve
   the AO reach in the digest and watch the gate go red. It needs a second exe
   built with a wrong constant and this lane had one build slot for Part B, so
   the refuter is only partly retired (B1.5).
3. **No `--vt` arm.** `--vt` is reasoned into the switch digest because it
   changes the output from the same inputs, but no `--vt` bake was compared.
4. **Two regions of one worldspace.** Both are 5x5 chunks of Commonwealth at
   dim 4. No far ring (dim 16/32), no second worldspace, and **no
   whole-Commonwealth incremental run** -- the brief's region-bakes-only rule
   stands and the B5 extrapolation to "hours" is arithmetic, not a measurement.
5. **The 1 s fixed cost is a warm-cache number.** The OS file cache held this
   region after dozens of bakes; a cold first run will be slower and was not
   measured.
6. **The ledger has never been read back after a NifSkope version change.** It
   carries `version 1` and the reader checks it, but no upgrade path has been
   exercised because there is nothing yet to upgrade from.

---

## B9 Finished-work review against the skills this part was told to use

**`ww-prototype-is-not-the-product`.** Met, and it is the spine of Part B. The
edits are made **in a real copy of `Fallout4.esm`** by `b_esmedit.py` -- a
compressed `LAND` height raised with the GRUP size chain fixed up, a `REFR`
moved -- not by a loose-file override, because rows 1, 2, 3, 5 and 7 of the
dependency map live in the ESM and nothing else can reach them. Every comparison
is between two output trees the shipping exe actually wrote. **Where the skill
was nearly broken**: the first `refs` arm moved a reference the bake does not
draw, so the "test" compared a plugin edit that never reached the product. That
is the prototype trap wearing a real-plugin costume, and the per-arm floor is
what caught it.

**`ww-control-calibration`.** Met in three places, and each earned its keep. The
`null` arm is the instrument's known answer -- nothing changed, so the full bake
must not move and the incremental run must rebake nothing *and still match*; if
it moved, the arm reports **BROKEN CONTROL**, not PASS. The `merge-ok` arm in B2
is a negative control proving the default command does **not** refuse, and
without it all four refusal arms would have passed on the build that refused
everything. And B4's determinism check is two independent full bakes compared to
each other, not a claim about the writer.

**`ww-spec-gate-audit`.** Met on the ordering that matters: the dependency map
(B1) was written **before** the code, and the gate's verdicts are reported
including the two arms that were passing for reasons they had not asked about
(B2.1) and the arm whose floor was a lie (B3.1). **The audit's honest finding is
that two of B1's own claims did not survive the code** -- the merge is not a
whole-region pass, and a missing output is not a refusal -- and both are
corrected in place at B1.6 rather than edited out of the text above them.

**`ww-texel-picture`.** Met: real `.DDS` texels off disk, the window and the x8
gain both stated, sha1s beside the panels so the picture is never the proof, and
the dirty grids computed from logged data with a refusal if they disagree with
the exe.

**`nifskope-ww-build-verify`.** Met: the final exe is stamped (08:42:33,
21,935,616 B, sha1 `1e4e2c5cc5a0f34e058dbe67a9ac6fd9d52d8968`) and its **objects**
were checked -- `lodgen.o` and `nifcli.o` both 08:42:31 -- rather than
`exe -nt src`, which a link triggered by the other translation unit satisfies
just as well. That check is in `MISTAKES_ENTRIES.md` because without it at least
two of this lane's gate runs would have measured a previous exe.

**`nifskope-ww-lodgen`.** Met: `--road-detail 1` on every bake and picture;
region bakes only; Fallout4.exe confirmed down before every build and every exe
launch; no UI file touched and `E:/Projects/NifskopeWWE_ui` never entered;
nothing committed and `git stash` never run; patch scripts written with the Write
tool rather than heredocs, and the one place a heredoc was tried anyway
(`b_addmoveid.py`, first attempt) failed on exactly the backslash rule the skill
names.
