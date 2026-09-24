
/* WW_PBRM_CENSUS=<ABSOLUTE path> (lane PBRR0, docs/NIFSKOPE_PBR_RENDERER.md s9).
 *
 * One line per shape, written the first time the shape is drawn: which route
 * SERVED it (legacy / direct / sibling; nifx, swap and fo76 join in R1), the
 * file serving it, the .pbrm envelope version if one resolved, and why PBR did
 * not serve it. It echoes RESOLVED state -- the program the renderer actually
 * bound and the fields resolvePbrm() actually wrote -- never the intent. The
 * header line echoes every harness pin as asked AND as served.
 *
 * In R0 every shape reads route=legacy: PBR display is off by the constant
 * pbrmFeatureEnabled above, and the refusal says so. Detection is unchanged.
 * Armed only by an absolute path; renders nothing and touches no uniform.
 */
static int wwPbrmCensusArmedState = -1;
static QString wwPbrmCensusPath;

bool wwPbrmCensusArmed()
{
	if ( wwPbrmCensusArmedState < 0 ) {
		const QString p = qEnvironmentVariable( "WW_PBRM_CENSUS" ).trimmed();
		wwPbrmCensusArmedState = ( !p.isEmpty() && QDir::isAbsolutePath( p ) ) ? 1 : 0;
		wwPbrmCensusPath = p;
	}
	return wwPbrmCensusArmedState == 1;
}

static QString wwPinAsked( const char * var )
{
	return qEnvironmentVariableIsSet( var ) ? qEnvironmentVariable( var ) : QStringLiteral( "unset" );
}

static QString wwPinEcho( const char * key, const char * var, const QString & served, int parsed,
	const char * note )
{
	QString s = QStringLiteral( " %1=%2(asked=%3" ).arg( QLatin1StringView( key ), served, wwPinAsked( var ) );
	if ( parsed == -2 )
		s += QStringLiteral( " REFUSED: unusable value" );
	if ( note && *note )
		s += QStringLiteral( "; " ) + QLatin1StringView( note );
	return s + QLatin1Char( ')' );
}

static QString wwPbrmCensusHeader()
{
	static const char * const modeNames[] = { "legacy", "pbr", "both" };
	const int mode = pbrmMode();
	QString h = QStringLiteral( "# WW_PBRM_CENSUS stage=R0 pbr=%1" )
		.arg( pbrmFeatureEnabled ? QStringLiteral( "on" )
			: QStringLiteral( "off(pbrmFeatureEnabled=false src/gl/glproperty.cpp)" ) );
	h += wwPinEcho( "mode", "WW_PBRM_MODE",
		QLatin1StringView( modeNames[( mode >= 0 && mode <= 2 ) ? mode : 0] ), wwEnvPbrmMode(), "" );
	h += wwPinEcho( "autoreplace", "WW_PBRM_AUTOREPLACE",
		pbrmAutoReplaceEnabled() ? QStringLiteral( "on" ) : QStringLiteral( "off" ), wwEnvAutoReplace(), "" );
	h += wwPinEcho( "lighting", "WW_LIGHTING_MODE", QStringLiteral( "legacy" ), -1,
		"no lighting modes before R2a" );
	QStringList lookdev;
	for ( const QString & k : QProcessEnvironment::systemEnvironment().keys() )
		if ( k.startsWith( QLatin1StringView( "WW_LOOKDEV" ) ) )
			lookdev << k + QLatin1Char( '=' ) + qEnvironmentVariable( k.toLatin1().constData() );
	lookdev.sort();
	h += QStringLiteral( " lookdev=off(asked=%1; no lookdev stage before R2b)" )
		.arg( lookdev.isEmpty() ? QStringLiteral( "unset" ) : lookdev.join( QLatin1Char( ',' ) ) );
	h += wwPinEcho( "particles", "WW_RENDER_PARTICLES",
		wwRenderParticlesPin() ? QStringLiteral( "on" ) : QStringLiteral( "off" ),
		wwEnvBool( "WW_RENDER_PARTICLES" ), "" );
	h += wwPinEcho( "shadows", "WW_RENDER_SHADOWS", QStringLiteral( "off" ), -1, "not built" );
	h += wwPinEcho( "contact", "WW_RENDER_CONTACT", QStringLiteral( "off" ), -1, "not built" );
	h += wwPinEcho( "ao", "WW_RENDER_AO", QStringLiteral( "off" ), -1, "not built" );
	h += wwPinEcho( "ssgi", "WW_RENDER_SSGI", QStringLiteral( "off" ), -1, "not built" );
	return h;
}

