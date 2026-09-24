#!/usr/bin/env python
"""Lane HKXEDIT2 hook-up, as a REFUSING script (ww-anchored-hookup).

The lane's own work is in NEW files (src/hkxclipedit.{h,cpp},
src/animdopesheet.{h,cpp}, src/animworkspace.{h,cpp}, src/animworkspacetest.cpp,
tests/hkxclipedit_gate.cpp, tests/spells/animws.sh,
res/hkx_annotation_vocabulary.txt) plus src/hkxplayback.{h,cpp} (replaceClip,
the held node -- the brief lets this lane edit those). The lines an EXISTING
file needs so the new files join the build and the window are listed here and
NOT applied: NifSkope.pro, src/nifskope.h, src/nifskope.cpp and
src/nifskope_ui.cpp are other lanes'.

  python scratchpad/hkxedit2_20260910/hookup.py            # --check (default): counts, writes nothing
  python scratchpad/hkxedit2_20260910/hookup.py --apply    # refuses unless every anchor matches once

ORDER: lane HKXEDIT1's hook-up (scratchpad/hkxedit1_20260910/hookup.py) goes
FIRST -- this build defines WW_HKXCLIP_CANON, which links src/hkxfile.cpp (the
canonical packfile layout the workspace's Save goes through so HKXPACK reads
annotation names), and the script refuses when src/hkxfile.cpp is not in the
.pro. When src/nifskope.h carries HKXEDIT1's `HkxModel * hkx;` the second tier
is applied too: DEFINES += WW_ANIMWS_HKXMODEL, so the Blocks-tab .hkx document
and the workspace edit the same clip. Every anchor carries the file's REAL
line ending (src/nifskope.cpp is CRLF where this edit lands; the others are
LF) -- the script tries "\n" and "\r\n" and uses whichever matches once -- and
the CR count is asserted to move by exactly the CRs the inserted text carries.
Inserted text is marked "(lane HKXEDIT2)" so a resume can tell applied from
not by the marker, never by the anchor.

What the edits do: the seven new sources join HEADERS / SOURCES; the
vocabulary file is copied beside the exe like nif.xml; NifSkope gets an
AnimWorkspace `animws` in a dock `dAnimWs` ("Animation", bottom area, hidden
like its siblings; the OLD Animation Manager dock STAYS until the new one is
proven -- the follow-up retires it); the window's Undo/Redo actions are
created from wwAnimUndoGroup() instead of nif->undoStack (the group holds the
NIF's stack, the .hkx document's and the workspace's, the active one following
focus); NifSkope::select tells the workspace (viewport -> sheet); the
transport, time, sequence, isolate and gizmo-commit signals are wired like the
old dock's; the harness call goes beside the other harness calls.
"""
import sys, os

REPO = r"E:\Projects\NifskopeWildWastelandEdition"

