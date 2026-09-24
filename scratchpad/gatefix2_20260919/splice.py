#!/usr/bin/env python
"""GATEFIX2's two document writes, in binary, matching each file's own endings.

  python splice.py check    -- report only
  python splice.py write    -- splice MISTAKES.md and append to both skill trees
"""
import io
import os
import sys

ROOT = "E:/Projects/NifskopeWildWastelandEdition"
MIST = ROOT + "/MISTAKES.md"
SKILLS = [ROOT + "/.claude/skills/ww-control-calibration/SKILL.md",
          "E:/Projects/Claude/.claude/skills/ww-control-calibration/SKILL.md"]

ENTRY = """## 2026-09-19 -- two gate bars were set from the healthy state alone, on an artefact another lane could re-bake (GATEFIX2)

`tests/spells/native_lighting.sh` carried two bars: darkest-fifth IoU own vs
flat `<= 0.800`, and own-minus-flat blockSD `>= 3.50`.  Both were pinned on
2026-09-16 11:54:47 from ONE measurement of the HEALTHY state (0.705 and 4.56),
against an untracked `.lodt` container that lane VT1 was entitled to re-bake --
and did, at 16:44:04 the same day.  Nobody had measured what the DEFECT each row
exists to catch reads.

Measured 2026-09-19, by building the defects as fixtures out of the own cache's
own tiles and rendering each through the gate's own framing:

      state                                   IoU        blockSD
      the sheet is not read at all            1.000      0.00
      the sheet's UP and NORTH are swapped    0.887      1.54
      THE HEALTHY STATE                       0.850      2.28
      the right normals in the wrong places   0.821      1.00   (0.822 / 0.821 on two more seeds)

The healthy IoU sits INSIDE the broken population, so no threshold of the form
"IoU <= bar" can admit the healthy state and reject all three defects.  That row
was never a gate; it was green for three days only because the container it was
pinned to happened to read 0.705, and red for three days after that container
changed.  The blockSD floor of 3.50 was not wrong in kind, only in provenance:
the healthy state on today's container reads 2.28, and 3.50 was a fact about one
generation of a 5.9 MB file nothing tracks.

How it was found: the defects were realised as INPUT fixtures that need no
rebuild -- an exact copy of the own cache (the input is not read), the own sheet
with two channels exchanged (its axes are wrong), and the own sheet's own BC1
blocks permuted within each tile (right values, wrong places) -- and each was
rendered and measured.

The rule: **a bar is set BETWEEN two measured populations, the healthy state and
the defect the row names, with the margin stated.**  A bar measured only on the
healthy state is a record of one artefact.  When the healthy value lands inside
the broken population the statistic is replaced, not tuned.  And prefer a bar
that is a RATIO against a floor computed in the SAME run from the subject's own
data over any absolute number, because that one cannot go stale when the subject
is re-made.

"""

