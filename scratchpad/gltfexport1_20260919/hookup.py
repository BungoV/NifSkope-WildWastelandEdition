# Lane GLTFEXPORT1's hookup: the edits that must land in files ANOTHER LANE IS
# HOLDING. 2026-09-19.
#
# NifSkope.pro, src/nifcli.cpp and src/nifskope_ui.cpp were all "M" in
# `git status` at this lane's launch (lane HORIZONOUT owns them and about 235
# other files). So this lane wrote everything else as NEW files and put the
# four unavoidable splices here, where they can be applied in one step, in the
# build window, after HORIZONOUT has finished -- instead of being smeared into
# a shared working tree by two lanes at once.
#
# IT REFUSES. Every hunk is ANCHORED on text that must be present exactly once.
# If an anchor is missing, or ambiguous, or the hunk is already there, nothing
# at all is written and the reason is printed. There is no "apply what fits":
# a partial splice into a file someone else is editing is the failure mode this
# script exists to prevent.
#
#   python scratchpad/gltfexport1_20260919/hookup.py --check    say what it would do
#   python scratchpad/gltfexport1_20260919/hookup.py            do it
#
# LINE ENDINGS. All four files measured 2026-09-19 with Python byte counts:
# NifSkope.pro 788 LF / 0 CRLF, src/nifcli.cpp 8720 / 0, src/nifskope_ui.cpp
# 33775 / 0, src/lib/importex/importex.cpp 216 / 0 -- LF only, every one. This
# script reads and writes in binary with newline='' and inserts "\n", so it
# cannot convert a file it touches. It re-measures before writing and refuses
# if what it finds is not what is written above.
import io
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
CHECK = "--check" in sys.argv

# (path, expected LF count is not pinned -- only "no CRLF at all" is)
HUNKS = []


def hunk(path, anchor, insert, where="after", note=""):
    HUNKS.append(dict(path=path, anchor=anchor, insert=insert, where=where, note=note))


def before(path, anchor, insert, note=""):
    HUNKS.append(dict(path=path, anchor=anchor, insert=insert, where="before", note=note))


def replace(path, old, new, note=""):
    HUNKS.append(dict(path=path, anchor=old, insert=new, where="replace", note=note))


# ---------------------------------------------------------------------------
# 1. NifSkope.pro -- the new translation units.
#
# bodybuildpanel.h and gltfexportdialog.h carry Q_OBJECT, so they MUST be in
# HEADERS or moc never runs and the vtables are missing at link.

# NifSkope.pro is TAB-indented (measured 2026-09-19: line 200 is
# "^Isrc/gltfexportnif.h \"), not four spaces. An anchor with the wrong
# whitespace is exactly the kind of near-miss this script refuses on.

hunk("NifSkope.pro",
     "\tsrc/gltfexportnif.h \\\n",
     "\tsrc/gltfexportopts.h \\\n"
     "\tsrc/gltfexportchar.h \\\n"
     "\tsrc/gltfexportdialog.h \\\n"
     "\tsrc/bodybuild.h \\\n"
     "\tsrc/bodybuildpanel.h \\\n",
     note="five new headers (two carry Q_OBJECT)")

hunk("NifSkope.pro",
     "\tsrc/gltfexportnif.cpp \\\n",
     "\tsrc/gltfexportopts.cpp \\\n"
     "\tsrc/gltfexportchar.cpp \\\n"
     "\tsrc/gltfexportdialog.cpp \\\n"
     "\tsrc/gltfexportdialogtest.cpp \\\n"
     "\tsrc/bodybuild.cpp \\\n"
     "\tsrc/bodybuildpanel.cpp \\\n",
     note="six new sources")

# ---------------------------------------------------------------------------
# 2. src/nifcli.cpp -- one flag for every dialog row, on `gltf`.

hunk("src/nifcli.cpp",
     '#include "gltfexportnif.h"\t\t\t// lane HKX4\n',
     '#include "gltfexportchar.h"\t\t\t// lane GLTFEXPORT1\n'
     '#include "gltfexportopts.h"\t\t\t// lane GLTFEXPORT1\n',
     note="the options struct and the character export")

