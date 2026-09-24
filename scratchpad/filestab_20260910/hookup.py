#!/usr/bin/env python3
"""Lane FILESTAB's hook-up: the edits four files that ANOTHER LANE OWNS need so
that this lane's new files (src/filestab.{h,cpp}, src/filestabtest.cpp) join the
build and the renames land.

Lane BUILD8 is writing src/nifskope.cpp, src/nifskope_ui.cpp, NifSkope.pro and
src/gltfexport.* while this lane runs, so nothing here is applied by the lane
that wrote it (CONSTITUTION 1, "one lane per file"; ww-anchored-hookup).

    python scratchpad/filestab_20260910/hookup.py            # --check, writes NOTHING
    python scratchpad/filestab_20260910/hookup.py --apply    # all files at once

--check prints, per edit, the number of times its anchor matches (it must be
exactly 1) and the file's CR count before and after; a single anchor that does
not match once refuses the whole run and writes nothing.

LINE ENDINGS. src/nifskope.cpp is MIXED -- CRLF with LF blocks -- so every
inserted line takes the ending of the line it is anchored to, read out of the
file's real bytes, and the run asserts the CR delta equals the number of CRLF
lines it inserted. src/nifskope.h, src/nifskope_ui.cpp, src/ui/nifskope.ui and
NifSkope.pro are LF-only and stay so.

APPLIED OR NOT is decided by a MARKER, never by an anchor (ww-anchored-hookup
section 4): every inserted block carries "lane FILESTAB", and --check reports
whether the marker is already in the file.
"""

import io
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
MARKER = 'lane FILESTAB'


# --------------------------------------------------------------- the edits ---
# ("path", "replace"|"after"|"before", anchor, payload, expected_count)
#   replace: anchor and payload are STRINGS with no line ending; every one of
#            `expected_count` occurrences is substituted.
#   after / before: anchor is a LIST of consecutive whole lines (no endings);
#            payload is a LIST of lines, written with the anchor's own ending.

