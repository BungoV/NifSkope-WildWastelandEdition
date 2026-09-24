# CHANGE_NEEDED — lane CARDWIDTH, 2026-09-10

One document outside this lane's file list states a rule this lane's change
supersedes, and the lane did not touch it.

## `docs/LODGEN_IMPOSTOR_SPEC.md`

Two passages, both about coverage:

* around line 269 — *"The COVERAGE FLOOR is 16/255: from there a texel counts as
  covered and carries its own values …"* — still true of the MEASUREMENT, but it
  no longer describes what the sheet's alpha CONTAINS. From 2026-09-10 the
  base-colour sheet's alpha is re-encoded so that a covered texel is written at
  or above 160 and an uncovered one at 0.
* around line 318 — *"coverage is a FRACTION after the bake's downsample, not a
  cut-out: a twig thinner than a texel reads below 0.5. A consumer alpha-tests at
  0.5 for full crowns and tests lower, or blends, for bare trees."* — this is the
  sentence that caused the defect. It is now: a consumer tests at the `.lodm`'s
  own `coverage.test`, which this generator writes as 128, and a set with no
  `coverage` key must be tested at 16/255, not at 0.5.

The replacement text is in `docs/LODGEN_CARD_SHEETS.md` §4 ("THE COVERAGE
CONTRACT") and `docs/LODGEN_LODM_FORMAT.md` (the `coverage` key and invariant 8a),
both of which this lane owns and has updated. Whoever owns the spec should splice
the same contract in.

Nothing else outside the lane's files needed a change.
