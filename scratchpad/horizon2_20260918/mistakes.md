## 2026-09-19 -- HORIZON2 -- The refuter's "independent reference" calls the same function the march does

- `--horizon-refute` exists so the horizon bake can be falsified from inside
  itself, and `tests/spells/lodgen_horizon.sh` G3 scores the sheet against it
  with a **97% pre-registered floor**. Three documents describe that reference as
  independent: "every pencil at its own EXACT azimuth, a constant 32-unit step,
  mip 0".
- **It calls `LodgenHorizonField::maxAlong`** -- `src/lodghorizonrefute.h:107`
  and `:112` -- which is the function the march calls. The azimuths, the step and
  the mip are independent. **The FOOTPRINT is not**, and the footprint was the
  defect.
- How it was found: this lane changed `maxAlong`'s tap from a 2x2 block to one
  square, re-baked chunk 4.4.-12, and the REFERENCE's own census numbers moved --
  `horizonRefuteMeanRefDeg` 63.565 -> 58.995, `…MeanRefPencilDeg` 60.041 ->
  54.226, `vhorRefuteMeanRefDeg` 18.875 -> 17.738 -- in the same bake that moved
  the sheet. A reference that moves when the march is edited was never scoring
  the march.
- The cost: `horizonRefuteA240E15` sat at **51.69%** against a 97% floor and the
  lane that shipped it read that as "the field is 51.69% right", when the true
  reading is "two instruments that share a function disagree by 48 points and
  BOTH stand about 8.5 degrees above the real skyline". The sheet called an entire
  downtown chunk **0.0% lit at a 15-degree sun** where a raw-input witness says
  10.2%, and no gate in the tree could say so.
- The rules:
  * **Before a floor is pre-registered between a producer and a reference, name
    every function they share.** `grep -n "<the producer's class>::" <the
    refuter>` is one command. If the list is not empty, the floor measures
    agreement, not correctness, and the document must say which.
  * **A reference whose own reported numbers move when the producer is edited is
    not a reference.** Print the reference's own aggregate in the census -- this
    one did, which is the only reason this was catchable -- and read it as a
    regression signal in its own right.
  * The check that CAN fail is one built from the raw inputs. This lane added it:
    `tests/spells/lodgen_horizon_witness.py` + `.json`, ten receivers whose
    skylines come from the BTD heightmap and the placements as exact world boxes,
    with the pre-fix sheet as a red control that must fail on a real file.

## 2026-09-19 -- HORIZON2 -- A "resolution invariant" that errs on the safe side, with its price never measured

- `src/lodghorizon.h` chose its mip level against `binWidthFraction * d` -- the
  azimuth bin's own width -- and took a 2x2 block at every tap, and said so in a
  comment: the smear "is therefore never larger than the smear 22.5-degree bins
  already carry, and it errs toward MORE occlusion, never less, **which is the
  safe side for a shadow**".
- What that actually produced is a stored byte that is the **maximum over the
  bin's 22.5-degree sector**. The viewer does not read a sector maximum:
  `src/btdterrain.cpp` takes the sun's azimuth, finds the two bins either side
  and BLENDS them, which is only meaningful if each byte is the skyline in its
  own direction. Measured price, against a witness sharing no code:
  **+8.52 degrees of one-sided bias**, 0.0% of chunk 4.4.-12 lit at azimuth 240
  elevation 15 against a true 10.2%.
- The same day, the same file gained a correction *inside* the unmeasured
  convention: "the footprint is the bin, not twice the bin", level picked at
  `2 * cell <= wantCell`, which moved mean |march - reference| 4.103 -> 3.651 and
  the "bins say HIGHER" cause column 681 -> 351. Both numbers are real. Both were
  measured against the reference that shares the function, inside a convention
  nobody had put a number on. **A measured improvement inside an unmeasured
  convention is the most convincing way to be wrong.**
