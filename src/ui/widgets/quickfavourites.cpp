/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "quickfavourites.h"

#include "nifskope.h"
#include "spellbook.h"
#include "model/nifmodel.h"
#include "wwskin.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QCursor>
#include <QEvent>
#include <QMenu>
#include <QSettings>

//! @file quickfavourites.cpp Blender's Quick Favourites

namespace {

const char * const FavouritesKey = "QuickFavourites";

//! The SpellBook a menu belongs to, walking out through nested submenus.
//! A book's submenus are plain QMenus, so the action in one is still a spell of
//! the book several parents up.
const SpellBook * bookOf( const QObject * menu )
{
	for ( const QObject * o = menu; o; o = o->parent() )
		if ( auto * book = qobject_cast<const SpellBook *>( o ) )
			return book;
	return nullptr;
}

QString cleanLabel( QString text )
{
	text.remove( QLatin1Char( '&' ) );
	// menu rows carry their shortcut after a tab
	const int tab = text.indexOf( QLatin1Char( '\t' ) );
	if ( tab >= 0 )
		text.truncate( tab );
	return text.trimmed();
}

} // namespace

QString wwFavouriteId( const SpellBook * book, QAction * action )
{
	if ( !action || action->isSeparator() || action->menu() )
		return QString();
	if ( book ) {
		// spellFor is not const, and asking a book about its own action does not
		// change it -- the constness here is about not being handed a book to edit
		if ( SpellPtr spell = const_cast<SpellBook *>( book )->spellFor( action ) )
			return QStringLiteral( "spell:%1/%2" ).arg( spell->page(), spell->name() );
	}
	if ( !action->objectName().isEmpty() )
		return QStringLiteral( "action:%1" ).arg( action->objectName() );
	return QString();
}

QStringList wwFavourites()
{
	return QSettings().value( QLatin1String( FavouritesKey ) ).toStringList();
}

bool wwIsFavourite( const QString & id )
{
	return !id.isEmpty() && wwFavourites().contains( id );
}

bool wwToggleFavourite( const QString & id )
{
	if ( id.isEmpty() )
		return false;
	QStringList list = wwFavourites();
	const bool pinning = !list.contains( id );
	if ( pinning )
		list.append( id );		// newest last, so the menu keeps the order they were added in
	else
		list.removeAll( id );
	QSettings().setValue( QLatin1String( FavouritesKey ), list );
	return pinning;
}

namespace {

//! Right-click on a menu entry offers to pin it.
class FavouriteMenuFilter final : public QObject
{
public:
	explicit FavouriteMenuFilter( QObject * owner ) : QObject( owner ) {}

protected:
	bool eventFilter( QObject * watched, QEvent * event ) override
	{
		if ( event->type() != QEvent::ContextMenu )
			return false;
		QMenu * menu = qobject_cast<QMenu *>( watched );
		if ( !menu )
			return false;
		auto * click = static_cast<QContextMenuEvent *>( event );
		QAction * over = menu->actionAt( click->pos() );
		if ( !over || over->isSeparator() )
			return false;

		const QString id = wwFavouriteId( bookOf( menu ), over );
		QMenu offer;
		if ( id.isEmpty() ) {
			/* Said, not silently skipped. A right-click that does nothing on
			 * some rows and opens a menu on others reads as broken; this reads
			 * as a rule.
			 */
			QAction * why = offer.addAction( over->menu()
				? QObject::tr( "A submenu cannot be a favourite" )
				: QObject::tr( "This entry has no name to remember it by" ) );
			why->setEnabled( false );
		} else {
			const bool pinned = wwIsFavourite( id );
			QAction * toggle = offer.addAction( pinned
				? QObject::tr( "Remove from Quick Favourites" )
				: QObject::tr( "Add to Quick Favourites" ) );
			QObject::connect( toggle, &QAction::triggered, [id]() { wwToggleFavourite( id ); } );
		}
		offer.exec( click->globalPos() );
		return true;		// the right-click is spent on this, not passed on
	}
};

} // namespace

void wwInstallFavouriteMenuFilter( QObject * owner )
{
	if ( qApp )
		qApp->installEventFilter( new FavouriteMenuFilter( owner ) );
}

void wwShowQuickFavourites( QWidget * window, const QPoint & at )
{
	auto * skope = qobject_cast<NifSkope *>( window );
	if ( !skope )
		return;
	NifModel * nif = skope->getNifModel();

	QMenu menu( skope );
	menu.setTitle( QObject::tr( "Quick Favourites" ) );
	menu.setToolTipsVisible( true );

	/* One book for the whole menu, built against what is selected now, and kept
	 * alive until exec() returns -- a spell's QAction belongs to its book, so a
	 * book built per row and destroyed per row takes the row with it.
	 */
	const QModelIndex idx = nif ? skope->currentNifIndex() : QModelIndex();
	SpellBook book( nif, idx, skope, SLOT( select( const QModelIndex & ) ) );

	int offered = 0;
	for ( const QString & id : wwFavourites() ) {
		if ( id.startsWith( QLatin1String( "spell:" ) ) ) {
			SpellPtr spell = SpellBook::lookup( id.mid( 6 ) );
			if ( !nif || !spell || !spell->isApplicable( nif, idx ) )
				continue;		// hidden, not greyed: a hand-made shortlist stays short
			QAction * row = menu.addAction( cleanLabel( spell->name() ) );
			row->setToolTip( spell->hint() );
			QObject::connect( row, &QAction::triggered, skope, [skope, nif, idx, spell]() {
				SpellBook run( nif, idx, skope, SLOT( select( const QModelIndex & ) ) );
				run.cast( nif, idx, spell );
			} );
			offered++;
			continue;
		}
		if ( !id.startsWith( QLatin1String( "action:" ) ) )
			continue;
		QAction * found = skope->findChild<QAction *>( id.mid( 7 ) );
		if ( !found || !found->isEnabled() )
			continue;
		QAction * row = menu.addAction( found->icon(), cleanLabel( found->text() ) );
		row->setToolTip( found->toolTip() );
		row->setCheckable( found->isCheckable() );
		row->setChecked( found->isChecked() );
		QObject::connect( row, &QAction::triggered, found, &QAction::trigger );
		offered++;
	}

	if ( !offered ) {
		/* Blender's empty state, and for its reason: the menu is empty on a
		 * fresh install and there is nothing on screen that would tell you how
		 * to fill it.
		 */
		QAction * empty = menu.addAction( wwFavourites().isEmpty()
			? QObject::tr( "No favourites yet — right-click a menu entry to add one" )
			: QObject::tr( "No favourite applies to this selection" ) );
		empty->setEnabled( false );
	}
	menu.exec( at.isNull() ? QCursor::pos() : at );
}
