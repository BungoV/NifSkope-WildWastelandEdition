#include "io/nifxfile.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>


namespace {

//! A byte-offset JSON scanner. It VALIDATES nothing that QJsonDocument has not
//! already validated; its only job is to report where each token starts and ends.
struct NifxScanner
{
	const QByteArray & b;
	int n;

	explicit NifxScanner( const QByteArray & bytes ) : b( bytes ), n( int( bytes.size() ) ) {}

	char at( int p ) const { return ( p >= 0 && p < n ) ? b.at( p ) : '\0'; }

	int ws( int p ) const
	{
		while ( p < n ) {
			const char c = b.at( p );
			if ( c != ' ' && c != '\t' && c != '\n' && c != '\r' )
				break;
			p++;
		}
		return p;
	}

	//! p at the opening quote; returns the offset after the closing quote, or -1.
	int str( int p ) const
	{
		if ( at( p ) != '"' )
			return -1;
		p++;
		while ( p < n ) {
			const char c = b.at( p );
			if ( c == '\\' ) {
				p += 2;
				continue;
			}
			if ( c == '"' )
				return p + 1;
			p++;
		}
		return -1;
	}

	int value( int p, int depth ) const
	{
		if ( depth > 256 )
			return -1;
		const char c = at( p );
		if ( c == '"' )
			return str( p );
		if ( c == '{' || c == '[' ) {
			const char close = ( c == '{' ) ? '}' : ']';
			p = ws( p + 1 );
			if ( at( p ) == close )
				return p + 1;
			for ( ;; ) {
				if ( c == '{' ) {
					p = str( p );
					if ( p < 0 )
						return -1;
					p = ws( p );
					if ( at( p ) != ':' )
						return -1;
					p = ws( p + 1 );
				}
				p = value( p, depth + 1 );
				if ( p < 0 )
					return -1;
				p = ws( p );
				if ( at( p ) == ',' ) {
					p = ws( p + 1 );
					continue;
				}
				if ( at( p ) == close )
					return p + 1;
				return -1;
			}
		}
		// number / true / false / null
		const int s = p;
		while ( p < n ) {
			const char d = b.at( p );
			if ( d == ',' || d == '}' || d == ']' || d == ' ' || d == '\t' || d == '\n' || d == '\r' )
				break;
			p++;
		}
		return p > s ? p : -1;
	}

	//! Members of the object whose '{' is at p. False on a structural surprise.
	bool members( int p, QList<NifxDocument::Member> & out ) const
	{
		out.clear();
		if ( at( p ) != '{' )
			return false;
		p = ws( p + 1 );
		if ( at( p ) == '}' )
			return true;
		for ( ;; ) {
			NifxDocument::Member m;
			m.keySpan.begin = p;
			m.keySpan.end = str( p );
			if ( m.keySpan.end < 0 )
				return false;
			m.key = decodeString( m.keySpan );
			p = ws( m.keySpan.end );
			if ( at( p ) != ':' )
				return false;
			p = ws( p + 1 );
			m.valueSpan.begin = p;
			m.valueSpan.end = value( p, 1 );
			if ( m.valueSpan.end < 0 )
				return false;
			out.append( m );
			p = ws( m.valueSpan.end );
			if ( at( p ) == ',' ) {
				p = ws( p + 1 );
				continue;
			}
			return at( p ) == '}';
		}
	}

	QString decodeString( const NifxDocument::Span & s ) const
	{
		const QByteArray raw = "[" + b.mid( s.begin, s.end - s.begin ) + "]";
		const QJsonDocument d = QJsonDocument::fromJson( raw );
		return d.isArray() && !d.array().isEmpty() ? d.array().at( 0 ).toString() : QString();
	}
};

QByteArray nifxJsonString( const QString & s )
{
	QByteArray a = QJsonDocument( QJsonArray { s } ).toJson( QJsonDocument::Compact );
	// ["..."] -> "..."
	return a.mid( 1, a.size() - 2 );
}

//! The whitespace run that ends at `p` (exclusive), after the previous '{' or ','.
QByteArray nifxIndentBefore( const QByteArray & b, int p )
{
	int s = p;
	while ( s > 0 ) {
		const char c = b.at( s - 1 );
		if ( c != ' ' && c != '\t' && c != '\n' && c != '\r' )
			break;
		s--;
	}
	return b.mid( s, p - s );
}

} // namespace


