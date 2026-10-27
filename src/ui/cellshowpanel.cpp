/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellshowpanel.h"
#include "wwswitch.h"
#include "widgets/wwdropdown.h"	// lane OPENCELLFIX2: the dock's one dropdown

#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QRadioButton>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <algorithm>
namespace {

bool red( const char * which )
{
	return wwEnvStr( "WW_CELLSHOW_RED" ) == QLatin1String( which );
}

//! The release that would close the menu triggers the row instead (as src/nifskope_ui.cpp PersistentActionMenu).
class StayOpen final : public QObject
{
public:
	using QObject::QObject;

protected:
	bool eventFilter( QObject * o, QEvent * e ) override
	{
		auto * m = qobject_cast<QMenu *>( o );
		if ( !m )
			return false;
		if ( e->type() == QEvent::MouseButtonRelease ) {
			auto * me = static_cast<QMouseEvent *>( e );
			QAction * a = m->actionAt( me->position().toPoint() );
			if ( me->button() == Qt::LeftButton && a && a->isEnabled() && a->isCheckable() && !a->menu() ) {
				a->trigger();
				return true;
			}
		} else if ( e->type() == QEvent::KeyPress ) {
			auto * ke = static_cast<QKeyEvent *>( e );
			QAction * a = m->activeAction();
			if ( a && a->isEnabled() && a->isCheckable() && !a->menu()
				&& ( ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter || ke->key() == Qt::Key_Space ) ) {
				a->trigger();
				return true;
			}
		}
		return false;
	}
};

void installStayOpen( QMenu * m, QObject * filter )
{
	m->installEventFilter( filter );
	for ( QAction * a : m->actions() )
		if ( a->menu() )
			installStayOpen( a->menu(), filter );
}

//! A field in the menu and its copy in the panel follow each other's enabled state.
class EnabledEcho final : public QObject
{
public:
	EnabledEcho( QWidget * from, const QList<QWidget *> & to ) : QObject( from )
	{
		for ( QWidget * w : to )
			mirrors.append( w );
	}
	void apply( QWidget * from )
	{
		for ( QWidget * w : mirrors )
			if ( w )
				w->setEnabled( from->isEnabled() );
	}

protected:
	bool eventFilter( QObject * o, QEvent * e ) override
	{
		if ( e->type() == QEvent::EnabledChange )
			apply( static_cast<QWidget *>( o ) );
		return false;
	}

private:
	QList<QPointer<QWidget>> mirrors;
};

struct Builder
{
	const WwMenuPanelStyle & style;
	QWidget * root;
	bool noSync = red( "sync" );

	//! One fold: an arrow, the title, a body. Returns the body's layout.
	QVBoxLayout * fold( QVBoxLayout * into, const QString & title, const QString & id, bool heading, QAction * enabler )
	{
		auto * head = new QWidget;
		auto * hl = new QHBoxLayout( head );
		hl->setContentsMargins( 0, heading ? 4 : 0, 0, 0 );
		hl->setSpacing( 2 );
		auto * arrow = new QToolButton( head );
		arrow->setObjectName( QStringLiteral( "CellShowFold_" ) + id );
		arrow->setAutoRaise( true );
		arrow->setCheckable( true );
		arrow->setToolButtonStyle( Qt::ToolButtonIconOnly );
		hl->addWidget( arrow );
		QLabel * label = heading && style.heading ? style.heading( title, head ) : new QLabel( title, head );
		hl->addWidget( label, 1 );
		into->addWidget( head );
		auto * body = new QWidget;
		body->setObjectName( QStringLiteral( "CellShowBody_" ) + id );
		auto * bl = new QVBoxLayout( body );
		bl->setContentsMargins( 18, 0, 0, 2 );
		bl->setSpacing( 1 );
		into->addWidget( body );
		const QString key = style.foldGroup + QLatin1Char( '/' ) + id;
		const bool open = style.foldGroup.isEmpty() || QSettings().value( key, heading ).toBool();
		auto show = [arrow, body]( bool on ) {
			arrow->setArrowType( on ? Qt::DownArrow : Qt::RightArrow );
			body->setVisible( on );
		};
		arrow->setChecked( open );
		show( open );
		const bool keep = !style.foldGroup.isEmpty();	// by value: the builder is gone once the panel is up
		QObject::connect( arrow, &QToolButton::toggled, arrow, [show, key, keep]( bool on ) {
			show( on );
			if ( keep )
				QSettings().setValue( key, on );
		} );
		if ( enabler ) {	// a submenu greys as one (a Pass that needs a bake)
			auto grey = [head, body, enabler]() {
				head->setEnabled( enabler->isEnabled() );
				body->setEnabled( enabler->isEnabled() );
			};
			grey();
			QObject::connect( enabler, &QAction::changed, head, grey );
		}
		return bl;
	}

