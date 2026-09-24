P = 'E:/Projects/NifskopeWildWastelandEdition/src/scenetest.cpp'
s = open(P, 'rb').read().decode('utf-8')
cr0 = s.count('\r')
def rep(old, new):
    global s
    assert s.count(old) == 1, (old[:60], s.count(old))
    s = s.replace(old, new)
rep('''	struct Step { QCheckBox * box; const char * name; bool ( *get )(); const char * echo; bool pixels; };
	const Step steps[] = {
		{ sky, "Sky", &wwLookdevSky, "sky:dome", true },
		{ sun, "Sun", &wwLookdevSun, "sun:", true },
		{ clouds, "Clouds", &wwLookdevClouds, "clouds:drawn", true },
	};''', '''	struct Step { QCheckBox * box; const char * name; bool ( *get )(); const char * echo; const char * key; };
	const Step steps[] = {
		{ sky, "Sky", &wwLookdevSky, "sky:dome", "Settings/Render/Scene/Lookdev Sky" },
		{ sun, "Sun", &wwLookdevSun, "sun:", "Settings/Render/Scene/Lookdev Sun" },
		{ clouds, "Clouds", &wwLookdevClouds, "clouds:drawn", "Settings/Render/Scene/Lookdev Clouds" },
	};''')
rep('''		check( st, QStringLiteral( "(live) the drawn echo names the %1 pass" ).arg( QLatin1StringView( k.name ) ),
			echo.contains( QLatin1StringView( k.echo ) ) );
	}
''', '''		check( st, QStringLiteral( "(live) the drawn echo names the %1 pass" ).arg( QLatin1StringView( k.name ) ),
			echo.contains( QLatin1StringView( k.echo ) ) );
		check( st, QStringLiteral( "(save) the %1 row wrote ON to its setting" ).arg( QLatin1StringView( k.name ) ),
			QSettings().value( QLatin1StringView( k.key ) ).toBool() );
	}
''')
rep('''	check( st, QStringLiteral( "(live) the Game Day row changes the Status line" ), e4 != e32 );
''', '''	check( st, QStringLiteral( "(live) the Game Day row changes the Status line" ), e4 != e32 );
	check( st, QStringLiteral( "(save) the Moon row and Game Day 32 wrote their settings" ),
		QSettings().value( QLatin1StringView( "Settings/Render/Scene/Lookdev Moon" ) ).toBool()
		&& std::fabs( QSettings().value( QLatin1StringView( "Settings/Render/Scene/Lookdev Game Day" ) ).toDouble() - 32.0 ) < 1e-6 );
''')
rep('''         Red: WW_R2A_RED=nolive (the rows never reach the state): FAILS.
     WW_SCENE_TEST_LOG=<path>''', '''         Each row also writes its setting (the load half is the driver's: a
         pre-seeded scope, then a shot, pbr_wx1_gates.sh "persist").
         Red: WW_R2A_RED=nolive (the rows never reach the state): FAILS.
         Red: WW_R2A_RED=nosave (nothing is written): the (save) checks FAIL.
     WW_SCENE_TEST_LOG=<path>''')
assert s.count('\r') == cr0
open(P, 'wb').write(s.encode('utf-8'))
print('ok p07')
