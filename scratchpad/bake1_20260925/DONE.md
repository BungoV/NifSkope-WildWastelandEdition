DONE -- lane BAKE1, whole-Commonwealth LOD bake from bungo's MO2 load order into mods\FO4CSLOD (2026-09-25).

Release (run-copy) exe sha1: 27a7bb29c80ac6a37cafa1aa7bdb09128a19b50c (build 8, branch bake1-20260925).

## 1 Skills loaded
nifskope-ww-worktree-build, nifskope-ww-lodgen, nifskope-ww-render-shot, ww-panel-run-harness,
ww-lodl-offline-census, mo2-mod-content-census; read during the crash work: nifskope-ww-crash-diagnose.

## 2 What was built
Five source fixes/features, each a patch script under scratchpad/bake1_20260925/ (anchor count 1, CR count unchanged):
- src/esmdata.{h,cpp} (24f5f39b): the ESM reader joins a LATER plugin's world group and its cell groups
  (including persistent ones) to an overridden cell. Before, the 22,827 REFRs in BNS Trees.esp never reached a bake.
  src/nifcli.cpp: the tree-candidate lister reads the persistent overlay too (36 -> 79 candidates).
- src/lodofile.cpp (ad482f49): the .lodo vertex-fetch remap stays a true permutation when a mesh has vertices
  no triangle uses. BNS LOD trees have them; the old code wrote pos[~0u*3] and crashed (rc 139).
- CLI `--dim all` (23cde121): rings 4+8+16+32 in the panel's queue order in one pass (cards link only at 16/32).
  CLI `--fo4cs-one-root` (opt-in): arrays, manifests and the .lodb go under --native, the panel's layout.
  Both are off by default, so the output is unchanged when they are not given.
- src/nativeemit.cpp (77f6eae5): the vertex-AO loop's LAND read is serialized with a mutex. libfo76utils
  decompresses a record inside the shared reader, so two pool workers corrupted it. That crashed bake run 1
  after 70 min (rc 127, "Qt Concurrent has caught an exception thrown from a worker thread").
- No file format change. Shipped defaults unchanged.

The bake: scratchpad/bake1_20260925/bake.sh (lodl stage, then one chunks stage: VT + height + cover, native
objects, cards + arrays, --dim all, whole map -96..95, --fo4cs-one-root, stock set to scratch).
- The panel cannot express cover ON under the FO4CS target. The cover row sits in the hidden stock-terrain
  section, and coverOptions() needs the stock .BTR/texture rows, which would write a stock set into the mod.
  So the bake ran as the panel's command-line equivalent.
- The heightmap stage was skipped. It refuses a modded land corpus (cd9657c5 vs the pinned d8337d02), and its
  file name would shadow the FO4CS mod's own map.

Run 2: 06:01:38 to 07:37:25 = 95 min 47 s (lodl 19 s; chunks 5,728 s). Stage times:
- meshes 3,427 s
- VT textures 1,844 s
- card arrays 151 s
- instances 1,743 s, of which the proximity identity join took 1,722 s on one core (98,849 placements,
  12.2 M samples). That is 30 min of one busy core with no new file. It is slow, not stuck.

Peak working set 9.23 GB.

Output: mods\FO4CSLOD is 3,677 files, 15,958,992,355 bytes (15.96 GB):
- .lodt 5 (15.25 GB)
- .lodj 3,060
- .txt 487
- .DDS 96
- .lodm 25
- .lodl, .lodo, .lodi and .lodb, 1 each

## 3 Gates (numbers, red runs)
- G1 PASS.
  - bake rc 0; every stage line printed (stages.txt: lodl rc 0, chunks rc 0).
  - --native-verify rc 0, cardCount 79.
  - census check: 38 checks, 0 failures. Its floor caught both doctored claims (lodo.clusterCount, lodi.instanceCount).
  - lodm/lodl/lodb readers rc 0; --lodt-check VT.32/16/8/4/2 all rc 0.
  - 0 Qt Concurrent lines.
  - Red: run 1 on build 7 died rc 127. The land-lock gate over 36 ring-4 chunks: build 7 rc 127, build 8 rc 0.
    Build 8 on the pool and build 7 with --threads 1 give the same bytes.
- G2 PASS. The .lodb lists 47 plugins: the 30 active lines of plugins.txt + 8 base ESMs + 9 CC ESLs, 0 missing.
  They include BNS Trees, BNS Landscape, Grasslands - Healthy and TrueGrass. Red: the vanilla-only record lists 1 plugin.
