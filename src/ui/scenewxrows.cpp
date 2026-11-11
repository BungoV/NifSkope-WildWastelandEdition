#include "scenewxrows.h"
#include "livepanel.h"
#include "wwswitch.h"
#include "features/weather/wxoverride.h"
#include "ui/widgets/wwnumberfield.h"
#include "wwskin.h"

#include <QAbstractSpinBox>
#include <QEvent>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QTimer>
#include <QScrollArea>

#include <algorithm>
#include <cstdio>

void wwSceneAddWeatherOverrides( const std::function<void( const QString & )> & heading,
	const std::function<void( const QString &, QWidget * )> & row, QWidget * tree )
{
	QString group;
	for ( int i = 0; i < wwWxCount(); i++ ) {
		const WwWxRow & r = wwWxRow( WwWx( i ) );
		if ( group != QLatin1String( r.group ) ) {
			group = QLatin1String( r.group );
			heading( QObject::tr( "Weather Overrides: %1" ).arg( group ) );
		}
		WwSwitchRowSpec s;
		s.sw = r.sw;
		s.type = r.n == 3 ? QStringLiteral( "color" ) : QStringLiteral( "number" );
		s.min = r.min;
		s.max = r.max;
		s.step = r.step;
		s.fallback = [i]() { return wwWxWeatherText( WwWx( i ) ); };
		QWidget * w = wwSwitchRowWidget( s, tree );
		w->setToolTip( QObject::tr( "%1 (%2). Italic: the weather's own value; type to pin it, Reset to go back." )
			.arg( QLatin1String( r.sw ), QLatin1String( r.unit ) ) );
		row( QLatin1String( r.label ), w );
	}
	auto * foot = new QWidget( tree );
	auto * lay = new QHBoxLayout( foot );
	lay->setContentsMargins( 0, 0, 0, 0 );
	auto * all = new QPushButton( QObject::tr( "Reset all" ), foot );
	all->setToolTip( QObject::tr( "Every weather override back to the weather" ) );
	auto * save = new QPushButton( QObject::tr( "Save as .recx" ), foot );
	save->setToolTip( QObject::tr( "A new .recx in Fusion Shaders - Assets/Saved Weathers (never overwrites)" ) );
	lay->addWidget( all );
	lay->addWidget( save );
	lay->addStretch( 1 );
	QObject::connect( all, &QPushButton::clicked, foot, [foot]() {
		int n = 0;
		for ( int i = 0; i < wwWxCount(); i++ ) {
			const QByteArray sw = wwWxRow( WwWx( i ) ).sw;
			if ( !wwEnvStr( sw.constData() ).trimmed().isEmpty() ) {
				wwSwitchRowSet( sw, QString(), true, foot );
				n++;
			}
		}
		std::fprintf( stderr, "weather override: reset all (%d rows back to the weather)\n", n );
	} );
	QObject::connect( save, &QPushButton::clicked, foot, [save]() {
		const QString r = wwWxSaveRecx();
		save->setToolTip( r );
	} );
	row( QObject::tr( "Weather Overrides" ), foot );
}

/* ---- MODULES1 R6-11 (bungo b61: "UI elements bunching up and getting cut off, and this menu in general does not
 * conform to our nifskope ui elements standards"): the rows are the panel standard now (skill nifskope-ww-panel-style,
 * the LOD and body panels' form): a scroll area holding ONE label | field grid, headings by wwHeading, every field
 * the width of the value column, one row height, a selector never narrower than its longest entry, a long text elided
 * with the whole of it in the tooltip, and a window narrower than the rows scrolls. The Name | Value tree is gone.
 * Red WW_SCENE_LAYOUT_RED=1: no tidy pass (each field at its own hint). ---- */
namespace {
constexpr const char * kRowsProp = "wwSceneRowCount";
constexpr const char * kFullProp = "wwSceneFullText";

QGridLayout * rowsGrid( QWidget * body )
{
	return body ? qobject_cast<QGridLayout *>( body->layout() ) : nullptr;
}

int takeRow( QWidget * body )
{
	const int r = body->property( kRowsProp ).toInt();
	body->setProperty( kRowsProp, r + 1 );
	return r;
}

void elideNow( QLabel * l )
{
	const QString full = l->property( kFullProp ).toString();
	const QString cut = l->fontMetrics().elidedText( full.simplified(), Qt::ElideRight, std::max( 0, l->width() - 2 ) );
	if ( l->text() != cut )
		l->setText( cut );
}

//! a name greys with its field; an elided text follows its width
struct RowWatch : QObject
{
	QLabel * name = nullptr;
	explicit RowWatch( QObject * p ) : QObject( p ) {}
	bool eventFilter( QObject * o, QEvent * e ) override
	{
		if ( name && e->type() == QEvent::EnabledChange )
			name->setEnabled( static_cast<QWidget *>( o )->isEnabled() );
		if ( !name && e->type() == QEvent::Resize )
			elideNow( static_cast<QLabel *>( o ) );
		return false;
	}
};

//! the rows of a body: each name label with its field
QList<QLabel *> rowNames( QWidget * body )
{
	QList<QLabel *> out;
	for ( QLabel * l : body->findChildren<QLabel *>( QStringLiteral( "sceneRowName" ) ) )
		if ( l->buddy() )
			out << l;
	return out;
}
}	// namespace

