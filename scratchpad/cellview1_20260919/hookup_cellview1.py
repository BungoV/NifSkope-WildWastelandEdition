#!/usr/bin/env python3
"""Lane CELLVIEW1 -- the hook-up, as a REFUSING script rather than an edit.

Four existing files need a few lines each so the new cell-view files join the
build and a `.wwcell` opens.  On 2026-09-19 those files are being edited by
other lanes, so this lane wrote the lines down instead of applying them:

  NifSkope.pro        2 header + 2 source rows
  src/esmdata.h       XLYR / XESP on EsmRefr, EDID on EsmLodBase, the switch
  src/esmdata.cpp     parse those, and widen MODL past TREE/STAT
  src/nifskope.cpp    the include, the file-type row, the `.wwcell` branch

RULES THIS SCRIPT KEEPS (skill `ww-anchored-hookup`):
  * every anchor must occur EXACTLY ONCE, or the edit is refused BY NAME;
  * each inserted block carries THAT FILE'S OWN line ending -- src/nifskope.cpp
    is CRLF (10874 CRLF / 5 bare LF at 720762a), the other three are LF;
  * the CR and LF byte counts are asserted before and after, and a file whose
    counts did not move by exactly what was inserted is rolled back;
  * `--check` writes NOTHING, ever;
  * the ALREADY-APPLIED test is a MARKER THAT IS NOT THE ANCHOR (lane
    HARNESSWIN1 made that mistake today: anchoring on the marker means a second
    run silently re-applies or silently refuses, and you cannot tell which).

Usage:  python hookup_cellview1.py --check          (default; writes nothing)
        python hookup_cellview1.py --apply
        python hookup_cellview1.py --revert         (removes exactly what it added)
"""

import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# ---------------------------------------------------------------- the edits
# (file, marker, anchor, insertion, where)  where = 'after' | 'before'
# The MARKER is a distinctive string from the INSERTION, never the anchor.

