/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WW_SPELLPALETTE_H
#define WW_SPELLPALETTE_H

#include <QString>

class QAction;
class QWidget;
class SpellBook;

//! \file spellpalette.h The Ctrl+Shift+P command palette

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
 */
QAction * wwSpellPalette( QWidget * parent, SpellBook & book, const QString & context );

#endif