RENAMES_CPP = [
    # --- the browser's compact row -------------------------------------------
    ('"Search NIFs..."', '"Search files..."', 1),
    ('"Show favorite NIFs only"', '"Show favorite files only"', 1),
    ('"Choose which NIF sources are shown"', '"Choose which file sources are shown"', 1),
    ('addAction( tr( "Loose NIFs" ) )', 'addAction( tr( "Loose files" ) )', 1),
    ('"Load every selected NIF as a document"',
     '"Load every selected file as a document"', 2),
    ('"Reload available NIFs from the resource paths configured in Settings"',
     '"Reload available files from the resource paths configured in Settings"', 2),
    # --- the tree ------------------------------------------------------------
    ('new QStandardItem( tr( "Available NIFs" ) )',
     'new QStandardItem( tr( "Available files" ) )', 1),
    ('tr( "Loose NIF" ) : tr( "Archive NIF" )',
     'tr( "Loose file" ) : tr( "Archive file" )', 1),
    ('"No configured NIF resources for %1"', '"No configured file resources for %1"', 1),
    ('tr( "NIF Browser Favorites" )', 'tr( "File Browser Favorites" )', 2),
    # --- the Loaded list -----------------------------------------------------
    ('setHorizontalHeaderLabels( { tr( "Loaded NIFs ',
     'setHorizontalHeaderLabels( { tr( "Loaded files ', 1),
    ('"Search loaded NIFs', '"Search loaded files', 1),
    ('title = tr( "Loaded NIFs ', 'title = tr( "Loaded files ', 2),
    ('title = tr( "Loaded NIF ', 'title = tr( "Loaded file ', 1),
    ('"Drag a NIF here, or right-click to add files."',
     '"Drag a file here, or right-click to add files."', 1),
    ('"No loaded NIFs match ', '"No loaded files match ', 1),
    ('"Drag one Loaded NIF at a time to save it"',
     '"Drag one loaded file at a time to save it"', 1),
    ('"The skeleton for Loaded NIFs', '"The skeleton for Loaded files', 1),
    ('"Use as the skeleton for Loaded NIFs', '"Use as the skeleton for Loaded files', 1),
    ('"Loaded NIFs row marks ', '"Loaded files row marks ', 1),
    # --- the four context menus ---------------------------------------------
    ('addAction( tr( "Open NIF" ) )', 'addAction( tr( "Open" ) )', 1),
    ('"Open NIF in New Window"', '"Open in New Window"', 1),
    ('"Add %1 Selected to Loaded NIFs"', '"Add %1 Selected to Loaded files"', 1),
    (': tr( "Add to Loaded NIFs" ) );', ': tr( "Add to Loaded files" ) );', 1),
    ('adaptiveText = tr( "Add to Loaded NIFs" );',
     'adaptiveText = tr( "Add to Loaded files" );', 1),
    ('"Add NIF to Loaded NIFs', '"Add file to Loaded files', 2),
    ('"Load any NIF from disk as a workspace document"',
     '"Load any file from disk as a workspace document"', 1),
    # two menu items and one comment at :921 that quotes the menu item by name
    ('"Use as Skeleton for Loaded NIFs"', '"Use as Skeleton for Loaded files"', 3),
    ('"Every other loaded NIF evaluates its bones against this file, "',
     '"Every other loaded file evaluates its bones against this file, "', 2),
    ('"Show All Secondary NIFs"', '"Show All Secondary Files"', 2),
    ('"Hide All Secondary NIFs"', '"Hide All Secondary Files"', 2),
    ('"Remove from Loaded NIFs"', '"Remove from Loaded files"', 2),
    ('"Remove %1 from Loaded NIFs', '"Remove %1 from Loaded files', 1),
    ('"Write this loaded NIF to a file"', '"Write this loaded file to disk"', 1),
    ('"Loaded NIFs unsaved, to be saved wherever you want it."',
     '"Loaded files unsaved, to be saved wherever you want it."', 1),
    ('tr( "Revert Loaded NIF" )', 'tr( "Revert Loaded File" )', 1),
    ('"Reverting %1 discards every unsaved change in that loaded NIF.',
     '"Reverting %1 discards every unsaved change in that loaded file.', 1),
    ('tr( "Add NIFs to Loaded NIFs" )', 'tr( "Add files to Loaded files" )', 1),
    ('tr( "Add NIFs" ),', 'tr( "Add files" ),', 1),
    ('"Added %1 NIF(s) to Loaded NIFs"', '"Added %1 file(s) to Loaded files"', 1),
    ('tr( "Unsaved Loaded NIF" ),', 'tr( "Unsaved loaded file" ),', 1),
    ('"%1 has unsaved changes. Removing it from Loaded NIFs permanently "',
     '"%1 has unsaved changes. Removing it from Loaded files permanently "', 1),
    # --- status lines and the external drop menu -----------------------------
    ('"Could not load %1 into the Loaded NIFs workspace."',
     '"Could not load %1 into the Loaded files workspace."', 1),
    ('"Finished loading background NIFs"', '"Finished loading background files"', 1),
    ('"Loading background NIFs... %1 remaining"',
     '"Loading background files... %1 remaining"', 1),
    ('"%1 dropped NIF(s) ready in this workspace."',
     '"%1 dropped file(s) ready in this workspace."', 1),
    ('"Open First Here; Add Rest to Loaded NIFs"',
     '"Open First Here; Add Rest to Loaded files"', 1),
    ('"The current document is the clean starter. Open the first NIF here; "',
     '"The current document is the clean starter. Open the first file here; "', 1),
    ('"additional NIFs stay in Loaded NIFs." )',
     '"additional files stay in Loaded files." )', 1),
    ('"Keep the current document and all unsaved work; add every dropped NIF "',
     '"Keep the current document and all unsaved work; add every dropped file "', 1),
]

RENAMES_UI_CPP = [
    ('addTab( tr( "NIFs" ) )', 'addTab( tr( "Files" ) )', 1),
    # longest first: the three shorter forms are substrings of nothing else once
    # this one is gone
    ('"Drag to resize NIF Browser and Loaded NIFs"',
     '"Drag to resize File Browser and Loaded files"', 3),
    ('"Resize NIF Browser and Loaded NIFs"', '"Resize File Browser and Loaded files"', 2),
    ('"NIF Browser and Loaded NIFs splitter"',
     '"File Browser and Loaded files splitter"', 1),
    ('"NIF Browser and Loaded NIFs" )', '"File Browser and Loaded files" )', 1),
]

RENAMES_UI_XML = [
    ('<string>NIF Browser</string>', '<string>File Browser</string>', 2),
]


