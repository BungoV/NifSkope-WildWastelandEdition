// lane OPENCELLFIX1 unit test: the Cell dock's Show tab (src/ui/cellshowpanel.cpp), offscreen.
// Prints one line per check, then PASS or FAIL; exit code 0 only on PASS.
// Red: WW_CELLSHOW_RED=menu (a tick closes the menu again) must FAIL.
// Red: WW_CELLSHOW_RED=sync (an action that changes leaves its box behind) must FAIL.

#include "cellshowpanel.h"

#include <QAbstractButton>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QRadioButton>
#include <QSettings>
#include <QStandardPaths>
#include <QToolButton>
#include <QWidgetAction>

#include <cstdio>

namespace {

int checks = 0, fails = 0;

void check( bool ok, const char * what )
{
	checks++;
	fails += !ok;
	std::printf( "  %s %s\n", ok ? "ok  " : "FAIL", what );
}

QAction * tick( QMenu * m, const char * name, bool on )
{
	QAction * a = m->addAction( QString::fromLatin1( name ) );
	a->setObjectName( QString::fromLatin1( name ) );
	a->setCheckable( true );
	a->setChecked( on );
	return a;
}

void heading( QMenu * m, const char * text, const char * id )
{
	auto * wa = new QWidgetAction( m );
	wa->setObjectName( QStringLiteral( "ShowHeading_" ) + QString::fromLatin1( id ) );
	wa->setDefaultWidget( new QLabel( QString::fromLatin1( text ) ) );
	m->addAction( wa );
}

//! The menu the Cell dock builds, in miniature: headings, ticks, a radio group, a submenu, a value row.
struct Menu
{
	QMenu menu;
	QAction * fog = nullptr, * sky = nullptr, * radA = nullptr, * radB = nullptr, * inner = nullptr;
	QMenu * sub = nullptr;
	QDoubleSpinBox * field = nullptr;

