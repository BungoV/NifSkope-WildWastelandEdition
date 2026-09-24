#!/usr/bin/env python
# Lane UINOTES1b -- the SECOND repair of the gate file, 2026-09-12.
#
# Everything here changes src/animworkspacetest.cpp ONLY. Not one line of
# product behaviour is touched: these are the gate's own defects, found by
# running it (the brief: fix expected literals and gate bugs, never change
# behaviour to make a gate pass).
#
# It refuses rather than guesses: every anchor must match exactly once, every
# string replacement must have the count it expects, the file must be pure LF
# on entry and on exit, and nothing is written until every edit has been made
# in memory.
import sys

P = r"E:\Projects\NifskopeWildWastelandEdition\src\animworkspacetest.cpp"

def die(msg):
    sys.stdout.write("REFUSED: " + msg + "\n")
    sys.exit(3)

raw = open(P, "rb").read()
if b"\r" in raw:
    die("the file has CR bytes on entry; this script only writes LF")
lines = raw.decode("utf-8").split("\n")
n_in = len(lines)

def tabify(block):
    """Four spaces = one tab, which is how this file is indented. A line whose
    indent is NOT a multiple of four is a continuation -- the ' *' of a block
    comment, or the '.arg(' lined up under a check()'s first argument -- and it
    keeps the tab depth of the line above it, with the rest as real spaces."""
    out = []
    tabs = 0
    for ln in block.split("\n"):
        s = ln.lstrip(" ")
        ind = len(ln) - len(s)
        if not s:
            out.append("")
            continue
        if ind % 4 == 0:
            tabs = ind // 4
            out.append("\t" * tabs + s)
        else:
            pad = ind - 4 * tabs
            if pad < 1:
                die("a continuation line is indented less than the line above it: " + repr(ln))
            out.append("\t" * tabs + " " * pad + s)
    return out

def find_block(anchor, label):
    a = [l.strip() for l in anchor.strip("\n").split("\n")]
    k = len(a)
    hits = [i for i in range(len(lines) - k + 1)
            if [l.strip() for l in lines[i:i + k]] == a]
    if len(hits) != 1:
        die("%s: the anchor matched %d times, it must match exactly once" % (label, len(hits)))
    return hits[0], k

def rep(label, anchor, new):
    i, k = find_block(anchor, label)
    lines[i:i + k] = tabify(new.strip("\n"))
    sys.stdout.write("  ok  %s\n" % label)

def ins_after(label, anchor, new):
    i, k = find_block(anchor, label)
    lines[i + k:i + k] = tabify(new.strip("\n"))
    sys.stdout.write("  ok  %s\n" % label)

# ---------------------------------------------------------------- 1. include
rep("(1) QEventLoop is included",
"""
#include <QDoubleSpinBox>
#include <QFile>
""",
"""
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QFile>
""")

# ------------------------------------------------- 2. settle + selectRowNamed
rep("(2) settle() and a selection that survives the rebuild",
"""
                auto selectRowNamed = [&]( const QString & name ) {
                    for ( int i = 0; i < list->count(); i++ )
                        if ( list->item( i )->data( Qt::UserRole ).toString() == name ) {
                            list->clearSelection();
                            list->setCurrentRow( i );
                            list->item( i )->setSelected( true );
                            return true;
                        }
                    return false;
                };
""",
"""
                /* THE LIST REBUILDS 50 ms LATE (measured by lane UINOTES1b,
                 * 2026-09-12). Everything that goes through the hub -- delete,
                 * cut, rename, and SELECTING a row, which activates it --
                 * finishes in WwHkxAnimHub::clipsChanged, and
                 * AnimWorkspace::clipsChanged() ends with refreshLater(): a
                 * 50 ms single-shot timer (src/animworkspace.cpp:261-264,
                 * 1192-1241). A single processEvents() cannot see past it, so
                 * a gate that reads the rows straight after the action reads
                 * the OLD rows. Paste and Duplicate call refresh() themselves,
                 * which is exactly why those two used to pass and Cut, Del and
                 * rename used to fail. settle() runs the event loop for 120 ms
                 * -- the same wait a hand makes without noticing -- so the gate
                 * reads what the eye would see. */
                auto settle = [&]() {
                    QEventLoop loop;
                    QTimer::singleShot( 120, &loop, &QEventLoop::quit );
                    loop.exec();
                    qApp->processEvents();
                };
                auto selectOnce = [&]( const QString & name ) {
                    for ( int i = 0; i < list->count(); i++ )
                        if ( list->item( i )->data( Qt::UserRole ).toString() == name ) {
                            list->clearSelection();
                            list->setCurrentRow( i );
                            list->item( i )->setSelected( true );
                            return true;
                        }
                    return false;
                };
                // selecting a row activates it, which starts that same 50 ms
                // rebuild; let it land and put the selection back on the row
                // the rebuild made, so the action lands on the row the eye sees
                auto selectRowNamed = [&]( const QString & name ) {
                    if ( !selectOnce( name ) )
                        return false;
                    settle();
                    return selectOnce( name );
                };
""")

