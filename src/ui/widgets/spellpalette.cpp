/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "spellpalette.h"

#include "quickfavourites.h"
#include "spellbook.h"
#include "wwskin.h"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QGuiApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QScreen>
#include <QSet>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>

/*! \file spellpalette.cpp The command palette
 *
 *  DELIBERATELY NOT A LINE EDIT IN THE MENU. A popup QMenu holds a keyboard
 *  grab, so a hosted widget only receives the keys the menu chooses to forward;
 *  QMenu::keyPressEvent otherwise spends every printable key on first-letter
 *  jump; click-to-focus through a QWidgetAction is the flakiest interaction in
 *  Qt; a filtered menu cannot expand a submenu inline, so hits have to be
 *  hoisted onto proxy actions; and the popup is anchored where it was exec'd, so
 *  it can neither grow nor re-centre. Every one of those is novel code fighting
 *  the widget.
 *
 *  A dialog is boring code, and it is the only version that can show what is NOT
 *  available — which is the single biggest confusion in this menu, since Batch,
 *  Sanitize, Optimize and Error Checking vanish on a block row BY DESIGN.
 */

namespace {

//! One searchable entry: the action to run, plus everything a query can match on.
struct PaletteRow
{
	QAction * action = nullptr;
	QString label;			//!< menu text with any "\tShortcut" suffix removed
	QString group;			//!< the submenu path it came from, "" at top level
	QString shortcut;
	QString hint;
	bool enabled = false;
	bool destructive = false;
	QString key;			//!< everything above, folded for matching
};

/*! Walk the book's action tree.
 *
 *  The tree, not the spell registry — that is what makes the four hand-added
 *  native actions (Set/Clear Parent, the verb row, the diff trio) searchable
 *  without registering them as spells, and it is why every row's payload can be
 *  just a QAction.
 */
QString cleanMenuTitle( QString title )
{
	title.replace( QLatin1String( "&&" ), QLatin1String( "&" ) );
	title.remove( QLatin1Char( '&' ) );		// menu-bar titles carry their accelerator
	return title;
}

void collect( QMenu * menu, const QString & path, SpellBook & book, QVector<PaletteRow> & out )
{
	for ( QAction * a : menu->actions() ) {
		if ( a->isSeparator() )
			continue;
		// the row that opens this, offering to open it again
		if ( a->objectName() == QLatin1String( WW_PALETTE_SEARCH_ROW ) )
			continue;
		if ( a->menu() ) {
			QString title = a->menu()->title();
			title.replace( QLatin1String( "&&" ), QLatin1String( "&" ) );
			collect( a->menu(), path.isEmpty() ? title
											   : path + QLatin1String( " ▸ " ) + title, book, out );
			continue;
		}

		PaletteRow r;
		r.action = a;
		r.group = path;
		// the Hierarchy and verb rows put their shortcut in the label after a tab
		const QStringList parts = a->text().split( QLatin1Char( '\t' ) );
		r.label = parts.value( 0 );
		r.shortcut = parts.value( 1 );
		if ( r.shortcut.isEmpty() && !a->shortcut().isEmpty() )
			r.shortcut = a->shortcut().toString( QKeySequence::NativeText );
		if ( SpellPtr s = book.spellFor( a ) ) {
			r.hint = s->hint();
			r.destructive = s->destructive();
		}
		/* Enabled, not visible. checkActions HIDES what does not apply, and the
		 * whole point of the palette is to say "Update All Tangent Spaces —
		 * Batch — not applicable here" rather than nothing at all.
		 */
		r.enabled = a->isEnabled();
		// group is part of the key on purpose: it is what makes "block copy" and
		// "transform copy" distinguishable when four spells are called Copy
		r.key = ( r.label + QLatin1Char( ' ' ) + r.group + QLatin1Char( ' ' )
				+ r.shortcut + QLatin1Char( ' ' ) + r.hint ).simplified();
		out << r;
	}
}

//! Enter/arrows belong to the list even while the field has focus.
class PaletteEdit final : public QLineEdit
{
public:
	PaletteEdit( QTreeView * view, QDialog * dlg ) : QLineEdit( dlg ), list( view ), dialog( dlg ) {}

protected:
	void keyPressEvent( QKeyEvent * e ) override
	{
		switch ( e->key() ) {
		case Qt::Key_Down:
		case Qt::Key_Up:
		case Qt::Key_PageDown:
		case Qt::Key_PageUp:
			QCoreApplication::sendEvent( list, e );
			return;
		case Qt::Key_Return:
		case Qt::Key_Enter:
			dialog->accept();
			return;
		case Qt::Key_Escape:
			/* Closes, whatever is typed. It used to clear first and close only on
			 * a second press, on the reasoning that a half-typed search is not a
			 * thing to lose to one keystroke — but this opens where a menu used to
			 * and is dismissed like one, and a popup that eats the first Escape
			 * reads as a popup that will not close. Retyping four characters is
			 * cheaper than that doubt.
			 */
			dialog->reject();
			return;
		default:
			break;
		}
		QLineEdit::keyPressEvent( e );
	}

private:
	QTreeView * list;
	QDialog * dialog;
};

/*! Dismiss on a click outside, the way the menu it replaces does.
 *
 *  Qt::Popup is what makes this reachable at all: a popup holds the mouse grab,
 *  so a press anywhere is delivered to the popup widget rather than swallowed by
 *  the modality that a plain modal dialog imposes. The press is then judged on
 *  its GLOBAL position, because the grab routes it through whatever widget it
 *  arrived at and the local coordinates mean nothing.
 */
class PaletteDismiss final : public QObject
{
public:
	explicit PaletteDismiss( QDialog * dlg ) : QObject( dlg ), dialog( dlg ) {}

protected:
	bool eventFilter( QObject *, QEvent * e ) override
	{
		if ( e->type() != QEvent::MouseButtonPress )
			return false;
		const QPoint at = static_cast<QMouseEvent *>( e )->globalPosition().toPoint();
		if ( dialog->frameGeometry().contains( at ) )
			return false;
		dialog->reject();
		return true;		// the click that dismisses is spent dismissing
	}

private:
	QDialog * dialog;
};

} // namespace

