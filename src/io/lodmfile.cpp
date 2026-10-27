#include "io/lodmfile.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QtEndian>

static const char LODM_MAGIC[4] = { 'L', 'O', 'D', 'M' };
static const quint32 LODM_VERSION = 1;
static const qsizetype LODM_PAYLOAD_CAP = 4 * 1024 * 1024;	// a LOD material is small by design


LodmMaterial lodmParse( const QByteArray & bytes )
{
	LodmMaterial m;
	if ( bytes.size() < 12 ) {
		m.error = QStringLiteral( "too short to hold a LODM envelope" );
		return m;
	}
	if ( !bytes.startsWith( QByteArray( LODM_MAGIC, 4 ) ) ) {
		m.error = QStringLiteral( "bad magic (expected LODM)" );
		return m;
	}
	m.version = int( qFromLittleEndian<quint32>( reinterpret_cast<const uchar *>( bytes.constData() ) + 4 ) );
	if ( m.version != int( LODM_VERSION ) ) {
		m.error = QStringLiteral( "unsupported envelope version %1" ).arg( m.version );
		return m;
	}
	const quint32 declared = qFromLittleEndian<quint32>( reinterpret_cast<const uchar *>( bytes.constData() ) + 8 );
	if ( declared > quint32( LODM_PAYLOAD_CAP ) ) {
		m.error = QStringLiteral( "payload %1 exceeds the cap" ).arg( declared );
		return m;
	}
	if ( qsizetype( declared ) != bytes.size() - 12 ) {
		m.error = QStringLiteral( "payload size %1 does not match the %2 bytes present" )
			.arg( declared ).arg( bytes.size() - 12 );
		return m;
	}
	QJsonParseError perr;
	const QJsonDocument doc = QJsonDocument::fromJson( bytes.mid( 12 ), &perr );
	if ( perr.error != QJsonParseError::NoError || !doc.isObject() ) {
		m.error = QStringLiteral( "malformed JSON payload: %1" ).arg( perr.errorString() );
		return m;
	}
	m.root = doc.object();
	/* PAYLOAD VERSION 2 is the CARD family's (IMPOSTORWIND1 sway A, CARDFIX1 step 6):
	 * a card, card array or aggregate whose _n.A is a model's own wind weight says
	 * `lodm` 2 and `sway` "model". Nothing else may claim it: a source (a material)
	 * is version 1 and is refused BY NAME if it says 2. */
	{
		const int pv = m.root.value( QStringLiteral( "lodm" ) ).toInt( 0 );
		const QString k = m.root.value( QStringLiteral( "kind" ) ).toString( QStringLiteral( "source" ) );
		const bool cardFamily = k == QLatin1String( "card" ) || k == QLatin1String( "cardArray" )
			|| k == QLatin1String( "aggregate" );
		if ( pv == 2 && !cardFamily ) {
			m.error = QStringLiteral( "lodm 2 is the card family's version (card, cardArray, aggregate: the "
				"model sway); a \"%1\" .lodm is lodm 1" ).arg( k );
			return m;
		}
		// lodm 3 (LODMNAME1): the both-families array layout, arrays only
		const bool arrayKind = k == QLatin1String( "array" ) || k == QLatin1String( "cardArray" );
		if ( pv == 3 && !arrayKind ) {
			m.error = QStringLiteral( "lodm 3 is the array layout (array, cardArray); a \"%1\" .lodm is "
				"lodm 1 or 2" ).arg( k );
			return m;
		}
		if ( pv != int( LODM_VERSION ) && pv != 2 && pv != 3 ) {
			m.error = QStringLiteral( "payload is not a lodm 1 object (nor a card family lodm 2, nor an "
				"array lodm 3)" );
			return m;
		}
		m.payloadVersion = pv;
	}
	m.family = m.root.value( QStringLiteral( "family" ) ).toString();
	const bool v3 = m.payloadVersion == 3;
	if ( m.family != QLatin1String( "legacy" ) && m.family != QLatin1String( "pbr" )
		&& !( v3 && m.family == QLatin1String( "mixed" ) ) ) {
		m.error = v3 ? QStringLiteral( "family must be legacy, pbr or mixed, not \"%1\"" ).arg( m.family )
			: QStringLiteral( "family must be legacy or pbr, not \"%1\"" ).arg( m.family );
		return m;
	}
	m.pbr = ( m.family == QLatin1String( "pbr" ) );
	m.kind = m.root.value( QStringLiteral( "kind" ) ).toString( QStringLiteral( "source" ) );
	const QJsonObject tex = m.root.value( QStringLiteral( "textures" ) ).toObject();
	if ( !v3 ) {
		m.color = tex.value( QLatin1String( lodmColorKey( m.pbr ) ) ).toString();
		m.normal = tex.value( QStringLiteral( "normal" ) ).toString();
		m.mask = tex.value( QLatin1String( lodmMaskKey( m.pbr ) ) ).toString();
		m.emissive = tex.value( QStringLiteral( "emissive" ) ).toString();
	} else {
		// one shared normal, then each family's own colour/mask/emissive, legacy before pbr
		m.normal = tex.value( QStringLiteral( "normal" ) ).toString();
		for ( int f = 0; f < 2; ++f ) {
			const QString fk = f ? QStringLiteral( "pbr" ) : QStringLiteral( "legacy" );
			if ( !tex.contains( fk ) )
				continue;
			const QJsonObject b = tex.value( fk ).toObject();
			auto & s = m.fam[f];
			s.present = true;
			s.color = b.value( QLatin1String( lodmColorKey( f ) ) ).toString();
			s.mask = b.value( QLatin1String( lodmMaskKey( f ) ) ).toString();
			s.emissive = b.value( QStringLiteral( "emissive" ) ).toString();
			if ( s.color.isEmpty() ) {
				m.error = QStringLiteral( "lodm 3 textures.%1 names no %2" ).arg( fk, QLatin1String( lodmColorKey( f ) ) );
				return m;
			}
		}
		m.mixed = m.fam[0].present && m.fam[1].present;
		const bool famOk = m.mixed ? m.family == QLatin1String( "mixed" )
			: ( m.fam[0].present && m.family == QLatin1String( "legacy" ) )
				|| ( m.fam[1].present && m.family == QLatin1String( "pbr" ) );
		if ( !famOk || m.normal.isEmpty() ) {
			m.error = QStringLiteral( "lodm 3 family \"%1\" does not match its texture blocks (legacy %2, pbr %3, "
				"normal %4)" ).arg( m.family ).arg( int( m.fam[0].present ) ).arg( int( m.fam[1].present ) ).arg( int( !m.normal.isEmpty() ) );
			return m;
		}
		const QJsonArray layers = m.root.value( QStringLiteral( "array" ) ).toObject()
			.value( QStringLiteral( "layers" ) ).toArray();
		for ( int i = 0; i < layers.size(); ++i ) {
			const QJsonObject L = layers.at( i ).toObject();
			const QString lf = L.value( QStringLiteral( "family" ) ).toString();
			const int f = lf == QLatin1String( "pbr" ) ? 1 : lf == QLatin1String( "legacy" ) ? 0 : -1;
			if ( f < 0 || !m.fam[f].present ) {
				m.error = QStringLiteral( "lodm 3 layer %1 family \"%2\" has no texture block" ).arg( i ).arg( lf );
				return m;
			}
			const int nl = L.value( QStringLiteral( "normalLayer" ) ).toInt( -1 );
			const int fl = L.value( QStringLiteral( "familyLayer" ) ).toInt( -1 );
			if ( nl != i || fl != m.fam[f].layers ) {
				m.error = QStringLiteral( "lodm 3 layer %1: normalLayer %2 familyLayer %3 (expected %1 and %4)" )
					.arg( i ).arg( nl ).arg( fl ).arg( m.fam[f].layers );
				return m;
			}
			m.fam[f].layers++;
			m.layerPbr.append( qint8( f ) );
			m.layerFamilyIndex.append( fl );
		}
		for ( int f = 0; f < 2; ++f )
			if ( m.fam[f].present && m.fam[f].layers == 0 ) {
				m.error = QStringLiteral( "lodm 3 textures.%1 has no layer" ).arg( QLatin1String( f ? "pbr" : "legacy" ) );
				return m;
			}
		const int first = m.fam[0].present ? 0 : 1;
		m.pbr = !m.mixed && first == 1;
		m.color = m.fam[first].color;
		m.mask = m.fam[first].mask;
		m.emissive = m.fam[first].emissive;
	}
	// the emissive MULTIPLE (the colour is folded into the sheet); absent means 1
	m.emissiveScale = float( m.root.value( QStringLiteral( "emissiveScale" ) ).toDouble( 1.0 ) );
	m.heightInBlue = m.root.value( QStringLiteral( "heightInBlue" ) ).toBool( false );
	m.ok = true;
	return m;
}

