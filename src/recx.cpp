#include "recx.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <vector>

#include <cstdio>

namespace {

struct RecxFile
{
	QString path;
	QString provider;	// its folder under Plugins/
	QVector<QJsonObject> entries;
};

struct RecxCategory
{
	QString signature;	// every file's path, size and time: a change re-reads them all
	QStringList roots;
	QVector<RecxFile> files;
	QString summary;
	QElapsedTimer checked;
};

QHash<QString, RecxCategory> & cats()
{
	static QHash<QString, RecxCategory> c;
	return c;
}

// every Plugins/<Provider>/<category>/*.recx under the roots; one file per Provider/category/name (the first root's)
QStringList listFiles( const QStringList & roots, const QString & category, QStringList & providerOf, QString & sig )
{
	QStringList out;
	QSet<QString> seen;
	for ( int r = 0; r < roots.size(); r++ ) {
		const QDir plugins( roots.at( r ) + QStringLiteral( "/Plugins" ) );
		if ( !plugins.exists() )
			continue;
		const QStringList providers = plugins.entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase );
		for ( const QString & p : providers ) {
			const QDir d( plugins.filePath( p ) + QLatin1Char( '/' ) + category );
			if ( !d.exists() )
				continue;
			const QFileInfoList fl = d.entryInfoList( { QStringLiteral( "*.recx" ) }, QDir::Files, QDir::Name | QDir::IgnoreCase );
			for ( const QFileInfo & f : fl ) {
				const QString rel = ( p + QLatin1Char( '/' ) + f.fileName() ).toLower();
				if ( seen.contains( rel ) )
					continue;
				seen.insert( rel );
				out << f.absoluteFilePath();
				providerOf << p;
				sig += QStringLiteral( "%1|%2|%3\n" ).arg( f.absoluteFilePath() ).arg( f.size() ).arg( f.lastModified().toMSecsSinceEpoch() );
			}
		}
	}
	return out;
}

void rescan( RecxCategory & c, const QStringList & roots, const QString & category )
{
	if ( c.checked.isValid() && c.roots == roots && c.checked.elapsed() < 500 )
		return;
	c.checked.start();
	QStringList providerOf;
	QString sig = roots.join( QLatin1Char( ';' ) ) + QLatin1Char( '\n' );
	const QStringList paths = listFiles( roots, category, providerOf, sig );
	if ( sig == c.signature )
		return;
	c.signature = sig;
	c.roots = roots;
	c.files.clear();
	QStringList lines;
	for ( int i = 0; i < paths.size(); i++ ) {
		QFile f( paths.at( i ) );
		if ( !f.open( QIODevice::ReadOnly ) ) {
			lines << QStringLiteral( "%1: unreadable" ).arg( paths.at( i ) );
			continue;
		}
		QJsonParseError err;
		const QJsonDocument doc = QJsonDocument::fromJson( f.readAll(), &err );
		if ( err.error != QJsonParseError::NoError || !doc.isObject() ) {
			lines << QStringLiteral( "%1: not JSON (%2 at %3)" ).arg( paths.at( i ), err.errorString() ).arg( err.offset );
			continue;
		}
		RecxFile rf;
		rf.path = paths.at( i );
		rf.provider = providerOf.at( i );
		for ( const QJsonValue & v : doc.object().value( QStringLiteral( "entries" ) ).toArray() )
			if ( v.isObject() )
				rf.entries << v.toObject();
		lines << QStringLiteral( "%1: %2 entries" ).arg( paths.at( i ) ).arg( rf.entries.size() );
		c.files << rf;
	}
	c.summary = QStringLiteral( "%1 file(s) under %2 root(s)\n" ).arg( c.files.size() ).arg( roots.size() ) + lines.join( QLatin1Char( '\n' ) );
	std::fprintf( stderr, "recx: %s: %s\n", qPrintable( category ), qPrintable( c.summary.section( QLatin1Char( '\n' ), 0, 0 ) ) );
	for ( const QString & l : std::as_const( lines ) )
		if ( !l.endsWith( QStringLiteral( "entries" ) ) )
			std::fprintf( stderr, "recx: %s\n", qPrintable( l ) );
}

// "Plugin.ext|0xLOCALID": the plugin and the id; false for anything else (a bare EditorID)
bool parseFormKey( const QString & k, QString & plugin, quint32 & id )
{
	const int bar = k.indexOf( QLatin1Char( '|' ) );
	if ( bar <= 0 )
		return false;
	plugin = k.left( bar ).trimmed();
	bool ok = false;
	id = k.mid( bar + 1 ).trimmed().toUInt( &ok, 0 );
	return ok;
}

// 0 form, 1 exact EditorID, 2 wildcard, -1 no claim
int rankOf( const QJsonObject & e, const QString & key, const QString & plugin, quint32 localId, const QString & edid )
{
	int best = -1;
	auto take = [&]( int r ) { if ( best < 0 || r < best ) best = r; };
	const QString rk = e.value( key ).toString();
	if ( !rk.isEmpty() ) {
		QString p;
		quint32 id = 0;
		if ( parseFormKey( rk, p, id ) ) {
			if ( p.compare( plugin, Qt::CaseInsensitive ) == 0 && id == localId )
				take( 0 );
		} else if ( rk.compare( edid, Qt::CaseInsensitive ) == 0 ) {
			take( 1 );
		}
	}
	const QString ek = e.value( QStringLiteral( "editorId" ) ).toString();
	if ( !ek.isEmpty() && !edid.isEmpty() ) {
		if ( ek.contains( QLatin1Char( '*' ) ) || ek.contains( QLatin1Char( '?' ) ) ) {
			const QRegularExpression re( QRegularExpression::wildcardToRegularExpression( ek ), QRegularExpression::CaseInsensitiveOption );
			if ( re.match( edid ).hasMatch() )
				take( 2 );
		} else if ( ek.compare( edid, Qt::CaseInsensitive ) == 0 ) {
			take( 1 );
		}
	}
	return best;
}

}	// namespace

