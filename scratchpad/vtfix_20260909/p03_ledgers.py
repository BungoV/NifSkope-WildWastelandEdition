# VTFIX patch 3 -- the two ledgers.
#   WW_CHANGES.md is MIXED (19,020 CR / 23,237 LF); its top region is LF-only,
#   so the new entry is LF and the CR count must not move by one byte.
#   MISTAKES.md is LF-only, newest at the top.
import io, os
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")

# ---------------------------------------------------------------- WW_CHANGES
P = "WW_CHANGES.md"
b = open(P, "rb").read()
CR0, LF0 = b.count(b"\r"), b.count(b"\n")
assert (CR0, LF0) == (19020, 23237), (CR0, LF0)
TITLE = b"# NifSkope \xe2\x80\x94 Wild Wasteland Edition: Change Log\n\n"
assert b.startswith(TITLE)

ENTRY = """## 2026-09-09 - V9a: the terrain colour sheet is a THIRD consumer of the terrain normal

`tests/spells/lodgen_terrain_vt.sh` (harness only -- **no source changed**).

V9a asked for the `--vt` assembled dim-4 colour sheet to be byte-identical to a
direct bake, and after lane BUILD2 it was not: same size (174,888 B), different
bytes. Attributed and bounded by lane VTFIX; the full working is
`scratchpad/lane_vtfix_report.md`.

**It is not `dominantBase`.** Measured, on the harness's own fixture, from a
copy of the BUILD2 exe: with `--cover` the two colour sheets differ; **without
`--cover` they are byte-identical**, on all four dim-4 chunks of the fixture.
`dominantBase` reaches the colour composite unconditionally, so a rescope would
move texels with the tint off as well. (Reading agrees: the tile baker scopes it
to the enclosing dim-4 chunk's 16 cells, which is the chunk baker's own set.)

**What it is.** `nrm` is computed before the colour composite on both paths, and
its Z is the ground-cover slope gate's operand -- `theta = acos(nrm.z)`, then
`gate`, then `coverByte`, then `tintW = coverByte/255 * tintStrength` (0.350),
which tints the COLOUR. `spec_terrain_vt.md` §4.4 exempts the msn and the AO
within one heightfield sample of a chunk boundary, because the tile bake has a
one-cell ring where the chunk bake clamps; nobody noticed that with `--cover`
the colour inherits that same exemption through the tint. The msn is not the
sheet's only consumer.

MEASURED, mip 0, per texel, through a BC1 decoder that shares no code with the
writer (`scratchpad/vtfix_20260909/ddsdiff.py`):

| chunk | floor (`--no-cover`) | ceiling (tint on vs off) | under test | max | in the 4-texel band |
|---|---|---|---|---|---|
| `4.-24.24` | 0 | 207,945 (79.3%) | **43** of 262,144 (0.016%) | 17/255 | 43 of 43 |
| `4.-20.24` | 0 | 191,588 | **84** (0.032%) | 12/255 | 84 of 84 |
| `4.-24.28` | 0 | 6,979 | **0** | 0 | -- |
| `4.-20.28` | 0 | 21,760 | **0** | 0 | -- |

Every differing texel is within 4 texels -- 128 world units, exactly one
heightfield sample -- of the chunk's OUTER boundary, and the nearest one to an
interior dim-2 tile seam is 29 texels away, which is the shape a `dominantBase`
rescope would NOT have.

**The assembled bytes are the better ones**, measured rather than assumed: the
ground is continuous, so the step across a chunk seam should be the step of any
adjacent texel pair. `scratchpad/vtfix_20260909/seam.py` reads the `_msn` step
across the (-24,24)|(-20,24) seam at **4.955 ringed against 7.312 clamped**
(-32%), north/south 4.600 against 6.068 (-24%), with the interior control the
same to three digits on both bakes (1.803 / 1.797) and the step onto a sheet's
own last column 1.961 ringed against 3.310 clamped (-41%). Vanilla cannot
arbitrate at this size and is reported as a negative result: our sheet differs
from Bethesda's on 99.6-99.8% of texels either way, and the two variants'
distance to vanilla agrees to three decimals (18.649 both).

**Attribution.** Lane RENAME's only edits to `src/lodgen.cpp` are two comments,
three path/message strings and three hex comments beside unchanged values. Run
on `release/NifSkope.before.exe` (BUILD1: TERRAINFIX in, RENAME out), all twelve
sheets -- colour, `_msn`, `_data`, four bakes -- are byte-identical to BUILD2's.
The bytes moved with lane TERRAINFIX's shared normal reconstruction, and they
moved through the cover gate.

**V9a re-pinned, in two halves, each with a control:**
* with the tint OFF, byte identity on all four dim-4 chunks -- the exact
  `dominantBase` gate the spec asked for, and now it is on the pair where byte
  identity can honestly hold;
* with the tint ON, every differing texel within 4 texels of the chunk's outer
  boundary, no texel within 8 of an interior tile seam, max channel difference
  <= 24/255 (measured 17), differing texels <= 0.05% of a sheet (measured
  0.032%), **and at least one chunk must differ** -- a floor, so a check that
  cannot fail is refused.

The check was run against three inputs before it was trusted: the real pair
(0 bars failed), a sheet-wide difference (16 bars failed across the four
chunks), and two identical sheets (the floor fired). `lodgen_terrain_vt.sh` is
**32 checks, 0 failures**; `lodgen_terrain.sh` 26/0 with the pyramid msn
unchanged at `UP=G 76/32/51/32` against the direct bake's `76/32/51/32` and
vanilla's `99/67/67/67`; `lodgen_identity.sh` PASS.

**OPEN, and it is bungo's call, not a lane's:** the real defect is the DIRECT
chunk bake's edge clamp, not the assembled sheet. Giving the chunk baker the
same one-cell ring would make both paths correct AND identical, but it moves the
edge bytes of every terrain chunk sheet in every worldspace and re-baselines the
byte-identity gates.

"""
out = TITLE + ENTRY.encode("utf-8") + b[len(TITLE):]
assert out.count(b"\r") == CR0, (out.count(b"\r"), CR0)
open(P, "wb").write(out)
print("OK", P, len(out), "CR", out.count(b"\r"), "LF", out.count(b"\n"))