	void box( QVBoxLayout * into, QAction * a )
	{
		const bool radio = a->actionGroup() && a->actionGroup()->isExclusive();
		QAbstractButton * b = radio ? static_cast<QAbstractButton *>( new QRadioButton( a->text() ) )
			: static_cast<QAbstractButton *>( new QCheckBox( a->text() ) );
		b->setObjectName( QStringLiteral( "CellShowTab_" ) + ( a->objectName().isEmpty() ? a->text() : a->objectName() ) );
		b->setAutoExclusive( false );	// the action group decides; the box only echoes it
		b->setToolTip( a->toolTip() == a->text() ? QString() : a->toolTip() );
		b->setChecked( a->isChecked() );
		b->setEnabled( a->isEnabled() );
		b->setProperty( "wwShowAction", QVariant::fromValue( static_cast<QObject *>( a ) ) );
		into->addWidget( b );
		QPointer<QAction> pa( a );
		QObject::connect( b, &QAbstractButton::clicked, b, [pa, b]( bool on ) {
			if ( !pa )
				return;
			if ( pa->isChecked() != on )
				pa->trigger();	// the menu's own path: toggled, and the group's triggered
			QSignalBlocker sb( b );
			b->setChecked( pa->isChecked() );	// a refused or exclusive change shows what the action says
		} );
		if ( !noSync ) {
			QObject::connect( a, &QAction::toggled, b, [b]( bool on ) {
				QSignalBlocker sb( b );
				b->setChecked( on );
			} );
			QObject::connect( a, &QAction::changed, b, [a, b]() {
				b->setEnabled( a->isEnabled() );
				b->setText( a->text() );
			} );
		}
	}

	void values( QVBoxLayout * into, QWidget * panel )
	{
		auto * src = qobject_cast<QGridLayout *>( panel->layout() );
		if ( !src )
			return;
		auto * host = new QWidget;
		auto * g = new QGridLayout( host );
		g->setContentsMargins( 0, 0, 0, 0 );
		g->setHorizontalSpacing( 8 );
		g->setVerticalSpacing( 2 );
		g->setColumnMinimumWidth( 0, 96 );
		g->setColumnStretch( 1, 1 );
		int rows = 0;
		for ( int r = 0; r < src->rowCount(); r++ ) {
			QLayoutItem * li = src->itemAtPosition( r, 0 );
			QLayoutItem * fi = src->itemAtPosition( r, 1 );
			auto * l = li ? qobject_cast<QLabel *>( li->widget() ) : nullptr;
			auto * f = fi ? qobject_cast<QDoubleSpinBox *>( fi->widget() ) : nullptr;
			if ( !l || !f )
				continue;
			auto * ml = new QLabel( l->text(), host );
			ml->setToolTip( l->toolTip() );
			auto * mf = new QDoubleSpinBox( host );
			mf->setObjectName( f->objectName() + QStringLiteral( "_Show" ) );
			mf->setDecimals( f->decimals() );
			mf->setRange( f->minimum(), f->maximum() );
			mf->setSingleStep( f->singleStep() );
			mf->setPrefix( f->prefix() );
			mf->setSuffix( f->suffix() );
			mf->setToolTip( f->toolTip() );
			mf->setValue( f->value() );
			mf->setProperty( "wwShowField", QVariant::fromValue( static_cast<QObject *>( f ) ) );
			if ( style.number )
				style.number( mf );
			g->addWidget( ml, rows, 0 );
			g->addWidget( mf, rows, 1 );
			rows++;
			QPointer<QDoubleSpinBox> pf( f );
			QObject::connect( mf, qOverload<double>( &QDoubleSpinBox::valueChanged ), mf, [pf]( double v ) {
				if ( pf && pf->value() != v )
					pf->setValue( v );	// the menu field's own valueChanged applies it
			} );
			if ( !noSync )
				QObject::connect( f, qOverload<double>( &QDoubleSpinBox::valueChanged ), mf, [mf]( double v ) {
					QSignalBlocker sb( mf );
					mf->setValue( v );
				} );
			auto * echo = new EnabledEcho( f, { ml, mf } );
			echo->apply( f );
			f->installEventFilter( echo );
		}
		if ( rows )
			into->addWidget( host );
		else
			delete host;
	}