hunk("src/nifcli.cpp",
     "\tbool gltfRootMotion = false;\t\t\t// lane HKX4\n",
     "\tGltfExportOptions gltfOpts;\t\t\t// lane GLTFEXPORT1: one struct, two front ends\n"
     "\tbool gltfUsedNext = false;\n"
     "\tQString gltfFlagError;\n",
     note="the options local")

hunk("src/nifcli.cpp",
     '\t\telse if ( t == QLatin1String( "--root-node" ) ) gltfRootNode = next();\t// lane BUILD8\n',
     "\t\t// lane GLTFEXPORT1: every export option, through the SAME function the\n"
     "\t\t// dialog's rows drive, so a flag and a row cannot mean different things.\n"
     "\t\telse if ( int gr = gltfExportParseFlag( t, ( i + 1 < a.size() ) ? a.at( i + 1 ) : QString(),\n"
     "\t\t\t\t\t\t\t\t\t\t\t\tgltfOpts, gltfUsedNext, gltfFlagError ) ) {\n"
     "\t\t\tif ( gr < 0 ) { err() << \"gltf: \" << gltfFlagError << Qt::endl; return 2; }\n"
     "\t\t\tif ( gltfUsedNext ) i++;\n"
     "\t\t}\n",
     note="the flag fall-through")

replace("src/nifcli.cpp",
        "int cmdGltf( const QString & file, const QString & outFile, const QString & clipFile,\n"
        "\t\t\t const QString & bonesFile, bool rootMotion )\n",
        "int cmdGltf( const QString & file, const QString & outFile, const QString & clipFile,\n"
        "\t\t\t const QString & bonesFile, bool rootMotion, const GltfExportOptions & optsIn )\n",
        note="cmdGltf takes the options")

replace("src/nifcli.cpp",
        "\tGltfExportReport report;\n"
        "\tQString error;\n"
        "\tif ( !gltfExportNifWrite( &nif, QModelIndex(), haveClip ? &clip : nullptr, boneNames,\n"
        "\t\t\t\t\t\t\t  rootMotion, outFile, report, error ) ) {\n"
        "\t\terr() << \"gltf: \" << error << Qt::endl;\n"
        "\t\treturn 1;\n"
        "\t}\n",
        "\tGltfExportReport report;\n"
        "\tGltfExportCharReport charReport;\n"
        "\tQString error;\n"
        "\t// lane GLTFEXPORT1. optsIn carries the flags; --root-motion is folded in so\n"
        "\t// the old spelling still means what it meant. With no new flag at all this\n"
        "\t// is gltfExportOptionsAreLegacy() and gltfExportCharacter() forwards to the\n"
        "\t// old writer unchanged -- the byte-identity row of the gate.\n"
        "\tGltfExportOptions opts = optsIn;\n"
        "\tif ( rootMotion && opts.rootMotion == GltfExportOptions::RootMotion::Strip )\n"
        "\t\topts.rootMotion = GltfExportOptions::RootMotion::Root;\n"
        "\tif ( !haveClip )\n"
        "\t\topts.includeClip = false;\n"
        "\tif ( !gltfExportCharacter( &nif, QModelIndex(), haveClip ? &clip : nullptr, boneNames,\n"
        "\t\t\t\t\t\t\t   opts, outFile, report, charReport, error ) ) {\n"
        "\t\terr() << \"gltf: \" << error << Qt::endl;\n"
        "\t\treturn 1;\n"
        "\t}\n"
        "\tfor ( const QString & s : gltfExportOptionsSummary( opts ) )\n"
        "\t\tout() << \"  \" << s << Qt::endl;\n"
        "\tfor ( const QString & s : charReport.notes )\n"
        "\t\tout() << \"  \" << s << Qt::endl;\n"
        "\tif ( !charReport.unmatchedBones.isEmpty() )\n"
        "\t\tout() << \"  \" << charReport.unmatchedBones.size() << \" bone(s) reached no node: \"\n"
        "\t\t\t  << charReport.unmatchedBones.join( QStringLiteral( \", \" ) ) << Qt::endl;\n",
        note="cmdGltf goes through the character export")

