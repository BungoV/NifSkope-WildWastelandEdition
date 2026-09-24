# Lane CHANVIEW1 -- every baked channel of chunk 4.4.-12 shown, one picture each, same framing as the AO

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main, ONLY lane in the tree (starts AFTER SLAB1 lands; the
  director launches you). Exe at launch: SLAB1's final `release/NifSkope.exe` (read mtime, size, sha1 yourself; first
  line of the report). Rung ONCE before your first build: `release/NifSkope.before_chanview1.exe` (never delete any
  `release/NifSkope.before_*.exe`, `release/NifSkope.archlock1_rung.exe`, `release/NifSkope.at_0117.exe`, or a
  `NifSkope_inuse_*.exe`). Markers `scratchpad/chanview1_20260918/BUILDING` (touch FIRST) / `DONE` (first word
  `chanview`). Report `scratchpad/chanview1_20260918/lane_chanview1_report.md`, INCREMENTAL; `PENDING.md` past half
  your context. Never commit, never `git stash`, never edit `WW_CHANGES.md` or `HANDOFF.md`.
- Game: `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` is ITS OWN command before every build and every exe
  run; Fallout4 up = stop, PENDING.md. A NifSkope with no `--port` is bungo's window: rename the exe aside as
  `NifSkope_inuse_<pid>.exe` at link time, never kill it. Headless runs: `--port <unused>` + `WW_WINDOW_AT=1960,40`,
  one at a time, second monitor only, every path ABSOLUTE `E:/...`.
- Build: MSYS2 UCRT64, `export PATH=/ucrt64/bin:/e/Tools/GIT/mingw64/bin:$PATH`, `mingw32-make -f Makefile.Release -j8`,
  make's exit code is the gate (`nifskope-ww-build-verify`). ONE background waiter at a time. `date` for every
  timestamp.
- Read first: `CONSTITUTION.md`; `HANDOFF.md` top block (the SLAB1 LANDED block and the hotfix 7/7b/7c/7d entries);
  root `MISTAKES.md` top 15 (05:1x: a file byte is not a pixel; 05:0x: a proxy plane shown as the channel);
  `src/lodinative.cpp` IN FULL (the WW_LODL_AO seam ~482-500 and the note lines ~866-880: that is the ONE seam you
  extend); `docs/LODGEN_NATIVE_LODO_LODI.md` s2 (the `.lodo` vertex layout: `sway` at 0x0E, `selfAO`), s4 (the
  `.lodi` placement record: `sky` 0x11, `ground` 0x12, `seed` 0x13, `identity`, the v6 vertex-AO stream), s4.3
  (seed is NOT the yaw); `docs/LODGEN_TERRAIN_VT.md` v2 header (role 5 mask = R roughness G metallic B AO A ground
  cover; role 6 emissive; wetness and shore DROPPED by ruling); `src/btdterrain.cpp` where WW_LODL_AO samples the
  mask sheet's B bilinearly (hotfix 7b) -- that sampler takes a channel index, or you give it one;
  `scratchpad/viewfix_20260917/render_slots_e.sh` (the framing script) and the hotfix 7c/7d picture logs under
  `scratchpad/viewfix_20260917/images/*.log` for the exact `full` and `close` arguments.
- Skills (repo `.claude/skills`): `nifskope-ww-render-shot` (read its first section before any loop; channels 1..9
  exist for the STOCK .bto path -- mirror their numbering where a channel means the same thing), `ww-texel-picture`,
  `nifskope-ww-build-verify`, `ww-test-harness-add`, `ww-control-calibration`. Any procedure you invent twice becomes
  a skill under `scratchpad/chanview1_20260918/skills_proposed/<name>/SKILL.md` with frontmatter.

## bungo's words, verbatim (2026-09-18 06:0x)
"Beyond the AO, you will also now show me on the same chunk: Leaf sway bake, identity bake, sky visibility bake.
Anything else I'm missing?" The director's answer named ground-contact blend, the mask sheet's ground cover, and the
roughness/metallic channels as the ones his list lacked, and told him wetness and shore are not baked on the native
path by ruling. Standing: no default moves, no format change, `--road-detail 1`, pictures never from the desktop.

bungo 06:1x, same round: "show me also ground contact blend, ground cover, and roughness / metallic or specular /
gloss in this case, since these are baked from legacy textures and materials, not .pbrm". So: the mask-r / mask-g
captions and note lines NAME THE RULE that served each layer, from the sheet index's per-layer census
(`docs/LODGEN_TERRAIN_VT.md` v2: legacy gloss inverted into roughness, metallic 0, vs a `.pbrm` layer); a chunk
whose metallic is constant 0 is pictured and captioned "constant 0 (legacy materials)"; and one extra picture shows
the SOURCE gloss beside the baked roughness for one layer so the inversion can be read.

## The work (in this order; each step lands in the report before the next)
1. **The bake is SLAB1's after-bake**, `scratchpad/slab1_20260918/after/` (chunk 4.4.-12, the chunkD3 recipe with the
   slab lattice). Do not re-bake unless a file it needs is missing; if you must, the identical command, and say so.
   Read its `.lodi`/`.lodo`/sheets with the existing Python readers (`tests/spells/lodgen_native_decode.py`,
   `lodgen_vt_check.py`; one reader per format) and TABLE, per channel, min / max / mean / count-of-distinct over the
   chunk BEFORE any picture: identity, sky, ground, seed (per placement); sway, selfAO (per vertex, per drawn slot
   mesh); v6 scene AO (per instance vertex); mask sheet R, G, B, A; emissive present or absent; normal sheet.
   A channel that is constant over the chunk is reported as constant and STILL pictured (the picture then proves the
   constant, and the note line says "constant N").
