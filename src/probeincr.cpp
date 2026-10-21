/* lane BAKEINCR1 (2026-10-04): the incremental probe bake's record and check. See probeincr.h. */

#include "probeincr.h"

#include "cellview.h"
#include "esmdata.h"
#include "lodgen.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>
#include <QProcessEnvironment>
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>

namespace
{
std::atomic<bool> g_recording { false };
QMutex g_recMutex;
QMap<QString, ProbeIncr::Asset> g_rec;   // key: root|folder|ext|path, lower case

const float kCellUnits = 4096.0f;

QString sha1Hex( const QByteArray & b )
{
	return QString::fromLatin1( QCryptographicHash::hash( b, QCryptographicHash::Sha1 ).toHex() );
}

QString fileSha1( const QString & path )
{
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) )
		return QString();
	QCryptographicHash h( QCryptographicHash::Sha1 );
	if ( !h.addData( &f ) )
		return QString();
	return QString::fromLatin1( h.result().toHex() );
}

/* The knobs that are NOT a bake's identity: where it writes and how the run is driven
 * (paths, window, shot, settings scope, timers), what this lane's own driver sets, and
 * light and time (relight is a runtime pass). The resource stack and the data root are
 * not identity either: every file they resolve is content-hashed in the asset lines,
 * so a loose override dirties exactly the cells that read it, not the world. Anything
 * else WW_* is in: an unknown knob can only over-rebake. */
bool knobIsRunOnly( const QString & k )
{
	static const QSet<QString> exact {
		QStringLiteral( "WW_CELL_OPEN" ), QStringLiteral( "WW_CELL_DATAROOT" ),
		QStringLiteral( "WW_CELL_PROBES" ), QStringLiteral( "WW_CELL_PROBES_HIDE" ),
		QStringLiteral( "WW_CELL_PROBE_BAKE" ), QStringLiteral( "WW_CELL_PROBE_BAKE_DIR" ),
		QStringLiteral( "WW_CELL_PROBE_SOUP" ), QStringLiteral( "WW_CELL_PROBE_SOUP_REFS" ),
		QStringLiteral( "WW_SETTINGS_SCOPE" ), QStringLiteral( "WW_WINDOW_AT" ),
		QStringLiteral( "WW_LODGEN_RESOURCES" ), QStringLiteral( "WW_LODGEN_MO2" ),
		QStringLiteral( "WW_CELL_LIGHTS" ), QStringLiteral( "WW_CELL_FAR_LOD" ),
	};
	static const char * prefixes[] = { "WW_RENDER_", "WW_CELL_SPEED_", "WW_CELL_BAKEINCR_", "WW_LOOKDEV_",
		"WW_CELL_GI_", "WW_CELL_LIT" };
	if ( exact.contains( k ) )
		return true;
	for ( const char * p : prefixes )
		if ( k.startsWith( QLatin1String( p ) ) )
			return true;
	return false;
}

QByteArray exeSha1()
{
	static const QByteArray id = []() {
		const QString exe = QCoreApplication::instance() ? QCoreApplication::applicationFilePath() : QString();
		const QString s = exe.isEmpty() ? QString() : fileSha1( exe );
		return s.isEmpty() ? QByteArray( "unreadable" ) : s.toLatin1();
	}();
	return id;
}

struct Feeder
{
	QCryptographicHash h { QCryptographicHash::Sha1 };
	void raw( const void * p, int n ) { h.addData( QByteArrayView( static_cast<const char *>( p ), n ) ); }
	void i( qint64 v )
	{
		char buf[24];
		const int n = qsnprintf( buf, sizeof( buf ), "%lld;", static_cast<long long>( v ) );
		raw( buf, n );
	}
	void f( float v ) { raw( &v, 4 ); }   // raw bytes: a 0.001-unit nudge must move the digest
	void s( const QString & v ) { const QByteArray b = v.toLower().toUtf8(); raw( b.constData(), b.size() ); raw( "\x1f", 1 ); }
	QString hex() { return QString::fromLatin1( h.result().toHex() ); }
};

void feedSwap( Feeder & F, const EsmWorld & world, quint32 form )
{
	F.i( form );
	if ( !form )
		return;
	const EsmMaterialSwap & m = world.materialSwap( form );
	F.i( m.exists ? 1 : 0 );
	F.i( m.rows.size() );
	for ( const EsmMaterialSubst & r : m.rows ) {
		F.s( r.original );
		F.s( r.replacement );
		F.i( r.hasColorRemap ? 1 : 0 );
		F.f( r.colorRemap );
	}
}

