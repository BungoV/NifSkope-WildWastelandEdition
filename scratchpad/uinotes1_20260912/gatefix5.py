#!/usr/bin/env python
# Lane UINOTES1b -- the FIFTH pass over the gate file, 2026-09-12 05:4x.
#
#   (a) the rename waits. Committing the inline editor goes through the hub, so
#       the rows only carry the new name after the same 50 ms rebuild; the gate
#       read them two pumps later and saw the old ones. Same for Escape.
#   (b) a spy on the Drop. The gate's synthetic drop leaves the hub's order
#       untouched, while the slot that drop queues reorders it correctly when it
#       is called by name -- so the question is whether the Drop event reaches
#       the viewport at all. The gate now counts it with an event filter of its
#       own, which answers that with a number.
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
rep("(a) the rename is read after the rebuild, not before it",
"""
                if ( ed ) {
                    ed->setText( renamed );
                    QKeyEvent ret( QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier );
                    QApplication::sendEvent( ed, &ret );
                    qApp->processEvents();
                    qApp->processEvents();
                }
                const QStringList namedNow = clipNames();
""",
"""
                if ( ed ) {
                    ed->setText( renamed );
                    QKeyEvent ret( QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier );
                    QApplication::sendEvent( ed, &ret );
                    settle();   // the rename goes through the hub: same 50 ms rebuild
                }
                const QStringList namedNow = clipNames();
""")

rep("(a) Escape is read after the rebuild too",
"""
                    if ( ed2 ) {
                        QKeyEvent esc( QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier );
                        QApplication::sendEvent( ed2, &esc );
                        qApp->processEvents();
                    }
""",
"""
                    if ( ed2 ) {
                        QKeyEvent esc( QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier );
                        QApplication::sendEvent( ed2, &esc );
                        settle();
                    }
""")

# --------------------------------------------------------------- (b)
rep("(b) a spy counts the Drop event on the viewport",
"""
                        QListWidgetItem * moved = list->takeItem( src );
                        list->insertItem( dst, moved );
""",
"""
                        /* Does the Drop event arrive at all? A filter of the
                         * gate's own on the same viewport counts it. The dock's
                         * filter (animworkspace.cpp:1577-1584) queues the commit
                         * from exactly this event, so "seen 1, order unchanged"
                         * and "seen 0" are two different faults and this tells
                         * them apart. */
                        struct DropSpy : public QObject
                        {
                            int seen = 0;
                            bool eventFilter( QObject *, QEvent * e ) override
                            {
                                if ( e->type() == QEvent::Drop )
                                    seen++;
                                return false;
                            }
                        };
                        DropSpy spy;
                        list->viewport()->installEventFilter( &spy );
                        QListWidgetItem * moved = list->takeItem( src );
                        list->insertItem( dst, moved );
""")

rep("(b) the spy's number is printed with step 2",
"""
                        say( *st, QStringLiteral( "  (o) the drop, step 2 -- straight after the Drop event: %1  |  playback: %2" )
                             .arg( clipNames().join( QStringLiteral( " | " ) ), pb->names().join( QStringLiteral( " | " ) ) ) );
""",
"""
                        say( *st, QStringLiteral( "  (o) the drop, step 2 -- straight after the Drop event (the gate's own filter saw %1 Drop event(s), and the event was %2): %3  |  playback: %4" )
                             .arg( spy.seen ).arg( de.isAccepted() ? QStringLiteral( "accepted" ) : QStringLiteral( "ignored" ) )
                             .arg( clipNames().join( QStringLiteral( " | " ) ), pb->names().join( QStringLiteral( " | " ) ) ) );
                        list->viewport()->removeEventFilter( &spy );
""")

out = "\n".join(lines)
if "\r" in out:
    die("a CR crept in; nothing written")
open(P, "wb").write(out.encode("utf-8"))
sys.stdout.write("written: %s, %d lines in, %d lines out\n" % (P, n_in, len(lines)))