# ------------------------------------------------------------ 3. liveNames()
ins_after("(3) liveNames(): the rows that really have a clip behind them",
"""
                auto clipNames = [&]() {
                    QStringList out;
                    for ( int i = 0; i < list->count(); i++ )
                        if ( list->item( i )->data( Qt::UserRole + 1 ).toBool() )
                            out << list->item( i )->data( Qt::UserRole ).toString();
                    return out;
                };
""",
"""
                /* THE DOCK'S CLIP ROWS ARE NOT THE SCENE'S ANIMATIONS LIST.
                 * A file that loaded but is not a clip -- skeleton.hkx is the
                 * standard one, and the fixture ships it -- is shown as a
                 * refused row and is deliberately kept OUT of
                 * Scene::animGroups (src/hkxanimui.h, lines 20-33). So a
                 * row-for-row comparison of the two lists uses the rows that
                 * really have a clip behind them, which is what animGroups
                 * holds. A gate that compared all four rows against three
                 * names could never pass. */
                auto liveNames = [&]() {
                    QStringList out;
                    for ( const QString & n : clipNames() )
                        if ( pb->find( n ) )
                            out << n;
                    return out;
                };
""")

# ---------------------------------------------------------------- 4. fire()
rep("(4) fire() waits for the rebuild it caused",
"""
                auto fire = [&]( const char * name ) {
                    auto * s = list->findChild<QShortcut *>( QString::fromLatin1( name ) );
                    if ( s )
                        QMetaObject::invokeMethod( s, "activated" );
                    qApp->processEvents();
                    return s != nullptr;
                };
""",
"""
                auto fire = [&]( const char * name ) {
                    auto * s = list->findChild<QShortcut *>( QString::fromLatin1( name ) );
                    if ( s )
                        QMetaObject::invokeMethod( s, "activated" );
                    settle();
                    return s != nullptr;
                };
""")

# ------------------------------------------- 5. the scene's list, first check
rep("(5) the scene's list carries the clip rows",
"""
                check( *st, QStringLiteral( "(o) the scene's own animations list carries the same three, in the same order: %1" ).arg( sc->animGroups.join( QStringLiteral( " | " ) ) ),
                       sc && sc->animGroups.mid( sc->animGroups.count() - 3 ) == three );
""",
"""
                const QStringList threeLive = liveNames();
                check( *st, QStringLiteral( "(o) the scene's own animations list carries the same three, in the same order: %1 (the rows with a clip behind them: %2)" )
                       .arg( sc->animGroups.join( QStringLiteral( " | " ) ), threeLive.join( QStringLiteral( " | " ) ) ),
                       sc && threeLive.count() >= 3 && sc->animGroups.mid( sc->animGroups.count() - threeLive.count() ) == threeLive );
""")