void feedBase( Feeder & F, const EsmWorld & world, quint32 base )
{
	const EsmLodBase & b = world.lodBase( base );
	F.i( b.formID );
	F.i( b.type );
	for ( int k = 0; k < 4; k++ )
		F.s( b.models[k] );
	F.s( b.model );
	F.i( b.hasLod ? 1 : 0 );
	F.i( b.hasColorRemap ? 1 : 0 );
	F.f( b.colorRemap );
	F.i( b.recordFlags );
	F.i( b.hasDestructible ? 1 : 0 );
	F.i( b.waterType );
	F.f( b.trunkFlexibility );
	F.f( b.branchFlexibility );
	F.f( b.leafAmplitude );
	F.f( b.leafFrequency );
	feedSwap( F, world, b.materialSwap );
}

void feedRef( Feeder & F, const EsmWorld & world, const EsmRefr & r )
{
	F.i( r.formID );
	F.i( r.base );
	F.i( r.baseType );
	F.i( r.deleted ? 1 : 0 );
	F.i( r.initiallyDisabled ? 1 : 0 );
	F.i( r.layer );
	F.i( r.enableParent );
	F.i( r.enableParentOpposite ? 1 : 0 );
	F.i( r.teleport );
	F.i( r.hasPrim ? 1 : 0 );
	F.i( r.primType );
	for ( float v : r.primHalf )
		F.f( v );
	feedSwap( F, world, r.materialSwap );
	/* A LIGHT is light, not geometry: its radius, XLIG, light box and -- when its base
	 * has no model -- its very position are relight inputs, read at runtime. A LIGH with
	 * a model is a bulb or a lamp in the soup (cellview's soupRole), so then it is placed. */
	const bool light = std::memcmp( &r.baseType, "LIGH", 4 ) == 0;
	const EsmLodBase & b = world.lodBase( r.base );
	if ( light && b.model.isEmpty() ) {
		F.i( -1 );
		return;
	}
	for ( float v : r.pos )
		F.f( v );
	for ( float v : r.rot )
		F.f( v );
	F.f( r.scale );
	feedBase( F, world, r.base );
	const QVector<EsmScolPart> & parts = world.scolParts( r.base );
	F.i( parts.size() );
	for ( const EsmScolPart & pt : parts ) {
		feedBase( F, world, pt.base );
		F.i( pt.placements.size() );
		for ( const EsmScolPlacement & pl : pt.placements ) {
			for ( float v : pl.pos )
				F.f( v );
			for ( float v : pl.rot )
				F.f( v );
			F.f( pl.scale );
		}
	}
}

void feedLand( Feeder & F, const EsmWorld & world, int x, int y, QSet<quint32> & ltexSeen )
{
	EsmLand land;
	if ( !world.land( x, y, land ) || !land.valid ) {
		F.i( -2 );
		return;
	}
	F.raw( &land.heights[0][0], int( sizeof( land.heights ) ) );
	F.i( land.hasColors ? 1 : 0 );
	if ( land.hasColors )
		F.raw( &land.colors[0][0][0], int( sizeof( land.colors ) ) );
	auto ltex = [&]( quint32 form ) {
		F.i( form );
		if ( !form || ltexSeen.contains( form ) )
			return;
		ltexSeen.insert( form );
		const EsmLtexTextureSet & ts = world.ltexTextureSet( form );
		F.i( ts.exists ? 1 : 0 );
		F.s( ts.diffuse );
		F.s( ts.normal );
		F.s( ts.specular );
		F.s( ts.material );
	};
	for ( int q = 0; q < 4; q++ ) {
		ltex( land.baseTex[q] );
		F.i( land.layers[q].size() );
		for ( const EsmLandLayer & L : land.layers[q] ) {
			ltex( L.ltex );
			F.raw( L.opacity, int( sizeof( L.opacity ) ) );
		}
	}
}

bool byForm( const EsmRefr & a, const EsmRefr & b ) { return a.formID < b.formID; }

QString assetKey( const QString & root, const QString & folder, const QString & ext, const QString & path )
{
	return ( root + QLatin1Char( '|' ) + folder + QLatin1Char( '|' ) + ext + QLatin1Char( '|' ) + path ).toLower();
}
} // namespace

