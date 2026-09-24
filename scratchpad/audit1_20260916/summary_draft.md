## Summary

1. **Exe.** Audited `a843fca6` (22,567,424 B, 12:45:27), rung as `release/NifSkope.before_audit1.exe`. Shipping now **`48f7f1ab`, 19:30:16**: seven edits, three files, every one a refusal or a message.
2. **The writer did not move.** Region (a) re-baked on the fixed exe: **55 of 55 byte-identical**, 0 differ, 0 missing, only the bake record excluded.
3. **Board.** BOARD-LINE
4. **Every red is attributed.** Three standing reds plus two nobody had claimed were all the GATE, decided by running the exe both ways, each fixed with its own refuter; three more are pre-registered known reds (grass cover, the `.BTO` drop, CARDWIDTH).
5. **Four CONFIRMED defects fixed:** two payload bounds tests that wrapped in `quint64`, the aggregate rules that stopped at version 4 while the default bake writes version 5, a valued switch that silently did nothing spelled last (`--incremental` full-baked at exit 0), and a warning that named an outcome that did not happen. A fifth, `aggregateStride 0` on a v5 file, was found by the fix's own verifier.
6. **Four handed on with reasons** (6.7): the unguarded road-triangle index, the texture failure that never reaches the exit code, the census counting freed bytes before the delete, the viewer's 16-bit bucket overflow.
7. **Bakes.** WALL-LINE
8. **Decode.** 15 invariants, 76 files, 304 checks, 0 violations, 21 refuters of which 19 went red -- the two that did not are the exe's own aggregate rules, which is what F3 and F4 fix.
9. **Design gaps, as rows:** `cardCorpusHash` is zero on every bake including one baked against 23 real cards; `--dim` takes one integer and a second `--native` run replaces rather than merges, so no `.lodi` this CLI writes can carry two non-zero slots; under the shipped defaults the object library is rebuilt on every bake of any kind.
10. **bungo's row** (thick leaves): measured, not a defect -- the cause is the `--library near` DEFAULT, not the card bake. Section L.
