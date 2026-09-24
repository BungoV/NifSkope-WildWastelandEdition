#!/usr/bin/env python3
"""Lane NATIVEVIEW1's hook-up: route `.lodi` to a built document and let a
`.lodl` document carry a `.lodi`'s objects, plus the two new sources in the
project file.

Anchored, refusing, and byte-exact: every anchor is read out of the FILE, must
match EXACTLY ONCE, and carries its own line ending. `--check` (the default)
writes nothing and reports what would change. CR/LF counts are printed before
and after so a mixed-ending file (src/nifskope.cpp is one) cannot be silently
normalised.
"""

import sys
import os
import re

ROOT = "E:/Projects/NifskopeWildWastelandEdition"

# ---------------------------------------------------------------- the edits

PRO_H = "\tsrc/lodifile.h \\\n"
PRO_H_ADD = "\tsrc/lodinative.h \\\n\tsrc/lodtsheets.h \\\n"
PRO_C = "\tsrc/lodifile.cpp \\\n"
PRO_C_ADD = "\tsrc/lodinative.cpp \\\n\tsrc/lodtsheets.cpp \\\n"

INCLUDE_ANCHOR = '#include "hkxplayback.h"\n'
INCLUDE_ADD = (
    '#include "lodinative.h"\t// lane NATIVEVIEW1: the .lodi/.lodo built document\n'
)

FILETYPE_ANCHOR = '\t{ "Landscape Terrain", "lodl" },\n'
FILETYPE_ADD = (
    "\t// nor is the native object LOD: a .lodi is a PLACEMENT table and its\n"
    "\t// geometry is in the .lodo beside it, so it is built here too (lodinative.cpp)\n"
    '\t{ "Native Object LOD", "lodi" },\n'
)

LODL_KEEP_ANCHOR = (
    "\t\t\tloaded = nifCreateLodtTerrainScene( nif, fname, spec, &terr, &notes );\n"
    "\t\t\tif ( loaded )\n"
    "\t\t\t\tlodtPendingRegion = spec;\n"
)
LODL_KEEP_ADD = """
\t\t\t/* One document carrying both halves of the native bake: the terrain
\t\t\t * from this `.lodl` and the objects from a `.lodi`, which is what a
\t\t\t * picture of the region needs. The terrain builder owns createNew(),
\t\t\t * so the objects go under the root it has just made. Unset, none of
\t\t\t * this runs and the document is exactly the terrain document. */
\t\t\tconst QString lodlObjects = qEnvironmentVariable( "WW_LODL_OBJECTS" );
\t\t\tif ( loaded && !lodlObjects.isEmpty() ) {
\t\t\t\tLodiSceneSpec ospec;
\t\t\t\tlodiSpecFromEnv( ospec );
\t\t\t\tif ( !ospec.haveRegion ) {
\t\t\t\t\t// the terrain's own region, so the two halves cannot disagree
\t\t\t\t\tospec.x0 = spec.x0;
\t\t\t\t\tospec.y0 = spec.y0;
\t\t\t\t\tospec.x1 = spec.x1;
\t\t\t\t\tospec.y1 = spec.y1;
\t\t\t\t\tospec.haveRegion = true;
\t\t\t\t}
\t\t\t\tQString oerr, onotes;
\t\t\t\tnif->holdUpdates( true );
\t\t\t\tconst bool ook = nifAppendLodiObjects( nif, nif->getBlockIndex( 0 ),
\t\t\t\t\tlodlObjects, ospec, &oerr, &onotes );
\t\t\t\tnif->holdUpdates( false );
\t\t\t\tnif->updateModel();
\t\t\t\tif ( ook && !onotes.isEmpty() )
\t\t\t\t\tqInfo().noquote() << "lodl objects:\\n" << onotes;
\t\t\t\tif ( !ook )
\t\t\t\t\tqWarning() << "lodl objects:" << oerr;
\t\t\t}
"""

LOADFILE_ANCHOR = (
    "\t\tif ( !loaded && !terr.isEmpty() )\n"
    '\t\t\tqWarning() << "lodt terrain:" << terr;\n'
    "\t} else {\n"
    "\t\tloaded = nif->loadFromFile( fname );\n"
)
LOADFILE_ADD = (
    "\t\tif ( !loaded && !terr.isEmpty() )\n"
    '\t\t\tqWarning() << "lodt terrain:" << terr;\n'
    '\t} else if ( f.suffix().compare( QLatin1String( "lodi" ), Qt::CaseInsensitive ) == 0 ) {\n'
    "\t\t/* Native object LOD: the placements are in this file, the geometry is\n"
    "\t\t * in the `.lodo` beside it, and the scene is BUILT out of the two the\n"
    "\t\t * same way the landscape routes above build theirs. The notes say how\n"
    "\t\t * many placements were read and how many were drawn, so a picture is\n"
    "\t\t * never the only evidence that the file was understood. */\n"
    "\t\tQString oerr, onotes;\n"
    "\t\tLodiSceneSpec ospec;\n"
    "\t\tlodiSpecFromEnv( ospec );\n"
    "\t\tloaded = nifCreateLodiObjectScene( nif, fname, ospec, &oerr, &onotes );\n"
    "\t\tif ( loaded && !onotes.isEmpty() )\n"
    '\t\t\tqInfo().noquote() << "lodi objects:\\n" << onotes;\n'
    "\t\tif ( !loaded && !oerr.isEmpty() )\n"
    '\t\t\tqWarning() << "lodi objects:" << oerr;\n'
    "\t} else {\n"
    "\t\tloaded = nif->loadFromFile( fname );\n"
)

