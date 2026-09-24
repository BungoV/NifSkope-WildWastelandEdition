R = 'E:/Projects/NifskopeWildWastelandEdition/'

def patch(path, pairs):
    s = open(R + path, 'rb').read().decode('utf-8')
    cr0 = s.count('\r')
    for old, new in pairs:
        assert s.count(old) == 1, (path, old[:70], s.count(old))
        s = s.replace(old, new)
    assert s.count('\r') == cr0
    open(R + path, 'wb').write(s.encode('utf-8'))

patch('src/ui/scenewindow.h', [
("""	QCheckBox * groundBox = nullptr;
	QLabel * statusLabel = nullptr;
""", """	QCheckBox * groundBox = nullptr;
	QLabel * statusLabel = nullptr;
	// the weather preview rows (lane PBRWX1)
	QCheckBox * skyBox = nullptr;
	QCheckBox * cloudsBox = nullptr;
	QCheckBox * sunBox = nullptr;
	QCheckBox * moonBox = nullptr;
	QDoubleSpinBox * gameDayBox = nullptr;
"""),
("""private:
	void syncEnabled();
""", """private:
	void syncEnabled();
	//! the cloud scroll: repaint at 10 Hz while Lookdev and Clouds are on (never in a harness run)
	void syncCloudTimer();
"""),
("""	QTimer * m_saveTimer = nullptr;
""", """	QTimer * m_saveTimer = nullptr;
	QTimer * m_cloudTimer = nullptr;
"""),
])

patch('src/ui/scenewindow.cpp', [
("""#include "esmweather.h"
""", """#include "esmweather.h"
#include "harnesswindow.h"
"""),
("""	/* ---- later stages (disabled rows) ---- */
	heading( tr( "Sky" ) );
	placeholderCheck( tr( "Sky" ) );
	placeholderCheck( tr( "Clouds" ) );
	placeholderCheck( tr( "Sun" ) );
	placeholderCheck( tr( "Moon" ) );
	heading( tr( "Ground" ) );""", """	/* ---- the weather preview (lane PBRWX1): every part its own live row, all OFF as shipped ---- */
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
	heading( tr( "Ground" ) );"""),
("""	connect( groundBox, &QCheckBox::toggled, this, [this, live]( bool on ) {
		if ( live )
			wwLookdevSetGround( on );
		refreshStatus();
		if ( m_repaint )
			m_repaint();
	} );
""", """	connect( groundBox, &QCheckBox::toggled, this, [this, live]( bool on ) {
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
"""),
("""	for ( QWidget * w : { static_cast<QWidget *>( pluginBox ), static_cast<QWidget *>( weatherBox ),
			static_cast<QWidget *>( hourBox ), static_cast<QWidget *>( groundBox ) } )
		if ( w )
			w->setEnabled( ld );
	refreshStatus();
}
""", """	for ( QWidget * w : { static_cast<QWidget *>( pluginBox ), static_cast<QWidget *>( weatherBox ),
			static_cast<QWidget *>( hourBox ), static_cast<QWidget *>( groundBox ),
			static_cast<QWidget *>( skyBox ), static_cast<QWidget *>( cloudsBox ), static_cast<QWidget *>( sunBox ),
			static_cast<QWidget *>( moonBox ), static_cast<QWidget *>( gameDayBox ) } )
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
"""),
])
print('ok scene')
