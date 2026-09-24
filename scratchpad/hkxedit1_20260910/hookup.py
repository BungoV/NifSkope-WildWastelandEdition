#!/usr/bin/env python
"""Lane HKXEDIT1 hook-up, as a REFUSING script (ww-anchored-hookup).

The lane's own work is in NEW files (src/hkxfile.{h,cpp}, src/hkxmodel.{h,cpp},
src/hkxmodeltest.cpp, tests/hkxfile_gate.cpp, res/hkclasses_fo4.json). The
lines an EXISTING file needs so they join the build and the window are listed
here and NOT applied: NifSkope.pro, src/nifskope.h, src/nifskope.cpp and
src/nifskope_ui.cpp are held by other lanes' pending patches.

  python scratchpad/hkxedit1_20260910/hookup.py            # --check (default): counts, writes nothing
  python scratchpad/hkxedit1_20260910/hookup.py --apply    # refuses unless every anchor matches once

Every anchor carries the file's REAL line ending (src/nifskope.cpp is mixed,
mostly CRLF, but both regions edited here are LF blocks; the others are LF-
only) and the CR count is asserted unchanged after --apply. Inserted text is
marked "(lane HKXEDIT1)" so a resume can tell applied from not by the marker,
never by the anchor.

What the edits do, in one paragraph: the two new sources and the class
database join the build (the JSON is copied beside the exe like nif.xml);
NifSkope gets an HkxModel `hkx` beside `kfm`; NifSkope::load() routes an .hkx
to it the way it routes a .kfm (the previous NIF stays in the viewport, the
Blocks tab shows the packfile's objects, and the clip is ALSO handed to the
Files-tab animation route so it plays on that NIF); saveFile() writes an .hkx
through the model; the block views are bound to `hkx` while an .hkx is the
document and back to `nif` when a NIF loads. Undo/Redo: the window's actions
are created from nif->undoStack (nifskope_ui.cpp:23693); the hkx document's
edits are undoable on hkx->undoStack, which those actions do NOT reach until a
QUndoGroup binds both -- owed, listed in the report.
"""
import sys, os

REPO = r"E:\Projects\NifskopeWildWastelandEdition"

EDITS = [
    # ---- NifSkope.pro: headers, sources, the class database beside the exe
    ("NifSkope.pro", "after", "\tsrc/hkxwrite.h \\\n",
     "\tsrc/hkxfile.h \\\n\tsrc/hkxmodel.h \\\n"),
    ("NifSkope.pro", "after", "\tsrc/hkxwrite.cpp \\\n",
     "\tsrc/hkxfile.cpp \\\n\tsrc/hkxmodel.cpp \\\n\tsrc/hkxmodeltest.cpp \\\n"),
    ("NifSkope.pro", "replace", "\t\tbuild/docsys/kfmxml/kfm.xml\n",
     "\t\tbuild/docsys/kfmxml/kfm.xml \\\n\t\tres/hkclasses_fo4.json\n"),
    # ---- nifskope.h: the model beside kfm
    ("src/nifskope.h", "after", "class KfmModel;\n",
     "class HkxModel;	// lane HKXEDIT1: a Havok packfile as a block tree\n"),
    ("src/nifskope.h", "after", "\tKfmModel * kfmEmpty;\n",
     "\t//! Stores an .hkx packfile in memory as blocks (lane HKXEDIT1).\n\tHkxModel * hkx;\n\tHkxModel * hkxEmpty;\n"),
    # ---- nifskope.cpp: include, construction, load route, save route
    ("src/nifskope.cpp", "after", '#include "model/kfmmodel.h"\n',
     '#include "hkxmodel.h"	// lane HKXEDIT1\n#include "filestab.h"	// lane HKXEDIT1: the clip route beside the block route\n'),
    ("src/nifskope.cpp", "after", "\tkfmEmpty = new KfmModel( this );\n",
     "\thkx = new HkxModel( this );	// (lane HKXEDIT1)\n\thkxEmpty = new HkxModel( this );\n"),
    ("src/nifskope.cpp", "after",
     "\tif ( f.suffix().compare( \"kfm\", Qt::CaseInsensitive ) == 0 ) {\n\t\temit completeLoading( kfm->loadFromFile( fname ), fname );\n\n\t\tf.setFile( kfm->getFolder(), kfm->get<QString>( kfm->getKFMroot(), \"NIF File Name\" ) );\n\n\t\treturn;\n\t}\n",
     "\t/* AN .hkx OPENED AS A DOCUMENT IS A BLOCK TREE (lane HKXEDIT1, bungo\n"
     "\t * 2026-09-10: \"Just make hkx fully editable in our nifskope\"). The same\n"
     "\t * route as a .kfm: the packfile's objects become the Blocks tab, the NIF\n"
     "\t * already in the viewport stays there, and the clip is ALSO handed to\n"
     "\t * the Files-tab animation route so it plays on that NIF. A refusal is\n"
     "\t * the model's own sentence (field and value), never silence. */\n"
     "\tif ( f.suffix().compare( \"hkx\", Qt::CaseInsensitive ) == 0 ) {\n"
     "\t\tconst bool ok = hkx->loadFromFile( fname );\n"
     "\t\tif ( ok ) {\n"
     "\t\t\twwReleaseBlockListColumns();\n"
     "\t\t\tlist->setModel( hkx );\n"
     "\t\t\ttree->setModel( hkx );\n"
     "\t\t\twwApplyBlockListColumns();\n"
     "\t\t\twireBlockListSelection();\n"
     "\t\t\tif ( ogl )\n"
     "\t\t\t\twwFilesTabOpenAnimation( ogl, fname, hkx->toBytes(), fname );\n"
     "\t\t} else if ( ui && ui->statusbar ) {\n"
     "\t\t\tui->statusbar->showMessage( hkx->loadError(), 12000 );\n"
     "\t\t}\n"
     "\t\temit completeLoading( ok, fname );\n"
     "\t\treturn;\n"
     "\t}\n"
     "\t// a NIF replacing an .hkx document takes the block views back (lane HKXEDIT1)\n"
     "\tif ( tree->model() == hkx ) {\n"
     "\t\twwReleaseBlockListColumns();\n"
     "\t\tlist->setModel( proxy );\n"
     "\t\ttree->setModel( nif );\n"
     "\t\twwApplyBlockListColumns();\n"
     "\t\twireBlockListSelection();\n"
     "\t}\n"),
    ("src/nifskope.cpp", "after",
     "\tif ( fname.endsWith( \".KFM\", Qt::CaseInsensitive ) ) {\n\t\tsaved = kfm->saveToFile( fname );\n",
     "\t} else if ( fname.endsWith( \".hkx\", Qt::CaseInsensitive ) && tree->model() == hkx ) {\n"
     "\t\tsaved = hkx->saveToFile( fname );	// (lane HKXEDIT1) the packfile writer; byte-identical when unedited\n"),
    # ---- nifskope_ui.cpp: the one-line harness call (ww-test-harness-add)
    ("src/nifskope_ui.cpp", "after", "\twwFilesTabHarness( skope );\n",
     "\t{\n\t\textern void wwHkxModelHarness( NifSkope * );	// (lane HKXEDIT1) WW_HKXMODEL_TEST\n\t\twwHkxModelHarness( skope );\n\t}\n"),
]

