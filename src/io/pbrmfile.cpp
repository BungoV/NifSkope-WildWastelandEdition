#include "io/pbrmfile.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>


//! 64 MiB, per the spec.
static constexpr qint64 PBRM_MAX_PAYLOAD = 64 * 1024 * 1024;

//! Capabilities this build provides, as semantic name -> version. A requirement
//! naming anything else, or a higher version, fails closed.
static bool pbrmProvides( const QString & name, int version )
{
	if ( name == QLatin1String( "standard.primaryUv" ) )
		return version <= 1;
	return false;
}


bool pbrmNormalisePath( const QString & authored, QString & out )
{
	out.clear();
	QString p = authored.trimmed();
	if ( p.isEmpty() )
		return false;			// "no texture", not an error

	p.replace( QLatin1Char( '/' ), QLatin1Char( '\\' ) );

	// Reject before collapsing, so a rejected shape cannot be normalised into an
	// accepted one: drive-qualified (C:\), UNC (\\host), root-qualified (\x) and
	// any parent traversal.
	if ( p.size() >= 2 && p.at( 1 ) == QLatin1Char( ':' ) )
		return false;
	if ( p.startsWith( QLatin1String( "\\\\" ) ) )
		return false;
	if ( p.startsWith( QLatin1Char( '\\' ) ) )
		return false;
	for ( const QString & seg : p.split( QLatin1Char( '\\' ) ) ) {
		if ( seg == QLatin1String( ".." ) )
			return false;
	}

	while ( p.startsWith( QLatin1String( ".\\" ) ) )
		p.remove( 0, 2 );
	while ( p.contains( QLatin1String( "\\\\" ) ) )
		p.replace( QLatin1String( "\\\\" ), QLatin1String( "\\" ) );

	p = p.toLower();

	// A leading "textures\" is optional so both authoring conventions resolve to
	// the same key.
	if ( p.startsWith( QLatin1String( "textures\\" ) ) )
		p.remove( 0, 9 );

	if ( p.isEmpty() )
		return false;

	out = p;
	return true;
}


static float pbrmNumber( const QJsonObject & values, const char * key, float dflt )
{
	const QJsonValue v = values.value( QLatin1String( key ) );
	return v.isDouble() ? float( v.toDouble() ) : dflt;
}

static bool pbrmBool( const QJsonObject & values, const char * key, bool dflt )
{
	const QJsonValue v = values.value( QLatin1String( key ) );
	return v.isBool() ? v.toBool() : dflt;
}

//! "#rrggbb" -> linear-ish float triple (kept as authored sRGB values).
static void pbrmColor( const QJsonObject & values, const char * key, float * rgb )
{
	const QJsonValue v = values.value( QLatin1String( key ) );
	if ( !v.isString() )
		return;
	QString s = v.toString().trimmed();
	if ( s.startsWith( QLatin1Char( '#' ) ) )
		s.remove( 0, 1 );
	if ( s.size() < 6 )
		return;
	bool okR = false, okG = false, okB = false;
	int r = s.mid( 0, 2 ).toInt( &okR, 16 );
	int g = s.mid( 2, 2 ).toInt( &okG, 16 );
	int b = s.mid( 4, 2 ).toInt( &okB, 16 );
	if ( okR && okG && okB ) {
		rgb[0] = float( r ) / 255.0f;
		rgb[1] = float( g ) / 255.0f;
		rgb[2] = float( b ) / 255.0f;
	}
}

//! Resolve one texture slot: enabled + a valid path is what makes it samplable.
static PbrmMaterial::Slot pbrmSlot( const QJsonObject & uv, const char * socket,
                                    QJsonObject & valuesOut, QStringList & diags )
{
	PbrmMaterial::Slot slot;
	valuesOut = QJsonObject();

	const QJsonValue sv = uv.value( QLatin1String( socket ) );
	if ( !sv.isObject() )
		return slot;
	const QJsonObject so = sv.toObject();

	slot.enabled = so.value( QStringLiteral( "enabled" ) ).toBool( false );
	slot.path = so.value( QStringLiteral( "path" ) ).toString();
	valuesOut = so.value( QStringLiteral( "values" ) ).toObject();

	if ( !slot.path.trimmed().isEmpty() ) {
		slot.pathValid = pbrmNormalisePath( slot.path, slot.lookupPath );
		if ( !slot.pathValid ) {
			// Per the spec this disables the slot and reports a diagnostic; it
			// does NOT make the envelope unparsable.
			diags.append( QStringLiteral( "%1: unusable texture path \"%2\"" )
				.arg( QLatin1String( socket ), slot.path ) );
		}
	}
	return slot;
}


