#!/usr/bin/env python
# Lane UINOTES1b -- the THIRD pass over the gate file, 2026-09-12 05:4x.
#
# Two repairs of the gate's own timing, and two DIAGNOSTICS that turn a failure
# I can only reason about into numbers I can read. Nothing here touches product
# behaviour.
#
#   (a) selectRowNamed settles TWICE. Selecting a row activates it, and the
#       activation schedules the 50 ms rebuild; the re-selection after the first
#       settle schedules a SECOND one, which landed in the middle of F2's inline
#       editor and destroyed it. Settling again leaves no timer pending when the
#       action fires.
#   (b) the (q) floor squeezes a BUTTON, not the column: the column cannot go
#       under its own layout's minimum (measured: 219 px with the splitter
#       handed 40), so that floor could never fire.
#   (c) Ctrl+A is measured again with the list's signals blocked, which says
#       whether the row selection is lost inside selectAll() or inside what
#       selecting drives.
#   (d) the drop prints the row order and the playback's order at each step, so
#       "the drop did not reorder" names WHERE it was lost.
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
                die("a continuation line is indented less than the line above: " + repr(ln))
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

# ------------------------------------------------- (a) settle twice
rep("(a) the selection settles twice, so nothing is left pending",
"""
                auto selectRowNamed = [&]( const QString & name ) {
                    if ( !selectOnce( name ) )
                        return false;
                    settle();
                    return selectOnce( name );
                };
""",
"""
                auto selectRowNamed = [&]( const QString & name ) {
                    if ( !selectOnce( name ) )
                        return false;
                    settle();              // the activation's rebuild lands
                    if ( !selectOnce( name ) )
                        return false;
                    settle();              // and so does the re-selection's
                    return selectOnce( name );
                };
""")

# ------------------------------------------------- (c) Ctrl+A, blocked
rep("(c) Ctrl+A measured again with the list's signals blocked",
"""
                    check( *st, QStringLiteral( "(o) Ctrl+A selects every row: %1 of %2 (one pump later %3, once the list rebuilds %4)" )
                           .arg( selNow ).arg( list->count() ).arg( selPumped ).arg( selSettled ),
                           sa && selNow == list->count() && list->count() >= 3 );
                }
""",
"""
                    check( *st, QStringLiteral( "(o) Ctrl+A selects every row: %1 of %2 (one pump later %3, once the list rebuilds %4)" )
                           .arg( selNow ).arg( list->count() ).arg( selPumped ).arg( selSettled ),
                           sa && selNow == list->count() && list->count() >= 3 );
                    /* WHERE DOES THE SELECTION GO? The same selectAll(), with
                     * the list's own signals blocked so nothing it drives can
                     * run. Four here and one above means the rows ARE selected
                     * and something the selection drives takes them away again;
                     * one here means selectAll() itself only took one row. */
                    list->blockSignals( true );
                    list->selectAll();
                    list->blockSignals( false );
                    say( *st, QStringLiteral( "  (o) Ctrl+A with the list's signals blocked: %1 of %2 selected, current row %3, selection mode %4" )
                         .arg( list->selectedItems().count() ).arg( list->count() )
                         .arg( list->currentRow() ).arg( int( list->selectionMode() ) ) );
                }
""")

# ------------------------------------------------- (d) the drop, step by step
rep("(d) the drop prints every step",
"""
                        QListWidgetItem * moved = list->takeItem( src );
                        list->insertItem( dst, moved );
                        QMimeData md;
                        QDropEvent de( QPointF( 8, list->visualItemRect( moved ).center().y() ), Qt::MoveAction,
                                       &md, Qt::LeftButton, Qt::NoModifier, QEvent::Drop );
                        QApplication::sendEvent( list->viewport(), &de );
                        settle();
                        const QStringList afterDrop = clipNames();
""",
"""
                        QListWidgetItem * moved = list->takeItem( src );
                        list->insertItem( dst, moved );
                        say( *st, QStringLiteral( "  (o) the drop, step 1 -- the rows after the fake move: %1  |  playback: %2" )
                             .arg( clipNames().join( QStringLiteral( " | " ) ), pb->names().join( QStringLiteral( " | " ) ) ) );
                        QMimeData md;
                        QDropEvent de( QPointF( 8, list->visualItemRect( moved ).center().y() ), Qt::MoveAction,
                                       &md, Qt::LeftButton, Qt::NoModifier, QEvent::Drop );
                        QApplication::sendEvent( list->viewport(), &de );
                        say( *st, QStringLiteral( "  (o) the drop, step 2 -- straight after the Drop event: %1  |  playback: %2" )
                             .arg( clipNames().join( QStringLiteral( " | " ) ), pb->names().join( QStringLiteral( " | " ) ) ) );
                        qApp->processEvents();
                        say( *st, QStringLiteral( "  (o) the drop, step 3 -- after one pump (the queued commit runs here): %1  |  playback: %2" )
                             .arg( clipNames().join( QStringLiteral( " | " ) ), pb->names().join( QStringLiteral( " | " ) ) ) );
                        settle();
                        const QStringList afterDrop = clipNames();
                        say( *st, QStringLiteral( "  (o) the drop, step 4 -- after the list rebuilds: %1  |  playback: %2" )
                             .arg( afterDrop.join( QStringLiteral( " | " ) ), pb->names().join( QStringLiteral( " | " ) ) ) );
""")

# ------------------------------------------------- (b) the (q) floor
rep("(b) the (q) floor squeezes a button, not the column",
"""
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
""",
"""
                    /* THE COLUMN CANNOT BE SQUEEZED, and that is the point of
                     * the check above: handed 40 px by the splitter it stopped
                     * at 219, because its own layout's minimum holds it there.
                     * So the floor squeezes what the predicate actually reads
                     * -- one of the three buttons -- and puts it straight back.
                     * Same predicate, same run, proved able to fail. */
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
                    if ( leftCol->layout() )
                        leftCol->layout()->activate();
                    qApp->processEvents();
                    say( *st, QStringLiteral( "  (q floor) the splitter was handed 40 px for the column; it settled at %1 px (its layout's minimum is %2)" )
                         .arg( leftCol->width() ).arg( leftCol->minimumSizeHint().width() ) );
                    auto * squeezeMe = widget<QToolButton>( ws, "AnimWsLoadAnim" );
                    const QSize wasSize = squeezeMe ? squeezeMe->size() : QSize();
                    if ( squeezeMe )
                        squeezeMe->resize( 8, squeezeMe->height() );
                    QString who;
                    const int bad = clippedCount( &who );
                    check( *st, QStringLiteral( "(q floor) with one button squeezed to 8 px the SAME test calls the header clipped (%1 clipped:%2)" )
                           .arg( bad ).arg( who ), bad > 0 );
                    if ( squeezeMe )
                        squeezeMe->resize( wasSize );
""")

out = "\n".join(lines)
if "\r" in out:
    die("a CR crept in; nothing written")
open(P, "wb").write(out.encode("utf-8"))
sys.stdout.write("written: %s, %d lines in, %d lines out\n" % (P, n_in, len(lines)))
