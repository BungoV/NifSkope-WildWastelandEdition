"""UINOTES2 step 2 -- Ctrl+A keeps every row.

UINOTES1b's one red: `selectAll()` left 1 of N rows selected. Measured chain,
from that lane's report and re-read here:

  itemSelectionChanged -> listRowChosen -> selectEntry(drive=true)
  -> WwHkxAnimHub::activate -> GLView::setSceneSequence
  -> GLView::sequenceChanged -> AnimWorkspace::setSequenceByName
  -> list->setCurrentItem( it )        <-- Qt's ClearAndSelect

Two places throw a multi-selection away, and both are fixed here:

  (a) setSequenceByName's setCurrentItem, which is the synchronous one the
      gate caught;
  (b) rebuildList, which clears the list and then restores the CURRENT row
      only -- so even with (a) fixed, the 50 ms debounced refresh would eat
      the selection a moment later.

Smallest blast radius of the three candidates the report named: the guard is
on the DRIVEN re-entry (the selection flag on one setCurrentItem call), not on
the drive itself, so single-click still drives the viewport exactly as before.

Refusing script: exact-once anchors, pure LF, all-or-nothing.
"""
import os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "src", "animworkspace.cpp")

with open(SRC, "rb") as f:
    orig = f.read()
assert orig.count(b"\r") == 0
text = orig.decode("utf-8")

subs = []

# ---- (a) the driven re-entry no longer clears the selection
subs.append((
    "\t\tif ( it->data( Qt::UserRole ).toString() == name ) {\n"
    "\t\t\tlist->setCurrentItem( it );\n"
    "\t\t\tsyncing = false;\n",
    "\t\tif ( it->data( Qt::UserRole ).toString() == name ) {\n"
    "\t\t\t/* THE DRIVEN RE-ENTRY (lane UINOTES2, 2026-09-12). This is reached\n"
    "\t\t\t   from the VIEWPORT: something activated a clip and the list is\n"
    "\t\t\t   being told about it. Qt's one-argument setCurrentItem carries\n"
    "\t\t\t   ClearAndSelect, so when the activation came from the list's own\n"
    "\t\t\t   selection -- Ctrl+A selects every row, which activates the\n"
    "\t\t\t   current one -- this call threw every other selected row away and\n"
    "\t\t\t   Ctrl+A ended with one row selected. If the row is ALREADY part of\n"
    "\t\t\t   the selection, move the current row and leave the selection\n"
    "\t\t\t   alone; if it is not, select it as before, because then the\n"
    "\t\t\t   viewport is showing something the list is not and the row has to\n"
    "\t\t\t   become visible.\n"
    "\n"
    "\t\t\t   What would refute it: a user with a multi-selection who expects\n"
    "\t\t\t   the list to collapse to the one clip the viewport switched to.\n"
    "\t\t\t   That trade is deliberate -- a selection the user made outranks\n"
    "\t\t\t   the viewport's echo of it. */\n"
    "\t\t\tlist->setCurrentItem( it, it->isSelected() ? QItemSelectionModel::NoUpdate\n"
    "\t\t\t\t\t\t\t\t\t\t\t\t\t : QItemSelectionModel::ClearAndSelect );\n"
    "\t\t\tsyncing = false;\n",
))

# ---- (b) a rebuild keeps the whole selection, not just the current row
subs.append((
    "void AnimWorkspace::rebuildList()\n"
    "{\n"
    "\tsyncing = true;\n"
    "\tlist->clear();\n",
    "void AnimWorkspace::rebuildList()\n"
    "{\n"
    "\tsyncing = true;\n"
    "\t/* WHAT WAS SELECTED, before the list is thrown away and built again.\n"
    "\t   Restoring only the current row (which is what this did until\n"
    "\t   2026-09-12) meant every refresh silently reduced a multi-selection to\n"
    "\t   one row, so Delete / Copy / Cut acted on one clip however many the\n"
    "\t   user had picked. The key is kind + name, because a NIF sequence and a\n"
    "\t   loaded clip may share a name. */\n"
    "\tQSet<QString> wasSelected;\n"
    "\tfor ( int i = 0; i < list->count(); i++ ) {\n"
    "\t\tconst QListWidgetItem * it = list->item( i );\n"
    "\t\tif ( it->isSelected() )\n"
    "\t\t\twasSelected.insert( ( it->data( Qt::UserRole + 1 ).toBool() ? QStringLiteral( \"C|\" ) : QStringLiteral( \"S|\" ) )\n"
    "\t\t\t\t+ it->data( Qt::UserRole ).toString() );\n"
    "\t}\n"
    "\tlist->clear();\n",
))

subs.append((
    "\t// keep the selection\n"
    "\tfor ( int i = 0; i < list->count(); i++ ) {\n"
    "\t\tQListWidgetItem * it = list->item( i );\n"
    "\t\tif ( it->data( Qt::UserRole ).toString() == curEntry && it->data( Qt::UserRole + 1 ).toBool() == curIsClip ) {\n"
    "\t\t\tlist->setCurrentItem( it );\n"
    "\t\t\tbreak;\n"
    "\t\t}\n"
    "\t}\n",
    "\t// keep the selection -- the current row AND every other row that was selected\n"
    "\tQListWidgetItem * cur = nullptr;\n"
    "\tfor ( int i = 0; i < list->count(); i++ ) {\n"
    "\t\tQListWidgetItem * it = list->item( i );\n"
    "\t\tconst QString name = it->data( Qt::UserRole ).toString();\n"
    "\t\tconst bool isClip = it->data( Qt::UserRole + 1 ).toBool();\n"
    "\t\tif ( !cur && name == curEntry && isClip == curIsClip )\n"
    "\t\t\tcur = it;\n"
    "\t\tif ( wasSelected.contains( ( isClip ? QStringLiteral( \"C|\" ) : QStringLiteral( \"S|\" ) ) + name ) )\n"
    "\t\t\tit->setSelected( true );\n"
    "\t}\n"
    "\tif ( cur ) {\n"
    "\t\t// the same rule as the driven re-entry: do not let Qt's ClearAndSelect\n"
    "\t\t// undo the selection this function has just put back\n"
    "\t\tlist->setCurrentItem( cur, cur->isSelected() ? QItemSelectionModel::NoUpdate\n"
    "\t\t\t\t\t\t\t\t\t\t\t\t  : QItemSelectionModel::ClearAndSelect );\n"
    "\t}\n",
))

bad = 0
for anchor, _ in subs:
    n = text.count(anchor)
    if n != 1:
        bad += 1
        print("ANCHOR x%d (want 1): %s" % (n, anchor.splitlines()[0][:90]))
if bad:
    sys.exit("refused: %d anchor(s) did not match exactly once; nothing written" % bad)

for anchor, rep in subs:
    text = text.replace(anchor, rep, 1)

out = text.encode("utf-8")
assert out.count(b"\r") == 0
with open(SRC, "wb") as f:
    f.write(out)
print("written %s: CR %d LF %d bytes %d (was %d)" % (SRC, out.count(b"\r"), out.count(b"\n"), len(out), len(orig)))
