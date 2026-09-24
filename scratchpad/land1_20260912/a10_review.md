
## A10. Finished-work review against the skills this part was told to use

Each skill is answered with what it demanded, what was done, and -- where they
differ -- the gap, named rather than smoothed.

**`ww-prototype-is-not-the-product`.** The demand is that nothing is reported on
a stand-in for the thing that ships. Met: all 119 sweep bakes and every gate bake
were made by `release/NifSkope.exe` itself, writing real `.DDS` sheets to disk,
and every panel of both pictures is one of those files read back -- no synthetic
sheet, no modelled rule, no scorer output standing in for a bake. The switch
ships in the same exe that was measured.

**`ww-control-calibration`.** The demand is that an instrument is shown to give
the known answer before it is trusted on an unknown one. Met and it mattered:
lane TILING4's scorer was imported UNCHANGED and re-run in this lane on its own
known cases (A5) before a single candidate was scored, and the `_msn` comparison
was run against **the heightmap's own octave floor** (9.63 degrees) rather than
against zero -- which is the only reason the 10.38-degree result could be read as
"carries nothing new" instead of as a disagreement. The convention sweep in A2 is
the same principle applied to a suspicious constant: a mean of exactly 90 degrees
is a transposed axis, not a measurement.

**`ww-spec-gate-audit`.** The demand is that gates are registered before
candidates are scored and that the validation set is not opened early. Met: the
gates are written at A0 with a 07:12 timestamp, ahead of the patch at A0b, and
the seven validation sheets were not opened until the winner had been chosen on
the other seven. **The audit's verdict is a failure and is reported as one**:
gate F3 is NOT MET on either set (repeat 5/7 and 5/7 against a 7/7 bar), and two
of the three failing rows are the rung's own -- no change to the sampler can pass
a gate whose own floor fails. That is why the switch ships `off`.

**`ww-texel-picture`.** The demand is real texels, a stated window, and a
comparison a person can check. Met: both pictures use the window
`warp_sweep.py` already uses (x0=300, y0=40, 128 texels, 3:1 nearest neighbour)
so they line up with a picture bungo has already looked at, with the whole sheet
at 1:1 underneath because a swirl is a 30-100 texel feature that a 128-texel crop
can hide. Vanilla's own sheet is one of the six panels, and the two sheets were
chosen by **measured macro slope** (the flattest of the selection seven and the
steepest of the validation seven), not by eye.

**`nifskope-ww-build-verify`.** The demand is that a harness verdict is read next
to the exe that produced it, and that the exe's objects are checked rather than
`exe -nt src`. Met: A8 stamps the exe's timestamp, size and sha1 beside every
harness line, and the object timestamps were read. **Gap: the chain was run with
the interpreter named** precisely because MSYS2's python has no numpy and an
empty number reads like a regression -- the ROADS1 trap, met and avoided.

**`nifskope-ww-lodgen`.** Met on the mechanical rules: `--road-detail 1` on every
bake and every picture; region bakes only; no UI file touched; nothing committed;
patch scripts written with the Write tool because heredocs eat backslashes and
apostrophes (met four times, and broken once anyway -- see the Qt6 entry in
`MISTAKES_ENTRIES.md`).
