# LANE LEDGERFIX1 -- rebuild the NifSkope ledgers, then commit and push (bungo: minimal path)

Header: cwd E:\Projects\NifskopeWildWastelandEdition (branch main). No build, no GUI launch. Launched 16:46 2026-09-24.
Model Opus 5.5.

## Owner's words (bungo, 2026-09-24, verbatim as recorded)
"just go with the minimal path" -- then COMMIT AND PUSH to main.

## The work
Your full instructions are E:\Projects\NifskopeWildWastelandEdition\scratchpad\RECOVERY_LEDGERS_20260924.md, steps 2-7
(step 1 is done). Read it first, then scratchpad\RESUME_20260924.md, then CONSTITUTION.md.
Current-state facts to add to the new HANDOFF top block beyond RESUME_20260924.md: the ledger wipe and this recovery;
the next NifSkope lane after this one is FOG (scratchpad/pbrprep1_20260924/spec_fog.md; sky stays unfogged).
Load skills: `nifskope-ww-commit`, `search-lean`, `ww-contract-provenance` only if you touch a contract page.

## Hard rules learned today (the wipe)
- NEVER open a file for writing in the same expression that reads it. Read into a variable, close, then write.
  Before every write of HANDOFF.md / WW_CHANGES.md / MISTAKES.md assert the new content is LONGER than the old.
- Line endings: measure with Python byte counts only (grep/heredocs lie). Keep each file's existing endings;
  mixed files get a binary splice. `git diff --numstat` BEFORE committing: a ledger edit must show additions,
  never a whole-file delete+add.
- Public wording: this repo is public. It never names the engine symbol source (use "Todd's treat"), never says
  "leaked". Run the gate grep from scratchpad/pdbscrub1_20260924/DONE.md over everything you stage. No game data
  (.esm/.esp/.esl/.ba2/.bsa, vanilla .nif/.dds/.hkx extracts), no binaries, nothing over 5 MB, scratchpad TEXT only.
  brief_pdbscrub1.md stays ignored. No Mixamo files.
- Commit by explicit path list only, never -a / -A / `git add .`. Small commits (ledgers first, then .gitignore, then
  the rest) so an interruption loses nothing. Push to origin main at the end; read back `git status -sb` showing
  in sync, and `git ls-remote origin main` equal to local HEAD.
- Harden splice_lane.py / splice_codex.py as step 5 says (refuse empty anchor, refuse a target under 10 kB).

## Gates
- HANDOFF.md, WW_CHANGES.md, MISTAKES.md each larger than their 720762a size, top block dated 2026-09-24 (time READ from
  `date`, never typed), each of the 9 lane lines + 3 director lines from step 3 present (list them with line numbers).
- Wording gate: zero hits in staged content. Size gate: no staged file over 5 MB; no path matching the game-data list.
- Pushed: remote HEAD == local HEAD.

## Report
Path: E:\Projects\NifskopeWildWastelandEdition\scratchpad\ledgerfix1_20260924\DONE.md, written incrementally (one line per
step as it completes). Sections: 1 skills loaded; 2 what was restored from where (sizes, dates); 3 lines re-added;
4 .gitignore changes and what stayed untracked (count + total size); 5 commits (sha + one line each) and push read-back;
6 anything you could not do and why.
Last section: the skills you loaded, the skills you wished had existed, and the ones you wrote. A procedure you
re-derived from memory or worked out from first principles is a missing skill -- write it under
`E:\Projects\Claude\.claude\skills\<name>\SKILL.md` before you finish. Declining is allowed: name the procedure and
say why it will not recur.
Final message to the overseer: under 300 words, verdict first.
