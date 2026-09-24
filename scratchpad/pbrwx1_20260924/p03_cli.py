P = 'E:/Projects/NifskopeWildWastelandEdition/src/esmweather.cpp'
s = open(P, 'rb').read().decode('utf-8')
cr0 = s.count('\r')

def rep(old, new):
    global s
    assert s.count(old) == 1, (old[:70], s.count(old))
    s = s.replace(old, new)

rep(r""" *   --census          every WTHR: parse verdict + the NAM0/DALC histograms
 *   --list            every WTHR, one line each
""", r""" *   --census          every WTHR: parse verdict + the NAM0/DALC histograms
 *   --list            every WTHR, one line each
 *   --sky             (with --weather + --hour) the engine clock (lane PBRWX1): `gmst`, `climate2`, and per
 *                     hour `skyclock`, `skycolor`, `cloud` (layers 0..15) and `moon` (per --day)
 *   --day d[,d..]     game days for the `moon` lines (default 0)
 *   --cloudtime s     real seconds for the `cloud` offsets (default 0)
""")

rep(r"""	QString plugins, plugin, dataDir, weather, hours, tnamArg, climateArg;
	bool census = false, doList = false;""", r"""	QString plugins, plugin, dataDir, weather, hours, tnamArg, climateArg, daysArg;
	bool census = false, doList = false, sky = false;
	double cloudTime = 0.0;""")

rep(r"""		else if ( a == "--list" )
			doList = true;""", r"""		else if ( a == "--list" )
			doList = true;
		else if ( a == "--sky" )
			sky = true;
		else if ( a == "--day" )
			daysArg = next();
		else if ( a == "--cloudtime" )
			cloudTime = next().toDouble();""")