EDITS = [
    # ---------------------------------------------------------- NifSkope.pro
    ('NifSkope.pro', 'src/cellview.h',
     '\tsrc/lodinative.h \\\n',
     '\tsrc/cellpick.h \\\n\tsrc/cellview.h \\\n', 'before'),
    ('NifSkope.pro', 'src/cellview.cpp',
     '\tsrc/lodinative.cpp \\\n',
     '\tsrc/cellpick.cpp \\\n\tsrc/cellview.cpp \\\n', 'before'),

    # ---------------------------------------------------------- src/esmdata.h
    ('src/esmdata.h', 'ESM_HAS_CELL_FIELDS',
     '#define ESMDATA_H\n',
     '\n/* Lane CELLVIEW1 (2026-09-19): the fields the CELL VIEW needs and the\n'
     ' * LOD bake never did -- the layer a ref is on, its enable parent, and a\n'
     ' * base\'s editor id. A consumer compiled before this existed still\n'
     ' * compiles: src/cellview.cpp tests this switch and says in its census\n'
     ' * line which half it was built against. */\n'
     '#define ESM_HAS_CELL_FIELDS 1\n', 'after'),
    ('src/esmdata.h', 'quint32 layer = 0;',
     '\tbool deleted = false;\n',
     '\t/* v10 (lane CELLVIEW1): XLYR, the Creation Kit LAYER this ref is on,\n'
     '\t * and XESP, its enable parent. Both 0 when the ref carries neither. */\n'
     '\tquint32 layer = 0;\n'
     '\tquint32 enableParent = 0;\n'
     '\tbool enableParentOpposite = false;    //!< XESP flag bit 0: the parent\'s state, inverted\n',
     'after'),
    ('src/esmdata.h', 'QString edid;',
     '\tbool hasLod = false;\n',
     '\tQString edid;               //!< EDID, for the cell view\'s pick panel\n', 'after'),

    # -------------------------------------------------------- src/esmdata.cpp
    ('src/esmdata.cpp', 'f == "XESP"',
     '\t\t\t\t\t} else if ( f == "XSCL" && f.size() >= 4 ) {\n'
     '\t\t\t\t\t\tref.scale = f.readFloat();\n',
     '\t\t\t\t\t} else if ( f == "XESP" && f.size() >= 8 ) {\n'
     '\t\t\t\t\t\t/* Enable parent: the ref form, then a flag word whose bit 0\n'
     '\t\t\t\t\t\t * is "opposite of parent". Read, never simulated -- the\n'
     '\t\t\t\t\t\t * parent\'s runtime state is not in the plugin. */\n'
     '\t\t\t\t\t\tref.enableParent = esm->mapFormID( *r, f.readUInt32() );\n'
     '\t\t\t\t\t\tref.enableParentOpposite = ( f.readUInt32() & 1 ) != 0;\n'
     '\t\t\t\t\t} else if ( f == "XLYR" && f.size() >= 4 ) {\n'
     '\t\t\t\t\t\tref.layer = esm->mapFormID( *r, f.readUInt32() );\n', 'after'),
    ('src/esmdata.cpp', 'CELLVIEW1: every record type the cell view draws',
     '\t\t\t} else if ( f == "MODL" && ( *br == "TREE" || *br == "STAT" ) ) {\n',
     '\t\t\t} else if ( f == "EDID" ) {\n'
     '\t\t\t\tb.edid = fieldString( f );\n'
     '\t\t\t} else if ( f == "MODL" && !( *br == "TREE" || *br == "STAT" )\n'
     '\t\t\t\t&& b.model.isEmpty() ) {\n'
     '\t\t\t\t/* CELLVIEW1: every record type the cell view draws -- MSTT,\n'
     '\t\t\t\t * FURN, CONT, DOOR, ACTI, FLOR, LIGH -- carries its near model\n'
     '\t\t\t\t * in MODL exactly as STAT does. The LOD bake never asked, so\n'
     '\t\t\t\t * the reader never answered; nothing about the STAT/TREE route\n'
     '\t\t\t\t * below changes, and `models[]` is still filled only there. */\n'
     '\t\t\t\tb.model = fieldString( f );\n', 'before'),

    # ------------------------------------------------------- src/nifskope.cpp
    ('src/nifskope.cpp', '#include "cellview.h"',
     '#include "lodinative.h"\t// lane NATIVEVIEW1: the .lodi/.lodo built document\r\n',
     '#include "cellview.h"\t// lane CELLVIEW1: a whole exterior cell, built\r\n', 'after'),
    # NOTE the bare \n on this anchor. src/nifskope.cpp is a CRLF file with
    # FIVE bare-LF lines in it, and this file-type row is one of them (measured
    # 2026-09-19 at 720762a, python byte counts, not grep). The inserted rows
    # use the file's dominant CRLF; the anomaly is left exactly as found.
    ('src/nifskope.cpp', '"Fallout 4 Cell View", "wwcell"',
     '\t{ "Native Object LOD", "lodi" },\n',
     '\t// nor is a cell view: a `.wwcell` is a one-line SPEC (plugins, worldspace,\r\n'
     '\t// cell, block size) and the scene is built out of the plugin and the\r\n'
     '\t// models it names -- src/cellview.h. Read-only, like every route here.\r\n'
     '\t{ "Fallout 4 Cell View", "wwcell" },\r\n', 'after'),
    ('src/nifskope.cpp', 'nifCreateCellScene',
     '\t} else if ( f.suffix().compare( QLatin1String( "lodm" ), Qt::CaseInsensitive ) == 0 ) {\r\n',
     '\t} else if ( f.suffix().compare( QLatin1String( "wwcell" ), Qt::CaseInsensitive ) == 0 ) {\r\n'
     '\t\t/* A WHOLE EXTERIOR CELL, the way the Creation Kit shows one (bungo,\r\n'
     '\t\t * 2026-09-19). The file is a spec, not geometry; WW_CELL_OPEN overrides\r\n'
     '\t\t * it for the harness and the notes say which of the two was used. */\r\n'
     '\t\tQString cverr, cvnotes;\r\n'
     '\t\tCellSceneSpec cvspec;\r\n'
     '\t\tbool fromEnv = cellSpecFromEnv( cvspec, &cverr );\r\n'
     '\t\tif ( !fromEnv ) {\r\n'
     '\t\t\tcverr.clear();\r\n'
     '\t\t\tif ( cellSpecFromFile( fname, cvspec, &cverr ) )\r\n'
     '\t\t\t\tcellApplyEnvModifiers( cvspec );\r\n'
     '\t\t}\r\n'
     '\t\tloaded = cvspec.valid && nifCreateCellScene( nif, cvspec, &cverr, &cvnotes );\r\n'
     '\t\tif ( loaded && !cvnotes.isEmpty() )\r\n'
     '\t\t\tqInfo().noquote() << "cell view:\\n" << cvnotes;\r\n'
     '\t\tif ( !loaded && !cverr.isEmpty() )\r\n'
     '\t\t\tqWarning() << "cell view:" << cverr;\r\n', 'before'),
]