const NifxDocument::MaterialEntry * NifxDocument::findMaterial( const QString & node ) const
{
	for ( const MaterialEntry & e : material )
		if ( e.node.compare( node, Qt::CaseInsensitive ) == 0 )
			return &e;
	return nullptr;
}

NifxDocument nifxParse( const QByteArray & bytes )
{
	NifxDocument d;
	d.bytes = bytes;
	if ( bytes.size() > NIFX_MAX_BYTES ) {
		d.error = QStringLiteral( "larger than the 1 MiB .nifx cap" );
		return d;
	}
	const NifxScanner sc( d.bytes );
	int p = 0;
	if ( bytes.startsWith( "\xEF\xBB\xBF" ) )
		p = 3;
	p = sc.ws( p );
	if ( sc.at( p ) != '{' ) {
		d.error = p >= sc.n ? QStringLiteral( "empty .nifx" )
			: QStringLiteral( "not the JSON generation (first byte is not '{')" );
		return d;
	}
	QJsonParseError perr {};
	const QJsonDocument doc = QJsonDocument::fromJson( bytes.mid( p ), &perr );
	if ( perr.error != QJsonParseError::NoError || !doc.isObject() ) {
		d.error = QStringLiteral( "malformed JSON: %1" ).arg( perr.errorString() );
		return d;
	}
	const QJsonObject root = doc.object();
	const QJsonValue ver = root.value( QStringLiteral( "version" ) );
	if ( ver.isUndefined() ) {
		d.error = QStringLiteral( "no \"version\" field" );
		return d;
	}
	if ( !ver.isDouble() ) {
		d.error = QStringLiteral( "\"version\" is not a number" );
		return d;
	}
	if ( ver.toDouble() != 1.0 ) {
		d.error = QStringLiteral( "unsupported .nifx version %1 (this reader knows 1)" ).arg( ver.toDouble() );
		return d;
	}
	d.version = 1;

	if ( !sc.members( p, d.members ) ) {
		d.error = QStringLiteral( "could not locate the top-level members" );
		return d;
	}
	int sections = 0;
	for ( const NifxDocument::Member & m : d.members ) {
		if ( m.key == QLatin1String( "version" ) )
			continue;
		sections++;
		if ( m.key == QLatin1String( "material" ) ) {
			d.hasMaterial = true;
			d.materialSpan = m.valueSpan;
		} else {
			d.unknownSections.append( m.key );
		}
	}
	if ( sections > NIFX_MAX_SECTIONS ) {
		d.error = QStringLiteral( "%1 sections, over the cap of %2" ).arg( sections ).arg( NIFX_MAX_SECTIONS );
		return d;
	}

	if ( d.hasMaterial ) {
		QList<NifxDocument::Member> nodes;
		if ( sc.at( d.materialSpan.begin ) != '{' ) {
			d.warnings << QStringLiteral( "\"material\" is not an object; ignored" );
		} else if ( !sc.members( d.materialSpan.begin, nodes ) ) {
			d.error = QStringLiteral( "could not locate the material entries" );
			return d;
		}
		if ( nodes.size() > NIFX_MAX_NODES ) {
			d.error = QStringLiteral( "%1 material nodes, over the cap of %2" ).arg( nodes.size() ).arg( NIFX_MAX_NODES );
			return d;
		}
		for ( const NifxDocument::Member & nm : nodes ) {
			NifxDocument::MaterialEntry e;
			e.node = nm.key;
			e.keySpan = nm.keySpan;
			e.valueSpan = nm.valueSpan;
			QList<NifxDocument::Member> fields;
			if ( sc.at( nm.valueSpan.begin ) != '{' || !sc.members( nm.valueSpan.begin, fields ) ) {
				e.problem = QStringLiteral( "entry is not an object" );
			} else {
				for ( const NifxDocument::Member & f : fields ) {
					if ( f.key != QLatin1String( "pbrm" ) )
						continue;	// unknown keys are kept and ignored
					if ( sc.at( f.valueSpan.begin ) != '"' ) {
						e.problem = QStringLiteral( "\"pbrm\" is not a string" );
						e.pbrmSpan = NifxDocument::Span();
						continue;
					}
					e.pbrmSpan = f.valueSpan;	// the last one wins, as in QJsonDocument
					e.pbrm = sc.decodeString( f.valueSpan );
					e.problem.clear();
				}
				if ( !e.pbrmSpan.valid() && e.problem.isEmpty() )
					e.problem = QStringLiteral( "no \"pbrm\" key" );
				else if ( e.pbrmSpan.valid() && e.pbrm.trimmed().isEmpty() )
					e.problem = QStringLiteral( "\"pbrm\" is empty" );
			}
			e.valid = e.problem.isEmpty();
			if ( !e.valid )
				d.warnings << QStringLiteral( "material \"%1\": %2" ).arg( e.node, e.problem );
			for ( const NifxDocument::MaterialEntry & prev : d.material )
				if ( prev.node.compare( e.node, Qt::CaseInsensitive ) == 0 ) {
					d.warnings << QStringLiteral( "material \"%1\" duplicates \"%2\" (case-insensitive); the first wins" )
						.arg( e.node, prev.node );
					break;
				}
			d.material.append( e );
		}
	}
	d.ok = true;
	return d;
}

