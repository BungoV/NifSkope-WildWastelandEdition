# VTFIX patch 4 -- append sections 3..6 to scratchpad/lane_vtfix_report.md
# (written as a script rather than a heredoc: the heredoc attempt died on an
#  unmatched quote, which is mistake 4 in the report below.)
import io, os
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")
P = "scratchpad/lane_vtfix_report.md"
s = io.open(P, encoding="utf-8", newline="").read()
assert "\r" not in s
assert "## 3." not in s, "already appended"

TAIL = """
---

## 3. Re-pin, not fix -- and what the re-pin costs

**No source file was changed.** `src/lodgen.cpp`, `src/lodtfile.cpp` and
`src/lodtfile.h` are exactly as lanes TERRAINFIX, RENAME and BUILD2 left them.
The code is right; V9a's bar was not.

`tests/spells/lodgen_terrain_vt.sh` (21,440 bytes, LF-only, CR 0 before and
after; patch script `scratchpad/vtfix_20260909/p01_harness_v9.py`, original kept
beside it as `lodgen_terrain_vt.sh.orig`):

1. **Two more bakes**, `--vt` and direct, both WITHOUT `--cover` (about 11 s
   together).
2. **V9a-1, the `dominantBase` gate, exact.** With the tint off, the assembled
   colour sheet must be byte-identical to the direct bake **on all four dim-4
   chunks** of the fixture, not one. This is what the spec actually wanted, now
   asked on the pair where byte identity can honestly hold.
3. **V9a-2, with the tint on**, through a BC1 decoder re-typed from the format
   inside the harness (a check that decoded through `lodgenWriteDds` could not
   fail on `lodgenWriteDds`). Four bars and a floor:
   * every differing texel within **4 texels** of the chunk's OUTER boundary
     (that band is 3.1% of a sheet, so this is discriminating, not permissive);
   * no differing texel within **8 texels** of an interior dim-2 tile seam
     (measured nearest: 29) -- the `dominantBase`-rescope refuter;
   * max channel difference **<= 24/255** (measured 17);
   * differing texels **<= 0.05%** of a sheet (measured 0.016% and 0.032%);
   * **at least one chunk must differ** -- the floor. Without it, a change that
     silently disabled the cover pass, the tint or the decode would make all
     four bars pass on nothing.

**The check was run against three inputs before it was believed** (CONSTITUTION
rule 4 -- show the invariant failing):

| input | result |
|---|---|
| the real pair (`--vt --cover` vs `--cover`) | 127 differing texels over four chunks, **0 bars failed**, rc 0 |
| a sheet-wide difference (`--cover` vs `--no-cover`, the ceiling) | 428,272 texels, **16 bars failed** across the four chunks, rc 1 |
| two identical sheets (the no-cover pair) | 0 texels, **the floor fired**, rc 1 |

**Every number pinned in the harness is quoted with its measurement in the
comment beside it**, and the justification is this report plus the
`WW_CHANGES.md` entry. No bar was moved to fit a reading without one.

## 4. Gates

Run on a COPY of the BUILD2 exe (`scratchpad/vtfix_20260909/ns_run/`,
`release/NifSkope.exe` of 18:43:13) so lane OFFSCREEN2 kept the link slot; the
process table was checked once and showed neither `Fallout4.exe` nor
`NifSkope.exe`. **No build was needed or attempted** -- the change is a harness
and two ledgers, and the exe is already newer than every source the harness's
own preflight names (it passes that check in the log).

| gate | result |
|---|---|
| `tests/spells/lodgen_terrain_vt.sh` | **32 checks, 0 failures, RESULT PASS**, rc 0 (was 31 checks / 1 failure; V9a became two checks) |
| `tests/spells/lodgen_terrain.sh` | **26 checks, 0 failures, PASS**, rc 0 |
| `tests/spells/lodgen_identity.sh` | **RESULT PASS**, rc 0 |
| the pyramid `_msn` numbers from lane TERRAINFIX | **unchanged**: assembled `UP=G D0=76 D1=32 D2=51 D3=32`, direct `UP=G 76/32/51/32`, vanilla's own sheet `UP=G 99/67/67/67` as the control -- statistic for statistic what BUILD1 reported |

Logs: `scratchpad/vtfix_20260909/logs/`. Files changed: `WW_CHANGES.md`
(+81 lines, 0 removed, mixed file, **CR unchanged at 19,020**), `MISTAKES.md`
(+70, LF-only, CR 0), `tests/spells/lodgen_terrain_vt.sh` (342 -> 442 lines,
LF-only, CR 0), and `scratchpad/vtfix_20260909/`. **No commits**
(CONSTITUTION 8).

**Owed, and named so it is not lost:** the real defect is the DIRECT chunk
bake's edge clamp, not the assembled sheet. Giving the chunk baker the same
one-cell ring the tile baker has would make both paths correct AND
byte-identical, and V9a could go back to a plain `cmp`. It moves the edge bytes
of every terrain chunk sheet in every worldspace and re-baselines the
byte-identity gates, so it is bungo's call, not a lane's.

## 5. Mistakes

Four entries, written into `MISTAKES.md` at the root the moment they were
recognised:

1. **A byte-identity bar written for a sheet with three consumers, and only two
   counted.** V9a pinned the colour to byte identity while the spec had already
   exempted the normal at a chunk boundary; the colour reads that same normal
   through the ground-cover slope gate. Rule: before pinning a channel to byte
   identity, list every consumer of every operand it reads -- an exemption
   granted to an operand propagates to every channel that reads it.
2. **A comparison script reported DIFFERS for a file that did not exist.** My
   own `bake4.sh` printed `cover: DIFFERS / nocover: DIFFERS` from a run that
   had failed rc=127 and written nothing, because `cmp` returns 2 for a missing
   operand and the `||` branch cannot tell 2 from 1. Caught by the `MISSING`
   lines printed above it. Fixed (`p02_bake4_missing.py`) and the guard
   demonstrated firing.
3. **A control drawn one texel from the boundary sat inside the defect.** The
   first seam statistic used a sheet's last two columns as its control and made
   the CLAMPED bake look better (2.53 against 2.21); with the control taken from
   the sheet's interior the reading reverses (2.75 against 4.07). Found because
   the two bakes' controls disagreed by 84% where a control must agree.
4. **Patches typed into a heredoc**, which `nifskope-ww-lodgen` names as a trap
   by name -- first a Python patch whose anchor then matched zero times, and
   later a bash heredoc holding this very report section, which died on an
   unmatched quote. Both were caught (an assertion, and a parse error), so
   nothing was damaged; recording the repeat is itself the entry.

## 6. Finished-work skill review (CONSTITUTION 1a)

**Loaded and used.** `nifskope-ww-lodgen` -- the CLI table (`--vt`, `--cover`,
`--tex-dir`, `--terrain-region`, the worldspace IDs), the rule about copying
`release/` to a scratch directory for a long CLI run that must not block builds,
which is the whole reason this lane could measure anything while OFFSCREEN2 held
the link, and the editing traps (patch scripts with anchor counts, Python byte
counts for line endings) -- see mistake 4 for the two times I did not.
`ww-control-calibration` -- its five parts map onto section 2.1 one for one: the
known-answer input, the floor carrying the signal's own amplitude through the
same lossy pipeline, the ceiling from the same data with the property removed,
the independent replication, and the instruction to report the negative result
(vanilla) rather than quietly drop it. `nifskope-ww-build-verify` and
`nifskope-ww-resume-pending` were named in the brief and **not** loaded: no
build was needed or attempted, so neither procedure applied. Saying so rather
than pretending.

**The skill that SHOULD exist, and I am recommending it rather than writing it
into a tree other lanes hold: `ww-sheet-diff`** -- decode two of our DDS sheets
and say WHERE they differ. Three pieces of this lane were re-derived from first
principles and will be needed the next time a bake is questioned: a BC1/BC3
decoder that shares no code with `lodgenWriteDds` (written twice today, once as
`scratchpad/vtfix_20260909/ddsdiff.py` and once inside the harness, because a
spell may not import from `scratchpad/`); the locality statistic that separates
a chunk-boundary defect from a tile-seam defect (distance to the outer boundary
AND to the interior seam, with the band's share of the sheet printed beside it,
or 100% within 4 texels means nothing); and the seam-continuity test with its
interior control, which is the only instrument here that says which of two
variants is RIGHT rather than merely which is different. The working code is
`ddsdiff.py`, `analyze.py`, `analyze2.py` and `seam.py` under
`scratchpad/vtfix_20260909/`, and the trap it must carry is mistake 3. **The
director should place it in both skill trees**, since they drift.

**Declined, with reasons.** A skill for mistake 1's lesson: it is one paragraph
and it belongs beside the thing it guards, which is where it now is -- a comment
at both halves of V9a and an entry in `MISTAKES.md`. A skill nobody would think
to load is a skill nobody loads. And a skill for running three lodgen harnesses
on a scratch copy of the exe: four lines of shell, already the second half of a
`nifskope-ww-lodgen` bullet.
"""
out = s + TAIL
assert "\r" not in out
io.open(P, "w", encoding="utf-8", newline="").write(out)
print("OK", P, len(out.encode("utf-8")), "bytes, LF", out.count("\n"))
