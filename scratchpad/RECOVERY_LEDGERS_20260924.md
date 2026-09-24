# LEDGER RECOVERY -- MINIMAL PATH (bungo 2026-09-24: "just go with the minimal path"; then COMMIT AND PUSH)

## What happened
13:57 a director one-liner opened HANDOFF.md / WW_CHANGES.md / MISTAKES.md / scratchpad/_handoff_anchor.txt with 'wb'
before reading them -> emptied. Last commit of the three: 720762a (2026-09-09). No other backup exists (bungo).
FO4CS Codex ledgers were NOT affected.

## Done already (director, 2026-09-24)
- Step 1 DONE: the three files are back at their 09-09 committed content (byte-verified). The PBRWX1 line that was
  written into the empty HANDOFF is saved at scratchpad/handoff_after_wipe_pbrwx1_line.md.

## Still to do (next account)
2. SEARCH RESULT (find_ledger_copies_out.txt): HANDOFF newest = ~\.claude\file-history\843169d8-...\5fcfe493b278bdd9@v76
   (09-12 08:19, 450000 B -- verify it is NifSkope's HANDOFF, then use it: +3 days over 09-09). WW_CHANGES newest =
   scratchpad/build8_20260910/WW_CHANGES.md.bak (1490786 B, content ~09-10, already Todd's-treat-reworded by lane r2).
   MISTAKES: no newer copy -> stays 09-09.
   Do NOT dig through transcripts for the gap -- bungo ruled the minimal path; lane folders keep the detail.
3. HANDOFF: write a fresh top block of CURRENT state (sources: scratchpad/RESUME_20260924.md, memory, lane DONE.md files),
   then add tonight's lane lines via splice_lane.py (anchor file must be reset first to the start of the newest status
   line): pbrr1, lightangles1, pbrlodfix1, pbrr2a, pbrr2b, pbrr3, pbrr4, pdbscrub1 (label TODDSTREAT1), pbrwx1.
   Director lines to re-add by hand: RULED 12:1x (sky unfogged / night light follows sun arc, moon visual only / linear
   HDR only while Bloom or SSGI on); HELD 12:2x five divergences; RULED 12:2x (a,b OpenPBR weight + metal F82 tint and
   e Burley stays, in FO4CS + NifSkope; c roughness floor, d emitter widening HELD).
   WW_CHANGES + MISTAKES: splice_lane.py adds tonight's sections in the same pass.
4. Re-apply the Todd's treat wording to these three files (the 09-09 text has the old words again) and run the gate grep
   from scratchpad/pdbscrub1_20260924/DONE.md.
5. MISTAKES entry: "2026-09-24 director -- one-liner opened files 'wb' before reading them and emptied three ledgers
   last committed 09-09. Rule: read into a variable, then write; splice scripts refuse an empty anchor or a target
   under 10 kB; commit ledgers daily." Harden splice_lane.py / splice_codex.py accordingly.
6. BEFORE COMMIT: .gitignore game data everywhere (*.esm *.esp *.esl *.ba2 *.bsa, vanilla .nif/.dds/.hkx extracts) and
   scratchpad binaries (*.exe *.dll *.npz *.npy *.log *.bak* *.lodj, anything > 5 MB) -- keep scratchpad text only.
   Measured: a broad add would be 8 GB incl. 4.4 GB Fallout4.esm copies + 15 files over GitHub's 100 MB limit.
   No game data was ever committed. brief_pdbscrub1.md stays ignored.
7. COMMIT AND PUSH to main (bungo), explicit path lists, public-wording gate clean first.
