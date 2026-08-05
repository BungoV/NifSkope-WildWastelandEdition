/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WW_SPELLPALETTE_H
#define WW_SPELLPALETTE_H

#include <QPoint>
#include <QString>

class QAction;
class QWidget;
class SpellBook;

//! \file spellpalette.h The Ctrl+Shift+P command palette

/*! objectName of the context-menu row that opens the palette.
 *
 *  collect() skips it. The row is an action on the book like any other, so it
 *  listed itself — a "Search…" entry inside the search, offering to open the
 *  thing already open.
 */
#define WW_PALETTE_SEARCH_ROW "WWPaletteSearchRow"

/*! Search everything a SpellBook offers, and run it.
 *
 *  Takes an ALREADY-BUILT book rather than the spell registry, so the actions a
 *  caller hand-added to it — the Block List's verb row, Select & View, Set/Clear
 *  Parent, the diff trio — are searchable for free, and every row's payload is
 *  just the QAction the menu would have shown.
 *
 *  Returns the chosen action WITHOUT triggering it, because the caller owns the
 *  book: the Block List builds its book on the stack of contextMenu, and the
 *  palette has to be opened after exec() returns while that book is still alive.
 *  Returns nullptr if nothing was chosen.
 *
 *  \param context a short line naming what the search is scoped to, shown in the
 *                 header — e.g. the clicked block. Empty is fine.
 *  \param at      global point to open at, normally where the right-click was, so
 *                 it appears where the menu it replaces did. A null point centres
 *                 it on the parent window, which is right for the keyboard
 *                 shortcut — there is no click to be near.
 */
QAction * wwSpellPalette( QWidget * parent, SpellBook & book, const QString & context,
	const QPoint & at = QPoint() );

#endif
