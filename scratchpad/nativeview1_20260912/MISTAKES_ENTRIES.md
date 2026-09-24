## 2026-09-12 -- lane NATIVEVIEW1 (the viewer opens the native bake)

1. **The game check and the exe launch were the SAME shell command, so the
   launch could not act on the check.** The brief says `tasklist | grep -i
   Fallout4` before every exe launch, up = stop. I wrote
   `tasklist | grep -i Fallout4 ; <render command>` in one Bash call: the grep
   printed `Fallout4.exe 55208 ... 7,552,500 K` and the render had ALREADY run
   by the time I read it. bungo was in the game. Nothing was harmed (a no-GUI
   render off a second monitor slot), but the gate was decorative, not a gate.
   THE RULE: a precondition check is its OWN command and its output is READ
   before the guarded command is typed. A check whose result arrives after the
   action it guards is not a check.

2. **A bash heredoc halved every backslash in a hook-up script -- twice.**
   `python - <<'PY'` carrying `b'\tsrc/lodifile.h \\\n'` arrived with one
   backslash where two were written, so the anchor never matched
   (`AssertionError: 0`); the second time the same trap hit a patch string
   containing `"\\r\\n"`. The repo skill `ww-anchored-hookup` says in so many
   words that hook-up scripts are written with the Write tool and never a
   heredoc. THE RULE, restated because I broke it after reading it: anything
   carrying a backslash goes through Write/Edit. Not "usually" -- always.

3. **Anchor counting cancelled a real hit to zero on a mixed-ending file.**
   `hits_lf = data.count(anchor_lf) - data.count(anchor_crlf)` is arithmetic,
   not searching: on `src/nifskope.cpp` (9,560 CR against 10,708 LF) it went
   negative and the sum reported "anchor matches 0 times" for all six anchors,
   which read as a broken script rather than a broken counter. Fixed by a
   `find_all()` that matches each newline as `\r?\n` and replaces THE TEXT
   ACTUALLY FOUND, so the file's own endings survive. THE RULE: on a mixed file
   you search for both forms and keep what you found; you never subtract counts.

4. **The idempotence probe matched the anchor's own first line, so three edits
   were silently SKIPPED and reported as "already present".** The probe was the
   addition's first line -- which for a `replace` edit is by construction the
   anchor's first line too. Fixed to the first line the ADDITION has that the
   ANCHOR does not, compared as a WHOLE LINE (a substring test still matched,
   because `... == 0` is a prefix of `... == 0 ) ) {`). THE RULE: an
   already-applied probe must be a line only the applied state has, and line
   comparisons are whole-line comparisons.

5. **I prepended `materials\` to the `.lodo` material strings and broke the
   lookup I was trying to make work.** The 136 material strings in this bake
   are absolute build-machine paths
   (`C:\Projects\Fallout4\Build\PC\Data\Materials\LOD\RockSlab01LOD.BGSM`).
   `Game::GameManager::get_full_path` (`src/gamemanager.cpp` 598-628) searches
   for the archive folder at any `/` boundary and ERASES everything before it,
   so the raw path resolves by itself; putting `materials\` at offset 0 makes
   the search stop at `n == 0`, nothing is erased, and the lookup misses.
   Measured, not guessed: the strings were dumped and the resolver read. THE
   RULE: read the resolver before normalising a path for it.

6. **I assumed the `.BTR` and the `.BTO` of one chunk share a coordinate space,
   and photographed the `.BTO` at the wrong camera.** The legacy control frame
   came back as an empty 5,179-byte PNG and gate (c) read IoU 0.0000, which I
   first read as "the native scene is wrong". It was the camera. Read from the
   files: the `.BTR`'s shape sits at translation 0,0,0 over chunk-local vertices
   (0..4096 with a shape scale of 4), so it needs a CHUNK-LOCAL camera
   (`8192,8192,0`); the `.BTO`'s `BSSubIndexTriShape` carries Translation
   (-81920, 98304, 0), so it is in WORLD space and takes the same camera as the
   `.lodi`. Two files of one bake, two spaces. THE RULE: before blaming the
   thing under test for an empty frame, read the translation of the block you
   are photographing. A camera is an input, not a background fact.

7. **`grep -c` prints `0` AND exits 1, so `$(grep -c ... || echo 0)` produced
   `0` twice** and a census capture became the two-line string `0\n0`. The
   comparison then failed with a message that looked like a real count mismatch
   ("0 != 676") in a gate that was otherwise correct. Fixed with
   `X=$(grep -cv '^#' "$F" 2>/dev/null | head -1); X="${X:-0}"` in all three
   captures. THE RULE: `|| echo` after a command that already printed its answer
   appends a second answer. Default an EMPTY capture, do not fall back over a
   non-zero exit.

8. **The heredoc-backslash trap a THIRD time, and this one cost a render run.**
   A patch script for `shots.sh` was passed through an inline `python -c`; the
   continuation backslashes were eaten, the script's asserts never ran on the
   text I thought I had written, the file was left unpatched, and ten frames
   were re-rendered at the old camera before the diff showed it. Entry 2 of this
   same lane is the same mistake. THE RULE, now with a cost attached: patch
   scripts are written with the Write tool, always, and a patch is verified by
   re-reading the target's changed line, not by the patch script's own exit code.

9. **I built a shader block that pointed at a model-space normal map without
   declaring it one, and the land came back 40 percent too dark.** The sheet
   beside a `.lodl` is an `_msn`; read as a tangent-space map it lights wrong
   (mean luma 70.6 against the `.BTR`'s 121.1, MAD 50.457). The answer was in
   the bake's own `.BTR` the whole time: Shader Flags 1 = 0x80401000
   (`Model_Space_Normals` set, `Specular` clear) and Shader Flags 2 = 3
   (`ZBuffer_Write` | `LOD_Landscape`), with the bit meanings read from
   `build/nif.xml`. After setting those three bits: MAD 35.821, mean luma 92.5.
   THE RULE: when a viewer route reproduces a file the bake already writes, read
   that file's OWN flags and copy them; do not hand-write a shader block from
   what the texture slots seem to need.

10. **I appended a document bullet through `python -c "..."` in a DOUBLE-QUOTED
   bash string and every backticked code span was deleted.** The shell ran each
   span as command substitution before python saw the text, so
   `docs/LODGEN_BTD_FORMAT.md` gained a bullet reading "the sheet's is an ." and
   "Shader Flags 1 , which is  set with  clear", while the wrapper printed a
   plausible byte count. Found by reading the inserted lines back. **This exact
   entry is already in this ledger, dated 2026-09-11 (lane UI5-HOVERPIC), and I
   had not read that far down the file.** Repaired with a patch script written
   through the Write tool. THE RULE, again: text carrying backticks, quotes or
   `$` goes into a `.py` file written with the Write tool, never into a
   double-quoted `python -c`; and the inserted lines are read back before the
   edit is believed.
