#!/usr/bin/env python
"""Lane BUILD11: finish lane HKXEDIT1's hook-up.

scratchpad/hkxedit1_20260910/hookup.py --apply wrote NifSkope.pro and
src/nifskope.h and then died on src/nifskope.cpp with

    AssertionError: CR count moved in ...\\src/nifskope.cpp

which is the script's own defect, not the file's: nifskope.cpp is mixed and
the four regions it edits are CRLF, so the script converts the inserted text
to CRLF and the CR count MUST grow by exactly the CRs of that text
(ww-anchored-hookup section 1: "assert the CR count moves by exactly the CRs
of the inserted text"), not stay equal.

This finisher takes the EDITS table from that script BY IMPORT -- nothing is
retyped -- and applies only the two files it never wrote.

  python scratchpad/build11_20260910/hookup1_rest.py           # check
  python scratchpad/build11_20260910/hookup1_rest.py --apply
"""
import sys, os, importlib.util

REPO = r"E:\Projects\NifskopeWildWastelandEdition"
SRC = os.path.join(REPO, "scratchpad", "hkxedit1_20260910", "hookup.py")

spec = importlib.util.spec_from_file_location("hkxedit1_hookup", SRC)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

REMAINING = {"src/nifskope.cpp", "src/nifskope_ui.cpp"}
DONE_ALREADY = {"NifSkope.pro", "src/nifskope.h"}


def main():
    apply = "--apply" in sys.argv

    # 1. prove the two already-written files really carry the text
    for path in sorted(DONE_ALREADY):
        b = open(os.path.join(REPO, path), "rb").read()
        n = sum(b.count(t.encode("utf-8")) for p, m, a, t in mod.EDITS if p == path)
        print("ALREADY %-18s inserted-text occurrences = %d (expect %d)"
              % (path, n, sum(1 for p, m, a, t in mod.EDITS if p == path)))

    # 2. plan the two remaining files
    byfile = {}
    ok = True
    for path, mode, anchor, text in mod.EDITS:
        if path not in REMAINING:
            continue
        full = os.path.join(REPO, path)
        if path in mod.CRLF_FILES:
            anchor = anchor.replace("\n", "\r\n")
            text = text.replace("\n", "\r\n")
        a = anchor.encode("utf-8")
        t = text.encode("utf-8")
        b = open(full, "rb").read()
        n = b.count(a)
        already = b.count(t)
        print("%-20s %-7s anchor_count=%d inserted_already=%d text_CR=%d"
              % (path, mode, n, already, t.count(b"\r")))
        if n != 1 or already != 0:
            ok = False
        byfile.setdefault(full, []).append((mode, a, t))

    if not ok:
        print("REFUSED: an anchor does not match exactly once, or the text is already in")
        return 2
    if not apply:
        print("--check only; nothing written")
        return 0

    for full, edits in byfile.items():
        b = open(full, "rb").read()
        cr0, lf0, n0 = b.count(b"\r"), b.count(b"\n"), len(b)
        dcr = 0
        for mode, a, t in edits:
            assert b.count(a) == 1, full
            b = b.replace(a, a + t) if mode == "after" else b.replace(a, t)
            dcr += t.count(b"\r")
        assert b.count(b"\r") == cr0 + dcr, \
            "CR moved by %d, expected %d in %s" % (b.count(b"\r") - cr0, dcr, full)
        open(full, "wb").write(b)
        print("wrote %s  bytes %d -> %d (+%d)  CR %d -> %d (+%d, expected +%d)  LF %d -> %d"
              % (full, n0, len(b), len(b) - n0, cr0, b.count(b"\r"),
                 b.count(b"\r") - cr0, dcr, lf0, b.count(b"\n")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
