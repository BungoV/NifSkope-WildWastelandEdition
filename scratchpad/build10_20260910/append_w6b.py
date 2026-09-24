#!/usr/bin/env python3
"""Lane BUILD10 -- WATER6's pictures section, appended to its report."""
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
P = os.path.join(ROOT, "scratchpad", "lane_water6_report.md")

TEXT = """
## 9. The pictures

| file | what it shows |
|---|---|
| `scratchpad/build10_20260910/images/directx_convention.png` | the CHECKED-IN test image at texel level, 4 x 4 texels drawn 170 px square, each with its own R and G printed on it, an arrow for the direction, and the direction the shipped decoder reads back. North is DARK green (G = 1) and points up; south is BRIGHT green (G = 255) and points down -- which is the whole of the convention in one picture. The blue cast is B = A = 255 (speed 15, confidence 15), so only R and G carry anything under test |
| `scratchpad/build10_20260910/images/water_window_whole.png` | the window at first open after this lane's build, the whole worldspace fitted (0.12 px a texel); every row still one to a line |
| `scratchpad/build10_20260910/images/water_window_mouth.png` | 2.00 px a texel at the river, the five-point curve with its width band, the source pin and the dye pin, and the summary line naming body 2 "harness river" |

All three were opened and read; the window pair was grabbed from inside the
application, never a desktop capture, with an ABSOLUTE `SHOT` path.
"""


def main():
    b = open(P, "rb").read()
    assert b.count(b"\r") == 0
    assert b.count(b"## 9. The pictures") == 0, "already appended"
    t = TEXT.encode("utf-8")
    assert t.count(b"\r") == 0
    open(P, "wb").write(b + t)
    print("appended %d bytes -> %d, CR 0" % (len(t), len(b) + len(t)))


if __name__ == "__main__":
    main()