# src/nifskope.cpp is MIXED (9,520 CR over 10,669 LF) and all four regions
# edited here are CRLF -- measured with Python byte counts; `cat -A` through
# the tool showed no ^M and was wrong (MISTAKES.md 2026-09-10). The anchors
# above are written with "\n" for legibility and converted per file here.
CRLF_FILES = {"src/nifskope.cpp"}


def main():
    apply = "--apply" in sys.argv
    ok = True
    plan = []
    for path, mode, anchor, text in EDITS:
        full = os.path.join(REPO, path)
        b = open(full, "rb").read()
        if path in CRLF_FILES:
            anchor = anchor.replace("\n", "\r\n")
            text = text.replace("\n", "\r\n")
        a = anchor.encode("utf-8")
        n = b.count(a)
        cr = b.count(b"\r")
        print("%-20s %-7s count=%d CR=%d  anchor=%r" % (path, mode, n, cr, anchor[:60]))
        if n != 1:
            ok = False
        marker = b"(lane HKXEDIT1)" in b or b"lane HKXEDIT1" in b
        if marker:
            print("   NOTE %s already carries a 'lane HKXEDIT1' marker: applied before?" % path)
        plan.append((full, mode, a, text.encode("utf-8"), cr))
    if not ok:
        print("REFUSED: an anchor does not match exactly once; nothing written")
        return 2
    if not apply:
        print("--check only: every anchor matches once; run with --apply to write")
        return 0
    byfile = {}
    for full, mode, a, t, cr in plan:
        byfile.setdefault(full, []).append((mode, a, t, cr))
    for full, edits in byfile.items():
        b = open(full, "rb").read()
        cr0 = b.count(b"\r")
        for mode, a, t, cr in edits:
            assert b.count(a) == 1, full
            if mode == "after":
                b = b.replace(a, a + t)
            else:
                b = b.replace(a, t)
        assert b.count(b"\r") == cr0, "CR count moved in %s" % full
        open(full, "wb").write(b)
        print("wrote %s (+%d bytes, CR %d unchanged)" % (full, len(b) - len(open(full, "rb").read()) + len(b) - len(b), cr0))
    return 0


if __name__ == "__main__":
    sys.exit(main())