- G3 PASS. 79 bases carry a card layer, 42 of them from BNS Trees.esp (load index 2c, e.g. 2c004c50).
  Red: the vanilla-only bake has 0 index-2c bases; the ring-4-only run on his order links 0 of 42.
- G4 PASS. The VT.2 mask sheet is BC3 (cover in alpha). Grass cells picked from the plugins read non-uniform:
  - -20,25: 153 distinct values, sd 16.9
  - -24,24: 135 distinct values, sd 19.8
  - -22,22: 174 distinct values, sd 28.3

  Red: the same region without --cover (g4red/) is BC1 with no cover channel on all 3 cells.
- G5 PASS. Before/after listings (mods top level, MO2 root, profile files, all with mtimes) differ only by the
  new ./FO4CSLOD line and the ./mods folder mtime (04:37:20, when FO4CSLOD was created); profile 13/13 unchanged.
  The bake's own layout census: "3675 file(s), 0 outside". The new ./FO4CSLOD line is the positive control:
  the listing does catch a new top-level write.

Finding (not a gate): FORCE_CARD is 0 of 184,431 instances. The dry run had 20,165.
- On the whole map, ring 4 reaches every tree first. The record keeps that arrival (MNAM slot 0, which has a mesh).
- The far-ring C lines (171,220, all linked) therefore never key a kept arrival.
- slotInstances: full 184069/342/20/0; dry 12756/1/8364/14553.
- The dry run's thousands came from far-ring chunks reaching outside its ring-4 area. It is not a regression.
- It does mean nothing in this .lodi is forced onto a card. The reader must choose cards by ring/distance from
  the base's card layer. This is a question for the card-link owner.

Also noted: "79 textures unreadable" in the object arrays (dry run: 62). Both equal that run's card-base count.
458 "not found in archives" lines are for loose ground/material textures; the VT still reports all 12,276 tiles present.

## 4 Exe sha1 + commits
- Run copy build 8: 27a7bb29c80ac6a37cafa1aa7bdb09128a19b50c (scratchpad/bake1_20260925/run/release/NifSkope.exe).
- Rung (pre-lane): 3eaefafa (release/NifSkope.before_bake1.exe).
- Build 7: 96c99832 (run 1, crashed).
- Commits on bake1-20260925:
  - 24f5f39b
  - ad482f49
  - 23cde121
  - e27fe279
  - 77f6eae5
  - plus this report's text commit (see git log)
- No push, no merge.

## 5 What the final bake needs from this lane
Merge all four fix commits first: 24f5f39b, ad482f49, 23cde121, 77f6eae5. Without 24f5f39b, BNS is absent;
without ad482f49 the bake segfaults on BNS LOD trees; without 77f6eae5 it dies in the instances stage.

Then, in order:
1. The tree card bake (79 candidates, N8, about 15 min).
2. `lodgen --mo2-profile <Default> --worldspace 3C --lodl <MOD>`.
3. The chunks stage, with these flags:
   `--terrain-region -96 -96 95 95 --dim all --out-dir <scratch> --tex-dir <scratch>/textures --native <MOD>
   --vt <MOD> --vt-height --vt-density 16 --cover --impostors <cards> --arrays --fo4cs-one-root`.
   No heightmap stage on a modded order.

Budget about 1 h 40 min plus cards.

Owed rulings:
- (a) The panel needs a cover row under the FO4CS target, or the whole bake stays CLI-only.
- (b) The FORCE_CARD rule on a whole map (above).
- (c) Whether the proximity join should run in parallel or be bounded (30 min serial).
- (d) bungo adds +FO4CSLOD to his modlist himself. The director does not.

## 6 Skill review
- Loaded: the six listed in section 1.
- Wished for: a whole-map bake procedure (flags, stage timings, what a long serial phase is, how to verify)
  -> written as ww-whole-map-lod-bake. Also a "shared reader inside a thread pool" check -> written as
  ww-shared-reader-in-pool.
- Written this lane (E:\Projects\Claude\.claude\skills\):
  - ww-esm-later-plugin-groups
  - ww-meshopt-unused-vertices
  - ww-stripped-frame-by-strings
  - ww-shared-reader-in-pool
  - ww-whole-map-lod-bake
- Not copied to the AISkills repo. Three of them name the owner and the symbol source. That repo's tree is
  dirty, so the copy and the scrub are owed to a pass that owns it.
