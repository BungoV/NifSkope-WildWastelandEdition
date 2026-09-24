P = 'E:/Projects/NifskopeWildWastelandEdition/src/esmweather.cpp'
H = 'E:/Projects/NifskopeWildWastelandEdition/src/esmweather.h'
s = open(P, 'rb').read().decode('utf-8')
h = open(H, 'rb').read().decode('utf-8')
cr0 = s.count('\r'); hr0 = h.count('\r')

def rep(old, new, which='s'):
    global s, h
    t = s if which == 's' else h
    assert t.count(old) == 1, (which, old[:70], t.count(old))
    t = t.replace(old, new)
    if which == 's': s = t
    else: h = t

# header: the cloud offset helper
rep(r"""//! elevation / azimuth of a sky vector, degrees""", r"""//! the cloud scroll offset: fract(speed * 0.1 * seconds), REAL seconds (Clouds::Update, 0.1 at 0x2c5442c@155)
float wwCloudOffset( float speed, double seconds );
//! elevation / azimuth of a sky vector, degrees""", 'h')

rep(r"""EsmWeather::EsmWeather() = default;""", r"""/* ------------------------------------------------------------------------
 * The engine clock (lane PBRWX1). See esmweather.h's header comment.
 * ---------------------------------------------------------------------- */

QString WwSkyGmst::describe() const
{
	auto one = [this]( const char * name, double v ) {
		const bool esm = fromEsm.contains( QString::fromLatin1( name ), Qt::CaseInsensitive );
		return QString( "%1=%2(%3)" ).arg( QString::fromLatin1( name ) ).arg( v, 0, 'g', 7 ).arg( esm ? "esm" : "exe" );
	};
	QStringList o;
	o << one( "fSunXExtreme", sunX ) << one( "fSunYExtreme", sunY ) << one( "fSunAlphaTransTime", alphaTrans )
	  << one( "fDaytimeColorExtension", colorExt ) << one( "fSunShadowScale", shadowScale )
	  << one( "fSunShadowMinAngle", shadowMin ) << one( "fSunBaseSize", sunBase ) << one( "fSunGlareSize", sunGlare )
	  << one( "fWeatherCloudSpeedMax", cloudSpeedMax ) << one( "iSecundaSize", secundaSize )
	  << one( "iMasserSize", masserSize ) << one( "fSecundaAngleFadeStart", secundaFadeStart )
	  << one( "fSecundaAngleFadeEnd", secundaFadeEnd ) << one( "fMasserAngleFadeStart", masserFadeStart )
	  << one( "fMasserAngleFadeEnd", masserFadeEnd );
	return o.join( QChar( ' ' ) );
}

WwSkyClock wwSkyClock( double hour, const unsigned char tnam[4], const WwSkyGmst & gIn )
{
	WwSkyGmst g = gIn;
	if ( wwLookdevRed( "sunfade2h" ) )
		g.alphaTrans = 2.0f;
	if ( wwLookdevRed( "colorext05" ) )
		g.colorExt = 0.5f;
	if ( wwLookdevRed( "hourstuck" ) )
		hour = 12.0;
	WwSkyClock c;
	c.hour = hour;
	const double t = hour;
	const double srMid = ( double( tnam[0] ) + double( tnam[1] ) ) / 12.0;
	const double ssMid = ( double( tnam[2] ) + double( tnam[3] ) ) / 12.0;
	const double hh = double( g.alphaTrans ) * 0.5;
	c.A = srMid - hh;
	c.B = srMid + hh;
	c.C = ssMid - hh;
	c.D = ssMid + hh;

	// the disc alpha (Sun::Update)
	if ( t < c.A || t > c.D )
		c.sunAlpha = 0.0f;
	else if ( t < c.B )
		c.sunAlpha = float( ( t - c.A ) / std::max( 1e-9, c.B - c.A ) );
	else if ( t <= c.C )
		c.sunAlpha = 1.0f;
	else
		c.sunAlpha = float( 1.0 - ( t - c.C ) / std::max( 1e-9, c.D - c.C ) );
	if ( wwLookdevRed( "sunalpha1" ) )
		c.sunAlpha = 1.0f;

	// the position, with the night branch
	double x;
	if ( ( t >= c.A && t <= c.D ) || wwLookdevRed( "nonightbranch" ) ) {
		x = 1.0 - 2.0 * ( t - c.A ) / std::max( 1e-9, c.D - c.A );
	} else {
		const double tt = t >= c.D ? t - c.D : 24.0 - c.D + t;
		x = 2.0 * tt / std::max( 1e-9, 24.0 - ( c.D - c.A ) ) - 1.0;
	}
	const float X = g.sunX;
	c.sunPos[0] = float( x * X );
	c.sunPos[1] = g.sunY;
	c.sunPos[2] = float( std::fabs( X ) - std::fabs( x * X ) );

	// the light: normalise, bias z, floor z, renormalise (Sun+0x38, TO the light here)
	float l = std::sqrt( c.sunPos[0] * c.sunPos[0] + c.sunPos[1] * c.sunPos[1] + c.sunPos[2] * c.sunPos[2] );
	if ( l < 1e-9f )
		l = 1.0f;
	const float nx = c.sunPos[0] / l, ny = c.sunPos[1] / l;
	float z = c.sunPos[2] / l + g.shadowScale * kEngineDegToRad;
	const float floorZ = g.shadowMin * kEngineDegToRad;
	if ( floorZ > z )
		z = floorZ;
	l = std::sqrt( nx * nx + ny * ny + z * z );
	c.lightDir[0] = nx / l;
	c.lightDir[1] = ny / l;
	c.lightDir[2] = z / l;

	// the stars alpha (Stars::Update)
	{
		const double ext = g.colorExt;
		const double rise1 = tnam[1] / 6.0, set0 = tnam[2] / 6.0;
		const double P = tnam[0] / 6.0 - ext, Q = tnam[3] / 6.0 + ext;
		const double sM = rise1 - ( rise1 - P ) / 2.0, uM = Q - ( Q - set0 ) / 2.0;
		double a;
		if ( t <= P || t >= Q )
			a = 1.0;
		else if ( t < sM )
			a = ( sM - t ) / std::max( 1e-9, sM - P );
		else if ( t <= uM )
			a = 0.0;
		else
			a = ( t - uM ) / std::max( 1e-9, Q - uM );
		c.starsAlpha = float( std::clamp( a, 0.0, 1.0 ) );
	}
	c.keys = wwTodKeys( hour, tnam, double( g.colorExt ) );
	return c;
}

float wwMoonAlpha( const WwSkyClock & c, const WwSkyGmst & gIn, float fadeStart, float fadeEnd )
{
	if ( wwLookdevRed( "moonalpha1" ) )
		return 1.0f;
	WwSkyGmst g = gIn;
	if ( wwLookdevRed( "sunfade2h" ) )
		g.alphaTrans = 2.0f;
	const double hh = double( g.alphaTrans ) * 0.5;
	const double t = c.hour;
	const double inA = c.D + hh * fadeStart, inB = c.D + hh * fadeEnd;
	const double outA = c.A - hh * fadeEnd, outB = c.A - hh * fadeStart;
	double a;
	if ( t >= inB || t <= outA )
		a = 1.0;
	else if ( t > inA && t < inB )
		a = ( t - inA ) / std::max( 1e-9, inB - inA );
	else if ( t > outA && t < outB )
		a = ( outB - t ) / std::max( 1e-9, outB - outA );
	else
		a = 0.0;
	return float( std::clamp( a, 0.0, 1.0 ) );
}

int wwMoonPhase( double gameDays, unsigned char moons )
{
	if ( wwLookdevRed( "phasestuck" ) )
		return 0;
	const int L = moons & 0x3F;
	if ( L <= 0 )
		return -1;
	const long long d = (long long) std::floor( std::max( 0.0, gameDays ) );
	return int( ( d % ( 8LL * L ) ) / L );
}

const char * wwMoonPhaseSuffix( int phase )
{
	static const char * const n[8] = { "full", "three_wan", "half_wan", "one_wan", "new", "one_wax", "half_wax",
		"three_wax" };
	return ( phase >= 0 && phase < 8 ) ? n[phase] : n[0];
}

float wwCloudOffset( float speed, double seconds )
{
	if ( wwLookdevRed( "cloudgametime" ) )
		seconds *= 20.0;	// the game's default timescale: what a game-time clock would do
	const double o = double( speed ) * 0.1 * seconds;
	return float( o - std::floor( o ) );
}

void wwSkyAngles( const float v[3], double * elevDeg, double * azimDeg )
{
	const double l = std::sqrt( double( v[0] ) * v[0] + double( v[1] ) * v[1] + double( v[2] ) * v[2] );
	const double r2d = 180.0 / 3.14159265358979323846;
	if ( elevDeg )
		*elevDeg = l > 1e-12 ? std::asin( std::clamp( double( v[2] ) / l, -1.0, 1.0 ) ) * r2d : 0.0;
	if ( azimDeg ) {
		double a = std::atan2( double( v[0] ), -double( v[1] ) ) * r2d;
		if ( a < 0.0 )
			a += 360.0;
		*azimDeg = a;
	}
}

void wwRgbToLab( const float rgb[3], float lab[3] )
{
	float c[3];
	for ( int i = 0; i < 3; i++ ) {
		const float v = rgb[i];
		c[i] = ( v > 0.04045f ? std::pow( ( v + 0.055f ) * 0.9478673f, 2.4f ) : v * 0.07739938f ) * 100.0f;
	}
	float X = c[0] * 0.4124f + c[1] * 0.3576f + c[2] * 0.1805f;
	float Y = c[0] * 0.2126f + c[1] * 0.7152f + c[2] * 0.0722f;
	float Z = c[0] * 0.0193f + c[1] * 0.1192f + c[2] * 0.9505f;
	X *= 0.010521111f;
	Y *= 0.01f;
	Z *= 0.0091841696f;
	auto f = []( float v ) { return v > 0.008856f ? std::cbrt( v ) : v * 7.787f + 0.13793103f; };
	const float fx = f( X ), fy = f( Y ), fz = f( Z );
	lab[0] = 116.0f * fy - 16.0f;
	lab[1] = 500.0f * ( fx - fy );
	lab[2] = 200.0f * ( fy - fz );
}

void wwLabToRgb( const float lab[3], float rgb[3] )
{
	const float fy = ( lab[0] + 16.0f ) * 0.00862069f;
	const float fx = lab[1] * 0.002f + fy;
	const float fz = fy - lab[2] * 0.005f;
	auto g = []( float v ) {
		const float v3 = v * v * v;
		return v3 > 0.008856f ? v3 : ( v - 0.13793103f ) * 0.12841916f;
	};
	const float X = g( fx ) * 95.047f * 0.01f;
	const float Y = g( fy ) * 100.0f * 0.01f;
	const float Z = g( fz ) * 108.883f * 0.01f;
	float c[3];
	c[0] = X * 3.2406f + Y * -1.5372f + Z * -0.4986f;
	c[1] = X * -0.9689f + Y * 1.8758f + Z * 0.0415f;
	c[2] = X * 0.0557f + Y * -0.2040f + Z * 1.0570f;
	for ( int i = 0; i < 3; i++ ) {
		const float v = c[i];
		const float e = v > 0.0031308f ? 1.055f * std::pow( v, 1.0f / 2.4f ) - 0.055f : v * 12.92f;
		rgb[i] = std::clamp( e, 0.0f, 1.0f );
	}
}

void wwLabBlend( const unsigned char a[3], const unsigned char b[3], float t, float rgb[3] )
{
	if ( wwLookdevRed( "rgbblend" ) ) {
		for ( int c = 0; c < 3; c++ )
			rgb[c] = ( float( a[c] ) * ( 1.0f - t ) + float( b[c] ) * t ) / 255.0f;
		return;
	}
	float ca[3], cb[3], la[3], lb[3], l[3];
	for ( int c = 0; c < 3; c++ ) {
		ca[c] = float( a[c] ) / 255.0f;
		cb[c] = float( b[c] ) / 255.0f;
	}
	wwRgbToLab( ca, la );
	wwRgbToLab( cb, lb );
	for ( int c = 0; c < 3; c++ )
		l[c] = la[c] * ( 1.0f - t ) + lb[c] * t;
	wwLabToRgb( l, rgb );
}

EsmWeather::EsmWeather() = default;""")

