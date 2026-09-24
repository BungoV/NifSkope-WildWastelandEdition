#!/usr/bin/env python
# Lane UINOTES1b -- the FOURTH pass over the gate file, 2026-09-12 05:4x.
#
#   (a) selectRowNamed stops re-selecting at the end. Every selection schedules
#       a 50 ms rebuild; the last one was left pending and landed in the middle
#       of F2's inline editor, which is why "F2 opens an editor" started failing
#       the moment the gate learned to wait. One selection, one settle, then
#       read back what the rebuild left -- nothing pending afterwards.
#   (b) the drop is measured in two halves: the real path (the checks, as they
#       are), and then the same fake move with the queued slot CALLED BY NAME.
#       If the order moves when it is called and not when it is dropped, the
#       delivery is what is missing; if it refuses both ways, the commit is.
#       Both are printed, neither is a check.
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

# --------------------------------------------------------------- (a)
rep("(a) one selection, one settle, nothing left pending",
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
""",
"""
                auto selectRowNamed = [&]( const QString & name ) {
                    if ( !selectOnce( name ) )
                        return false;
                    /* The selection activates the row, the activation schedules
                     * the 50 ms rebuild, and the rebuild keeps the current row
                     * -- with syncing on, so it starts nothing further. One
                     * wait is therefore enough, and selecting AGAIN afterwards
                     * would leave a rebuild pending that lands in the middle of
                     * whatever the caller does next. */
                    settle();
                    QListWidgetItem * cur = list->currentItem();
                    return cur && cur->data( Qt::UserRole ).toString() == name && cur->isSelected();
                };
""")

# --------------------------------------------------------------- (b)
rep("(b) the drop is asked which half refuses",
"""
                        check( *st, QStringLiteral( "(o) ...and the scene's own list with it: %1 (the rows with a clip behind them: %2)" )
                               .arg( sc->animGroups.join( QStringLiteral( " | " ) ), afterLive.join( QStringLiteral( " | " ) ) ),
                               sc && sc->animGroups.mid( sc->animGroups.count() - afterLive.count() ) == afterLive );
                    }
""",
"""
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
                    }
""")

out = "\n".join(lines)
if "\r" in out:
    die("a CR crept in; nothing written")
open(P, "wb").write(out.encode("utf-8"))
sys.stdout.write("written: %s, %d lines in, %d lines out\n" % (P, n_in, len(lines)))
