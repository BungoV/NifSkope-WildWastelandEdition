/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellshowpanel.h"
#include "wwswitch.h"

#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QRadioButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

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

void syncInto( QWidget * panel )
{
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
