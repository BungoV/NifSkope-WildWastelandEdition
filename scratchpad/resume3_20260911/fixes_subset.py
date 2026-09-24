#!/usr/bin/env python3
"""RESUME3: apply the SUBSET of NIFPARSE1's 25 edits that the verdict supports.

NIFPARSE1's `fixes.py` is all-or-nothing by design, and its own recorded lesson
is "never the whole list on a guess". Rather than edit another lane's artefact,
this script imports its EDITS table and filters it BY FILE -- which is exactly
how that lane's fixes are grouped:

    src/message.cpp        F1  the worker-thread message-box guard
    src/gamemanager.{h,cpp} F2 the archive read/write lock
    src/data/nifvalue.cpp  F3  the lazy static-table re-init behind call_once
    src/model/nifmodel.cpp F4+F5 blockHashes read, and the pseudonym pointer bug
    src/xml/nifexpr.cpp    F6  four shared QRegularExpressions -> thread_local
    src/lodgenparallel.*   F7  the --chunk-threads memory cap and its census word

    python fixes_subset.py --files a,b,c            # check
    python fixes_subset.py --files a,b,c --apply

It re-runs the same anchor and CR assertions per file, and PRINTS THE EDITS IT
LEFT BEHIND by name, so a skipped fix is a stated refusal and never a silence.
"""
import sys, os, importlib.util

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(ROOT, 'scratchpad', 'nifparse1_20260911', 'fixes.py')

spec = importlib.util.spec_from_file_location('nifparse1_fixes', SRC)
mod = importlib.util.module_from_spec(spec)
sys.argv_backup = sys.argv
sys.argv = ['fixes.py']            # so its main() is not run on import
spec.loader.exec_module(mod)
sys.argv = sys.argv_backup

EDITS = mod.EDITS
NL = chr(10)


def main():
    argv = sys.argv[1:]
    apply = '--apply' in argv
    want = []
    if '--files' in argv:
        want = [s.strip() for s in argv[argv.index('--files') + 1].split(',') if s.strip()]
    if not want:
        print('REFUSED: --files is required; this script never applies the whole list')
        return 2

    picked = [e for e in EDITS if e[0] in want]
    skipped = [e for e in EDITS if e[0] not in want]
    files_wanted = sorted(set(e[0] for e in picked))
    missing = [w for w in want if w not in files_wanted]
    if missing:
        print('REFUSED: no edit in fixes.py touches ' + ', '.join(missing))
        return 2

    print('PICKED   %d of %d edits over %d file(s):' % (len(picked), len(EDITS), len(files_wanted)))
    for f in files_wanted:
        print('   %-26s %d edit(s)' % (f, sum(1 for e in picked if e[0] == f)))
    print('LEFT BEHIND, deliberately, %d edit(s) over:' % len(skipped))
    for f in sorted(set(e[0] for e in skipped)):
        print('   %-26s %d edit(s)' % (f, sum(1 for e in skipped if e[0] == f)))
    print()

    files = {}
    ok = True
    for path, mode, anchor, text in picked:
        full = os.path.join(ROOT, path)
        if path not in files:
            files[path] = open(full, 'rb').read()
        b = files[path]
        a = anchor.encode('utf-8')
        n = b.count(a)
        first = anchor.strip().split(NL)[0][:60]
        print('%-24s %-7s count=%d  %s' % (path, mode, n, first))
        if n != 1:
            ok = False
            print('   REFUSE: anchor must match exactly once, matched %d' % n)
            continue
        t = text.encode('utf-8')
        files[path] = b.replace(a, (a + t) if mode == 'after' else t, 1)

    for path in files:
        before = open(os.path.join(ROOT, path), 'rb').read()
        cr_b, cr_a = before.count(b'\r'), files[path].count(b'\r')
        print('%-24s CR before=%d after=%d  bytes %d -> %d'
              % (path, cr_b, cr_a, len(before), len(files[path])))
        if cr_b != cr_a:
            ok = False
            print('   REFUSE: line endings changed')

    if not ok:
        print(NL + 'RESULT REFUSED - nothing written')
        return 2
    if not apply:
        print(NL + 'RESULT CHECK OK - %d edits, all anchors matched once, nothing written' % len(picked))
        return 0
    for path, data in files.items():
        open(os.path.join(ROOT, path), 'wb').write(data)
        print('wrote ' + path)
    print(NL + 'RESULT APPLIED - %d edits' % len(picked))
    return 0


if __name__ == '__main__':
    sys.exit(main())
