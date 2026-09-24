
---

## 4. Owed / red / bungo's calls

### RED — nothing this lane can close

1. **`native_open.sh` fails one check, and failed it before this lane started.**
   `the .lodi scene covers the same pixels as the .BTO (IoU 0.8179 >= 0.95)`.
   Run on both exes, same number to four decimals. It is an OBJECT placement or
   object-culling question, not a lighting one. This lane leaves it exactly as
   it found it and does not claim it. It needs an owner.

### BUNGO'S CALLS — measured, deliberately not acted on

2. **The legacy `.BTR` is on a different program, and the HANDOFF's explanation
   for it was wrong.** The HANDOFF said the `.BTR`'s darkness was "one fixed
   frame". The census says otherwise:

   ```
   # WW_PROGRAM_CENSUS  light(view) = 0 0 1
   shape="Land"             bsver=130 msn=1 lodland=1 prog=sk_msn.prog
   shape="Terrain -20,24"   bsver=130 msn=1 lodland=0 prog=fo4_default.prog
   shape="obj-at" / "obj"   bsver=130 msn=0 lodland=0 prog=fo4_default.prog
   ```

   The `.BTR`'s `Land` shape is Shader Type 18 (`ST_WorldMap4`), which
   `res/shaders/fo4_default.prog` excludes by condition, so the program scan
   hands it to `res/shaders/sk_msn.prog` — **a model-space path already, the
   Skyrim one.** It was never on the broken path. That is why its dark fraction
   is 16.44% before and 16.44% after, to the hundredth, in both views.

   So the brief's gate (c) asked for something that cannot happen: "the same
   view of the legacy `.BTR` moves too". **It does not move, and the reason is
   measured rather than guessed.** I did not make it move. Making it move means
   routing Shader Type 18 to `fo4_default.prog`, or changing `sk_msn.prog`, and
   either is a renderer routing decision with a blast radius well outside this
   lane's brief — every Skyrim model-space shape in the tree rides the same
   program. **This is bungo's call, and it is the one thing in this lane I would
   ask him about first.** The open question underneath it is whether the two
   model-space paths agree with each other on brightness at all; that was NOT
   measured here.

3. **The FO4CS citation the brief asked for does not exist.** The brief asked me
   to read the channel convention off FO4CS's shader and cite it with numbers.
   `E:/Projects/Fo4CommunityShaders/fallout4-community-shaders` contains exactly
   **one** mention of `_msn` — `docs/RE/far-field-terrain-lod.md:968`, which is a
   count of files, not a decode — and no shader anywhere in the tree that reads
   those channels. There is nothing there to cite. The convention in section 0
   therefore rests entirely on the other leg the brief asked for: six of
   Bethesda's own shipped sheets, decoded and correlated against the heights of
   the same cells, with a row-order refuter that collapses the statistic. That
   leg is strong on its own, but the brief asked for two and got one, and I am
   naming the gap rather than dressing the one up as two.

### OWED

4. **Nothing is committed.** Per the charter: no commit, no `git stash`. The
   working tree carries this lane's four modified files and four new ones, listed
   in `CHANGED_FILES.txt` with byte counts before and after. Committing is
   bungo's word, by explicit path list.

5. **The change has not been seen in bungo's own window.** Everything here is
   headless render-shot evidence and harness counts. If his NifSkope is open it
   is running the old exe and needs a restart to show any of this.

6. **The `.BTR`/`.lodl` brightness comparison is not a like-for-like one and is
   not made.** They are different programs (point 2). Where a picture puts them
   side by side it is labelled a CONTROL, not a comparison.

7. **Gate (b)'s dark-mask IoU went degenerate and its replacement is honest
   about that.** After the change neither arm has a pixel under luma 40, so the
   IoU that the brief registered (0.86 on the rung) has nothing left to measure
   and reads 0.000. That number is not evidence of anything by itself. The
   threshold-free companion — darkest-fifth IoU, 0.858 to 0.705 — is what the
   gate actually tests, together with the block SD of the own-minus-flat
   difference, and both are in `native_lighting.sh` with their rung values beside
   their floors.

8. **The terrain in this chunk is not steep, so the absolute effect is modest.**
   Cells (-20,24)..(-17,27) are gentle ground: the real normals there sit close
   to "up", so a flat sheet is a fair approximation of them and the two arms
   differ by a mean of only 6.14 luma. What the rung got wrong was the
   DIRECTION, not the magnitude, and gate (d) is where that is shown on a
   fixture with a known answer. **A steep chunk was not tested.** If bungo wants
   the effect shown at its largest, the lane to run is the same one on
   mountainous cells.

### NOT MEASURED, stated plainly

9. No PBR renderer work was done and none was looked at (standing order).
10. No writer changed, no bake output changed, no default changed. `lodgen`'s
    own gates were not re-run because nothing this lane touched can reach them —
    `src/btdterrain.cpp` was READ, not modified, in the end: the flag it already
    sets turned out to be correct and the defect was entirely on the viewer side.
11. Nothing was measured in the game. This is a viewer lane.
12. The tone map was not modelled. Every gate is built on ORDER and EQUALITY,
    never on a ratio; section 1's gate (d) says why and gives the number that
    would have made a ratio wrong (the flat arm's top/oblique luma ratio is
    0.877 where `N.L` alone would say 0.444).
