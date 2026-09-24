"""CARDLINK1 patch 4: docs/LODGEN_NATIVE_LODO_LODI.md -- the card link (4.13 new; 3, 4.1, 4.4, 5, deviation 5)."""
import sys
WT = 'E:/Projects/NifskopeWWE-cardlink1'
P = WT + '/scratchpad/cardlink1_20260924/patch/doc/'
path = WT + '/docs/LODGEN_NATIVE_LODO_LODI.md'
with open(path, 'rb') as f:
    b = f.read()
cr0 = b.count(b'\r')
with open(P + 's413.txt', 'rb') as f:
    s413 = f.read().replace(b'\r\n', b'\n')

reps = [
    (b"| 0x28 | u64 | `cardCorpusHash` \xe2\x80\x94 over the card candidate list, N, tile class and every card source model |",
     b"| 0x28 | u64 | `cardCorpusHash` \xe2\x80\x94 **PROPOSED (R19, not ruled), \xc2\xa74.13:** FNV-1a 64 over the linked card arrays' files (name, size, bytes), sets in lower-cased-name order; **0 = no card linked** |"),
    (b"A reader sizes its card-draw pass from the header alone; `cardCount > baseCount` is refused by name**",
     b"A reader sizes its card-draw pass from the header alone; `cardCount > baseCount` is refused by name, and (CARDLINK1) the reader RECOUNTS it over the base rows and refuses a mismatch by name, \xc2\xa74.13**"),
    (b"layer, high 5 bits the card array set**; 0xFFFF = no card), `u16 flags`,",
     b"layer, high 5 bits the card array set**, the set's rank in \xc2\xa74.13's order; 0xFFFF = no card), `u16 flags`,"),
    (b"| 0x14 | 2 | `flags` | u16: bit0 mirrored, bit1 force-card, bit2",
     b"| 0x14 | 2 | `flags` | u16: bit0 mirrored, bit1 force-card (\xc2\xa74.13: the base has a card and the ring slot has no mesh, or a `C` line put it on its card), bit2"),
    (b"mesh in any slot has `rep[0..3]` all 0xFFFF and **must** have a `cardLayer`.\n",
     b"mesh in any slot has `rep[0..3]` all 0xFFFF and **must** have a `cardLayer`.\n"
     b"From lane CARDLINK1 such a base is written when a card array links it, and\n"
     b"left out of the table otherwise (\xc2\xa74.13, deviation 5).\n"),
    (b"`cardCount` > `baseCount`; a base's `fullTriangles`",
     b"`cardCount` > `baseCount`; `cardCount` not equal to the base rows naming a card layer, or rows naming one while `cardCorpusHash` is 0 (CARDLINK1, \xc2\xa74.13); a base's `fullTriangles`"),
    (b"5. **The bake writes what the stock ring bakes and nothing more:** no card layer\n"
     b"   (`cardLayer` = 0xFFFF everywhere, `cardCorpusHash` = 0), no `crossPx16` (0),",
     b"5. **The bake writes what the stock ring bakes and nothing more:** no card layer\n"
     b"   (`cardLayer` = 0xFFFF everywhere, `cardCorpusHash` = 0) **unless a\n"
     b"   `--impostors --arrays` bake links its card arrays (lane CARDLINK1,\n"
     b"   2026-09-24, \xc2\xa74.13): then cardLayer, cardCount, cardCorpusHash and\n"
     b"   FORCE_CARD are written, and a base with a card but no mesh is kept**; no `crossPx16` (0),"),
    (b"\n## 5. Refusal policy \xe2\x80\x94 hard for the generator, soft for the consumer\n",
     b"\n" + s413 + b"## 5. Refusal policy \xe2\x80\x94 hard for the generator, soft for the consumer\n"),
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
print('docs patched: %d edits' % len(reps))
