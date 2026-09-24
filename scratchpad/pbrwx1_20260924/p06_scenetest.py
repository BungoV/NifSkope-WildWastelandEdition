R = 'E:/Projects/NifskopeWildWastelandEdition/'
P = R + 'src/scenetest.cpp'
s = open(P, 'rb').read().decode('utf-8')
cr0 = s.count('\r')

def rep(old, new):
    global s
    assert s.count(old) == 1, (old[:70], s.count(old))
    s = s.replace(old, new)

rep("""     WW_SCENE_TEST_LOG=<path>
""", """     WW_SCENE_TEST_WEATHER=1
         (weather preview, lane PBRWX1) after the lookdev leg: the Sky, Clouds,
         Sun and Moon rows start OFF (masters ship off), are enabled in Lookdev,
         and each one reaches the state AND the picture (Sky / Sun / Clouds by
         pixels, Moon by the drawn echo, the Game Day row by the Status line).
         Red: WW_R2A_RED=nolive (the rows never reach the state): FAILS.
     WW_SCENE_TEST_LOG=<path>
""")

rep("""void restartLeg( NifSkope * skope, WwScState & st )
{""", """void weatherLeg( NifSkope * skope, WwScState & st )
{
	QWidget * w = skope->findChild<QWidget *>( QStringLiteral( "SceneWindow" ) );
	check( st, QStringLiteral( "(floor) the Scene window exists (weather leg)" ), w != nullptr );
	if ( !w )
		return;
	auto * mode = w->findChild<QComboBox *>( QStringLiteral( "sceneMode" ) );
	auto * hour = w->findChild<QDoubleSpinBox *>( QStringLiteral( "lookdevHour" ) );
	auto * status = w->findChild<QLabel *>( QStringLiteral( "lookdevStatus" ) );
	auto * sky = w->findChild<QCheckBox *>( QStringLiteral( "lookdevSky" ) );
	auto * clouds = w->findChild<QCheckBox *>( QStringLiteral( "lookdevClouds" ) );
	auto * sun = w->findChild<QCheckBox *>( QStringLiteral( "lookdevSun" ) );
	auto * moon = w->findChild<QCheckBox *>( QStringLiteral( "lookdevMoon" ) );
	auto * day = w->findChild<QDoubleSpinBox *>( QStringLiteral( "lookdevGameDay" ) );
	const bool all = mode && hour && status && sky && clouds && sun && moon && day;
	check( st, QStringLiteral( "(floor) the Sky, Clouds, Sun, Moon and Game Day rows exist" ), all );
	if ( !all )
		return;
	check( st, QStringLiteral( "(ship) every preview row starts OFF in a fresh scope" ),
		!sky->isChecked() && !clouds->isChecked() && !sun->isChecked() && !moon->isChecked()
		&& !wwLookdevSky() && !wwLookdevClouds() && !wwLookdevSun() && !wwLookdevMoon() );
	if ( !w->isVisible() ) {
		w->show();
		pump();
	}
	mode->setCurrentIndex( 0 );
	pump();
	check( st, QStringLiteral( "(rows) the preview rows are greyed outside Lookdev" ),
		!sky->isEnabled() && !clouds->isEnabled() && !sun->isEnabled() && !moon->isEnabled() && !day->isEnabled() );
	mode->setCurrentIndex( 2 );
	pump();
	check( st, QStringLiteral( "(rows) the preview rows are enabled in Lookdev" ),
		sky->isEnabled() && clouds->isEnabled() && sun->isEnabled() && moon->isEnabled() && day->isEnabled() );

	hour->setValue( 12.0 );
	pump();
	struct Step { QCheckBox * box; const char * name; bool ( *get )(); const char * echo; bool pixels; };
	const Step steps[] = {
		{ sky, "Sky", &wwLookdevSky, "sky:dome", true },
		{ sun, "Sun", &wwLookdevSun, "sun:", true },
		{ clouds, "Clouds", &wwLookdevClouds, "clouds:drawn", true },
	};
	for ( const Step & k : steps ) {
		const QImage before = freshGrab( skope );
		k.box->setChecked( true );
		const QImage after = freshGrab( skope );
		const int d = diffCount( before, after );
		const QString echo = wwLookdevSummary();
		say( st, QStringLiteral( "  %1 on: %2 px differ; status: %3" ).arg( QLatin1StringView( k.name ) ).arg( d ).arg( echo ) );
		check( st, QStringLiteral( "(live) the %1 row reached the state" ).arg( QLatin1StringView( k.name ) ), k.get() );
		check( st, QStringLiteral( "(live) %1 on changes the viewport (%2 px >= 500)" ).arg( QLatin1StringView( k.name ) ).arg( d ),
			d >= 500 );
		check( st, QStringLiteral( "(live) the drawn echo names the %1 pass" ).arg( QLatin1StringView( k.name ) ),
			echo.contains( QLatin1StringView( k.echo ) ) );
	}

	hour->setValue( 22.0 );
	day->setValue( 4.0 );
	pump();
	freshGrab( skope );
	moon->setChecked( true );
	const QImage m4 = freshGrab( skope );
	const QString e4 = status->text();
	check( st, QStringLiteral( "(live) the Moon row reached the state" ), wwLookdevMoon() );
	check( st, QStringLiteral( "(live) the drawn echo names the moon pass" ), e4.contains( QLatin1StringView( "moon:" ) ) );
	day->setValue( 32.0 );
	const QImage m32 = freshGrab( skope );
	const QString e32 = status->text();
	say( st, QStringLiteral( "  day 4 -> 32 at 22:00: %1 px differ; status: %2" ).arg( diffCount( m4, m32 ) ).arg( e32 ) );
	check( st, QStringLiteral( "(live) the Game Day row reached the state (%1)" ).arg( wwLookdevGameDay() ),
		std::fabs( wwLookdevGameDay() - 32.0 ) < 1e-6 );
	check( st, QStringLiteral( "(live) the Game Day row changes the Status line" ), e4 != e32 );

	for ( QCheckBox * b : { sky, sun, clouds, moon } )
		b->setChecked( false );
	day->setValue( 0.0 );
	hour->setValue( 12.0 );
	pump();
	check( st, QStringLiteral( "(live) every preview row back OFF reached the state" ),
		!wwLookdevSky() && !wwLookdevClouds() && !wwLookdevSun() && !wwLookdevMoon() );
}

void restartLeg( NifSkope * skope, WwScState & st )
{""")

rep("""	if ( qEnvironmentVariable( "WW_SCENE_TEST_LOOKDEV" ) == QLatin1StringView( "1" ) )
		lookdevLeg( skope, *st );
""", """	if ( qEnvironmentVariable( "WW_SCENE_TEST_LOOKDEV" ) == QLatin1StringView( "1" ) )
		lookdevLeg( skope, *st );
	if ( qEnvironmentVariable( "WW_SCENE_TEST_WEATHER" ) == QLatin1StringView( "1" ) )
		weatherLeg( skope, *st );
""")

assert s.count('\r') == cr0
open(P, 'wb').write(s.encode('utf-8'))
print('ok scenetest')
