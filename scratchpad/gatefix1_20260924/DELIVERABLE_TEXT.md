## HANDOFF text

**2026-09-25 01:0x GATEFIX1 (lane, Opus 5.5) -- DONE.** Branch gatefix1-20260924 (from b2f3073): 3e343a8,
d0a8e55, 1d2c769. Not merged. Exe dca43d83 = b2f3073 unmodified; no source code changed.
- Three LOD gates that were red are green again: stock_baseline.sha256, native_open.sh and lodgen_btofree.sh.
  None of the reds was a generator defect.
  - The baseline and the btofree pin were stale. Nine ruled moves are named rung by rung in
    scratchpad/gatefix1_20260924/DONE.md.
  - native_open inherited bungo's saved settings (water drew white).
  - The new btofree pin exposed three harness defects: a stale .lodj exclusion, a ledger drop mode that
    had crashed since 09-17 unseen, and census rows compared without the record's own volatile mask.
- **Overseer action:** copy E:\Projects\NifskopeWWE-gatefix1\release\NifSkope.before_gatefix1.exe (sha1
  dca43d83) into main's release/. lodgen_btofree's new default pin needs it; without it the byte legs skip.
- Kept green on dca43d83:
  - lodgen_native 32/0
  - lodgen_cardlink PASS (with the cardlink1 RUNG/CARDS)
  - lodgen_incremental 0 failures
  - lodgen_loadorder 24/0
  - lod_generation 128 checks
- Trap: lodgen_incremental rewrites the tracked scratchpad/incr_gate_work/* when it runs. Never commit those.

## WW_CHANGES text

- **2026-09-25 GATEFIX1 (lane, Opus 5.5): three LOD gates green again, no code changed.**
  - `tests/baselines/stock_baseline.sha256` is regenerated on dca43d83. It had moved twice, and both
    moves are rulings: the NiAlphaProperty threshold now follows the BGSM alpha ref (bungo 2026-09-18,
    "Alpha test 80 looks better"), and CELLVIEW2b resolves absolute build-path BGSMs.
  - `tests/spells/native_open.sh` runs every window in its own wiped `WW_SETTINGS_SCOPE`. It had been
    measuring the operator's saved view, not the exe.
  - `tests/spells/lodgen_btofree.sh` is re-pinned to `before_gatefix1`. The old pin predated nine ruled
    moves (EditorMarker exclusion, .lodi v6/v7, the horizon stream in and out, CELLVIEW2b, the alpha
    ref, DEFAULTS2 blend edges, BLENDSEAM1).
  - A rung that writes the native cache now has it compared byte for byte, no longer excluded.
  - The ledger's drop mode works; it had crashed since 09-17 without anyone seeing. It checks the
    bto clause, the layout count and the end line against the moved manifest.
  - Census rows are compared through `lodb_read.normalise()`.
  - `--generators-differ` checks that the VTFIX1 generator word moved every chunk's inputs.
  - Refuters: `scratchpad/gatefix1_20260924/ledger_refute.py` 8/0. The gate is 30/0 on the pin and
    30/0 against a byte-padded exe.

## MISTAKES text

- **2026-09-24 GATEFIX1: a harness leg that had never run was counted as a guard.** The btofree ledger
  drop mode had crashed with an AttributeError on every call since AUDIT1 (09-17): shape() was changed to
  return text, and drop mode still handed that text to a function that expects a dict. Nobody saw it,
  because the old rung wrote a version 1 record and the leg printed SKIP before reaching the call.
  **Rule:** when a pin moves, or a skip condition stops holding, run the newly reachable legs against
  mutated inputs before trusting their verdict. A leg that has only ever skipped has proved nothing.
- **2026-09-24 GATEFIX1: one settings-scope wipe per run is not isolation.** The first native_open fix
  wiped the scope once. The first window then saved its layout into the scope, and the second window
  opened 2 rows shorter, which refused leg (c) on a size mismatch. Wipe before every window.

## Skill review

- **nifskope-ww-worktree-build:** worked as written. Gap: the historical `release/NifSkope.before_*.exe`
  rungs are not in its copy list, and a rung only starts beside the DLLs. One line would cover it.
- **nifskope-ww-lodgen:** its btofree paragraph named the old pin. Updated this session: the pin is now
  before_gatefix1, a .lodj is swept rather than excluded, the generator word is checked, and it points
  to the new skill.
- **New: ww-stale-gate-attribution** (E:\Projects\Claude\.claude\skills\ww-stale-gate-attribution\SKILL.md)
  covers:
  - what to do when git history is squashed: order the rung exes by mtime, run them beside the DLLs,
    try each flag set in turn;
  - recursive bisection on per-file signatures;
  - naming each move by dump diff, the decoder census, the .lodi version word and the failed-models diff;
  - re-pinning without loosening, including the generator word;
  - per-window settings scopes for GUI gates.
  It is not copied to E:\Tools\AISkills (FO4 modding skills; this is WW harness practice).
