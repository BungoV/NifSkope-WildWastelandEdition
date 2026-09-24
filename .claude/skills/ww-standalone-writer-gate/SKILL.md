---
name: ww-standalone-writer-gate
description: Gate a NEW binary-format writer/reader pair in the NifSkope Wild Wasteland tree (E:\Projects\NifskopeWildWastelandEdition) when release/NifSkope.exe cannot be built or run -- the game is up, another lane's gates hold the exe, or the lane is on account B. Links the writer translation units standalone against Qt6Core with a 40-line main, runs the hand-written known-answer fixture, the INDEPENDENT decoder, the two-write byte-identity check, and single-byte mutations with the CRCs re-signed so every row rule is seen to refuse by name. Written from lane NATIVE0b (2026-09-10), which proved .lodo/.lodi this way in one hour while the exe was locked. Use for any src/*file.{h,cpp} container writer before its CLI hook-up exists; never as a substitute for the built exe's own gate afterwards.
---

# Standalone writer gate (no exe, no make)

## When
A format writer lives in its own translation units (`src/lodofile.cpp`, `src/lodifile.cpp`,
`src/io/lodvfile.cpp` ...) that depend on QtCore only, and the exe is unavailable. CONSTITUTION 6
says a lane that cannot build ends BUILD PENDING -- but "cannot build" is not "cannot measure":
the writers can be linked alone, in ~30 s, without touching `release/` or `GeneratedFiles/`.

## The four measurements, in order
1. **Syntax with the REAL flags** (`nifskope-ww-build-verify`, "When you CANNOT build"):
   `bash scratchpad/<lane>/sx_tmp.sh src/x.cpp`, RC from `${PIPESTATUS[0]}`, under MSYS2 UCRT64.
2. **Standalone link + the known-answer fixture.** A `fixture_main.cpp` with three modes
   (`<dir>` write the fixture, `--verify <a> <b>` read back with every check on, `--mutate <in>
   <out> <off> <xor>` flip one byte). Link:
   ```
   MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'cd /e/Projects/NifskopeWildWastelandEdition && g++ -std=gnu++2a -O1 -fexceptions -mthreads -DUNICODE -D_UNICODE -DWIN32 -DQT_NO_DEBUG -DQT_CORE_LIB -DQT_NEEDS_QMAIN -Isrc -Isrc/io -IC:/msys64/ucrt64/include/qt6 -IC:/msys64/ucrt64/include/qt6/QtCore -Ilib scratchpad/<lane>/fixture_main.cpp src/a.cpp src/b.cpp src/io/lodvfile.cpp -LC:/msys64/ucrt64/lib -lQt6Core -o scratchpad/<lane>/fixture_tool.exe'
   ```
   Run the tool FROM THE MSYS2 SHELL (Qt6Core.dll is on its PATH, not Git-Bash's).
   `src/io/lodvfile.cpp` includes `lodtfile.h` for a constant but links alone (it calls no
   `lodt*` symbol) -- `-Isrc -Isrc/io` and nothing else.
3. **The independent decoder** (`tests/spells/<fmt>_decode.py`: struct + zlib, no C++ shared)
   with `--expect <fixture>/X.expect.txt` -- the answers the fixture wrote down BEFORE the run.
   Report `N checks, 0 failures` with N. Then two writes -> `cmp` both files.
4. **Refusal controls, two tiers.** Tier one, a plain byte flip: the CRC answers
   (`headerCrc32` / `indexCrc32` / a chunk `crc32`) and proves only the CRC. Tier two,
   `resign.py <in> <out> <off> <xor>`: flip the byte, then recompute every CRC that covers it
   from the CONTRACT's arithmetic, so the row rule is what refuses -- "base table is not
   sorted by formId ascending at row 1", "family 2 is neither legacy nor pbr", "reserved
   header byte at 0xb9". Run BOTH readers on each mutant and quote both refusal strings.
   Worked copies: `scratchpad/native0_20260910/{fixture_main.cpp,resign.py}`.

## Traps that cost a control each
* **A float's sign bit is byte +3 of the field**, not +2. `216.0f` = `43 58 00 00` LE; flipping
  byte +2's 0x80 gives 432.0, still positive, and the "never 0" rule is never exercised.
  Compute the offset from the row layout and say which byte in the control's label.
* **A fixture with one instance per chunk cannot exercise an in-chunk order rule.** Mutating
  the ref of a lone instance is not an order violation and the reader is RIGHT to accept it.
  Either add a two-instance chunk to the fixture or report the rule as checked-but-unexercised.
* **The mutation label must match the offset arithmetic**, and the reader's refusal text is
  the proof, not the label: paste the string.
* A `--verify` mode in the fixture tool that only prints `identity matches|MISMATCH` does not
  exercise the pairing REFUSAL in the real reader (`lodgenNativeVerify`); the decoder's
  `FAIL pair:` line does. Quote the one that refused.

## What this does not prove
Linking, moc, the `.pro` dependency lists (qmake is owed after adding sources), the CLI
hook-up, and every number about a REAL worldspace file. The lane still ends BUILD PENDING
with these four measurements in its report, and the built exe re-runs the fixture through
its own `--<fmt>-fixture` switch as the first gate.

## Name the rule that must answer, and check the refusal NAMES it (lane NATIVE1a, 2026-09-11)

A mutation that the file rejects proves nothing on its own: what matters is WHICH rule
rejected it. Give every case the substring its refusal must contain and fail the case when
the refusal does not carry it, even though the file was refused. Two traps cost this lane a
round each, and both look like a passing gate:

* **A field INSIDE the header CRC's own range is answered by the CRC, not by the rule.**
  `.lodo`'s `headerCrc32` covers 0x10..0xFF, and `indexCrc32` is stored at 0xA8 -- inside it.
  Flipping the stored `indexCrc32` is therefore caught by `headerCrc32` and the case reads
  "refused" while `indexCrc32` was never consulted. Corrupt a PAYLOAD byte instead and
  re-sign ONLY the header, so `indexCrc32` is the rule with something to say. Keep a
  header-only re-sign beside the full re-sign for exactly this.
* **A mutation that also moves a DERIVED field is answered by the pairing rule first.**
  Flipping `objectCorpusHash` to test a staleness check also changes `lodoIdentity`
  (FNV over headerCrc32, modelCorpusHash, objectCorpusHash), so the reader refuses with
  "lodoIdentity does not name this .lodo" and the staleness rule never runs. Re-derive every
  dependent field after the mutation, then re-sign, so the rule UNDER TEST is the one that
  fires. Write the re-derivation into the doctor, not into the report as an excuse.

The general form: before writing a case, ask what else the mutated bytes feed, and either
re-derive it or move the mutation. Then read the refusal TEXT, not the exit code.
