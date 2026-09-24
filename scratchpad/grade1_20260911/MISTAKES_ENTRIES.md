## 2026-09-12 — I read a flag's name instead of the census, and measured an empty road mask (lane GRADE1)

The brief asked for the curve on ground texels, roads excluded. I baked an arm
with `--roads`, differenced it against the no-flag bake to find the road texels,
got **zero** road texels on both tiles, and started writing that as a finding
about the test chunks.

`--roads` is not opt-in. Roads are ON by default, so the `--roads` arm was
byte-identical to the no-flag arm in `tex/`, `mod/` and `obj/` — and the bake's
own census had said so twice in each: `roads 1 roadPlacements 142 ...
roadTexels 27509`. I had both censuses on disk before I ran the difference.

**A bake prints what it did. Read the census before inventing a mask from a flag
name, and treat an empty mask as an instrument failure until the census agrees
it is empty.** A mask of exactly zero is never evidence about the data; it is
evidence about the code that built it. The fix is the opposite arm: differencing
a `--no-roads` bake gives (-20,24) no roads at all (true, and the census agrees:
that chunk has none) and (-20,20) 25,189 road texels, 9.61 %.

## 2026-09-12 — I correlated the residual of a model that ignores its input (lane GRADE1)

To name the position-dependent part of the tone difference I took the residual
of the **best-fitting** model — chosen by RMS, which selected the affine fit —
and correlated it against height, slope, AO and our own luminance. The table
came out with `ours lum` reading exactly **r = -0.0000**, which I nearly wrote
down as "the residual is independent of our brightness".

The affine fit's slope was 0.019. A fit with slope ~0 predicts a constant, so
its residual is `constant − vanilla`: I was correlating VANILLA's structure and
calling it ours, and the perfect zero was the arithmetic tell (least squares
orthogonalises the residual against every regressor, and `ours` was the
regressor). Redone on the constant-gain residual and on the raw difference, the
same correlation reads **+0.835** and **+0.827** against phase-twin floors of
−0.056 and +0.012 — the strongest signal in the lane, and it had been showing as
its own negation.

**Lowest RMS does not mean "the model to interpret".** Before reading a
residual, check that the model actually uses its input: compare its RMS against
the TARGET's own standard deviation (here 9.478 and 11.899, which is exactly
what the affine and gamma fits scored), and refuse to interpret a model whose
error equals the trivial predictor's. An r of exactly 0.000 against a variable
the model was fitted on is a definition, not a measurement.

## 2026-09-12 — the picture was wrong in two ways that only looking could catch (lane GRADE1)

`cmp_tone.png` drew the ours-minus-vanilla difference map through a colour ramp
with a 1.8x multiplier left in from a sanity test, so every texel beyond ±22
levels clipped to full red or full blue and the map read as a solid two-tone
field — on a tile whose whole point is that the difference is structured. And
the residual panel of `curve.png` cited `corr['ours lum']` out of the curve
JSON, which is the affine model's r (the mistake above), printing −0.000 under
a scatter that visibly slopes.

Both were found by opening the PNGs and reading them, after the scripts had
exited 0 and the JSON was right.

**A picture is a measurement and gets the same treatment: state what it must
show before you draw it, then look at the file and check it shows that.** A
saturated ramp and a mislabelled statistic are both silent — no exception, no
failed assertion, no wrong number in any log.

## 2026-09-12 — the heredoc trap again, on a file the skill says to Write (lane GRADE1)

Creating a new instrument with `cat > g1_curve.py <<'PYEOF'` died with
`unexpected EOF while looking for matching '`'. Heredocs arrive CRLF in this
environment; the terminator never matches. This is already written down twice
(`feedback_crlf_python_edits`, `nifskope-ww-build-verify`: *patch with a script
file, never a heredoc*) and I did it anyway because the file was "just a script,
not a patch".

**The rule has no exception for scripts.** Every new file in this tree goes
through the Write tool.
