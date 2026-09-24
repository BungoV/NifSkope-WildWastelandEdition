/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "scenewindow.h"

#include "gl/glproperty.h"
#include "gl/scenelighting.h"
#include "gl/lookdevstage.h"
#include "esmweather.h"
#include "harnesswindow.h"
#include "ui/widgets/wwnumberfield.h"
#include "wwskin.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QHeaderView>
#include <QScreen>
#include <QSettings>
#include <QSignalBlocker>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

const char * SceneWindow::geometryKey()
{
	return "Settings/Scene Window/Geometry";
}

SceneWindow::SceneWindow( QWidget * mainWindow, std::function<void()> repaint, std::function<void( bool )> visibility )
	: QWidget( mainWindow, Qt::Tool ), m_repaint( std::move( repaint ) ), m_visibility( std::move( visibility ) )
{
	setObjectName( QStringLiteral( "SceneWindow" ) );
	setWindowTitle( tr( "Scene" ) );
	setAttribute( Qt::WA_DeleteOnClose, false );
	// the headless backstop keeps a remembered geometry off the primary screen
	// (NifSkope::wwPlaceHeadlessWindow)
	setProperty( "wwOwnGeometry", true );
	// the stored lookdev plugin/weather/hour/ground (pins win), before the rows read them
	wwLookdevLoadSettings();

	auto * page = new QVBoxLayout( this );
	page->setContentsMargins( 6, 6, 6, 6 );
	page->setSpacing( 4 );

	tree = new QTreeWidget( this );
	tree->setObjectName( QStringLiteral( "SceneRows" ) );
	tree->setColumnCount( 2 );
	tree->setHeaderLabels( { tr( "Name" ), tr( "Value" ) } );
	tree->setRootIsDecorated( false );
	tree->setUniformRowHeights( true );
	tree->setAlternatingRowColors( false );
	tree->setSelectionMode( QAbstractItemView::NoSelection );
	tree->setEditTriggers( QAbstractItemView::NoEditTriggers );
	tree->setFocusPolicy( Qt::NoFocus );
	tree->header()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
	tree->header()->setSectionResizeMode( 1, QHeaderView::Stretch );
	tree->setStyleSheet( wwSelectionTreeQss() );
	page->addWidget( tree, 1 );

	auto heading = [this]( const QString & text ) {
		auto * it = new QTreeWidgetItem( tree );
		it->setText( 0, text );
		it->setFirstColumnSpanned( true );
		QFont f = it->font( 0 );
		f.setWeight( QFont::DemiBold );
		it->setFont( 0, f );
		it->setFlags( Qt::ItemIsEnabled );
	};
	auto row = [this]( const QString & name, QWidget * w, bool enabled ) {
		auto * it = new QTreeWidgetItem( tree );
		it->setText( 0, name );
		it->setFlags( enabled ? Qt::ItemIsEnabled : Qt::NoItemFlags );
		w->setEnabled( enabled );
		tree->setItemWidget( it, 1, w );
	};
	auto placeholderCheck = [&]( const QString & name ) {
		row( name, new QCheckBox( tree ), false );
	};

	/* ---- Mode (R2a: real) ---- */
	heading( tr( "Mode" ) );

	modeBox = new QComboBox( tree );
	modeBox->setObjectName( QStringLiteral( "sceneMode" ) );
	modeBox->addItems( { tr( "Legacy" ), tr( "Studio" ), tr( "Lookdev" ) } );
	modeBox->setCurrentIndex( std::clamp( wwSceneMode(), 0, 2 ) );
	wwMatchFieldStyle( modeBox );
	row( tr( "Lighting" ), modeBox, true );

	exposureBox = new QDoubleSpinBox( tree );
	exposureBox->setObjectName( QStringLiteral( "sceneExposure" ) );
	exposureBox->setRange( -10.0, 10.0 );
	exposureBox->setSingleStep( 0.1 );
	exposureBox->setDecimals( 2 );
	exposureBox->setSuffix( QStringLiteral( " EV" ) );
	exposureBox->setValue( double( wwSceneExposureEV() ) );
	wwMakeScrubField( exposureBox );
	row( tr( "Exposure" ), exposureBox, true );

	viewBox = new QComboBox( tree );
	viewBox->setObjectName( QStringLiteral( "sceneViewTransform" ) );
	viewBox->addItems( { tr( "Standard" ), tr( "AgX" ), tr( "Khronos PBR Neutral" ) } );
	viewBox->setCurrentIndex( wwSceneViewTransform() );
	wwMatchFieldStyle( viewBox );
	row( tr( "View Transform" ), viewBox, true );

	routeViewBox = new QCheckBox( tree );
	routeViewBox->setObjectName( QStringLiteral( "sceneRouteView" ) );
	routeViewBox->setChecked( pbrmRouteView() );
	row( tr( "PBR Route View" ), routeViewBox, true );

	/* ---- Weather (R2b: W1 real -- plugin, weather, hour) ---- */
	heading( tr( "Weather" ) );
	pluginBox = new QComboBox( tree );
	pluginBox->setObjectName( QStringLiteral( "lookdevPlugin" ) );
	wwMatchFieldStyle( pluginBox );
	row( tr( "Plugin" ), pluginBox, true );
	weatherBox = new QComboBox( tree );
	weatherBox->setObjectName( QStringLiteral( "lookdevWeather" ) );
	wwMatchFieldStyle( weatherBox );
	row( tr( "Weather" ), weatherBox, true );
	hourBox = new QDoubleSpinBox( tree );
	hourBox->setObjectName( QStringLiteral( "lookdevHour" ) );
	hourBox->setRange( 0.0, 23.99 );
	hourBox->setSingleStep( 0.25 );
	hourBox->setDecimals( 2 );
	hourBox->setValue( wwLookdevHour() );
	wwMakeScrubField( hourBox );
	row( tr( "Hour" ), hourBox, true );
	statusLabel = new QLabel( tree );
	statusLabel->setObjectName( QStringLiteral( "lookdevStatus" ) );
	statusLabel->setWordWrap( true );
	statusLabel->setTextInteractionFlags( Qt::TextSelectableByMouse );
	row( tr( "Status" ), statusLabel, true );

	/* ---- the weather preview (lane PBRWX1): every part its own live row, all OFF as shipped ---- */
	heading( tr( "Sky" ) );
	auto liveCheck = [&]( QCheckBox *& box, const char * name, const QString & label, bool on ) {
		box = new QCheckBox( tree );
		box->setObjectName( QLatin1StringView( name ) );
		box->setChecked( on );
		row( label, box, true );
	};
	liveCheck( skyBox, "lookdevSky", tr( "Sky" ), wwLookdevSky() );
	liveCheck( cloudsBox, "lookdevClouds", tr( "Clouds" ), wwLookdevClouds() );
	liveCheck( sunBox, "lookdevSun", tr( "Sun" ), wwLookdevSun() );
	liveCheck( moonBox, "lookdevMoon", tr( "Moon" ), wwLookdevMoon() );
	gameDayBox = new QDoubleSpinBox( tree );
	gameDayBox->setObjectName( QStringLiteral( "lookdevGameDay" ) );
	gameDayBox->setRange( 0.0, 100000.0 );
	gameDayBox->setSingleStep( 1.0 );
	gameDayBox->setDecimals( 0 );
	gameDayBox->setValue( wwLookdevGameDay() );
	wwMakeScrubField( gameDayBox );
	row( tr( "Game Day" ), gameDayBox, true );
	heading( tr( "Ground" ) );
	groundBox = new QCheckBox( tree );
	groundBox->setObjectName( QStringLiteral( "lookdevGround" ) );
	groundBox->setChecked( wwLookdevGround() );
	row( tr( "Ground Plane" ), groundBox, true );
	heading( tr( "Fog" ) );
	liveCheck( fogBox, "lookdevFog", tr( "Fog" ), wwLookdevFog() );	// lane FOG1
	heading( tr( "Effects" ) );
	liveCheck( shadowsBox, "lookdevShadows", tr( "Cascaded Shadows" ), wwLookdevShadows() );	// lane CSM1
	placeholderCheck( tr( "Contact Shadows" ) );
	placeholderCheck( tr( "SSAO" ) );
	placeholderCheck( tr( "SSGI" ) );
	placeholderCheck( tr( "Bloom" ) );

	/* ---- live: every enabled row applies on change ----
	 * red "nolive": the rows change but never reach the state, which the
	 * harness's live-effect check must refuse. */
	const bool live = !wwR2aRed( "nolive" );
	connect( modeBox, qOverload<int>( &QComboBox::currentIndexChanged ), this, [this, live]( int i ) {
		if ( live )
			wwSetSceneMode( i == 2 ? WwSceneLookdev : i == 1 ? WwSceneStudio : WwSceneLegacy );
		syncEnabled();
		if ( i == 2 )
			refreshLookdev();
		if ( m_repaint )
			m_repaint();
	} );
	connect( exposureBox, qOverload<double>( &QDoubleSpinBox::valueChanged ), this, [this, live]( double v ) {
		if ( live ) {
			wwSetSceneExposureEV( float( v ) );
			wwSceneSaveSettings();
		}
		if ( m_repaint )
			m_repaint();
	} );
	connect( viewBox, qOverload<int>( &QComboBox::currentIndexChanged ), this, [this, live]( int i ) {
		if ( live ) {
			wwSetSceneViewTransform( i );
			wwSceneSaveSettings();
		}
		if ( m_repaint )
			m_repaint();
	} );
	connect( routeViewBox, &QCheckBox::toggled, this, [this, live]( bool on ) {
		if ( live )
			setPbrmRouteView( on );
		if ( m_repaint )
			m_repaint();
	} );
	connect( pluginBox, qOverload<int>( &QComboBox::activated ), this, [this, live]( int i ) {
		QString path = pluginBox->itemData( i ).toString();
		if ( path.isEmpty() ) {
			// "Browse..."
			path = QFileDialog::getOpenFileName( this, tr( "Plugin" ), EsmWeather::gameDataDir(),
				QStringLiteral( "Plugins (*.esm *.esp *.esl)" ) );
			if ( path.isEmpty() ) {
				refreshPlugins();
				return;
			}
		}
		if ( live )
			wwLookdevSetPlugin( path );
		refreshPlugins();
		refreshLookdev();
		if ( m_repaint )
			m_repaint();
	} );
	connect( weatherBox, qOverload<int>( &QComboBox::activated ), this, [this, live]( int i ) {
		if ( live )
			wwLookdevSetWeather( weatherBox->itemData( i ).toString() );
		refreshStatus();
		if ( m_repaint )
			m_repaint();
	} );
	connect( hourBox, qOverload<double>( &QDoubleSpinBox::valueChanged ), this, [this, live]( double h ) {
		if ( live )
			wwLookdevSetHour( h );
		refreshStatus();
		if ( m_repaint )
			m_repaint();
	} );
	connect( groundBox, &QCheckBox::toggled, this, [this, live]( bool on ) {
		if ( live )
			wwLookdevSetGround( on );
		refreshStatus();
		if ( m_repaint )
			m_repaint();
	} );
	auto liveToggle = [this, live]( QCheckBox * box, void ( *set )( bool ) ) {
		connect( box, &QCheckBox::toggled, this, [this, live, set]( bool on ) {
			if ( live )
				set( on );
			syncCloudTimer();
			refreshStatus();
			if ( m_repaint )
				m_repaint();
		} );
	};
	liveToggle( skyBox, &wwLookdevSetSky );
	liveToggle( cloudsBox, &wwLookdevSetClouds );
	liveToggle( sunBox, &wwLookdevSetSun );
	liveToggle( moonBox, &wwLookdevSetMoon );
	liveToggle( fogBox, &wwLookdevSetFog );
	liveToggle( shadowsBox, &wwLookdevSetShadows );
	connect( gameDayBox, qOverload<double>( &QDoubleSpinBox::valueChanged ), this, [this, live]( double d ) {
		if ( live )
			wwLookdevSetGameDay( d );
		refreshStatus();
		if ( m_repaint )
			m_repaint();
	} );
	m_cloudTimer = new QTimer( this );
	m_cloudTimer->setInterval( 100 );
	connect( m_cloudTimer, &QTimer::timeout, this, [this]() {
		if ( m_repaint )
			m_repaint();
	} );
	refreshPlugins();
	syncEnabled();
	if ( wwLookdevActive() )
		refreshLookdev();
	else
		refreshStatus();

	m_saveTimer = new QTimer( this );
	m_saveTimer->setSingleShot( true );
	m_saveTimer->setInterval( 300 );
	connect( m_saveTimer, &QTimer::timeout, this, [this]() {
		if ( isVisible() )
			saveGeometryNow();
	} );

	// the stored geometry, before the first show (so it never flashes elsewhere)
	const QByteArray g = QSettings().value( QLatin1StringView( geometryKey() ) ).toByteArray();
	if ( !g.isEmpty() && restoreGeometry( g ) )
		m_placed = true;
	else
		resize( 380, 560 );
}