NifxDocument nifxParseFile( const QString & path )
{
	QFile f( path );
	if ( !f.exists() ) {
		NifxDocument d;
		d.error = QStringLiteral( "absent" );
		return d;
	}
	if ( !f.open( QIODevice::ReadOnly ) ) {
		NifxDocument d;
		d.error = QStringLiteral( "cannot open: %1" ).arg( f.errorString() );
		return d;
	}
	if ( f.size() > NIFX_MAX_BYTES ) {
		NifxDocument d;
		d.error = QStringLiteral( "larger than the 1 MiB .nifx cap" );
		return d;
	}
	return nifxParse( f.readAll() );
}

static bool nifxSplice( NifxDocument & d, int begin, int end, const QByteArray & with, QString * why )
{
	QByteArray b = d.bytes;
	b.replace( begin, end - begin, with );
	NifxDocument n = nifxParse( b );
	if ( !n.ok ) {
		if ( why )
			*why = QStringLiteral( "edit would break the document: %1" ).arg( n.error );
		return false;
	}
	d = n;
	return true;
}

bool nifxSetMaterial( NifxDocument & d, const QString & node, const QString & pbrm, QString * why )
{
	if ( !d.ok ) {
		if ( why )
			*why = QStringLiteral( "document did not parse" );
		return false;
	}
	const QByteArray val = nifxJsonString( pbrm );
	const QByteArray entryText = "{ \"pbrm\": " + val + " }";

	for ( const NifxDocument::MaterialEntry & e : d.material ) {
		if ( e.node.compare( node, Qt::CaseInsensitive ) != 0 )
			continue;
		if ( e.pbrmSpan.valid() )
			return nifxSplice( d, e.pbrmSpan.begin, e.pbrmSpan.end, val, why );
		if ( d.bytes.at( e.valueSpan.begin ) != '{' )
			return nifxSplice( d, e.valueSpan.begin, e.valueSpan.end, entryText, why );
		// an object without "pbrm": add it as the last member
		const int close = e.valueSpan.end - 1;
		int q = close;
		while ( q > e.valueSpan.begin && QByteArray( " \t\r\n" ).contains( d.bytes.at( q - 1 ) ) )
			q--;
		const bool empty = ( q == e.valueSpan.begin + 1 );
		return nifxSplice( d, q, q, ( empty ? QByteArray( " " ) : QByteArray( ", " ) ) + "\"pbrm\": " + val
			+ ( empty ? " " : "" ), why );
	}

	const QByteArray keyText = nifxJsonString( node );
	if ( d.hasMaterial && d.bytes.at( d.materialSpan.begin ) == '{' ) {
		if ( !d.material.isEmpty() ) {
			const NifxDocument::MaterialEntry & last = d.material.last();
			const QByteArray indent = nifxIndentBefore( d.bytes, last.keySpan.begin );
			return nifxSplice( d, last.valueSpan.end, last.valueSpan.end,
				"," + indent + keyText + ": " + entryText, why );
		}
		return nifxSplice( d, d.materialSpan.begin, d.materialSpan.end,
			"{ " + keyText + ": " + entryText + " }", why );
	}
	if ( d.hasMaterial ) {
		// "material" exists but is not an object: replace its value
		return nifxSplice( d, d.materialSpan.begin, d.materialSpan.end,
			"{ " + keyText + ": " + entryText + " }", why );
	}
	if ( d.members.isEmpty() ) {
		if ( why )
			*why = QStringLiteral( "document has no members" );
		return false;
	}
	const NifxDocument::Member & lastTop = d.members.last();
	const QByteArray indent = nifxIndentBefore( d.bytes, lastTop.keySpan.begin );
	return nifxSplice( d, lastTop.valueSpan.end, lastTop.valueSpan.end,
		"," + indent + "\"material\": { " + keyText + ": " + entryText + " }", why );
}