# read(): the new fields
rep(r"""	QVector<QByteArray> dalcs;
	try {
		ESMFile::ESMField f( *esm, *r );
		while ( f.next() ) {
			if ( f == "EDID" )
				out.edid = fieldString( f );
			else if ( f == "NAM0" )
				out.nam0 = QByteArray( reinterpret_cast<const char *>( f.data() ), qsizetype( f.size() ) );
			else if ( f == "DALC" )
				dalcs << QByteArray( reinterpret_cast<const char *>( f.data() ), qsizetype( f.size() ) );
		}""", r"""	QVector<QByteArray> dalcs;
	QByteArray pnam, jnam, qnam, rnam, onam, imsp;
	quint32 present = 0;
	for ( int i = 0; i < 32; i++ )
		out.cloudSpeedX[i] = out.cloudSpeedY[i] = 127;
	try {
		ESMFile::ESMField f( *esm, *r );
		auto bytes = [&f]() { return QByteArray( reinterpret_cast<const char *>( f.data() ), qsizetype( f.size() ) ); };
		while ( f.next() ) {
			if ( f == "EDID" )
				out.edid = fieldString( f );
			else if ( f == "NAM0" )
				out.nam0 = bytes();
			else if ( f == "DALC" )
				dalcs << bytes();
			else if ( f == "DATA" && f.size() >= 5 )
				out.sunGlare = quint8( f.data()[4] );
			else if ( f == "IMSP" )
				imsp = bytes();
			else if ( f == "PNAM" )
				pnam = bytes();
			else if ( f == "JNAM" )
				jnam = bytes();
			else if ( f == "QNAM" )
				qnam = bytes();
			else if ( f == "RNAM" )
				rnam = bytes();
			else if ( f == "ONAM" )
				onam = bytes();
			else if ( f == "LNAM" && f.size() >= 4 ) {
				std::memcpy( &out.cloudLayers, f.data(), 4 );
				out.hasLnam = true;
			} else if ( f == "NAM1" && f.size() >= 4 )
				std::memcpy( &out.cloudNam1, f.data(), 4 );
			else {
				// x0TX: chr(0x30 + i) + "0TX", i = 0..31
				for ( int i = 0; i < 32; i++ ) {
					const char sig[5] = { char( 0x30 + i ), '0', 'T', 'X', 0 };
					if ( f == sig ) {
						out.cloudTex[i] = fieldString( f );
						if ( !out.cloudTex[i].isEmpty() )
							present |= ( 1u << i );
						break;
					}
				}
			}
		}""")

