# Lane CARDS-AGG -- MISTAKES.md entries (the director splices)


Text for `MISTAKES.md`; the lane did not append them itself (the director
splices). Also in `scratchpad/cards_agg_20260911/MISTAKES_ENTRIES.md`.

1. **2026-09-11, lane CARDS-AGG — the composite normalised by sample COUNT and
   not by AREA, and the aggregate carried 94 percent too much coverage.** Each
   tree's layer divided its accumulated coverage by the number of samples that
   landed in a target texel, so a tree covering a tenth of a coarse texel was
   composited as if it covered all of it. Measured mass error 0.9449 against a
   ceiling of 0.0138. Found by gate A3's CEILING arm, which is the only arm that
   could have found it: the subject alone looked plausible, the floor was
   comfortably worse, and only "the subject is far below what a 64-texel frame
   could do" said anything was wrong. **The rule: a resample weights by the
   share of the TARGET's area a sample stands for, and every downsampling gate
   carries a ceiling, not just a floor.**

2. **2026-09-11, lane CARDS-AGG — the first picture gate used IoU at a hard
   alpha test and its own CEILING read 0.507.** A canopy at 64 texels is mostly
   partial coverage, so thresholding measured the threshold. `ww-silhouette-compare`
   §3 says exactly this and the lane re-derived it the expensive way. **The
   rule: when the subject is a partially covered sheet, measure a
   threshold-free quantity — the coverage MASS — and keep the thresholded number
   only as a labelled aside.**

3. **2026-09-11, lane CARDS-AGG — the BC4 alpha decoder in the gate interpolated
   with `(7-i)*a0`, which produces values above 255.** numpy said so with an
   overflow warning that was nearly ignored because "the picture looked right".
   The correct eight-value mode is `(6-i)*a0 + (i+1)*a1` over 7. **The rule: an
   out-of-range warning from a decoder is a decoder defect, not noise; a
   known-answer block (a0 = a1 = 255 must decode to 255 everywhere) costs two
   lines.**

4. **2026-09-11, lane CARDS-AGG — a mutation case's expected refusal substring
   was guessed and the reader named a DIFFERENT, correct rule.** Growing row 0's
   `coveredCount` was labelled "coveredFirst moved"; what actually answered was
   "covers instance 4, which stands in cell (2, 1)" — the cell rule, and the
   right one. `ww-standalone-writer-gate` warns about exactly this ("the
   mutation label must match the offset arithmetic, and the reader's refusal
   text is the proof, not the label"). Fixed by making the label say which rule
   answers AND adding a separate case that exercises `coveredFirst` alone.

5. **2026-09-11, lane CARDS-AGG — a relative `--impostors` path found zero card
   sets and the lane spent a round on it.** The trap is already in
   `nifskope-ww-lodgen`; the lane read the skill but did not apply it to the
   FIRST bake command it typed. **The rule: every path handed to the lodgen CLI
   is absolute, without exception, and a skill's trap list is a checklist for
   the command line, not background reading.**

---

