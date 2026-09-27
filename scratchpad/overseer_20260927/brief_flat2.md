# Lane FLAT2 -- one-colour terrain chunk sheets stored as one value

Worktree: E:\Projects\NifskopeWWE-flat2, branch flat2-20260927 from night-trial @ 5b338d39 (already created).
FIRST read the shared rules: C:\Users\bungo\AppData\Local\Temp\claude\E--Projects-Claude\b560e4ec-6e66-4c21-9572-1ad4acca0043\scratchpad\night_rules.md
Report: scratchpad\flat2_20260927\DONE.md in the worktree (incremental; commit by path as soon as a step compiles).

## His words (2026-09-27, verbatim)
"And our bakes are optimized? If there is a single color on the whole map, it gets reduced in size? Since it doesn't
need to render 1024 for a whole one color mask" / "on the whole chunk I mean" / "And it'd be merged at the end?"

## What exists
- The water planes (id / flow / shore) in the .lodl v3 plane container already store a uniform tile as a 16-byte
  directory entry with no payload (src/watermark.cpp ~181, lodtfile.cpp readPlaneStore). Reuse that idea/format style.
- TIDY1 (in night-trial) drops whole emissive files that are all black (lodgenLayersBlack, lodgen.cpp ~17073).
- NOT collapsed today: the terrain VT per-chunk sheets (colour, _msn normal, mask/_gsaos-style sheets, any other
  per-chunk page -- list them all from the writer, check-existing-first) and any per-chunk mesh texture page. Each is
  written full size (e.g. 1024) even when the whole chunk is one value (open sea, flat uniform ground).

## The work
1. MEASURE first, on the installed whole-map bake (read-only, mods\FO4CSLOD) or a fresh Boston + one sea-heavy region
   bake: per sheet kind, how many chunk sheets are (a) bit-exactly uniform after decode, (b) uniform within the codec's
   own error (every BC block decodes to within 1 level of one value), and the bytes they occupy. Whole-map estimate.
   If the saving is under 1% of the terrain bake, stop and report the numbers (declining is allowed).
2. If worth it: a uniform chunk sheet is not written; the chunk's record carries a flag + the one value per channel
   (RGBA8 or the channel's native form). Choose the smallest format change; follow the rules' format-change clause and
   do NOT collide with GROUND1's .lodi v12 or WATER1's v3 (both in night-trial). The viewer (and every in-tree reader)
   draws the value for flagged chunks. List the FO4CS reader change in DELIVERABLE_TEXT.
3. Also a uniform whole MIP TAIL is irrelevant -- only whole-chunk sheets count. Do not change per-texel content.
4. Switch: on by default (it is a size fix, not a feature); `--no-collapse-uniform` (gate only) = today's bytes.

## Gates
- Off = night-trial's bake byte for byte.
- On: every collapsed sheet decodes to exactly the stored value (case a) or within the codec's own error (case b --
  prove no texel moves more than it already did through BC); every non-uniform sheet byte-identical; counts + bytes
  saved (Boston, sea region, whole-map estimate); viewer renders of a collapsed chunk pixel-identical (or within 1
  level) to today's. A refuter: a deliberately non-uniform sheet must NOT collapse (prove the test fails on it).
- Build green; bake time unchanged within noise.

## Pictures (full size, 60 px title bar, never a contact sheet)
One map view marking which chunks collapsed (by sheet kind) over the whole Commonwealth, and one before/after of a
sea-edge area in the default view (should look identical).

## Rules
- NifSkope runs: `bash E:/Projects/NifskopeWWE-fix1/scratchpad/fix1_20260926/turn.sh acquire flat2 21600`, release
  after EACH run; lanes GATES, WATER1, TILING5 share it. Second monitor (WW_WINDOW_AT=1920,0), unused --port, one
  instance. Avast auto-sandboxes fresh exe copies (silent rc 0 exit, empty log): reuse a copy that already ran; skill
  ww-gui-launch-silent-exit. No retry loops.
- Build: nifskope-ww-worktree-build skill (object source: NifskopeWWE-night is built at night-trial @ 5b338d39,
  section 5b). ONE build at a time on the machine: check no make/g++/cc1plus is running first. Game gate first.
- Never kill anything; bungo's NifSkope (main tree) stays untouched. Commit by explicit path; never -a/-A, stash, push.
- Never write under E:\Projects\Fallout 4 Mods\ (the installed bake is read-only to you).
- Public repo: no game data, no binaries, no PDB names.
- Report sections: 1 skills loaded; 2 measurement table; 3 format choice + reader impact; 4 gates table
  (expected/measured/PASS-FAIL); 5 pictures; 6 DELIVERABLE_TEXT (WW_CHANGES + HANDOFF entries); LAST: skills used /
  wished / written (write missing ones under E:\Projects\Claude\.claude\skills\<name>\SKILL.md, copy to E:\Tools\AISkills).