	void walk( QMenu * m, QVBoxLayout * top, QVBoxLayout * into )
	{
		QVBoxLayout * cur = into;
		for ( QAction * a : m->actions() ) {
			if ( a->isSeparator() )
				continue;
			auto * wa = qobject_cast<QWidgetAction *>( a );
			QWidget * dw = wa ? wa->defaultWidget() : nullptr;
			if ( dw && qobject_cast<QLabel *>( dw ) ) {
				// a heading row: a new top-level fold
				QString id = a->objectName().section( QLatin1Char( '_' ), -1 );
				if ( id.isEmpty() )
					id = a->text();
				cur = fold( top, static_cast<QLabel *>( dw )->text(), id, true, nullptr );
			} else if ( dw ) {
				values( cur, dw );
			} else if ( a->menu() ) {
				const QString id = a->menu()->objectName().isEmpty() ? a->text() : a->menu()->objectName();
				walk( a->menu(), top, fold( cur, a->text(), id, false, a ) );
			} else if ( a->isCheckable() ) {
				box( cur, a );
			}
		}
	}
};

//! A Display row's "read everything again" (wwMenuPanelSync calls it; the row's own signals call it too).
class RowSync final : public QObject
{
public:
	RowSync( QObject * parent, std::function<void()> f ) : QObject( parent ), fn( std::move( f ) )
	{
		setObjectName( QStringLiteral( "wwRowSync" ) );	// found by name: a .cpp-local class has no Q_OBJECT (no moc)
	}
	std::function<void()> fn;
};

void syncInto( QWidget * panel )
{
	for ( QObject * o : panel->findChildren<QObject *>( QStringLiteral( "wwRowSync" ) ) )
		static_cast<RowSync *>( o )->fn();
	for ( QAbstractButton * b : panel->findChildren<QAbstractButton *>() ) {
		auto * a = qobject_cast<QAction *>( b->property( "wwShowAction" ).value<QObject *>() );
		if ( !a )
			continue;
		QSignalBlocker sb( b );
		b->setChecked( a->isChecked() );
		b->setEnabled( a->isEnabled() );
	}
	for ( QDoubleSpinBox * mf : panel->findChildren<QDoubleSpinBox *>() ) {
		auto * f = qobject_cast<QDoubleSpinBox *>( mf->property( "wwShowField" ).value<QObject *>() );
		if ( !f )
			continue;
		QSignalBlocker sb( mf );
		mf->setValue( f->value() );
	}
}

} // namespace

void wwKeepMenuOpenOnTick( QMenu * menu )
{
	if ( !menu || red( "menu" ) )
		return;
	installStayOpen( menu, new StayOpen( menu ) );
}

QWidget * wwMenuAsPanel( QMenu * menu, QWidget * parent, const WwMenuPanelStyle & style )
{
	auto * panel = new QWidget( parent );
	panel->setObjectName( QStringLiteral( "CellShowPanel" ) );
	auto * top = new QVBoxLayout( panel );
	top->setContentsMargins( 0, 0, 0, 0 );
	top->setSpacing( 2 );
	Builder b{ style, panel };
	if ( menu )
		b.walk( menu, top, top );
	top->addStretch( 1 );
	return panel;
}

void wwMenuPanelSync( QWidget * panel )
{
	if ( panel && !red( "sync" ) )
		syncInto( panel );
}

// ---- lane OPENCELLFIX2: the Display tab and its presets

