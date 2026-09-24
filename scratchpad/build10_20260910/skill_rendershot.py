#!/usr/bin/env python3
"""Lane BUILD10 -- amend `nifskope-ww-render-shot` where today's build
disproved it (nifskope-ww-resume-pending section 7.4).

The skill says WW_RENDER_SIZE is "clamped to one screen" and that any request
comes out 1507x1067.  Measured on `release/NifSkope.exe` 16:23:22 and 16:45:53
today, four requests:

    1500x1000 -> 1500x941     1600x1000 -> 1600x941
    1920x1080 -> 1920x1021    1507x1000 -> 1507x941

The WIDTH is honoured exactly at least to 1920 and the HEIGHT loses a constant
59 px of window chrome.  A lane that trusted the old sentence would never
reproduce an existing baseline, which is exactly what happened to the Charles
pair before three probe renders found it.
"""
import os

P = r"E:\Projects\Claude\.claude\skills\nifskope-ww-render-shot\SKILL.md"

OLD = """* **`WW_RENDER_SIZE` is clamped to one screen.** Asking for 1400x1400 on this
  machine gives **1507x1067**, and so does asking for 360x360. The hook says so
  itself near `src/nifskope_ui.cpp:21404`. Never quote a requested size as the
  picture's size -- read it back with PIL.
"""

NEW = """* **`WW_RENDER_SIZE` is clamped, and the two axes are clamped DIFFERENTLY.**
  Asking for 1400x1400 on this machine gave **1507x1067**, and so did asking for
  360x360. The hook says so itself near `src/nifskope_ui.cpp:21404`. Never quote
  a requested size as the picture's size -- read it back with PIL.
* **Measured again 2026-09-10 (lane BUILD10), and the width is NOT clamped up:**
  `1500x1000 -> 1500x941`, `1600x1000 -> 1600x941`, `1920x1080 -> 1920x1021`,
  `1507x1000 -> 1507x941`. The WIDTH is honoured exactly at least to 1920; the
  HEIGHT loses a constant **59 px** of window chrome. So a docstring that says a
  baseline was rendered "1500x1000" is naming the REQUEST, and asking for that
  again does not reproduce the file: the Charles flow baseline is 1507x941 and
  needs `WW_RENDER_SIZE=1507x1000`. **A "same framing" claim is a byte
  comparison in the script (`cmp` against the baseline PNG, refusing before it
  draws), never a sentence in a docstring.**
"""


def main():
    b = open(P, "rb").read()
    cr = b.count(b"\r")
    a = OLD.encode("utf-8")
    n = b.count(a)
    print("anchor count=%d CR=%d bytes=%d" % (n, cr, len(b)))
    assert n == 1
    out = b.replace(a, NEW.encode("utf-8"))
    assert out.count(b"\r") == cr
    open(P, "wb").write(out)
    print("wrote SKILL.md %d -> %d bytes, CR %d unchanged" % (len(b), len(out), cr))


if __name__ == "__main__":
    main()