SKILL = """
## Set the bar BETWEEN two populations, and build the broken one first (lane GATEFIX2, 2026-09-19)

Everything above is about making a floor honest.  This is the failure one step
earlier: a bar set from the HEALTHY reading alone, with nobody having measured
what the defect reads.

`tests/spells/native_lighting.sh` had two such bars, both pinned from one run on
one untracked container.  Measured against the defects they exist to catch:

| state | darkest-fifth IoU | blockSD |
|---|---|---|
| the sheet is not read at all | 1.000 | 0.00 |
| the sheet's UP and NORTH channels swapped | 0.887 | 1.54 |
| **the healthy state** | **0.850** | **2.28** |
| the right normals in the wrong places (three seeds) | 0.821 / 0.822 / 0.821 | 1.00 / 1.07 / 1.04 |

The healthy IoU is INTERLEAVED with the broken values, so no threshold of that
shape exists at all.  The row had never been a gate, and its greenness had only
ever been a property of the artefact it was pinned to.

**Three broken states that need no rebuild.** The defect is usually described as
a code fault ("the shader read the model-space sheet as a tangent-space one"),
and emulating a code fault means building.  Realise it as an INPUT fixture
instead, from the subject's own bytes, and the whole calibration costs one
render each:

| the defect in words | the fixture | what it proves |
|---|---|---|
| the input is not read | an exact COPY of the subject's own cache | the statistic's value when the picture cannot move -- and, free, that the renderer is deterministic (our two frames were byte-identical) |
| the input's axes are wrong | exchange two channels of every texel, re-encoding the endpoints and preserving the block mode | catches a channel-order defect, which in a normal sheet is measured rather than declared and is therefore live |
| right values, wrong places | permute the codec's own blocks within each tile, fixed seed | the phase twin with the amplitude, histogram and codec matched exactly (part 2 above), and the only one of the three that a UV or indexing bug looks like |

Run several seeds for the shuffle and quote the WORST one in the margin.

**Then prefer a bar that cannot go stale.** Two shapes, in order of preference:

1. **A ratio against a floor computed in the same run.** "The container's own
   normals must make at least 1.60x as much block-scale shading as the SAME
   normals shuffled into the wrong places" needs no re-pinning when the
   container is re-baked, because both halves move together.  Give it a small
   absolute companion floor so a total collapse cannot satisfy it as 0/0.
2. **An expectation predicted from the INPUT, calibrated on a known-answer pair
   in the same run.** Looking straight down, N.L is the normal's up component;
   two synthetic arms with exactly known up (0.9988 and 0.8751) give the luma
   response on the same frames; the container's own mean up then predicts the
   frame's mean luma before it is measured. Healthy 0.09 luma from prediction,
   channel-swapped 31.03 -- a 300x separation, and no number in the row belongs
   to any particular container.

   State the lever arm and use the row ONE-SIDED. That calibration spans 0.1237
   of up; a deviation far outside it is a failure, but the size of the deviation
   is not itself a prediction and must not be quoted as one.

**The check, before writing any threshold into a gate:** name the defect the row
exists to catch, build it, and put its number in the same column as the healthy
one. If you cannot state both numbers, you are not setting a bar, you are
recording a measurement.
"""


def splice_mistakes(write):
    b = open(MIST, "rb").read()
    crlf = b.count(b"\r\n")
    print("MISTAKES.md %d B, %d CRLF, %d LF total" % (len(b), crlf, b.count(b"\n")))
    anchor = b"Newest at the top.\r\n\r\n"
    i = b.find(anchor)
    if i < 0:
        raise SystemExit("anchor not found -- refusing to write")
    if b.count(anchor) != 1:
        raise SystemExit("anchor is not exact-once -- refusing to write")
    cut = i + len(anchor)
    add = ENTRY.replace("\n", "\r\n").encode("utf-8")
    print("  anchor at %d, entry %d B, CR %d LF %d (must be equal)"
          % (cut, len(add), add.count(b"\r"), add.count(b"\n")))
    print("  byte before the splice: %r   first byte after: %r" % (b[cut - 2:cut], b[cut:cut + 2]))
    if add.count(b"\r") != add.count(b"\n"):
        raise SystemExit("the entry is not pure CRLF -- refusing to write")
    out = b[:cut] + add + b[cut:]
    if write:
        open(MIST, "wb").write(out)
        n = open(MIST, "rb").read()
        print("  written: %d B, %d CRLF, %d LF (append-only past the header: %s)"
              % (len(n), n.count(b"\r\n"), n.count(b"\n"), n[cut + len(add):] == b[cut:]))


def append_skill(p, write):
    if not os.path.exists(p):
        print("SKILL MISSING %s" % p)
        return
    b = open(p, "rb").read()
    crlf = b.count(b"\r\n")
    pure = crlf == b.count(b"\n")
    add = (SKILL.replace("\n", "\r\n") if pure else SKILL).encode("utf-8")
    tail = b"\r\n" if pure else b"\n"
    sep = b"" if b.endswith(tail) else tail
    print("%s: %d B, CRLF %d of %d LF -> appending %d B as %s"
          % (p, len(b), crlf, b.count(b"\n"), len(add), "CRLF" if pure else "LF"))
    if write:
        open(p, "wb").write(b + sep + add)
        print("  now %d B" % len(open(p, "rb").read()))


def main():
    write = len(sys.argv) > 1 and sys.argv[1] == "write"
    splice_mistakes(write)
    for p in SKILLS:
        append_skill(p, write)
    print("WROTE" if write else "CHECK ONLY -- nothing written")


if __name__ == "__main__":
    main()
