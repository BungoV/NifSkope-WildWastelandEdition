"""DOCFIX1: LODGEN_CENSUS.md items 1-3.
1 version row: src/lodofile.h:78 LODO_VERSION = 4 (v1/v2/v3 refused by name, lodofile.cpp:1749-1768);
  src/lodifile.cpp:971-990 accepts 3..9, refuses 1 and 2; default .lodi v7 (lodifile.h:243);
  .lodl src/lodtfile.cpp:57-58 1..3, writer 2 (lodtfile.h:159); .lodt src/io/lodvfile.h:91 LODTEX_VERSION = 2.
2 `unmeasured` added to §1.2 rule 5.
3 §7: the gated pairs are v3/v3 (measured: LODO\\x03, LODI\\x03) and today's decoders refuse a v3 .lodo."""
import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/docs/LODGEN_CENSUS.md'
reps = [
(b"""`MANIFEST` (`docs/LODGEN_MANIFEST_FORMAT.md`), `LODM`
(`docs/LODGEN_LODM_FORMAT.md`).""", b"""`MANIFEST` (`docs/LODGEN_MANIFEST_FORMAT.md`), `LODM`
(`docs/LODGEN_LODM_FORMAT.md`), `BTD` (`docs/LODGEN_BTD_FORMAT.md`, whose sections are
named, not numbered: `BTD Header`).""", 1),
(b"| `version` | pair | the two container versions actually loaded, `lodo3/lodi3` | NATIVE 3, NATIVE 4 | put a v2 pair in place: the row must print the refusal, not the version | `refused:v1`, `refused:v2` | `unread` |",
 b"| `version` | quad | the four far-field container versions actually loaded, `lodo<v>/lodi<v>/lodl<v>/lodt<v>`; a default bake today reads `lodo4/lodi7/lodl2/lodt2`. `.lodo` is 4 only; `.lodi` is 3..9 (7 by default, 9 with `--scrappable`, 8 retired but still read, 3..6 older bakes); `.lodl` is 1..3 (2 by default, 3 with `--water-bodies`); `.lodt` is 2 only. A file the arm does not use prints `-` in its slot | NATIVE 3, NATIVE 4, BTD Header, VT 3.1 | put a v3 `.lodo` in place: the row must print the refusal, not the version | `refused:lodo:v1`, `refused:lodo:v2`, `refused:lodo:v3`, `refused:lodi:v1`, `refused:lodi:v2`, `refused:lodl:v<n>` (outside 1..3), `refused:lodt:v<n>` (not 2), and `refused:<file>:v<n>` for any other unknown version | `unread` |", 1),
(b"""5. **A default accuses its own plumbing.** `uncounted`, `unread`, `unset`,
   `unchecked` and `unwired` are the five defaults used here, and every one of
   them reads as a fault.""",
 b"""5. **A default accuses its own plumbing.** `uncounted`, `unread`, `unset`,
   `unchecked`, `unwired` and `unmeasured` are the six defaults used here, and
   every one of them reads as a fault. `unmeasured` is the default of a runtime
   TIMING (the \xc2\xa75.3 `shadowMarchMs` / `shadowMapMs`): FO4CS's timer never wrote
   the field, and no bake number exists to stand in for it.""", 1),
(b"""Measured on the nine-chunk Sanctuary pair (NATIVE1b, 2026-09-11 10:17:44):
**59 checks, 0 failures, 31 census words the files cannot carry.** On LODUI1's
four-cell region pair (2026-09-11 13:25:22): 59 checks, 0 failures, the same 31.""",
 b"""Measured on the nine-chunk Sanctuary pair (NATIVE1b, 2026-09-11 10:17:44):
**59 checks, 0 failures, 31 census words the files cannot carry.** On LODUI1's
four-cell region pair (2026-09-11 13:25:22): 59 checks, 0 failures, the same 31.

**That figure is HISTORICAL and cannot be repeated as it stands.** Every pair it
was measured on (the table under Provenance) is a **version 3 `.lodo` beside a
version 3 `.lodi`** (their first eight bytes read `LODO 03 00 00 00` and
`LODI 03 00 00 00`), and both today's decoders refuse a version 3 `.lodo` by name
(`src/lodofile.cpp` `if ( h.version == 3 )`, `tests/spells/lodgen_native_decode.py`
`if h['version'] == 3:`), because v4 reinterprets the base row. A current figure
needs a re-bake at `.lodo` 4 / `.lodi` 7 and a fresh run; until then 59/0/31 is a
2026-09-11 measurement, not a gate.""", 1),
]
b = open(P, 'rb').read(); cr0 = b.count(b'\r\n'); bad = False
for old, new, n in reps:
    c = b.count(old)
    if c != n: print('REFUSE', old[:70], c); bad = True; continue
    b = b.replace(old, new)
if bad or b.count(b'\r\n') != cr0: sys.exit(1)
if '--check' in sys.argv: print('check OK'); sys.exit(0)
open(P, 'wb').write(b); print('written', len(b), 'CRLF', b.count(b'\r\n'))
