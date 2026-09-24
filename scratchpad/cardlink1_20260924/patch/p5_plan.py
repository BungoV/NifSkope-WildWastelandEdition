"""CARDLINK1 patch 5: docs/FO4CS_IMPROVED_LOD_PLAN.md -- rows 11 and 28 closed with evidence, and the R0 gap note."""
import sys
WT = 'E:/Projects/NifskopeWWE-cardlink1'
path = WT + '/docs/FO4CS_IMPROVED_LOD_PLAN.md'
with open(path, 'rb') as f:
    b = f.read()
cr0 = b.count(b'\r')
EV = (b"gate `tests/spells/lodgen_cardlink.sh` on Sanctuary (9 chunks, dim 4, 23 real tree card sets, "
      b"`--impostors-from-level 0`): cardCount 23 of 2,970 bases, 23 of 23 layers resolve over 10 arrays, "
      b"cardCorpusHash `65d2bf61ff72c5b2` = the contract recomputed outside the exe, moves to `f1b93d3ee8c01b3a` "
      b"when one albedo texel changes, FORCE_CARD on 3,446 of 3,526 placements and none on a card-less base; "
      b"the rung exe writes 0 / 0xFFFF / 0")
reps = [
    (b"| 11 | **no bake writes a card layer.** `cardLayer` is `0xFFFF` on every base and `cardCorpusHash` is 0 |",
     b"| 11 | ~~**no bake writes a card layer.** `cardLayer` is `0xFFFF` on every base and `cardCorpusHash` is 0~~ "
     b"\xe2\x80\x94 **DONE 2026-09-24, lane CARDLINK1** (NOT FLOWN): a `--native --impostors --arrays` bake links its card "
     b"arrays and writes cardLayer, cardCount, cardCorpusHash (**proposed R19**, NATIVE \xc2\xa74.13, not ruled) and "
     b"FORCE_CARD; " + EV + b" |"),
    (b"| 28 | **`cardCount` is bounded but never recounted** by the writer's own reader (added 2026-09-23) |",
     b"| 28 | ~~**`cardCount` is bounded but never recounted** by the writer's own reader (added 2026-09-23)~~ "
     b"\xe2\x80\x94 **DONE 2026-09-24, lane CARDLINK1**: `lodoRead` recounts it and refuses a mismatch by name; "
     b"`--native-verify` on a pair whose cardCount was edited 23 -> 24 (CRC recomputed) refuses with "
     b"`cardCount 24 but 23 base row(s) name a card layer`, the rung exe does not name cardCount |"),
    (b"* **The writer's own reader has one gap, and R0 should close it:** `cardCount` is bounded but never RECOUNTED.\n"
     b"  The loader should recount it from the base table (\xc2\xa75 row 28).\n",
     b"* ~~**The writer's own reader has one gap, and R0 should close it:** `cardCount` is bounded but never RECOUNTED.~~\n"
     b"  **Closed 2026-09-24 (lane CARDLINK1):** the writer's reader now recounts it (\xc2\xa75 row 28). R0 should recount it too.\n"),
]
for a, r in reps:
    c = b.count(a)
    if c != 1:
        sys.exit('anchor count %d for %r' % (c, a[:70]))
    b = b.replace(a, r)
if b.count(b'\r') != cr0:
    sys.exit('CR count moved')
with open(path, 'wb') as f:
    f.write(b)
print('plan patched: %d edits' % len(reps))