# ------------------------------------------------------------- 6. Ctrl+A
rep("(6) Ctrl+A is read three times",
"""
                // ---- Ctrl+A
                fire( "AnimWsListSelectAll" );
                check( *st, QStringLiteral( "(o) Ctrl+A selects every row: %1 of %2" ).arg( list->selectedItems().count() ).arg( list->count() ),
                       list->selectedItems().count() == list->count() && list->count() >= 3 );
""",
"""
                /* ---- Ctrl+A, read at three moments, because a rebuild keeps
                 * only the current row: straight after the key, after one pump
                 * of the event loop, and after the 50 ms rebuild has landed.
                 * The check is on the FIRST number -- what the key itself did.
                 * The other two are printed, so a selection that is eaten
                 * afterwards shows up as a number instead of an opinion. */
                {
                    settle();
                    auto * sa = list->findChild<QShortcut *>( QStringLiteral( "AnimWsListSelectAll" ) );
                    int selNow = -1, selPumped = -1, selSettled = -1;
                    if ( sa ) {
                        QMetaObject::invokeMethod( sa, "activated" );
                        selNow = list->selectedItems().count();
                        qApp->processEvents();
                        selPumped = list->selectedItems().count();
                        settle();
                        selSettled = list->selectedItems().count();
                    }
                    check( *st, QStringLiteral( "(o) Ctrl+A selects every row: %1 of %2 (one pump later %3, once the list rebuilds %4)" )
                           .arg( selNow ).arg( list->count() ).arg( selPumped ).arg( selSettled ),
                           sa && selNow == list->count() && list->count() >= 3 );
                }
""")

# ------------------------------------------ 7. the scene's list after the move
rep("(7) the scene's list followed the move",
"""
                check( *st, QStringLiteral( "(o) the scene's list followed the move as well: %1" ).arg( sc->animGroups.join( QStringLiteral( " | " ) ) ),
                       sc && sc->animGroups.mid( sc->animGroups.count() - 3 ) == three );
""",
"""
                const QStringList upLive = liveNames();
                check( *st, QStringLiteral( "(o) the scene's list followed the move as well: %1 (the rows with a clip behind them: %2)" )
                       .arg( sc->animGroups.join( QStringLiteral( " | " ) ), upLive.join( QStringLiteral( " | " ) ) ),
                       sc && upLive == threeLive && sc->animGroups.mid( sc->animGroups.count() - upLive.count() ) == upLive );
""")

# --------------------------------------------- 8. the menu-driven Move down
rep("(8) the menu-driven Move down waits for the rebuild",
"""
                    QContextMenuEvent cme3( QContextMenuEvent::Mouse, mp, list->viewport()->mapToGlobal( mp ) );
                    QApplication::sendEvent( list->viewport(), &cme3 );
                    qApp->processEvents();
                    const QStringList afterMenuMove = clipNames();
""",
"""
                    QContextMenuEvent cme3( QContextMenuEvent::Mouse, mp, list->viewport()->mapToGlobal( mp ) );
                    QApplication::sendEvent( list->viewport(), &cme3 );
                    settle();
                    const QStringList afterMenuMove = clipNames();
""")

# ------------------------------------------------------------- 9. the drop
rep("(9) the drop waits, and is weighed against the rows that have clips",
"""
                        QApplication::sendEvent( list->viewport(), &de );
                        qApp->processEvents();
                        qApp->processEvents();
                        const QStringList afterDrop = clipNames();
                        check( *st, QStringLiteral( "(o) the drop wrote the new order into the clips themselves: %1 -> %2" )
                               .arg( beforeDrop.join( QStringLiteral( " | " ) ), afterDrop.join( QStringLiteral( " | " ) ) ),
                               afterDrop.count() == beforeDrop.count() && afterDrop != beforeDrop
                               && afterDrop == pb->names().mid( pb->names().count() - beforeDrop.count() ) );
                        check( *st, QStringLiteral( "(o) ...and the scene's own list with it: %1" ).arg( sc->animGroups.join( QStringLiteral( " | " ) ) ),
                               sc && sc->animGroups.mid( sc->animGroups.count() - afterDrop.count() ) == afterDrop );
""",
"""
                        QApplication::sendEvent( list->viewport(), &de );
                        settle();
                        const QStringList afterDrop = clipNames();
                        const QStringList afterLive = liveNames();
                        check( *st, QStringLiteral( "(o) the drop wrote the new order into the clips themselves: %1 -> %2 (the rows with a clip behind them: %3)" )
                               .arg( beforeDrop.join( QStringLiteral( " | " ) ), afterDrop.join( QStringLiteral( " | " ) ), afterLive.join( QStringLiteral( " | " ) ) ),
                               afterDrop.count() == beforeDrop.count() && afterDrop != beforeDrop
                               && afterLive.count() > 0
                               && afterLive == pb->names().mid( pb->names().count() - afterLive.count() ) );
                        check( *st, QStringLiteral( "(o) ...and the scene's own list with it: %1 (the rows with a clip behind them: %2)" )
                               .arg( sc->animGroups.join( QStringLiteral( " | " ) ), afterLive.join( QStringLiteral( " | " ) ) ),
                               sc && sc->animGroups.mid( sc->animGroups.count() - afterLive.count() ) == afterLive );
""")

