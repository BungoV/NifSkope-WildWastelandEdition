"""BAKE2: the .lodi v10 wide-scale section in docs/LODGEN_NATIVE_LODO_LODI.md. Anchors == 1, CR count unchanged."""
import sys, os
P = 'E:/Projects/NifskopeWWE-bake2/docs/LODGEN_NATIVE_LODO_LODI.md'
E = []
def ed(old, new): E.append((old, new))

ed('# `.lodo` v5 (v4..v5) + `.lodi` v7 (v3..v9)', '# `.lodo` v5 (v4..v5) + `.lodi` v7 (v3..v10)')
ed('''> `--scrappable` writes **version 9** (§4.12); the ways back''',
   '''> `--scrappable` writes **version 9** (§4.12); a placement scaled above 7.99988
> writes **version 10** (§4.14, lane BAKE2 2026-09-25); the ways back''')
ed('''retired and read only (§4.11). The reader accepts `.lodi` 3..9 and refuses 1''',
   '''retired and read only (§4.11). The reader accepts `.lodi` 3..10 and refuses 1''')
ed('''or 9 when it carries the workshop-scrappable bit (§4.12)**;''',
   '''or 9 when it carries the workshop-scrappable bit (§4.12), or 10 when any instance carries the wide-scale bit (§4.14)**;''')
ed('''| 0x0C | 2 | `scale` | u16, `scale = v / 8192`, range 0 … 7.99988. **A stored 0 is a refusal** (§4.1b) |''',
   '''| 0x0C | 2 | `scale` | u16, `scale = v / 8192`, range 0 … 7.99988; **with flags bit 7 (version 10), `scale = 8 + v / 8192`, range 8 … 15.99988** (§4.14). **A stored 0 without bit 7 is a refusal** (§4.1b) |''')
ed('''**bits 7–15 reserved, and a set reserved bit is a refusal** (`LODI_INST_FLAGS_KNOWN` = 0x7F) |''',
   '''**bit7 wide scale (version 10 only, §4.14: set by the writer alone, refused by name below version 10)**; **bits 8–15 reserved, and a set reserved bit is a refusal** (`LODI_INST_FLAGS_KNOWN` = 0xFF) |''')
ed('''`scale` maxes at **7.99988** and `baseId` at **65,535**.''',
   '''`scale` maxes at **15.99988** (7.99988 before version 10, §4.14) and `baseId` at **65,535**.''')
ed('''| **hard: `.lodi`** (`lodiRead`) | versions **1 and 2 refused by name**, anything outside 3…9;''',
   '''| **hard: `.lodi`** (`lodiRead`) | versions **1 and 2 refused by name**, anything outside 3…10;''')
ed('''instance flag bit 6 below v9 (§4.1);''', '''instance flag bit 6 below v9 (§4.1); instance flag bit 7 below v10 (§4.14);''')
ed('''**Census**, on its own `native-scrappable:` prefix, ending with the word the
gate greps for: `scrappablePlacements`.
''', '''**Census**, on its own `native-scrappable:` prefix, ending with the word the
gate greps for: `scrappablePlacements`.

### 4.14 The wide-scale bit (`.lodi` v10, lane BAKE2, 2026-09-25; the director's ruling (a))

**Why.** The instance record stores its scale as `u16 / 8192`, so nothing above
65535/8192 = **7.99988** fits, and the writer refused the whole file on such a ref
(§4.2). The engine and the CK allow a reference scale up to **10.0**. Nuka-World
places four LOD-carrying cliffs above the old line (0604D45A 9.97, 0604D45D 8.33,
0604DDA1 9.23, 0604DDB9 8.33 in his load order), so its `.lodi` could not be
written at all. Dropping them was ruled out: the distant view shows the game's
own data.

**The rule.** Instance flag **bit 7, `LODI_INST_SCALE_WIDE` (0x80)**:

| bit 7 | `scale` means | range | step |
|---|---|---|---|
| clear | `v / 8192` (every version) | 0 … 7.99988 | 1/8192 |
| set (v10 only) | `8 + v / 8192` | 8 … 15.99988 | 1/8192 |

`lodiScaleWord()` / `lodiScaleValue()` / `lodiScaleQuantised()` in
`src/lodifile.h` are the one encoder and the one decoder. At or below 7.99988 the
word is `lround(s × 8192)` clamped to u16, the exact arithmetic of every earlier
version, and the bit is clear: **no instance anywhere is coarser than before**.
Above **15.99988** the writer still REFUSES (never clamps), naming the ref; 16 is
60 percent headroom over the engine's 10. A caller that sets bit 7 itself is
refused: the writer alone decides it from the scale.

**The version moves only when it must.** Like v9 (§4.12), the version word is the
only thing that tells a reader which flag bits may appear. It rises to **10 only
when some instance carries bit 7**. A file whose scales all fit is the v7 (or v9)
file this writer always wrote, **byte for byte**, version word included, so every
`.lodi` already installed stays valid and readers keep accepting 7 and 9.
Version 10 is the **v9 layout** (bit 6 keeps its meaning; 512-byte header, no new
table, no header word). A `--lodi-v6` bake that meets a wide scale is REFUSED:
no pre-v7 version can say the bit, and dropping it would draw the object at an
eighth of the scale it should have or worse.

**Readers.** `lodiRead`: bit 7 below version 10 is refused by name; a stored 0 is
refused only without bit 7 (a wide 0 is 8.0). `lodinative.cpp` decodes the scale
through `lodiScaleValue`. The independent decoder
(`tests/spells/lodgen_native_decode.py`) and the fields spell (`j0`, `j0b`: bit 7
appears exactly when the version is 10) read both. **The FO4CS reader owes the
same decode** (version 10 accepted, bit 7 = +8).

**Census**, on its own `native-wide-scale:` prefix: placements above the line,
of the total, the max scale and the `.lodi` version written.
''')

b = open(P, 'rb').read(); cr = b.count(b'\r'); s = b.decode('utf-8'); bad = 0
for o, n in E:
    c = s.count(o)
    if c != 1: print('ANCHOR x%d: %r' % (c, o[:80])); bad += 1; continue
    s = s.replace(o, n)
if bad: sys.exit('refused')
nb = s.encode('utf-8'); assert nb.count(b'\r') == cr
if os.environ.get('DRY'): sys.exit('DRY ok')
open(P, 'wb').write(nb); print('patched docs')