HKX_ROUTE = [
    '',
    '\t/* AN .hkx IS AN ANIMATION, NOT A DOCUMENT (lane FILESTAB, bungo 2026-09-10).',
    '\t *',
    '\t * Opening it the way a .nif is opened would REPLACE the model it is meant',
    '\t * to play on, which is the one thing it must not do. It goes instead to the',
    '\t * same HkxPlayback::load() the render toolbar\'s "Load Animation (.hkx)..."',
    '\t * button calls, and answers with the same summary sentence.',
    '\t *',
    '\t * A row inside a .ba2 has no disk path, so its bytes are extracted here and',
    '\t * staged to a temporary file; a LOOSE row keeps its real path, which is what',
    '\t * lets the skeleton search look beside the clip and in CharacterAssets above',
    '\t * it. With nothing open to play it on, wwFilesTabOpenAnimation refuses in',
    '\t * words and loads nothing.',
    '\t */',
    '\tif ( wwFilesTabIsAnimation( filepath ) ) {',
    '\t\tQString clipDisk;',
    '\t\tQByteArray clipBytes;',
    '\t\tif ( source == NifBrowserLooseFile ) {',
    '\t\t\tclipDisk = filepath;',
    '\t\t} else if ( currentArchive ) {',
    '\t\t\tconst std::string key( filepath.toLower().toStdString() );',
    '\t\t\tif ( currentArchive->findFile( key ) ) {',
    '\t\t\t\tBA2File::UCharArray store;',
    '\t\t\t\tconst unsigned char * ptr = nullptr;',
    '\t\t\t\tconst size_t n = currentArchive->extractFile( ptr, store, key );',
    '\t\t\t\tclipBytes = QByteArray( reinterpret_cast<const char *>( ptr ), qsizetype( n ) );',
    '\t\t\t}',
    '\t\t}',
    '\t\tconst WwAnimOpen r = wwFilesTabOpenAnimation( ogl, clipDisk, clipBytes, filepath );',
    '\t\tif ( ui && ui->statusbar )',
    '\t\t\tui->statusbar->showMessage( r.sentence, 12000 );',
    '\t\tif ( r.loaded )',
    '\t\t\trebuildLoadedNifsBrowserGroup();',
    '\t\telse',
    '\t\t\tQMessageBox::information( this, tr( "Load Animation" ), r.sentence );',
    '\t\treturn r.loaded;',
    '\t}',
]

CLIP_ROWS = [
    '\t/* LOADED ANIMATIONS SIT BESIDE THE MODELS (lane FILESTAB, bungo 2026-09-10:',
    '\t * "rename Loaded NIFs to Loaded Files").',
    '\t *',
    '\t * A clip is not a document -- it has no NifModel and no window -- so it',
    '\t * carries neither of the document roles and the row delegate finds no marks',
    '\t * to draw on it. What tells it apart at a glance is its own icon, a play',
    '\t * triangle in the skin\'s toggle colour, and its row menu is the one place',
    '\t * HkxPlayback::unload() is reachable from.',
    '\t */',
    '\tif ( ogl ) {',
    '\t\tScene * clipScene = ogl->getScene();',
    '\t\tif ( clipScene && clipScene->hkx ) {',
    '\t\t\tconst QString playing = clipScene->hkx->activeName();',
    '\t\t\tconst QStringList clipNames = clipScene->hkx->names();',
    '\t\t\tfor ( const QString & clipName : clipNames ) {',
    '\t\t\t\tauto * row = new QStandardItem( clipName );',
    '\t\t\t\trow->setEditable( false );',
    '\t\t\t\trow->setDragEnabled( false );',
    '\t\t\t\trow->setDropEnabled( false );',
    '\t\t\t\trow->setIcon( wwFilesTabClipIcon() );',
    '\t\t\t\trow->setData( clipName, NifBrowserClipRole );',
    '\t\t\t\tQStringList clipTip;',
    '\t\t\t\tclipTip << ( clipName == playing',
    '\t\t\t\t\t? tr( "Loaded animation \\xE2\\x80\\x94 playing" )',
    '\t\t\t\t\t: tr( "Loaded animation \\xE2\\x80\\x94 right-click to play or unload it" ) );',
    '\t\t\t\tif ( const HkxClipEntry * e = clipScene->hkx->find( clipName );',
    '\t\t\t\t\t e && !e->path.isEmpty() )',
    '\t\t\t\t\tclipTip << QDir::toNativeSeparators( e->path );',
    '\t\t\t\trow->setToolTip( clipTip.join( QLatin1Char( \'\\n\' ) ) );',
    '\t\t\t\tloadedNifsModel->appendRow( row );',
    '\t\t\t}',
    '\t\t}',
    '\t}',
]

