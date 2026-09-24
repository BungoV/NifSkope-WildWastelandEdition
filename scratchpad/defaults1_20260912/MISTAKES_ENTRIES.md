## 2026-09-12 -- lane DEFAULTS1 (bungo's four ruled defaults go into the code)

1. **A bash heredoc halved the `\n` in a patch anchor -- the SAME trap the lane
   above me ledgered hours earlier, and I hit it twice in a row.** Two patches
   against `src/lodgenmanager.cpp` were sent as `python - <<'PY'` with anchors
   holding the two characters backslash-n as they sit in a C++ string literal.
   The heredoc delivered a real newline instead, so the anchor count came back
   `0` while `grep` proved the text was there, and I spent two round trips
   doubting the file rather than the transport. NATIVEVIEW1's entry #2 of the
   same day says it in one line, and the repo skill `ww-anchored-hookup` says
   it before that. THE RULE, restated a third time: **any patch text carrying a
   backslash goes into a script file written with the Write tool**, and the
   script builds the sequence itself (`NL = chr(92) + 'n'`) rather than trusting
   a quoted literal to survive the shell. Nothing was written -- the script
   refuses before the first replace unless every anchor is found exactly once --
   so the cost was time, not a corrupt source.

   OCCURRENCE COUNT, ADDED LATER THE SAME SESSION: a fourth time, in a
   throwaway probe rather than a patch -- `python - <<'PY'` carrying
   `if '\' in s:` arrived as `if '` and died with
   `SyntaxError: EOL while scanning string literal`. The rule above is not
   about PATCHES, it is about any Python that carries a backslash: it goes in
   a file. `scratchpad/defaults1_20260912/btrstrings.py` is that probe, and it
   builds the character as `BS = chr(92)`.

   AND A FIFTH, an hour later, in the patch that re-bases the panel self-test:
   `python - <<'PY'` carrying a C++ `"\n"` literal inside an anchor arrived
   with a real newline, so the anchor count was 0 while the head anchor in the
   same script matched. The script REFUSED and wrote nothing, which is the only
   reason this is a note and not a corrupt `src/nifskope_ui.cpp`. Rewritten as
   `scratchpad/defaults1_20260912/fix_tickedbox.py` with the Write tool, and the
   anchor re-chosen so it quotes no escape sequence at all -- the better fix,
   because an anchor that cannot carry a backslash cannot lose one.

2. **I nearly shipped a latent bug by keeping `idColor` inside the identity
   gate.** Decoupling the manifest sidecar from `opts.identity` meant moving
   several blocks out of `if ( opts.identity )` in `lodgen.cpp`, and the obvious
   reading is that `idColor` -- a vertex COLOUR -- belongs with the vertex
   colours and stays gated. It does not: `lodgenNativeLighting()` reads
   `bucket.col` to key the object index for the native `.lodo` / `.lodi`
   writers, so with identity off every vertex would have been keyed to object
   65535 and the FO4CS data would have been silently wrong while every legacy
   byte gate stayed green. Caught by asking, for each block being ungated, WHO
   ELSE READS IT -- not by a test, because no test existed that would have
   failed. THE RULE: when a flag stops gating a block, list every reader of the
   values that block computes before moving it, and if a reader is on the other
   side of the flag say so in the diff. Gate (e) of
   `tests/spells/lodgen_defaults.sh` now exists precisely so this class of
   error cannot be silent again.

3. **Two renders in a row measured something other than what I said they
   measured, and both times the frame looked plausible.** First, the `.BTR`
   pictures came back as one flat colour because the camera was pinned at the
   `.BTO`'s WORLD centre: a `.BTO` is in world units, a `.BTR`'s Land shape is
   in the FILE's own space (bounding sphere centre 2048,2048), and the `chunk`
   node scales the 4096-unit box by 4, so the camera is 8192,8192,0 at ortho
   half-width 8192. Second -- worse, because the frame was a perfectly good
   picture of the wrong file -- the paint-1 and paint-0 renders came back
   PIXEL-IDENTICAL (difference bounding box None) while the two sheets differ
   by 8,837 of 174,888 bytes, because `WW_LODGEN_RESOURCES="$root;$DATA"` lets
   the game's own `Commonwealth.4.-20.20.DDS` win the lookup; every panel was
   the SHIPPED sheet. Found by a swap test: rendering one bake's `.BTR` against
   the OTHER bake's resource root gave a byte-identical frame, which is
   impossible if the root is being read. THE RULE: a render that is meant to
   show a bake's own texture gets the shim root ALONE, never with the vanilla
   Data appended, and every picture pair is checked with a difference number
   BEFORE it is described -- an identical pair is a broken measurement, not a
   result.

4. **`git show HEAD:` was used as the "before" column of CHANGED_FILES.txt in
   a shared tree that is many lanes ahead of HEAD.** The table claimed
   `src/lodgen.cpp` went 364,264 -> 594,075 bytes, which is every lane's
   uncommitted work since the last commit and not remotely this lane's diff;
   `MISTAKES.md` read 68,834 -> 428,704 the same way. Nobody was misled because
   I read my own table, but a reader of the report would have been. THE RULE:
   in a shared worktree, "before" is a number this lane MEASURED before it
   edited, or the honest words `not snapshotted`; HEAD may appear only in a
   separately labelled block that says it is a landmark, not a baseline.
