# IMPOSTORFIX3 -- PENDING (written past half context, Sat Sep 19 16:38 CEDT 2026)

If this lane dies here, everything below is already ON DISK and nothing is
committed. Nothing needs re-doing to recover; read `report.md` sections 0..6.

## Landed in the working tree (NOT committed, by design)

    src/lodgen.cpp                      8-ring height fill + census + self-check
    tests/spells/impostor_bc_decode.py  both BC3 alpha ramps + known answers
    tests/spells/impostor_sheet_check.py  continuity clause + far-plane clause
    tests/spells/impostor_draw.sh       IOU_FLOOR 0.35 -> 0.50, new rows 14a/14b
    MISTAKES.md                         4 entries (CRLF splice, 9965 -> 10043 CR)
    .claude/skills/ww-reference-card-diagnose/SKILL.md   new section 11
    E:/Projects/Claude/.claude/skills/ww-reference-card-diagnose/SKILL.md
                                        was STALE at 9163 B, now the same 16455 B

The exe in the slot is the 8-ring build: `release/NifSkope.exe`,
23,504,896 B, 2026-09-19 15:57:53, sha1 220662f1eb1f344a8f26b0e0470b1b961976263d.
The rung is `release/NifSkope.before_impostorfix3.exe`, 23,504,384 B,
15:52:26, sha1 af4577556f2b80ee71a048c637cbe218643ee8d7.

## NOT applied, and must not be applied without bungo's word

    hookup_ruling_alpha.py   alpha cut 0.0627 -> 0.20   (--check: 2/2 anchors, once)
    hookup_ruling_swap.py    `_n` height <-> sway       (--check: 7/7 anchors, once)

Both refuse unless every anchor matches exactly once. Neither has been run
without `--check`. The height-consistency rejection was NOT applied and should
not be: it costs the rock 0.065.

## Nothing is owed: the lane finished at 16:43

Sections 1..8 of `report.md` are written, the neighbour gates all ran, `DONE`
is on disk and `BUILDING` is gone. This file is kept as the recovery map, not
as a list of work outstanding. The two red steps and why neither is this
lane's are in report.md section 7.

## Traps a resuming lane should not step in again

- Two `.lodm` in one fixture resolve the SAME sheets (`registerLooseSheets`
  walks up to the nearest `textures/`). The A/B needs `fixture_r3/` as its own
  root -- it is already built.
- `rebake_dds.sh` does not forward `EXTRA`, and it deletes the old DDS first.
  The rock needs `--no-trees-only` passed to lodgen directly.
- Background bash resets cwd: absolute paths in every backgrounded command.
- Importing `s2_rockplace` re-runs its module-level measurement.