CLIP_MENU_BRANCH = [
    '\t\t\t// lane FILESTAB: a loaded ANIMATION row. Not a document, so neither of',
    '\t\t\t// the two lookups above finds it, and it gets its own two-item menu.',
    '\t\t\telse if ( !index.data( NifBrowserClipRole ).toString().isEmpty() )',
    '\t\t\t\tshowLoadedClipMenu( index.data( NifBrowserClipRole ).toString(),',
    '\t\t\t\t\tloadedNifsView->viewport()->mapToGlobal( pos ) );',
]

CLIP_METHODS = [
    '/*! The row menu of a loaded ANIMATION in the Loaded-files list.',
    ' *',
    ' *  Lane FILESTAB. Two items and no more: play it, or drop it. Unload is the',
    ' *  only caller of HkxPlayback::unload() in the program -- the restore it does',
    ' *  is byte-exact, and tests/spells/files_tab.sh gate (4) is that claim.',
    ' */',
    'void NifSkope::showLoadedClipMenu( const QString & clipName, const QPoint & globalPos )',
    '{',
    '\tif ( !ogl ) return;',
    '\tScene * sc = ogl->getScene();',
    '\tif ( !sc || !sc->hkx || !sc->hkx->has( clipName ) ) return;',
    '',
    '\tQMenu menu( this );',
    '\tQAction * play = menu.addAction( tr( "Play This Animation" ) );',
    '\tplay->setEnabled( sc->hkx->activeName() != clipName );',
    '\tplay->setToolTip( tr( "Make it the sequence the transport plays" ) );',
    '\tmenu.addSeparator();',
    '\tQAction * drop = menu.addAction( tr( "Unload Animation" ) );',
    '\tdrop->setToolTip( tr( "Drop this clip and put every bone it posed back "',
    '\t\t"exactly as it was" ) );',
    '\tmenu.setToolTipsVisible( true );',
    '',
    '\tQAction * chosen = menu.exec( globalPos );',
    '\tif ( chosen == play ) {',
    '\t\togl->setSceneSequence( clipName );',
    '\t\trebuildLoadedNifsBrowserGroup();',
    '\t} else if ( chosen == drop ) {',
    '\t\tif ( wwFilesTabUnloadAnimation( ogl, clipName ) )',
    '\t\t\trebuildLoadedNifsBrowserGroup();',
    '\t}',
    '}',
    '',
    '/*! WW_FILESTAB_TEST seam: show the Files page and rebuild its tree NOW.',
    ' *',
    ' *  A harness forces the state it measures. The populate is otherwise deferred',
    ' *  while the left editor is on another page, so a harness that only set the',
    ' *  resource roots would census an empty tree and pass nothing.',
    ' */',
    'void NifSkope::wwFilesTabShowAndRebuild()',
    '{',
    '\tsetLeftColumnMode( LeftNifs );',
    '\tif ( dLeft ) dLeft->show();',
    '\tnifBrowserIndexSignature.clear();',
    '\tnifBrowserTreeSignature.clear();',
    '\tpopulateConfiguredNifBrowserNow();',
    '}',
    '',
    '/*! WW_FILESTAB_TEST seam: put one row in the Files tree and open it exactly',
    ' *  as a double-click on that row does -- a real row with the real role data,',
    ' *  through the view\'s own doubleClicked signal, so the gate exercises the',
    ' *  route the user takes and not a private back door. */',
    'void NifSkope::wwFilesTabOpenRow( const QString & diskPath )',
    '{',
    '\tif ( !bsaModel || !bsaProxyModel || !bsaView ) return;',
    '\tauto * name = new QStandardItem( QFileInfo( diskPath ).fileName() );',
    '\tname->setData( NifBrowserLooseFile, NifBrowserSourceRole );',
    '\tauto * path = new QStandardItem( diskPath );',
    '\tpath->setData( NifBrowserLooseFile, NifBrowserSourceRole );',
    '\tbsaModel->appendRow( { name, path, new QStandardItem() } );',
    '\tconst QModelIndex proxy = bsaProxyModel->mapFromSource( name->index() );',
    '\tif ( proxy.isValid() )',
    '\t\temit bsaView->doubleClicked( proxy );',
    '}',
    '',
]