# ------------------------------------------------------- 10. the clean-up
rep("(10) the clean-up keeps every row the fixture came with",
"""
                // ---- leave the fixture as the gates after this one expect it
                for ( const QString & n : clipNames() )
                    if ( n != entry )
                        WwHkxAnimHub::instance()->unload( ogl, n );
                ws->refresh();
                qApp->processEvents();
""",
"""
                /* ---- leave the fixture as the gates after this one expect it.
                 * "As it was" is names0, not the one entry: the fixture also
                 * ships the refused skeleton row, and unloading THAT left the
                 * list one row short and the gates after this one looking at a
                 * fixture this gate had quietly changed. */
                for ( const QString & n : clipNames() )
                    if ( !names0.contains( n ) )
                        WwHkxAnimHub::instance()->unload( ogl, n );
                ws->refresh();
                settle();
""")

# ------------------------------------------------- 11. (j) Qt's own buttons
rep("(11j) Qt's own qt_* buttons are not ours to tip",
"""
                            int untipped = 0, buttons = 0;
                            QString untippedName;
                            for ( QAbstractButton * b : ws->findChildren<QAbstractButton *>() ) {
                                buttons++;
                                if ( b->toolTip().isEmpty() ) {
                                    untipped++;
                                    untippedName = b->objectName();
                                }
                            }
""",
"""
                            /* Qt builds buttons of its own inside its own
                             * widgets -- qt_menubar_ext_button is the menu
                             * bar's overflow chevron -- and they are not ours
                             * to write a tip on: the count is over OUR buttons,
                             * and says so. */
                            auto oursToTip = []( QAbstractButton * b ) {
                                return !b->objectName().startsWith( QStringLiteral( "qt_" ) );
                            };
                            int untipped = 0, buttons = 0;
                            QString untippedName;
                            for ( QAbstractButton * b : ws->findChildren<QAbstractButton *>() ) {
                                if ( !oursToTip( b ) )
                                    continue;
                                buttons++;
                                if ( b->toolTip().isEmpty() ) {
                                    untipped++;
                                    untippedName = b->objectName();
                                }
                            }
""")

rep("(11j) the floor counts the same set",
"""
                                QAbstractButton * victim = nullptr;
                                for ( QAbstractButton * b : ws->findChildren<QAbstractButton *>() )
                                    if ( !b->toolTip().isEmpty() ) { victim = b; break; }
                                const QString keep = victim ? victim->toolTip() : QString();
                                if ( victim ) victim->setToolTip( QString() );
                                int u2 = 0;
                                for ( QAbstractButton * b : ws->findChildren<QAbstractButton *>() )
                                    if ( b->toolTip().isEmpty() ) u2++;
""",
"""
                                QAbstractButton * victim = nullptr;
                                for ( QAbstractButton * b : ws->findChildren<QAbstractButton *>() )
                                    if ( oursToTip( b ) && !b->toolTip().isEmpty() ) { victim = b; break; }
                                const QString keep = victim ? victim->toolTip() : QString();
                                if ( victim ) victim->setToolTip( QString() );
                                int u2 = 0;
                                for ( QAbstractButton * b : ws->findChildren<QAbstractButton *>() )
                                    if ( oursToTip( b ) && b->toolTip().isEmpty() ) u2++;
""")

rep("(11j) the message names the set it counted",
"""
                            check( *st, QStringLiteral( "(j) every button tipped: %1 untipped of %2 (%3)" ).arg( untipped ).arg( buttons ).arg( untippedName ), untipped == 0 && buttons >= 12 );
""",
"""
                            check( *st, QStringLiteral( "(j) every button tipped: %1 untipped of %2 of ours (Qt's own qt_* buttons are not ours to tip) (%3)" ).arg( untipped ).arg( buttons ).arg( untippedName ), untipped == 0 && buttons >= 12 );
""")

