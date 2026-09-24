#!/usr/bin/env python
"""Lane BUILD11: splice lane HKXEDIT1's and lane SKELFIX's WW_CHANGES.md entries
in at the top, under the title line, and rewrite HKXEDIT2's BUILD PENDING title.

WW_CHANGES.md is MIXED and stays so; the 2026-09 entries at the top are LF-only.
The CR count is 19,020 and must be UNCHANGED -- asserted, in binary, before and
after (CONSTITUTION 8; the file is never normalised and never rewritten by a
text-mode write).

The two entry files open with an HTML comment addressed to the director; it is
dropped, as is each file's own PREDICTION status block, which this script
replaces with the measured build result read from BUILD_STATUS below.

  python scratchpad/build11_20260910/splice_changes.py           # check
  python scratchpad/build11_20260910/splice_changes.py --apply
"""
import sys, os, io

REPO = r"E:\Projects\NifskopeWildWastelandEdition"
CHANGES = os.path.join(REPO, "WW_CHANGES.md")
TITLE = "# NifSkope \u2014 Wild Wasteland Edition: Change Log\n"
CR_EXPECTED = 19020

E1 = os.path.join(REPO, "scratchpad", "hkxedit1_20260910", "WW_CHANGES_ENTRY.md")
SF = os.path.join(REPO, "scratchpad", "skelfix_20260910", "WW_CHANGES_ENTRY.md")

# the measured block that replaces each entry's own prediction / BUILD PENDING text
STATUS_PATH = os.path.join(REPO, "scratchpad", "build11_20260910", "status_block.md")


def strip_leading_comment(text):
    if text.lstrip().startswith("<!--"):
        i = text.index("-->") + 3
        text = text[i:]
    return text.lstrip("\n")


def main():
    apply = "--apply" in sys.argv
    b = open(CHANGES, "rb").read()
    cr0, n0 = b.count(b"\r"), len(b)
    print("WW_CHANGES.md bytes=%d CR=%d (expected %d)" % (n0, cr0, CR_EXPECTED))
    assert cr0 == CR_EXPECTED, "CR count is not the expected %d" % CR_EXPECTED
    title = TITLE.encode("utf-8")
    assert b.count(title) == 1, "the title line does not match exactly once"

    status = open(STATUS_PATH, encoding="utf-8").read()
    assert "\r" not in status

    parts = []
    for path, name in ((E1, "HKXEDIT1"), (SF, "SKELFIX")):
        t = open(path, encoding="utf-8").read().replace("\r\n", "\n")
        t = strip_leading_comment(t)
        assert ("lane %s" % name) in t or name in t
        parts.append(t.rstrip("\n") + "\n\n" + status.rstrip("\n") + "\n\n")
    add = "".join(parts).encode("utf-8")
    assert add.count(b"\r") == 0, "the spliced text must be LF-only"
    for name in ("lane HKXEDIT1", "lane SKELFIX"):
        assert name.encode() not in b, "%s already spliced" % name

    nb = b.replace(title, title + b"\n" + add, 1)
    print("would grow by %d bytes; CR %d -> %d" % (len(nb) - n0, cr0, nb.count(b"\r")))
    assert nb.count(b"\r") == cr0, "CR count moved"
    if not apply:
        print("--check only; nothing written")
        return 0
    with open(CHANGES, "wb") as f:
        f.write(nb)
    c = open(CHANGES, "rb").read()
    print("wrote WW_CHANGES.md bytes %d -> %d  CR %d (unchanged)  LF %d -> %d"
          % (n0, len(c), c.count(b"\r"), b.count(b"\n"), c.count(b"\n")))
    assert c.count(b"\r") == CR_EXPECTED
    return 0


if __name__ == "__main__":
    sys.exit(main())
