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
	if ( m.envelopeVersion < 4 || m.envelopeVersion > 6 ) {
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
	// schemaVersion must equal the envelope version (PBRM-v6.md; FO4CS
	// PBRM.cpp:1199) -- a v5 payload in a v6 envelope is not a v6 material.
	if ( !sver.isDouble() || sver.toDouble() != double( m.envelopeVersion ) ) {
		m.error = QStringLiteral( "schemaVersion does not match envelope version %1" ).arg( m.envelopeVersion );
		return m;
	}
	m.specularV6 = ( m.envelopeVersion >= 6 );
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
	m.diffuseRoughness = qBound( 0.0f, pbrmNumber( vBase, "diffuseRoughness", 0.0f ), 1.0f );

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
	// RMAOS alpha. v4/v5: the dielectric F0 itself, clamped to [0, 0.16] like
	// the FO4CS reader. v6: the OpenPBR specular WEIGHT, [0,1], default 1,
	// sampled only while alphaCarries is "Specular Weight" (the v6 default)
	// and overrideSpecularWeight is false.
	const QLatin1String alphaDefault = m.specularV6 ? QLatin1String( "Specular Weight" ) : QLatin1String( "Dielectric F0" );
	m.alphaCarries = vRmaos.value( QStringLiteral( "alphaCarries" ) ).toString( alphaDefault );
	const bool alphaIsWeight = m.specularV6
		&& m.alphaCarries.compare( QLatin1String( "Specular Weight" ), Qt::CaseInsensitive ) == 0;
	const bool alphaIsF0 = !m.specularV6
		&& m.alphaCarries.compare( QLatin1String( "Dielectric F0" ), Qt::CaseInsensitive ) == 0;
	const bool alphaIsPorosity = m.alphaCarries.compare( QLatin1String( "Porosity" ), Qt::CaseInsensitive ) == 0
		|| ( !m.specularV6 && !alphaIsF0 );
	if ( m.specularV6 ) {
		m.specularWeight = qBound( 0.0f, pbrmNumber( vRmaos, "specularWeight", 1.0f ), 1.0f );
		m.overrideSpecularWeight = pbrmBool( vRmaos, "overrideSpecularWeight", true )
			|| !samplable( m.rmaos ) || !alphaIsWeight;
		m.overrideF0 = true;		// no authored F0 in v6
	} else {
		m.f0 = qBound( 0.0f, pbrmNumber( vRmaos, "f0", 0.04f ), 0.16f );
		// RMAOS alpha is F0 only while alphaCarries says so; under Porosity, F0
		// always takes its constant.
		m.overrideF0 = pbrmBool( vRmaos, "overrideF0", true ) || !samplable( m.rmaos ) || !alphaIsF0;
	}
	m.overridePorosity = pbrmBool( vRmaos, "overridePorosity", false );
	m.porosity = pbrmNumber( vRmaos, "porosity", 0.5f );

	// v6 Pattern B: primarySpecularColor carries the specular parameters in its
	// sparse `values`. ior / iorMax are read whether or not the slot is enabled;
	// the colour only when it is (a disabled slot is white).
	{
		QJsonObject vSpec;
		m.specularColor = pbrmSlot( uv, "primarySpecularColor", vSpec, m.diagnostics );
		if ( m.specularV6 ) {
			m.specularIor = qBound( 0.0f, pbrmNumber( vSpec, "ior", 1.5f ), 1000.0f );
			m.specularIorMax = qBound( 0.0f, pbrmNumber( vSpec, "iorMax", 4.25f ), 1000.0f );
			m.overrideSpecularColor = pbrmBool( vSpec, "overrideColor", true );
			m.overrideSpecularIor = pbrmBool( vSpec, "overrideIor", true );
			if ( m.specularColor.enabled )
				pbrmColor( vSpec, "color", m.specularTint );
			if ( m.specularColor.enabled ) {
				const bool hasMap = m.specularColor.pathValid;
				if ( hasMap && !m.overrideSpecularColor )
					m.features |= PbrmMaterial::SpecularColorTexture;
				if ( hasMap && !m.overrideSpecularIor )
					m.features |= PbrmMaterial::SpecularIorTexture;
				// overrideIor=false with no map: the editor preview packs the
				// constant as a texel over the ceiling, so it renders
				// min(ior, iorMax) (FO4CS PBRM.cpp:1002-1006).
				if ( !m.overrideSpecularIor && !hasMap )
					m.specularIor = qMin( m.specularIor, m.specularIorMax );
			}
		}
	}

	// Emission (lane PBRR4): OpenPBR emission_luminance in nits; 100 nits is linear 1.0
	// before exposure. A pre-2026-09-17 `intensity` IS luminance/100 (the editor loads it
	// as luminance = intensity x 100, materialdocument.cpp:211-216). With a map the
	// overrides default false: the map's RGB replaces the constant (ED:542-543).
	pbrmColor( vEmis, "color", m.emissiveRGB );
	if ( vEmis.contains( QStringLiteral( "luminance" ) ) )
		m.emissiveIntensity = qMax( 0.0f, pbrmNumber( vEmis, "luminance", 0.0f ) ) / 100.0f;
	else
		m.emissiveIntensity = qMax( 0.0f, pbrmNumber( vEmis, "intensity", 0.0f ) );
	m.emissiveMask = qBound( 0.0f, pbrmNumber( vEmis, "mask", 1.0f ), 1.0f );
	m.overrideEmissiveColor = pbrmBool( vEmis, "overrideColor", !samplable( m.emissive ) ) || !samplable( m.emissive );
	m.overrideEmissiveMask = pbrmBool( vEmis, "overrideMask", !samplable( m.emissive ) ) || !samplable( m.emissive );

	// Tint masks (lane PBRR4, special layer 0 of the editor). The slot's enable is the
	// switch; a channel samples the map only with a valid path and its override off
	// (the legacy `use<c>` spelling migrates as override = !use).
	{
		QJsonObject vTint;
		m.tintMask = pbrmSlot( uv, "primaryTintMask", vTint, m.diagnostics );
		m.tintEnabled = m.tintMask.enabled;
		static const char * const ch[4] = { "R", "G", "B", "A" };
		for ( int c = 0; c < 4; c++ ) {
			const QByteArray ov = QByteArray( "overrideMask" ) + ch[c];
			const QByteArray use = QByteArray( "use" ) + ch[c];
			bool over = true;
			if ( vTint.contains( QLatin1StringView( ov ) ) )
				over = pbrmBool( vTint, ov.constData(), true );
			else if ( vTint.contains( QLatin1StringView( use ) ) )
				over = !pbrmBool( vTint, use.constData(), false );
			m.tintUseTexture[c] = m.tintEnabled && samplable( m.tintMask ) && !over;
			m.tintMaskConst[c] = qBound( 0.0f, pbrmNumber( vTint, ( QByteArray( "mask" ) + ch[c] ).constData(), 0.0f ), 1.0f );
			pbrmColor( vTint, ( QByteArray( "color" ) + ch[c] ).constData(), m.tintColor[c] );
		}
		const QString ov = vTint.value( QStringLiteral( "overlap" ) ).toString( QStringLiteral( "Normalize" ) );
		m.tintOverlap = ov == QLatin1StringView( "Add" ) ? 1 : ov == QLatin1StringView( "Priority RGBA" ) ? 2 : 0;
	}

	// Composition (lane PBRR4; FO4CS PBRM.cpp:654-664 + 747-757). Unknown mode names fall
	// back to the family default rather than refusing the binding.
	{
		const QJsonObject settings = root.value( QStringLiteral( "settings" ) ).toObject();
		const QJsonObject comp = settings.value( QStringLiteral( "Transparency/Composition" ) ).toObject();
		const QString mode = comp.value( QStringLiteral( "mode" ) ).toString().trimmed();
		static const char * const modes[8] = { "Opaque", "Alpha Test", "Alpha Blend", "Premultiplied",
			"Additive", "Multiply", "Physical Transmission", "Dedicated Water Pass" };
		m.composition = 0;
		for ( int k = 0; k < 8; k++ )
			if ( mode.compare( QLatin1StringView( modes[k] ), Qt::CaseInsensitive ) == 0 )
				m.composition = k;
		m.globalOpacity = qBound( 0.0f, pbrmNumber( comp, "alpha", 1.0f ), 1.0f );
		m.alphaThreshold = qBound( 0.0f, pbrmNumber( comp, "threshold", 0.5f ), 1.0f );
		m.alphaSourceConstant = comp.value( QStringLiteral( "alphaSource" ) ).toString()
			.compare( QLatin1StringView( "Constant" ), Qt::CaseInsensitive ) == 0;
		const QJsonObject depth = settings.value( QStringLiteral( "Transparency/Depth" ) ).toObject();
		m.depthWrite = pbrmBool( depth, "depthWrite", m.composition <= 1 );
		m.twoSided = pbrmBool( settings.value( QStringLiteral( "Shading/Core" ) ).toObject(), "twoSided", false );
	}

	// Derived feature mask (parser/cache bits, not the old BGSM CB2 sentinel).
	// The v6 specular-colour bits were set above.
	quint32 f = m.features;
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
		if ( m.specularV6 && !m.overrideSpecularWeight )
			f |= PbrmMaterial::RmaosSpecularWeight;
		if ( alphaIsPorosity )
			f |= PbrmMaterial::RmaosPorosity;
	}
	// the map is SAMPLED only when a channel is not overridden (FO4CS PBRM.cpp:896-897);
	// an unsampled map must not abort the binding when it fails to load
	if ( samplable( m.emissive ) && ( !m.overrideEmissiveColor || !m.overrideEmissiveMask ) )
		f |= PbrmMaterial::EmissiveTexture;
	m.features = f;

	m.ok = !m.unsupported;
	return m;
}


float pbrmIorF0( float ior )
{
	const float eta = ior > 0.0f ? ior : 0.0f;
	const float r = ( eta - 1.0f ) / ( eta + 1.0f );
	return r * r;
}


float pbrmDielectricF0( const PbrmMaterial & m, PbrmF0Law law )
{
	if ( law == PbrmF0Law::V5 ) {
		// The red control: the document's RMAOS scalar taken as an F0, the
		// reading a v5-only consumer makes of a v6 file. No cap, on purpose.
		return m.specularV6 ? m.specularWeight : m.f0;
	}
	if ( m.specularV6 )
		return qMin( m.specularWeight * pbrmIorF0( m.specularIor ), 1.0f );
	// v4/v5: IOR 0 uploads iorF0 = 1 and cap 0.16, so min(f0 x 1, 0.16).
	return qMin( m.f0, 0.16f );
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
