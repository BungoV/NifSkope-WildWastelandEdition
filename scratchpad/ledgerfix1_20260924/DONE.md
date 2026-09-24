# LEDGERFIX1 DONE (started 2026-09-24 16:47)

## 1 Skills loaded
- nifskope-ww-commit, search-lean

## 2 Restored from where
- Before: HANDOFF/WW_CHANGES/MISTAKES byte-equal to 720762a (git diff --quiet), sizes 139036 / 1392757 / 68834.
- HANDOFF.md <- ~/.claude/file-history/843169d8-.../5fcfe493b278bdd9@v76 (mtime 2026-09-12 08:19, 450000 B, LF-only,
  its last 123102 B equal the 09-09 file's tail; top block written 09-10 17:18, newest dates 09-12).
- WW_CHANGES.md <- scratchpad/build8_20260910/WW_CHANGES.md.bak (content to 09-10, reworded 09-24 14:16; 1490786 B,
  CR 19020 = HEAD's CR count, mixed endings kept).
- MISTAKES.md: no newer copy; stays 09-09 (68834 B).
- Script: scratchpad/ledgerfix1_20260924/restore.py (read, close, assert longer, write, read back).

## 3 Lines re-added (splice_all.py, one pass, 16:52; LF-only splices, CR counts unchanged: HANDOFF 0, WW 19020, MISTAKES 0)
- HANDOFF.md 450000 -> 481076 B (720762a: 139036). New top block at line 3, "## TOP BLOCK -- written 2026-09-24 16:52".
  Lane lines: 55 PBRWX1, 75 TODDSTREAT1, 87 PBRR4, 99 PBRR3, 158 PBRR2B, 185 PBRR2A, 289 PBRLODFIX1, 310 LIGHTANGLES1,
  324 PBRR1. Director lines: 97 RULED 12:2x (a,b,e adopted; c,d held), 98 HELD 12:2x five divergences,
  157 RULED 12:1x (sky unfogged / night light on the sun arc, moon visual only / linear HDR only with Bloom or SSGI).
- WW_CHANGES.md 1490786 -> 1502091 B (720762a: 1392757): recovery entry + 9 lane entries at lines 3-131, tail byte-identical.
- MISTAKES.md 68834 -> 80463 B (720762a: 68834): the wipe entry (step 5) + 9 lane entries at lines 8-128.
- Step 4 wording: 3 rewordings in the restored HANDOFF (lines from 09-10..09-12 naming the symbol source / its tools),
  2 in TODDSTREAT1's text (a dump-pattern list, a paraphrase). Gate grep: HANDOFF 0, MISTAKES 0, WW_CHANGES 6 = the
  6 ordinary "leaked" lines pdbscrub1 already allowed.
- Step 5: splice_lane.py and splice_codex.py (director's session scratchpad 208f5149-.../scratchpad/) hardened:
  read-close-write, refuse anchor < 8 B or not "- ", refuse target < 10 kB, refuse new not longer, CR count fixed,
  read-back. splice_lane.py also fixed: WW entries now go above the first "## " entry (it pointed at a "### "
  mid-file at byte 279797 of the restored file). Proven: empty anchor -> AssertionError, ledgers byte-unchanged.
  scratchpad/_handoff_anchor.txt reset to "- 2026-09-24 15:09 PBRWX1".

## 4 .gitignore and what stays untracked
- .gitignore 2793 -> 4981 B, LF-only, append-only except one line of my own removed (a pattern the wording gate hits).
  Added: game data everywhere (*.esm *.esp *.esl *.ba2 *.bsa); fixtures/*.hkx + fixtures/*.nif (a third-party clip and
  a vanilla body); tests/fixtures/pbr_*_data/ (built from vanilla files by the tracked pbr_r*_fixtures.py);
  scratchpad hkx/mixamo/bgsm/bgem/gltf/glb/fbx/xml/swf/wav/xwm/tri; scratchpad binaries, logs, arrays, backups
  (exe dll o a npz npy pkl log err pre bak* lodj lodb lodm gif mp4 tsv zip 7z pyc btd hdr tga); game-record dumps
  (*_refs.json) and the one >5 MB text dump; skill copies from other projects (xse-plugin, fo4cs-*, core-*, fo4-*,
  behaivor-graph, hudframework, prismaui, rdc-cli, swf) HELD for bungo; five files naming another outside RE source
  HELD for bungo (pdbscrub1 folder, RESUME_20260924.md, brief_pbrwx1.md, two pbrrender0 research notes).
- Untracked-not-ignored before: 18727 paths, ~8.03 GB (4.4 GB of Fallout4.esm copies, 136 files > 5 MB).
  After: 10805 paths to commit (text), ~118 MB total incl. tracked edits; 0 over 5 MB; 0 game-data paths.
  Ignored now (stays on disk, untracked): ~7,900 paths, ~7.9 GB.
- Gates over the staging set: wording regex 19 files = ordinary "leaked" sense + NifSkope.pro's own linker setting
  (all allowed by pdbscrub1); game data 0; > 5 MB 0. The scrub tools that carry the old strings (splice_all.py,
  gate_scan.py) moved to the session scratchpad, outside the repo.
- Line endings: 111 tracked modified, 8 mixed, all CR deltas match the diff; one LF file now CRLF =
  scratchpad/handoff_fo4cs/samples/make_samples.log, a regenerated tool log (79 new lines, native CRLF), committed as is.
- commits so far: a4c2069 ledgers; cea8808 .gitignore; 85c0b14 src/res (203 files); 8fa18e3 tests/tools (178); 5e4f069 docs/skills (103). Note: a4c2069 also carried pdbscrub1's pre-staged rename of the engine comparison doc (content change followed in 5e4f069).
