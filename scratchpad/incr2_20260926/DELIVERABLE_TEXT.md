## HANDOFF text

### Lane INCR2 (2026-09-26): whole-map quick rebakes; all four worldspaces re-baked and installed

- **Branch.** incr2-20260926 (worktree E:\Projects\NifskopeWWE-fix1, from main @335dfa50). Not merged, not pushed.
- **Commits.**
  - 889dbb4c: `--incremental` works with `--dim all` on the FO4CS target. It keeps a raw chunk cache in the out-dir,
    writes `product` rows in the .lodb, has a null run, and skips the VT when nothing moved.
  - 952f566f: the gate spell tests/spells/lodgen_incr_whole.sh, and docs/LODGEN_BAKE_RECORD.md section 6a.
  - 050388bc: an absolute out row is read as it stands. The spell gains `SCRBASE`, and its null leg now asserts the
    census.
- **Gates (Commonwealth -32 -32 -1 -1, 85 jobs, exe 152ae83f).**

  | Run | Wall time | Result |
  |---|---|---|
  | Full | 979 s | reference |
  | Null | 128 s | identical |
  | Forced whole replay | 488 s | identical |
  | Stale raw cache | 413 s | 1 chunk rebaked, identical |
  | One LOD mesh swapped | 628 s | identical to a full bake of the swapped input |

  The refuter `WW_LODGEN_INCR_NO_RESTORE=1` came out different, as it must.
- **Far Harbor whole map (300 jobs).** The full bake takes 1022 s. An incremental after the .lodl changed took 594 s
  and is identical to a full bake. A null run takes 24 s.
- **Installed** into mods\FO4CSLOD\FO4CSLOD with the game down, from exe 152ae83f.
  - Only files whose sha1 changed were copied, 21 in all. Each replaced file was moved first to
    scratchpad/incr2_20260926/replaced/.
  - Read-back census: READBACK OK on all four.

  | Worldspace | Files | Products intact | .lodo / .lodi |
  |---|---|---|---|
  | Commonwealth | 3677 | 130/130 | v6 / v7 |
  | SanctuaryHillsWorld | 163 | 17/17 | v6 / v7 |
  | DLC03FarHarbor | 578 | 110/110 | v6 / v7 |
  | NukaWorld | 654 | 85/85 | v6 / v10 |

- **Far Harbor land fill had read nothing before.** The vanilla dim-4 .BTR heights of the two DLCs are now in the
  input cache: 1552 files, 12.5 MB, listed in btr_cache_manifest.tsv. Far Harbor now fills 17465 cells.
- **Owed.**
  - Exe a3720b7c (commit 050388bc) is built but has not run: Avast custody terminated it.
  - With that exe, a null incremental on the staged Commonwealth must print "nothing moved".
  - The exe that ran rebakes every chunk whenever the out-dir is on another drive than the mod tree.
  - The panel's quick-rebake row still covers one ring only.
- **Detail:** scratchpad/incr2_20260926/DONE.md. Stage: the session scratch incr2/stage. Commonwealth out-dir:
  scratchpad/incr2_20260926/scr/cw.

## WW_CHANGES text

### INCR2: quick rebakes of a whole map

2026-09-26 INCR2: `lodgen --incremental <mod> --native <mod> --dim all` works on the FO4CS target, with `--arrays`
and `--impostors`.
- **Raw chunk cache.** Each chunk's raw .BTO and manifest are kept in `<out-dir>/lodgen_chunk_cache`.
  - A clean chunk is restored from the cache, so the texture arrays, the merge, the far-ring cut and the card arrays
    all run over the whole list, as a full bake runs them.
  - A missing or stale cache entry rebakes that chunk only.
- **Region products.** The .lodb lists `product <relpath> <sha1>` for every file no chunk claims.
  - A lost product replays every chunk from the caches.
  - When nothing moved, no stage runs.
  - The VT pyramid is skipped when no input moved.
- **Card sets.** Their bytes are folded into the switches, so a re-rendered card set refuses an incremental run.
- **Gate:** tests/spells/lodgen_incr_whole.sh. A null run, a forced replay, a stale cache entry and a swapped mesh
  each come out byte-identical to a full bake. The refuter comes out different.
- **Timings.**

  | Bake | Full | Incremental after one product changed | Null |
  |---|---|---|---|
  | Far Harbor | 1022 s | 594 s | 24 s |
  | Commonwealth region | 979 s | 628 s (one mesh changed) | 128 s |

- **Cross-drive out-dir.** An out-dir on another drive than the mod tree made every chunk "output lost". Since
  050388bc an absolute out row is read as it stands (built, not yet run).
- **Four worldspaces re-baked and installed** into FO4CSLOD: Commonwealth, pre-war Sanctuary, Far Harbor and
  Nuka-World. Far Harbor's land fill now reads vanilla's heights.

## MISTAKES text

### 2026-09-26 -- lane INCR2

- **A gate that could not fail on its input.** The first touch leg flipped one bit in a small LOD nif, and no output
  moved: the full bakes before and after were equal. So "incremental == full" proved nothing there.
  - Rule: before trusting a touch gate, show that the two full bakes differ.
  - The mesh swap (4 files differ) replaced it.
- **The null leg compared bytes only.** A run that rebakes everything is byte-identical too. The whole Commonwealth
  said "3060 of 3060 chunks dirty (3060 output lost)" twice, 55 min each, and the byte gate would have passed.
  - Cause: the out-dir was on E: and the mod tree on C:, so the record stores absolute out rows. The diff joined
    them to the record folder.
  - None of the gates put the two on different drives.
  - Now the null leg asserts "nothing moved", and `SCRBASE` can put the out-dir on another drive.
- **A background bake cannot be stopped from here.** Stopping the shell task leaves the NifSkope child running, and
  killing it was refused. Before launching a long run, make sure it is the one you want.
- **Avast custody.** A freshly linked exe was refused with "Permission denied", and so was its copy. Three launch
  attempts meant three pop-ups on bungo's screen. Rule: launch once, read AvastSvc.log, then stop.
- **An input cache that looked complete.** Far Harbor's land fill "filled 0, missing 1150" because the cache held the
  DLC colour but not the DLC dim-4 .BTR heights. Read the `missing` count in the census, not just the rc.