namespace {

const char * const kPresetGroup = "CellEditor/DisplayPresets";
const char * const kPresetCurrent = "CellEditor/DisplayPreset";
const char * const kLastGroup = "CellEditor/DisplayLast";	// the level a row comes back to when ticked again

QString trp( const char * s )
{
	return QCoreApplication::translate( "CellDisplayPanel", s );
}

QString groupId( QString g )
{
	return g.remove( QLatin1Char( ' ' ) );
}

QAction * findAction( QMenu * m, const QString & name )
{
	if ( !m || name.isEmpty() )
		return nullptr;
	for ( QAction * a : m->actions() ) {
		if ( a->objectName() == name || ( a->menu() && a->menu()->objectName() == name ) )
			return a;
		if ( a->menu() )
			if ( QAction * r = findAction( a->menu(), name ) )
				return r;
	}
	return nullptr;
}

QList<QAction *> exclusiveItems( QMenu * m )
{
	QList<QAction *> out;
	for ( QAction * a : m->actions() )
		if ( a->isCheckable() && !a->menu() && a->actionGroup() && a->actionGroup()->isExclusive() )
			out.append( a );
	return out;
}

bool menuOpen( const QMenu * m )
{
	return m && m->isEnabled() && m->menuAction()->isEnabled();
}

int checkedIn( const QList<QAction *> & items )
{
	for ( int i = 0; i < int( items.size() ); i++ )
		if ( items.at( i )->isChecked() )
			return i;
	return -1;
}

//! Every checkable action of the tree with its preset key: its objectName, else "<menu>/<index>".
void collectActions( QMenu * m, QList<QPair<QString, QAction *>> & out )
{
	const QList<QAction *> acts = m->actions();
	for ( int i = 0; i < int( acts.size() ); i++ ) {
		QAction * a = acts.at( i );
		if ( a->menu() )
			collectActions( a->menu(), out );
		else if ( a->isCheckable() && !qobject_cast<QWidgetAction *>( a ) )
			out.append( { a->objectName().isEmpty() ? m->objectName() + QLatin1Char( '/' ) + QString::number( i )
				: a->objectName(), a } );
	}
}

void collectFields( QMenu * m, QList<QDoubleSpinBox *> & out )
{
	for ( QAction * a : m->actions() ) {
		if ( a->menu() )
			collectFields( a->menu(), out );
		else if ( auto * wa = qobject_cast<QWidgetAction *>( a ) )
			if ( wa->defaultWidget() )
				for ( QDoubleSpinBox * f : wa->defaultWidget()->findChildren<QDoubleSpinBox *>() )
					if ( !f->objectName().isEmpty() )
						out.append( f );
	}
}

bool actionReachable( QAction * a )
{
	auto * m = qobject_cast<QMenu *>( a->parent() );
	return a->isEnabled() && ( !m || menuOpen( m ) );
}

QMenu * menuOf( const QWidget * panel )
{
	return panel ? qobject_cast<QMenu *>( panel->property( "wwDisplayMenu" ).value<QObject *>() ) : nullptr;
}

void unstyleRed( QToolButton * d )
{
	if ( red( "dropdown" ) ) {	// red: a dropdown without the Object Mode selector's look
		d->setStyleSheet( QString() );
		d->setAutoRaise( true );
	}
}

void refreshPresetBar( QWidget * panel )
{
	if ( !panel )
		return;
	auto * pick = panel->findChild<WwDropdown *>( QStringLiteral( "CellDisplayPresetPicker" ) );
	if ( !pick )
		return;
	const QStringList names = wwDisplayPresetNames();
	const QString cur = wwDisplayPresetCurrent();
	{
		QSignalBlocker sb( pick );
		pick->clear();
		pick->addItems( names );
		pick->setCurrentIndex( int( names.indexOf( cur ) ) );
		if ( pick->currentIndex() < 0 )
			pick->setText( names.isEmpty() ? trp( "No presets" ) : trp( "Presets" ) );
	}
	pick->setEnabled( !names.isEmpty() );
	const bool have = names.contains( cur );
	for ( const char * n : { "CellDisplayPresetNext" } )
		if ( auto * b = panel->findChild<QToolButton *>( QLatin1String( n ) ) )
			b->setEnabled( !names.isEmpty() );
	for ( const char * n : { "CellDisplayPresetRename", "CellDisplayPresetDelete" } )
		if ( auto * b = panel->findChild<QToolButton *>( QLatin1String( n ) ) )
			b->setEnabled( have );
}

void presetBar( QWidget * panel, QVBoxLayout * top )
{
	auto * bar = new QWidget( panel );
	bar->setObjectName( QStringLiteral( "CellDisplayPresetBar" ) );
	auto * hl = new QHBoxLayout( bar );
	hl->setContentsMargins( 0, 0, 0, 4 );
	hl->setSpacing( 4 );
	hl->addWidget( new QLabel( trp( "Preset" ), bar ) );
	auto * pick = new WwDropdown( bar );
	pick->setObjectName( QStringLiteral( "CellDisplayPresetPicker" ) );
	unstyleRed( pick );
	hl->addWidget( pick, 1 );
	auto button = [bar, hl]( const char * name, const QString & text ) {
		auto * b = new QToolButton( bar );
		b->setObjectName( QLatin1String( name ) );
		b->setText( text );
		b->setToolButtonStyle( Qt::ToolButtonTextOnly );
		hl->addWidget( b );
		return b;
	};
	QToolButton * next = button( "CellDisplayPresetNext", trp( "Next" ) );
	QToolButton * save = button( "CellDisplayPresetSave", trp( "Save" ) );
	QToolButton * ren = button( "CellDisplayPresetRename", trp( "Rename" ) );
	QToolButton * del = button( "CellDisplayPresetDelete", trp( "Delete" ) );
	top->addWidget( bar );
	QPointer<QWidget> p( panel );
	QObject::connect( pick, &WwDropdown::activated, bar, [p, pick]( int i ) { wwDisplayPresetApply( p, pick->itemText( i ) ); } );
	QObject::connect( next, &QToolButton::clicked, bar, [p]() { wwDisplayPresetNext( p ); } );
	QObject::connect( save, &QToolButton::clicked, bar, [p]() {
		bool ok = false;
		const QString n = QInputDialog::getText( p, trp( "Save preset" ), trp( "Name" ), QLineEdit::Normal,
			wwDisplayPresetCurrent(), &ok );
		if ( ok )
			wwDisplayPresetSave( p, n );	// the same name overwrites
	} );
	QObject::connect( ren, &QToolButton::clicked, bar, [p]() {
		const QString cur = wwDisplayPresetCurrent();
		bool ok = false;
		const QString n = QInputDialog::getText( p, trp( "Rename preset" ), trp( "Name" ), QLineEdit::Normal, cur, &ok );
		if ( ok )
			wwDisplayPresetRename( p, cur, n );
	} );
	QObject::connect( del, &QToolButton::clicked, bar, [p]() { wwDisplayPresetDelete( p, wwDisplayPresetCurrent() ); } );
	refreshPresetBar( panel );
}

struct DisplayBuilder
{
	QMenu * menu;
	Builder & b;
	QVBoxLayout * top;
	QSet<QAction *> used;
	QHash<QString, QVBoxLayout *> groups;
	QVBoxLayout * fusion = nullptr;
	const bool noTicks = red( "ticks" );
	const bool oneGroup = red( "groups" );
	int leftovers = 0;