PbrmMaterial pbrmParse( const QByteArray & bytes )
{
	PbrmMaterial m;

	if ( bytes.size() < 12 ) {
		m.error = QStringLiteral( "too short to hold a PBRM envelope" );
		return m;
	}
	if ( !bytes.startsWith( QByteArray( "PBRM", 4 ) ) ) {
		m.error = QStringLiteral( "bad magic (expected PBRM)" );
		return m;
	}

	const uchar * u = reinterpret_cast<const uchar *>( bytes.constData() );
	auto le32 = [u]( int off ) -> quint32 {
		return quint32( u[off] ) | ( quint32( u[off + 1] ) << 8 )
			| ( quint32( u[off + 2] ) << 16 ) | ( quint32( u[off + 3] ) << 24 );
	};

	m.envelopeVersion = int( le32( 4 ) );
	if ( m.envelopeVersion != 5 && m.envelopeVersion != 4 ) {
		m.error = QStringLiteral( "unsupported envelope version %1" ).arg( m.envelopeVersion );
		return m;
	}

	const qint64 declared = qint64( le32( 8 ) );
	if ( declared > PBRM_MAX_PAYLOAD ) {
		m.error = QStringLiteral( "payload %1 exceeds the 64 MiB cap" ).arg( declared );
		return m;
	}
	// Must consume the rest EXACTLY — truncation and trailing bytes are errors.
	if ( declared != bytes.size() - 12 ) {
		m.error = QStringLiteral( "payload size %1 does not match the %2 bytes present" )
			.arg( declared ).arg( bytes.size() - 12 );
		return m;
	}

	QJsonParseError perr {};
	const QJsonDocument doc = QJsonDocument::fromJson( bytes.mid( 12 ), &perr );
	if ( perr.error != QJsonParseError::NoError || !doc.isObject() ) {
		m.error = QStringLiteral( "malformed JSON payload: %1" ).arg( perr.errorString() );
		return m;
	}
	const QJsonObject root = doc.object();

	if ( root.value( QStringLiteral( "schema" ) ).toString() != QLatin1String( "FO4.PBRM.Material" ) ) {
		m.error = QStringLiteral( "unknown schema" );
		return m;
	}
	const QJsonValue sver = root.value( QStringLiteral( "schemaVersion" ) );
	if ( !sver.isDouble() || ( sver.toInt() != 5 && sver.toInt() != 4 ) ) {
		m.error = QStringLiteral( "unsupported schemaVersion" );
		return m;
	}
	m.shader = root.value( QStringLiteral( "shader" ) ).toString();
	if ( m.shader.trimmed().isEmpty() ) {
		m.error = QStringLiteral( "empty shader name" );
		return m;
	}

	// requirements: any capability we do not provide fails CLOSED. Checked before
	// the shader family so a Standard document with a future requirement is not
	// rendered as if it were plain Standard.
	const QJsonValue reqv = root.value( QStringLiteral( "requirements" ) );
	if ( reqv.isObject() ) {
		const QJsonObject req = reqv.toObject();
		for ( auto it = req.constBegin(); it != req.constEnd(); ++it ) {
			if ( !it.value().isDouble() || it.value().toInt() <= 0 ) {
				m.error = QStringLiteral( "malformed requirement version for \"%1\"" ).arg( it.key() );
				return m;
			}
			if ( !pbrmProvides( it.key(), it.value().toInt() ) ) {
				m.unsupported = true;
				m.diagnostics.append( QStringLiteral( "requires %1 v%2, not provided" )
					.arg( it.key() ).arg( it.value().toInt() ) );
			}
		}
	}

	// An unknown shader family is VALID but unsupported — never coerce it to
	// Standard. v4 spelled some names with a trailing " Surface".
	QString canon = m.shader.trimmed();
	if ( canon.endsWith( QLatin1String( " Surface" ), Qt::CaseInsensitive ) )
		canon.chop( 8 );
	if ( canon.compare( QLatin1String( "Standard" ), Qt::CaseInsensitive ) != 0 ) {
		m.unsupported = true;
		m.diagnostics.append( QStringLiteral( "shader family \"%1\" is not in the supported slice" ).arg( m.shader ) );
	}
	m.shader = canon;

	const QJsonObject uv = root.value( QStringLiteral( "primaryUv" ) ).toObject();
	QJsonObject vBase, vNorm, vRmaos, vEmis;
	m.baseColor = pbrmSlot( uv, "primaryBaseColor", vBase, m.diagnostics );
	m.normal    = pbrmSlot( uv, "primaryNormal", vNorm, m.diagnostics );
	m.rmaos     = pbrmSlot( uv, "primaryRmaos", vRmaos, m.diagnostics );
	m.emissive  = pbrmSlot( uv, "primaryEmissive", vEmis, m.diagnostics );

	auto samplable = []( const PbrmMaterial::Slot & s ) {
		return s.enabled && s.pathValid;
	};

	// Constants. A missing texture forces its positive overrides on, so the
	// constant applies — with the documented exemption for porosity, whose
	// absence has a meaningful derived value instead.
	m.overrideColor = pbrmBool( vBase, "overrideColor", true ) || !samplable( m.baseColor );
	pbrmColor( vBase, "color", m.baseColorRGB );
	m.overrideOpacity = pbrmBool( vBase, "overrideOpacity", true ) || !samplable( m.baseColor );
	m.opacity = pbrmNumber( vBase, "opacity", 1.0f );

	m.overrideNormal = pbrmBool( vNorm, "overrideNormal", true ) || !samplable( m.normal );
	m.normalStrength = pbrmNumber( vNorm, "strength", 1.0f );
	m.heightInBlue = pbrmBool( vNorm, "heightInBlue", false );
	m.curvatureInAlpha = pbrmBool( vNorm, "curvatureInAlpha", false );
	m.cavitySpecOcclusion = pbrmNumber( vNorm, "cavitySpecOcclusion", 1.0f );

	m.overrideRoughness = pbrmBool( vRmaos, "overrideRoughness", true ) || !samplable( m.rmaos );
	m.roughness = pbrmNumber( vRmaos, "roughness", 0.5f );
	m.overrideMetallic = pbrmBool( vRmaos, "overrideMetallic", true ) || !samplable( m.rmaos );
	m.metallic = pbrmNumber( vRmaos, "metallic", 0.0f );
	m.overrideAo = pbrmBool( vRmaos, "overrideAo", true ) || !samplable( m.rmaos );
	m.ao = pbrmNumber( vRmaos, "ao", 1.0f );
	m.alphaCarries = vRmaos.value( QStringLiteral( "alphaCarries" ) ).toString( QStringLiteral( "Dielectric F0" ) );
	m.f0 = pbrmNumber( vRmaos, "f0", 0.04f );
	const bool alphaIsF0 = ( m.alphaCarries.compare( QLatin1String( "Dielectric F0" ), Qt::CaseInsensitive ) == 0 );
	// RMAOS alpha is F0 only while alphaCarries says so; under Porosity, F0
	// always takes its constant.
	m.overrideF0 = pbrmBool( vRmaos, "overrideF0", true ) || !samplable( m.rmaos ) || !alphaIsF0;
	m.overridePorosity = pbrmBool( vRmaos, "overridePorosity", false );
	m.porosity = pbrmNumber( vRmaos, "porosity", 0.5f );

	pbrmColor( vEmis, "color", m.emissiveRGB );
	m.emissiveIntensity = pbrmNumber( vEmis, "intensity", 0.0f );

	// Derived feature mask (parser/cache bits, not the old BGSM CB2 sentinel).
	quint32 f = 0;
	if ( samplable( m.baseColor ) ) {
		f |= PbrmMaterial::BaseColorTexture;
		if ( !m.overrideOpacity )
			f |= PbrmMaterial::OpacityTexture;
	}
	if ( samplable( m.normal ) ) {
		f |= PbrmMaterial::NormalTexture;
		if ( m.heightInBlue )
			f |= PbrmMaterial::NormalHeightBlue;
		if ( m.curvatureInAlpha )
			f |= PbrmMaterial::NormalCurvature;
	}
	if ( samplable( m.rmaos ) ) {
		f |= PbrmMaterial::RmaosTexture;
		if ( !m.overrideRoughness )
			f |= PbrmMaterial::RmaosRoughness;
		if ( !m.overrideMetallic )
			f |= PbrmMaterial::RmaosMetallic;
		if ( !m.overrideAo )
			f |= PbrmMaterial::RmaosAo;
		if ( !m.overrideF0 && alphaIsF0 )
			f |= PbrmMaterial::RmaosF0;
		if ( !alphaIsF0 )
			f |= PbrmMaterial::RmaosPorosity;
	}
	if ( samplable( m.emissive ) )
		f |= PbrmMaterial::EmissiveTexture;
	m.features = f;

	m.ok = !m.unsupported;
	return m;
}


PbrmMaterial pbrmParseFile( const QString & path )
{
	PbrmMaterial m;
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		m.error = QStringLiteral( "cannot open \"%1\": %2" ).arg( path, f.errorString() );
		return m;
	}
	if ( f.size() > PBRM_MAX_PAYLOAD + 12 ) {
		m.error = QStringLiteral( "file larger than a PBRM envelope can be" );
		return m;
	}
	return pbrmParse( f.readAll() );
}