DOCK_BLOCK = (
    "\n"
    "\t/* THE ANIMATION WORKSPACE (lane HKXEDIT2, bungo 2026-09-10: \"Just make hkx\n"
    "\t * fully editable in our nifskope\"; \"Animation manager was one of the\n"
    "\t * first features for nifskope, and it's pretty old and outdated btw\").\n"
    "\t * One dock: the NIF's sequences and the loaded .hkx clips in one list,\n"
    "\t * Blender's dope sheet with keys per bone track, annotations as markers,\n"
    "\t * float tracks, one transport row, the old manager's sequence controls as\n"
    "\t * rows, and the clip fully editable (keys, annotations, trim, retime,\n"
    "\t * root-motion bake, Save as .hkx). The Animation Manager dock above stays\n"
    "\t * until this one is proven in the app; a follow-up retires it. */\n"
    "\tdAnimWs = new QDockWidget( tr( \"Animation\" ), this );\n"
    "\tdAnimWs->setObjectName( \"AnimWorkspaceDock\" );\n"
    "\tanimws = new AnimWorkspace( dAnimWs );\n"
    "\tanimws->setNif( nif );\n"
    "\tanimws->setGLView( ogl );\n"
    "\tanimws->addAnimActions( ui->aAnimLoop, ui->aAnimSwitch );\n"
    "#ifdef WW_ANIMWS_HKXMODEL\n"
    "\tanimws->setHkxModel( hkx );\n"
    "\tanimws->installUndoGroup( nif->undoStack, hkx->undoStack );\n"
    "#else\n"
    "\tanimws->installUndoGroup( nif->undoStack, nullptr );\n"
    "#endif\n"
    "\tdAnimWs->setWidget( animws );\n"
    "\tdAnimWs->setAllowedAreas( Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea\n"
    "\t\t| Qt::RightDockWidgetArea );\n"
    "\taddDockWidget( Qt::BottomDockWidgetArea, dAnimWs );\n"
    "\tdAnimWs->hide();\n"
    "\tconnect( animws, &AnimWorkspace::indexSelected, this, &NifSkope::select );\n"
    "\tconnect( animws, &AnimWorkspace::timeChanged, ogl, &GLView::setSceneTime );\n"
    "\tconnect( ogl, &GLView::sceneTimeChanged, animws, &AnimWorkspace::setTime );\n"
    "\tconnect( this, &NifSkope::completeLoading, animws, &AnimWorkspace::refreshLater );\n"
    "\tconnect( animws, &AnimWorkspace::sequenceActivated, ogl, &GLView::setSceneSequence );\n"
    "\tconnect( ogl, &GLView::sequenceChanged, animws, &AnimWorkspace::setSequenceByName );\n"
    "\tconnect( animws, &AnimWorkspace::isolateBlock, ogl, &GLView::setSoloBlock );\n"
    "\tconnect( ogl, &GLView::transformCommitted, animws, &AnimWorkspace::keyNodeTransform );\n"
    "\t// the transport asks the application exactly as the old dock does: forward\n"
    "\t// the signal into the old dock's, whose lambda below owns the clock\n"
    "\tconnect( animws, &AnimWorkspace::playPauseRequested, timeline, &TimelineWidget::playPauseRequested );\n"
    "\tconnect( ui->aAnimPlay, &QAction::toggled, animws, [this]( bool on ) {\n"
    "\t\tanimws->setPlayingState( on, ogl->animationSpeed() < 0.0f );\n"
    "\t} );\n"
    "\tconnect( ogl, &GLView::sequenceStopped, animws, [this]() {\n"
    "\t\tanimws->setPlayingState( false, false );\n"
    "\t} );\n"
    "\tconnect( dAnimWs->toggleViewAction(), &QAction::triggered, [this]( bool on ) {\n"
    "\t\tif ( on && dAnimWs->isFloating() ) {\n"
    "\t\t\tdAnimWs->setFloating( false );\n"
    "\t\t\taddDockWidget( Qt::BottomDockWidgetArea, dAnimWs );\n"
    "\t\t}\n"
    "\t} );\n"
)

