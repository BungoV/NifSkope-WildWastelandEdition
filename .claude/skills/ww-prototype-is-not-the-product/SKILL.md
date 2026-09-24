---
name: ww-prototype-is-not-the-product
description: Before any count from an offline (python) re-implementation of a bake reaches a gate, a document, a changelog entry or a handoff, bake the same sheets with the real exe and score them with the same scorer. Use whenever a lane sweeps a design space offline and then ships a number. Lane TILING4, 2026-09-12.
---

# The prototype is not the product

A python re-implementation of a bake is the only way to sweep a design space,
and its ABSOLUTE numbers are not the product's. TILING4's offline hex tiling
read "no cost in repeat"; the real exe on the same fourteen sheets read 9 of 14
against the warp's 11 of 14, and the doc and changelog had already been written
with the prototype's counts (`MISTAKES.md`, 2026-09-12, lane TILING4).

## The rule

Before any count from the prototype reaches a gate, a document, a changelog
entry or a handoff block:

1. bake the same sheets with the real exe (own out-dir, region bakes only);
2. score them with the **same** scorer, imported unchanged (`f3_full.py`
   imports the instrument modules; it does not copy their formulas);
3. put that table in the report beside the prototype's, and say which claim
   is withdrawn if they disagree. The product wins, always.

Budget it: a dim-4 chunk bakes in 3-6 seconds, so fourteen sheets in three arms
is about four minutes. No sweep is large enough to make that unaffordable for
the final answer.

## Red flags

* a document or changelog entry being written while the only measurement of
  the shipped code is two chunks wide;
* "parity C++ == prototype at N positions" offered as the proof -- parity says
  the two agree on those positions, not that either one's per-sheet score is the
  bake's;
* an offline scorer used as an absolute instrument. TILING4's
  `scratchpad/splat1_20260911/offline_bake.py` was wrong both ways by up to
  0.27 on the repeat.

## Related

`ww-control-calibration` (known answers first), `ww-spec-gate-audit` (the gate
as registered), `nifskope-ww-lodgen` (region bakes).