QString BSShaderLightingProperty::wwPbrmCensusFields( const QString & servedProgram ) const
{
	const bool	direct = name.endsWith( QLatin1StringView( ".pbrm" ), Qt::CaseInsensitive );
	const bool	bgsx = name.endsWith( QLatin1StringView( ".bgsm" ), Qt::CaseInsensitive )
		|| name.endsWith( QLatin1StringView( ".bgem" ), Qt::CaseInsensitive );
	const QString	envelope = ( pbrmValid || pbrmUnsupported )
		? QStringLiteral( "v%1" ).arg( pbrm.envelopeVersion ) : QStringLiteral( "none" );

	QString	route, path, refusal;
	if ( servedProgram == QLatin1StringView( "pbrm_default.prog" ) ) {
		route = direct ? QStringLiteral( "direct" ) : QStringLiteral( "sibling" );
		path = pbrmPath;
		refusal = QStringLiteral( "none" );
	} else {
		route = QStringLiteral( "legacy" );
		if ( name.isEmpty() )
			path = QStringLiteral( "(embedded)" );
		else if ( material )
			path = name;
		else
			path = QStringLiteral( "(embedded; material not loaded)" );

		// Why PBR did not serve it: the first gate that said no ...
		if ( !pbrmFeatureEnabled )
			refusal = QStringLiteral( "pbr display off: pbrmFeatureEnabled=false (R0, detection unchanged)" );
		else if ( wwLodChannelView != 0 )
			refusal = QStringLiteral( "lod channel preview is a data view" );
		else if ( pbrmMode() == PbrmModeLegacy )
			refusal = QStringLiteral( "mode legacy" );
		else if ( pbrmMode() == PbrmModeLegacyAndPBR && !pbrmValid )
			refusal = QStringLiteral( "no pbrm resolved" );
		else
			refusal = QStringLiteral( "pbr program refused the shape" );

		// ... and what the resolver found, which R0 leaves exactly as it was.
		if ( pbrmValid )
			refusal += QStringLiteral( "; resolved %1 not served" ).arg( pbrmPath );
		else if ( pbrmUnsupported )
			refusal += QStringLiteral( "; %1 unsupported" ).arg( pbrmPath );
		else if ( direct )
			refusal += QStringLiteral( "; direct .pbrm not found or malformed" );
		else if ( bgsx && !pbrmAutoReplaceEnabled() )
			refusal += QStringLiteral( "; sibling lookup off (auto-replace gated)" );
		else if ( bgsx )
			refusal += QStringLiteral( "; no sibling .pbrm" );
		else if ( name.isEmpty() )
			refusal += QStringLiteral( "; embedded shader, no material name" );
		else
			refusal += QStringLiteral( "; no .pbrm candidate" );
	}

	return QStringLiteral( "material=\"%1\" route=%2 path=\"%3\" envelope=%4 refusal=\"%5\"" )
		.arg( name, route, path, envelope, refusal );
}

void wwPbrmCensus( const QString & shapeName, const char * kind, const BSShaderLightingProperty * sp,
	const QString & servedProgram )
{
	if ( !wwPbrmCensusArmed() )
		return;

	QString	row = QStringLiteral( "shape=\"%1\" kind=%2 " ).arg( shapeName, QLatin1StringView( kind ) );
	if ( sp )
		row += sp->wwPbrmCensusFields( servedProgram );
	else
		row += QStringLiteral( "material=\"\" route=legacy path=\"(none)\" envelope=none "
			"refusal=\"no shader property\"" );

	static QSet<QString>	seen;
	if ( seen.contains( row ) )
		return;
	const bool	first = seen.isEmpty();
	seen.insert( row );

	QFile	f( wwPbrmCensusPath );
	if ( f.open( QIODevice::Append | QIODevice::Text ) ) {
		QTextStream	s( &f );
		if ( first )
			s << wwPbrmCensusHeader() << "\n";
		s << row << "\n";
	}
}