QStringList wwRecxRoots( const QStringList & pluginFiles, const QString & gameData )
{
	const QByteArray pin = qgetenv( "WW_RECX_ROOT" );
	if ( !pin.isEmpty() )
		return { QDir::fromNativeSeparators( QString::fromLocal8Bit( pin ) ) };
	QStringList out;
	auto add = [&]( const QString & d ) {
		if ( d.isEmpty() )
			return;
		const QString c = QDir::cleanPath( QFileInfo( d ).absoluteFilePath() );
		for ( const QString & o : std::as_const( out ) )
			if ( o.compare( c, Qt::CaseInsensitive ) == 0 )
				return;
		out << c;
	};
	for ( int i = pluginFiles.size() - 1; i >= 0; i-- )
		add( QFileInfo( pluginFiles.at( i ) ).absolutePath() );
	add( gameData );
	const QString app = QCoreApplication::applicationDirPath();
	// the shipped files: beside the exe once installed, else the source tree's (one of the two: the same files twice
	// would claim every record twice)
	if ( QFileInfo( app + QStringLiteral( "/Data/Plugins" ) ).isDir() )
		add( app + QStringLiteral( "/Data" ) );
	else
		add( app + QStringLiteral( "/../res/Data" ) );
	return out;
}

WwRecxHit wwRecxFind( const QStringList & roots, const QStringList & pluginFiles, const QString & category,
	const QString & key, const QString & plugin, quint32 localId, const QString & edid, const QString & block )
{
	RecxCategory & c = cats()[category];
	rescan( c, roots, category );
	// the merge order: a provider named like a loaded plugin at that plugin's place, the others after, by name
	QHash<QString, int> plugOrder;
	for ( int i = 0; i < pluginFiles.size(); i++ )
		plugOrder.insert( QFileInfo( pluginFiles.at( i ) ).completeBaseName().toLower(), i );
	QStringList loose;
	for ( const RecxFile & f : std::as_const( c.files ) )
		if ( !plugOrder.contains( f.provider.toLower() ) && !loose.contains( f.provider.toLower() ) )
			loose << f.provider.toLower();
	std::sort( loose.begin(), loose.end() );
	auto orderOf = [&]( const QString & p ) {
		const QString l = p.toLower();
		return plugOrder.contains( l ) ? plugOrder.value( l ) : int( pluginFiles.size() ) + int( loose.indexOf( l ) );
	};
	struct Claim { int rank, order, entry; const RecxFile * f; };
	std::vector<Claim> claims;
	for ( const RecxFile & f : std::as_const( c.files ) )
		for ( int e = 0; e < f.entries.size(); e++ ) {
			if ( !f.entries.at( e ).value( block ).isObject() )
				continue;
			const int r = rankOf( f.entries.at( e ), key, plugin, localId, edid );
			if ( r >= 0 )
				claims.push_back( { r, orderOf( f.provider ), e, &f } );
		}
	WwRecxHit hit;
	if ( claims.empty() )
		return hit;
	// best rank first; at one rank the later provider; within one provider the first file by path, then entry
	std::sort( claims.begin(), claims.end(), []( const Claim & a, const Claim & b ) {
		if ( a.rank != b.rank )
			return a.rank < b.rank;
		if ( a.order != b.order )
			return a.order > b.order;
		const int pc = a.f->path.compare( b.f->path, Qt::CaseInsensitive );
		if ( pc != 0 )
			return pc < 0;
		return a.entry < b.entry;
	} );
	static const char * names[3] = { "form", "editorId", "wildcard" };
	const Claim & w = claims.front();
	hit.ok = true;
	hit.entry = w.f->entries.at( w.entry );
	hit.block = hit.entry.value( block ).toObject();
	hit.file = w.f->path;
	hit.provider = w.f->provider;
	hit.rung = QString::fromLatin1( names[w.rank] );
	static QSet<QString> logged;
	for ( size_t i = 1; i < claims.size(); i++ ) {
		const Claim & l = claims[i];
		hit.overridden << QStringLiteral( "%1 (%2)" ).arg( l.f->path, QString::fromLatin1( names[l.rank] ) );
		const bool clash = l.rank == w.rank && l.order == w.order;
		hit.conflict = hit.conflict || clash;
		const QString msg = QStringLiteral( "recx: %1 %2 of %3: %4 (%5, %6) over %7 (%8, %9)" )
			.arg( clash ? QStringLiteral( "CONFLICT" ) : QStringLiteral( "override" ), block, edid.isEmpty() ? plugin : edid,
				hit.file, hit.provider, hit.rung, l.f->path, l.f->provider, QString::fromLatin1( names[l.rank] ) );
		if ( !logged.contains( msg ) ) {
			logged.insert( msg );
			std::fprintf( stderr, "%s\n", qPrintable( msg ) );
		}
	}
	return hit;
}

QString wwRecxScanSummary( const QString & category )
{
	return cats().value( category ).summary;
}
