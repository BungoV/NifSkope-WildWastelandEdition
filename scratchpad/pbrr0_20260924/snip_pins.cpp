/* THE R0 RENDER PINS (lane PBRR0, docs/NIFSKOPE_PBR_RENDERER.md s9).
 *
 * The old-vs-new shading harness (tests/spells/pbr_shade_ab.sh) pins every
 * switch a later PBR stage adds, identically in both arms, so no picture ever
 * depends on a QSettings value the user last ticked. In R0 they PIN STATE ONLY:
 * WW_PBRM_MODE and WW_PBRM_AUTOREPLACE override the cached menu values but still
 * pass through pbrmFeatureEnabled (false), so no pixel can move; the lighting,
 * lookdev, shadow, contact, AO and SSGI switches have no feature behind them yet
 * and are only echoed by WW_PBRM_CENSUS. Each is read once per process.
 * Return: -1 unset, 0/1 (or the mode), -2 set but unusable -- echoed as a
 * refusal in the census header, never silently treated as unset.
 */
static int wwEnvBool( const char * var )
{
	const QString s = qEnvironmentVariable( var ).trimmed().toLower();
	if ( s.isEmpty() )
		return -1;
	if ( s == QLatin1StringView( "0" ) || s == QLatin1StringView( "off" ) || s == QLatin1StringView( "false" ) )
		return 0;
	if ( s == QLatin1StringView( "1" ) || s == QLatin1StringView( "on" ) || s == QLatin1StringView( "true" ) )
		return 1;
	return -2;
}

static int wwEnvPbrmMode()
{
	static const int v = []() {
		const QString s = qEnvironmentVariable( "WW_PBRM_MODE" ).trimmed().toLower();
		if ( s.isEmpty() )
			return -1;
		if ( s == QLatin1StringView( "legacy" ) || s == QLatin1StringView( "0" ) )
			return int( PbrmModeLegacy );
		if ( s == QLatin1StringView( "pbr" ) || s == QLatin1StringView( "1" ) )
			return int( PbrmModePBR );
		if ( s == QLatin1StringView( "both" ) || s == QLatin1StringView( "legacyandpbr" ) || s == QLatin1StringView( "2" ) )
			return int( PbrmModeLegacyAndPBR );
		return -2;
	}();
	return v;
}

static int wwEnvAutoReplace()
{
	static const int v = wwEnvBool( "WW_PBRM_AUTOREPLACE" );
	return v;
}

bool wwRenderParticlesPin()
{
	// Unset (and unusable) keeps what the shot hook always did: particles ON.
	static const int v = wwEnvBool( "WW_RENDER_PARTICLES" );
	return v != 0;
}

