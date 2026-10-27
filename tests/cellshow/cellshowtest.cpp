// lane OPENCELLFIX1 unit test: the Cell dock's Show tab (src/ui/cellshowpanel.cpp), offscreen.
// Prints one line per check, then PASS or FAIL; exit code 0 only on PASS.
// Red: WW_CELLSHOW_RED=menu (a tick closes the menu again) must FAIL.
// Red: WW_CELLSHOW_RED=sync (an action that changes leaves its box behind) must FAIL.
// lane OPENCELLFIX2 reds, each must FAIL: WW_CELLSHOW_RED=ticks|groups|dropdown|presets|nest|livebind, WW_DROPDOWN_RED=1.

#include "cellshowpanel.h"
#include "widgets/wwdropdown.h"

#include <QAbstractButton>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QRadioButton>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
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

//! Answers the next name prompt (QInputDialog) with `text`, as a user typing it and pressing OK.
void answer( const QString & text )
{
	QTimer::singleShot( 0, [text]() {
		for ( QWidget * w : QApplication::topLevelWidgets() )
			if ( auto * d = qobject_cast<QInputDialog *>( w ); d && d->isVisible() ) {
				d->setTextValue( text );
				d->accept();
				return;
			}
	} );
}

QList<QToolButton *> presetButtons( QWidget * panel )
{
	return panel->findChildren<QToolButton *>( QStringLiteral( "CellDisplayPreset" ) );
}

QToolButton * presetButton( QWidget * panel, const QString & name )
{
	for ( QToolButton * b : presetButtons( panel ) )
		if ( b->property( "wwPresetName" ).toString() == name )
			return b;
	return nullptr;
}

