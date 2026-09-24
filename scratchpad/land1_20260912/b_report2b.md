
## B9 Finished-work review against the skills this part was told to use

**`ww-prototype-is-not-the-product`.** Met, and it is the spine of Part B. The
edits are made **in a real copy of `Fallout4.esm`** by `b_esmedit.py` -- a
compressed `LAND` height raised with the GRUP size chain fixed up, a `REFR`
moved -- not by a loose-file override, because rows 1, 2, 3, 5 and 7 of the
dependency map live in the ESM and nothing else can reach them. Every comparison
is between two output trees the shipping exe actually wrote. **Where the skill
was nearly broken**: the first `refs` arm moved a reference the bake does not
draw, so the "test" compared a plugin edit that never reached the product. That
is the prototype trap wearing a real-plugin costume, and the per-arm floor is
what caught it.

**`ww-control-calibration`.** Met in three places, and each earned its keep. The
`null` arm is the instrument's known answer -- nothing changed, so the full bake
must not move and the incremental run must rebake nothing *and still match*; if
it moved, the arm reports **BROKEN CONTROL**, not PASS. The `merge-ok` arm in B2
is a negative control proving the default command does **not** refuse, and
without it all four refusal arms would have passed on the build that refused
everything. And B4's determinism check is two independent full bakes compared to
each other, not a claim about the writer.

**`ww-spec-gate-audit`.** Met on the ordering that matters: the dependency map
(B1) was written **before** the code, and the gate's verdicts are reported
including the two arms that were passing for reasons they had not asked about
(B2.1) and the arm whose floor was a lie (B3.1). **The audit's honest finding is
that two of B1's own claims did not survive the code** -- the merge is not a
whole-region pass, and a missing output is not a refusal -- and both are
corrected in place at B1.6 rather than edited out of the text above them.

**`ww-texel-picture`.** Met: real `.DDS` texels off disk, the window and the x8
gain both stated, sha1s beside the panels so the picture is never the proof, and
the dirty grids computed from logged data with a refusal if they disagree with
the exe.

**`nifskope-ww-build-verify`.** Met: the final exe is stamped (08:42:33,
21,935,616 B, sha1 `1e4e2c5cc5a0f34e058dbe67a9ac6fd9d52d8968`) and its **objects**
were checked -- `lodgen.o` and `nifcli.o` both 08:42:31 -- rather than
`exe -nt src`, which a link triggered by the other translation unit satisfies
just as well. That check is in `MISTAKES_ENTRIES.md` because without it at least
two of this lane's gate runs would have measured a previous exe.

**`nifskope-ww-lodgen`.** Met: `--road-detail 1` on every bake and picture;
region bakes only; Fallout4.exe confirmed down before every build and every exe
launch; no UI file touched and `E:/Projects/NifskopeWWE_ui` never entered;
nothing committed and `git stash` never run; patch scripts written with the Write
tool rather than heredocs, and the one place a heredoc was tried anyway
(`b_addmoveid.py`, first attempt) failed on exactly the backslash rule the skill
names.
