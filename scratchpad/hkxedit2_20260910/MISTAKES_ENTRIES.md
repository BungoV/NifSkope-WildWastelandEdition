## 2026-09-10 -- lane HKXEDIT2 (the animation workspace)

1. **A Bash heredoc halved the backslashes again -- twice, in one lane, after
   reading five recordings of the trap.** (a) A `python - <<'EOF'` check that
   counted `.pro` anchors wrote `"\\\n"` for backslash-newline and got
   backslash-n, so `src/hkxanimui.h \` counted 0 and looked like a wrong
   anchor; the file was fine. (b) A `python - <<'PYEOF'` PATCH of
   `hookup.py` turned `\\t`/`\\n` inside Python string literals into real
   tabs and newlines and left the script with a SyntaxError, and its second
   replacement matched nothing (0 of 1) while printing no error. Found by
   running the script (a). Rule, unchanged and now the sixth time: **any
   script or patch whose text carries a backslash goes through the Write or
   Edit tool, never a heredoc -- INCLUDING one-off checks**, because a check
   that counts 0 for the wrong reason sends the lane to look for a defect
   that is not there.
2. **A gate pre-registered by the brief cannot hold on the model it was
   written for, and the lane found it only by running the gate.** Gate (c),
   "delete that key -> the clip returns to its pre-edit decode exactly", is
   false whenever a key exists at every frame: deleting the key at 46 makes
   46 the interpolation of 45 and 47, 0.877 deg from the original. The
   honest gate is "Undo returns the pre-edit decode exactly" (which holds,
   measured). Rule: when a brief's gate presumes a mechanism (here: that a
   key is a layer over an untouched frame), test the presumption against the
   key model FIRST and pre-register the corrected statement before writing
   the code that would have to fake it (`ww-spec-gate-audit`).
3. **HKX5's writer files print EMPTY annotation text in HKXPACK, and three
   lanes' gates never asked.** HKX5 gated transforms through HKXPACK (gate
   e); HKX1's reader and HKXEDIT1's oracle read the names, so nobody looked
   at what HKXPACK printed for a string. Found here because gate (d) grepped
   HKXPACK's XML for the added name. Rule: a round-trip gate through a
   third-party reader compares EVERY field family the writer emits (numbers,
   strings, pointers), not the one the lane was about.
4. **The dope sheet's `docs` container was declared by value in the header
   and used by pointer in the source** -- six compile errors on the first
   syntax pass. Caught by `-fsyntax-only` with the real flags in 40 s, which
   is what it is for; recorded because the header was written from the plan
   and the source from a later decision (pointer stability under QHash
   rehash) without going back to the header.
5. **Three `std::min/max( int, qsizetype )` errors** across two files: Qt 6
   containers return `qsizetype`. Wrap with `int( ... )`; the fourth lane in
   a day to pay this.