QWidget * wwSceneRowsCreate( QWidget * window )
{
	auto * area = new QScrollArea( window );
	area->setObjectName( QStringLiteral( "SceneRows" ) );
	area->setWidgetResizable( true );
	area->setFrameShape( QFrame::NoFrame );
	auto * body = new QWidget( area );
	body->setObjectName( QStringLiteral( "SceneRowsBody" ) );
	auto * g = new QGridLayout( body );	// the panels' form (bodybuildpanel.cpp, lodgenmanager.cpp)
	g->setContentsMargins( 0, 0, 6, 0 );
	g->setHorizontalSpacing( 8 );
	g->setVerticalSpacing( 4 );
	g->setColumnStretch( 1, 1 );
	body->setProperty( kRowsProp, 0 );
	area->setWidget( body );
	return body;
}

QWidget * wwSceneRowsArea( QWidget * body )
{
	for ( QWidget * p = body ? body->parentWidget() : nullptr; p; p = p->parentWidget() )
		if ( qobject_cast<QScrollArea *>( p ) )
			return p;
	return nullptr;
}

void wwSceneRowsHeading( QWidget * body, const QString & text )
{
	QGridLayout * g = rowsGrid( body );
	if ( !g )
		return;
	QLabel * h = wwHeading( text, body );
	h->setObjectName( QStringLiteral( "sceneRowHeading" ) );
	const int r = takeRow( body );
	if ( r > 0 )
		h->setContentsMargins( 0, 8, 0, 0 );	// a group stands off the rows above it
	g->addWidget( h, r, 0, 1, 2 );
}

QLabel * wwSceneRowsAdd( QWidget * body, const QString & name, QWidget * field, bool enabled )
{
	QGridLayout * g = rowsGrid( body );
	if ( !g || !field )
		return nullptr;
	auto * l = new QLabel( name, body );
	l->setObjectName( QStringLiteral( "sceneRowName" ) );
	l->setBuddy( field );
	l->setEnabled( enabled );
	field->setEnabled( enabled );
	if ( !enabled )
		field->setProperty( "wwSceneRowDead", true );
	if ( !field->toolTip().isEmpty() )
		l->setToolTip( field->toolTip() );
	auto * watch = new RowWatch( field );
	watch->name = l;
	field->installEventFilter( watch );
	const int r = takeRow( body );
	g->addWidget( l, r, 0 );
	g->addWidget( field, r, 1 );
	return l;
}

QLabel * wwSceneElideLabel( QWidget * parent )
{
	auto * l = new QLabel( parent );
	l->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Fixed );	// its text never widens or raises the row
	l->setMinimumWidth( 40 );
	l->installEventFilter( new RowWatch( l ) );
	return l;
}

void wwSceneElideSet( QLabel * l, const QString & full )
{
	if ( !l )
		return;
	l->setProperty( kFullProp, full );
	l->setToolTip( full );
	elideNow( l );
}