2. **One viewer switch, `WW_LODL_CHANNEL=<name>`**, on the native path, at the same seam WW_LODL_AO uses (WW_LODL_AO=1
   stays exactly what it is: `ao`). Names, and what each draws flat as vertex colour on the placements, terrain
   untouched unless stated: `identity` (hashed colour per placement identity, the stock channel 1 look; and
   `identityraw` = the raw value greyscale), `sky` (per-placement byte grey), `ground` (per-placement byte grey, AND
   the terrain drawn in the same grey ramp so the blend can be read against it), `seed` (hashed colour per tree, 0 =
   black for non-trees), `sway` (per-vertex weight grey on the drawn mesh: trunk dark, crown light), `selfao` (the
   `.lodo` byte alone), `ao` (= WW_LODL_AO today, unchanged bytes). And on the TERRAIN, through the sheet sampler:
   `mask-r` roughness, `mask-g` metallic, `mask-b` AO (= today's terrain AO read), `mask-a` ground cover, `emissive`
   (or the note line "emissive sheet absent"), `normal` (the model-space normal sheet as RGB, no lighting). Every
   channel writes a note line the way WW_LODL_AO does: channel name, source file, N placements or texels read, min,
   max, mean -- READ BACK from what was uploaded, not from intent. An unknown name refuses by name and draws nothing
   different (the default render).
3. **The refuter per channel** before any picture goes out: for each channel, two renders that MUST differ -- the
   channel against the default render, pixel-difference > 0 -- and the note line's mean must equal the table's mean
   from step 1 within rounding. A channel whose two renders are byte-identical is NOT WIRED (MISTAKES 05:1x); fix it,
   never caption it.
4. **The pictures**, `scratchpad/chanview1_20260918/images/`: for EVERY channel above, the `full` and the `close`
   framing of `render_slots_e.sh` (close = `24900 -41300 2600 8 450`; full = the hotfix 7c arguments), SHEETS and
   LODI at SLAB1's after-bake, `WW_RENDER_FLAT=1` where the channel is a flat byte, texturing ON for `normal` and
   `emissive` (skill section on channels 8/9 says why alpha test needs it). File names `chunk_<channel>_{full,close}.png`.
   One contact sheet `channels_contact.png`, all channels at the close framing in a labelled grid (channel name, the
   note line's min/max/mean under each cell), through `ww-texel-picture`'s caption rules (the number in the caption is
   the number in the report). Plus the mask sheet's four channels as TEXEL crops of the same 256x256 texel window under
   the deck, side by side, one picture.
5. **Gate** `tests/spells/lodl_channels.sh` (`ww-test-harness-add`): (a) every channel name renders and its note
   line is present with N > 0 (or "constant"/"absent" by name); (b) each channel's render differs from the default
   render (pixel diff > 0) -- the floor that fails on an unwired channel; (c) the note-line mean equals the Python
   reader's mean within 1 for every per-placement and per-vertex channel; (d) `ao` with WW_LODL_CHANNEL unset equals
   `WW_LODL_AO=1` byte-for-byte (the way back is unchanged); (e) an unknown name refuses by name, render identical to
   default. Neighbours on your final exe, before/after with owners: `render_shot.sh` (82/0), `lodl_open.sh` (23/0),
   `native_open.sh` (17/0/2), `native_lighting.sh` (14/0), `lodgen_native.sh`.
6. **Docs**: the channel table in `docs/LODGEN_NATIVE_LODO_LODI.md` (a viewer section, provenance per line) and the
   `nifskope-ww-render-shot` skill (repo copy) gains the native channel list beside the stock one -- deliver the skill
   text in the report; the director applies it to both trees.

## Gates (pre-registered)
- G1 every channel: render != default (pixel diff > 0), note line present, mean == reader mean within 1.
- G2 `ao` unchanged: WW_LODL_AO=1 render byte-identical to before the lane (rung exe, same bake, same framing).
- G3 every picture's caption number is the report's number; 13+ channels x 2 framings + contact + texel crop on disk.
- G4 no bake output changed: the lane is viewer-only; `find scratchpad/slab1_20260918/after -newer <launch stamp>` empty.
- G5 neighbours unchanged.

## Rules
- Viewer only. No writer, no format, no bake default is touched. `src/lodinative.cpp`, `src/btdterrain.cpp` (sampler
  channel index), `src/glview.cpp` only if the seam forces it (say why). Terrain sheets are read through the existing
  sampler; no second decoder.
- A picture of a channel reads the channel the SHIPPED file carries, never a proxy of it (MISTAKES 05:0x).
- Never a desktop capture; one instance; second monitor.

## Report (`lane_chanview1_report.md`, incremental)
0 exe at launch + rung; 1 the channel table from the files; 2 the switch (names, seam, note-line format); 3 the
refuter table (per channel: pixel diff vs default, note mean vs reader mean); 4 pictures with captions; 5 gate counts
and each refuter shown red; 6 neighbours; 7 build (mtime, size, sha1, `find src tests res -newer` empty); 8 docs +
skill text; 9 WW_CHANGES paragraph + HANDOFF LANDED block for the director to splice; 10 rows for bungo (anything you
chose he might rule on: the identity hash palette, the ground ramp colouring); 11 MISTAKES entries (write them at
the top of root MISTAKES.md yourself, the moment you see one); 12 finished-work skill review. END with `DONE`
(first word `chanview`) and five plain sentences for bungo, ending with: his open window needs a restart.
