#!/usr/bin/env python
"""Lane BUILD11: the three includes the hook-ups needed and did not carry.

`bash sx_BUILD11.sh src/nifskope.cpp src/nifskope_ui.cpp` after the three
hook-ups landed:

  src/nifskope.cpp:7807  comparison between distinct pointer types 'QObject*'
                         and 'AnimWorkspace*' lacks a cast
  src/nifskope.cpp:7808  invalid use of incomplete type 'class AnimWorkspace'
  src/nifskope_ui.cpp:23699 / :23704  invalid use of incomplete type 'class QUndoGroup'
  src/nifskope_ui.cpp:24298           invalid use of incomplete type 'class HkxModel'

Each is the same defect: a hook-up inserted a CALL into an existing file and
no INCLUDE beside it, so the type is only the forward declaration the header
carries. Three lines, no behaviour, marked "(lane BUILD11)".

Line endings: src/nifskope.cpp is mixed and its include block is CRLF (lane
HKXEDIT1's own include edit matched there with \\r\\n); src/nifskope_ui.cpp is
LF-only. Both asserted by byte count, before and after.
"""
import sys, os

REPO = r"E:\Projects\NifskopeWildWastelandEdition"

EDITS = [
    # (file, anchor, text) -- anchors carry the file's real line ending
    ("src/nifskope.cpp",
     '#include "hkxmodel.h"\t// lane HKXEDIT1\r\n',
     '#include "animworkspace.h"\t// (lane BUILD11) HKXEDIT2\'s select() edit calls animws->setCurrentIndex; nifskope.h only forward-declares it\r\n'),
    ("src/nifskope_ui.cpp",
     '#include <QUndoStack>\n',
     '#include <QUndoGroup>\t// (lane BUILD11) HKXEDIT2 creates the Undo/Redo actions from wwAnimUndoGroup()\n'),
    ("src/nifskope_ui.cpp",
     '#include "animworkspace.h"\t\t// lane HKXEDIT2\n',
     '#include "hkxmodel.h"\t\t// (lane BUILD11) the WW_ANIMWS_HKXMODEL branch reads hkx->undoStack\n'),
]


def main():
    apply = "--apply" in sys.argv
    byfile, ok = {}, True
    for path, anchor, text in EDITS:
        full = os.path.join(REPO, path)
        b = open(full, "rb").read()
        a, t = anchor.encode(), text.encode()
        n, already = b.count(a), b.count(t)
        print("%-20s anchor=%d already=%d  text_CR=%d  %r" %
              (path, n, already, t.count(b"\r"), anchor[:44]))
        if n != 1 or already:
            ok = False
        byfile.setdefault(full, []).append((a, t))
    if not ok:
        print("REFUSED; nothing written")
        return 2
    if not apply:
        print("--check only; nothing written")
        return 0
    for full, items in byfile.items():
        b = open(full, "rb").read()
        cr0, n0 = b.count(b"\r"), len(b)
        dcr = 0
        for a, t in items:
            assert b.count(a) == 1
            b = b.replace(a, a + t, 1)
            dcr += t.count(b"\r")
        assert b.count(b"\r") == cr0 + dcr, "CR moved by %d, expected %d" % (b.count(b"\r") - cr0, dcr)
        open(full, "wb").write(b)
        print("wrote %-24s bytes %d -> %d  CR %d -> %d (+%d)" %
              (os.path.relpath(full, REPO), n0, len(b), cr0, b.count(b"\r"), dcr))
    return 0


if __name__ == "__main__":
    sys.exit(main())
