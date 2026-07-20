# Block List multi-block copy/paste — headless test

End-to-end test for the multi-selection **Copy Branch (Ctrl+C)** and **Paste
Branch (Ctrl+V)** spells. Copy Branch unions every *selected* block's branch;
Paste Branch slots each pasted root back in — and, when the paste target is not
a node (the realistic Ctrl+V case, where the current block is a shape), falls
back to the nearest NiNode ancestor. Guards the multi-root fix: every selected
root must be slotted in (not just the first), with each branch's internal links
remapped to the freshly pasted blocks.

## Test asset

Any FO4 mesh with **two or more BSTriShapes** under a NiNode works. The harness
copies the two smallest shapes (each an independent branch: shape → shader
property → skin/texture set …).

## Run (PowerShell)

The `WW_COPYPASTE_TEST` hook (nifskope_ui.cpp) selects the two shapes in the
block list (which publishes the selection Copy Branch reads), casts **Copy
Branch** then **Paste Branch** through `NifSkope::castSpell` — the exact path
the Ctrl+C / Ctrl+V shortcuts and the context menu share — pasting onto a *shape*
so the nearest-NiNode fallback is exercised, verifies the invariants on the live
model, optionally saves, and quits. Log: `release/ww_copypaste_test.log`.

```powershell
$env:WW_COPYPASTE_TEST='1'
# optional: also save the pasted result for the byte-level verifier
$env:WW_TEST_SAVE='E:\path\to\work\target_pasted.nif'
release\NifSkope.exe E:\path\to\work\target.nif
```

The log ends in `PASS` when all of these hold:

- the clipboard received a `nibranch` payload (copy succeeded),
- block-count delta == the copied branch **union** size (both branches were
  copied, not just one),
- the nearest NiNode gained exactly `roots.count()` children whose block types
  match the copied roots **in order** (every root slotted in, order preserved),
- no pasted block has a child link pointing back into the original block range
  (internal links were remapped, none left dangling).

## Verify (byte level)

Re-checks the saved file so save/load can't hide a corruption the live-model
check missed. Pass the expected final block count (the harness logs
`blocks N -> M`; use `M`).

```sh
python verify_copypaste.py work/target_pasted.nif <M>
```

Checks every NiNode-family node's Children links resolve to a real block (no
`-1` dangling, none out of range) and the block count matches. Prints each
node's Children so the re-attached roots are visible; ends in `PASS`/`FAIL`.