HEADER_DEFINE = [
    '',
    '/* lane FILESTAB (2026-09-10): the Files tab. Defined by the hook-up so that',
    ' * src/filestabtest.cpp compiles both BEFORE the hook-up (its two seam calls',
    ' * are skipped BY NAME in the log) and after it (they run). */',
    '#define WW_FILESTAB_HOOKUP 1',
]

HEADER_DECLS = [
    '\t//! lane FILESTAB: the row menu of a loaded ANIMATION in the Loaded-files',
    '\t//! list -- play it, or unload it and restore every bone it posed.',
    '\tvoid showLoadedClipMenu( const QString & clipName, const QPoint & globalPos );',
    '\t//! WW_FILESTAB_TEST seam: show the Files page and rebuild its tree now.',
    '\tvoid wwFilesTabShowAndRebuild();',
    '\t//! WW_FILESTAB_TEST seam: add one row to the Files tree and open it the',
    '\t//! way a double-click does.',
    '\tvoid wwFilesTabOpenRow( const QString & diskPath );',
]

EDITS = [
    # ---------------------------------------------------------- NifSkope.pro --
    ('NifSkope.pro', 'after', ['\tsrc/hkxplaybacktest.cpp \\'],
     ['\tsrc/filestab.cpp \\', '\tsrc/filestabtest.cpp \\'], 1),
    ('NifSkope.pro', 'after', ['\tsrc/hkxplayback.h \\'],
     ['\tsrc/filestab.h \\'], 1),

    # ---------------------------------------------------------- nifskope.h ----
    ('src/nifskope.h', 'after', ['#include "btdterrain.h"'], HEADER_DEFINE, 1),
    ('src/nifskope.h', 'after',
     ['\tbool addWorkspaceDocumentFromFile( const QString & path );'],
     HEADER_DECLS, 1),

    # ---------------------------------------------------------- nifskope.cpp --
    ('src/nifskope.cpp', 'after', ['#include "wwskin.h"'],
     ['#include "filestab.h"\t// lane FILESTAB: the Files tab\'s extensions and .hkx route',
      '#include "hkxplayback.h"'], 1),
    ('src/nifskope.cpp', 'after',
     ['constexpr int NifBrowserFavouriteRole = Qt::UserRole + 42;'],
     ['//! lane FILESTAB: a Loaded-files row that is a loaded ANIMATION, by name.',
      'constexpr int NifBrowserClipRole = Qt::UserRole + 43;'], 1),
    ('src/nifskope.cpp', 'replace',
     '\treturn ( s.ends_with( ".nif" ) || s.ends_with( ".bto" ) || s.ends_with( ".btr" ) );',
     '\treturn wwFilesTabAccepts( s );\t// lane FILESTAB: one list, in src/filestab.cpp',
     1),
    # the same line occurs in three functions; the line above it says which one
    # (the other two declare `const QModelIndex nameIndex`)
    ('src/nifskope.cpp', 'after',
     ['\tQModelIndex nameIndex = index.sibling( index.row(), 0 );',
      '\tconst int source = nameIndex.data( NifBrowserSourceRole ).toInt();'],
     HKX_ROUTE, 1),
    ('src/nifskope.cpp', 'before',
     ['\tif ( loadedNifsView ) {',
      '\t\tloadedNifsView->header()->setStretchLastSection( true );'],
     CLIP_ROWS, 1),
    ('src/nifskope.cpp', 'after',
     ['\t\t\telse if ( BackgroundNifDocument * background = backgroundDocumentFromBrowserIndex( index ) )',
      '\t\t\t\tshowBackgroundDocumentMenu( background, loadedNifsView->viewport()->mapToGlobal( pos ) );'],
     CLIP_MENU_BRANCH, 1),
    ('src/nifskope.cpp', 'before', ['void NifSkope::rebuildLoadedNifsBrowserGroup()'],
     CLIP_METHODS, 1),

    # ------------------------------------------------------- nifskope_ui.cpp --
    ('src/nifskope_ui.cpp', 'after', ['#include "hkxplayback.h"'],
     ['#include "filestab.h"'], 1),
    ('src/nifskope_ui.cpp', 'after', ['\twwHkxAnimHarness( skope );'],
     ['',
      '\t// TEST HARNESS (WW_FILESTAB_TEST=1): lane FILESTAB\'s Files-tab gates.',
      '\t// The whole harness is src/filestabtest.cpp; this is its only line here.',
      '\twwFilesTabHarness( skope );'], 1),
]

