#!/usr/bin/env python3
"""RESUME3: the three places that still say the fault is in the parser.

Comments and usage text are read by the next lane and by bungo, and all three
of these now state a cause this lane refuted with 10,240 loads. A wrong reason
left in the tree is how BAKEPERF1's single stack became a week-old "fact".

    python stale_comments.py --check
    python stale_comments.py --apply
"""
import sys, os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
NL = chr(10)
TAB = chr(9)

EDITS = [
    ('src/nifcli.cpp',
     TAB + TAB + "/* The CHUNK queue's own number, default 1. See lodgenparallel.h:" + NL +
     TAB + TAB + " * building NifModels on worker threads is not safe in this tree" + NL +
     TAB + TAB + " * yet, so the chunk fan-out is opt-in and the default bake is" + NL +
     TAB + TAB + " * the serial one that has always run. */" + NL,
     TAB + TAB + "/* The CHUNK queue's own number, default 1. See lodgenparallel.h." + NL +
     TAB + TAB + " * It is SAFE as of 2026-09-11 (lane RESUME3: 20 of 20 clean at 16" + NL +
     TAB + TAB + " * on each of two regions, byte-identical to the serial bake), and it" + NL +
     TAB + TAB + " * still defaults to 1 because it is SLOWER and much hungrier --" + NL +
     TAB + TAB + " * 2.3x the wall time on 9 chunks, 2.2x on 25, and 25 GB of peak" + NL +
     TAB + TAB + " * working set against 3.8. The default is a speed decision now, not" + NL +
     TAB + TAB + " * a safety one. */" + NL),

    ('src/lodgenparallel.cpp',
     'int g_chunkThreads = 1;                     //!< ONE until the parser is safe' + NL,
     'int g_chunkThreads = 1;                     //!< ONE: safe at 16, but slower' + NL),
]


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else '--check'
    files, ok = {}, True
    for path, old, new in EDITS:
        full = os.path.join(ROOT, path)
        if path not in files:
            files[path] = open(full, 'rb').read()
        cur = files[path]
        a = old.encode('utf-8')
        n = cur.count(a)
        print('%-24s replace count=%d  %s' % (path, n, old.strip().split(NL)[0][:56]))
        if n != 1:
            ok = False
            print('   REFUSE: matched %d times, not 1' % n)
            continue
        files[path] = cur.replace(a, new.encode('utf-8'), 1)
    for path in files:
        before = open(os.path.join(ROOT, path), 'rb').read()
        print('%-24s CR before=%d after=%d  bytes %d -> %d'
              % (path, before.count(b'\r'), files[path].count(b'\r'),
                 len(before), len(files[path])))
        if before.count(b'\r') != files[path].count(b'\r'):
            ok = False
            print('   REFUSE: line endings changed')
    if not ok:
        print(NL + 'RESULT REFUSED - nothing written')
        return 2
    if mode == '--apply':
        for path, data in files.items():
            open(os.path.join(ROOT, path), 'wb').write(data)
            print('wrote ' + path)
        print(NL + 'RESULT APPLIED - %d edits' % len(EDITS))
    else:
        print(NL + 'RESULT CHECK OK - %d edits, nothing written' % len(EDITS))
    return 0


if __name__ == '__main__':
    sys.exit(main())
