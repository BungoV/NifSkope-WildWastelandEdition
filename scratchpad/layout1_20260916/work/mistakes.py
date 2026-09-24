"""Lane LAYOUT1 (2026-09-16): the lane's own entries at the top of MISTAKES.md."""
P = 'E:/Projects/NifskopeWildWastelandEdition/MISTAKES.md'
ANCHOR = '## 2026-09-16 17:0x -- lane BTOFREE1'
NEW = '''## 2026-09-16 20:4x -- lane LAYOUT1 (every FO4CS output under `Data/FO4CSLOD/`)

1. **Moved a file and left the ledger pointing at where it used to be, which
   cost the entry its digest and would have made every later `--incremental`
   run rebake the whole region in silence.**
   The `.BTO` manifest sidecar moved under `FO4CSLOD/<ws>/` with everything else,
   and the teardown in `src/nifcli.cpp` was changed to drop it there -- but the
   ledger callback fifty lines above still composed the sidecar's path from the
   mod folder's root. Nothing writes there any more, so the ledger recorded
   `"Commonwealth.4.-20.24.BTO.manifest.txt "` with an EMPTY sha1 instead of
   `"... 2ec14d0b0103..."`. An empty digest never matches, so the chunk would
   have been dirty forever, and NOTHING would have said so: no error, no
   warning, just a bake that quietly never reuses anything.
   **How it was found:** the byte-identity gate (step 2 of the brief) compared
   the rung's `.lodb` with the new one, and the ledger was the single file out
   of 84 that did not normalise. It was found because the gate compares EVERY
   file the bake wrote rather than the ones the lane expected to change.
   **The rule:** when a written path moves, the paths RECORDED INSIDE other
   files move with it, and the same function composes both. A grep for the moved
   spelling is not enough -- `outDir + "/" + name` does not contain the old
   spelling and still names the old place. The new ledger row in
   `tests/spells/lodgen_layout.sh` leg (a) resolves and re-hashes every entry,
   so this class fails loudly from now on.

2. **Wrote a shell harness through a heredoc twice more and shipped two
   literal `\n` into `lodgen_byte_gate.sh`, where they did not fail -- they
   passed the letter `n` as an argument and the comparison read MISSING.**
   The trap is in this ledger three times already. It bit again because the
   halved backslash landed in a LINE CONTINUATION, where the result is still
   valid shell: `cmpone "$A" \n\t\t\t"$B"` runs `cmpone` with three
   arguments, the second being `n`, and `cmpone` reports a missing file rather
   than a syntax error. `bash -n` passes it. A gate that quietly compares the
   wrong thing is worse than one that crashes.
   **How it was found:** re-reading the phase I had just re-based, with
   `cat -A`, because the previous entry in this ledger told me to stop trusting
   what a heredoc wrote.
   **The rule, sharpened:** every harness edit goes through a Python patch file
   with `count == 1` assertions AND is read back with `cat -A` afterwards.
   `bash -n` is not a check for this class.

3. **Ran three failed builds in a row by guessing at the toolchain
   environment instead of reading how the previous build in this same session
   had been invoked.**
   `make` was not on the Bash tool's PATH (127), then g++ could not write a
   temporary file because TMP pointed at `C:\Windows`, then the link step
   failed because `git` was not on PATH for the revision stamp. Each was a
   thirty-second failure, but together they were four minutes and four
   notifications for a build whose recipe was already written down in
   `scratchpad/layout1_20260916/work/build1.log`.
   **How it was found:** the third `Error 127` in a row.
   **The rule:** before re-running a build in a session that has already built
   once, read the LAST SUCCESSFUL invocation out of its own log and repeat it,
   rather than reconstructing the environment from memory.

'''
s = open(P, encoding='utf-8', newline='').read()
assert s.count(ANCHOR) == 1, s.count(ANCHOR)
s = s.replace(ANCHOR, NEW + ANCHOR, 1)
open(P, 'w', encoding='utf-8', newline='').write(s)
d = open(P, 'rb').read()
print('ok CR %d LF %d' % (d.count(b'\r'), d.count(b'\n')))
