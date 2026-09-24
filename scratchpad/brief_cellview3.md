# Lane CELLVIEW3 -- the cell viewer's three visible faults (BUILD LANE, owns build + exe slots)

## Header
- Tree `E:/Projects/NifskopeWildWastelandEdition`, main. `date` for every timestamp. Never commit, never `git stash`,
  never edit `WW_CHANGES.md` / `HANDOFF.md`. Folder `scratchpad/cellview3_20260919/` (`BUILDING` first, `DONE` last;
  `report.md` incremental, section 0 inside ten tool calls; if you cannot write report.md, write `DELIVERABLE_TEXT.md`;
  `PENDING.md` past half context). Pictures only under that folder, NEVER repo-root `images/`.
- You OWN the build slot and the exe slot. `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` as ITS OWN command
  before every build and exe run. Fallout4 up = park at `BUILD PENDING`, never wait-loop. ONE NifSkope,
  `--port <unused>` + `WW_WINDOW_AT=1960,40`, absolute `E:/...` paths. A NifSkope without `--port` is bungo's: never
  kill it; rename the exe aside at link time. Build: MSYS2 UCRT64, skill `nifskope-ww-build-verify`. Rung ONCE
  `release/NifSkope.before_cellview3.exe`; never touch another rung. NEVER run any `release/NifSkope.before_*.exe`
  with a GUI (older rungs write bungo's real Recent Files list). A red-control on an old exe is done by reasoning
  or by a source revert in your own build, not by launching a rung.
- Exe at launch: 23,619,072 B, 2026-09-19 18:14:53, sha1 ef4dab1f9c8a819e8692e4bd40e430d96e7329b8.
- ANOTHER LANE (IMPOSTORFIX4) is live and OFFLINE ONLY (python on files). It does not build or run the exe. Do not
  touch `src/impostor*`, `src/gl/impostordraw.*`, `res/shaders/impostor_oct.*`, gltf*, bodybuild*, harnesswindow*.
- Read first: `CONSTITUTION.md`; HANDOFF top block; MISTAKES.md top 10;
  `scratchpad/cellview2b_20260919/DELIVERABLE_TEXT.md`; `scratchpad/cellview2_20260919/PENDING.md`; skills
  `nifskope-ww-build-verify`, `ww-test-harness-add`, `nifskope-ww-render-shot`, `ww-independent-placement-check`,
  `ww-gate-owns-its-fixtures`, `ww-spec-gate-audit`.

## The work, in this order (repairs, no toggles, no INI keys)
1. IDENTITY OVERLAY: 154 of 240 Sanctuary (-20,7) placements land in the grey "unknown" bucket. FIRST decide by
   measurement which it is: (a) those references truly have no LOD identity (no LOD model / not in the `.lodi`) and
   grey is CORRECT; (b) the join from reference to `.lodi` row loses them (form id load-order byte, master index,
   persistent vs temporary refs, the `(chunk<<16)|group` key, chunk the ref falls in vs the chunk the lane baked);
   (c) the `.lodi` used was baked for too few chunks. Table: per record type and per has-LOD-model yes/no, how many
   joined / did not. Use an independent count (python over the `.lodi` + the plugin, skill
   `ww-independent-placement-check`) -- not the viewer's own number. Repair (b)/(c) if found. If (a): split the
   bucket in two in legend and colour -- "no LOD model (correct)" vs "has a LOD model but no group (a defect)".
   The legend's printed rgb must equal the colour drawn (today it prints mauve for a grey bucket); add a gate row
   that samples the picture.
2. BARE GROUND QUADS: 133 of 1024 quads have no BTXT and no ATXT layer over the threshold. Find what the game does
   there (the LAND/LTEX default: worldspace/default landscape texture -- xEdit definitions and the vanilla records
   are the authority, quote them) and do that. Then repair gate row 7: its floor is the whole population (1024 of
   1024 while named "most"); set the floor from measurement after the repair and state the rule used. Never lower a
   floor to pass; a floor that was mis-set is corrected with the reasoning written above it.
   Layer edges: blend by the VTXT per-vertex opacity if the data is already read (say what it costs); if that is
   more than a small change, report it and leave it.
3. MAGENTA DOWNTOWN: candidate cause = `.bgem` effect materials never read by `lodgenLoadModel`. Confirm or clear on
   ONE named car with numbers (which shape, which material path, what the loader returns). If confirmed, the
   smallest honest repair: read the `.bgem` base texture so the shape draws textured, or draw an un-readable
   material as neutral grey and COUNT it in the cell's report line -- magenta is reserved for a genuinely missing
   file. Say which you did and why.
4. Only if 1-3 are closed with context to spare: a pick screenshot that shows the GL view together with the dock
   (`QWidget::grab` cannot; compose from `grabFramebuffer` + the dock grab, or `QScreen::grabWindow`).
5. Gates: `cell_pick.sh`, `cell_open.sh` before/after; neighbours `render_shot.sh`, `harness_window.sh`,
   `native_open.sh`; say why each was picked. Every new row must be shown failing on the pre-repair state.
6. Pictures for bungo, before | after, same camera: Sanctuary -20,7 ground, Sanctuary identity with legend, the
   downtown cell with the cars. LOOK at each and describe in plain words what is still wrong.

## Rules
Authored LOD models only, never decimate; `--road-detail 1` always; masters ship OFF (the overlay is a view mode,
already has its menu row); no "fixed/final/true" -- mechanism + refuter; never pick the flattering view; plain words.

## Report
Exe mtime/size/sha1; the identity join table; gate counts before -> after; WW_CHANGES + HANDOFF text for the
director; your MISTAKES appended to root MISTAKES.md (top of file CRLF, byte splice, print CR before/after); skill
text for any repeatable procedure, written to BOTH trees (`.claude/skills` in the repo AND
`E:/Projects/Claude/.claude/skills`, equal sha1 printed; FO4-general ones also `E:/Tools/AISkills`).
Final message under 250 words.