# (file, mode, [candidate anchors -- the first that matches ONCE is used], text)
EDITS = [
    # ---- NifSkope.pro: headers, sources, the defines, the vocabulary beside the exe
    ("NifSkope.pro", "after", ["\tsrc/hkxwrite.h \\\n"],
     "\tsrc/hkxclipedit.h \\\n\tsrc/animdopesheet.h \\\n\tsrc/animworkspace.h \\\n"),
    ("NifSkope.pro", "after", ["\tsrc/hkxanimuitest.cpp \\\n"],
     "\tsrc/hkxclipedit.cpp \\\n\tsrc/animdopesheet.cpp \\\n\tsrc/animworkspace.cpp \\\n\tsrc/animworkspacetest.cpp \\\n"),
    ("NifSkope.pro", "after", ["DEFINES += WW_HKXANIM_UI\n"],
     "# (lane HKXEDIT2) the animation workspace's Save goes through the canonical\n"
     "# packfile layout (src/hkxfile, lane HKXEDIT1), so HKXPACK reads its annotation\n"
     "# names; without the define the writer's own bytes are kept.\n"
     "DEFINES += WW_HKXCLIP_CANON\n"),
    ("NifSkope.pro", "replace", ["\t\tres/hkclasses_fo4.json\n", "\t\tbuild/docsys/kfmxml/kfm.xml\n"],
     None),   # text built at run time from the anchor that matched (see below)
    # ---- nifskope.h
    ("src/nifskope.h", "after", ["class TimelineWidget;\n"],
     "class AnimWorkspace;	// lane HKXEDIT2: the animation workspace dock\n"),
    ("src/nifskope.h", "after", ["\tTimelineWidget * timeline = nullptr;\n"],
     "\t//! The animation workspace (lane HKXEDIT2), replacing the Animation Manager\n\tAnimWorkspace * animws = nullptr;\n"),
    ("src/nifskope.h", "after", ["\tQDockWidget * dTimeline;\n"],
     "\tQDockWidget * dAnimWs = nullptr;	// (lane HKXEDIT2)\n"),
    # ---- nifskope_ui.cpp
    ("src/nifskope_ui.cpp", "after", ["#include \"hkxanimui.h\"\t\t// lane HKX3\n"],
     "#include \"animworkspace.h\"		// lane HKXEDIT2\n"),
    ("src/nifskope_ui.cpp", "after", ["\twwHkxAnimUiHarness( skope );\n"],
     "\twwAnimWorkspaceHarness( skope );	// (lane HKXEDIT2) WW_ANIMWS_TEST\n"),
    ("src/nifskope_ui.cpp", "replace", ["\tundoAction = nif->undoStack->createUndoAction( this, tr( \"&Undo\" ) );\n"],
     "\tundoAction = wwAnimUndoGroup()->createUndoAction( this, tr( \"&Undo\" ) );	// (lane HKXEDIT2) one group: the NIF's stack, the .hkx document's, the workspace's\n"),
    ("src/nifskope_ui.cpp", "replace", ["\tredoAction = nif->undoStack->createRedoAction( this, tr( \"&Redo\" ) );\n"],
     "\tredoAction = wwAnimUndoGroup()->createRedoAction( this, tr( \"&Redo\" ) );	// (lane HKXEDIT2)\n"),
    ("src/nifskope_ui.cpp", "after", ["\ttimeline->addAnimActions( ui->aAnimLoop, ui->aAnimSwitch );\n"],
     DOCK_BLOCK),
    # the two dock lists (workspace set-up and activateWorkspace): appended LAST so
    # the workspace indices keep pointing where they did (the comment above the list)
    ("src/nifskope_ui.cpp", "replace", ["\t\tdSkeletonMgr, dUnfuckMgr, dLodGen\n\t};\n\tfor ( QDockWidget * manager : workspaceManagers )\n"],
     "\t\tdSkeletonMgr, dUnfuckMgr, dLodGen, dAnimWs\t// (lane HKXEDIT2) the animation workspace\n\t};\n\tfor ( QDockWidget * manager : workspaceManagers )\n"),
    ("src/nifskope_ui.cpp", "replace", ["\t\t\tdSkeletonMgr, dUnfuckMgr, dLodGen\n\t\t};\n\t\tauto activateWorkspace"],
     "\t\t\tdSkeletonMgr, dUnfuckMgr, dLodGen, dAnimWs\t// (lane HKXEDIT2)\n\t\t};\n\t\tauto activateWorkspace"),
    # ---- nifskope.cpp (CRLF here): viewport -> sheet selection
    ("src/nifskope.cpp", "after", ["\tif ( timeline && sender() != timeline )\n\t\ttimeline->setCurrentIndex( idx );\n"],
     "\tif ( animws && sender() != animws )\n\t\tanimws->setCurrentIndex( idx );	// (lane HKXEDIT2) viewport -> the dope sheet's bone row\n"),
]

