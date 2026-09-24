
## 6. The confirmed bugs fixed, and the exe they are in

Step 6's rule was the narrow one: a one-to-few-line fix for a CONFIRMED defect,
each with a gate that goes red before and green after, and never a design
change, a default change or a format bump. **Seven edits went in, in three
files, all of them refusals or messages; not one of them can move a baked
byte, and section 6.4 measures that claim rather than asserting it.**

### 6.1 The rung, the marker, the two builds

| | bytes | mtime | sha1 |
|---|---|---|---|
| the audited exe, rung as `release/NifSkope.before_audit1.exe` | 22,567,424 | copy taken 2026-09-17 19:27 | `a843fca68c18c2740efddcb20e9fe715b7732a22` |
| after F1--F6 (intermediate, superseded) | 22,567,424 | 2026-09-17 ~19:10 | `a03bf5cbb72bdf6ba4ea38e9ecf445dec0cbaaf7` |
| **the exe this report ends on, `release/NifSkope.exe`** | **22,567,424** | **2026-09-17 19:30:16** | **`48f7f1ab0e563bfe24aeb2003dc8b72cc41a3fd1`** |

The rung copy carries the audited exe's own bytes -- its sha1 is the one the
addendum names, so the rung is the exe this lane was handed and not a copy of
something I built. Every other `release/NifSkope.before_*.exe`,
`NifSkope.archlock1_rung.exe` and `NifSkope_inuse_2000.exe` is untouched (16
files, all still present and hashed in my notes). `tasklist` was clear of
`Fallout4.exe` before each build; the `BUILDING` marker was taken before the
first build and cleared after the second. Both builds reported `make rc=0` with
zero `error:` lines.

Sources after the seven edits: `src/lodifile.cpp` 93,389 B, `src/lodofile.cpp`
98,267 B, `src/nifcli.cpp` 405,280 B, all three still CR 0.

### 6.2 The seven edits

| id | defect (section 4) | site | the edit | red before | green after |
|---|---|---|---|---|---|
| **F1** | C1 | `src/lodifile.cpp:973` | `if ( t.off + t.bytes > h.fileBytes )` becomes `if ( t.bytes > h.fileBytes \|\| t.off > h.fileBytes - t.bytes )` | `lodgen_native_doctor.py lodi-wrap` on a real pair: `--native-verify` **accepted it, rc 0** | the same file is **REFUSED by name** |
| **F2** | C1 | `src/lodofile.cpp:1807` | the same one-line rewrite in the `.lodo` table reader | `lodo-wrap`: **accepted, rc 0** | **REFUSED** |
| **F3** | C2 | `src/lodifile.cpp:896`, `:949` | `if ( h.version == LODI_VERSION_AGGREGATE )` becomes `\|\| ( v5 && h.aggregateCount )` -- the three aggregate HEADER refusals reach version 5 | `agg-views` (aggregateViews forced to 1): **accepted, rc 0** | **REFUSED** |
| **F4** | C2 | `src/lodifile.cpp:1182` | `if ( h.version == LODI_VERSION_AGGREGATE )` becomes `if ( h.aggregateCount )` -- the whole aggregate PAYLOAD gate reaches version 5 | `agg-record` (a cell-order/index violation): **accepted, rc 0** | **REFUSED** |
| **F5** | C4 | `src/nifcli.cpp:7607`, `:7629` | the `--land-guide` warning says `the default stands` instead of `off stands` | the exe printed `off stands` and then let the DEFAULT `flatwarp:1.0` stand | the message names what actually happens; no baked byte moves |
| **F6** | C8 | `src/nifcli.cpp:7430`, `:7436`, `:8051` | the lodgen arg loop remembers the switch whose value was missing; after the loop, `error: <switch> needs a value`, `return 2` | `--incremental` spelled last: **rc 0, 15 files baked, no diagnostic** | **rc 2 in 0 s, 0 files written**, `error: --incremental needs a value` |
| **F7** | found by this lane's own verifier, not by the diff | `src/lodifile.cpp:1317` | the `aggregateStride` REPORTING ternary reaches version 5 | `--native-verify` on `bake/aggreal` (97 aggregates, stride 48 in the bytes) printed `aggregateStride 0` beside `aggregateCount 97` | prints **48**, while the three no-aggregate v5 bakes still print 0 |

**F3 came within one token of refusing every default bake.** My first reading of
`src/lodifile.cpp` had `h.aggregateCount` being read only inside the v4-only
block, which would have made the new guard dead. Reading `:843-880` showed the
v5 block reads all eight aggregate words at `:868-875`, so the count is
populated -- and that is exactly why the guard is `( v5 && h.aggregateCount )`
and not `v5` alone: the v4 rules include a `aggregateCount == 0` refusal, and a
default bake is version 5 with `aggregateCount 0`. Without the `&&` clause F3
would have refused every shipped default bake. The refuter for that is block A
of section 6.3, which is the only reason I know it.

**F7 is a fix that only existed because of F1--F6.** It is not in the diff
review: nothing in the campaign's diff is wrong on its face at `:1317`. It
surfaced when block A of my verifier printed the `aggregateStride` each
legitimate file reports, to prove the new refusals had not started refusing
real files -- and `bake/aggreal` reported 0 for a word its own header holds at
48. The ternary could not simply be dropped: `aggregateStride` defaults to
`LODI_AGGREGATE_STRIDE` (48) in `src/lodifile.h:377`, not to 0, so an
unguarded print would make a version-3 file report 48 out of thin air. The word
is now printed exactly when the reader read it, which is v4 or v5.

**The exe on disk carries the edits, checked in its own bytes rather than in
the build log.** Three strings, counted in both exes with `grep -a -c`:

| string | audited exe | fixed exe |
|---|---|---|
| `off stands` | 1 | **0** |
| `the default stands` | 1 (the sibling message that already said it) | **2** |
| `needs a value` | 0 | **1** |

A build log that says `rc=0` says the compiler was happy, not that the binary
beside it is the one that was compiled. This is the cheapest instrument that
answers the second question.