	Menu()
	{
		heading( &menu, "World", "World" );
		fog = tick( &menu, "Fog", false );
		sky = tick( &menu, "Sky", true );
		heading( &menu, "Lighting", "Lighting" );
		auto * g = new QActionGroup( &menu );
		g->setExclusive( true );
		radA = tick( &menu, "ModeA", true );
		radB = tick( &menu, "ModeB", false );
		g->addAction( radA );
		g->addAction( radB );
		sub = menu.addMenu( QStringLiteral( "Pass" ) );
		sub->setObjectName( QStringLiteral( "PassMenu" ) );
		inner = tick( sub, "Inner", false );
		auto * host = new QWidget;
		auto * gl = new QGridLayout( host );
		gl->addWidget( new QLabel( QStringLiteral( "Strength" ) ), 0, 0 );
		field = new QDoubleSpinBox;
		field->setObjectName( QStringLiteral( "Strength" ) );
		field->setRange( 0, 10 );
		field->setValue( 1.5 );
		gl->addWidget( field, 0, 1 );
		auto * wa = new QWidgetAction( &menu );
		wa->setDefaultWidget( host );
		menu.addAction( wa );
	}
};

template <typename T>
T * box( QWidget * panel, const char * name )
{
	return panel->findChild<T *>( QStringLiteral( "CellShowTab_" ) + QString::fromLatin1( name ) );
}

void click( QMenu * m, QAction * a )
{
	const QPointF at = QRectF( m->actionGeometry( a ) ).center();
	QMouseEvent press( QEvent::MouseButtonPress, at, m->mapToGlobal( at ), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
	QApplication::sendEvent( m, &press );
	QMouseEvent release( QEvent::MouseButtonRelease, at, m->mapToGlobal( at ), Qt::LeftButton, Qt::NoButton, Qt::NoModifier );
	QApplication::sendEvent( m, &release );
	QApplication::processEvents();
}

} // namespace

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );   // every line reaches the log even if a check crashes
	qputenv( "QT_QPA_PLATFORM", "offscreen" );
	QStandardPaths::setTestModeEnabled( true );
	QApplication app( argc, argv );
	QApplication::setOrganizationName( QStringLiteral( "cellshowtest" ) );
	QApplication::setApplicationName( QStringLiteral( "cellshowtest" ) );
	QSettings().clear();

	std::printf( "the Show menu stays open on a tick\n" );
	{
		Menu m;
		wwKeepMenuOpenOnTick( &m.menu );
		m.menu.popup( QPoint( 100, 100 ) );
		QApplication::processEvents();
		check( m.menu.isVisible(), "the menu is up (floor)" );
		click( &m.menu, m.fog );
		check( m.fog->isChecked(), "a click ticks the row" );
		check( m.menu.isVisible(), "the menu stays open after a tick" );
		click( &m.menu, m.fog );
		check( !m.fog->isChecked() && m.menu.isVisible(), "a second click unticks it, menu still open" );
		m.menu.hide();
	}

	std::printf( "the Show tab drives the menu and follows it\n" );
	WwMenuPanelStyle style;
	style.foldGroup = QStringLiteral( "Test/Folds" );
	{
		Menu m;
		QWidget * panel = wwMenuAsPanel( &m.menu, nullptr, style );
		auto * fog = box<QCheckBox>( panel, "Fog" );
		auto * sky = box<QCheckBox>( panel, "Sky" );
		auto * radB = box<QRadioButton>( panel, "ModeB" );
		auto * inner = box<QCheckBox>( panel, "Inner" );
		check( fog && sky && inner, "a check box per ticked row, the submenu's too" );
		check( radB != nullptr, "an exclusive group's rows are radio buttons" );
		check( sky && sky->isChecked() && fog && !fog->isChecked(), "the boxes start as the actions are" );
		if ( fog )
			fog->click();
		check( m.fog->isChecked(), "a box drives its action" );
		m.sky->setChecked( false );
		check( sky && !sky->isChecked(), "an action changed elsewhere moves its box" );
		if ( radB )
			radB->click();
		check( m.radB->isChecked() && !m.radA->isChecked(), "a radio picks in the action group" );
		m.sub->menuAction()->setEnabled( false );
		auto * fold = panel->findChild<QWidget *>( QStringLiteral( "CellShowBody_PassMenu" ) );
		check( fold && !fold->isEnabled(), "a greyed submenu greys its fold" );
		auto * mf = panel->findChild<QDoubleSpinBox *>( QStringLiteral( "Strength_Show" ) );
		check( mf && qFuzzyCompare( mf->value(), 1.5 ), "the value row is mirrored with its number" );
		if ( mf )
			mf->setValue( 4.0 );
		check( qFuzzyCompare( m.field->value(), 4.0 ), "the tab's number sets the menu's" );
		m.field->setValue( 2.5 );
		check( mf && qFuzzyCompare( mf->value(), 2.5 ), "the menu's number moves the tab's" );
		{
			const QSignalBlocker b( m.fog );
			m.fog->setChecked( false );
		}
		wwMenuPanelSync( panel );
		check( fog && !fog->isChecked(), "a sync reads a change made with the signals blocked" );
		auto * arrow = panel->findChild<QToolButton *>( QStringLiteral( "CellShowFold_World" ) );
		check( arrow && arrow->isChecked(), "a heading fold starts open" );
		if ( arrow )
			arrow->click();
		check( !QSettings().value( QStringLiteral( "Test/Folds/World" ), true ).toBool(), "closing a fold is saved" );
		delete panel;
	}
	{
		Menu m;
		QWidget * panel = wwMenuAsPanel( &m.menu, nullptr, style );
		auto * arrow = panel->findChild<QToolButton *>( QStringLiteral( "CellShowFold_World" ) );
		check( arrow && !arrow->isChecked(), "a new panel starts with that fold closed" );
		delete panel;
	}

	std::printf( "%d checks, %d failed\n%s\n", checks, fails, fails ? "FAIL" : "PASS" );
	return fails ? 1 : 0;
}
