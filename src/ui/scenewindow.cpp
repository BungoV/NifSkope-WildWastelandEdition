/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "scenewindow.h"

#include "gl/glproperty.h"
#include "gl/scenelighting.h"
#include "gl/lookdevstage.h"
#include "gl/cellvolair.h"	// lane VOLAIR1
#include "gl/cellpasstime.h"	// lane PERF1
#include "gl/gametaa.h"
#include "gl/shadowset.h"	// lane TERRSHADOW1
#include "gl/atmosphere.h"	// lane ATMO1
#include "gl/cloudmap.h"	// lane CLOUD1
#include "gl/cloudvol.h"	// lane CLOUD2
#include "gl/cloudshadow.h"	// lane CLOUD3
#include "gl/cloudfly.h"	// lane CLOUD4
#include "gl/cellssgi.h"	// lane SSGI1
#include "gl/celllights.h"	// lane GAPTOOL1: the SSAO row
#include "gl/cellbloom.h"	// lane GAPTOOL1: the Bloom row
#include "gl/fxparity.h"	// lane FXPARITY1
#include "ui/cellshowpanel.h"	// lane OPENCELLFIX2: the Display tab's fog and shadow rows share these settings
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
	// lane ATMO1: the physical sky in place of the dome, Off / Low / Medium / High / Ultra, ships Off
	physicalSkyBox = new QComboBox( tree );
	physicalSkyBox->setObjectName( QStringLiteral( "physicalSky" ) );
	physicalSkyBox->addItems( { tr( "Off" ), tr( "Low" ), tr( "Medium" ), tr( "High" ), tr( "Ultra" ) } );
	physicalSkyBox->setCurrentIndex( std::clamp( wwAtmoTier(), 0, 4 ) );
	wwMatchFieldStyle( physicalSkyBox );
	row( tr( "Physical Sky" ), physicalSkyBox, true );
	// lane SKYA (ATMO2): the physical sky's air on the surfaces (needs Physical Sky), Off / On, ships Off
	aerialPerspectiveBox = new QComboBox( tree );
	aerialPerspectiveBox->setObjectName( QStringLiteral( "aerialPerspective" ) );
	aerialPerspectiveBox->addItems( { tr( "Off" ), tr( "On" ) } );
	aerialPerspectiveBox->setCurrentIndex( std::clamp( wwAtmoAp(), 0, 1 ) );
	wwMatchFieldStyle( aerialPerspectiveBox );
	row( tr( "Aerial Perspective" ), aerialPerspectiveBox, true );
	// lane SKYA (ATMOSUN1): the key light and the ambient coloured by the air (needs Physical Sky), ships Off
	sunlightFromSkyBox = new QComboBox( tree );
	sunlightFromSkyBox->setObjectName( QStringLiteral( "sunlightFromSky" ) );
	sunlightFromSkyBox->addItems( { tr( "Off" ), tr( "Tint" ), tr( "Full" ) } );
	sunlightFromSkyBox->setCurrentIndex( std::clamp( wwAtmoSun(), 0, 2 ) );
	wwMatchFieldStyle( sunlightFromSkyBox );
	row( tr( "Sunlight From Sky" ), sunlightFromSkyBox, true );
	// lane SKYA (NIGHT1): the moon's phase in its light, stars and moon dimmed by the air (needs Physical Sky), ships Off
	moonPhaseStarsBox = new QComboBox( tree );
	moonPhaseStarsBox->setObjectName( QStringLiteral( "moonPhaseStars" ) );
	moonPhaseStarsBox->addItems( { tr( "Off" ), tr( "On" ) } );
	moonPhaseStarsBox->setCurrentIndex( std::clamp( wwAtmoNight(), 0, 1 ) );
	wwMatchFieldStyle( moonPhaseStarsBox );
	row( tr( "Moon Phase and Stars" ), moonPhaseStarsBox, true );
	// lane SKYA (EXPO1): the eye adaptation counts the sky (needs Physical Sky), ships Off
	exposureFromSkyBox = new QComboBox( tree );
	exposureFromSkyBox->setObjectName( QStringLiteral( "exposureFromSky" ) );
	exposureFromSkyBox->addItems( { tr( "Off" ), tr( "On" ) } );
	exposureFromSkyBox->setCurrentIndex( std::clamp( wwAtmoExpo(), 0, 1 ) );
	wwMatchFieldStyle( exposureFromSkyBox );
	row( tr( "Exposure From Sky" ), exposureFromSkyBox, true );
	// lane SKYA (ATMOCHECK1): the physical sky's luminance in the weather's colors (needs Physical Sky), ships Physical
	skyColorBox = new QComboBox( tree );
	skyColorBox->setObjectName( QStringLiteral( "skyColor" ) );
	skyColorBox->addItems( { tr( "Physical" ), tr( "Weather Tint" ) } );
	skyColorBox->setCurrentIndex( std::clamp( wwAtmoTint(), 0, 1 ) );
	wwMatchFieldStyle( skyColorBox );
	row( tr( "Sky Color" ), skyColorBox, true );
	// lane SKYA (SKYGI1): the probes' sky light from the physical sky (needs Physical Sky), ships Off
	giSkyFromPhysicalBox = new QComboBox( tree );
	giSkyFromPhysicalBox->setObjectName( QStringLiteral( "giSkyFromPhysicalSky" ) );
	giSkyFromPhysicalBox->addItems( { tr( "Off" ), tr( "On" ) } );
	giSkyFromPhysicalBox->setCurrentIndex( std::clamp( wwAtmoGi(), 0, 1 ) );
	wwMatchFieldStyle( giSkyFromPhysicalBox );
	row( tr( "GI Sky From Physical Sky" ), giSkyFromPhysicalBox, true );
	liveCheck( cloudsBox, "lookdevClouds", tr( "Clouds" ), wwLookdevClouds() );
	// lane CLOUD1: the per-weather cloud map (cloud volumes read it); ships Off
	cloudMapBox = new QComboBox( tree );
	cloudMapBox->setObjectName( QStringLiteral( "cloudMap" ) );
	cloudMapBox->addItems( { tr( "Off" ), tr( "Low" ), tr( "Medium" ), tr( "High" ), tr( "Ultra" ) } );
	cloudMapBox->setCurrentIndex( std::clamp( wwCloudMapTier(), 0, 4 ) );
	wwMatchFieldStyle( cloudMapBox );
	row( tr( "Cloud Map" ), cloudMapBox, true );
	// lane CLOUD2: the volume clouds over the physical sky, from the cloud map; ships Off
	volumeCloudsBox = new QComboBox( tree );
	volumeCloudsBox->setObjectName( QStringLiteral( "volumeClouds" ) );
	volumeCloudsBox->addItems( { tr( "Off" ), tr( "Low" ), tr( "Medium" ), tr( "High" ), tr( "Ultra" ) } );
	volumeCloudsBox->setCurrentIndex( std::clamp( wwCloudVolTier(), 0, 4 ) );
	wwMatchFieldStyle( volumeCloudsBox );
	row( tr( "Volume Clouds" ), volumeCloudsBox, true );
	// lane CLOUD3: the clouds' shadows on the ground, in the fog and in the GI's sun; ships Off
	cloudShadowsBox = new QComboBox( tree );
	cloudShadowsBox->setObjectName( QStringLiteral( "cloudShadows" ) );
	cloudShadowsBox->addItems( { tr( "Off" ), tr( "Low" ), tr( "Medium" ), tr( "High" ), tr( "Ultra" ) } );
	cloudShadowsBox->setCurrentIndex( std::clamp( wwCloudShadowTier(), 0, 4 ) );
	wwMatchFieldStyle( cloudShadowsBox );
	row( tr( "Cloud Shadows" ), cloudShadowsBox, true );
	// lane CLOUD4: a camera at cloud height flies through the volume clouds; ships Off
	flyThroughCloudsBox = new QComboBox( tree );
	flyThroughCloudsBox->setObjectName( QStringLiteral( "flyThroughClouds" ) );
	flyThroughCloudsBox->addItems( { tr( "Off" ), tr( "Low" ), tr( "Medium" ), tr( "High" ), tr( "Ultra" ) } );
	flyThroughCloudsBox->setCurrentIndex( std::clamp( wwCloudFlyTier(), 0, 4 ) );
	wwMatchFieldStyle( flyThroughCloudsBox );
	row( tr( "Fly-through Clouds" ), flyThroughCloudsBox, true );
	liveCheck( sunBox, "lookdevSun", tr( "Sun" ), wwLookdevSun() );
	// lane TERRSHADOW1: Engine (the sky clock's arc) or Real (Boston's solar path); ships Engine
	sunPathBox = new QComboBox( tree );
	sunPathBox->setObjectName( QStringLiteral( "sunPath" ) );
	sunPathBox->addItems( { tr( "Engine" ), tr( "Real" ) } );
	sunPathBox->setCurrentIndex( std::clamp( wwSunPathMode(), 0, 1 ) );
	wwMatchFieldStyle( sunPathBox );
	row( tr( "Sun Path" ), sunPathBox, true );
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
	heading( tr( "Volumetric Air" ) );
	liveCheck( volAirBox, "volAir", tr( "Volumetric Air" ), wwVolAirOn() );	// lane VOLAIR1: every mode; replaces the fog while on
	// lane FARGRID1: the far grid, Off / Low / Medium / High / Ultra, ships Off
	farAirBox = new QComboBox( tree );
	farAirBox->setObjectName( QStringLiteral( "farAir" ) );
	farAirBox->addItems( { tr( "Off" ), tr( "Low" ), tr( "Medium" ), tr( "High" ), tr( "Ultra" ) } );
	farAirBox->setCurrentIndex( std::clamp( wwVolAirFarTier(), 0, 4 ) );
	wwMatchFieldStyle( farAirBox );
	row( tr( "Far Air" ), farAirBox, true );
	liveCheck( volPartBox, "volPart", tr( "Volumetric Particles" ), wwVolPartOn() );	// lane VOLPART1: lit steam, seen through the air
	liveCheck( fxParityBox, "fxParity", tr( "Particle Game Rules" ), wwFxParityOn() );	// lane FXPARITY1: every mode, ships off
	liveCheck( fogDriftBox, "fogDrift", tr( "Fog Drift" ), wwVolAirDriftOn() );	// lane FOGDRIFT1: ships off
	heading( tr( "Effects" ) );
	liveCheck( shadowsBox, "lookdevShadows", tr( "Cascaded Shadows" ), wwLookdevShadows() );	// lane CSM1
	liveCheck( taaBox, "gameTaa", tr( "Temporal Anti-Aliasing" ), wwGameTaaRowOn() );	// lane MOTION1: every mode, ships off
	// lane TERRSHADOW1: the unified shadow set, each Off / Low / Medium / High / Ultra, ships Off
	auto tierBox = [&]( QComboBox *& box, const char * name, const QString & label, int tier ) {
		box = new QComboBox( tree );
		box->setObjectName( QLatin1StringView( name ) );
		box->addItems( { tr( "Off" ), tr( "Low" ), tr( "Medium" ), tr( "High" ), tr( "Ultra" ) } );
		box->setCurrentIndex( std::clamp( tier, 0, 4 ) );
		wwMatchFieldStyle( box );
		row( label, box, true );
	};
	tierBox( modernTaaBox, "modernTaa", tr( "Improved Temporal Anti-Aliasing" ), wwTaa2Tier() );	// lane TAA2: ships Off
	liveCheck( modernTaaHdrBox, "modernTaaHdr", tr( "Improved Temporal Anti-Aliasing Before Tone Map" ), wwTaa2HdrOn() );	// lane TAA3: ships Off
	tierBox( cascadeQualityBox, "cascadeQuality", tr( "Cascade Quality" ), wwShadowCascadeTier() );
	tierBox( contactShadowsBox, "contactShadows", tr( "Screen-Space Shadows" ), wwShadowContactTier() );
	tierBox( terrainShadowsBox, "terrainShadows", tr( "Terrain Shadows" ), wwShadowTerrainTier() );
	// lane SHADOWPAR1: every FO4CS-parity item at once (WW_SHADOW_PAR picks single ones), and the cascade reach
	liveCheck( shadowParBox, "shadowPar", tr( "Fusion Shaders Shadow Rules" ), wwShadowParMask() != 0 );
	shadowDistanceBox = new QDoubleSpinBox( tree );
	shadowDistanceBox->setObjectName( QStringLiteral( "shadowDistance" ) );
	shadowDistanceBox->setRange( 2000.0, 24000.0 );
	shadowDistanceBox->setSingleStep( 500.0 );
	shadowDistanceBox->setDecimals( 0 );
	shadowDistanceBox->setValue( wwShadowDistance() );
	wwMakeScrubField( shadowDistanceBox );
	row( tr( "Shadow Distance" ), shadowDistanceBox, true );
	// lane GAPTOOL1: SSAO and Bloom were greyed placeholders; now the cell view's own AO and bloom mode (a pin greys them)
	liveCheck( aoBox, "cellAo", tr( "Screen-Space Ambient Occlusion" ), wwCellAoOn() );
	aoBox->setEnabled( !wwCellAoPinned() );
	liveCheck( ssgiBox, "cellSsgi", tr( "Screen-Space Global Illumination" ), wwCellSsgiOn() );	// lane SSGI1: ships off
	bloomBox = new QComboBox( tree );
	bloomBox->setObjectName( QStringLiteral( "cellBloom" ) );
	bloomBox->addItems( { tr( "Vanilla" ), tr( "Fusion Shaders" ) } );
	bloomBox->setCurrentIndex( wwCellBloomSettings().fo4cs ? 1 : 0 );
	wwMatchFieldStyle( bloomBox );
	row( tr( "Bloom" ), bloomBox, true );
	bloomBox->setEnabled( !wwCellBloomPinned( "mode" ) );
	heading( tr( "Diagnostics" ) );
	liveCheck( frameTimersBox, "frameTimers", tr( "Frame Timers" ), PassTime::on() );	// lane PERF1: ships off

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
		const bool order = path == QLatin1StringView( "<load order>" );	// lane SKYTOP1
		if ( order )
			path.clear();
		else if ( path.isEmpty() ) {
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
	liveToggle( volAirBox, &wwVolAirSetOn );
	liveToggle( volPartBox, &wwVolPartSetOn );
	liveToggle( fxParityBox, &wwFxParitySetOn );	// lane FXPARITY1
	liveToggle( fogDriftBox, &wwVolAirSetDriftOn );	// lane FOGDRIFT1
	liveToggle( shadowsBox, &wwLookdevSetShadows );
	liveToggle( taaBox, &wwGameTaaSetOn );
	liveToggle( modernTaaHdrBox, &wwTaa2SetHdrOn );	// lane TAA3
	liveToggle( ssgiBox, &wwCellSsgiSetOn );
	liveToggle( aoBox, &wwCellAoSetOn );
	connect( bloomBox, qOverload<int>( &QComboBox::currentIndexChanged ), this, [this, live]( int i ) {
		if ( live ) {
			WwCellBloomSettings s = wwCellBloomSettings();
			s.fo4cs = i == 1;
			wwCellBloomSet( s );
		}
		refreshStatus();
		if ( m_repaint )
			m_repaint();
	} );
	liveToggle( frameTimersBox, &PassTime::setOn );	// lane PERF1
	auto liveTier = [this, live]( QComboBox * box, void ( *set )( int ) ) {
		connect( box, qOverload<int>( &QComboBox::currentIndexChanged ), this, [this, live, set]( int i ) {
			if ( live )
				set( i );
			refreshStatus();
			if ( m_repaint )
				m_repaint();
		} );
	};
	liveTier( sunPathBox, &wwSunPathSetMode );
	liveTier( physicalSkyBox, &wwAtmoSetTier );	// lane ATMO1
	liveTier( aerialPerspectiveBox, &wwAtmoSetAp );	// lane SKYA
	liveTier( sunlightFromSkyBox, &wwAtmoSetSun );
	liveTier( moonPhaseStarsBox, &wwAtmoSetNight );
	liveTier( exposureFromSkyBox, &wwAtmoSetExpo );
	liveTier( skyColorBox, &wwAtmoSetTint );
	liveTier( giSkyFromPhysicalBox, &wwAtmoSetGi );
	liveTier( cloudMapBox, &wwCloudMapSetTier );	// lane CLOUD1
	liveTier( volumeCloudsBox, &wwCloudVolSetTier );	// lane CLOUD2
	liveTier( cloudShadowsBox, &wwCloudShadowSetTier );	// lane CLOUD3
	liveTier( flyThroughCloudsBox, &wwCloudFlySetTier );	// lane CLOUD4
	liveTier( modernTaaBox, &wwTaa2SetTier );	// lane TAA2
	liveTier( cascadeQualityBox, &wwShadowSetCascadeTier );
	liveTier( contactShadowsBox, &wwShadowSetContactTier );
	liveTier( terrainShadowsBox, &wwShadowSetTerrainTier );
	liveTier( farAirBox, &wwVolAirSetFarTier );	// lane FARGRID1
	connect( shadowParBox, &QCheckBox::toggled, this, [this, live]( bool on ) {
		if ( live )
			wwShadowSetParMask( on ? WwParAll : 0 );
		refreshStatus();
		if ( m_repaint )
			m_repaint();
	} );
	connect( shadowDistanceBox, qOverload<double>( &QDoubleSpinBox::valueChanged ), this, [this, live]( double d ) {
		if ( live )
			wwShadowSetDistance( d );
		refreshStatus();
		if ( m_repaint )
			m_repaint();
	} );
	if ( live ) {	// lane OPENCELLFIX2 (notes 22, 26)
		auto shareTick = [this]( QCheckBox * box, const char * topic, bool ( *get )() ) {
			const QString t = QLatin1String( topic );
			connect( box, &QCheckBox::toggled, this, [t]() { wwLiveChanged( t ); } );
			wwLiveWatch( box, t, [box, get]() {
				const QSignalBlocker b( box );
				box->setChecked( get() );
			} );
		};
		auto shareTier = [this]( QComboBox * box, const char * topic, int ( *get )() ) {
			const QString t = QLatin1String( topic );
			connect( box, qOverload<int>( &QComboBox::currentIndexChanged ), this, [t]() { wwLiveChanged( t ); } );
			wwLiveWatch( box, t, [box, get]() {
				const QSignalBlocker b( box );
				box->setCurrentIndex( std::clamp( get(), 0, 4 ) );
			} );
		};
		shareTick( volAirBox, "volAir", &wwVolAirOn );
		shareTier( farAirBox, "farAir", &wwVolAirFarTier );
		shareTick( volPartBox, "volPart", &wwVolPartOn );
		shareTick( fogDriftBox, "fogDrift", &wwVolAirDriftOn );
		shareTick( shadowsBox, "lookdevShadows", &wwLookdevShadows );
		shareTier( cascadeQualityBox, "cascadeQuality", &wwShadowCascadeTier );
		shareTier( contactShadowsBox, "contactShadows", &wwShadowContactTier );
		shareTier( terrainShadowsBox, "terrainShadows", &wwShadowTerrainTier );
		shareTick( shadowParBox, "shadowPar", []() { return wwShadowParMask() != 0; } );
		connect( shadowDistanceBox, qOverload<double>( &QDoubleSpinBox::valueChanged ), this,
			[]() { wwLiveChanged( QStringLiteral( "shadowDistance" ) ); } );
		wwLiveWatch( shadowDistanceBox, QStringLiteral( "shadowDistance" ), [box = shadowDistanceBox]() {
			const QSignalBlocker b( box );
			box->setValue( wwShadowDistance() );
		} );
	}
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
			static_cast<QWidget *>( fogBox ), static_cast<QWidget *>( shadowsBox ),
			static_cast<QWidget *>( sunPathBox ), static_cast<QWidget *>( physicalSkyBox ),
			static_cast<QWidget *>( aerialPerspectiveBox ), static_cast<QWidget *>( sunlightFromSkyBox ),
			static_cast<QWidget *>( moonPhaseStarsBox ), static_cast<QWidget *>( exposureFromSkyBox ),
			static_cast<QWidget *>( skyColorBox ), static_cast<QWidget *>( giSkyFromPhysicalBox ),
			static_cast<QWidget *>( cloudMapBox ), static_cast<QWidget *>( volumeCloudsBox ),
			static_cast<QWidget *>( cloudShadowsBox ), static_cast<QWidget *>( flyThroughCloudsBox ),
			static_cast<QWidget *>( cascadeQualityBox ),
			static_cast<QWidget *>( contactShadowsBox ), static_cast<QWidget *>( terrainShadowsBox ),
			static_cast<QWidget *>( shadowParBox ), static_cast<QWidget *>( shadowDistanceBox ) } )
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
	const bool run = modeBox && modeBox->currentIndex() == 2 && wwLookdevClouds() && wwWorldClockLive();
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
	// lane SKYTOP1: the first row reads the weather from the whole load order (the default); a plugin row overrides it
	pluginBox->addItem( tr( "Load order" ), QStringLiteral( "<load order>" ) );
	int at = cur.isEmpty() ? 0 : -1;
	for ( const QString & p : all ) {
		if ( !cur.isEmpty()
			&& QFileInfo( p ).absoluteFilePath().compare( QFileInfo( cur ).absoluteFilePath(), Qt::CaseInsensitive ) == 0 )
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