	QVBoxLayout * group( const QString & g0 )
	{
		const QString g = oneGroup ? trp( "Display" ) : g0;
		if ( !groups.contains( g ) )
			groups.insert( g, b.fold( top, g, groupId( g ), true, nullptr ) );
		return groups.value( g );
	}

	void markMenu( QAction * ma )
	{
		used.insert( ma );
		for ( QAction * a : ma->menu()->actions() ) {
			used.insert( a );
			if ( a->menu() )
				markMenu( a );
		}
	}

	WwDropdown * dropdown( const QString & name, const QList<QAction *> & items, bool quality )
	{
		auto * d = new WwDropdown;
		d->setObjectName( name );
		d->setProperty( "wwQuality", quality );
		for ( QAction * a : items )
			d->addItem( a->text() );
		unstyleRed( d );
		return d;
	}

	//! A submenu folded under a row: its choices as a label | dropdown row, its other rows as they are.
	void extra( QVBoxLayout * into, QAction * a )
	{
		used.insert( a );
		auto * wa = qobject_cast<QWidgetAction *>( a );
		if ( wa && wa->defaultWidget() && !qobject_cast<QLabel *>( wa->defaultWidget() ) ) {
			b.values( into, wa->defaultWidget() );
		} else if ( a->menu() ) {
			markMenu( a );
			QMenu * m = a->menu();
			const QList<QAction *> items = exclusiveItems( m );
			if ( !items.isEmpty() ) {
				auto * w = new QWidget;
				auto * hl = new QHBoxLayout( w );
				hl->setContentsMargins( 0, 0, 0, 0 );
				hl->setSpacing( 6 );
				auto * l = new QLabel( a->text(), w );
				WwDropdown * d = dropdown( QStringLiteral( "CellDisplayChoice_" ) + m->objectName(), items, false );
				hl->addWidget( l, 1 );
				hl->addWidget( d );
				into->addWidget( w );
				QPointer<QMenu> pm( m );
				auto sync = [pm, items, d, l]() {
					if ( !pm )
						return;
					QSignalBlocker sb( d );
					d->setCurrentIndex( checkedIn( items ) );
					d->setEnabled( menuOpen( pm ) );
					l->setEnabled( menuOpen( pm ) );
				};
				QObject::connect( d, &WwDropdown::activated, d, [items, sync]( int i ) {
					items.at( i )->trigger();
					sync();
				} );
				hook( w, sync, items, a );
			}
			for ( QAction * s : m->actions() )
				if ( !s->isSeparator() && !items.contains( s ) )
					extra( into, s );
		} else if ( a->isCheckable() ) {
			b.box( into, a );
		}
	}

	//! The row reads everything again on any change of what it shows.
	void hook( QWidget * w, const std::function<void()> & sync, const QList<QAction *> & acts, QAction * menuAct )
	{
		sync();
		new RowSync( w, sync );
		if ( b.noSync )
			return;
		for ( QAction * a : acts ) {
			QObject::connect( a, &QAction::toggled, w, sync );
			QObject::connect( a, &QAction::changed, w, sync );
		}
		if ( menuAct )
			QObject::connect( menuAct, &QAction::changed, w, sync );
	}

	void row( const WwDisplayRow & r )
	{
		QAction * t = findAction( menu, r.tick );
		if ( t && ( t->menu() || !t->isCheckable() ) )
			t = nullptr;
		QAction * cm = findAction( menu, r.choices );
		QMenu * c = cm && cm->menu() ? cm->menu() : nullptr;
		auto * custom = qobject_cast<WwDropdown *>( r.dropdown );
		if ( !t && !c && !custom )
			return;	// not in this menu
		rowOf( group( r.group ), r, t, c, custom );
	}

