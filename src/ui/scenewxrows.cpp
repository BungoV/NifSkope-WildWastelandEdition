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
	for ( int i = 0; i < tree->topLevelItemCount(); i++ )
		tree->topLevelItem( i )->setSizeHint( 0, QSize( -1, h ) );
	tree->doItemsLayout();
	std::fprintf( stderr, "scene window: rows %d px high (%d rows)\n", h, tree->topLevelItemCount() );
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
