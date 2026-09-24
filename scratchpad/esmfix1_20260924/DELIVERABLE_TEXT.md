## HANDOFF text
ESMFIX1 (2026-09-24, branch esmfix1-20260924 from 541bbe5; commit ab12bf3 + report; exe sha1 ec5959c4) -- DONE, not merged.

The ESM reader now loads TestWorldspace.esp ("AnotherOne's Test World"), which blocked his live profile.
- Cause: all 483 records carry file index FF, and the plugin has 1 master. The game and xEdit read an index at or beyond the master count as the plugin itself.
- Fix: lib/libfo76utils/src/esmfile.cpp now maps it that way for form versions below 0xC0 (up to Fallout 4). FO76 and Starfield are unchanged.

Gates:
- G1: his Default profile. All 47 plugins load and a Sanctuary bake runs (rc 0). Red: the rung refuses with "invalid form ID".
- G2: WRLD FF000F99 "TestDebugWorld" reads back as 1C000F99 (plugin 28), as xEdit's rule predicts. Red: the record is absent on the rung.
- G3: a 9-chunk Sanctuary bake on the 17 vanilla masters is byte-identical to the rung: 56 of 57 files match, and the .lodb differs only in its path, exe and time lines. lodgen_loadorder 24/0, lodgen_resources 4/0.

The final bake no longer needs TestWorldspace.esp unticked. lodgen_loadorder.sh G5 still unticks it in its profile copy. That text is stale, but the gate passes.

## WW_CHANGES text
- LOD generation now loads plugins whose form IDs point past their own master list, for example TestWorldspace.esp. Those form IDs resolve to the plugin itself, as they do in game. Before, the whole plugin was refused with "invalid form ID".

## MISTAKES text
(none)

## Skill review
- Loaded: nifskope-ww-worktree-build.
- Amended:
  - nifskope-ww-worktree-build: copy objects from a sibling worktree at the same commit.
  - nifskope-ww-lodgen: `--mo2-profile --list-worldspaces` proves every plugin loads; `--print-source` opens none.
  - mo2-mod-content-census: the TestWorldspace trap is fixed, plus the rule and the probe scripts.
- Declined: a skill for the form-ID rule. It is one line, now kept in mo2-mod-content-census and the source comment.