rep(r"""			std::memcpy( &out.dalcFresnel[tod], p + 28, 4 );
	}
	if ( why )
		why->clear();
	return true;
}""", r"""			std::memcpy( &out.dalcFresnel[tod], p + 28, 4 );
	}

	// the clouds (spec_clouds.md s1): the engine ORs the missing-texture mask into NAM1 at load
	out.cloudDisabled = wwLookdevRed( "nam1ignore" ) ? ~present : ( out.cloudNam1 | ~present );
	{
		const int tods = out.formVersion >= 111 ? 8 : 4;
		const int layersP = pnam.size() / ( tods * 4 );
		const unsigned char * pp = reinterpret_cast<const unsigned char *>( pnam.constData() );
		const int layersJ = jnam.size() / ( tods * 4 );
		for ( int tod = 0; tod < 8; tod++ ) {
			const int st = storedSlot( tod, tods );
			for ( int layer = 0; layer < 32; layer++ ) {
				if ( layer < layersP )
					std::memcpy( out.cloudColor[tod][layer], pp + ( layer * tods + st ) * 4, 3 );
				if ( layer < layersJ )
					std::memcpy( &out.cloudAlpha[tod][layer], jnam.constData() + ( layer * tods + st ) * 4, 4 );
			}
		}
		if ( !qnam.isEmpty() || !rnam.isEmpty() ) {
			for ( int i = 0; i < 32; i++ ) {
				if ( i < qnam.size() )
					out.cloudSpeedX[i] = quint8( qnam.at( i ) );
				if ( i < rnam.size() )
					out.cloudSpeedY[i] = quint8( rnam.at( i ) );
			}
		} else {
			for ( int i = 0; i < 4 && i < onam.size(); i++ )
				out.cloudSpeedX[i] = quint8( quint8( onam.at( i ) ) / 2 + 127 );
		}
		if ( wwLookdevRed( "speedswap" ) )
			for ( int i = 0; i < 32; i++ )
				std::swap( out.cloudSpeedX[i], out.cloudSpeedY[i] );
	}

	// IMSP -> IMGS HNAM[7], the Sky Scale (spec_weather_sky.md s2.5)
	{
		const int n = imsp.size() / 4;
		for ( int i = 0; i < n && i < 8; i++ ) {
			quint32 raw = 0;
			std::memcpy( &raw, imsp.constData() + i * 4, 4 );
			out.imsp[i] = raw ? esm->mapFormID( *r, raw ) : 0;
		}
		if ( n > 0 && n < 8 )
			for ( int tod = 0; tod < 8; tod++ )
				out.imsp[tod] = out.imsp[storedSlot( tod, 4 )];
		for ( int tod = 0; tod < 8; tod++ ) {
			if ( !out.imsp[tod] )
				continue;
			const ESMFile::ESMRecord * ig = esm->findRecord( out.imsp[tod] );
			if ( !ig || !( *ig == "IMGS" ) )
				continue;
			try {
				ESMFile::ESMField g( *esm, *ig );
				while ( g.next() ) {
					if ( g == "HNAM" && g.size() >= 32 ) {
						std::memcpy( &out.skyScale[tod], g.data() + 28, 4 );
						out.skyScaleFound++;
						break;
					}
				}
			} catch ( std::exception & ) {
			}
		}
	}
	if ( why )
		why->clear();
	return true;
}

bool EsmWeather::climateData( quint32 formID, WwClimateData & out, QString * why )
{
	out = WwClimateData();
	if ( formID == 0 )
		formID = 0x0000015FU;	// DefaultClimate
	if ( !esm ) {
		if ( why )
			*why = QStringLiteral( "no plugin loaded" );
		return false;
	}
	const ESMFile::ESMRecord * r = esm->findRecord( formID );
	if ( !r || !( *r == "CLMT" ) ) {
		if ( why )
			*why = QString( "%1 is not a CLMT" ).arg( hex8( formID ) );
		return false;
	}
	bool got = false;
	ESMFile::ESMField f( *esm, *r );
	while ( f.next() ) {
		if ( f == "EDID" )
			out.edid = fieldString( f );
		else if ( f == "FNAM" )
			out.sunTex = fieldString( f );
		else if ( f == "GNAM" )
			out.glareTex = fieldString( f );
		else if ( f == "TNAM" && f.size() >= 4 ) {
			std::memcpy( out.tnam, f.data(), std::min<size_t>( 6, f.size() ) );
			got = true;
		}
	}
	out.ok = got;
	if ( !got && why )
		*why = QString( "%1 has no TNAM" ).arg( hex8( formID ) );
	return got;
}

WwSkyGmst EsmWeather::gmst()
{
	WwSkyGmst g;
	if ( !esm || wwLookdevRed( "exegmst" ) )
		return g;
	struct Slot { const char * name; float * v; bool isInt; };
	const Slot slots_[] = {
		{ "fSunXExtreme", &g.sunX, false }, { "fSunYExtreme", &g.sunY, false },
		{ "fSunAlphaTransTime", &g.alphaTrans, false }, { "fDaytimeColorExtension", &g.colorExt, false },
		{ "fSunShadowScale", &g.shadowScale, false }, { "fSunShadowMinAngle", &g.shadowMin, false },
		{ "fSunBaseSize", &g.sunBase, false }, { "fSunGlareSize", &g.sunGlare, false },
		{ "fWeatherCloudSpeedMax", &g.cloudSpeedMax, false }, { "iSecundaSize", &g.secundaSize, true },
		{ "iMasserSize", &g.masserSize, true }, { "fSecundaAngleFadeStart", &g.secundaFadeStart, false },
		{ "fSecundaAngleFadeEnd", &g.secundaFadeEnd, false }, { "fMasserAngleFadeStart", &g.masserFadeStart, false },
		{ "fMasserAngleFadeEnd", &g.masserFadeEnd, false },
	};
	const ESMFile::ESMRecord * r0 = esm->findRecord( 0U );
	if ( !r0 )
		return g;
	for ( unsigned int gi = r0->next; gi; ) {
		const ESMFile::ESMRecord * grp = esm->findRecord( gi );
		if ( !grp )
			break;
		if ( grp->type == GRUP && FileBuffer::checkType( grp->flags, "GMST" ) ) {
			for ( unsigned int id = grp->children; id; ) {
				const ESMFile::ESMRecord * c = esm->findRecord( id );
				if ( !c )
					break;
				if ( c->type != GRUP && *c == "GMST" ) {
					// the winning version of this FormID (the last plugin that sets it)
					const ESMFile::ESMRecord * w = esm->findRecord( c->formID );
					if ( w ) {
						QString edid;
						QByteArray data;
						try {
							ESMFile::ESMField f( *esm, *w );
							while ( f.next() ) {
								if ( f == "EDID" )
									edid = fieldString( f );
								else if ( f == "DATA" )
									data = QByteArray( reinterpret_cast<const char *>( f.data() ), qsizetype( f.size() ) );
							}
						} catch ( std::exception & ) {
						}
						for ( const Slot & s : slots_ ) {
							if ( data.size() < 4 || edid.compare( QLatin1String( s.name ), Qt::CaseInsensitive ) != 0 )
								continue;
							if ( s.isInt ) {
								qint32 iv = 0;
								std::memcpy( &iv, data.constData(), 4 );
								*s.v = float( iv );
							} else {
								std::memcpy( s.v, data.constData(), 4 );
							}
							if ( !g.fromEsm.contains( QString::fromLatin1( s.name ) ) )
								g.fromEsm << QString::fromLatin1( s.name );
						}
					}
				}
				id = c->next;
			}
		}
		gi = grp->next;
	}
	return g;
}""")