- The rule: **"errs on the safe side" is a claim with units.** When a comment
  says a bias is acceptable, the number goes in the comment: how much bias, over
  what population, measured against what. If it cannot be measured yet, the
  comment says THAT, and the measurement is owed work with a name -- not a
  reassurance. A bias that is always in one direction is not noise and does not
  average out; it is an offset, and offsets are what a consumer's threshold sits
  on.

## 2026-09-19 -- HORIZON2 -- I read the shipped sheet with the WRITER's channel shifts, and blamed the bake

- My Python sheet reader took the bake's own packing constants -- the C++ builds
  a Qt `0xAARRGGBB` u32 with `shift[4] = {16,8,0,24}` (`src/lodgen.cpp:11110`) --
  and applied them to the u32 it read out of the FILE. Ten receivers came back
  disagreeing with my transcription of the march by up to **43.44 degrees**, and
  the first hypothesis I wrote down was that the bake's packing was wrong.
- It was not. The sheet writer hands the plane out as **R8G8B8A8**, so in the
  file bin *j* of each group of four is **byte j**: the shifts are `{0,8,16,24}`.
  Reading the file with the writer's shifts swaps bins 0 and 2 of every four.
  With the order corrected the worst disagreement fell to **3.67 degrees** against
  a 0.353-degree quantisation step, and the shipped viewer
  (`src/btdterrain.cpp:1762` -> `sheetChannel(role, tx, ty, bin % 4, …)`) was
  already reading it the same way. **My instrument was wrong, not the bake.**
- Found by recovering the permutation empirically -- the residual was a clean
  `[2,1,0,3]` within each group of four -- and then by a shift-order sweep, rather
  than by re-reading the C++ and believing it twice.
- A second thing fell out of it and it is the more useful half: **the in-bake
  refuter reads its bytes back out of the in-memory `planes`, never out of the
  file**, so it is structurally incapable of catching a file-order defect. If the
  writer's swizzle were ever wrong, every gate in the tree would stay green.
- The rules:
  * **A decoder is validated against the shipped CONSUMER, not against the
    producer's constants.** The viewer is the ground truth for file order.
  * **When a new reader disagrees with a shipped artefact, the reader is the
    suspect until it reproduces something independently known.** The control
    that settled this lane was `|STORED - PORT| mean 0.14 max 3.67` -- a
    transcription of the march agreeing with the bytes on disk -- and everything
    after it rests on that one number.
  * A readback that never touches the file cannot verify the file
    (`feedback_telemetry_echo_truth`, again).

## 2026-09-19 -- HORIZON2 -- My leading hypothesis was refuted by my own measurement, and I nearly did not run it

- Going into the lane the strongest candidate for the over-occlusion was lane
  SLAB1's doctrine: a square occludes from the ground only where its LOWEST
  object surface reaches down to the receiver (a WALL); a square whose whole
  object span stands above (a CEILING) should not darken the ground under it.
  `LodgenObjectHeightField` carries the MIN plane that makes that testable, and
  the story fit the symptom exactly.
- Measured (`scratchpad/horizon2_20260918/lattice_out.txt`): applying SLAB1's
  wall/ceiling law to the horizon field made the error against the third witness
  **WORSE, 7.33 -> 10.90 degrees.** It is refuted, and it is in the report as
  refuted with the number.
- The reason it fails is worth keeping: a shadow receiver at ground level is
  occluded by a ceiling it can see the underside of, and at 1 km a raised highway
  deck is exactly that. The SLAB law is about a surface's own ambient occlusion,
  which is a different question from "what is the highest thing on this bearing".
- The rule: **the hypothesis you arrive with is the one that most needs a red
  control, because everything you build will be shaped to test it.** Write the
  measurement that would refute it FIRST, and run it before the one that would
  confirm it.

## 2026-09-19 -- HORIZON2 -- An elimination pass that scored every candidate against the defect's own convention

- The first sweep over the brief's candidate causes (mip choice, tap size, growth,
  near-end, near-skip, reach, axis) reported that **nothing helped**: every rule
  variant scored the same or worse. That is a real result and it was wrong.
