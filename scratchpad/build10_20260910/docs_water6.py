#!/usr/bin/env python3
"""Lane BUILD10 -- WATER6's WW_CHANGES entry, at the TOP, and spec_water 3.5.

WW_CHANGES.md is MIXED (19,020 CR); the 2026-09 entries at the top are LF-only
and so is this one.  The CR count is asserted unchanged.
"""
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

TOP = "# NifSkope — Wild Wasteland Edition: Change Log\n\n"

ENTRY = """## 2026-09-10 -- the solver consumes what the water window stores, and the flow PNG speaks DirectX (lane WATER6, built and gated)

Lane WATER5 shipped a window that stores three things the solver did not read
(`scratchpad/water5_20260910/CHANGE_NEEDED.md` C1-C3), and wrote its flow map
with `+green = north`. This lane closes all four. `release/NifSkope.exe`
**16:45:53**; the gates were pre-registered in
`scratchpad/lane_water6_report.md` section 0 before any of the C++ existed.

* **Per-point weights.** A curve's points each carry a speed weight (1 = the
  curve's own speed). The conductance bump under the stroke was
  `1 + (4 - 1) q^2`; it is now `1 + (4 w - 1) q^2` with `w` interpolated along
  the segment, and the same weight multiplies the speed before the body's mean
  is taken. **At w = 1 that is the old expression character for character**,
  which is what makes the floor a BYTE gate: X1a hashes the body's flow words
  with no weights and with an explicit 1.0 at every point and gets
  `4fcec45d675860cd` twice over 29,312 texels. A weight ramped 1 -> 3 changes
  the hash, changes 76.5 percent of the body's texels (not all of them), and
  makes the reach read faster where the weight is: the mean speed nibble rises
  by **1.28 along the weighted stroke against 1.10 unweighted** on body 2 and
  **1.08 against 0.54** on the Charles.
* **A one-point curve is a pin, and it is a SOURCE.** `solveBody` used to skip
  any curve with fewer than two points; one now enters as a source disc of its
  own width -- the treatment a stroke's first point already gets. A sink is
  what the store's `OutletPin` already means, and the report says so rather
  than leaving it to be guessed. Measured: the pin moves **29,310 of 29,312**
  texels where it moved 0 before, with the refuter (the same body, pin removed,
  0 moved) run first.
* **An imported flow map is the AUTHORITY where painted.** `flowWordOf` asks
  the kind-10 raster layers before the solved field and before the automatic
  word, last painted wins; the layers are decoded once by lane WATER5's own
  decoder into a cache refreshed at the top of every read pass. The solve is
  not touched: this is a question about the word the document WRITES.
  Measured: **190 of 190** painted texels read the layer, **0** texels outside
  it move from the layer-free solve, and removing the layer puts **0 of 29,312**
  words wrong again.
* **The flow PNG's DirectX convention.** `R = 128 + 127 cos(theta)`,
  `G = 128 + 127 * -sin(theta)` -- both channels centred on 128, and green
  grows toward the image BOTTOM, which is what Substance, Houdini and every
  DirectX-era exporter write. East is (255, 128), north (128, 1), **south
  (128, 255)**. It round-trips all **65,536** words, the export -> import gate
  is still **0 differ of 21,754,958** painted texels, and the flipped-green
  refusal now says "+green = south, the DirectX convention" and still fires
  (0.151 as-is against 1.000 mirrored) with the control accepted right after.
  `tests/fixtures/flowmap_directx_4x4.png` is checked in: 16 texels written
  from the RULE by a script that shares no code with the codec, so a codec that
  round-trips itself perfectly and is nevertheless wrong still fails.

**The gates.** `tests/spells/water_weights.sh` (new) **PASS**, 16 of 16
against a floor of 15. `water_flow.sh` 63 checks / 3 failures on the Charles,
`water_mark.sh` 63 / 8 model + 20 / 0 dock, `water_window.sh` 46 / 2,
`lodl_water.sh` PASS, `lodl_open.sh` 23 / 0. **F2's island bank is still
12.01 / 22.40 and F5's p99 is still 8.44**, to the same two decimals as before
the weight change -- which is the evidence that the unweighted solve is
untouched across the whole file and not only on the body X1a hashes.

**One gate is RED as registered and was not moved.** X2b asks that the water
round a one-point pin points away from it, measured as the mean cosine against
the straight-line radial over four pin widths: **0.742 on body 2, 0.371 on
body 3** against 0.5. The sign is not in doubt (a sink reads below -0.5). The
Charles BENDS inside four widths, so the radial and the channel-following flow
diverge geometrically and the cosine falls without the pin behaving
differently. The instrument a later lane should use is named in the report:
the net flux through a ring around the pin, which curvature cannot bias.

**A crash was found and repaired in a HARNESS, not in the product.** The water
window's self-test captured `WaterMarkDoc * doc` once and reopened the file at
gate W3, after which `openFile`'s `delete doc` made every later use a
use-after-free. It had never crashed because `sweep()` began by reading a bool
and a pointer that happened to survive; this lane made `sweep()` begin by
comparing two QVector members and the process died with exit 139, taking gates
W4, W5, W6 and the check count with it. One line -- re-read the pointer after
the reopen -- and no check, assertion or widget touched.

**bungo's window:** he had `release\\NifSkope.exe` open (pid 20560) while this
built. It was RENAMED ASIDE, never killed, so his window keeps working on
`release/NifSkope_inuse_20560.exe`; **his next launch of `release\\NifSkope.exe`
(16:45:53) is the one with all of this in it.**

"""


def main():
    p = os.path.join(ROOT, "WW_CHANGES.md")
    b = open(p, "rb").read()
    cr = b.count(b"\r")
    a = TOP.encode("utf-8")
    n = b.count(a)
    print("anchor count=%d CR=%d bytes=%d" % (n, cr, len(b)))
    assert n == 1
    assert b.count(b"lane WATER6, built and gated") == 0, "already spliced"
    t = ENTRY.encode("utf-8")
    assert t.count(b"\r") == 0
    out = b.replace(a, a + t)
    assert out.count(b"\r") == cr
    open(p, "wb").write(out)
    print("wrote WW_CHANGES.md %d -> %d bytes, CR %d unchanged" % (len(b), len(out), cr))


if __name__ == "__main__":
    main()