	void rowOf( QVBoxLayout * into, const WwDisplayRow & r, QAction * t, QMenu * c, WwDropdown * custom )
	{
		const QString key = t ? t->objectName() : c ? c->objectName() : custom->objectName();
		QString label = r.label;
		if ( label.isEmpty() )
			label = t ? t->text() : c ? c->title() : QString();
		const QList<QAction *> items = c ? exclusiveItems( c ) : QList<QAction *>();
		if ( t )
			used.insert( t );
		if ( c )
			markMenu( c->menuAction() );
		const int off = !t && c ? r.off : -1;
		QList<QAction *> list = items;
		if ( off >= 0 && off < int( items.size() ) )
			list.removeAt( off );

		auto * w = new QWidget;
		w->setObjectName( QStringLiteral( "CellDisplayRow_" ) + key );
		w->setProperty( "wwDisplayRow", true );
		auto * hl = new QHBoxLayout( w );
		hl->setContentsMargins( 0, 0, 0, 0 );
		hl->setSpacing( 6 );
		WwDropdown * d = custom;
		if ( !d && list.size() >= 2 )
			d = dropdown( QStringLiteral( "CellDisplayChoice_" ) + key, list, r.quality );
		QCheckBox * box = nullptr;
		if ( d && noTicks ) {	// red: a row with settings has only its dropdown
			hl->addWidget( new QLabel( label, w ), 1 );
		} else {
			box = new QCheckBox( label, w );
			box->setObjectName( QStringLiteral( "CellDisplayTick_" ) + key );
			hl->addWidget( box, 1 );
		}
		if ( d ) {
			d->setParent( w );
			hl->addWidget( d );
		}
		into->addWidget( w );
		const QString lastKey = QLatin1String( kLastGroup ) + QLatin1Char( '/' ) + key;

		if ( custom ) {	// the dock's own dropdown (Color): its off item is hidden, the tick picks it
			custom->setProperty( "wwPresetKey", custom->objectName() );
			if ( QAction * oa = custom->itemMenu()->actions().value( r.off ) )
				oa->setVisible( false );
			const int cOff = r.off;
			auto sync = [custom, box, cOff]() {
				const bool on = custom->currentIndex() != cOff;
				if ( box ) {
					QSignalBlocker sb( box );
					box->setChecked( on );
				}
				custom->setEnabled( on || !box );
			};
			if ( box )
				QObject::connect( box, &QCheckBox::clicked, w, [custom, cOff, lastKey, sync]( bool on ) {
					const int cur = custom->currentIndex();
					if ( on && cur == cOff ) {
						int i = custom->findText( QSettings().value( lastKey ).toString() );
						if ( i < 0 || i == cOff )
							i = cOff == 0 ? 1 : 0;
						custom->setCurrentIndex( i );	// its own currentIndexChanged applies it
					} else if ( !on && cur != cOff ) {
						QSettings().setValue( lastKey, custom->currentText() );
						custom->setCurrentIndex( cOff );
					}
					sync();
				} );
			QObject::connect( custom, &WwDropdown::currentIndexChanged, w, sync );
			sync();
			new RowSync( w, sync );
		} else if ( t ) {	// a tick action, its choices (if any) beside it
			QPointer<QMenu> pc( c );
			auto sync = [t, box, d, list, pc]() {
				if ( box ) {
					QSignalBlocker sb( box );
					box->setChecked( t->isChecked() );
					box->setEnabled( t->isEnabled() );
				}
				if ( d ) {
					QSignalBlocker sb( d );
					d->setCurrentIndex( checkedIn( list ) );
					d->setEnabled( ( t->isChecked() || !box ) && t->isEnabled() && ( !pc || menuOpen( pc ) ) );
				}
			};
			if ( box )
				QObject::connect( box, &QCheckBox::clicked, w, [t, sync]( bool on ) {
					if ( t->isChecked() != on )
						t->trigger();	// the menu's own path
					sync();
				} );
			if ( d )
				QObject::connect( d, &WwDropdown::activated, w, [list, sync]( int i ) {
					list.at( i )->trigger();
					sync();
				} );
			QList<QAction *> watch = list;
			watch.append( t );
			hook( w, sync, watch, c ? c->menuAction() : nullptr );
		} else {	// a submenu alone: its off item is the tick
			QAction * offAct = items.value( off );
			QPointer<QMenu> pc( c );
			auto sync = [offAct, box, d, list, pc, lastKey]() {
				if ( !pc )
					return;
				const bool on = !offAct || !offAct->isChecked();
				if ( box ) {
					QSignalBlocker sb( box );
					box->setChecked( on );
					box->setEnabled( menuOpen( pc ) );
				}
				if ( d ) {
					QSignalBlocker sb( d );
					int i = checkedIn( list );
					if ( i < 0 )	// off: show the level a tick brings back
						i = std::max( 0, d->findText( QSettings().value( lastKey ).toString() ) );
					d->setCurrentIndex( i );
					d->setEnabled( ( on || !box ) && menuOpen( pc ) );
				}
			};
			if ( box && offAct )
				QObject::connect( box, &QCheckBox::clicked, w, [offAct, list, lastKey, sync]( bool on ) {
					const int cur = checkedIn( list );
					if ( on && cur < 0 ) {
						QAction * pick = nullptr;
						const QString last = QSettings().value( lastKey ).toString();
						for ( QAction * a : list )
							if ( !pick && ( a->text() == last ) )
								pick = a;
						for ( QAction * a : list )	// first time: Medium, else the first level
							if ( !pick && a->text() == trp( "Medium" ) )
								pick = a;
						if ( !pick && !list.isEmpty() )
							pick = list.first();
						if ( pick )
							pick->trigger();
					} else if ( !on && cur >= 0 ) {
						QSettings().setValue( lastKey, list.at( cur )->text() );
						offAct->trigger();
					}
					sync();
				} );
			if ( d )
				QObject::connect( d, &WwDropdown::activated, w, [list, lastKey, sync]( int i ) {
					QSettings().setValue( lastKey, list.at( i )->text() );
					list.at( i )->trigger();
					sync();
				} );
			hook( w, sync, items, c->menuAction() );
		}

		// what is finer than the row folds under it: the submenu's other rows and the named extras
		QList<QAction *> more;
		if ( c )
			for ( QAction * a : c->actions() )
				if ( !a->isSeparator() && !items.contains( a ) )
					more.append( a );
		for ( const QString & n : r.extras )
			if ( QAction * a = findAction( menu, n ) )
				if ( !more.contains( a ) )
					more.append( a );
		if ( !more.isEmpty() ) {
			QVBoxLayout * f = b.fold( into, trp( "%1 settings" ).arg( label ), QStringLiteral( "settings_" ) + key, false,
				c ? c->menuAction() : nullptr );
			for ( QAction * a : more )
				extra( f, a );
		}
	}