# the second tier: only when HKXEDIT1's model is in the window
TIER2 = [
    ("NifSkope.pro", "after", ["DEFINES += WW_HKXCLIP_CANON\n"],
     "# (lane HKXEDIT2) the Blocks-tab .hkx document (HkxModel) and the workspace edit one clip\nDEFINES += WW_ANIMWS_HKXMODEL\n"),
]

MARKER = "lane HKXEDIT2"


def vocab_text(anchor):
    # the copy list: append the vocabulary after whichever line matched
    line = anchor.rstrip("\n")
    return line + " \\\n\t\tres/hkx_annotation_vocabulary.txt\n"


def main():
    apply = "--apply" in sys.argv
    ok = True
    plan = []
    pro = open(os.path.join(REPO, "NifSkope.pro"), "rb").read()
    if b"src/hkxfile.cpp" not in pro:
        print("REFUSED: src/hkxfile.cpp is not in NifSkope.pro -- apply lane HKXEDIT1's hookup.py first (WW_HKXCLIP_CANON links it)")
        ok = False
    nh = open(os.path.join(REPO, "src", "nifskope.h"), "rb").read()
    tier2 = b"HkxModel * hkx;" in nh
    print("tier 2 (WW_ANIMWS_HKXMODEL): %s" % ("yes -- HkxModel * hkx is in nifskope.h" if tier2 else "no -- HkxModel not in the window yet"))
    edits = EDITS + (TIER2 if tier2 else [])
    for path, mode, anchors, text in edits:
        full = os.path.join(REPO, path)
        b = open(full, "rb").read()
        cr = b.count(b"\r")
        chosen = None
        counts = []
        for anchor in anchors:
            for eol in ("\n", "\r\n"):
                a = anchor.replace("\n", eol).encode("utf-8")
                n = b.count(a)
                counts.append((anchor[:40], repr(eol), n))
                if n == 1 and chosen is None:
                    chosen = (a, eol, anchor)
        if chosen is None:
            print("%-20s %-7s NO anchor matches once: %s" % (path, mode, counts))
            ok = False
            continue
        a, eol, anchor = chosen
        t = text if text is not None else vocab_text(anchor)
        t = t.replace("\n", eol).encode("utf-8")
        marker_present = MARKER.encode() in b
        print("%-20s %-7s count=1 eol=%s CR=%d anchor=%r%s" % (path, mode, repr(eol), cr, anchor[:48],
              "   NOTE: file already carries a '%s' marker -- applied before?" % MARKER if marker_present else ""))
        plan.append((full, mode, a, t, cr))
    if not ok:
        print("CHECK: refused (see above); nothing written")
        return 2
    if not apply:
        print("CHECK: every anchor matches once; %d edits over %d files; nothing written (use --apply)"
              % (len(plan), len(set(p[0] for p in plan))))
        return 0
    # apply, file by file, all edits of a file in one write
    byfile = {}
    for full, mode, a, t, cr in plan:
        byfile.setdefault(full, []).append((mode, a, t, cr))
    for full, items in byfile.items():
        b = open(full, "rb").read()
        cr0 = b.count(b"\r")
        added_cr = 0
        for mode, a, t, cr in items:
            assert b.count(a) == 1, "anchor moved between check and apply: %r" % a[:40]
            if mode == "after":
                b = b.replace(a, a + t, 1)
                added_cr += t.count(b"\r")
            else:
                b = b.replace(a, t, 1)
                added_cr += t.count(b"\r") - a.count(b"\r")
        cr1 = b.count(b"\r")
        assert cr1 == cr0 + added_cr, "CR count moved by %d, expected %d" % (cr1 - cr0, added_cr)
        open(full, "wb").write(b)
        print("APPLIED %s: %d edit(s), CR %d -> %d, %d bytes" % (os.path.relpath(full, REPO), len(items), cr0, cr1, len(b)))
    print("APPLY: done. Next: qmake before make (new HEADERS/SOURCES and DEFINES), then tests/spells/animws.sh")
    return 0


if __name__ == "__main__":
    sys.exit(main())
