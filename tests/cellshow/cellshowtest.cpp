// lane OPENCELLFIX1 unit test: the Cell dock's Show tab (src/ui/cellshowpanel.cpp), offscreen.
// Prints one line per check, then PASS or FAIL; exit code 0 only on PASS.
// Red: WW_CELLSHOW_RED=menu (a tick closes the menu again) must FAIL.
// Red: WW_CELLSHOW_RED=sync (an action that changes leaves its box behind) must FAIL.
// lane OPENCELLFIX2 reds, each must FAIL: WW_CELLSHOW_RED=ticks|groups|dropdown|presets|nest|livebind, WW_DROPDOWN_RED=1.
// Round 4 reds, each must FAIL: WW_CELLSHOW_RED=arrowframe|dropright|override|builtin|presetbar, WW_DROPDOWN_RED=fit.

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
#include <QImage>
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

QStringList pickerTexts( QWidget * panel )
{
	QStringList t;
	if ( auto * d = panel->findChild<WwDropdown *>( QStringLiteral( "CellDisplayPresetPicker" ) ) )
		for ( int i = 0; i < d->count(); i++ )
			t << d->itemText( i );
	return t;
}

WwDropdown * picker( QWidget * panel )
{
	return panel->findChild<WwDropdown *>( QStringLiteral( "CellDisplayPresetPicker" ) );
}

QToolButton * barButton( QWidget * panel, const char * which )
{
	return panel->findChild<QToolButton *>( QStringLiteral( "CellDisplayPreset" ) + QLatin1String( which ) );
}

//! Picks preset `name` in the Preset dropdown as a click in its list would.
void pickPreset( QWidget * panel, const QString & name )
{
	if ( WwDropdown * d = picker( panel ) )
		d->pick( d->findText( name ) );
	QCoreApplication::processEvents();
}