//! A right-click action of a preset button (Save, Rename, Delete).
QAction * presetAction( QToolButton * b, const char * which )
{
	QMenu * m = b ? b->findChild<QMenu *>( QStringLiteral( "CellDisplayPresetMenu" ) ) : nullptr;
	return m ? m->findChild<QAction *>( QStringLiteral( "CellDisplayPresetCtx" ) + QLatin1String( which ) ) : nullptr;
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

// ---- lane OPENCELLFIX2: the Display tab

QMenu * choices( QMenu * m, const char * name, const QStringList & items, int checked )
{
	QMenu * s = m->addMenu( QString::fromLatin1( name ) );
	s->setObjectName( QString::fromLatin1( name ) );
	auto * g = new QActionGroup( s );
	g->setExclusive( true );
	for ( int i = 0; i < int( items.size() ); i++ ) {
		QAction * a = s->addAction( items.at( i ) );
		a->setCheckable( true );
		a->setChecked( i == checked );
		g->addAction( a );
	}
	return s;
}

QAction * item( QMenu * m, const QString & text )
{
	for ( QAction * a : m->actions() )
		if ( a->text() == text )
			return a;
	return nullptr;
}

//! The Cell dock's Show menu in miniature, for the Display tab: plain ticks, a tick with choices, a quality menu
//! with Off, a menu with a value row and a tick inside, a heading, one tick no row names, and a Color dropdown.
struct DisplayMenu
{
	QMenu menu;
	QAction * borders, * grass, * farLod, * ssgi, * stray;
	QMenu * farType, * lamp, * ssgiQ, * bloom;
	QAction * bloomGlare = nullptr;
	QDoubleSpinBox * strength = nullptr;
	WwDropdown * color;

	DisplayMenu()
	{
		heading( &menu, "World", "world" );
		borders = tick( &menu, "Borders", false );
		grass = tick( &menu, "Grass", true );
		farLod = tick( &menu, "FarLod", true );
		farType = choices( &menu, "FarLodType", { QStringLiteral( "Vanilla" ), QStringLiteral( "FO4CS" ) }, 0 );
		lamp = choices( &menu, "LampShadows", { QStringLiteral( "Off" ), QStringLiteral( "Low" ), QStringLiteral( "Medium" ),
			QStringLiteral( "High" ), QStringLiteral( "Ultra" ) }, 0 );
		ssgi = tick( &menu, "Ssgi", false );
		ssgiQ = choices( &menu, "SsgiQuality", { QStringLiteral( "Low" ), QStringLiteral( "Medium" ), QStringLiteral( "High" ),
			QStringLiteral( "Ultra" ) }, 2 );
		bloom = choices( &menu, "Bloom", { QStringLiteral( "Vanilla" ), QStringLiteral( "FO4CS" ) }, 0 );
		bloomGlare = tick( bloom, "BloomGlare", false );
		auto * host = new QWidget;
		auto * gl = new QGridLayout( host );
		gl->addWidget( new QLabel( QStringLiteral( "Strength" ) ), 0, 0 );
		strength = new QDoubleSpinBox;
		strength->setObjectName( QStringLiteral( "BloomStrength" ) );
		strength->setRange( 0, 10 );
		strength->setValue( 1.0 );
		gl->addWidget( strength, 0, 1 );
		auto * wa = new QWidgetAction( bloom );
		wa->setDefaultWidget( host );
		bloom->addAction( wa );
		stray = tick( &menu, "Stray", false );
		color = new WwDropdown;
		color->setObjectName( QStringLiteral( "Color" ) );
		color->addItems( { QStringLiteral( "Textured" ), QStringLiteral( "Lit" ), QStringLiteral( "Normals" ) } );
	}

	QList<WwDisplayRow> rows()
	{
		const QStringList g = wwDisplayGroups();
		auto r = [g]( int group, const char * t, const char * c = "", int off = -1, bool q = false ) {
			WwDisplayRow x;
			x.group = g.at( group );
			x.tick = QLatin1String( t );
			x.choices = QLatin1String( c );
			x.off = off;
			x.quality = q;
			return x;
		};
		WwDisplayRow c;
		c.group = g.at( 0 );
		c.dropdown = color;	// moves into the panel's row (the panel owns it then)
		c.off = 0;
		c.label = QStringLiteral( "Color" );
		return { c, r( 0, "Borders" ), r( 1, "Grass" ), r( 1, "FarLod", "FarLodType" ), r( 2, "", "LampShadows", 0, true ),
			r( 2, "Ssgi", "SsgiQuality", -1, true ), r( 2, "", "Bloom", 0 ) };
	}
};

void displayChecks( const WwMenuPanelStyle & style )
{
	std::printf( "the Display tab: a tick per row, quality dropdowns, three groups\n" );
	QSettings().remove( QStringLiteral( "CellEditor/DisplayPresets" ) );	// before the panel: its preset strip reads them
	QSettings().remove( QStringLiteral( "CellEditor/DisplayPreset" ) );
	DisplayMenu m;
	QWidget * panel = wwMenuAsDisplay( &m.menu, m.rows(), nullptr, style );
	QList<QWidget *> rows;
	for ( QWidget * w : panel->findChildren<QWidget *>() )
		if ( w->property( "wwDisplayRow" ).toBool() )
			rows.append( w );
	check( rows.size() >= 8, "a row per feature (floor 8: 7 named + the stray)" );
	int ticked = 0;
	for ( QWidget * r : rows )
		ticked += r->findChildren<QCheckBox *>( QString(), Qt::FindDirectChildrenOnly ).size() == 1;
	check( !rows.isEmpty() && ticked == int( rows.size() ), "every row has its one tick" );
	check( wwDisplayLeftovers( panel ) == 1, "the tick no row names gets a row and is counted" );
	QStringList bodies;
	for ( const QString & gname : wwDisplayGroups() )
		if ( panel->findChild<QWidget *>( QStringLiteral( "CellShowBody_" ) + QString( gname ).remove( QLatin1Char( ' ' ) ) ) )
			bodies << gname;
	check( bodies == QStringList( { QStringLiteral( "Editor" ), QStringLiteral( "Vanilla" ), QStringLiteral( "Fusion Shaders" ) } ),
		"the groups are Editor, Vanilla, Fusion Shaders" );
	auto * vanilla = panel->findChild<QWidget *>( QStringLiteral( "CellShowBody_Vanilla" ) );
	check( vanilla && vanilla->findChild<QWidget *>( QStringLiteral( "CellDisplayRow_Grass" ) ),
		"a row sits in its own group (Grass under Vanilla)" );
	auto * fusion = panel->findChild<QWidget *>( QStringLiteral( "CellShowBody_FusionShaders" ) );
	check( fusion && fusion->findChild<QWidget *>( QStringLiteral( "CellDisplayRow_Stray" ) ), "the unnamed tick lands in Fusion Shaders" );

	const QStringList four = { QStringLiteral( "Low" ), QStringLiteral( "Medium" ), QStringLiteral( "High" ), QStringLiteral( "Ultra" ) };
	int quality = 0, qualityOk = 0;
	for ( WwDropdown * d : panel->findChildren<WwDropdown *>() ) {
		if ( !d->property( "wwQuality" ).toBool() )
			continue;
		quality++;
		QStringList texts;
		for ( int i = 0; i < d->count(); i++ )
			texts << d->itemText( i );
		qualityOk += texts == four;
	}
	check( quality == 2 && qualityOk == 2, "each quality row's dropdown is Low / Medium / High / Ultra" );
	int drops = 0, styled = 0;
	for ( WwDropdown * d : panel->findChildren<WwDropdown *>() ) {
		drops++;
		styled += WwDropdown::hasStyle( d );
	}
	check( drops >= 4 && styled == drops, "every dropdown has the Object Mode selector's look" );

	auto * lampTick = panel->findChild<QCheckBox *>( QStringLiteral( "CellDisplayTick_LampShadows" ) );
	auto * lampDrop = panel->findChild<WwDropdown *>( QStringLiteral( "CellDisplayChoice_LampShadows" ) );
	check( lampTick && !lampTick->isChecked() && lampDrop && !lampDrop->isEnabled(), "a quality row starts off (Off ticked)" );
	if ( lampTick )
		lampTick->click();
	check( item( m.lamp, QStringLiteral( "Medium" ) )->isChecked(), "ticking it the first time picks Medium" );
	if ( lampDrop )
		lampDrop->pick( 3 );
	check( item( m.lamp, QStringLiteral( "Ultra" ) )->isChecked(), "the dropdown picks the level in the menu" );
	if ( lampTick )
		lampTick->click();
	check( item( m.lamp, QStringLiteral( "Off" ) )->isChecked(), "unticking picks Off" );
	if ( lampTick )
		lampTick->click();
	check( item( m.lamp, QStringLiteral( "Ultra" ) )->isChecked(), "ticking again brings the last level back" );
	auto * farTick = panel->findChild<QCheckBox *>( QStringLiteral( "CellDisplayTick_FarLod" ) );
	if ( farTick )
		farTick->click();
	check( !m.farLod->isChecked(), "a tick drives its action" );
	m.farLod->setChecked( true );
	check( farTick && farTick->isChecked(), "the action moves its tick back" );
	auto * colorTick = panel->findChild<QCheckBox *>( QStringLiteral( "CellDisplayTick_Color" ) );
	if ( colorTick )
		colorTick->click();
	check( m.color->currentIndex() == 1, "the Color tick turns a color view on" );
	check( panel->findChild<QToolButton *>( QStringLiteral( "CellShowFold_settings_Bloom" ) ) != nullptr
		&& panel->findChild<QDoubleSpinBox *>( QStringLiteral( "BloomStrength_Show" ) ),
		"the finer settings fold under their row (Bloom's value row)" );

	std::printf( "Display rows: the fold arrow on the tick row, one height for every row (notes 10, 11)\n" );
	auto * bloomRow = panel->findChild<QWidget *>( QStringLiteral( "CellDisplayRow_Bloom" ) );
	auto * bloomArrow = panel->findChild<QToolButton *>( QStringLiteral( "CellShowFold_settings_Bloom" ) );
	check( bloomRow && bloomArrow && bloomArrow->parentWidget() == bloomRow, "the fold arrow sits on the tick row (Bloom)" );
	bool settingsRow = false;
	for ( QLabel * l : panel->findChildren<QLabel *>() )
		settingsRow |= l->text().endsWith( QLatin1String( " settings" ) );
	check( !settingsRow, "no separate \"... settings\" row" );
	auto * bloomBody = panel->findChild<QWidget *>( QStringLiteral( "CellShowBody_settings_Bloom" ) );
	const bool hidden0 = bloomBody && bloomBody->isHidden();
	if ( bloomArrow )
		bloomArrow->click();
	check( bloomBody && bloomBody->isHidden() != hidden0 && bloomBody->findChild<QDoubleSpinBox *>( QStringLiteral( "BloomStrength_Show" ) ),
		"the arrow unfolds the settings below the row" );
	int lined = 0;
	for ( QWidget * r : rows ) {
		auto * hl = qobject_cast<QHBoxLayout *>( r->layout() );
		auto * first = hl && hl->count() ? qobject_cast<QToolButton *>( hl->itemAt( 0 )->widget() ) : nullptr;
		lined += first && first->minimumWidth() == first->maximumWidth() && first->maximumWidth() > 0
			&& ( !first->isHidden() || first->sizePolicy().retainSizeWhenHidden() );
	}
	check( !rows.isEmpty() && lined == int( rows.size() ), "every row starts with the arrow or its empty space, so the ticks line up" );
	auto * grassSpace = panel->findChild<QWidget *>( QStringLiteral( "CellDisplayRow_Grass" ) );
	grassSpace = grassSpace ? grassSpace->findChild<QToolButton *>( QStringLiteral( "CellDisplayArrowSpace" ) ) : nullptr;
	check( grassSpace && grassSpace->isHidden(), "a row with nothing to fold shows no arrow (Grass)" );
	int h0 = -1, same = 0, withDrop = 0;
	for ( QWidget * r : rows ) {
		const int h = r->sizeHint().height();
		if ( h0 < 0 )
			h0 = h;
		same += h == h0;
		withDrop += r->findChild<WwDropdown *>() != nullptr;
	}
	check( withDrop >= 3 && same == int( rows.size() ), "every row is the same height, a dropdown row too" );
	auto * lampRow = panel->findChild<QWidget *>( QStringLiteral( "CellDisplayRow_LampShadows" ) );
	check( lampRow && lampDrop && lampDrop->parentWidget() == lampRow && lampTick
		&& lampDrop->sizeHint().height() == lampTick->sizeHint().height(), "the quality dropdown is inline, as tall as its tick" );

	std::printf( "Display presets: a button each, a click applies, right-click Save / Rename / Delete, a fresh panel\n" );
	auto * bar = panel->findChild<QWidget *>( QStringLiteral( "CellDisplayPresetBar" ) );
	auto * add = panel->findChild<QToolButton *>( QStringLiteral( "CellDisplayPresetAdd" ) );
	bool barText = false;
	if ( bar )
		for ( QLabel * l : bar->findChildren<QLabel *>() )
			barText |= !l->text().isEmpty();
	check( bar && add && presetButtons( panel ).isEmpty() && !barText && !panel->findChild<WwDropdown *>( QStringLiteral( "CellDisplayPresetPicker" ) )
		&& bar->findChildren<QWidget *>( QString(), Qt::FindDirectChildrenOnly ).size() == 1,
		"no presets: the strip is the + button alone (no label, no picker)" );
	m.grass->setChecked( true );
	m.ssgi->setChecked( false );
	m.strength->setValue( 1.0 );
	answer( QStringLiteral( "Quiet" ) );
	if ( add )
		add->click();
	check( wwDisplayPresetNames() == QStringList( QStringLiteral( "Quiet" ) ) && presetButtons( panel ).size() == 1,
		"+ saves the current Display as a named preset" );
	const QVariantMap quiet = wwDisplayState( panel );
	m.grass->setChecked( false );
	m.ssgi->setChecked( true );
	item( m.ssgiQ, QStringLiteral( "Ultra" ) )->trigger();
	m.strength->setValue( 3.0 );
	m.color->setCurrentIndex( 2 );
	answer( QStringLiteral( "Loud" ) );
	if ( add )
		add->click();
	check( wwDisplayPresetNames().size() == 2 && presetButtons( panel ).size() == wwDisplayPresetNames().size(),
		"every preset is a button, nothing to open (buttons == presets)" );
	QToolButton * quietB = presetButton( panel, QStringLiteral( "Quiet" ) );
	QToolButton * loudB = presetButton( panel, QStringLiteral( "Loud" ) );
	check( loudB && loudB->isChecked() && quietB && !quietB->isChecked(), "the active preset is highlighted" );
	check( bar && bar->layout() && bar->layout()->itemAt( bar->layout()->count() - 1 )->widget() == add, "+ ends the strip" );
	if ( quietB )
		quietB->click();
	check( m.grass->isChecked() && !m.ssgi->isChecked() && qFuzzyCompare( m.strength->value(), 1.0 ) && m.color->currentIndex() == 1
		&& wwDisplayState( panel ) == quiet && wwDisplayPresetCurrent() == QStringLiteral( "Quiet" ),
		"one click applies the whole preset, live (round trip)" );
	check( quietB && quietB->isChecked() && loudB && !loudB->isChecked(), "the highlight moves to the clicked preset" );
	if ( quietB )
		quietB->click();
	check( quietB && quietB->isChecked(), "a click on the active one keeps it highlighted" );
	check( loudB && loudB->contextMenuPolicy() == Qt::CustomContextMenu && presetAction( loudB, "Save" )
		&& presetAction( loudB, "Rename" ) && presetAction( loudB, "Delete" ), "right-click gives Save, Rename, Delete" );
	m.ssgi->setChecked( true );
	item( m.ssgiQ, QStringLiteral( "Ultra" ) )->trigger();
	m.strength->setValue( 3.0 );
	m.color->setCurrentIndex( 2 );
	m.grass->setChecked( true );	// Loud was saved with Grass off
	if ( QAction * a = presetAction( loudB, "Save" ) )
		a->trigger();
	if ( quietB )
		quietB->click();
	m.grass->setChecked( false );
	if ( loudB )
		loudB->click();
	check( m.grass->isChecked() && m.ssgi->isChecked() && wwDisplayPresetCurrent() == QStringLiteral( "Loud" ),
		"right-click Save overwrites the preset with the current Display" );
	answer( QStringLiteral( "Party" ) );
	if ( QAction * a = presetAction( loudB, "Rename" ) )
		a->trigger();
	QToolButton * partyB = presetButton( panel, QStringLiteral( "Party" ) );
	check( wwDisplayPresetNames() == QStringList( { QStringLiteral( "Party" ), QStringLiteral( "Quiet" ) } ) && partyB && partyB->isChecked()
		&& !presetButton( panel, QStringLiteral( "Loud" ) ) && presetButtons( panel ).size() == 2,
		"right-click Rename renames the button (and keeps it active)" );
	check( !wwDisplayPresetRename( panel, QStringLiteral( "Party" ), QStringLiteral( "Quiet" ) ), "a rename onto a used name is refused" );
	if ( QAction * a = presetAction( presetButton( panel, QStringLiteral( "Quiet" ) ), "Delete" ) )
		a->trigger();
	check( wwDisplayPresetNames() == QStringList( QStringLiteral( "Party" ) ) && presetButtons( panel ).size() == 1,
		"right-click Delete removes the preset and its button" );
	QCoreApplication::processEvents();	// the deleted buttons go
	check( wwDisplayPresetSave( panel, QStringLiteral( "Quiet" ) ) && presetButtons( panel ).size() == 2, "a save through the API adds its button" );

	QStringList many;
	for ( char c = 'A'; c <= 'H'; c++ )
		many << QStringLiteral( "Preset " ) + QLatin1Char( c );
	for ( const QString & n : many )
		wwDisplayPresetSave( panel, n );
	int lines = 0, inside = 0, firstOk = 0;
	const int width = 160;
	if ( bar && bar->layout() ) {
		bar->layout()->setGeometry( QRect( 0, 0, width, bar->layout()->heightForWidth( width ) ) );
		QSet<int> ys;
		QList<QToolButton *> all = presetButtons( panel );
		all << add;
		for ( QToolButton * b : all ) {
			ys.insert( b->geometry().y() );
			inside += b->geometry().right() < width;
			firstOk += ( b->geometry().x() == 0 ) == b->property( "wwSegmentFirst" ).toBool();
		}
		lines = int( ys.size() );
		check( presetButtons( panel ).size() == 10 && lines > 1 && inside == int( all.size() ) && firstOk == int( all.size() ),
			"many presets wrap onto more lines, each line its own strip" );
	} else {
		check( false, "many presets wrap onto more lines, each line its own strip" );
	}
	for ( const QString & n : many )
		wwDisplayPresetDelete( panel, n );
	wwDisplayPresetApply( panel, QStringLiteral( "Party" ) );
	delete panel;

	DisplayMenu m2;	// a fresh menu and panel: what was saved comes back
	QWidget * fresh = wwMenuAsDisplay( &m2.menu, m2.rows(), nullptr, style );
	check( wwDisplayPresetNames().size() == 2 && presetButtons( fresh ).size() == 2, "the presets persist for a fresh panel, a button each" );
	QToolButton * freshParty = presetButton( fresh, QStringLiteral( "Party" ) );
	check( freshParty && freshParty->isChecked(), "a fresh panel highlights the active preset" );
	if ( QToolButton * q = presetButton( fresh, QStringLiteral( "Quiet" ) ) )
		q->click();
	if ( freshParty )
		freshParty->click();
	check( m2.grass->isChecked() && m2.ssgi->isChecked() && item( m2.ssgiQ, QStringLiteral( "Ultra" ) )->isChecked()
		&& qFuzzyCompare( m2.strength->value(), 3.0 ) && m2.color->currentIndex() == 2, "a click on a fresh panel applies a saved preset" );
	check( !wwDisplayPresetSave( fresh, QStringLiteral( "a/b" ) ) && !wwDisplayPresetSave( fresh, QStringLiteral( " " ) ),
		"a name with a slash or no name is refused" );
	delete fresh;
}

//! notes 18-26: a group is one row (fold arrow + tick + name), its rows indented in its fold; a master greys them.
void groupChecks( const WwMenuPanelStyle & style )
{
	std::printf( "Display groups: one row per group, its rows in its fold, a master greys them (notes 18-26)\n" );
	QMenu menu;
	QAction * master = tick( &menu, "Group_Water", true );
	QAction * caustics = tick( &menu, "Caustics", true );
	tick( &menu, "Shared", true );
	tick( &menu, "Head", true );
	tick( &menu, "HeadKid", false );
	tick( &menu, "Loose", false );
	const QStringList g = wwDisplayGroups();
	auto r = [&]( const char * t, const char * parent, bool isMaster = false, bool shared = false ) {
		WwDisplayRow x;
		x.group = parent[0] ? QString() : g.at( 2 );
		x.tick = QLatin1String( t );
		x.parent = QLatin1String( parent );
		x.master = isMaster;
		x.shared = shared;
		return x;
	};
	const QList<WwDisplayRow> rows = { r( "Group_Water", "", true ), r( "Caustics", "Group_Water" ),
		r( "Shared", "Group_Water", false, true ), r( "Head", "" ), r( "HeadKid", "Head" ), r( "Loose", "" ) };
	QWidget * panel = wwMenuAsDisplay( &menu, rows, nullptr, style );
	auto * body = panel->findChild<QWidget *>( QStringLiteral( "CellShowBody_settings_Group_Water" ) );
	auto * kid = panel->findChild<QWidget *>( QStringLiteral( "CellDisplayRow_Caustics" ) );
	auto * shared = panel->findChild<QWidget *>( QStringLiteral( "CellDisplayRow_Shared" ) );
	auto * headBody = panel->findChild<QWidget *>( QStringLiteral( "CellShowBody_settings_Head" ) );
	auto * headKid = panel->findChild<QWidget *>( QStringLiteral( "CellDisplayRow_HeadKid" ) );
	auto * arrow = panel->findChild<QToolButton *>( QStringLiteral( "CellShowFold_settings_Group_Water" ) );
	check( arrow && arrow->parentWidget() == panel->findChild<QWidget *>( QStringLiteral( "CellDisplayRow_Group_Water" ) ),
		"the group row has its fold arrow, its tick and its name" );
	check( body && kid && body->isAncestorOf( kid ) && headBody && headKid && headBody->isAncestorOf( headKid ),
		"a group's rows sit in its fold, under a master and under a feature's own tick alike" );
	check( body && body->isHidden() && arrow && !arrow->isChecked(), "a group starts folded" );
	auto * loose = panel->findChild<QWidget *>( QStringLiteral( "CellDisplayRow_Loose" ) );
	check( loose && loose->findChild<QToolButton *>( QStringLiteral( "CellDisplayArrowSpace" ) ), "a plain row has no arrow" );
	master->setChecked( false );
	check( kid && !kid->isEnabled() && caustics->isChecked(), "a master off greys its rows; their own ticks stay" );
	check( shared && shared->isEnabled(), "a row the master does not gate stays live" );
	master->setChecked( true );
	check( kid && kid->isEnabled(), "the master on brings its rows back" );
	delete panel;
}

//! notes 22, 26: the Scene window's rows and the Display tab's are one setting; each side follows the other live.
void liveBindChecks()
{
	std::printf( "live bind: the Display tab and the Scene window follow each other (notes 22, 26)\n" );
	static bool state = false;
	QAction a;
	a.setCheckable( true );
	QCheckBox scene;
	QObject::connect( &a, &QAction::toggled, []( bool on ) {
		state = on;
		wwLiveChanged( QStringLiteral( "fog" ) );
	} );
	QObject::connect( &scene, &QCheckBox::toggled, []( bool on ) {
		state = on;
		wwLiveChanged( QStringLiteral( "fog" ) );
	} );
	wwLiveWatch( &a, QStringLiteral( "fog" ), [&a]() { a.setChecked( state ); } );
	wwLiveWatch( &scene, QStringLiteral( "fog" ), [&scene]() {
		const QSignalBlocker b( scene );
		scene.setChecked( state );
	} );
	scene.setChecked( true );
	check( state && a.isChecked(), "the Scene window's tick moves the Display row's" );
	a.setChecked( false );
	check( !state && !scene.isChecked(), "the Display row moves the Scene window's tick" );
	{
		QCheckBox gone;
		wwLiveWatch( &gone, QStringLiteral( "fog" ), [&gone]() { gone.setChecked( state ); } );
	}
	scene.setChecked( true );
	check( a.isChecked(), "a closed window drops out of the bind" );
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
	// lane OPENCELLFIX2: the test's own settings scope, an ini file beside the exe (never the app's key)
	QSettings::setDefaultFormat( QSettings::IniFormat );
	QSettings::setPath( QSettings::IniFormat, QSettings::UserScope, QCoreApplication::applicationDirPath() + QStringLiteral( "/settings" ) );
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

	displayChecks( style );
	groupChecks( style );
	liveBindChecks();

	std::printf( "%d checks, %d failed\n%s\n", checks, fails, fails ? "FAIL" : "PASS" );
	return fails ? 1 : 0;
}
