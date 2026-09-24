#!/usr/bin/env python
"""Lane BUILD11: apply lane HKXEDIT2's hook-up in TWO passes.

Why a driver at all. scratchpad/hkxedit2_20260910/hookup.py refuses:

    NifSkope.pro  after  NO anchor matches once:
        [('DEFINES += WW_HKXCLIP_CANON\\n', "'\\n'", 0), (... "'\\r\\n'", 0)]

Its TIER2 edit anchors on `DEFINES += WW_HKXCLIP_CANON`, which is the line its
own base edit #3 INSERTS. Both the check and the apply resolve every anchor
against the file as it is on disk BEFORE anything is written, so the tier-2
anchor can never count 1 in a single pass -- a chicken-and-egg defect in the
lane's own script, not in the tree. (PENDING.md predicted "16 anchors"; the
measured truth is 15 in pass one and the 16th only after pass one has landed.)

Nothing is retyped: EDITS, TIER2, DOCK_BLOCK and vocab_text are IMPORTED from
that script. This driver only splits the passes and keeps the same rules --
each anchor tried as LF then CRLF, the one that counts exactly 1 wins, the
inserted text must not already be present, and the CR count must move by
exactly the CRs the inserted text carries.

  python scratchpad/build11_20260910/hookup2_apply.py           # check both passes
  python scratchpad/build11_20260910/hookup2_apply.py --apply
"""
import sys, os, importlib.util

REPO = r"E:\Projects\NifskopeWildWastelandEdition"
SRC = os.path.join(REPO, "scratchpad", "hkxedit2_20260910", "hookup.py")

spec = importlib.util.spec_from_file_location("hkxedit2_hookup", SRC)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def resolve(edits, label):
    """Return [(full, mode, anchor_bytes, text_bytes)] or None if any anchor refuses."""
    plan, ok = [], True
    for path, mode, anchors, text in edits:
        full = os.path.join(REPO, path)
        b = open(full, "rb").read()
        chosen = None
        counts = []
        for anchor in anchors:
            for eol in ("\n", "\r\n"):
                a = anchor.replace("\n", eol).encode("utf-8")
                n = b.count(a)
                counts.append((anchor[:36], repr(eol), n))
                if n == 1 and chosen is None:
                    chosen = (a, eol, anchor)
        if chosen is None:
            print("  %-20s %-7s REFUSED %s" % (path, mode, counts))
            ok = False
            continue
        a, eol, anchor = chosen
        t = text if text is not None else mod.vocab_text(anchor)
        t = t.replace("\n", eol).encode("utf-8")
        already = b.count(t)
        print("  %-20s %-7s count=1 eol=%-6s text_CR=%-3d already=%d" %
              (path, mode, repr(eol), t.count(b"\r"), already))
        if already:
            ok = False
        plan.append((full, mode, a, t))
    return plan if ok else None


def write(plan):
    byfile = {}
    for full, mode, a, t in plan:
        byfile.setdefault(full, []).append((mode, a, t))
    for full, items in byfile.items():
        b = open(full, "rb").read()
        cr0, n0 = b.count(b"\r"), len(b)
        dcr = 0
        for mode, a, t in items:
            assert b.count(a) == 1, "anchor moved: %r" % a[:40]
            if mode == "after":
                b = b.replace(a, a + t, 1)
                dcr += t.count(b"\r")
            else:
                b = b.replace(a, t, 1)
                dcr += t.count(b"\r") - a.count(b"\r")
        assert b.count(b"\r") == cr0 + dcr, \
            "CR moved by %d, expected %d in %s" % (b.count(b"\r") - cr0, dcr, full)
        open(full, "wb").write(b)
        print("  APPLIED %-22s %d edit(s)  bytes %d -> %d (+%d)  CR %d -> %d (+%d)"
              % (os.path.relpath(full, REPO), len(items), n0, len(b), len(b) - n0,
                 cr0, b.count(b"\r"), dcr))


def main():
    apply = "--apply" in sys.argv

    pro = open(os.path.join(REPO, "NifSkope.pro"), "rb").read()
    if b"src/hkxfile.cpp" not in pro:
        print("REFUSED: src/hkxfile.cpp is not in NifSkope.pro -- lane HKXEDIT1's hook-up goes first")
        return 2
    nh = open(os.path.join(REPO, "src", "nifskope.h"), "rb").read()
    tier2 = b"HkxModel * hkx;" in nh
    print("HKXEDIT1 applied: yes (src/hkxfile.cpp in the .pro)")
    print("tier 2 (WW_ANIMWS_HKXMODEL): %s" % ("yes" if tier2 else "NO -- HkxModel not in the window"))

    print("PASS 1 -- the %d base edits" % len(mod.EDITS))
    p1 = resolve(mod.EDITS, "base")
    if p1 is None:
        print("REFUSED in pass 1; nothing written")
        return 2
    if not apply:
        print("PASS 2 -- the %d tier-2 edit(s): its anchor is created by pass 1, so it"
              % len(mod.TIER2))
        print("          cannot be checked before pass 1 lands (that is the whole point)")
        print("CHECK ONLY: %d edits resolve; nothing written" % len(p1))
        return 0
    write(p1)

    if not tier2:
        print("PASS 2 skipped: tier 2 not enabled")
        return 0
    print("PASS 2 -- the %d tier-2 edit(s)" % len(mod.TIER2))
    p2 = resolve(mod.TIER2, "tier2")
    if p2 is None:
        print("REFUSED in pass 2 -- pass 1 IS APPLIED; fix and finish tier 2 by hand")
        return 3
    write(p2)
    print("done. Next: qmake BEFORE make (new HEADERS/SOURCES and two DEFINES),")
    print("then delete the objects of every TU that reads the new macros.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