- The score was `|variant - third witness's MAXIMUM OVER EACH BIN'S SECTOR|`. The
  sector maximum is precisely what the shipped march computes. Every candidate was
  being graded on how well it reproduced the bug.
- Found by asking what the VIEWER reads -- it blends two bins, so it wants a
  directional sample -- and re-scoring against one pencil at each bin's own
  azimuth (`variants2.py`). The same variants immediately separated: shipped
  11.90, tap 1x1 9.93, mip from the segment 9.77, growth 1.3 8.14.
- The rule: **the truth a candidate is scored against is a choice, and it must be
  derived from the CONSUMER, not from the producer.** Write down what the
  downstream reader does with the number before choosing the metric; if the
  producer's own convention is the yardstick, every candidate that differs from
  the producer loses by construction.

## 2026-09-19 -- HORIZON2 -- `make rc=0` on a build that compiled nothing, because the Makefile had never heard of the header

- The brief names make's exit code as the build gate, which is the right rule
  (root `MISTAKES.md`, 2026-09-06). After editing `src/lodghorizon.h`,
  `mingw32-make -f Makefile.Release -j8` printed **`Nothing to be done for
  'first'.`** and exited **0**. `release/NifSkope.exe` was unchanged at 21:59:46.
- Cause: `grep -c lodghorizon Makefile.Release` returns **0**. qmake froze that
  Makefile's dependency lists before the header existed, so no object depends on
  it and make had nothing to do and no reason to say so. This is the sibling of
  the LODIV7 stale-object entry below, with a different signature: not an object
  older than a header it lists, but a header **no object lists at all**.
- Repaired by deleting the five objects of the translation units that include it
  (`btdterrain`, `lodgen`, `lodinative`, `nativeemit`, `nifcli`) and re-running
  make. The build log then named exactly those five and nothing else, which is
  also the check that the new exe is the old exe plus this diff.
- The rules:
  * **`make` exiting 0 is necessary and not sufficient. The exe's mtime moving is
    the other half**, and it costs one `stat`. "Nothing to be done" with a source
    you just edited is a FAILED build wearing a 0.
  * **After editing any header, `grep -c <header> Makefile.Release` before
    trusting the build.** Zero means the Makefile predates the header: delete the
    includers' objects, and note that a qmake run is owed because the hand repair
    does not survive one.
  * A lane brief that hands over a `PATH` is handing over a shell assumption:
    `/ucrt64/bin` is an MSYS2-shell path and resolves to nothing under Git-Bash,
    where MSYS2 lives at `/c/msys64`. The first build of this lane died on
    `mingw32-make: command not found`, rc=127. Copy the working line out of the
    previous lane's report, not out of prose.

## 2026-09-19 -- HORIZON2 -- A control that mixed the two changes it was controlling for

- The candidate fix was measured in Python before any C++ was written, and the
  file it left behind (`cand_bins.json`) was the thing the shipped exe was to be
  checked against. Comparing them gave `|C++ - python| mean 2.325 deg, max
  35.048` -- ten times the 0.353-degree quantisation step -- which read as "the
  C++ does not implement what was measured".
- It reads that way because `candidate.py` cast those bins with `growth` still at
  its default **1.5**, while the growth sweep in the same script was still
  choosing **1.3**, which is what shipped. The comparison was footprint+1.5
  against footprint+1.3: two changes, one number.
- Re-run with both changes at the shipped values (`control13.py`):
  **mean 0.244 degrees, 97.7% of bins within one stored step**, and the two
  sampled azimuths agree to 0.08 and 0.07 degrees.
- The rule: **a control is a comparison at IDENTICAL settings, and a script that
  sweeps a knob leaves its artefacts at whichever value the sweep ended on.**
  Re-generate the reference artefact at the shipped configuration before using it
  as a control, or record the configuration inside the artefact so the mismatch
  refuses instead of reporting a number.

