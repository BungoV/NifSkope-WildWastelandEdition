#include "scenewxrows.h"
#include "livepanel.h"
#include "wwswitch.h"
#include "features/weather/wxoverride.h"
#include "ui/widgets/wwnumberfield.h"

#include <QAbstractSpinBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QTimer>
#include <QTreeWidget>

#include <algorithm>
#include <cstdio>

void wwSceneAddWeatherOverrides( const std::function<void( const QString & )> & heading,
	const std::function<void( const QString &, QWidget * )> & row, QTreeWidget * tree )
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

void wwSceneTidyRows( QTreeWidget * tree )
{
	if ( !tree || wwEnvStr( "WW_SCENE_LAYOUT_RED" ).trimmed() == QLatin1String( "1" ) )
		return;
	QComboBox probe;
	probe.addItem( QStringLiteral( "Xg" ) );
	wwMatchFieldStyle( &probe );
	QDoubleSpinBox spin;
	int h = std::max( probe.sizeHint().height(), spin.sizeHint().height() );
	for ( int i = 0; i < tree->topLevelItemCount(); i++ ) {
		QTreeWidgetItem * it = tree->topLevelItem( i );
		if ( QWidget * w = tree->itemWidget( it, 1 ) ) {
			for ( QComboBox * c : w->findChildren<QComboBox *>() )
				c->setSizeAdjustPolicy( QComboBox::AdjustToContents );
			if ( auto * c = qobject_cast<QComboBox *>( w ) )
				c->setSizeAdjustPolicy( QComboBox::AdjustToContents );
			h = std::max( h, w->sizeHint().height() );
		}
	}
	h += 2;
	/* MODULES1 round 4 F3 (bungo b60 11-10, values half a row below their names): QSize( -1, h ) is an INVALID size,
	 * which the view ignores, so the rows stayed at the text's 16 px and every taller field hung below its row (offline
	 * probe notes/modules1/probe_f3: rows 16 -> 27, value y = row y). The hint goes on the value column with a valid
	 * width, and every selector takes one width (the widest one's); number fields keep the whole column. Red
	 * WW_SCENE_ROWS_RED=old: the invalid hint, each combo its own width. */
	const bool oldRows = wwEnvStr( "WW_SCENE_ROWS_RED" ).trimmed() == QLatin1String( "old" );
	int comboW = 0, combos = 0;
	if ( !oldRows ) {
		const QList<QComboBox *> all = tree->findChildren<QComboBox *>();
		for ( QComboBox * c : all )
			comboW = std::max( comboW, c->sizeHint().width() );
		for ( QComboBox * c : all ) {
			c->setFixedWidth( comboW );
			combos++;
		}
	}
	for ( int i = 0; i < tree->topLevelItemCount(); i++ ) {
		if ( oldRows )
			tree->topLevelItem( i )->setSizeHint( 0, QSize( -1, h ) );
		else
			tree->topLevelItem( i )->setSizeHint( 1, QSize( 1, h ) );
	}
	tree->doItemsLayout();
	std::fprintf( stderr, "scene window: rows %d px high (%d rows) combos %d at %d px%s\n", h, tree->topLevelItemCount(),
		combos, comboW, oldRows ? " (red old)" : "" );
	// the proof: once shown, each row's value top against its name row's top (must be equal), and the row heights
	struct Echo : QObject {
		QTreeWidget * t;
		explicit Echo( QTreeWidget * tr ) : QObject( tr ), t( tr ) {}
		bool eventFilter( QObject * o, QEvent * e ) override
		{
			if ( o == t && e->type() == QEvent::Show )
				QTimer::singleShot( 0, t, [tr = t]() { wwSceneRowsEcho( tr ); } );
			return false;
		}
	};
	tree->installEventFilter( new Echo( tree ) );
}

void wwSceneRowsEcho( QTreeWidget * tree )
{
	if ( !tree )
		return;
	int rows = 0, off = 0, worst = 0, hMin = 1 << 20, hMax = 0;
	QString first;
	for ( int i = 0; i < tree->topLevelItemCount(); i++ ) {
		QTreeWidgetItem * it = tree->topLevelItem( i );
		const QRect r = tree->visualItemRect( it );
		hMin = std::min( hMin, r.height() );
		hMax = std::max( hMax, r.height() );
		QWidget * w = tree->itemWidget( it, 1 );
		if ( !w )
			continue;
		rows++;
		const int dy = w->geometry().y() - r.y(), dh = w->geometry().bottom() - r.bottom();
		if ( dy != 0 || dh > 0 ) {
			off++;
			if ( first.isEmpty() )
				first = it->text( 0 );
			worst = std::max( { worst, std::abs( dy ), dh } );
		}
	}
	std::fprintf( stderr, "scene window rows: %d value rows, row h %d..%d, value off its row %d (worst %d px%s%s)\n", rows,
		hMin, hMax, off, worst, first.isEmpty() ? "" : ", first ", qPrintable( first ) );
}

QSize wwSceneWantedSize( QTreeWidget * tree, const QWidget * window )
{
	if ( !tree || wwEnvStr( "WW_SCENE_LAYOUT_RED" ).trimmed() == QLatin1String( "1" ) )
		return QSize();
	int name = 0, value = 0;
	const QFontMetrics fm( tree->font() );
	for ( int i = 0; i < tree->topLevelItemCount(); i++ ) {
		QTreeWidgetItem * it = tree->topLevelItem( i );
		QWidget * w = tree->itemWidget( it, 1 );
		if ( w ) {
			name = std::max( name, fm.horizontalAdvance( it->text( 0 ) ) );
			value = std::max( value, w->sizeHint().width() );
		}
	}
	const int chrome = tree->verticalScrollBar()->sizeHint().width() + 2 * tree->frameWidth() + 12 + 24;
	int wantW = name + 24 + value + chrome;
	int wantH = 720;
	if ( const QScreen * sc = window && window->screen() ? window->screen() : QGuiApplication::primaryScreen() ) {
		wantW = std::min( wantW, sc->availableGeometry().width() * 6 / 10 );
		wantH = std::min( wantH, sc->availableGeometry().height() * 85 / 100 );
	}
	return QSize( wantW, wantH );
}