void SceneWindow::syncEnabled()
{
	// exposure and the view transform act in Studio and Lookdev; greyed in Legacy.
	// The weather and ground rows act in Lookdev only.
	const int m = modeBox ? modeBox->currentIndex() : 0;
	exposureBox->setEnabled( m >= 1 );
	viewBox->setEnabled( m >= 1 );
	const bool ld = m == 2;
	for ( QWidget * w : { static_cast<QWidget *>( pluginBox ), static_cast<QWidget *>( weatherBox ),
			static_cast<QWidget *>( hourBox ), static_cast<QWidget *>( groundBox ),
			static_cast<QWidget *>( skyBox ), static_cast<QWidget *>( cloudsBox ), static_cast<QWidget *>( sunBox ),
			static_cast<QWidget *>( moonBox ), static_cast<QWidget *>( gameDayBox ),
			static_cast<QWidget *>( fogBox ), static_cast<QWidget *>( shadowsBox ) } )
		if ( w )
			w->setEnabled( ld );
	syncCloudTimer();
	refreshStatus();
}

void SceneWindow::syncCloudTimer()
{
	if ( !m_cloudTimer )
		return;
	// a harness run pins the cloud clock (WW_LOOKDEV_CLOUDTIME, else 0 s): nothing to animate
	const bool run = modeBox && modeBox->currentIndex() == 2 && wwLookdevClouds() && !wwHarnessRun();
	if ( run && !m_cloudTimer->isActive() )
		m_cloudTimer->start();
	else if ( !run && m_cloudTimer->isActive() )
		m_cloudTimer->stop();
}

