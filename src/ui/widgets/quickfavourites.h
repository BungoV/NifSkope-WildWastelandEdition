/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WW_QUICKFAVOURITES_H
#define WW_QUICKFAVOURITES_H

#include <QString>
#include <QStringList>

class QAction;
class QObject;
class QPoint;
class QWidget;
class SpellBook;

/*! \file quickfavourites.h Blender's Quick Favourites
 *
 *  Right-click any menu entry to pin it, Q to run it. Blender's, and it earns
 *  its place the same way there: the things one person reaches for twenty times
 *  an hour are not the things the next person does, and no menu layout can be
 *  right for both. A pinned list is the user saying which those are.
 *
 *  A favourite is stored as an ID, never as a pointer. Menus are rebuilt per
 *  right-click and per selection, so the QAction a favourite names does not
 *  exist between one press of Q and the next; it is resolved fresh each time.
 *
 *      spell:Page/Name    a registered spell, resolved through SpellBook::lookup
 *      action:objectName  a QAction of the main window, found by object name
 *
 *  Anything with neither is not offerable — an unnamed action built inline for
 *  one menu cannot be found again, and a favourite that silently does nothing
 *  is worse than one that was never offered.
 */

//! The stored ID for \a action, or empty when it cannot be named again.
//! \param book the menu's SpellBook, if it has one — that is what knows whether
//!             an action is a spell, and which.
QString wwFavouriteId( const SpellBook * book, QAction * action );

//! Ordered list of stored IDs, oldest first.
QStringList wwFavourites();

bool wwIsFavourite( const QString & id );

//! Pin or unpin, and persist. Returns the new state.
bool wwToggleFavourite( const QString & id );

/*! Watch for a right-click on a menu entry, anywhere in the application.
 *
 *  Qt does nothing with a right-click inside a QMenu, so this costs no existing
 *  gesture. Installed once on the application; \a owner only owns the filter's
 *  lifetime.
 */
void wwInstallFavouriteMenuFilter( QObject * owner );

/*! Open the Quick Favourites menu at \a at (global).
 *
 *  Entries that do not apply right now are left out rather than greyed: this
 *  menu is a shortlist someone assembled by hand, and padding it with rows that
 *  cannot run defeats the point of having made it short.
 */
void wwShowQuickFavourites( QWidget * window, const QPoint & at );

#endif