bool lodmArrayLayer( const LodmMaterial & m, int layer, bool * pbr, int * familyLayer )
{
	if ( m.payloadVersion != 3 ) {
		*pbr = m.pbr;
		*familyLayer = layer;
		return true;
	}
	if ( layer < 0 || layer >= m.layerPbr.size() )
		return false;
	*pbr = m.layerPbr.at( layer ) != 0;
	*familyLayer = m.layerFamilyIndex.at( layer );
	return true;
}

QByteArray lodmSerialise( const QJsonObject & root )
{
	const QByteArray payload = QJsonDocument( root ).toJson( QJsonDocument::Compact );
	QByteArray out;
	out.reserve( payload.size() + 12 );
	out.append( LODM_MAGIC, 4 );
	uchar u[4];
	qToLittleEndian<quint32>( LODM_VERSION, u );
	out.append( reinterpret_cast<const char *>( u ), 4 );
	qToLittleEndian<quint32>( quint32( payload.size() ), u );
	out.append( reinterpret_cast<const char *>( u ), 4 );
	out.append( payload );
	return out;
}

QString lodmSourceCandidate( const QString & material, const QString & diffuse )
{
	QString c = material;
	if ( c.isEmpty() ) {
		c = diffuse;
		c.replace( QChar( '/' ), QChar( '\\' ) );
		if ( c.startsWith( QStringLiteral( "data\\" ), Qt::CaseInsensitive ) )
			c.remove( 0, 5 );
		if ( c.startsWith( QStringLiteral( "textures\\" ), Qt::CaseInsensitive ) )
			c.remove( 0, 9 );
		if ( c.isEmpty() )
			return QString();
		c.prepend( QStringLiteral( "materials\\" ) );
	} else {
		c.replace( QChar( '/' ), QChar( '\\' ) );
		if ( c.startsWith( QStringLiteral( "data\\" ), Qt::CaseInsensitive ) )
			c.remove( 0, 5 );
	}
	const int dot = c.lastIndexOf( QChar( '.' ) );
	if ( dot > c.lastIndexOf( QChar( '\\' ) ) )
		c.truncate( dot );
	return c + QStringLiteral( ".lodm" );
}

bool lodmWriteFile( const QString & path, const QJsonObject & root )
{
	QFile f( path );
	if ( !f.open( QIODevice::WriteOnly ) )
		return false;
	const QByteArray bytes = lodmSerialise( root );
	return f.write( bytes ) == bytes.size();
}
