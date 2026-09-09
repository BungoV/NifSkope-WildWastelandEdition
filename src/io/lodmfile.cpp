#include "io/lodmfile.h"

#include <QFile>
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
	if ( m.root.value( QStringLiteral( "lodm" ) ).toInt( 0 ) != int( LODM_VERSION ) ) {
		m.error = QStringLiteral( "payload is not a lodm 1 object" );
		return m;
	}
	m.family = m.root.value( QStringLiteral( "family" ) ).toString();
	if ( m.family != QLatin1String( "legacy" ) && m.family != QLatin1String( "pbr" ) ) {
		m.error = QStringLiteral( "family must be legacy or pbr, not \"%1\"" ).arg( m.family );
		return m;
	}
	m.pbr = ( m.family == QLatin1String( "pbr" ) );
	m.kind = m.root.value( QStringLiteral( "kind" ) ).toString( QStringLiteral( "source" ) );
	const QJsonObject tex = m.root.value( QStringLiteral( "textures" ) ).toObject();
	m.color = tex.value( QLatin1String( lodmColorKey( m.pbr ) ) ).toString();
	m.normal = tex.value( QStringLiteral( "normal" ) ).toString();
	m.mask = tex.value( QLatin1String( lodmMaskKey( m.pbr ) ) ).toString();
	m.emissive = tex.value( QStringLiteral( "emissive" ) ).toString();
	// the emissive MULTIPLE (the colour is folded into the sheet); absent means 1
	m.emissiveScale = float( m.root.value( QStringLiteral( "emissiveScale" ) ).toDouble( 1.0 ) );
	m.heightInBlue = m.root.value( QStringLiteral( "heightInBlue" ) ).toBool( false );
	m.ok = true;
	return m;
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
