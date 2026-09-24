## HANDOFF text

**Lane CARDLINK1 (LOD-C), 2026-09-24, branch cardlink1-20260924. Not merged, NOT FLOWN.**
A `--native --impostors <cards> --arrays` bake now links its card arrays into the pair:
- `.lodo`: `cardLayer` = (set << 11) | layer on every base that has a card, `cardCount`, and `cardCorpusHash`.
- `.lodi`: FORCE_CARD on each placement that stands on its card.

**The hash definition is PROPOSED (R19). bungo has not ruled on it.** It is defined in `docs/LODGEN_NATIVE_LODO_LODI.md` §4.13.

**No new field and no version bump.** `.lodo` stays v4; every field already existed.

**The reader now recounts `cardCount`** and refuses a mismatch by name.

**Change to the CLI order:** the native block runs after the object passes (in `src/nifcli.cpp`, before the scratch teardown).

**The GUI is not hooked up yet.** `src/lodgenmanager.cpp` is not this lane's file. After the merge, run `python scratchpad/cardlink1_20260924/hookup_lodgenmanager.py <repo>`. It is anchored and refuses to run twice. Until then, a panel bake writes the pair without cards, exactly as before.

**Gate:** `tests/spells/lodgen_cardlink.sh`, Sanctuary 9 chunks, 23 real tree card sets. It passes with 0 failures:
- `cardCount` is 23.
- 0 of 23 layers are unresolved.
- The hash is `65d2bf61ff72c5b2`. It moves to `f1b93d3ee8c01b3a` when one texel changes.
- FORCE_CARD is set on 3,446 of 3,526 placements.
- `--native-verify` refuses a `cardCount` changed by one.
- The no-cards bake is byte-identical to the rung, all 34 files.
- Every card check fails on the rung exe.

Exe sha1: `1ff89a0804aebbe52020db9307172cfa276e6e4f`.

## WW_CHANGES text

**2026-09-24, lane CARDLINK1: the native pair carries its cards (NOT FLOWN).**

A FO4CS bake with impostor cards and arrays now writes, in the `.lodo`:
- each carded base's card array layer;
- the number of carded bases;
- a hash over the card arrays (proposed, not ruled).

It also marks, in the `.lodi`, every placement that stands on its card.

**Changed behaviour:**
- A base with a card but no LOD mesh is now kept in the table. Before, it was dropped.
- The pair's own reader recounts the card count and refuses a file where it is wrong.
- A bake without cards is byte for byte what it was.

Gate: `tests/spells/lodgen_cardlink.sh`.

## MISTAKES text

**2026-09-24, CARDLINK1: a gate helper looked for the wrong card-set file.** It globbed `<formid>_oct.lodm` in the card bake driver's output. That file is only written beside a chunk that places the card. The driver writes `<formid>_oct_albedo.png` and its siblings. It was caught before the first run, by listing the output by suffix. Rule: list what a producer actually wrote before a gate names its files.

**2026-09-24, CARDLINK1: the first helper unpacked the decoder's return value wrongly** (`h, L = read_lodo(...)`; the function returns only `L`). It was caught by reading the decoder's return statement before the run. Rule: read the callee's return, not a remembered shape.

## Skill review

**Loaded:**
- nifskope-ww-worktree-build
- nifskope-ww-build-verify
- nifskope-ww-lodgen
- nifskope-ww-render-shot
- search-lean

**Written: `nifskope-ww-worktree-build`, new section 6.** It covers:
- Prove main's objects are current with `make -n` before copying them.
- The `NIFSKOPE_REVISION` define moves in a worktree, so delete the objects that read it.
- Touch the copied objects if their times were preserved.
- The card bake driver uses a fixed `--port 45917` and a per-worktree lock, so it can run only one at a time per machine.
- The card set files are named `_oct_albedo.png`, not `_oct.lodm`.

**Wished for:** a skill for the native pair's card contract (reading `cardLayer` / arrays / FORCE_CARD). §4.13 and `tests/spells/lodgen_cardlink.py` serve for now.