namespace ProbeIncr
{
QByteArray red()
{
	static const QByteArray r = qgetenv( "WW_CELL_BAKEINCR_RED" );
	return r;
}

void startRecording()
{
	QMutexLocker lock( &g_recMutex );
	g_rec.clear();
	g_recording = true;
}

void stopRecording() { g_recording = false; }

bool recording() { return g_recording.load(); }

void note( const QString & root, const QString & relPath, const char * folder, const char * ext, bool ok,
	const QByteArray & bytes )
{
	if ( !g_recording.load() || relPath.isEmpty() )
		return;
	const QString f = QString::fromLatin1( folder ? folder : "" );
	const QString e = QString::fromLatin1( ext ? ext : "" );
	const QString key = assetKey( root, f, e, relPath );
	{
		QMutexLocker lock( &g_recMutex );
		if ( g_rec.contains( key ) )
			return;
	}
	Asset a;
	a.root = root;
	a.folder = f;
	a.ext = e;
	a.relPath = relPath;
	a.digest = ok && !bytes.isEmpty() ? sha1Hex( bytes ) : QStringLiteral( "missing" );
	QMutexLocker lock( &g_recMutex );
	if ( !g_rec.contains( key ) )
		g_rec.insert( key, a );
}

QVector<Asset> recorded()
{
	QMutexLocker lock( &g_recMutex );
	QVector<Asset> out;
	out.reserve( g_rec.size() );
	for ( auto it = g_rec.constBegin(); it != g_rec.constEnd(); ++it )
		out.append( it.value() );
	return out;
}

QStringList identityKnobs()
{
	QStringList out;
	const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	for ( const QString & k : env.keys() ) {
		if ( !k.startsWith( QLatin1String( "WW_" ) ) || knobIsRunOnly( k ) )
			continue;
		out.append( k + QLatin1Char( '=' ) + env.value( k ) );
	}
	out.sort();
	return out;
}

QString identity( const QString & world )
{
	Feeder F;
	F.raw( "bakeincr identity v1;", 21 );
	const QByteArray e = exeSha1();
	F.raw( e.constData(), e.size() );
	F.s( world );
	for ( const QString & k : identityKnobs() ) {
		const QByteArray b = k.toUtf8();   // values are case-sensitive: fed as typed
		F.raw( b.constData(), b.size() );
		F.raw( "\x1e", 1 );
	}
	return F.hex();
}

QString recordsDigest( const EsmWorld & world, int cx, int cy, int half )
{
	Feeder F;
	F.raw( "bakeincr records v1;", 20 );
	F.i( cx );
	F.i( cy );
	F.i( half );
	F.i( world.defaultWaterType() );
	QSet<quint32> ltexSeen;
	QSet<quint32> seen;
	/* LAND reaches one cell past the block (lodgen's ring rule): the painted ground and
	 * its colour sheet may sample a neighbour's edge. Everything else is the block. */
	for ( int y = cy - half - 1; y <= cy + half + 1; y++ ) {
		for ( int x = cx - half - 1; x <= cx + half + 1; x++ ) {
			F.i( x );
			F.i( y );
			feedLand( F, world, x, y, ltexSeen );
			if ( std::abs( x - cx ) > half || std::abs( y - cy ) > half )
				continue;
			const bool has = world.hasCell( x, y );
			F.i( has ? 1 : 0 );
			if ( !has )
				continue;
			F.i( world.cellForm( x, y ) );
			float wh = 0.0f;
			quint32 wt = 0;
			const bool water = world.cellWater( x, y, wh, &wt );
			F.i( water ? 1 : 0 );
			F.f( wh );
			F.i( wt );
			QVector<EsmRefr> refs = world.refrs( x, y );
			std::sort( refs.begin(), refs.end(), byForm );
			F.i( refs.size() );
			for ( const EsmRefr & r : refs ) {
				seen.insert( r.formID );
				feedRef( F, world, r );
			}
		}
	}
	// the worldspace's persistent refs, by position, exactly the bake's rectangle
	QVector<EsmRefr> pers = world.persistentRefrsIn( float( cx - half ) * kCellUnits, float( cy - half ) * kCellUnits,
		float( cx + half + 1 ) * kCellUnits, float( cy + half + 1 ) * kCellUnits );
	std::sort( pers.begin(), pers.end(), byForm );
	F.raw( "persistent;", 11 );
	for ( const EsmRefr & r : pers ) {
		if ( seen.contains( r.formID ) )
			continue;
		feedRef( F, world, r );
	}
	return F.hex();
}

bool writeRecord( const QString & dir, const QString & world, int cx, int cy, int half,
	const QString & records, const QString & cellRecords, const QString & far, QString * error )
{
	QStringList lines;
	lines << QStringLiteral( "WWBAKEINCR 1" );
	lines << QStringLiteral( "cell %1 %2" ).arg( cx ).arg( cy );
	lines << QStringLiteral( "half %1" ).arg( half );
	lines << QStringLiteral( "identity %1" ).arg( identity( world ) );
	for ( const QString & k : identityKnobs() )
		lines << QStringLiteral( "knob %1" ).arg( k );
	lines << QStringLiteral( "records %1" ).arg( records );
	lines << QStringLiteral( "cellrecords %1" ).arg( cellRecords );   // the asked cell alone: the nonbr red's
	lines << QStringLiteral( "far %1" ).arg( far );
	for ( const Asset & a : recorded() )
		lines << QStringLiteral( "asset %1\t%2\t%3\t%4\t%5" ).arg( a.digest, a.root, a.folder, a.ext, a.relPath );
	const QStringList tbk = QDir( dir ).entryList( { QStringLiteral( "*.tbk" ) }, QDir::Files, QDir::Name );
	if ( tbk.isEmpty() ) {
		if ( error )
			*error = QStringLiteral( "no .tbk in %1" ).arg( dir );
		return false;
	}
	for ( const QString & n : tbk )
		lines << QStringLiteral( "out %1\t%2" ).arg( fileSha1( QDir( dir ).filePath( n ) ), n );
	QFile f( QDir( dir ).filePath( QString::fromLatin1( recordName() ) ) );
	if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
		if ( error )
			*error = f.errorString();
		return false;
	}
	f.write( lines.join( QLatin1Char( '\n' ) ).toUtf8() + "\n" );
	return true;
}

