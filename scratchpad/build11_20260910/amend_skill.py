#!/usr/bin/env python
"""Lane BUILD11: replace the WW_RENDER_SIZE paragraph in nifskope-ww-render-shot
with the MEASURED law, in BOTH skill trees (they are byte-identical today, so the
same splice applies to each and each is asserted afterwards).

The old text says the size "is clamped to one screen" and that 1400x1400 and
360x360 both give 1507x1067. Measured on release/NifSkope.exe 17:08:39, three
requests, each read back with PIL: 1000x1000 -> 1293x941, 1500x1059 -> 1500x1000,
1800x800 -> 1800x741. That is not one clamp; it is a HEIGHT that always loses 59
px of chrome and a WIDTH raised to the window's own minimum when it is under it.
Lane SKELFIX's 1437x941 was the same law with a different (older) floor.
"""
import os, sys

TREES = [
    r"E:\Projects\NifskopeWildWastelandEdition\.claude\skills\nifskope-ww-render-shot\SKILL.md",
    r"E:\Projects\Claude\.claude\skills\nifskope-ww-render-shot\SKILL.md",
]

OLD = """* **`WW_RENDER_SIZE` is clamped to one screen.** Asking for 1400x1400 on this
  machine gives **1507x1067**, and so does asking for 360x360. The hook says so
  itself near `src/nifskope_ui.cpp:21404`. Never quote a requested size as the
  picture's size -- read it back with PIL.
"""

NEW = """* **`WW_RENDER_SIZE` is not a clamp -- it is `max(width, a floor)` by
  `height - 59`** (measured 2026-09-10, lane BUILD11, on `release/NifSkope.exe`
  17:08:39; three requests, every one read back with PIL):

  | requested | measured |
  |---|---|
  | 1000x1000 | **1293x941** |
  | 1500x1059 | **1500x1000** |
  | 1800x800 | **1800x741** |

  The HEIGHT is exactly `requested - 59` every time -- the window chrome the
  framebuffer does not get. The WIDTH is honoured exactly when it is above the
  main window's own `minimumSizeHint`, and silently RAISED to that floor when it
  is not: the hook does `skope->resize( rw, rh )` (`src/nifskope_ui.cpp` ~21636,
  after `qMax( ..., 320 )` / `qMax( ..., 240 )`) and a `QMainWindow` will not go
  below its minimum. **The floor moves with the build** -- 1437 on the 15:52:46
  exe of 2026-09-10 (lane SKELFIX's pictures came out 1437x941 from a request of
  1000x1000 and it was logged as a defect), 1293 on the 17:08:39 exe after the
  workspace and bar-alignment work -- so it is MEASURED per build, never
  remembered. To get an exact WxH picture: ask for `W x (H+59)` with W above the
  floor, and read the result back. An older note said the size "is clamped to one
  screen" and that 1400x1400 and 360x360 both gave 1507x1067; that was the same
  two rules seen through one machine's screen size. Never quote a requested size
  as the picture's size -- read it back with PIL.
"""


def main():
    apply = "--apply" in sys.argv
    for p in TREES:
        b = open(p, "rb").read()
        n = b.count(OLD.encode("utf-8"))
        print("%-70s bytes=%d CR=%d old_count=%d" % (p, len(b), b.count(b"\r"), n))
        if b"lane BUILD11" in b:
            print("    already amended")
            continue
        assert n == 1, p
        if apply:
            nb = b.replace(OLD.encode("utf-8"), NEW.encode("utf-8"), 1)
            assert nb.count(b"\r") == b.count(b"\r")
            open(p, "wb").write(nb)
            print("    wrote %d -> %d" % (len(b), len(nb)))
    if not apply:
        print("--check only")
    return 0


if __name__ == "__main__":
    sys.exit(main())
