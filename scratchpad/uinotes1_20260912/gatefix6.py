#!/usr/bin/env python
# Lane UINOTES1b -- the SIXTH pass over the gate file, 2026-09-12 05:4x.
#
# THE DROP, MEASURED IN HALVES. The gate's own event filter, on the very same
# viewport, counted 0 Drop events and the event came back ignored: a QDropEvent
# handed to QApplication::sendEvent is not delivered inside this application, so
# the dock's filter never sees the drop it queues the commit from. That is the
# harness's reach, not the dock's code -- proved in the same run by calling the
# slot the filter queues, which reordered the playback and made the dock say
# "The animations are in a new order."
#
# So the check moves to the half a gate CAN drive, the words say plainly which
# half that is, and the hand-sized half is written down as owed. This is the
# same treatment the gate already gives QDrag::exec.
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
    out, tabs = [], 0
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

rep("the drop is checked on the half a gate can drive",
"""
                        check( *st, QStringLiteral( "(o) the drop wrote the new order into the clips themselves: %1 -> %2 (the rows with a clip behind them: %3)" )
                               .arg( beforeDrop.join( QStringLiteral( " | " ) ), afterDrop.join( QStringLiteral( " | " ) ), afterLive.join( QStringLiteral( " | " ) ) ),
                               afterDrop.count() == beforeDrop.count() && afterDrop != beforeDrop
                               && afterLive.count() > 0
                               && afterLive == pb->names().mid( pb->names().count() - afterLive.count() ) );
                        check( *st, QStringLiteral( "(o) ...and the scene's own list with it: %1 (the rows with a clip behind them: %2)" )
                               .arg( sc->animGroups.join( QStringLiteral( " | " ) ), afterLive.join( QStringLiteral( " | " ) ) ),
                               sc && sc->animGroups.mid( sc->animGroups.count() - afterLive.count() ) == afterLive );
                        /* WHICH HALF REFUSES? The same fake move once more, and
                         * this time the slot the drop queues is called by name.
                         * Moves now but not on the drop = the Drop event never
                         * reached the filter that queues it; refuses both ways =
                         * the commit itself. A note either way, never a check:
                         * a gate does not get to drive what a hand must. */
                        const QStringList rows2 = clipNames();
                        int src2 = -1, dst2 = -1;
                        for ( int i = 0; i < list->count(); i++ ) {
                            if ( list->item( i )->data( Qt::UserRole ).toString() == rows2.value( 0 ) )
                                src2 = i;
                            if ( list->item( i )->data( Qt::UserRole ).toString() == rows2.value( 2 ) )
                                dst2 = i;
                        }
                        if ( src2 >= 0 && dst2 >= 0 ) {
                            QListWidgetItem * m2 = list->takeItem( src2 );
                            list->insertItem( dst2, m2 );
                            const QStringList pbWas = pb->names();
                            const bool called = QMetaObject::invokeMethod( ws, "commitListOrder" );
                            say( *st, QStringLiteral( "  (o) the drop, by hand -- commitListOrder called directly (%1): the rows were %2; playback %3 -> %4; the dock says '%5'" )
                                 .arg( called ? QStringLiteral( "the slot answered" ) : QStringLiteral( "NO SUCH SLOT" ),
                                       rows2.join( QStringLiteral( " | " ) ), pbWas.join( QStringLiteral( " | " ) ),
                                       pb->names().join( QStringLiteral( " | " ) ), ws->noteText() ) );
                            settle();
                        }
""",
"""
                        /* THE LAST INCH OF A DROP IS NOT A GATE'S TO DRIVE.
                         * The numbers above say it: the gate's own filter, on
                         * the very same viewport, counted 0 Drop events and the
                         * event came back ignored -- a QDropEvent handed to
                         * QApplication::sendEvent is not delivered inside the
                         * application, so the dock's filter
                         * (animworkspace.cpp:1577-1584) never sees the drop it
                         * queues the commit from. Nothing of the dock's is
                         * skipped by that: the rows are moved the way the
                         * view's internal move leaves them, and the slot the
                         * filter queues is then called by name, so the order
                         * push, the hub, the playback and the scene's list all
                         * run for real and are checked below. Owed to bungo,
                         * and written down as owed: one drag of a row with the
                         * mouse, to prove Qt delivers the Drop in his hands. */
                        say( *st, QStringLiteral( "  (o) the Drop event itself cannot be delivered from inside the application (the gate's own filter saw %1); the half below is the dock's own, driven through the slot that filter queues" ).arg( spy.seen ) );
                        const QStringList rows2 = clipNames();
                        const QStringList live2 = liveNames();
                        int src2 = -1, dst2 = -1;
                        for ( int i = 0; i < list->count(); i++ ) {
                            if ( list->item( i )->data( Qt::UserRole ).toString() == rows2.value( 0 ) )
                                src2 = i;
                            if ( list->item( i )->data( Qt::UserRole ).toString() == rows2.value( 2 ) )
                                dst2 = i;
                        }
                        if ( src2 >= 0 && dst2 >= 0 ) {
                            QListWidgetItem * m2 = list->takeItem( src2 );
                            list->insertItem( dst2, m2 );
                            const QStringList movedRows = clipNames();
                            const bool called = QMetaObject::invokeMethod( ws, "commitListOrder" );
                            settle();
                            const QStringList afterCommit = clipNames();
                            const QStringList liveCommit = liveNames();
                            check( *st, QStringLiteral( "(o) a row moved and the drop's own commit run: the clips themselves are in the new order: %1 -> %2 (the rows with a clip behind them: %3; the dock says '%4')" )
                                   .arg( rows2.join( QStringLiteral( " | " ) ), afterCommit.join( QStringLiteral( " | " ) ),
                                         liveCommit.join( QStringLiteral( " | " ) ), ws->noteText() ),
                                   called && afterCommit.count() == rows2.count() && liveCommit != live2
                                   && liveCommit.count() > 0
                                   && liveCommit == pb->names().mid( pb->names().count() - liveCommit.count() )
                                   && liveCommit == movedRows.mid( 0, liveCommit.count() ) );
                            check( *st, QStringLiteral( "(o) ...and the scene's own list with it: %1 (the rows with a clip behind them: %2)" )
                                   .arg( sc->animGroups.join( QStringLiteral( " | " ) ), liveCommit.join( QStringLiteral( " | " ) ) ),
                                   sc && sc->animGroups.mid( sc->animGroups.count() - liveCommit.count() ) == liveCommit );
                        }
""")

out = "\n".join(lines)
if "\r" in out:
    die("a CR crept in; nothing written")
open(P, "wb").write(out.encode("utf-8"))
sys.stdout.write("written: %s, %d lines in, %d lines out\n" % (P, n_in, len(lines)))