replace("src/nifcli.cpp",
        "\t\trc = cmdGltf( file, outFile, gltfClip, gltfBones, gltfRootMotion );\n",
        "\t\trc = cmdGltf( file, outFile, gltfClip, gltfBones, gltfRootMotion, gltfOpts );\n",
        note="the dispatch")

replace("src/nifcli.cpp",
        '\telse if ( cmd == QLatin1String( "gltf" ) )\t\t\t// lane HKX4\n',
        '\telse if ( cmd == QLatin1String( "gltf" ) || cmd == QLatin1String( "gltf-export" ) )\t// lane HKX4 / GLTFEXPORT1\n',
        note="gltf-export as the spelling of the command in the brief")

hunk("src/nifcli.cpp",
     '\t\t  << "                                          bone names the tracks are read by\\n"\n',
     '\t\t  << gltfExportOptionsHelp()\n',
     note="the flag help, generated from the same table as the dialog")

# ---------------------------------------------------------------------------
# 3. src/nifskope_ui.cpp -- the Body Build dock and the two harnesses.

hunk("src/nifskope_ui.cpp",
     '#include "animworkspace.h"\t\t// lane HKXEDIT2\n',
     '#include "bodybuildpanel.h"\t\t// lane GLTFEXPORT1\n',
     note="the Body Build panel's header")

# The harness block. MEASURED anchor (src/nifskope_ui.cpp:8226): the existing
# line takes `skope`, not `this` -- this is a free function called from a static
# helper, not a member.
hunk("src/nifskope_ui.cpp",
     "\twwAnimWorkspaceHarness( skope );\t// (lane HKXEDIT2) WW_ANIMWS_TEST\n",
     "\t{\n"
     "\t\t// lane GLTFEXPORT1: WW_BODY_BUILD (the build triangle) and\n"
     "\t\t// WW_GLTF_EXPORT_DIALOG (the export options dialog).\n"
     "\t\textern void wwBodyBuildHarness( NifSkope * );\n"
     "\t\textern void wwGltfExportDialogHarness( NifSkope * );\n"
     "\t\twwBodyBuildHarness( skope );\n"
     "\t\twwGltfExportDialogHarness( skope );\n"
     "\t}\n",
     note="the two harnesses, beside the others")

# The dock. NOT in `workspaceManagers` / `managers`: those two lists are
# INDEX-ADDRESSED (a stored workspace number picks a dock out of them), so
# appending to them would silently renumber every saved workspace. Body Build
# is an ordinary dock with its own View entry, like the inspector.
before("src/nifskope_ui.cpp",
       "\t/* THE ANIMATION WORKSPACE (lane HKXEDIT2, bungo 2026-09-10: \"Just make hkx\n",
       "\t/* THE BODY BUILD dock (lane GLTFEXPORT1, bungo 2026-09-19 05:0x: \"Fallout 4\n"
       "\t * has 'skin' bones, those are not used for animations, but only for the\n"
       "\t * character's build?\"). A PREVIEW: it scales the loaded character's *_skin\n"
       "\t * nodes from the RACE record's own Bone Scale Data and writes nothing.\n"
       "\t * The dock is a local -- the harness finds the panel by object name, and\n"
       "\t * nothing else in the window addresses it -- so nifskope.h gains no member\n"
       "\t * while another lane is holding that file. */\n"
       "\t{\n"
       "\t\tQDockWidget * dBodyBuild = new QDockWidget( tr( \"Body Build\" ), this );\n"
       "\t\tdBodyBuild->setObjectName( \"BodyBuildDock\" );\n"
       "\t\tBodyBuildPanel * bodyBuild = new BodyBuildPanel( dBodyBuild );\n"
       "\t\tbodyBuild->setObjectName( \"BodyBuildPanel\" );\n"
       "\t\tbodyBuild->setNif( nif );\n"
       "\t\tbodyBuild->setGLView( ogl );\n"
       "\t\tdBodyBuild->setWidget( bodyBuild );\n"
       "\t\tdBodyBuild->setAllowedAreas( Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea\n"
       "\t\t\t| Qt::BottomDockWidgetArea );\n"
       "\t\taddDockWidget( Qt::RightDockWidgetArea, dBodyBuild );\n"
       "\t\tdBodyBuild->hide();\n"
       "\t\t// A rebuilt scene has new Node objects: the saved transforms the panel\n"
       "\t\t// holds point at nodes that no longer exist, so it drops them WITHOUT\n"
       "\t\t// restoring and works the preview out again against what is there now.\n"
       "\t\tconnect( this, &NifSkope::completeLoading, bodyBuild,\n"
       "\t\t\t\t [bodyBuild]( bool, QString & ) { bodyBuild->onSceneRebuilt(); } );\n"
       "\t}\n"
       "\n",
       note="the Body Build dock")