rep(r"""				.arg( double( zm[0] ), 0, 'f', 2 ).arg( double( zm[1] ), 0, 'f', 2 ).arg( double( zm[2] ), 0, 'f', 2 ) );
		}
	} else if""", r"""				.arg( double( zm[0] ), 0, 'f', 2 ).arg( double( zm[1] ), 0, 'f', 2 ).arg( double( zm[2] ), 0, 'f', 2 ) );
		}
		if ( sky ) {
			const WwSkyGmst g = ew.gmst();
			say( QString( "gmst %1" ).arg( g.describe() ) );
			WwClimateData cd;
			{
				quint32 cid = 0;
				if ( !climateArg.isEmpty() )
					cid = climateArg.startsWith( "0x", Qt::CaseInsensitive ) ? climateArg.mid( 2 ).toUInt( nullptr, 16 )
					                                                         : climateArg.toUInt( nullptr, 16 );
				QString why;
				if ( !ew.climateData( cid, cd, &why ) )
					say( QString( "climate2 fallback reason=\"%1\"" ).arg( why ) );
			}
			say( QString( "climate2 edid=%1 tnam=%2,%3,%4,%5,%6,%7 moons=0x%8 sunTex=%9 glareTex=%10" ).arg( cd.edid )
				.arg( cd.tnam[0] ).arg( cd.tnam[1] ).arg( cd.tnam[2] ).arg( cd.tnam[3] ).arg( cd.tnam[4] ).arg( cd.tnam[5] )
				.arg( cd.tnam[5], 2, 16, QChar( '0' ) ).arg( cd.sunTex, cd.glareTex ) );
			say( QString( "sky imsp=%1 skyscale=%2 found=%3 sunglare=%4 lnam=%5 nam1=0x%6 disabled=0x%7" )
				.arg( [&]() { QStringList l; for ( int i = 0; i < 8; i++ ) l << hex8( w.imsp[i] ); return l.join( ',' ); }() )
				.arg( [&]() { QStringList l; for ( int i = 0; i < 8; i++ ) l << QString::number( double( w.skyScale[i] ), 'g', 7 ); return l.join( ',' ); }() )
				.arg( w.skyScaleFound ).arg( w.sunGlare ).arg( w.cloudLayers )
				.arg( hex8( w.cloudNam1 ), hex8( w.cloudDisabled ) ) );
			QVector<double> days;
			for ( const QString & d : daysArg.split( QChar( ',' ), Qt::SkipEmptyParts ) )
				days << d.toDouble();
			if ( days.isEmpty() )
				days << 0.0;
			auto c3 = []( const float v[3] ) {
				return QString( "%1,%2,%3" ).arg( double( v[0] ), 0, 'f', 3 ).arg( double( v[1] ), 0, 'f', 3 ).arg( double( v[2] ), 0, 'f', 3 );
			};
			for ( double hr : hourList() ) {
				const WwSkyClock c = wwSkyClock( hr, cd.tnam, g );
				double el, az;
				wwSkyAngles( c.sunPos, &el, &az );
				say( QString( "skyclock hour=%1 keys=%2,%3,%4 A=%5 B=%6 C=%7 D=%8 sunpos=%9 light=%10 sunalpha=%11 starsalpha=%12 elev=%13 az=%14" )
					.arg( hr, 0, 'f', 4 ).arg( wwTodName( c.keys.a ), wwTodName( c.keys.b ) ).arg( double( c.keys.t ), 0, 'f', 6 )
					.arg( c.A, 0, 'f', 6 ).arg( c.B, 0, 'f', 6 ).arg( c.C, 0, 'f', 6 ).arg( c.D, 0, 'f', 6 )
					.arg( c3( c.sunPos ) )
					.arg( QString( "%1,%2,%3" ).arg( double( c.lightDir[0] ), 0, 'f', 6 ).arg( double( c.lightDir[1] ), 0, 'f', 6 ).arg( double( c.lightDir[2] ), 0, 'f', 6 ) )
					.arg( double( c.sunAlpha ), 0, 'f', 6 ).arg( double( c.starsAlpha ), 0, 'f', 6 )
					.arg( el, 0, 'f', 4 ).arg( az, 0, 'f', 4 ) );
				float up[3], lo[3], hz[3], su[3], gl[3], mg[3], st[3], sl[3], am[3];
				EsmWeather::blendRowLab( w, WwRowSkyUpper, c.keys, up );
				EsmWeather::blendRowLab( w, WwRowSkyLower, c.keys, lo );
				EsmWeather::blendRowLab( w, WwRowHorizon, c.keys, hz );
				EsmWeather::blendRowLab( w, WwRowSun, c.keys, su );
				EsmWeather::blendRowLab( w, WwRowSunGlare, c.keys, gl );
				EsmWeather::blendRowLab( w, WwRowMoonGlare, c.keys, mg );
				EsmWeather::blendRowLab( w, WwRowStars, c.keys, st );
				EsmWeather::blendRowLab( w, WwRowSunlight, c.keys, sl );
				EsmWeather::blendRowLab( w, WwRowAmbient, c.keys, am );
				say( QString( "skycolor hour=%1 upper=%2 lower=%3 horizon=%4 sun=%5 glare=%6 moonglare=%7 stars=%8 sunlight=%9 ambient=%10 skyscale=%11 glarealpha=%12" )
					.arg( hr, 0, 'f', 4 ).arg( c3( up ), c3( lo ), c3( hz ), c3( su ), c3( gl ), c3( mg ), c3( st ), c3( sl ) ).arg( c3( am ) )
					.arg( double( EsmWeather::blendSkyScale( w, c.keys ) ), 0, 'f', 6 )
					.arg( double( c.sunAlpha * float( w.sunGlare ) / 255.0f ), 0, 'f', 6 ) );
				for ( int i = 0; i < 16; i++ ) {
					float rgb[3], al = 0.0f;
					EsmWeather::blendCloud( w, i, c.keys, rgb, &al );
					const quint32 li = quint32( i ) >= w.cloudLayers ? 0u : quint32( i );
					const float sx = EsmWeather::cloudSpeed( w.cloudSpeedX[li], g );
					const float sy = EsmWeather::cloudSpeed( w.cloudSpeedY[li], g );
					const bool drawn = !( w.cloudDisabled & ( 1u << i ) );
					say( QString( "cloud hour=%1 layer=%2 tex=%3 drawn=%4 rgb=%5 alpha=%6 speedx=%7 speedy=%8 offx=%9 offy=%10" )
						.arg( hr, 0, 'f', 4 ).arg( i ).arg( w.cloudTex[i].isEmpty() ? QString( "-" ) : QString( w.cloudTex[i] ).replace( QChar( ' ' ), QChar( '?' ) ) )
						.arg( drawn ? 1 : 0 ).arg( c3( rgb ) ).arg( double( al ), 0, 'f', 6 )
						.arg( double( sx ), 0, 'f', 8 ).arg( double( sy ), 0, 'f', 8 )
						.arg( double( wwCloudOffset( sx, cloudTime ) ), 0, 'f', 6 ).arg( double( wwCloudOffset( sy, cloudTime ) ), 0, 'f', 6 ) );
				}
				const float ma[2] = { wwMoonAlpha( c, g, g.secundaFadeStart, g.secundaFadeEnd ),
					wwMoonAlpha( c, g, g.masserFadeStart, g.masserFadeEnd ) };
				for ( double d : days ) {
					const int ph = wwMoonPhase( d, cd.tnam[5] );
					for ( int m = 0; m < 2; m++ ) {
						const bool on = m == 0 ? ( cd.tnam[5] & 0x40 ) : ( cd.tnam[5] & 0x80 );
						if ( !on )
							continue;
						const float size = m == 0 ? g.secundaSize : g.masserSize;
						const double half = std::atan( double( size ) / std::max( 1e-6, std::fabs( double( g.sunX ) ) ) ) * 180.0 / 3.14159265358979323846;
						say( QString( "moon hour=%1 day=%2 which=%3 phase=%4 suffix=%5 tex=textures/sky/%3_%5.dds alpha=%6 shadowalpha=%7 halfangle=%8 elev=%9 az=%10" )
							.arg( hr, 0, 'f', 4 ).arg( d, 0, 'f', 3 ).arg( m == 0 ? "secunda" : "masser" ).arg( ph )
							.arg( wwMoonPhaseSuffix( ph ) ).arg( double( ma[m] ), 0, 'f', 6 )
							.arg( double( std::min( ma[m], c.starsAlpha ) ), 0, 'f', 6 ).arg( half, 0, 'f', 4 )
							.arg( el, 0, 'f', 4 ).arg( az, 0, 'f', 4 ) );
					}
				}
			}
		}
	} else if""")

assert s.count('\r') == cr0
open(P, 'wb').write(s.encode('utf-8'))
print('ok cli')