# --------------------------------------------- 12. (n) the save writes a range
rep("(12n) the whole clip goes back in range before the round trip",
"""
                if ( !st->outDir.isEmpty() ) {
                    const QString axPath = st->outDir + QStringLiteral( "/inapp_axisstrip.hkx" );
                    HkxWriteReport repN;
                    QString errN;
""",
"""
                if ( !st->outDir.isEmpty() ) {
                    /* A SAVE WRITES THE PLAY RANGE, by the product's own design
                     * (src/hkxclipedit.h:120-127, "a SAVE writes the range
                     * alone", and the dock says so in words: "a save writes the
                     * range"). (l) leaves the range at 10..50, so the file that
                     * came back held 41 frames and the gate called a 93-frame
                     * clip unequal to it -- the gate's mistake, not the
                     * writer's. To weigh the bytes of the strip, the whole clip
                     * goes back in range first, and the numbers are printed. */
                    const HkxEditResult rangeBack = doc->setPlayRange( 0, doc->numFrames() - 1 );
                    say( *st, QStringLiteral( "  (n) the whole clip put back in range before the save: %1..%2, still partial=%3 (%4)" )
                         .arg( doc->rangeFirstFrame() ).arg( doc->rangeLastFrame() )
                         .arg( doc->rangeIsPartial() ? QStringLiteral( "yes" ) : QStringLiteral( "no" ) ).arg( rangeBack.message ) );
                    const QString axPath = st->outDir + QStringLiteral( "/inapp_axisstrip.hkx" );
                    HkxWriteReport repN;
                    QString errN;
""")

# ------------------------------------------------------- 13. (q) the floor
rep("(13q) the squeeze floor goes through the splitter",
"""
                    const QList<int> before = split->sizes();
                    const int minWas = leftCol->minimumWidth();
                    leftCol->setMinimumWidth( 0 );
                    leftCol->resize( 40, leftCol->height() );
                    if ( leftCol->layout() )
                        leftCol->layout()->activate();
                    qApp->processEvents();
                    QString who;
                    const int bad = clippedCount( &who );
                    check( *st, QStringLiteral( "(q floor) squeezed to 40 px the SAME test calls the header clipped (%1 clipped:%2)" ).arg( bad ).arg( who ), bad > 0 );
""",
"""
                    /* THE SQUEEZE HAS TO GO THROUGH THE SPLITTER. Resizing a
                     * splitter's child directly is undone by the splitter's own
                     * layout at the next event pump, so the column was back at
                     * its full width by the time the floor measured it and
                     * nothing was clipped -- a floor that never fired. Setting
                     * the sizes on the splitter is how a hand drags the handle,
                     * and the width it actually reached is printed beside the
                     * count. */
                    const QList<int> before = split->sizes();
                    const int minWas = leftCol->minimumWidth();
                    leftCol->setMinimumWidth( 0 );
                    QList<int> squeezed = before;
                    if ( squeezed.count() >= 2 ) {
                        const int total = squeezed.at( 0 ) + squeezed.at( 1 );
                        squeezed[0] = 40;
                        squeezed[1] = total - 40;
                    }
                    split->setSizes( squeezed );
                    leftCol->resize( 40, leftCol->height() );
                    if ( leftCol->layout() )
                        leftCol->layout()->activate();
                    qApp->processEvents();
                    QString who;
                    const int bad = clippedCount( &who );
                    check( *st, QStringLiteral( "(q floor) squeezed to 40 px the SAME test calls the header clipped (%1 clipped, the column is %2 px wide:%3)" )
                           .arg( bad ).arg( leftCol->width() ).arg( who ), bad > 0 );
""")

out = "\n".join(lines)
if "\r" in out:
    die("a CR crept in; nothing written")
open(P, "wb").write(out.encode("utf-8"))
sys.stdout.write("written: %s, %d lines in, %d lines out\n" % (P, n_in, len(lines)))