# ---------------------------------------------------------------------------

def main():
    print("root: %s" % ROOT)
    files = {}
    for h in HUNKS:
        files.setdefault(h["path"], None)

    # read + refuse on a line-ending surprise
    for p in list(files):
        full = os.path.join(ROOT, p)
        if not os.path.isfile(full):
            print("REFUSED: %s does not exist" % p)
            return 2
        raw = open(full, "rb").read()
        if b"\r\n" in raw:
            print("REFUSED: %s contains CRLF; this script was written for the LF-only "
                  "state measured on 2026-09-19 and will not convert a file" % p)
            return 2
        files[p] = raw.decode("utf-8")

    plan = []
    for h in HUNKS:
        s = files[h["path"]]
        n = s.count(h["anchor"])
        short = h["anchor"].strip().splitlines()[0][:64] if h["anchor"].strip() else "?"
        if h["where"] == "replace":
            already = h["insert"] in s
        else:
            already = h["insert"] in s
        if already:
            print("REFUSED: %s -- already applied (%s)" % (h["path"], h["note"]))
            return 3
        if n == 0:
            print("REFUSED: %s -- anchor not found: %r" % (h["path"], short))
            print("         (%s)" % h["note"])
            return 3
        if n > 1:
            print("REFUSED: %s -- anchor found %d times, it must be unique: %r"
                  % (h["path"], n, short))
            return 3
        plan.append((h, short))

    for h, short in plan:
        verb = {"replace": "replace", "before": "insert before"}.get(h["where"], "insert after")
        print("  %-28s %-13s %-62s  %s" % (h["path"], verb, short, h["note"]))

    if CHECK:
        print("")
        print("--check: %d hunk(s) would apply, nothing written" % len(plan))
        return 0

    for h in HUNKS:
        s = files[h["path"]]
        if h["where"] == "replace":
            files[h["path"]] = s.replace(h["anchor"], h["insert"], 1)
        elif h["where"] == "before":
            files[h["path"]] = s.replace(h["anchor"], h["insert"] + h["anchor"], 1)
        else:
            files[h["path"]] = s.replace(h["anchor"], h["anchor"] + h["insert"], 1)

    for p, s in files.items():
        full = os.path.join(ROOT, p)
        data = s.encode("utf-8")
        if b"\r\n" in data:
            print("REFUSED before writing: %s would gain CRLF" % p)
            return 4
        with io.open(full, "wb") as f:
            f.write(data)
        print("wrote %s (%d bytes, %d LF, 0 CRLF)" % (p, len(data), data.count(b"\n")))

    print("")
    print("%d hunk(s) applied. Build, then run:" % len(HUNKS))
    print("  bash tests/spells/gltf_export_options.sh")
    print("  bash tests/spells/body_build.sh")
    return 0


if __name__ == "__main__":
    sys.exit(main())