void SceneWindow::refreshPlugins()
{
	if ( !pluginBox )
		return;
	QSignalBlocker b( pluginBox );
	pluginBox->clear();
	const QString cur = wwLookdevPlugin();
	QStringList all = wwLookdevAvailablePlugins();
	if ( !cur.isEmpty() && !all.contains( cur, Qt::CaseInsensitive ) )
		all.prepend( cur );
	int at = -1;
	for ( const QString & p : all ) {
		if ( QFileInfo( p ).absoluteFilePath().compare( QFileInfo( cur ).absoluteFilePath(), Qt::CaseInsensitive ) == 0 )
			at = pluginBox->count();
		pluginBox->addItem( QFileInfo( p ).fileName(), p );
	}
	pluginBox->addItem( tr( "Browse..." ), QString() );
	pluginBox->setCurrentIndex( std::max( at, 0 ) );
}

void SceneWindow::refreshLookdev()
{
	if ( !weatherBox )
		return;
	QSignalBlocker b( weatherBox );
	weatherBox->clear();
	const QVector<WwWeatherEntry> list = wwLookdevWeathers();
	const QString key = wwLookdevWeatherKey();
	int at = -1;
	for ( const WwWeatherEntry & e : list ) {
		const QString id = QString( "%1" ).arg( e.formID, 8, 16, QChar( '0' ) ).toUpper();
		if ( e.edid.compare( key, Qt::CaseInsensitive ) == 0 || id.compare( key, Qt::CaseInsensitive ) == 0
			|| key.endsWith( QStringLiteral( "[" ) + id + QStringLiteral( "]" ), Qt::CaseInsensitive ) )
			at = weatherBox->count();
		weatherBox->addItem( QStringLiteral( "%1 [%2]" ).arg( e.edid, id ), e.edid.isEmpty() ? id : e.edid );
	}
	if ( at >= 0 )
		weatherBox->setCurrentIndex( at );
	refreshStatus();
}