QAction * wwSpellPalette( QWidget * parent, SpellBook & book, const QString & context,
	const QPoint & at )
{
	QVector<PaletteRow> rows;
	collect( &book, QString(), book, rows );

	/* And the window's own menus, so this searches what the program can do and
	 * not only what the spell registry can.
	 *
	 * Opened from the viewport, "find the function for this mode" means Edit
	 * Mode's operators, the paint tools, the transform gizmos -- none of which
	 * are spells, all of which are menu actions. They carry their own
	 * availability: a mode's actions are disabled outside it, and the palette
	 * already sorts what applies above what does not and says which is which.
	 */
	if ( parent ) {
		if ( auto * bar = parent->window()->findChild<QMenuBar *>() ) {
			QSet<QAction *> already;
			for ( const PaletteRow & r : rows )
				already.insert( r.action );
			QVector<PaletteRow> fromMenus;
			for ( QAction * top : bar->actions() )
				if ( top->menu() )
					collect( top->menu(), cleanMenuTitle( top->menu()->title() ), book, fromMenus );
			for ( const PaletteRow & r : fromMenus )
				if ( !already.contains( r.action ) )
					rows << r;
		}
	}
	if ( rows.isEmpty() )
		return nullptr;

	QDialog dialog( parent );
	/* A popup, not a modal dialog. It is opened from a right-click and it is
	 * dismissed the way anything opened from a right-click is: Escape, a click
	 * outside, a right-click elsewhere. Modality gives none of that -- clicks
	 * outside are discarded before anything can act on them -- and Qt::Popup
	 * gives all three, the last two through PaletteDismiss.
	 */
	dialog.setWindowFlags( Qt::Popup );
	dialog.resize( 640, 420 );
	qApp->installEventFilter( new PaletteDismiss( &dialog ) );
	// named so WW_SPELLSEARCH_TEST can drive them from inside the modal loop
	dialog.setObjectName( QStringLiteral( "WWSpellPalette" ) );

	auto * outer = new QVBoxLayout( &dialog );
	outer->setContentsMargins( 10, 10, 10, 10 );
	outer->setSpacing( 6 );

	auto * view = new QTreeView( &dialog );
	view->setObjectName( QStringLiteral( "SpellPaletteList" ) );
	auto * edit = new PaletteEdit( view, &dialog );
	edit->setObjectName( QStringLiteral( "SpellPaletteSearch" ) );
	edit->setPlaceholderText( context.isEmpty()
		? QObject::tr( "Search spells and actions…" )
		: QObject::tr( "Search spells and actions for %1…" ).arg( context ) );
	outer->addWidget( edit );

	auto * model = new QStandardItemModel( &dialog );
	model->setHorizontalHeaderLabels( { QObject::tr( "Entry" ), QObject::tr( "Group" ),
										QObject::tr( "Shortcut" ) } );
	view->setModel( model );
	view->setRootIsDecorated( false );
	view->setAllColumnsShowFocus( true );
	view->setSelectionBehavior( QAbstractItemView::SelectRows );
	view->setSelectionMode( QAbstractItemView::SingleSelection );
	view->setEditTriggers( QAbstractItemView::NoEditTriggers );
	view->setUniformRowHeights( true );
	view->setStyleSheet( wwSelectionTreeQss() );
	outer->addWidget( view, 1 );

	auto * strip = new QLabel( &dialog );
	strip->setWordWrap( false );
	strip->setTextFormat( Qt::PlainText );
	strip->setStyleSheet( QStringLiteral( "color: %1;" ).arg( wwSkinColor( "textMuted" ) ) );
	outer->addWidget( strip );

	const QColor dim( wwSkinColor( "textMuted" ) );

	// rebuilt per keystroke rather than filtered through a proxy: ~250 rows is
	// nothing, and a QSortFilterProxyModel subclass would be more code than this
	auto repopulate = [&]() {
		const QStringList terms = edit->text().simplified().split(
			QLatin1Char( ' ' ), Qt::SkipEmptyParts );		// verbatim applyBlockListFilter
		model->removeRows( 0, model->rowCount() );

		// applicable first, then the rest — the greyed tail is the answer to
		// "why is it not in the menu", and it must never be in the way
		for ( int pass = 0; pass < 2; pass++ ) {
			for ( const PaletteRow & r : rows ) {
				if ( r.enabled != ( pass == 0 ) )
					continue;
				bool hit = true;
				for ( const QString & t : terms )
					if ( !r.key.contains( t, Qt::CaseInsensitive ) ) { hit = false; break; }
				if ( !hit )
					continue;

				auto * iLabel = new QStandardItem( r.label );
				auto * iGroup = new QStandardItem( r.group );
				auto * iKeys = new QStandardItem( r.shortcut );
				for ( QStandardItem * it : { iLabel, iGroup, iKeys } ) {
					it->setEditable( false );
					// selectable but not enabled would make it unreachable by
					// keyboard AND unreadable; grey it and let it be read
					if ( !r.enabled )
						it->setForeground( dim );
				}
				iLabel->setData( QVariant::fromValue( quintptr( r.action ) ), Qt::UserRole );
				iLabel->setData( r.hint, Qt::UserRole + 1 );
				iLabel->setData( r.enabled, Qt::UserRole + 2 );
				iLabel->setData( r.destructive, Qt::UserRole + 3 );
				if ( !r.enabled )
					iLabel->setToolTip( QObject::tr( "Not applicable to this selection" ) );
				model->appendRow( { iLabel, iGroup, iKeys } );
			}
		}

		/* Auto-highlight the top hit, UNLESS it destroys something.
		 *
		 * Two keystrokes and Return must never reach Crop To Branch. When the
		 * best match is destructive nothing is highlighted, so the user has to
		 * arrow onto it deliberately.
		 */
		if ( model->rowCount() > 0 ) {
			const QModelIndex first = model->index( 0, 0 );
			if ( first.data( Qt::UserRole + 2 ).toBool()
				&& !first.data( Qt::UserRole + 3 ).toBool() )
				view->setCurrentIndex( first );
			else
				view->selectionModel()->clearSelection();
		}
		strip->setText( QString() );
	};

	auto showHint = [&]( const QModelIndex & current ) {
		if ( !current.isValid() ) { strip->setText( QString() ); return; }
		const QModelIndex row = current.sibling( current.row(), 0 );
		QString text = row.data( Qt::UserRole + 1 ).toString();
		if ( !row.data( Qt::UserRole + 2 ).toBool() ) {
			const QString why = QObject::tr( "Not applicable to this selection." );
			text = text.isEmpty() ? why : why + QLatin1Char( ' ' ) + text;
		}
		strip->setText( text );
	};

	/* Right-click a result to pin it, the same gesture as on a menu entry.
	 *
	 * This is where you find the thing you did not know the name of, so it is
	 * also where you decide it is worth keeping — making you find it, close the
	 * palette, then hunt down the menu it lives in to right-click it there would
	 * be a strange thing to insist on.
	 */
	view->setContextMenuPolicy( Qt::CustomContextMenu );
	QObject::connect( view, &QTreeView::customContextMenuRequested, &dialog,
		[&, view]( const QPoint & pos ) {
			const QModelIndex hit = view->indexAt( pos );
			if ( !hit.isValid() )
				return;
			auto * action = reinterpret_cast<QAction *>(
				hit.sibling( hit.row(), 0 ).data( Qt::UserRole ).value<quintptr>() );
			const QString id = wwFavouriteId( &book, action );
			QMenu offer( &dialog );
			if ( id.isEmpty() ) {
				QAction * why = offer.addAction( QObject::tr( "This entry has no name to remember it by" ) );
				why->setEnabled( false );
			} else {
				const bool pinned = wwIsFavourite( id );
				QAction * toggle = offer.addAction( pinned
					? QObject::tr( "Remove from Quick Favourites" )
					: QObject::tr( "Add to Quick Favourites" ) );
				QObject::connect( toggle, &QAction::triggered, [id]() { wwToggleFavourite( id ); } );
			}
			offer.exec( view->viewport()->mapToGlobal( pos ) );
		} );

	QObject::connect( edit, &QLineEdit::textChanged, &dialog, [&]() { repopulate(); } );
	QObject::connect( view->selectionModel(), &QItemSelectionModel::currentChanged, &dialog,
		[&]( const QModelIndex & cur, const QModelIndex & ) { showHint( cur ); } );
	QObject::connect( view, &QTreeView::activated, &dialog, [&]() { dialog.accept(); } );

	repopulate();
	view->header()->setStretchLastSection( false );
	view->header()->setSectionResizeMode( 0, QHeaderView::Stretch );
	view->header()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
	view->header()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );

	/* Where the click was, so it opens where the menu it replaces stood rather
	 * than in the middle of the window, which is a long way from the row being
	 * asked about. Clamped to the screen, or a right-click near the bottom edge
	 * opens 420px of palette off the end of it.
	 */
	QRect where( QPoint(), dialog.size() );
	if ( !at.isNull() ) {
		where.moveTopLeft( at );
	} else if ( parent ) {
		where.moveCenter( parent->window()->geometry().center() );
	}
	const QScreen * screen = QGuiApplication::screenAt( at.isNull() ? where.center() : at );
	if ( !screen )
		screen = QGuiApplication::primaryScreen();
	if ( screen ) {
		const QRect fits = screen->availableGeometry();
		where.moveLeft( std::min( where.left(), fits.right() - where.width() ) );
		where.moveTop( std::min( where.top(), fits.bottom() - where.height() ) );
		where.moveLeft( std::max( where.left(), fits.left() ) );
		where.moveTop( std::max( where.top(), fits.top() ) );
	}
	dialog.move( where.topLeft() );
	edit->setFocus();

	if ( dialog.exec() != QDialog::Accepted )
		return nullptr;

	const QModelIndex cur = view->currentIndex();
	if ( !cur.isValid() )
		return nullptr;
	const QModelIndex row = cur.sibling( cur.row(), 0 );
	// an inapplicable row is shown so it can be READ, never run
	if ( !row.data( Qt::UserRole + 2 ).toBool() )
		return nullptr;
	return reinterpret_cast<QAction *>( row.data( Qt::UserRole ).value<quintptr>() );
}
