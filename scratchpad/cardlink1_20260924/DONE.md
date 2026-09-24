DONE -- lane CARDLINK1 (LOD-C), 2026-09-24 22:29, branch cardlink1-20260924. Not merged. NOT FLOWN.

The exe is `release/NifSkope.exe` in the worktree (also saved as `release/cardlink1_step1.exe`).
Its sha1 is `1ff89a0804aebbe52020db9307172cfa276e6e4f`.

# 1. Skills loaded
- nifskope-ww-worktree-build
- nifskope-ww-build-verify
- nifskope-ww-lodgen
- nifskope-ww-render-shot
- search-lean

# 2. What was built
- **`src/nativeemit.cpp`**: new `lodgenNativeLinkCards()`. It reads the chunk manifests' 12-token `C` lines and the card-array `.lodm` files they name. From them the emitter writes:
  - `cardLayer` (set<<11 | layer);
  - `cardCount`;
  - `cardCorpusHash` (PROPOSED R19, NATIVE §4.13);
  - FORCE_CARD, when the base has a card AND either its ring slot has no mesh or a `C` line put it on its card.

  Other changes in this file:
  - Card-only bases are kept, with a radius taken from the card.
  - Library reuse now compares the card hash.
  - New census line `native-cards:`.
- **`src/lodofile.cpp`**:
  - The reader recounts `cardCount` and refuses a mismatch.
  - It refuses rows that name a card layer when the hash is 0.
  - The stale "version 3" wording is now "version 4".
- **`src/nifcli.cpp`**: the native block moved after the card arrays, before the scratch teardown. The link call is inside it.
- **No new field.** `.lodo` stays v4 and `.lodi` stays v7/v9.
- **GUI**: `hookup_lodgenmanager.py`. `src/lodgenmanager.cpp` is not this lane's file.
- **Docs**:
  - `LODGEN_NATIVE_LODO_LODI.md`: new §4.13; also §3 rows, §4.1 flags, §4.4, the §5 table and deviation 5.
  - `FO4CS_IMPROVED_LOD_PLAN.md`: rows 11 and 28 closed, and the R0 note.

# 3. Gates
**`tests/spells/lodgen_cardlink.sh`: RESULT PASS, 0 failures** (Sanctuary 9 chunks, 23 real tree sets, `--impostors-from-level 0`).

| Gate | New exe | Rung exe (0af99c99) |
|---|---|---|
| G1 | cardCount 23 = 23 tree bases with a set (of 2,970 bases); the carded rows are exactly those bases | cardCount 0 -> FAIL |
| G2 | 0 of 23 layers unresolved, over 10 arrays | FAIL |
| G3 | hash `65d2bf61ff72c5b2` = the contract recomputed in Python; one albedo texel flipped -> `f1b93d3ee8c01b3a` = its contract | hash 0 -> FAIL |
| FORCE_CARD | set on 3,446 of 3,526 placements, 0 on a base without a card | 0 -> FAIL |
| G4 | cardCount 23->24 (CRC fixed) refused: rc 1, "cardCount 24 but 23 base row(s) name a card layer"; the unedited pair gives rc 0 | refused only for pairing, never names cardCount |

**ID:** a bake without `--impostors` gives 34 files, all byte-identical to the rung's. The version word did not move either.

**Kept-green spells:**

| Spell | Result |
|---|---|
| lodgen_native | 29/0 |
| lodgen_card_arrays | PASS |
| lodgen_scrappable | 9/0 |
| lodgen_identjoin | 10/0 |
| lodi_v7 | 10 ok, 0 fail; G1 skipped (no g1_rung fixture) |
| native_open | 17 checks, 3 failures, 2 skipped |
| lodgen_btofree | 27 checks, 5 failures |

The native_open and btofree failures are **pre-existing, not this lane's**. The rung exe gives the identical verdict lines, diffed line for line, on both spells.
- native_open: the `.lodi` cover is 0.72, and the NCC checks fail on showcase1 fixtures.
- btofree: its bytes are pinned to `before_btofree1`, and other lanes have moved the bytes since.

The fixture copies for those spells sit in this worktree's gitignored scratchpad: horizonout, viewfix, horizon1/v8, lodiv7 v7/v6, about 1.3 GB. They are safe to delete.

# 4. Exe and commits
- Exe sha1: `1ff89a0804aebbe52020db9307172cfa276e6e4f`. The rung is `release/before_cardlink1.exe` (0af99c99...).
- Commits: f3b90a9, 99816ee, 8be67fa, 66b7124, plus the report commit.

# 5. What the final bake needs from this lane
- Merge the branch, then run `python scratchpad/cardlink1_20260924/hookup_lodgenmanager.py <repo>` so that panel bakes link too. Rebuild.
- Bake with `--native --impostors <card dir> --arrays`. At dim 4, cards only substitute with `--impostors-from-level N`, or where the ring slot is empty.
- bungo's ruling on R19, the hash definition in §4.13, is still owed. FO4CS must not compare it until he rules.
- `--incremental --native` is refused today. If it is ever allowed, replayed chunks must bring their manifest `C` lines with them.

# 6. Skill review
- **Loaded:** the five skills in section 1.
- **Written:** `nifskope-ww-worktree-build` gained section 6:
  - use `make -n` as the proof that main's objects are current;
  - the `NIFSKOPE_REVISION` define moves in a worktree;
  - touch the copied objects;
  - the card bake driver uses fixed port 45917 and a per-worktree lock;
  - the card set files are named `_oct_albedo.png`.
- **Wished for:** a skill for reading the native pair's card contract. For now §4.13 and `tests/spells/lodgen_cardlink.py` cover it.
- **Also wished for:** kept-green spells that take their fixture folder by an environment variable. scrappable, identjoin and lodi_v7 hard-code `$ROOT/scratchpad/...`, so a worktree has to copy about 1.3 GB.