bool nifxRemoveMaterial( NifxDocument & d, const QString & node, QString * why )
{
	for ( int i = 0; i < d.material.size(); i++ ) {
		const NifxDocument::MaterialEntry & e = d.material.at( i );
		if ( e.node.compare( node, Qt::CaseInsensitive ) != 0 )
			continue;
		if ( i > 0 )
			return nifxSplice( d, d.material.at( i - 1 ).valueSpan.end, e.valueSpan.end, QByteArray(), why );
		if ( d.material.size() > 1 )
			return nifxSplice( d, e.keySpan.begin, d.material.at( 1 ).keySpan.begin, QByteArray(), why );
		return nifxSplice( d, e.keySpan.begin, e.valueSpan.end, QByteArray(), why );
	}
	if ( why )
		*why = QStringLiteral( "no material entry for \"%1\"" ).arg( node );
	return false;
}

QString nifxPathForNif( const QString & nifPath )
{
	if ( nifPath.isEmpty() )
		return QString();
	const QFileInfo fi( nifPath );
	if ( fi.suffix().compare( QLatin1String( "nif" ), Qt::CaseInsensitive ) != 0 )
		return QString();
	return fi.path() + QLatin1Char( '/' ) + fi.completeBaseName() + QLatin1String( ".nifx" );
}

QString nifxMaterialFor( const QString & nifPath, const QString & node, QString * note )
{
	if ( note )
		note->clear();
	const QString path = nifxPathForNif( nifPath );
	if ( path.isEmpty() )
		return QString();

	struct Cached
	{
		bool exists = false;
		qint64 size = -1;
		QDateTime mtime;
		NifxDocument doc;
	};
	static QHash<QString, Cached> cache;

	const QFileInfo fi( path );
	const bool exists = fi.isFile();
	auto it = cache.find( path );
	if ( it == cache.end() || it->exists != exists
		|| ( exists && ( it->size != fi.size() || it->mtime != fi.lastModified() ) ) ) {
		Cached c;
		c.exists = exists;
		if ( exists ) {
			c.size = fi.size();
			c.mtime = fi.lastModified();
			c.doc = nifxParseFile( path );
		}
		if ( cache.size() > 4096 )
			cache.clear();
		it = cache.insert( path, c );
	}
	if ( !it->exists )
		return QString();
	const NifxDocument & d = it->doc;
	if ( !d.ok ) {
		if ( note )
			*note = QStringLiteral( "%1 refused: %2" ).arg( fi.fileName(), d.error );
		return QString();
	}
	const NifxDocument::MaterialEntry * e = d.findMaterial( node );
	if ( !e ) {
		if ( note )
			*note = QStringLiteral( "%1 has no material entry for \"%2\"" ).arg( fi.fileName(), node );
		return QString();
	}
	if ( !e->valid ) {
		if ( note )
			*note = QStringLiteral( "%1 entry \"%2\": %3" ).arg( fi.fileName(), e->node, e->problem );
		return QString();
	}
	return e->pbrm;
}