void SceneWindow::refreshStatus()
{
	if ( !statusLabel )
		return;
	statusLabel->setText( wwLookdevActive() ? wwLookdevSummary() : tr( "Lookdev off" ) );
}

void SceneWindow::saveGeometryNow()
{
	if ( wwR2aRed( "nosave" ) )
		return;
	QSettings().setValue( QLatin1StringView( geometryKey() ), saveGeometry() );
}

void SceneWindow::showEvent( QShowEvent * e )
{
	if ( !m_placed ) {
		m_placed = true;
		// first ever open: beside the main window's right edge, on its screen
		if ( QWidget * mw = parentWidget() ) {
			const QRect r = mw->frameGeometry();
			move( r.right() - width() - 40, r.top() + 80 );
		}
	}
	QWidget::showEvent( e );
	if ( m_visibility )
		m_visibility( true );
}

void SceneWindow::hideEvent( QHideEvent * e )
{
	saveGeometryNow();
	QWidget::hideEvent( e );
	if ( m_visibility )
		m_visibility( false );
}

void SceneWindow::moveEvent( QMoveEvent * e )
{
	QWidget::moveEvent( e );
	if ( isVisible() && m_saveTimer )
		m_saveTimer->start();
}

void SceneWindow::resizeEvent( QResizeEvent * e )
{
	QWidget::resizeEvent( e );
	if ( isVisible() && m_saveTimer )
		m_saveTimer->start();
}
