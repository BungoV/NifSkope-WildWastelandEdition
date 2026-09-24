#!/usr/bin/env python3
"""RESUME3, step 3: replace the DIAGNOSTIC comment left by mutex_off.py with the
verdict, now that the verdict exists.

BAKEPERF1's process-wide parse lock is gone, and it is gone on a measurement:
the model layer survived 10,240 loads on 16 threads with 0 digest mismatches
and 0 faults over 20 consecutive runs (`parsestress`), while four symbolised
faults in the real bake all sat under the CLI's unlocked message handler. The
lock was containment for a fault that was never in the parser.

    python mutex_final.py --check
    python mutex_final.py --apply
"""
import sys, os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PATH = os.path.join(ROOT, 'src', 'lodgen.cpp')

OLD = (
b"\t/* RESUME3 DIAGNOSTIC, 2026-09-11: BAKEPERF1's process-wide parse lock is\n"
b"\t * OFF here so the original fault can be photographed with symbols on.\n"
b"\t * This comment is rewritten by mutex_final.py once the verdict is in. */\n"
)

NEW = (
b"\t/* NIF PARSING IS NOT SERIALISED HERE ANY MORE (lane RESUME3, 2026-09-11).\n"
b"\t *\n"
b"\t * BAKEPERF1 put a process-wide mutex around the whole life of this\n"
b"\t * temporary document, on a single stack taken inside a bake where five\n"
b"\t * subsystems were live at once, and called it a containment rather than a\n"
b"\t * fix. It was containment for a fault that is not in the parser.\n"
b"\t *\n"
b"\t * The model layer on its own -- `NifSkope -no-gui parsestress`, 16 threads,\n"
b"\t * 8 reps, 4 fixtures, 20 consecutive runs -- did 10,240 loads with 0 digest\n"
b"\t * mismatches and 0 faults, with both of its sabotage floors seen to go red\n"
b"\t * first. The real bake's four symbolised faults were all under\n"
b"\t * `cliMessageHandler`, which wrote through an unlocked shared QTextStream\n"
b"\t * from every worker at once (src/nifcli.cpp, fixed there). The cache above\n"
b"\t * still means each model is parsed once per chunk.\n"
b"\t *\n"
b"\t * The way back, exact: `--chunk-threads 1`, the shipped default. */\n"
)


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else '--check'
    b = open(PATH, 'rb').read()
    cr0 = b.count(b'\r')
    c = b.count(OLD)
    print('src/lodgen.cpp   replace count=%d   (the diagnostic comment)' % c)
    if c != 1:
        print('RESULT REFUSED - the anchor matched %d times, not 1' % c)
        return 3
    nb = b.replace(OLD, NEW)
    print('src/lodgen.cpp   CR before=%d after=%d   LF %d -> %d'
          % (cr0, nb.count(b'\r'), b.count(b'\n'), nb.count(b'\n')))
    if nb.count(b'\r') != cr0:
        print('RESULT REFUSED - CR count moved')
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
