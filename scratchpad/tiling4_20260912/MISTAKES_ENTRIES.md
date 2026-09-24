## 2026-09-12 -- lane TILING4 (the hex tiling behind `--land-sample stochastic`)

1. **A modelled number was written into a shipped document while the measured
   one was four minutes away.** `d0_doc.py` put "6 of 7 and 6 of 7 on the
   repeat" into `docs/LODGEN_TERRAIN_VT.md` 2.5e, and the same count into the
   changelog entry. That was `scratchpad/splat1_20260911/offline_bake.py`'s answer -- a python
   re-implementation of the land-texture composite with no crevice term and no
   vanilla reuse. What was true: the product reads **4 of 7 and 5 of 7**, and
   TILING3's warp -- the thing this lane replaced -- passes the repeat on
   **more** sheets than the hex tiling does, 11 of 14 against 9 of 14. How it
   was found: by this lane's own `f3_real.py`, which had already shown the
   prototype disagreeing with the exe by up to 0.27 in **both** directions on
   the only two chunks it could check, **before** the document was written; the
   fourteen-sheet product bake (`f3_full.sh`, 42 bakes, 3-6 s each) then
   confirmed it. Corrected by `d1_doc.py` (both count paragraphs replaced, the
   provenance list extended) and by section 6 of the lane report, which states
   the withdrawn claim instead of quietly overwriting it. The rule: a
   prototype's absolute numbers never cross into a gate, a document, a
   changelog entry or a handoff -- bake the same sheets with the real exe and
   score them with the same scorer imported unchanged. Offered as a skill,
   `ww-prototype-is-not-the-product`, in section 9 of the report.

2. **A byte-identity gate's expectation was wrong and reported FAIL against a
   correct product.** Gate F2 arm C expected `--land-sample stochastic` to move
   exactly one file, the chunk colour DDS. What was true: it moves **three** --
   the colour DDS and `Commonwealth.VT.2.lodt` / `VT.4.lodt`, which are the
   virtual-texture pyramid written by the **second** `sampleLtex` call site, the
   site TILING2 edited nothing at and was bitten by. How it was found: by
   digesting TILING3's trees against this lane's, which showed TILING3's own
   warp moving the same pair and `--land-sample warp` reproducing its bytes
   exactly. The expectation is now three files, with the reason written beside
   it, and the `.lodt` pair is the lane's positive evidence that both sites were
   edited. The rule: when a gate and the product disagree, prove which one is
   wrong before changing either -- and an expectation list for a two-site change
   must enumerate both sites' outputs.

3. **A dictionary key was written from memory instead of from the function.**
   `f3_real.py` read `R['abs_ceil']` from `h1_sweep.score()`, which does not
   return it, and crashed. Fixed by computing the ceiling the way `score()`
   itself does (`ABS_CEIL` unless that sheet's own no-repeat control reads
   higher). The rule: read the function, do not remember it.

4. **Two patch attempts refused on CRLF, in a repo that has a memory note
   about exactly that.** A multi-line anchor typed into a `<<'EOF'` heredoc
   arrives with CRLF on this machine, so it cannot match an LF-only file and an
   anchored script refuses. No damage -- refusing is what those scripts are for
   -- but two round trips were lost. The rule: normalise the anchor inside the
   script (`s.replace('\r\n', '\n')`) rather than trusting the heredoc, and
   never "fix" a refusal by relaxing the anchor.