//! The arrow paints the same checked and unchecked (both pointing down), hovered or not: no pressed frame.
bool arrowFlat( QToolButton * a )
{
	if ( !a )
		return false;
	a->resize( a->sizeHint() );
	const bool was = a->isChecked();
	auto img = [a]( bool checked, bool hover ) {
		const QSignalBlocker sb( a );
		a->setChecked( checked );
		a->setAttribute( Qt::WA_UnderMouse, hover );
		a->setArrowType( Qt::DownArrow );
		return a->grab().toImage();
	};
	const QImage open = img( true, false ), plain = img( false, false ), openHover = img( true, true ), hover = img( false, true );
	img( was, false );
	return !open.isNull() && open == plain && openHover == hover && open == hover;
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
	QMenu * farType, * lamp, * ssgiQ, * bloom, * aoMethod;
	QAction * ao;
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
		ao = tick( &menu, "Ao", true );	// round 4: names as long as the Ambient Occlusion methods, and longer
		aoMethod = choices( &menu, "AoMethod", { QStringLiteral( "Vanilla Screen-Space Ambient Occlusion With Its Long Name" ),
			QStringLiteral( "Horizon-Based Ambient Occlusion Plus And Some More Words" ) }, 0 );
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
		WwDisplayRow ssgiRow = r( 2, "Ssgi", "SsgiQuality", -1, true );
		ssgiRow.heavy = true;	// built-in Low turns it off
		return { c, r( 0, "Borders" ), r( 1, "Grass" ), r( 1, "FarLod", "FarLodType" ), r( 2, "", "LampShadows", 0, true ),
			ssgiRow, r( 2, "", "Bloom", 0 ), r( 2, "Ao", "AoMethod" ) };
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
	int h0 = -1, same = 0;
	for ( QWidget * r : rows ) {
		const int h = r->sizeHint().height();
		if ( h0 < 0 )
			h0 = h;
		same += h == h0;
	}
	check( same == int( rows.size() ), "every row is the same height" );

	std::printf( "round 4: a row's dropdown is the first row of its fold, on the left at the child indent; it fits\n" );
	int onRow = 0;
	for ( QWidget * r : rows )
		onRow += r->findChildren<WwDropdown *>().size();
	check( onRow == 0, "no dropdown sits on a tick row" );
	auto * lampRow = panel->findChild<QWidget *>( QStringLiteral( "CellDisplayRow_LampShadows" ) );
	auto * lampBody = panel->findChild<QWidget *>( QStringLiteral( "CellShowBody_settings_LampShadows" ) );
	auto * lampArrow = panel->findChild<QToolButton *>( QStringLiteral( "CellShowFold_settings_LampShadows" ) );
	auto * lampDropRow = panel->findChild<QWidget *>( QStringLiteral( "CellDisplayDropRow_LampShadows" ) );
	auto * bodyL = lampBody ? qobject_cast<QVBoxLayout *>( lampBody->layout() ) : nullptr;
	check( lampRow && lampArrow && lampArrow->parentWidget() == lampRow && lampDrop && lampDropRow
		&& lampDrop->parentWidget() == lampDropRow && bodyL && bodyL->count() && bodyL->itemAt( 0 )->widget() == lampDropRow,
		"a dropdown with no other settings gets a fold arrow; the dropdown is the fold's first row" );
	check( lampDropRow && lampDropRow->sizeHint().height() == h0, "the dropdown's row is a tick row's height" );
	auto * aoBody = panel->findChild<QWidget *>( QStringLiteral( "CellShowBody_settings_Ao" ) );
	auto * aoDrop = panel->findChild<WwDropdown *>( QStringLiteral( "CellDisplayChoice_Ao" ) );
	auto * aoArrow = panel->findChild<QToolButton *>( QStringLiteral( "CellShowFold_settings_Ao" ) );
	auto * fusionArrow = panel->findChild<QToolButton *>( QStringLiteral( "CellShowFold_FusionShaders" ) );
	if ( fusionArrow && !fusionArrow->isChecked() )
		fusionArrow->click();
	if ( aoArrow && !aoArrow->isChecked() )
		aoArrow->click();
	if ( lampArrow && !lampArrow->isChecked() )
		lampArrow->click();
	const int width = 320;
	panel->resize( width, 1600 );
	panel->show();
	QCoreApplication::processEvents();
	auto * aoTick = panel->findChild<QCheckBox *>( QStringLiteral( "CellDisplayTick_Ao" ) );
	const int tickX = aoTick ? aoTick->mapTo( panel, QPoint( 0, 0 ) ).x() : -1;
	const int kidX = tickX + 16 + 6;	// a child row: its arrow space, the row's spacing, then its tick
	const int dropX = aoDrop ? aoDrop->mapTo( panel, QPoint( 0, 0 ) ).x() : -2;
	check( aoBody && aoBody->isVisible() && aoDrop && aoDrop->isVisible() && dropX == kidX,
		"the dropdown starts exactly where a child row's tick starts (no label column, not pushed right)" );
	const int right = aoDrop ? aoDrop->mapTo( panel, QPoint( aoDrop->width(), 0 ) ).x() : 1 << 20;
	check( panel->width() <= width && right <= panel->width(), "a long-named dropdown fits the panel's width (Ambient Occlusion)" );
	check( aoDrop && aoDrop->shownText() != aoDrop->text() && aoDrop->shownText().endsWith( QChar( 0x2026 ) )
		&& aoDrop->itemText( 1 ) == m.aoMethod->actions().at( 1 )->text(), "the closed box elides; the list keeps the whole names" );
	panel->hide();

	std::printf( "round 4: fold arrows have no frame open, closed, hovered (heading and row)\n" );
	check( arrowFlat( fusionArrow ), "an open heading arrow paints as a folded one pointing down (Fusion Shaders)" );
	check( arrowFlat( lampArrow ), "an open row arrow paints the same (Lamp shadows)" );

	std::printf( "round 4: the Preset row: label, dropdown, Next / Save / Rename / Delete; Low..Ultra built in\n" );
	auto * bar = panel->findChild<QWidget *>( QStringLiteral( "CellDisplayPresetBar" ) );
	bool barLabel = false;
	if ( bar )
		for ( QLabel * l : bar->findChildren<QLabel *>() )
			barLabel |= l->text() == QStringLiteral( "Preset" );
	QToolButton * nextB = barButton( panel, "Next" ), * saveB = barButton( panel, "Save" );
	QToolButton * renB = barButton( panel, "Rename" ), * delB = barButton( panel, "Delete" );
	check( bar && barLabel && picker( panel ) && nextB && saveB && renB && delB && !nextB->isHidden() && !saveB->isHidden()
		&& !renB->isHidden() && !delB->isHidden(), "with no saved presets the row is all there: label, dropdown, four buttons" );
	check( wwDisplayPresetNames().isEmpty() && pickerTexts( panel ) == four, "the dropdown lists Low, Medium, High, Ultra" );
	check( picker( panel ) && picker( panel )->currentIndex() < 0 && picker( panel )->text() == QStringLiteral( "Custom" ),
		"nothing applied yet reads Custom" );
	m.grass->setChecked( false );
	item( m.lamp, QStringLiteral( "Off" ) )->trigger();
	m.ssgi->setChecked( false );
	item( m.ssgiQ, QStringLiteral( "Low" ) )->trigger();
	item( m.bloom, QStringLiteral( "Vanilla" ) )->trigger();
	pickPreset( panel, QStringLiteral( "Ultra" ) );
	check( item( m.lamp, QStringLiteral( "Ultra" ) )->isChecked() && m.ssgi->isChecked() && item( m.ssgiQ, QStringLiteral( "Ultra" ) )->isChecked()
		&& item( m.bloom, QStringLiteral( "FO4CS" ) )->isChecked(), "Ultra: every Fusion Shaders row on at its top level" );
	check( !m.grass->isChecked(), "a built-in leaves Vanilla alone" );
	check( picker( panel )->currentText() == QStringLiteral( "Ultra" ) && renB && !renB->isEnabled() && delB && !delB->isEnabled(),
		"the dropdown shows the built-in; Rename and Delete grey on a built-in" );
	pickPreset( panel, QStringLiteral( "Medium" ) );
	check( item( m.lamp, QStringLiteral( "Medium" ) )->isChecked() && m.ssgi->isChecked() && item( m.ssgiQ, QStringLiteral( "Medium" ) )->isChecked(),
		"Medium steps every level down to Medium" );
	pickPreset( panel, QStringLiteral( "Low" ) );
	check( item( m.lamp, QStringLiteral( "Low" ) )->isChecked() && !m.ssgi->isChecked() && item( m.bloom, QStringLiteral( "FO4CS" ) )->isChecked(),
		"Low: the cheap set (a heavy row off, the light ones at Low)" );
	item( m.lamp, QStringLiteral( "High" ) )->trigger();
	QCoreApplication::processEvents();
	check( picker( panel )->currentIndex() < 0 && picker( panel )->text() == QStringLiteral( "Custom" ), "a change by hand after a preset reads Custom" );
	check( !wwDisplayPresetSave( panel, QStringLiteral( "Ultra" ) ) && !wwDisplayPresetRename( panel, QStringLiteral( "Ultra" ), QStringLiteral( "Mine" ) )
		&& !wwDisplayPresetDelete( panel, QStringLiteral( "Low" ) ), "a built-in is never overwritten, renamed or deleted" );

	m.grass->setChecked( true );
	m.ssgi->setChecked( false );
	m.strength->setValue( 1.0 );
	answer( QStringLiteral( "Quiet" ) );
	if ( saveB )
		saveB->click();
	check( wwDisplayPresetNames() == QStringList( QStringLiteral( "Quiet" ) ) && pickerTexts( panel ) == four + QStringList( QStringLiteral( "Quiet" ) )
		&& picker( panel )->currentText() == QStringLiteral( "Quiet" ), "Save keeps the Display as a named preset, listed after the built-ins" );
	check( renB && renB->isEnabled() && delB && delB->isEnabled(), "Rename and Delete work on his own preset" );
	const QVariantMap quiet = wwDisplayState( panel );
	m.grass->setChecked( false );
	m.ssgi->setChecked( true );
	item( m.ssgiQ, QStringLiteral( "Ultra" ) )->trigger();
	m.strength->setValue( 3.0 );
	m.color->setCurrentIndex( 2 );
	answer( QStringLiteral( "Loud" ) );
	if ( saveB )
		saveB->click();
	pickPreset( panel, QStringLiteral( "Quiet" ) );
	check( m.grass->isChecked() && !m.ssgi->isChecked() && qFuzzyCompare( m.strength->value(), 1.0 ) && m.color->currentIndex() == 1
		&& wwDisplayState( panel ) == quiet && wwDisplayPresetCurrent() == QStringLiteral( "Quiet" ),
		"picking a preset applies all of it, live (round trip)" );
	if ( nextB )
		nextB->click();
	check( wwDisplayPresetCurrent() == QStringLiteral( "Low" ), "Next goes round the built-ins and his own" );
	pickPreset( panel, QStringLiteral( "Loud" ) );
	answer( QStringLiteral( "Party" ) );
	if ( renB )
		renB->click();
	check( wwDisplayPresetNames() == QStringList( { QStringLiteral( "Party" ), QStringLiteral( "Quiet" ) } )
		&& picker( panel )->currentText() == QStringLiteral( "Party" ), "Rename renames the current preset (and keeps it current)" );
	check( !wwDisplayPresetRename( panel, QStringLiteral( "Party" ), QStringLiteral( "Quiet" ) ), "a rename onto a used name is refused" );
	pickPreset( panel, QStringLiteral( "Quiet" ) );
	if ( delB )
		delB->click();
	check( wwDisplayPresetNames() == QStringList( QStringLiteral( "Party" ) ) && pickerTexts( panel ) == four + QStringList( QStringLiteral( "Party" ) ),
		"Delete removes the current preset" );
	pickPreset( panel, QStringLiteral( "Party" ) );
	delete panel;

	DisplayMenu m2;	// a fresh menu and panel: what was saved comes back
	QWidget * fresh = wwMenuAsDisplay( &m2.menu, m2.rows(), nullptr, style );
	check( pickerTexts( fresh ) == four + QStringList( QStringLiteral( "Party" ) ), "the presets persist for a fresh panel" );
	pickPreset( fresh, QStringLiteral( "Party" ) );
	check( m2.grass->isChecked() == false && m2.ssgi->isChecked() && item( m2.ssgiQ, QStringLiteral( "Ultra" ) )->isChecked()
		&& qFuzzyCompare( m2.strength->value(), 3.0 ) && m2.color->currentIndex() == 2, "a fresh panel applies a saved preset" );
	check( !wwDisplayPresetSave( fresh, QStringLiteral( "a/b" ) ) && !wwDisplayPresetSave( fresh, QStringLiteral( " " ) ),
		"a name with a slash or no name is refused" );
	delete fresh;
}