	//! A switch no row names still gets one (and is counted, so a new switch cannot hide).
	void leftover( QMenu * m )
	{
		for ( QAction * a : m->actions() ) {
			if ( used.contains( a ) || a->isSeparator() )
				continue;
			auto * wa = qobject_cast<QWidgetAction *>( a );
			if ( wa && wa->defaultWidget() && qobject_cast<QLabel *>( wa->defaultWidget() ) )
				continue;	// a menu heading; the groups replace them
			if ( a->menu() && exclusiveItems( a->menu() ).isEmpty() ) {
				used.insert( a );
				leftover( a->menu() );
				continue;
			}
			leftovers++;
			if ( a->isCheckable() && !wa && !a->menu() ) {
				WwDisplayRow r;
				rowOf( group( trp( "Fusion Shaders" ) ), r, a, nullptr, nullptr );
			} else {
				extra( group( trp( "Fusion Shaders" ) ), a );
			}
		}
	}
};

} // namespace

QStringList wwDisplayGroups()
{
	return { trp( "Editor" ), trp( "Vanilla" ), trp( "Fusion Shaders" ) };
}

QWidget * wwMenuAsDisplay( QMenu * menu, const QList<WwDisplayRow> & rows, QWidget * parent, const WwMenuPanelStyle & style )
{
	auto * panel = new QWidget( parent );
	panel->setObjectName( QStringLiteral( "CellDisplayPanel" ) );
	panel->setProperty( "wwDisplayMenu", QVariant::fromValue( static_cast<QObject *>( menu ) ) );
	auto * top = new QVBoxLayout( panel );
	top->setContentsMargins( 0, 0, 0, 0 );
	top->setSpacing( 2 );
	presetBar( panel, top );
	Builder b{ style, panel };
	DisplayBuilder d{ menu, b, top };
	if ( !d.oneGroup )
		for ( const QString & g : wwDisplayGroups() )
			d.group( g );	// the fixed order
	for ( const WwDisplayRow & r : rows )
		d.row( r );
	if ( menu )
		d.leftover( menu );
	panel->setProperty( "wwDisplayLeftovers", d.leftovers );
	top->addStretch( 1 );
	return panel;
}

int wwDisplayLeftovers( const QWidget * panel )
{
	return panel ? panel->property( "wwDisplayLeftovers" ).toInt() : -1;
}

QVariantMap wwDisplayState( const QWidget * panel )
{
	QVariantMap s;
	QMenu * m = menuOf( panel );
	if ( !m )
		return s;
	QList<QPair<QString, QAction *>> acts;
	collectActions( m, acts );
	for ( const auto & ka : acts )
		s.insert( QStringLiteral( "tick:" ) + ka.first, ka.second->isChecked() );
	QList<QDoubleSpinBox *> fields;
	collectFields( m, fields );
	for ( QDoubleSpinBox * f : fields )
		s.insert( QStringLiteral( "field:" ) + f->objectName(), f->value() );
	for ( WwDropdown * d : panel->findChildren<WwDropdown *>() ) {
		const QString k = d->property( "wwPresetKey" ).toString();
		if ( !k.isEmpty() )
			s.insert( QStringLiteral( "pick:" ) + k, d->currentText() );
	}
	return s;
}