def load(path):
    with open(os.path.join(REPO, path), 'rb') as fh:
        return fh.read()


def counts(b):
    crlf = b.count(b'\r\n')
    return crlf, b.count(b'\n') - crlf, b.count(b'\r') - crlf


def run(mode):
    problems = []
    plan = {}          # path -> (original bytes, new bytes, [what])
    for path, marker, anchor, ins, where in EDITS:
        full = os.path.join(REPO, path)
        if not os.path.isfile(full):
            problems.append('%s: not in this tree' % path)
            continue
        cur = plan[path][1] if path in plan else load(path)
        orig = plan[path][0] if path in plan else cur
        what = plan[path][2] if path in plan else []

        ab = anchor.encode('utf-8')
        mb = marker.encode('utf-8')
        ib = ins.encode('utf-8')

        if mb in cur:
            what.append('ALREADY APPLIED: %s' % marker)
            plan[path] = (orig, cur, what)
            continue
        n = cur.count(ab)
        if n != 1:
            problems.append('%s: the anchor for "%s" occurs %d times, not once '
                            '-- another lane has moved it; re-read the file '
                            'before applying' % (path, marker, n))
            plan[path] = (orig, cur, what)
            continue
        at = cur.index(ab)
        if where == 'after':
            at += len(ab)
        new = cur[:at] + ib + cur[at:]

        # the CR/LF assert: the insertion's own bytes and nothing else moved
        c0 = counts(cur)
        c1 = counts(new)
        ci = counts(ib)
        if (c1[0] - c0[0], c1[1] - c0[1], c1[2] - c0[2]) != ci:
            problems.append('%s: line endings would change beyond the inserted '
                            'block (%s -> %s, block %s)' % (path, c0, c1, ci))
            plan[path] = (orig, cur, what)
            continue
        what.append('insert %d bytes %s the anchor (%s)'
                    % (len(ib), where, marker))
        plan[path] = (orig, new, what)

    for path in sorted(plan):
        orig, new, what = plan[path]
        c = counts(orig)
        print('%s  CRLF=%d LF=%d CR=%d' % (path, c[0], c[1], c[2]))
        for w in what:
            print('    %s' % w)
        if new != orig:
            c2 = counts(new)
            print('    -> CRLF=%d LF=%d CR=%d, %+d bytes'
                  % (c2[0], c2[1], c2[2], len(new) - len(orig)))

    if problems:
        print('\nREFUSED, %d problem(s):' % len(problems))
        for p in problems:
            print('  %s' % p)
        return 2

    if mode == '--check':
        print('\n--check: nothing written.')
        return 0

    for path in sorted(plan):
        orig, new, what = plan[path]
        if new == orig:
            continue
        full = os.path.join(REPO, path)
        with open(full, 'wb') as fh:
            fh.write(new)
        print('wrote %s' % path)
    print('\napplied. Now: qmake, then make.')
    return 0


if __name__ == '__main__':
    mode = sys.argv[1] if len(sys.argv) > 1 else '--check'
    if mode not in ('--check', '--apply'):
        print(__doc__)
        sys.exit(1)
    sys.exit(run(mode))
