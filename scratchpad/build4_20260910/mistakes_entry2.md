## 2026-09-10 — a gate's picture whose caption is clipped, and two reporting lines that made green results read as misses (lane CARDORTHO, found by lane BUILD4)

**What was done.** Lane CARDORTHO shipped `transition.py`, which writes the
three-panel `source | card | overlay` pictures that CONSTITUTION 5 makes the
deliverable, and `run.sh`, which echoes each step's headline as it lands.

**What was true instead.** Three separate reporting defects, none of which
changed a measurement and all of which changed what the measurement LOOKED like:

1. **The pictures' captions are unreadable.** The three panel titles are drawn
   at one y and overlap into a single illegible line across the top of all three
   PNGs, and the bottom caption runs off the right edge —
   `cardortho_transition_0003a28b.png` ends mid-number at `ctl: centre 12.04 px
   = 6.27 card tex`. `ww-texel-picture`'s caption arithmetic exists so the
   picture's number is the same number the report quotes; here the numbers had
   to be read out of `transition.log` instead, which is exactly the decoupling
   the rule forbids.
2. **`run.sh` line 57 reports `sidecars saying ortho: 19 of 20`.** Its
   denominator is `ls "$CARDS"/*.txt`, which counts `cards/library.txt` — the
   run-level manifest, which has no camera and correctly carries no `projection`
   line. The true reading is **19 of 19** card sidecars. A pre-registered "19 of
   19" printed as "19 of 20" reads as a miss on sight.
3. **`run.sh` step 2 printed nothing at all.** `sed -n '/bake 4/,$p'` looks for
   a line containing `bake 4`, and `lodgen_octahedral.sh` never writes one — its
   cube block is labelled by content. The step that exists to quote the cube
   proof against its pre-registration quoted nothing, on a round where the cube
   proof was the strongest result in the lane.

**How it was found.** By opening all three pictures (CONSTITUTION 5) and by
reading the numbers out of `lodgen_octahedral.log` when the step meant to print
them came back empty.

**The rule.** A reporting line is an instrument and falls under the 2026-09-04
21:33 rule like any other: it must be WRITTEN and it must be RIGHT on a known
input. Run every echo in a chain script once against a log that already exists
before the chain is handed over — a `grep`/`sed` anchor that matches nothing is
indistinguishable from a step that measured nothing. And for a picture: the
caption belongs inside the canvas by arithmetic, with the panel titles on their
own rows, or the picture cannot be the proof it was made to be.
