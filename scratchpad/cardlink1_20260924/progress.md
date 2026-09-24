# CARDLINK1 progress (lane LOD-C), worktree E:\Projects\NifskopeWWE-cardlink1, branch cardlink1-20260924 from 47b2cad

## 2026-09-24 21:16 -- worktree built (rung)
- Skills loaded: nifskope-ww-lodgen, nifskope-ww-build-verify, search-lean.
- qmake needed main's `.qmake.stash` (the compiler probe fails without it: "failed to parse default include paths").
- Copied from main (read only there): `.qmake.stash`, `GeneratedFiles/` (main HEAD 71f96c1 differs from 47b2cad only in
  scratchpad/docs; main `make -n` = 0 compiles, so its objects match 47b2cad), release runtime (*.dll, *.xml, qt.conf,
  style.qss, platforms/, imageformats/, styles/, shaders/). Objects touched newer than the checkout; lodbfile.o, main.o,
  about_dialog.o deleted (NIFSKOPE_REVISION define differs: a worktree .git is a file, qmake finds no revision).
- Build script: scratchpad/cardlink1_20260924/wt_build.sh (gated on make's rc, exe mtime moved, MZ, -nt).
- RUNG exe = release/before_cardlink1.exe, sha1 0af99c9921a331f6db19ea45a43488fb3736c5d4 (45 objects rebuilt).

## 2026-09-24 21:31 -- code landed, compiled (commit 99816ee)
- nativeemit: `lodgenNativeLinkCards(btoPaths, cardArrayBase, error)`; Write assigns cardLayer, card-only bases,
  cardCorpusHash (proposed R19), FORCE_CARD, census clause `native-cards:`; library reuse refuses "the card arrays moved".
- lodofile reader: cardCount recount (refused by name) + "card with hash 0" refusal; "version 3" strings -> 4.
- No new field: `.lodo` stays v4 (every field existed; only cardCorpusHash's definition changed, no exe ever wrote non-0).
- nifcli: the native block moved after the card arrays (before the scratch teardown); link call inside it.
- GUI: `hookup_lodgenmanager.py` (refusing, anchored; dry-run on a temp copy: applies once, refuses a second run).
- Build: 9 objects, BUILD-RC=0, exe sha1 1ff89a0804aebbe52020db9307172cfa276e6e4f (kept as release/cardlink1_step1.exe).
- Card bake started: CANDIDATES=trees, cells -20 24 -9 35, 23 candidates, port 45917 (driver's own), own exe.

## 21:39 card bake done, gate running, docs patched
- Card bake: 23 of 23 tree sets (`<formid>_oct_albedo.png` + normal/gsaos/g), rc 0, lock released.
- Gate `tests/spells/lodgen_cardlink.sh` + helper `lodgen_cardlink.py` written; run 1 in progress.
  G1/G2/G3a on the new exe: cardCount 23 of 2970 bases, 23 == 23 tree bases with a set, 0 of 23 layers
  unresolved over 10 arrays, cardCorpusHash 65d2bf61ff72c5b2 == contract recomputed, FORCE_CARD 3446 of 3526, 0 on a card-less base.
- docs/LODGEN_NATIVE_LODO_LODI.md: p4_docs.py, 8 edits (new 4.13; 3 hash row, cardCount row, cardLayer; 4.1 flags; 4.4; 5 table; deviation 5).
- Skill nifskope-ww-worktree-build: section 6 added (make -n proof, REVISION define, touch, card driver port 45917, set file names).

## 21:5x gate run 1: RESULT PASS (0 failures), exe 1ff89a08, rung 0af99c99
- G1 cardCount 23 == 23 tree bases with a set (of 2970 bases); rows naming a layer are exactly those.
- G2 0 of 23 layers unresolved over 10 arrays.
- G3 hash 65d2bf61ff72c5b2 == contract; one albedo texel of 00038599 flipped -> f1b93d3ee8c01b3a == contract.
- FORCE_CARD 3446 of 3526 instances, 0 on a card-less base.
- G4 --native-verify on cardCount 23->24 (CRC recomputed): rc 1 "cardCount 24 but 23 base row(s) name a card layer"; unedited pair rc 0.
- ID: bake without --impostors, 34 files byte-identical new vs rung (version word did not move either).
- RED on the rung: cardCount 0, cardCorpusHash 0, forced 0 -> G1/G2/G3/FORCE_CARD all FAIL there; bumped rung .lodo refused only by pairing (lodoIdentity), never by cardCount.
- docs/FO4CS_IMPROVED_LOD_PLAN.md rows 11 + 28 closed with these numbers (p5_plan.py).
- Kept-green spells running (btfxkz7if).

## 22:29 kept-green spells, DONE written
- lodgen_native 29/0, lodgen_card_arrays PASS, lodgen_scrappable 9/0, lodgen_identjoin 10/0, lodi_v7 10 ok/0 fail/1 skip.
- native_open 17 checks 3 failures and lodgen_btofree 27 checks 5 failures: IDENTICAL verdict lines on the rung exe -> pre-existing.
- Fixtures for those spells copied into this worktree's gitignored scratchpad (read from main, main untouched).