void wwSceneTidyRows( QWidget * body )
{
	QGridLayout * g = rowsGrid( body );
	if ( !g || wwEnvStr( "WW_SCENE_LAYOUT_RED" ).trimmed() == QLatin1String( "1" ) )
		return;
	QComboBox probe;
	probe.addItem( QStringLiteral( "Xg" ) );
	wwMatchFieldStyle( &probe );
	QDoubleSpinBox spin;
	int h = std::max( probe.sizeHint().height(), spin.sizeHint().height() );
	int combos = 0, comboW = 0, nameW = 0;
	const QList<QLabel *> names = rowNames( body );
	for ( QComboBox * c : body->findChildren<QComboBox *>() ) {
		c->setSizeAdjustPolicy( QComboBox::AdjustToContents );	// never narrower than its longest entry, as its list changes
		comboW = std::max( comboW, c->sizeHint().width() );
		combos++;
	}
	for ( QLabel * l : names ) {
		h = std::max( h, l->buddy()->sizeHint().height() );
		nameW = std::max( nameW, l->sizeHint().width() );
	}
	for ( QLabel * l : names ) {
		l->buddy()->setFixedHeight( h );	// one row height; the field fills the value column's width
		l->setFixedHeight( h );
	}
	g->setColumnMinimumWidth( 0, nameW );
	std::fprintf( stderr, "scene window: panel standard rows: %d rows, %d headings, row height %d px, name column %d px, %d selectors (widest entry %d px), fields fill the value column, the rows scroll under a narrow window (red WW_SCENE_LAYOUT_RED=1)\n",
		int( names.size() ), int( body->findChildren<QLabel *>( QStringLiteral( "sceneRowHeading" ) ).size() ), h, nameW, combos, comboW );
	// the proof: once shown, the geometry checks (and at three widths with WW_SCENE_ROWS_PROBE=1)
	struct Echo : QObject
	{
		QWidget * b;
		bool done = false;
		explicit Echo( QWidget * body ) : QObject( body ), b( body ) {}
		bool eventFilter( QObject * o, QEvent * e ) override
		{
			if ( o == b && e->type() == QEvent::Show && !done ) {
				done = true;
				QTimer::singleShot( 0, b, [body = b]() {
					wwSceneRowsEcho( body );
					if ( wwEnvStr( "WW_SCENE_ROWS_PROBE" ).trimmed() != QLatin1String( "1" ) )
						return;
					QWidget * win = body->window();
					const QSize was = win->size();
					const int widths[3] = { 400, 800, 1152 };
					for ( int i = 0; i < 3; i++ )
						QTimer::singleShot( 200 * ( i + 1 ), body, [body, win, w = widths[i], hh = was.height()]() {
							win->resize( w, hh );
							QTimer::singleShot( 100, body, [body]() { wwSceneRowsEcho( body ); } );
						} );
					QTimer::singleShot( 900, body, [win, was]() { win->resize( was ); } );
				} );
			}
			return false;
		}
	};
	body->installEventFilter( new Echo( body ) );
}

void wwSceneRowsEcho( QWidget * body )
{
	if ( !body )
		return;
	const QList<QLabel *> names = rowNames( body );
	auto * area = qobject_cast<QScrollArea *>( wwSceneRowsArea( body ) );
	const int viewW = area ? area->viewport()->width() : body->width();
	int outside = 0, comboCut = 0, nameCut = 0, overlap = 0, hMin = 1 << 20, hMax = 0, lastBottom = -1;
	QString first;
	auto bad = [&first]( int & n, const QLabel * l ) {
		n++;
		if ( first.isEmpty() )
			first = l->text();
	};
	for ( QLabel * l : names ) {
		QWidget * w = l->buddy();
		const QRect r = w->geometry(), nr = l->geometry();
		hMin = std::min( hMin, r.height() );
		hMax = std::max( hMax, r.height() );
		if ( !body->rect().contains( r ) || !body->rect().contains( nr ) )
			bad( outside, l );	// check 1: every row rect inside its parent
		if ( nr.width() < l->sizeHint().width() )
			bad( nameCut, l );	// check 2: no name wider than its column
		QList<QComboBox *> cs = w->findChildren<QComboBox *>();
		if ( auto * c = qobject_cast<QComboBox *>( w ) )
			cs << c;
		for ( QComboBox * c : cs )
			if ( c->width() < c->minimumSizeHint().width() )
				bad( comboCut, l );	// check 3: no selector text wider than its field
		if ( r.top() < lastBottom )
			bad( overlap, l );	// check 4: no row over the one above it
		lastBottom = r.bottom();
	}
	std::fprintf( stderr, "scene window rows: window %d px wide (rows view %d, rows body %d%s): %d rows; check 1 rows outside the body %d; check 2 names cut %d; check 3 selectors cut %d; check 4 rows overlapping %d; check 5 one row height %s (%d..%d px)%s%s\n",
		body->window()->width(), viewW, body->width(), body->width() > viewW ? ", scrolls sideways" : "", int( names.size() ),
		outside, nameCut, comboCut, overlap, hMin == hMax ? "yes" : "NO", hMin, hMax,
		first.isEmpty() ? "" : "; first failing row: ", qPrintable( first ) );
}

QSize wwSceneWantedSize( QWidget * body, const QWidget * window )
{
	if ( !body || wwEnvStr( "WW_SCENE_LAYOUT_RED" ).trimmed() == QLatin1String( "1" ) )
		return QSize();
	auto * area = qobject_cast<QScrollArea *>( wwSceneRowsArea( body ) );
	const int chrome = ( area ? area->verticalScrollBar()->sizeHint().width() + 2 * area->frameWidth() : 0 ) + 12 + 24;
	int wantW = body->minimumSizeHint().width() + chrome;
	int wantH = 720;
	if ( const QScreen * sc = window && window->screen() ? window->screen() : QGuiApplication::primaryScreen() ) {
		wantW = std::min( wantW, sc->availableGeometry().width() * 6 / 10 );
		wantH = std::min( wantH, sc->availableGeometry().height() * 85 / 100 );
	}
	return QSize( wantW, wantH );
}