void wwDisplaySetState( QWidget * panel, const QVariantMap & st )
{
	QMenu * m = menuOf( panel );
	if ( !m )
		return;
	QList<QPair<QString, QAction *>> acts;
	collectActions( m, acts );
	for ( int pass = 0; pass < 2; pass++ ) {	// twice: a tick can enable another
		for ( const auto & ka : acts ) {
			const auto it = st.constFind( QStringLiteral( "tick:" ) + ka.first );
			QAction * a = ka.second;
			if ( it == st.constEnd() || a->isChecked() == it->toBool() || !actionReachable( a ) )
				continue;
			if ( a->actionGroup() && a->actionGroup()->isExclusive() && !it->toBool() )
				continue;	// its group's chosen item turns it off
			a->trigger();	// the menu's own path: live
		}
	}
	QList<QDoubleSpinBox *> fields;
	collectFields( m, fields );
	for ( QDoubleSpinBox * f : fields ) {
		const auto it = st.constFind( QStringLiteral( "field:" ) + f->objectName() );
		if ( it != st.constEnd() && f->value() != it->toDouble() )
			f->setValue( it->toDouble() );
	}
	for ( WwDropdown * d : panel->findChildren<WwDropdown *>() ) {
		const QString k = d->property( "wwPresetKey" ).toString();
		const auto it = st.constFind( QStringLiteral( "pick:" ) + k );
		if ( k.isEmpty() || it == st.constEnd() )
			continue;
		const int i = d->findText( it->toString() );
		if ( i >= 0 && i != d->currentIndex() )
			d->setCurrentIndex( i );
	}
	syncInto( panel );
}

static bool presetName( const QString & n )
{
	return !n.trimmed().isEmpty() && !n.contains( QLatin1Char( '/' ) ) && !n.contains( QLatin1Char( '\\' ) );
}

static QString presetKey( const QString & n )
{
	return QLatin1String( kPresetGroup ) + QLatin1Char( '/' ) + n.trimmed();
}

QStringList wwDisplayPresetNames()
{
	QSettings s;
	s.beginGroup( QLatin1String( kPresetGroup ) );
	QStringList n = s.childKeys();
	std::sort( n.begin(), n.end(), []( const QString & a, const QString & b ) {
		return a.compare( b, Qt::CaseInsensitive ) < 0;
	} );
	return n;
}

QString wwDisplayPresetCurrent()
{
	return QSettings().value( QLatin1String( kPresetCurrent ) ).toString();
}

bool wwDisplayPresetSave( QWidget * panel, const QString & name )
{
	if ( !presetName( name ) || !menuOf( panel ) )
		return false;
	if ( !red( "presets" ) ) {	// red: nothing is kept
		QSettings s;
		s.setValue( presetKey( name ), wwDisplayState( panel ) );
		s.setValue( QLatin1String( kPresetCurrent ), name.trimmed() );
	}
	refreshPresetBar( panel );
	return true;
}

bool wwDisplayPresetApply( QWidget * panel, const QString & name )
{
	QSettings s;
	const QVariant v = s.value( presetKey( name ) );
	if ( !presetName( name ) || !v.isValid() || !menuOf( panel ) )
		return false;
	wwDisplaySetState( panel, v.toMap() );
	s.setValue( QLatin1String( kPresetCurrent ), name.trimmed() );
	refreshPresetBar( panel );
	return true;
}

bool wwDisplayPresetRename( QWidget * panel, const QString & from, const QString & to )
{
	QSettings s;
	if ( !presetName( from ) || !presetName( to ) || !s.contains( presetKey( from ) ) || s.contains( presetKey( to ) ) )
		return false;
	s.setValue( presetKey( to ), s.value( presetKey( from ) ) );
	s.remove( presetKey( from ) );
	if ( wwDisplayPresetCurrent() == from.trimmed() )
		s.setValue( QLatin1String( kPresetCurrent ), to.trimmed() );
	refreshPresetBar( panel );
	return true;
}

bool wwDisplayPresetDelete( QWidget * panel, const QString & name )
{
	QSettings s;
	if ( !presetName( name ) || !s.contains( presetKey( name ) ) )
		return false;
	s.remove( presetKey( name ) );
	if ( wwDisplayPresetCurrent() == name.trimmed() )
		s.remove( QLatin1String( kPresetCurrent ) );
	refreshPresetBar( panel );
	return true;
}

QString wwDisplayPresetNext( QWidget * panel )
{
	const QStringList names = wwDisplayPresetNames();
	if ( names.isEmpty() )
		return QString();
	const QString n = names.at( ( names.indexOf( wwDisplayPresetCurrent() ) + 1 ) % names.size() );
	return wwDisplayPresetApply( panel, n ) ? n : QString();
}