//! round 4: "if a setting in fusion shaders overrides something in vanilla, grey it out in vanilla".
void overrideChecks( const WwMenuPanelStyle & style )
{
	std::printf( "round 4: a Vanilla row greys while the Fusion Shaders row replacing it is on (live, value kept)\n" );
	QMenu menu;
	QAction * taa = tick( &menu, "Taa", true );
	QMenu * modern = choices( &menu, "ModernTaa", { QStringLiteral( "Off" ), QStringLiteral( "Low" ), QStringLiteral( "Ultra" ) }, 0 );
	tick( &menu, "Grass", true );
	const QStringList g = wwDisplayGroups();
	WwDisplayRow v;
	v.group = g.at( 1 );
	v.tick = QStringLiteral( "Taa" );
	v.overriddenBy = QStringLiteral( "ModernTaa" );
	WwDisplayRow grass;
	grass.group = g.at( 1 );
	grass.tick = QStringLiteral( "Grass" );
	WwDisplayRow f;
	f.group = g.at( 2 );
	f.choices = QStringLiteral( "ModernTaa" );
	f.off = 0;
	f.quality = true;
	QWidget * panel = wwMenuAsDisplay( &menu, { v, grass, f }, nullptr, style );
	auto * row = panel->findChild<QWidget *>( QStringLiteral( "CellDisplayRow_Taa" ) );
	auto * other = panel->findChild<QWidget *>( QStringLiteral( "CellDisplayRow_Grass" ) );
	check( row && row->isEnabled(), "the Fusion row off: the Vanilla row is live" );
	item( modern, QStringLiteral( "Low" ) )->trigger();
	check( row && !row->isEnabled() && taa->isChecked(), "the Fusion row on: the Vanilla row greys, its tick keeps its value" );
	check( other && other->isEnabled(), "a Vanilla row nothing replaces stays live" );
	item( modern, QStringLiteral( "Off" ) )->trigger();
	check( row && row->isEnabled() && taa->isChecked(), "the Fusion row off again: the Vanilla row comes back as it was" );
	delete panel;
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
	overrideChecks( style );
	liveBindChecks();

	std::printf( "%d checks, %d failed\n%s\n", checks, fails, fails ? "FAIL" : "PASS" );
	return fails ? 1 : 0;
}