rep(r"""void EsmWeather::blendDalc( const WwWeatherData & w, int axis, const WwTodKeys & k, float rgb[3] )
{
	for ( int c = 0; c < 3; c++ )
		rgb[c] = float( w.dalc[k.a][axis][c] ) * ( 1.0f - k.t ) + float( w.dalc[k.b][axis][c] ) * k.t;
}""", r"""void EsmWeather::blendDalc( const WwWeatherData & w, int axis, const WwTodKeys & k, float rgb[3] )
{
	for ( int c = 0; c < 3; c++ )
		rgb[c] = float( w.dalc[k.a][axis][c] ) * ( 1.0f - k.t ) + float( w.dalc[k.b][axis][c] ) * k.t;
}

void EsmWeather::blendRowLab( const WwWeatherData & w, int row, const WwTodKeys & k, float rgb[3] )
{
	wwLabBlend( w.color[k.a][row], w.color[k.b][row], k.t, rgb );
	for ( int c = 0; c < 3; c++ )
		rgb[c] *= 255.0f;
}

void EsmWeather::blendCloud( const WwWeatherData & w, int layer, const WwTodKeys & k, float rgb[3], float * alpha )
{
	const int l = ( layer < 0 || quint32( layer ) >= w.cloudLayers || layer >= 32 ) ? 0 : layer;
	wwLabBlend( w.cloudColor[k.a][l], w.cloudColor[k.b][l], k.t, rgb );
	for ( int c = 0; c < 3; c++ )
		rgb[c] *= 255.0f;
	if ( alpha ) {
		*alpha = w.cloudAlpha[k.a][l] * ( 1.0f - k.t ) + w.cloudAlpha[k.b][l] * k.t;
		if ( wwLookdevRed( "cloudalphaone" ) )
			*alpha = 1.0f;
	}
}

float EsmWeather::blendSkyScale( const WwWeatherData & w, const WwTodKeys & k )
{
	return w.skyScale[k.a] * ( 1.0f - k.t ) + w.skyScale[k.b] * k.t;
}

float EsmWeather::cloudSpeed( quint8 b, const WwSkyGmst & g )
{
	return g.cloudSpeedMax * ( 2.0f * float( b ) / 254.0f - 1.0f );
}""")

open(P, 'wb').write(s.encode('utf-8'))
open(H, 'wb').write(h.encode('utf-8'))
assert s.count('\r') == cr0 and h.count('\r') == hr0
print('ok cpp part 1')
