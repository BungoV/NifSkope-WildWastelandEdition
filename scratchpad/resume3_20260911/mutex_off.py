#!/usr/bin/env python3
"""RESUME3, step 2: turn the LODGEN PARSE LOCK off, so gate R2 can photograph
the ORIGINAL fault.

BAKEPERF1 put a process-wide mutex around the whole life of the temporary
`NifModel` in `lodgenLoadModel` (`src/lodgen.cpp`).  With it in, a 16-worker
bake does not fault, so the stack that names the fault cannot be taken.  It is a
containment its own comment disowns ("It is a containment, not a fix"), and the
verdict this lane is about to reach either replaces it with a real fix or keeps
it -- either way the diagnostic run needs it OFF.

A refusing script, not a hand edit (`ww-anchored-hookup`): every anchor carries
the file's real line ending and its real leading TABS, is asserted to match
exactly once, and the CR/LF/byte accounting is printed before and after.

    python mutex_off.py --check     # writes nothing
    python mutex_off.py --apply
"""
import sys, os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PATH = os.path.join(ROOT, 'src', 'lodgen.cpp')

# read out of the file's own bytes, tabs included; never retyped from a screen
OLD = (
b"\t/* NIF PARSING IS SERIALISED (lane BAKEPERF1, 2026-09-11).\n"
b"\t *\n"
b"\t * The NifModel / NifItem / nif.xml layer is not safe for two threads\n"
b"\t * parsing two documents at once in this tree: with the chunk queue fanned\n"
b"\t * out, five runs out of five faulted inside `NifItem::deleteChildItems()`\n"
b"\t * under `BaseModel::~BaseModel()` here, while every other worker sat in\n"
b"\t * `BaseModel::getItemInternal` / `NifExpr::partition` / `NifModel::get<>`.\n"
b"\t * The lock covers the WHOLE life of the temporary document, construction\n"
b"\t * to destruction, because the fault was in the destructor.\n"
b"\t *\n"
b"\t * Uncontended and free when the queue runs one chunk at a time, which is\n"
b"\t * the default (`lodgenChunkThreadCount()`). It is a containment, not a\n"
b"\t * fix: the layer itself is still unsafe and the cache above means each\n"
b"\t * model is parsed once per chunk regardless. */\n"
b"\tstatic QMutex parseMutex;\n"
b"\tQMutexLocker parseLock( &parseMutex );\n"
)

NEW = (
b"\t/* RESUME3 DIAGNOSTIC, 2026-09-11: BAKEPERF1's process-wide parse lock is\n"
b"\t * OFF here so the original fault can be photographed with symbols on.\n"
b"\t * This comment is rewritten by mutex_final.py once the verdict is in. */\n"
)


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else '--check'
    b = open(PATH, 'rb').read()
    cr0, lf0, n0 = b.count(b'\r'), b.count(b'\n'), len(b)
    c = b.count(OLD)
    print('src/lodgen.cpp   replace count=%d   (the parse lock, %d lines)'
          % (c, OLD.count(b'\n')))
    print('src/lodgen.cpp   CR before=%d  LF before=%d  bytes before=%d' % (cr0, lf0, n0))
    if c != 1:
        print('RESULT REFUSED - the anchor matched %d times, not 1. '
              'The file moved; re-derive the anchor from its own bytes.' % c)
        return 3
    nb = b.replace(OLD, NEW)
    cr1, lf1, n1 = nb.count(b'\r'), nb.count(b'\n'), len(nb)
    print('src/lodgen.cpp   CR after =%d  LF after =%d  bytes after =%d  (LF %+d)'
          % (cr1, lf1, n1, lf1 - lf0))
    if cr1 != cr0:
        print('RESULT REFUSED - CR count moved %d -> %d' % (cr0, cr1))
        return 4
    if mode == '--apply':
        open(PATH, 'wb').write(nb)
        print('wrote src/lodgen.cpp')
        print('RESULT APPLIED - 1 edit')
    else:
        print('RESULT CHECK OK - 1 edit, anchor matched once, nothing written')
    return 0


if __name__ == '__main__':
    sys.exit(main())
