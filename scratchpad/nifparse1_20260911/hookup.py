#!/usr/bin/env python3
"""Lane NIFPARSE1's cross-file hook-up, as a REFUSING script (ww-anchored-hookup).

The lane's own code is in NEW files (src/nifparsestress.{h,cpp}) plus files it
owns outright (src/model, src/xml, src/data, src/lodgenchunkpass, src/lodgenparallel).
The few lines that EXISTING files need so the new ones join the build and get a
way to be run live here, because `NifSkope.pro` belongs to lane CARDS-AGG while
it is alive and `src/nifcli.cpp` is shared.

    python hookup.py            # --check: writes NOTHING, prints every count
    python hookup.py --apply    # refuses unless every anchor still matches ONCE

Every file below is LF-only (measured: NifSkope.pro CR 0, src/nifcli.cpp CR 0),
so every anchor and every inserted line carries "\\n" and the CR count is
asserted unchanged on both sides.
"""

import sys, os

ROOT = r"E:\Projects\NifskopeWildWastelandEdition"

NL = chr(10)

# ---------------------------------------------------------------- the table
# ( path, "after" | "replace", anchor, text )
EDITS = [
    # 1. the two new sources join the project
    ("NifSkope.pro", "after",
     "\tsrc/lodgenparallel.h \\" + NL,
     "\tsrc/nifparsestress.h \\" + NL),

    ("NifSkope.pro", "after",
     "\tsrc/lodgenparallel.cpp \\" + NL,
     "\tsrc/nifparsestress.cpp \\" + NL),

    # 2. the CLI can reach the harness
    ("src/nifcli.cpp", "after",
     '#include "lodgenparallel.h"' + NL,
     '#include "nifparsestress.h"' + NL),

    # 3. its options. The anchor is the whole --chunk-threads line plus the
    #    comment above it, which occurs once; the bare option string occurs in
    #    the usage block too.
    ("src/nifcli.cpp", "after",
     "\t\telse if ( t == QLatin1String( \"--chunk-threads\" ) ) lodgenSetChunkThreadCount( next().toInt() );" + NL,
     "\t\t/* THE MODEL LAYER ON N THREADS, and nothing else in the picture" + NL
     + "\t\t * (lane NIFPARSE1, see src/nifparsestress.h). A bake crash cannot" + NL
     + "\t\t * tell the parser apart from the plugin reader, the texture cache," + NL
     + "\t\t * the archive layer and the message sink; this can. */" + NL
     + "\t\telse if ( t == QLatin1String( \"--stress-file\" ) ) stressFiles << next();" + NL
     + "\t\telse if ( t == QLatin1String( \"--stress-threads\" ) ) stressThreads = next().toInt();" + NL
     + "\t\telse if ( t == QLatin1String( \"--stress-reps\" ) ) stressReps = next().toInt();" + NL
     + "\t\telse if ( t == QLatin1String( \"--stress-sabotage\" ) ) stressSabotage = next();" + NL),

    # 4. their declarations, beside the other lodgen locals
    ("src/nifcli.cpp", "after",
     "\tQStringList lgResources;" + NL,
     "\tQStringList stressFiles;          // lane NIFPARSE1" + NL
     + "\tint stressThreads = 16;" + NL
     + "\tint stressReps = 8;" + NL
     + "\tQString stressSabotage;" + NL),

    # 5. the dispatch. A "replace" that REPEATS its own anchor, so the edit is
    #    visibly additive and the existing 'lodgen' branch is untouched. An
    #    "after" on the retired-'lodt' branch was tried first and was WRONG: it
    #    would have inserted between that branch's `{` and its body, leaving the
    #    lodt error on an `else if ( false )`.
    ("src/nifcli.cpp", "replace",
     "\telse if ( cmd == QLatin1String( \"lodgen\" ) )" + NL
     + "\t\trc = cmdLodgen( file, lgListWorldspaces, lgWorldspace," + NL,
     "\t/* THE MODEL LAYER ON N THREADS -- see src/nifparsestress.h and edit 3." + NL
     + "\t * The positional <file> is the first NIF; --stress-file adds more. */" + NL
     + "\telse if ( cmd == QLatin1String( \"parsestress\" ) ) {" + NL
     + "\t\tNifParseStressOptions so;" + NL
     + "\t\tif ( !file.isEmpty() )" + NL
     + "\t\t\tso.paths << file;" + NL
     + "\t\tso.paths << stressFiles;" + NL
     + "\t\tso.threads = stressThreads;" + NL
     + "\t\tso.reps = stressReps;" + NL
     + "\t\tso.sabotage = stressSabotage;" + NL
     + "\t\trc = nifParseStressRun( so, out() ) ? 1 : 0;" + NL
     + "\t}" + NL
     + "\telse if ( cmd == QLatin1String( \"lodgen\" ) )" + NL
     + "\t\trc = cmdLodgen( file, lgListWorldspaces, lgWorldspace," + NL),

    # 6. the usage line
    ("src/nifcli.cpp", "after",
     "\t\t  << \"  lodgen <file.esm> --list-worldspaces    LOD generation (rung 0): list\\n\"" + NL,
     "\t\t  << \"  parsestress <a.nif> [--stress-file b.nif]... [--stress-threads N]\\n\"" + NL
     + "\t\t  << \"              [--stress-reps N] [--stress-sabotage digest|share]\\n\"" + NL),
]


def main():
    apply = "--apply" in sys.argv
    files = {}
    ok = True
    for path, mode, anchor, text in EDITS:
        full = os.path.join(ROOT, path)
        if path not in files:
            with open(full, "rb") as f:
                files[path] = f.read()
        b = files[path]
        a = anchor.encode("utf-8")
        n = b.count(a)
        print("%-22s %-7s count=%d  %s" % (path, mode, n, anchor.strip()[:64].replace(NL, " ")))
        if n != 1:
            ok = False
            print("   REFUSE: anchor must match exactly once, matched %d" % n)
            continue
        t = text.encode("utf-8")
        if mode == "after":
            files[path] = b.replace(a, a + t, 1)
        else:
            files[path] = b.replace(a, t, 1)

    # CR assert: every one of these files is LF-only and must stay so
    for path in files:
        with open(os.path.join(ROOT, path), "rb") as f:
            before = f.read()
        cr_b, cr_a = before.count(b"\r"), files[path].count(b"\r")
        print("%-22s CR before=%d after=%d" % (path, cr_b, cr_a))
        if cr_b != cr_a:
            ok = False
            print("   REFUSE: line endings changed")

    if not ok:
        print(NL + "RESULT REFUSED - nothing written")
        return 2
    if not apply:
        print(NL + "RESULT CHECK OK - %d anchors, all matched once, nothing written" % len(EDITS))
        return 0
    for path, data in files.items():
        with open(os.path.join(ROOT, path), "wb") as f:
            f.write(data)
        print("wrote " + path)
    print(NL + "RESULT APPLIED - %d edits" % len(EDITS))
    return 0


if __name__ == "__main__":
    sys.exit(main())