for old, new, n in RENAMES_CPP:
    EDITS.append(('src/nifskope.cpp', 'replace', old, new, n))
for old, new, n in RENAMES_UI_CPP:
    EDITS.append(('src/nifskope_ui.cpp', 'replace', old, new, n))
for old, new, n in RENAMES_UI_XML:
    EDITS.append(('src/ui/nifskope.ui', 'replace', old, new, n))


# ------------------------------------------------------------------ engine ---

def read(path):
    with io.open(os.path.join(REPO, path), 'rb') as f:
        return f.read()


def write(path, data):
    with io.open(os.path.join(REPO, path), 'wb') as f:
        f.write(data)


def locate_block(data, lines):
    """Find the byte range of `lines` as consecutive whole lines.

    Returns (start, end_of_block_including_its_terminator, eol_of_first_line,
    eol_of_last_line, count). CRLF and LF are both tried and the total match
    count across the two is what has to be 1, because src/nifskope.cpp is mixed.
    """
    hits = []
    for eol in (b'\r\n', b'\n'):
        joined = eol.join(l.encode('utf-8') for l in lines) + eol
        start = data.find(joined)
        while start >= 0:
            # a whole-line match: it must start at a line boundary
            if start == 0 or data[start - 1:start] == b'\n':
                hits.append((start, start + len(joined), eol))
            start = data.find(joined, start + 1)
    return hits


def apply_edits(dry):
    files = {}
    for path, mode, anchor, payload, count in EDITS:
        if path not in files:
            files[path] = read(path)

    before_cr = {p: b.count(b'\r') for p, b in files.items()}
    before_len = {p: len(b) for p, b in files.items()}
    predicted_cr = {p: 0 for p in files}
    bad = 0
    already = {p: (MARKER.encode() in b) for p, b in files.items()}

    for path, mode, anchor, payload, count in EDITS:
        data = files[path]
        if mode == 'replace':
            a = anchor.encode('utf-8')
            n = data.count(a)
            ok = (n == count)
            print('%-22s replace x%-2d found %-3d %s  %r' %
                  (path, count, n, 'ok  ' if ok else 'REFUSE', anchor[:64]))
            if not ok:
                bad += 1
                continue
            data = data.replace(a, payload.encode('utf-8'))
            assert b'\n' not in payload.encode('utf-8')
        else:
            hits = locate_block(data, anchor)
            ok = (len(hits) == count == 1)
            print('%-22s %-6s     found %-3d %s  %r' %
                  (path, mode, len(hits), 'ok  ' if ok else 'REFUSE', anchor[0][:64]))
            if not ok:
                bad += 1
                continue
            start, end, eol = hits[0]
            block = eol.join(l.encode('utf-8') for l in payload) + eol
            at = end if mode == 'after' else start
            data = data[:at] + block + data[at:]
            if eol == b'\r\n':
                predicted_cr[path] += len(payload)
        files[path] = data

    print('')
    for p in sorted(files):
        after_cr = files[p].count(b'\r')
        delta = after_cr - before_cr[p]
        flag = 'ok' if delta == predicted_cr[p] else 'CR DELTA WRONG'
        print('%-22s bytes %d -> %d   CR %d -> %d (predicted +%d)  %s%s' %
              (p, before_len[p], len(files[p]), before_cr[p], after_cr,
               predicted_cr[p], flag,
               '   [MARKER ALREADY PRESENT BEFORE THIS RUN]' if already[p] else ''))
        if delta != predicted_cr[p]:
            bad += 1

    if bad:
        print('\n%d edit(s) refused -- NOTHING WAS WRITTEN.' % bad)
        return 1
    if dry:
        print('\n--check: every anchor matched as declared. Nothing written.')
        return 0
    for p, b in files.items():
        write(p, b)
    print('\n--apply: %d file(s) written.' % len(files))
    return 0


if __name__ == '__main__':
    sys.exit(apply_edits('--apply' not in sys.argv))