# ------------------------------------------------------------------ MISTAKES
P = "MISTAKES.md"
b = open(P, "rb").read()
assert b.count(b"\r") == 0
HDR = b"Newest at the top.\n\n"
i = b.index(HDR) + len(HDR)

M = """## 2026-09-09 -- a byte-identity bar was written for a sheet with three consumers, and only two were counted

- **What was done:** V9a asked the `--vt` assembled colour sheet to be
  byte-identical to a direct bake, with the note *"expected where dominantBase
  is chunk-scoped; a difference here means it was rescoped"*. The spec's own
  §4.4 had already accepted that the ringed tile bake and the clamped chunk bake
  disagree on the terrain NORMAL and the AO within one heightfield sample of a
  chunk boundary, and exempted the msn (V9b) and the AO (V9c) there.
- **What was true:** the normal has a third consumer. `nrm.z` is the ground-cover
  slope gate's operand, the gate makes `coverByte`, and `coverByte` weights the
  tint that is composited into the COLOUR. So with `--cover` the colour inherits
  exactly the exemption V9b and V9c were given, and a bar of byte identity
  cannot hold on that pair. Measured: 43 and 84 texels of 262,144 on two of the
  fixture's four chunks, max 17/255, every one of them inside the same 4-texel
  boundary band -- and with `--cover` off, byte-identical.
- **How it was found:** by asking which of the two suspects can reach the colour
  with the tint OFF. `dominantBase` can; the normal cannot. Two more bakes,
  eleven seconds, no rebuild.
- **The rule:** before pinning a channel to byte identity, list every consumer
  of every operand that channel reads, not just the operand's own output sheet.
  If any of those operands is already exempted somewhere, the exemption
  propagates to this channel too, and the bar has to be written around it.

## 2026-09-09 -- a comparison script reported DIFFERS for a file that did not exist

- **What was done:** `scratchpad/vtfix_20260909/bake4.sh` ended with two
  `cmp -s A B && echo IDENTICAL || echo DIFFERS` lines. A run against a
  hand-assembled copy of `release/` failed with rc=127 on all four bakes (a
  missing Qt DLL), wrote no sheets at all, and the script printed
  `cover: DIFFERS / nocover: DIFFERS` -- which is what a real, meaningful result
  looks like.
- **What was true:** nothing had been compared. `cmp` returns 2, not 1, for a
  missing operand, and `||` cannot tell 1 from 2.
- **How it was found:** the `== colour sheets ==` block above it printed
  `MISSING` four times in the same output. Had that block not been there the
  wrong verdict would have gone into the report.
- **The rule:** a comparison states its inputs exist before it states its
  verdict, and a missing input is its own outcome, never folded into "different".

## 2026-09-09 -- a control drawn one texel from the boundary sat inside the defect it was controlling for

- **What was done:** the first version of `scratchpad/vtfix_20260909/seam.py`
  used the step between a sheet's last two columns as the control for the step
  across the chunk seam, and read ratios of 2.53 (ringed) against 2.21
  (clamped) -- i.e. it made the CLAMPED bake look better.
- **What was true:** the clamped bake's own edge column is the defect. Putting
  it in the control inflated the denominator by 84% (3.310 against 1.797) and
  hid the effect. With the control taken from four adjacent-column pairs well
  inside the sheet, the ratios are 2.75 ringed against 4.07 clamped.
- **How it was found:** the two bakes' controls disagreed (1.961 vs 3.310) where
  a control that measures the terrain and not the defect must agree, and they do
  agree in the interior (1.803 vs 1.797).
- **The rule:** a control for a boundary defect is sampled where the defect
  cannot reach, and the check that it was is that the control reads the same on
  both subjects.

## 2026-09-09 -- a patch was typed into a heredoc, the trap `nifskope-ww-lodgen` names by name

- **What was done:** the fix for the mistake above was first attempted as a
  Python patch inside a bash heredoc rather than as a script written with the
  Write tool. The anchor matched zero times and the assertion fired.
- **What was true:** the skill's editing-traps section says exactly this --
  *"Heredocs halve backslashes and mangle `\\n` inside Python; write patch
  scripts with the Write tool and run them"*. The anchor carried both a line
  continuation and a tab.
- **How it was found:** the script's own `assert count == 1`, which is why the
  file was not damaged.
- **The rule:** the one in the skill, applied without exception. Recording the
  repeat is itself the entry (CONSTITUTION rule 2).

"""
out = b[:i] + M.encode("utf-8") + b[i:]
assert out.count(b"\r") == 0
open(P, "wb").write(out)
print("OK", P, len(out), "CR", out.count(b"\r"), "LF", out.count(b"\n"))
