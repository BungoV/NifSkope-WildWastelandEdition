#!/usr/bin/env python3
"""Lane BUILD9, 2026-09-10.

WW_LOADEDNIFS_TEST (src/nifskope_ui.cpp) asserts the EXPECTED TEXT of six
labels that lane FILESTAB renamed.  Six of its checks went red the moment the
renames landed -- not because the sibling stopped working, but because the
harness still quotes the old words.  Only the expected LITERALS move here; no
check name, no assertion and no widget is touched, so the log lines keep their
old names and a before/after comparison still lines up.

Default is --check (writes nothing).  --apply writes.
"""
import sys

PATH = "src/nifskope_ui.cpp"

# (expected count, old, new)
EDITS = [
    (1, 'tabText( 2 ) == QStringLiteral( "NIFs" )',
        'tabText( 2 ) == QStringLiteral( "Files" )'),
    (1, '== QStringLiteral( "Loaded NIFs · 2" )',
        '== QStringLiteral( "Loaded files · 2" )'),
    (1, '0, Qt::Horizontal ).toString() == QStringLiteral( "Loaded NIFs · 1 of 2" )',
        '0, Qt::Horizontal ).toString() == QStringLiteral( "Loaded files · 1 of 2" )'),
    (1, '"Use as the skeleton for Loaded NIFs — only one at a time"',
        '"Use as the skeleton for Loaded files — only one at a time"'),
    (1, '"The skeleton for Loaded NIFs — click to unmark it"',
        '"The skeleton for Loaded files — click to unmark it"'),
    (1, 'QStringLiteral( "Use as Skeleton for Loaded NIFs" )',
        'QStringLiteral( "Use as Skeleton for Loaded files" )'),
]


def main():
    apply = "--apply" in sys.argv
    raw = open(PATH, "rb").read()
    cr0, lf0, n0 = raw.count(b"\r"), raw.count(b"\n"), len(raw)
    text = raw.decode("utf-8")
    bad = 0
    for want, old, new in EDITS:
        got = text.count(old)
        print("  %-6s found %d  %s" % ("x%d" % want, got, old[:70]))
        if got != want:
            bad += 1
            continue
        text = text.replace(old, new)
    if bad:
        print("REFUSED: %d anchor(s) did not match as declared. Nothing written." % bad)
        return 1
    out = text.encode("utf-8")
    print("%s bytes %d -> %d   CR %d -> %d   LF %d -> %d"
          % (PATH, n0, len(out), cr0, out.count(b"\r"), lf0, out.count(b"\n")))
    if out.count(b"\r") != cr0 or out.count(b"\n") != lf0:
        print("REFUSED: line endings moved. Nothing written.")
        return 1
    if not apply:
        print("--check: every anchor matched as declared. Nothing written.")
        return 0
    open(PATH, "wb").write(out)
    print("--apply: written.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