UNDO_ANCHOR = (
    '\t\t\t|| f.suffix().compare( QLatin1String( "lodl" ), Qt::CaseInsensitive ) == 0 ) ) {\n'
)
UNDO_ADD = (
    '\t\t\t|| f.suffix().compare( QLatin1String( "lodl" ), Qt::CaseInsensitive ) == 0\n'
    '\t\t\t|| f.suffix().compare( QLatin1String( "lodi" ), Qt::CaseInsensitive ) == 0 ) ) {\n'
)

SAVE_ANCHOR = (
    '\t\t|| curFile.suffix().compare( QLatin1String( "lodl" ), Qt::CaseInsensitive ) == 0 )\n'
    "\t\treturn saveAsDlg();\n"
)
SAVE_ADD = (
    '\t\t|| curFile.suffix().compare( QLatin1String( "lodl" ), Qt::CaseInsensitive ) == 0\n'
    '\t\t|| curFile.suffix().compare( QLatin1String( "lodi" ), Qt::CaseInsensitive ) == 0 )\n'
    "\t\treturn saveAsDlg();\n"
)

# (path, mode, anchor, text)   mode: "after" | "replace"
EDITS = [
    ("NifSkope.pro", "after", PRO_H, PRO_H_ADD),
    ("NifSkope.pro", "after", PRO_C, PRO_C_ADD),
    ("src/nifskope.cpp", "after", INCLUDE_ANCHOR, INCLUDE_ADD),
    ("src/nifskope.cpp", "after", FILETYPE_ANCHOR, FILETYPE_ADD),
    ("src/nifskope.cpp", "after", LODL_KEEP_ANCHOR, LODL_KEEP_ADD),
    ("src/nifskope.cpp", "replace", LOADFILE_ANCHOR, LOADFILE_ADD),
    ("src/nifskope.cpp", "replace", UNDO_ANCHOR, UNDO_ADD),
    ("src/nifskope.cpp", "replace", SAVE_ANCHOR, SAVE_ADD),
]


def variants(text):
    """The anchor as it may sit in a file whose endings are LF, CRLF or mixed."""
    lf = text.replace("\r\n", "\n")
    crlf = lf.replace("\n", "\r\n")
    return lf, crlf


def find_all(data, anchor):
    """Every place `anchor` sits in `data`, whatever each line's ending is.

    src/nifskope.cpp is a MIXED file (9,560 CR against 10,708 LF), so an
    anchor cannot be matched as one fixed byte string; each of its newlines is
    matched as `\\r?\\n` and the TEXT ACTUALLY FOUND is what gets replaced, so
    the file's own endings survive the edit untouched.
    """
    pattern = "".join(
        re.escape(part) + ("\r?\n" if i < len(anchor.split("\n")) - 1 else "")
        for i, part in enumerate(anchor.replace("\r\n", "\n").split("\n"))
    )
    return [(m.start(), m.group(0)) for m in re.finditer(pattern, data)]


def apply_edits(write):
    by_file = {}
    for path, mode, anchor, add in EDITS:
        by_file.setdefault(path, []).append((mode, anchor, add))

    failures = 0
    for path, edits in by_file.items():
        full = os.path.join(ROOT, path).replace("\\", "/")
        with open(full, "rb") as fh:
            data = fh.read().decode("utf-8")
        cr0 = data.count("\r")
        lf0 = data.count("\n")
        n0 = len(data.encode("utf-8"))
        print("%-20s before  bytes %d  CR %d  LF %d" % (path, n0, cr0, lf0))

        for mode, anchor, add in edits:
            hit = find_all(data, anchor)
            head = anchor.strip().splitlines()[0][:62]
            if len(hit) != 1:
                print("  REFUSED: anchor matches %d times: %s" % (len(hit), head))
                failures += 1
                continue
            at, found = hit[0]
            crlf_here = "\r\n" in found
            t = variants(add)[1 if crlf_here else 0]
            # the probe is the first line the ADDITION has that the ANCHOR
            # does not, so a re-run recognises its own work instead of
            # matching a line the edit merely carried through
            anchor_lines = set(
                l.strip() for l in anchor.replace("\r\n", "\n").split("\n") if l.strip()
            )
            probe = ""
            for l in add.replace("\r\n", "\n").split("\n"):
                if l.strip() and l.strip() not in anchor_lines:
                    probe = l.strip()
                    break
            data_lines = set(
                l.strip() for l in data.replace("\r\n", "\n").split("\n") if l.strip()
            )
            if probe and probe in data_lines:
                # already applied: an idempotent re-run must not double it
                print("  SKIP (already present): %s" % head)
                continue
            data = data[:at] + ((found + t) if mode == "after" else t) + data[at + len(found):]
            print("  ok %-7s %s%s" % (mode, head, "  [CRLF]" if crlf_here else ""))

        cr1 = data.count("\r")
        lf1 = data.count("\n")
        out = data.encode("utf-8")
        print("%-20s after   bytes %d  CR %d  LF %d" % (path, len(out), cr1, lf1))
        if lf1 < lf0 or cr1 < cr0:
            print("  REFUSED: line endings would be LOST")
            failures += 1
            continue
        if write and not failures:
            with open(full, "wb") as fh:
                fh.write(out)
            print("  written")
    return failures


if __name__ == "__main__":
    write = "--write" in sys.argv
    rc = apply_edits(write)
    if rc:
        print("REFUSED: %d anchor(s) did not match exactly once; nothing written" % rc)
    elif not write:
        print("check only; re-run with --write")
    sys.exit(1 if rc else 0)