/* THE CHECK. WW_CELL_BAKEINCR_CHECK="<plugins>|<world>|x0,y0,x1,y1|<root>|<verdict file>".
 * Each cell's record is read from <root>/cell_<x>_<y>/bakeincr.txt and every line is
 * recomputed; the first that moved is the cell's reason. The verdict file holds one line
 * per cell ("cell X Y clean" / "cell X Y dirty <reason>") and the census line
 * "incremental: N of M cells dirty (...)". The cheap tests run first; the far soup
 * (a ring read from the LOD files) only for a cell nothing else has dirtied. */
int checkMain()
{
	QElapsedTimer clock;
	clock.start();
	const QStringList p = qEnvironmentVariable( "WW_CELL_BAKEINCR_CHECK" ).split( QLatin1Char( '|' ) );
	if ( p.size() != 5 ) {
		qWarning( "WW_CELL_BAKEINCR_CHECK wants <plugins>|<world>|x0,y0,x1,y1|<root>|<verdict file>" );
		return 2;
	}
	const QString plugins = p[0].trimmed(), worldName = p[1].trimmed(), root = p[3].trimmed();
	const QStringList r = p[2].split( QLatin1Char( ',' ) );
	QFile vf( p[4].trimmed() );
	if ( r.size() != 4 || !vf.open( QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text ) ) {
		qWarning( "WW_CELL_BAKEINCR_CHECK: bad rectangle or verdict file" );
		return 2;
	}
	QTextStream out( &vf );
	const int x0 = std::min( r[0].toInt(), r[2].toInt() ), x1 = std::max( r[0].toInt(), r[2].toInt() );
	const int y0 = std::min( r[1].toInt(), r[3].toInt() ), y1 = std::max( r[1].toInt(), r[3].toInt() );

	QString wsError;
	quint32 wsForm = 0;
	for ( const QPair<quint32, QString> & w : EsmWorld::listWorldspaces( plugins, &wsError ) )
		if ( w.second.compare( worldName, Qt::CaseInsensitive ) == 0 )
			wsForm = w.first;
	EsmWorld world;
	QString loadError;
	if ( !wsForm || !world.load( plugins, wsForm, &loadError ) ) {
		out << "error: cannot load " << worldName << " from " << plugins << ": " << wsError << loadError << "\n";
		return 3;
	}
	// the bake's own data-root rule (cellview.cpp): the spec's, else the stack's first entry
	QString dataRoot = qEnvironmentVariable( "WW_CELL_DATAROOT" );
	if ( dataRoot.isEmpty() ) {
		const QStringList stack = lodgenResourceSearchPaths();
		if ( !stack.isEmpty() )
			dataRoot = stack.first();
	}
	const QString idNow = identity( worldName );
	QHash<QString, QString> assetNow;   // the same file is asked by many cells
	QMap<QString, int> reasons;
	int dirty = 0, total = 0;
	for ( int y = y0; y <= y1; y++ ) {
		for ( int x = x0; x <= x1; x++ ) {
			total++;
			const QString cellDir = QDir( root ).filePath( QStringLiteral( "cell_%1_%2" ).arg( x ).arg( y ) );
			QFile rf( QDir( cellDir ).filePath( QString::fromLatin1( recordName() ) ) );
			QString why;
			QString records, cellRecords, far;
			int half = -1;
			QStringList assets, outs;
			if ( !rf.open( QIODevice::ReadOnly ) ) {
				why = QStringLiteral( "never baked" );
			} else {
				const QStringList lines = QString::fromUtf8( rf.readAll() ).split( QLatin1Char( '\n' ), Qt::SkipEmptyParts );
				QString idThen;
				for ( const QString & l : lines ) {
					if ( l.startsWith( QLatin1String( "identity " ) ) ) idThen = l.mid( 9 );
					else if ( l.startsWith( QLatin1String( "records " ) ) ) records = l.mid( 8 );
					else if ( l.startsWith( QLatin1String( "cellrecords " ) ) ) cellRecords = l.mid( 12 );
					else if ( l.startsWith( QLatin1String( "far " ) ) ) far = l.mid( 4 );
					else if ( l.startsWith( QLatin1String( "half " ) ) ) half = l.mid( 5 ).toInt();
					else if ( l.startsWith( QLatin1String( "asset " ) ) ) assets << l.mid( 6 );
					else if ( l.startsWith( QLatin1String( "out " ) ) ) outs << l.mid( 4 );
				}
				if ( lines.isEmpty() || lines.first() != QLatin1String( "WWBAKEINCR 1" ) || half < 0 )
					why = QStringLiteral( "record unreadable" );
				else if ( idThen != idNow )
					why = QStringLiteral( "identity (exe or knobs)" );
			}
			if ( why.isEmpty() ) {
				for ( const QString & o : outs ) {
					const int t = o.indexOf( QLatin1Char( '\t' ) );
					if ( t < 0 || fileSha1( QDir( cellDir ).filePath( o.mid( t + 1 ) ) ) != o.left( t ) ) {
						why = QStringLiteral( "output lost or edited" );
						break;
					}
				}
				if ( outs.isEmpty() )
					why = QStringLiteral( "output lost or edited" );
			}
			/* the red nonbr: the neighbour rule off -- the asked cell's own records alone */
			if ( why.isEmpty() && ( red() == "nonbr" ? recordsDigest( world, x, y, 0 ) != cellRecords
				: recordsDigest( world, x, y, half ) != records ) )
				why = QStringLiteral( "records" );
			if ( why.isEmpty() ) {
				for ( const QString & a : assets ) {
					// digest \t root \t folder \t ext \t path
					const QStringList c = a.split( QLatin1Char( '\t' ) );
					if ( c.size() != 5 ) {
						why = QStringLiteral( "record unreadable" );
						break;
					}
					// the red nomodels: the fingerprint ignores models
					if ( red() == "nomodels" && c[3].compare( QLatin1String( ".nif" ), Qt::CaseInsensitive ) == 0 )
						continue;
					const QString key = assetKey( c[1], c[2], c[3], c[4] );
					auto it = assetNow.constFind( key );
					if ( it == assetNow.constEnd() ) {
						QByteArray bytes;
						const QByteArray folder = c[2].toLatin1(), ext = c[3].toLatin1();
						const bool ok = lodgenReadAssetBytes( c[1], c[4], folder.constData(), ext.constData(), bytes );
						it = assetNow.insert( key, ok && !bytes.isEmpty() ? sha1Hex( bytes ) : QStringLiteral( "missing" ) );
					}
					if ( it.value() != c[0] ) {
						why = QStringLiteral( "asset " ) + c[4];
						break;
					}
				}
			}
			if ( why.isEmpty() && cellBakeFarDigest( worldName, dataRoot, x, y, half ) != far )
				why = QStringLiteral( "far" );
			if ( why.isEmpty() ) {
				out << "cell " << x << " " << y << " clean\n";
			} else {
				dirty++;
				reasons[why.startsWith( QLatin1String( "asset " ) ) ? QStringLiteral( "asset" ) : why]++;
				out << "cell " << x << " " << y << " dirty " << why << "\n";
			}
		}
	}
	QStringList rs;
	for ( auto it = reasons.constBegin(); it != reasons.constEnd(); ++it )
		rs << QStringLiteral( "%1 %2" ).arg( it.value() ).arg( it.key() );
	out << "incremental: " << dirty << " of " << total << " cells dirty ("
		<< ( rs.isEmpty() ? QStringLiteral( "none" ) : rs.join( QLatin1String( ", " ) ) ) << "), check "
		<< clock.elapsed() << " ms" << ( red().isEmpty() ? QString() : QStringLiteral( " RED " ) + QString::fromLatin1( red() ) )
		<< "\n";
	return 0;
}
} // namespace ProbeIncr
