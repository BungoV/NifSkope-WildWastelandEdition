// lane ESPWRITE2 (2026-11-02): cell edits saved as an override plugin (src/celledit.h)

#include "celledit.h"
#include "cellsurface.h"

#include "cellpick.h"
#include "cellview.h"
#include "cellworkspace.h"
#include "esmdata.h"
#include "glview.h"
#include "io/espwrite.h"
#include "ui/opencellcatalog.h"
#include "wwswitch.h"
#include "gl/glscene.h"
#include "gl/glshape.h"
#include "gl/renderer.h"
#include "esmfile.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QKeyEvent>
#include <QSet>
#include <QTextStream>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

using namespace EspWrite;

namespace {

constexpr float kPi = 3.14159265358979323846f;
QStringList g_lastWarnings;	// ESPWRITE3B: the last plan's warnings (cellEditWarnings)

QString baseName( QString p )
{
	p.replace( QChar( 92 ), QChar( '/' ) );
	return p.mid( p.lastIndexOf( QChar( '/' ) ) + 1 ).trimmed();
}

int indexOfName( const QStringList & list, const QString & name )
{
	for ( int i = 0; i < list.size(); i++ )
		if ( baseName( list[i] ).compare( name, Qt::CaseInsensitive ) == 0 )
			return i;
	return -1;
}

QStringList orderOf( const QString & plugins )
{
	QStringList out;
	for ( const QString & p : plugins.split( QChar( ',' ), Qt::SkipEmptyParts ) )
		if ( !p.trimmed().isEmpty() )
			out.append( p.trimmed() );
	return out;
}

QString hex8( quint32 v )
{
	return QStringLiteral( "%1" ).arg( v, 8, 16, QLatin1Char( '0' ) ).toUpper();
}

bool fail( QString * error, const QString & m )
{
	if ( error )
		*error = m;
	return false;
}

float rd( const QByteArray & b, int off )
{
	float f = 0.0f;
	if ( off + 4 <= b.size() )
		memcpy( &f, b.constData() + off, 4 );
	return f;
}

quint32 ru32( const QByteArray & b, int off )
{
	quint32 v = 0;
	if ( off + 4 <= b.size() )
		memcpy( &v, b.constData() + off, 4 );
	return v;
}

// ------------------------------------------------------------------ EditorID index
struct BaseInfo
{
	quint32 form = 0;
	QString sig;
	float lo[3] = { -16, -16, -16 }, hi[3] = { 16, 16, 16 };
};

bool isBaseSig( const char * s )
{
	static const char * const kSigs[] = { "STAT", "MSTT", "MISC", "WEAP", "ARMO", "AMMO", "BOOK", "FURN", "ACTI",
		"LIGH", "CONT", "DOOR", "FLOR", "TREE", "SCOL", "IDLM", "NOTE", "KEYM", "ALCH", "TACT", "TERM", "PKIN",
		"BNDS", "SOUN", "TXST", "ADDN", "HAZD", "ASPC", "CMPO", "INGR" };
	for ( const char * k : kSigs )
		if ( memcmp( k, s, 4 ) == 0 )
			return true;
	return false;
}

QString indexKey( const QStringList & plugins )
{
	QString k;
	for ( const QString & p : plugins )
		k += p.toLower() + QLatin1Char( ':' ) + QString::number( QFileInfo( p ).size() ) + QLatin1Char( ',' );
	return k;
}

QHash<QString, BaseInfo> & edidIndex( const QStringList & plugins, QString * error )
{
	static QHash<QString, QHash<QString, BaseInfo>> cache;
	const QString key = indexKey( plugins );
	auto it = cache.find( key );
	if ( it != cache.end() )
		return *it;
	QHash<QString, BaseInfo> & out = cache[key];
	try {
		ESMFile esm( plugins.join( QLatin1Char( ',' ) ).toLocal8Bit().constData() );
		const ESMFile::ESMRecord * r0 = esm.findRecord( 0U );
		unsigned int id = r0 ? r0->next : 0U;
		while ( id ) {
			const ESMFile::ESMRecord * g = esm.findRecord( id );
			if ( !g )
				break;
			char lab[4];
			memcpy( lab, &g->flags, 4 );
			if ( *g == "GRUP" && g->formID == 0U && isBaseSig( lab ) ) {
				for ( unsigned int c = g->children; c; ) {
					const ESMFile::ESMRecord * r = esm.findRecord( c );
					if ( !r )
						break;
					if ( !( *r == "GRUP" ) ) {
						BaseInfo bi;
						bi.form = r->formID;
						bi.sig = QString::fromLatin1( lab, 4 );
						QString edid;
						ESMFile::ESMField f( esm, *r );
						while ( f.next() ) {
							if ( f == "EDID" ) {
								const char * p = reinterpret_cast<const char *>( f.data() );
								size_t n = f.size();
								while ( n > 0 && p[n - 1] == '\0' )
									n--;
								edid = QString::fromLatin1( p, qsizetype( n ) );
							} else if ( f == "OBND" && f.size() >= 12 ) {
								for ( int i = 0; i < 3; i++ )
									bi.lo[i] = float( f.readInt16() );
								for ( int i = 0; i < 3; i++ )
									bi.hi[i] = float( f.readInt16() );
							}
						}
						const QString k = edid.toLower();
						if ( !k.isEmpty() && !out.contains( k ) )
							out.insert( k, bi );
					}
					c = r->next;
				}
			}
			id = g->next;
		}
	} catch ( std::exception & e ) {
		if ( error )
			*error = QString::fromLatin1( e.what() );
		cache.remove( key );
		static QHash<QString, BaseInfo> empty;
		empty.clear();
		return empty;
	}
	return out;
}

// ------------------------------------------------------------------ the session's extras (not in the header)
struct Extra
{
	GLView * view = nullptr;
	Scene * scene = nullptr;
	struct Action
	{
		enum Kind { Hover, Anchor, Face, Commit } kind = Hover;
		double x = 0, y = 0;
	};
	std::vector<Action> queue;
	bool hoverPending = false;
	double hoverX = 0, hoverY = 0;
	bool lmbDown = false;
	double pressX = 0, pressY = 0;
	bool dragging = false;
	QVector<CellEditItem> lastSaved;
	QString lastReopen;
	QString lastLine;
	CellSurfaceHit lastHit;
	// the script
	bool scriptLoaded = false;
	QStringList script;
	int scriptAt = 0;
	int waitFrames = 0;
	bool waitReopen = false;
	int reopenFrames = 0;
	quint32 scriptRef = 0;      //!< pickref: the ref move / turn / scale / disable change
	QString logPath;
	int probeLogs = 0;
};

Extra & extra()
{
	static Extra e;
	return e;
}

void logLine( const QString & s )
{
	Extra & x = extra();
	if ( x.logPath.isEmpty() )
		return;
	QFile f( x.logPath );
	if ( f.open( QIODevice::WriteOnly | QIODevice::Append ) ) {
		f.write( s.toUtf8() );
		f.write( "\n" );
	}
}

void say( const QString & text, bool refusal )
{
	extra().lastLine = text;
	logLine( ( refusal ? QStringLiteral( "REFUSED " ) : QStringLiteral( "status " ) ) + text );
	emit CellEditBus::instance()->status( text, refusal );
}

void changed()
{
	emit CellEditBus::instance()->changed();
}

} // namespace

// ================================================================== the core
QStringList cellEditWarnings()
{
	return g_lastWarnings;
}

QString cellEditRed()
{
	return QString::fromLocal8Bit( qgetenv( "WW_CELLEDIT_RED" ) ).trimmed().toLower();
}

bool cellEditEnabled()
{
	return wwEnvStr( "WW_CELL_EDIT" ).trimmed() != QLatin1String( "0" );
}

bool cellEditResolveEdid( const QStringList & plugins, const QString & edid, quint32 & loForm, QString & sig,
	float lo[3], float hi[3], QString * error )
{
	if ( plugins.isEmpty() )
		return fail( error, QStringLiteral( "no load order: open a cell first" ) );
	if ( edid.trimmed().isEmpty() )
		return fail( error, QStringLiteral( "no EditorID" ) );
	QString e;
	const QHash<QString, BaseInfo> & idx = edidIndex( plugins, &e );
	if ( !e.isEmpty() )
		return fail( error, e );
	auto it = idx.constFind( edid.trimmed().toLower() );
	if ( it == idx.constEnd() )
		return fail( error, QStringLiteral( "%1 is no placeable base form in this load order" ).arg( edid.trimmed() ) );
	loForm = it->form;
	sig = it->sig;
	for ( int i = 0; i < 3; i++ ) {
		lo[i] = it->lo[i];
		hi[i] = it->hi[i];
	}
	return true;
}

void cellEditMatrix( const float rot[3], float m[3][3], bool redGhost )
{
	const float s = redGhost ? 1.0f : -1.0f;
	const float x = s * rot[0], y = s * rot[1], z = s * rot[2];
	const float sX = std::sin( x ), cX = std::cos( x ), sY = std::sin( y ), cY = std::cos( y ), sZ = std::sin( z ),
		cZ = std::cos( z );
	m[0][0] = cY * cZ;
	m[0][1] = -cY * sZ;
	m[0][2] = sY;
	m[1][0] = sX * sY * cZ + sZ * cX;
	m[1][1] = cX * cZ - sX * sY * sZ;
	m[1][2] = -sX * cY;
	m[2][0] = sX * sZ - cX * sY * cZ;
	m[2][1] = cX * sY * sZ + sX * cZ;
	m[2][2] = cX * cY;
}

void cellEditPose( float yaw, bool flat, const float normal[3], float rot[3] )
{
	rot[0] = rot[1] = 0.0f;
	rot[2] = yaw;
	if ( !flat )
		return;
	double u[3] = { normal[0], normal[1], normal[2] };
	double ul = std::sqrt( u[0] * u[0] + u[1] * u[1] + u[2] * u[2] );
	if ( !( ul > 1.0e-6 ) )
		return;
	for ( double & v : u )
		v /= ul;
	double f[3] = { std::sin( yaw ), std::cos( yaw ), 0.0 };
	double d = f[0] * u[0] + f[1] * u[1] + f[2] * u[2];
	for ( int i = 0; i < 3; i++ )
		f[i] -= d * u[i];
	double fl = std::sqrt( f[0] * f[0] + f[1] * f[1] + f[2] * f[2] );
	if ( !( fl > 1.0e-6 ) ) {	// the surface faces along the heading: keep +Y in the plane
		f[0] = 0.0;
		f[1] = u[2];
		f[2] = -u[1];
		fl = std::sqrt( f[1] * f[1] + f[2] * f[2] );
		if ( !( fl > 1.0e-6 ) ) {
			f[0] = 1.0;
			f[1] = f[2] = 0.0;
			fl = 1.0;
		}
	}
	for ( double & v : f )
		v /= fl;
	const double r[3] = { f[1] * u[2] - f[2] * u[1], f[2] * u[0] - f[0] * u[2], f[0] * u[1] - f[1] * u[0] };
	// columns r, f, u: M = fromEuler( x, y, z ) and the REFR stores -( x, y, z )
	const double m02 = u[0], m12 = u[1], m22 = u[2], m01 = f[0], m00 = r[0];
	const double x = std::atan2( -m12, m22 );
	const double y = std::asin( std::max( -1.0, std::min( 1.0, m02 ) ) );
	const double z = std::atan2( -m01, m00 );
	rot[0] = float( -x );
	rot[1] = float( -y );
	rot[2] = float( -z );
}

float cellEditYawToward( const float from[3], const float to[3] )
{
	float r[3];
	yawToward( from, to, r );
	return r[2];
}

void cellEditGhostBox( const CellEditItem & it, float lo[3], float hi[3], float tip[3], bool redGhost )
{
	float m[3][3];
	cellEditMatrix( it.rot, m, redGhost );
	for ( int i = 0; i < 3; i++ ) {
		lo[i] = 1.0e30f;
		hi[i] = -1.0e30f;
	}
	for ( int c = 0; c < 8; c++ ) {
		const float p[3] = { ( c & 1 ) ? it.hi[0] : it.lo[0], ( c & 2 ) ? it.hi[1] : it.lo[1],
			( c & 4 ) ? it.hi[2] : it.lo[2] };
		for ( int i = 0; i < 3; i++ ) {
			const float w = it.pos[i] + it.scale * ( m[i][0] * p[0] + m[i][1] * p[1] + m[i][2] * p[2] );
			lo[i] = std::min( lo[i], w );
			hi[i] = std::max( hi[i], w );
		}
	}
	for ( int i = 0; i < 3; i++ )
		tip[i] = it.pos[i] + m[i][1] * 48.0f;
}

QString cellEditGhostLine( const CellEditItem & it )
{
	float lo[3], hi[3], tip[3];
	cellEditGhostBox( it, lo, hi, tip, cellEditRed() == QLatin1String( "ghost" ) );
	auto f = []( float v ) { return QString::number( double( v ), 'f', 3 ); };
	return QStringLiteral( "ghost base %1 pos %2 %3 %4 rot %5 %6 %7 scale %8 obnd %9 %10 %11 %12 %13 %14 aabb %15 %16 %17 "
		"%18 %19 %20 tip %21 %22 %23" )
		.arg( hex8( it.base ) ).arg( f( it.pos[0] ), f( it.pos[1] ), f( it.pos[2] ) )
		.arg( QString::number( double( it.rot[0] ), 'f', 6 ), QString::number( double( it.rot[1] ), 'f', 6 ),
			QString::number( double( it.rot[2] ), 'f', 6 ) )
		.arg( f( it.scale ) )
		.arg( f( it.lo[0] ), f( it.lo[1] ), f( it.lo[2] ), f( it.hi[0] ), f( it.hi[1] ), f( it.hi[2] ) )
		.arg( f( lo[0] ), f( lo[1] ), f( lo[2] ), f( hi[0] ), f( hi[1] ), f( hi[2] ) )
		.arg( f( tip[0] ), f( tip[1] ), f( tip[2] ) );
}

// ------------------------------------------------------------------ the plan
namespace {

//! The plugins a plan copies from, opened once each.
struct Sources
{
	QStringList order;
	QHash<QString, std::shared_ptr<PluginIndex>> open;
	PluginIndex * get( const QString & name, QString * error )
	{
		const QString k = name.toLower();
		auto it = open.find( k );
		if ( it != open.end() )
			return it->get();
		const int i = indexOfName( order, name );
		if ( i < 0 ) {
			fail( error, QStringLiteral( "%1 is not in the load order" ).arg( name ) );
			return nullptr;
		}
		auto p = std::make_shared<PluginIndex>();
		if ( !p->open( order[i], error ) )
			return nullptr;
		open.insert( k, p );
		return p.get();
	}
};

//! A load-order form in a plugin's own numbering (its masters, then itself).
bool fileForm( const ESMFile * esm, const QStringList & order, quint32 lo, const QString & fileName,
	const QStringList & fileMasters, quint32 & out, QString * plugin )
{
	const unsigned int fi = esm->getFormIDFile( lo );
	if ( fi >= unsigned( order.size() ) )
		return false;
	const QString n = baseName( order[int( fi )] );
	if ( plugin )
		*plugin = n;
	int idx = indexOfName( fileMasters, n );
	if ( idx < 0 && n.compare( fileName, Qt::CaseInsensitive ) == 0 )
		idx = int( fileMasters.size() );
	if ( idx < 0 )
		return false;
	out = ( quint32( idx ) << 24 ) | ESMFile::getFormIDLocal( lo );
	return true;
}

//! The file being extended ("Add to existing"): its masters, flag, next id and every cell edit it holds.
struct Carry
{
	bool ok = false;
	QStringList masters;
	bool light = false;
	quint32 next = 0;
	QVector<CellEdit> cells;
	QHash<quint32, int> interiorAt;     //!< cell form -> cells index
	QHash<quint32, QByteArray> worldRaw, pcellRaw;
	int ownRefs = 0, overrides = 0, foreign = 0;
	// ESPWRITE3B espmerge: what else the file holds, written back as it was
	QVector<EspWrite::Node> keepTop;
	QByteArray snam, tail;
	QString author;
	quint32 intv = 1;
};

bool addRefs( const EspWrite::Node& g6, CellEdit & ce, quint32 nm, Carry & c, QString * error )
{
	for ( const EspWrite::Node& g : g6.kids ) {
		if ( !g.isGroup || ( g.type != 8 && g.type != 9 ) )
			return fail( error, QStringLiteral( "Add to existing: a cell's children hold group type %1" ).arg( g.type ) );
		for ( const EspWrite::Node& r : g.kids ) {
			if ( r.isGroup )
				return fail( error, QStringLiteral( "Add to existing: a nested group under a cell's refs" ) );
			const QByteArray sig = recordSig( r.raw );
			const quint32 form = recordForm( r.raw );
			bool simple = ( form >> 24 ) == nm && sig == "REFR";
			RefAdd a;
			if ( simple ) {
				QByteArray payload;
				QVector<Sub> subs;
				if ( !recordPayload( r.raw, payload, error ) || !splitSubs( payload, subs, error ) )
					return false;
				bool haveData = false;
				for ( const Sub & s : subs ) {
					if ( s.tag == "NAME" && s.data.size() == 4 )
						a.base = ru32( s.data, 0 );
					else if ( s.tag == "XSCL" && s.data.size() == 4 )
						a.scale = rd( s.data, 0 );
					else if ( s.tag == "DATA" && s.data.size() == 24 ) {
						for ( int i = 0; i < 3; i++ ) {
							a.pos[i] = rd( s.data, 4 * i );
							a.rot[i] = rd( s.data, 12 + 4 * i );
						}
						haveData = true;
					} else
						simple = false;
				}
				// re-made byte-identical only when this writer made it: same flags, tail and subrecord set
				simple = simple && a.base && haveData && r.raw.mid( 16, 8 ) == newRecordTail()
					&& !( recordFlags( r.raw ) & ~( kRefPersistent | kRefDisabled ) );
			}
			if ( simple ) {	// a ref this panel wrote: re-made, so a change can move it
				const quint32 fl = recordFlags( r.raw );
				a.persistent = ( fl & kRefPersistent ) != 0;
				a.disabled = ( fl & kRefDisabled ) != 0;
				a.keepForm = form;
				ce.adds.append( a );
				c.ownRefs++;
			} else {	// ESPWRITE3B espmerge: anything else is carried verbatim (an override, or another tool's record)
				RefOverride o;
				o.rec.raw = r.raw;
				o.rec.ours = true;
				o.persistent = g.type == 8;
				ce.overrides.append( o );
				( sig == "REFR" || sig == "ACHR" ? c.overrides : c.foreign )++;
			}
		}
	}
	return true;
}

bool readCarry( const QString & path, Carry & c, QString * error )
{
	PluginIndex idx;
	if ( !idx.open( path, error ) )
		return false;
	c.masters = idx.masters();
	c.light = ( idx.tes4Flags() & kTes4Light ) != 0;
	if ( idx.tes4Flags() & kTes4Localized )
		return fail( error, QStringLiteral( "Add to existing: %1 is localised (its strings live in .strings files)" ).arg( baseName( path ) ) );
	const QByteArray & bytes = idx.bytes();
	{
		QByteArray payload;
		QVector<Sub> subs;
		if ( !recordPayload( bytes.left( idx.tes4End() ), payload, error ) || !splitSubs( payload, subs, error ) )
			return false;
		for ( const Sub & s : subs ) {
			if ( s.data.size() > 0xFFFF )
				return fail( error, QStringLiteral( "Add to existing: %1's header holds a %2 past 64 KiB" ).arg( baseName( path ),
					QString::fromLatin1( s.tag ) ) );
			if ( s.tag == "HEDR" && s.data.size() >= 12 )
				c.next = ru32( s.data, 8 );
			else if ( s.tag == "CNAM" )
				c.author = QString::fromLatin1( s.data.constData(), int( qstrnlen( s.data.constData(), uint( s.data.size() ) ) ) );
			else if ( s.tag == "SNAM" )
				c.snam += sub( "SNAM", s.data );
			else if ( s.tag == "SCRN" || s.tag == "TNAM" )
				c.tail += sub( s.tag.constData(), s.data );
			else if ( s.tag == "INTV" && s.data.size() == 4 )
				c.intv = ru32( s.data, 0 );
		}
	}
	QVector<EspWrite::Node> top;
	if ( !parseNodes( bytes, idx.tes4End(), bytes.size(), top, error ) )
		return false;
	const quint32 nm = quint32( c.masters.size() );
	auto ours = []( const QByteArray & raw ) {
		Copied k;
		k.raw = raw;
		k.ours = true;
		return k;
	};
	for ( const EspWrite::Node& t : top ) {
		if ( !t.isGroup || t.type != 0 )
			return fail( error, QStringLiteral( "Add to existing: a top-level record outside any group" ) );
		if ( t.label == label4( "CELL" ) ) {
			for ( const EspWrite::Node& b : t.kids )
				for ( const EspWrite::Node& s : b.kids ) {
					const EspWrite::Node* cell = nullptr;
					for ( const EspWrite::Node& k : s.kids ) {
						if ( !k.isGroup && recordSig( k.raw ) == "CELL" ) {
							cell = &k;
							CellEdit ce;
							ce.keep = true;
							ce.cell = ours( k.raw );
							c.interiorAt.insert( recordForm( k.raw ), c.cells.size() );
							c.cells.append( ce );
						} else if ( k.isGroup && k.type == 6 && cell ) {
							if ( !addRefs( k, c.cells.last(), nm, c, error ) )
								return false;
						} else
							return fail( error, QStringLiteral( "Add to existing: an interior block holds something else" ) );
					}
				}
		} else if ( t.label == label4( "WRLD" ) ) {
			QByteArray wraw;
			for ( const EspWrite::Node& k : t.kids ) {
				if ( !k.isGroup && recordSig( k.raw ) == "WRLD" ) {
					wraw = k.raw;
					c.worldRaw.insert( recordForm( k.raw ), k.raw );
					CellEdit ce;	// ESPWRITE3B: the WRLD itself is kept, children or none
					ce.world = recordForm( k.raw );
					ce.worldRec = ours( k.raw );
					ce.keep = true;
					c.cells.append( ce );
					continue;
				}
				if ( !k.isGroup || k.type != 1 || wraw.isEmpty() )
					return fail( error, QStringLiteral( "Add to existing: a world group out of order" ) );
				const quint32 wf = recordForm( wraw );
				QByteArray praw;
				std::function<bool( const EspWrite::Node& )> walk = [&]( const EspWrite::Node& g ) -> bool {
					int at = -1;
					for ( const EspWrite::Node& n : g.kids ) {
						if ( !n.isGroup && recordSig( n.raw ) == "CELL" ) {
							if ( g.type == 1 ) {
								praw = n.raw;
								c.pcellRaw.insert( wf, n.raw );
							}
							CellEdit ce;	// kept with or without a group 6
							ce.world = wf;
							ce.cell = ours( n.raw );
							ce.worldRec = ours( wraw );
							ce.keep = true;
							if ( !praw.isEmpty() )
								ce.persistentCell = ours( praw );
							at = c.cells.size();
							c.cells.append( ce );
						} else if ( n.isGroup && n.type == 6 && at >= 0 ) {
							if ( !addRefs( n, c.cells[at], nm, c, error ) )
								return false;
						} else if ( n.isGroup && ( n.type == 4 || n.type == 5 ) ) {
							if ( !walk( n ) )
								return false;
						} else
							return fail( error, QStringLiteral( "Add to existing: a world holds group type %1" ).arg( n.type ) );
					}
					return true;
				};
				if ( !walk( k ) )
					return false;
			}
		} else {
			c.keepTop.append( t );	// ESPWRITE3B espmerge: written back verbatim
		}
	}
	c.ok = true;
	return true;
}

} // namespace

bool cellEditMakePlan( const CellSceneSpec & spec, QVector<CellEditItem> & items, const CellEditTarget & target,
	EspWrite::Plan & plan, QString * summary, QString * error )
{
	QVector<CellEditChange> none;
	return cellEditMakePlan( spec, items, none, target, plan, summary, error );
}

bool cellEditMakePlan( const CellSceneSpec & spec, QVector<CellEditItem> & items, QVector<CellEditChange> & changes,
	const CellEditTarget & target, EspWrite::Plan & plan, QString * summary, QString * error )
{
	plan = Plan();
	if ( target.path.trimmed().isEmpty() )
		return fail( error, QStringLiteral( "no plugin file named" ) );
	const QString tname = baseName( target.path );
	if ( !tname.endsWith( QLatin1String( ".esp" ), Qt::CaseInsensitive )
		&& !tname.endsWith( QLatin1String( ".esl" ), Qt::CaseInsensitive ) )
		return fail( error, QStringLiteral( "%1: the plugin must be an .esp or an .esl" ).arg( tname ) );
	if ( items.isEmpty() && changes.isEmpty() )
		return fail( error, QStringLiteral( "nothing to save: place a reference first" ) );
	g_lastWarnings.clear();
	if ( !spec.valid && spec.plugins.isEmpty() )
		return fail( error, QStringLiteral( "no cell open" ) );
	const QStringList order = orderOf( spec.plugins );
	const int targetInOrder = indexOfName( order, tname );
	if ( targetInOrder >= 0 && !target.append )
		return fail( error, QStringLiteral( "%1 is in the open load order: tick Add to existing, or name another file" ).arg( tname ) );
	if ( target.append && !QFileInfo::exists( target.path ) )
		return fail( error, QStringLiteral( "Add to existing: %1 does not exist" ).arg( target.path ) );

	EsmWorld w;
	QString e;
	if ( spec.interior ) {
		if ( !w.loadInterior( spec.plugins, spec.interiorCell, &e ) )
			return fail( error, e.isEmpty() ? QStringLiteral( "cannot open %1" ).arg( spec.interiorCell ) : e );
	} else {
		quint32 wsForm = 0;
		for ( const auto & p : EsmWorld::listWorldspaces( spec.plugins, &e ) )
			if ( p.second.compare( spec.world, Qt::CaseInsensitive ) == 0 )
				wsForm = p.first;
		if ( !wsForm )
			return fail( error, QStringLiteral( "no worldspace %1 in the load order" ).arg( spec.world ) );
		if ( !w.load( spec.plugins, wsForm, &e ) )
			return fail( error, e );
	}
	const ESMFile * esm = w.plugin();
	if ( !esm )
		return fail( error, QStringLiteral( "the load order did not open" ) );

	Carry carry;
	if ( target.append && !readCarry( target.path, carry, error ) )
		return false;

	Sources src;
	src.order = order;
	/* The plugin a record is copied from: its winner, or, when that loads AFTER the target (the target is in the load
	 * order), the last plugin up to the target that holds it. The target cannot master a later file; the later
	 * winner is reported as a clobber warning below. */
	auto winnerOf = [&]( quint32 lo ) -> QString {
		const QString wn = w.recordPlugin( lo );
		if ( targetInOrder < 0 || indexOfName( order, wn ) <= targetInOrder )
			return wn;
		for ( int i = targetInOrder; i >= 0; i-- ) {
			const QString n = baseName( order[i] );
			PluginIndex * p = src.get( n, nullptr );
			quint32 ff = 0;
			if ( p && fileForm( esm, order, lo, n, p->masters(), ff, nullptr ) && p->has( ff ) )
				return n;
		}
		return wn;
	};
	// the copy of a load-order record from its winner (our own file's copy when the winner is the target)
	auto copyOf = [&]( quint32 lo, Copied & out, QString * winner ) -> bool {
		const QString wn = winnerOf( lo );
		if ( winner )
			*winner = wn;
		if ( wn.isEmpty() )
			return fail( error, QStringLiteral( "record %1 is not in the load order" ).arg( hex8( lo ) ) );
		if ( wn.compare( tname, Qt::CaseInsensitive ) == 0 )
			return true;	// the caller takes the carried copy
		PluginIndex * p = src.get( wn, error );
		if ( !p )
			return false;
		quint32 ff = 0;
		if ( !fileForm( esm, order, lo, wn, p->masters(), ff, nullptr ) || !p->has( ff ) )
			return fail( error, QStringLiteral( "record %1 is not in its winner %2" ).arg( hex8( lo ), wn ) );
		out.raw = p->recordBytes( ff );
		out.source = wn;
		out.sourceMasters = p->masters();
		out.sourceLocalized = ( p->tes4Flags() & kTes4Localized ) != 0;
		return true;
	};

	// the open cell's winner sets the masters: its own masters, then itself
	const quint32 openCell = spec.interior ? w.interior().cellForm : w.cellForm( spec.cx, spec.cy );
	if ( !openCell )
		return fail( error, QStringLiteral( "the open cell has no CELL record" ) );
	const QString openWinner = winnerOf( openCell );
	if ( openWinner.compare( tname, Qt::CaseInsensitive ) == 0 ) {
		if ( !target.append )
			return fail( error, QStringLiteral( "%1 already holds this cell: tick Add to existing" ).arg( tname ) );
		plan.masters = carry.masters;
	} else {
		PluginIndex * p = src.get( openWinner, error );
		if ( !p )
			return false;
		QStringList m;
		for ( const QString & s : p->masters() )
			m.append( baseName( s ) );
		m.append( openWinner );
		if ( target.append ) {
			QStringList cm;
			for ( const QString & s : carry.masters )
				cm.append( baseName( s ).toLower() );
			QStringList lm;
			for ( const QString & s : m )
				lm.append( s.toLower() );
			// ESPWRITE3B: the cell's masters must START the file's list (more masters after them read the same)
			if ( cm.mid( 0, lm.size() ) != lm )
				return fail( error, QStringLiteral( "Add to existing: %1's masters (%2) do not start with this cell's (%3)" )
					.arg( tname, carry.masters.join( QStringLiteral( ", " ) ), m.join( QStringLiteral( ", " ) ) ) );
			plan.masters = carry.masters;
		} else
			plan.masters = m;
	}
	plan.target = tname;
	plan.light = target.append ? carry.light : target.light;
	if ( target.append ) {
		plan.nextFloor = carry.next;
		plan.cells = carry.cells;
		plan.keepTop = carry.keepTop;
		plan.tes4Snam = carry.snam;
		plan.tes4Tail = carry.tail;
		plan.intv = carry.intv;
		if ( !carry.author.isEmpty() )
			plan.author = carry.author;
	}
	const quint32 nm = quint32( plan.masters.size() );

	// the new references, one CellEdit per cell (build() merges them with the carried ones by form)
	QHash<quint32, int> at;
	auto interiorEdit = [&]( quint32 cellLo, int & ci ) -> bool {
		ci = at.value( cellLo, -1 );
		if ( ci >= 0 )
			return true;
		CellEdit ce;
		QString wn;
		if ( !copyOf( cellLo, ce.cell, &wn ) )
			return false;
		if ( ce.cell.raw.isEmpty() ) {	// the target wins: its carried copy
			quint32 ff = 0;
			if ( !fileForm( esm, order, cellLo, tname, carry.masters, ff, nullptr ) || !carry.interiorAt.contains( ff ) )
				return fail( error, QStringLiteral( "%1 wins cell %2 but does not hold it" ).arg( tname, hex8( cellLo ) ) );
			ce.cell = carry.cells[carry.interiorAt.value( ff )].cell;
		}
		ci = plan.cells.size();
		at.insert( cellLo, ci );
		plan.cells.append( ce );
		return true;
	};
	// ESPWRITE3B: the CellEdit of an exterior cell (or the world's persistent cell), made once per cell
	auto exteriorEdit = [&]( quint32 cellLo, bool persistent, int & ci ) -> bool {
		ci = at.value( cellLo, -1 );
		if ( ci >= 0 )
			return true;
		CellEdit ce;
		{
			const quint32 wsLo = w.worldspace();
			QString wn;
			if ( !copyOf( wsLo, ce.worldRec, &wn ) )
				return false;
			quint32 wsFile = 0;
			if ( ce.worldRec.raw.isEmpty() ) {
				if ( !fileForm( esm, order, wsLo, tname, carry.masters, wsFile, nullptr ) || !carry.worldRaw.contains( wsFile ) )
					return fail( error, QStringLiteral( "%1 wins the world but does not hold it" ).arg( tname ) );
				ce.worldRec.raw = carry.worldRaw.value( wsFile );
				ce.worldRec.ours = true;
			}
			ce.world = recordForm( ce.worldRec.raw );
			// the world's persistent cell, from the world's winner (ruling R3)
			if ( carry.pcellRaw.contains( ce.world ) && ce.worldRec.ours ) {
				ce.persistentCell.raw = carry.pcellRaw.value( ce.world );
				ce.persistentCell.ours = true;
			} else if ( !ce.worldRec.ours ) {
				PluginIndex * p = src.get( wn, error );
				if ( !p )
					return false;
				const quint32 pc = p->persistentCell( ce.world );
				if ( pc ) {
					ce.persistentCell = ce.worldRec;
					ce.persistentCell.raw = p->recordBytes( pc );
				}
			}
			if ( persistent ) {
				if ( ce.persistentCell.raw.isEmpty() )
					return fail( error, QStringLiteral( "%1 has no persistent cell for a persistent ref" ).arg( spec.world ) );
				ce.cell = ce.persistentCell;
			} else {
				QString cw;
				if ( !copyOf( cellLo, ce.cell, &cw ) )
					return false;
				if ( ce.cell.raw.isEmpty() ) {
					bool found = false;
					quint32 ff = 0;
					if ( fileForm( esm, order, cellLo, tname, carry.masters, ff, nullptr ) )
						for ( const CellEdit & k : std::as_const( carry.cells ) )
							if ( !k.cell.raw.isEmpty() && recordForm( k.cell.raw ) == ff ) {
								ce.cell = k.cell;
								found = true;
								break;
							}
					if ( !found )
						return fail( error, QStringLiteral( "%1 wins cell %2 but does not hold it" ).arg( tname, hex8( cellLo ) ) );
				}
			}
		}
		ci = plan.cells.size();
		at.insert( cellLo, ci );
		plan.cells.append( ce );
		return true;
	};
	for ( CellEditItem & it : items ) {
		QString bp;
		const unsigned int fi = esm->getFormIDFile( it.base );
		if ( fi >= unsigned( order.size() ) )
			return fail( error, QStringLiteral( "base %1 is not in the load order" ).arg( hex8( it.base ) ) );
		bp = baseName( order[int( fi )] );
		const int mi = indexOfName( plan.masters, bp );
		if ( mi < 0 )
			return fail( error, QStringLiteral( "base %1 (%2) comes from %3, which is not a master of %4 (%5)" )
				.arg( it.baseEdid, hex8( it.base ), bp, tname, plan.masters.join( QStringLiteral( ", " ) ) ) );
		it.baseFile = ( quint32( mi ) << 24 ) | ESMFile::getFormIDLocal( it.base );

		quint32 cellLo = openCell;
		int cx = 0, cy = 0;
		if ( !spec.interior ) {
			cx = int( std::floor( it.pos[0] / 4096.0f ) );
			cy = int( std::floor( it.pos[1] / 4096.0f ) );
			cellLo = it.persistent ? 0xFFFFFFFFu : w.cellForm( cx, cy );
			if ( !cellLo )
				return fail( error, QStringLiteral( "no cell at %1,%2 in %3" ).arg( cx ).arg( cy ).arg( spec.world ) );
		}
		int ci = at.value( cellLo, -1 );
		if ( spec.interior ) {
			if ( !interiorEdit( cellLo, ci ) )
				return false;
		} else if ( !exteriorEdit( cellLo, it.persistent, ci ) ) {
			return false;
		}
		RefAdd a;
		a.base = it.baseFile;
		for ( int i = 0; i < 3; i++ ) {
			a.pos[i] = it.pos[i];
			a.rot[i] = it.rot[i];
		}
		a.scale = it.scale;
		a.persistent = it.persistent;
		plan.cells[ci].adds.append( a );
	}

	/* ESPWRITE3 / 3B: changes to references already in the open cell (an interior, an exterior grid cell or the
	 * world's persistent cell), as overrides of their winners. Ref 0 = a lighting (XCLL) change to the open interior. */
	QStringList warnings;
	// a form in a plugin's own numbering -> the load order's (a light plugin's ids are FE + slot)
	auto loFromFile = [&]( quint32 ff, const QString & file, const QStringList & fileMasters ) -> quint32 {
		const quint32 mi = ff >> 24;
		const QString n = mi < quint32( fileMasters.size() ) ? baseName( fileMasters[int( mi )] ) : file;
		const int oi = indexOfName( order, n );
		if ( oi < 0 )
			return 0;
		for ( quint32 s = 0; s < 0x1000; s++ ) {
			const quint32 fe = 0xFE000000u | ( s << 12 ) | ( ff & 0xFFFu );
			if ( esm->getFormIDFile( fe ) == unsigned( oi ) )
				return fe;
		}
		return ( quint32( oi ) << 24 ) | ( ff & 0xFFFFFFu );
	};
	QSet<quint32> changed;
	for ( CellEditChange & ch : changes ) {
		if ( !ch.ref ) {
			if ( !spec.interior )
				return fail( error, QStringLiteral( "lighting changes work in interior cells only" ) );
			if ( ch.light.isEmpty() )
				return fail( error, QStringLiteral( "a lighting change names no field" ) );
			int ci = -1;
			if ( !interiorEdit( openCell, ci ) )
				return false;
			const quint32 cf = recordForm( plan.cells[ci].cell.raw );
			for ( CellEdit & c : plan.cells )
				if ( !c.world && !c.cell.raw.isEmpty() && recordForm( c.cell.raw ) == cf ) {
					QByteArray raw;
					if ( !changeCellLight( c.cell.raw, ch.light, raw, &warnings, error ) )
						return false;
					c.cell.raw = raw;
				}
			continue;
		}
		if ( changed.contains( ch.ref ) )
			return fail( error, QStringLiteral( "reference %1 is changed twice" ).arg( hex8( ch.ref ) ) );
		changed.insert( ch.ref );
		const float * np = ch.move ? ch.pos : nullptr;
		const float * nr = ch.turn ? ch.rot : nullptr;
		const float * ns = ch.resize ? &ch.scale : nullptr;
		const QString wn = winnerOf( ch.ref );
		if ( wn.isEmpty() )
			return fail( error, QStringLiteral( "reference %1 is not in the load order" ).arg( hex8( ch.ref ) ) );
		quint32 ffT = 0;
		if ( target.append && fileForm( esm, order, ch.ref, tname, carry.masters, ffT, nullptr ) ) {
			// the file being extended already holds this ref (its own, or an override): change that copy in place
			const quint32 ff = ffT;
			bool found = false;
			for ( CellEdit & c : plan.cells ) {
				for ( RefAdd & a : c.adds )
					if ( a.keepForm == ff ) {
						for ( int k = 0; k < 3; k++ ) {
							if ( np )
								a.pos[k] = np[k];
							if ( nr )
								a.rot[k] = nr[k];
						}
						if ( ns && red() != QLatin1String( "editscale" ) )
							a.scale = *ns;
						if ( ch.disable && red() != QLatin1String( "editflag" ) )
							a.disabled = true;
						ch.persistent = a.persistent;
						found = true;
					}
				for ( RefOverride & o : c.overrides )
					if ( recordForm( o.rec.raw ) == ff ) {
						QByteArray raw;
						if ( !changeRef( o.rec.raw, np, nr, ns, ch.disable, raw, error ) )
							return false;
						o.rec.raw = raw;
						ch.persistent = o.persistent;
						found = true;
					}
			}
			if ( found )
				continue;
		}
		if ( wn.compare( tname, Qt::CaseInsensitive ) == 0 )
			return fail( error, QStringLiteral( "%1 wins reference %2 but does not hold it" ).arg( tname, hex8( ch.ref ) ) );
		PluginIndex * p = src.get( wn, error );
		if ( !p )
			return false;
		quint32 ff = 0;
		if ( !fileForm( esm, order, ch.ref, wn, p->masters(), ff, nullptr ) || !p->has( ff ) )
			return fail( error, QStringLiteral( "reference %1 is not in its winner %2" ).arg( hex8( ch.ref ), wn ) );
		const QVector<PluginIndex::Group> chain = p->chain( ff );
		if ( chain.isEmpty() || ( chain[0].type != 8 && chain[0].type != 9 ) )
			return fail( error, QStringLiteral( "reference %1 is not under a cell in %2" ).arg( hex8( ch.ref ), wn ) );
		const quint32 refCell = loFromFile( chain[0].label, wn, p->masters() );
		quint32 worldLo = 0;
		for ( const PluginIndex::Group & g : chain )
			if ( g.type == 1 )
				worldLo = loFromFile( g.label, wn, p->masters() );
		if ( spec.interior ? ( worldLo || refCell != openCell ) : ( !worldLo || worldLo != w.worldspace() ) )
			return fail( error, QStringLiteral( "reference %1 is not in the open cell %2" ).arg( hex8( ch.ref ),
				spec.interior ? spec.interiorCell : spec.world ) );
		RefOverride o;
		o.rec.raw = p->recordBytes( ff );
		o.rec.source = wn;
		o.rec.sourceMasters = p->masters();
		o.rec.sourceLocalized = ( p->tes4Flags() & kTes4Localized ) != 0;
		QByteArray raw;
		if ( !changeRef( o.rec.raw, np, nr, ns, ch.disable, raw, error ) )
			return false;
		o.rec.raw = raw;
		o.persistent = chain[0].type == 8;
		ch.persistent = o.persistent;
		int ci = -1;
		if ( spec.interior ? !interiorEdit( openCell, ci ) : !exteriorEdit( o.persistent ? 0xFFFFFFFFu : refCell, o.persistent, ci ) )
			return false;
		plan.cells[ci].overrides.append( o );
		// a ref the cell's precombines (XCRI) or previs (XPRI) name: the change shows only after a rebuild
		QSet<quint32> pre;
		if ( !o.persistent && precombinedRefs( plan.cells[ci].cell.raw, pre, nullptr ) && pre.contains( recordForm( o.rec.raw ) ) )
			warnings << QStringLiteral( "cell is precombined: changing %1 needs a precombine / previs rebuild" ).arg( hex8( ch.ref ) );
	}
	// a plugin after the target in the load order that wins the cell or a changed ref hides the edit there
	if ( targetInOrder >= 0 ) {
		auto later = [&]( quint32 lo, const QString & what ) {
			const QString wn2 = w.recordPlugin( lo );
			if ( indexOfName( order, wn2 ) > targetInOrder )
				warnings << QStringLiteral( "%1 loads after %2 and overrides %3: the edit will not show" ).arg( wn2, tname, what );
		};
		later( openCell, QStringLiteral( "the cell" ) );
		for ( const CellEditChange & ch : std::as_const( changes ) )
			if ( ch.ref )
				later( ch.ref, QStringLiteral( "reference %1" ).arg( hex8( ch.ref ) ) );
	}
	warnings.removeDuplicates();
	if ( red() == QLatin1String( "nowarn" ) )
		warnings.clear();
	g_lastWarnings = warnings;
	if ( nm > 0xFE )
		return fail( error, QStringLiteral( "too many masters" ) );
	if ( summary ) {
		const QString where = spec.interior ? spec.interiorCell : QStringLiteral( "%1 %2,%3" ).arg( spec.world ).arg( spec.cx ).arg( spec.cy );
		*summary = target.append
			? QStringLiteral( "Adds %1 reference(s) in %2 to %3 (keeps its %4 reference(s) and %5 override(s)); masters %6" )
				.arg( items.size() ).arg( where, tname ).arg( carry.ownRefs ).arg( carry.overrides )
				.arg( plan.masters.join( QStringLiteral( ", " ) ) )
			: QStringLiteral( "Writes %1 reference(s) in %2 to a new %3 %4; masters %5" )
				.arg( items.size() ).arg( where, plan.light ? QStringLiteral( "light" ) : QStringLiteral( "full" ), tname,
					plan.masters.join( QStringLiteral( ", " ) ) );
		if ( !changes.isEmpty() )
			*summary += QStringLiteral( "; changes %1 existing reference(s)" ).arg( changes.size() );
		for ( const QString & wl : std::as_const( warnings ) )
			*summary += QStringLiteral( ". Warning: " ) + wl;
	}
	return true;
}

QString cellEditReopenText( const CellSceneSpec & spec, const QString & target )
{
	QStringList paths = orderOf( spec.plugins );
	if ( indexOfName( paths, baseName( target ) ) < 0 )
		paths.append( QDir::fromNativeSeparators( target ) );
	QString line = spec.interior ? openCellInteriorLine( paths, spec.interiorCell )
		: openCellExteriorLine( paths, spec.world, spec.cx, spec.cy, std::max( 1, spec.n ) );
	if ( !spec.resources.isEmpty() )
		line += QStringLiteral( "\nresources|" ) + spec.resources.join( QLatin1Char( ';' ) );
	return line;
}

bool cellEditSave( const CellSceneSpec & spec, QVector<CellEditItem> & items, const CellEditTarget & target,
	const QString & specDir, QString * reopenPath, QString * line, QString * error )
{
	QVector<CellEditChange> none;
	return cellEditSave( spec, items, none, target, specDir, reopenPath, line, error );
}

bool cellEditSave( const CellSceneSpec & spec, QVector<CellEditItem> & items, QVector<CellEditChange> & changes,
	const CellEditTarget & target, const QString & specDir, QString * reopenPath, QString * line, QString * error )
{
	Plan plan;
	QString summary;
	if ( !cellEditMakePlan( spec, items, changes, target, plan, &summary, error ) )
		return false;
	Report rep;
	if ( !write( target.path, plan, &rep, error ) )
		return false;
	// the new ids back onto the items, in the order they were added
	int k = 0;
	for ( const CellEdit & c : std::as_const( plan.cells ) )
		for ( const RefAdd & a : c.adds )
			if ( !a.keepForm || red() == QLatin1String( "renumber" ) ) {
				if ( k < items.size() && a.base == items[k].baseFile )
					items[k].formOut = a.formOut;
				k++;
			}
	const QString dir = specDir.isEmpty() ? openCellSpecDir() : specDir;
	const QString stem = QFileInfo( target.path ).completeBaseName();
	QString cellName = spec.interior ? spec.interiorCell : QStringLiteral( "%1_%2_%3" ).arg( spec.world ).arg( spec.cx ).arg( spec.cy );
	for ( QChar & ch : cellName )
		if ( !ch.isLetterOrNumber() && ch != QLatin1Char( '_' ) && ch != QLatin1Char( '-' ) )
			ch = QLatin1Char( '_' );
	const QString wwcell = QDir( dir ).filePath( QStringLiteral( "%1_%2.wwcell" ).arg( stem, cellName ) );
	if ( !openCellWriteSpec( wwcell, cellEditReopenText( spec, target.path ), error ) )
		return false;
	if ( reopenPath )
		*reopenPath = wwcell;
	if ( line )
		*line = rep.line( baseName( target.path ) ) + QStringLiteral( "; reopen %1" ).arg( QFileInfo( wwcell ).fileName() );
	return true;
}

// ================================================================== the session
CellEditBus * CellEditBus::instance()
{
	static CellEditBus * b = new CellEditBus;
	return b;
}

CellEditSession & cellEditSession()
{
	static CellEditSession s;
	static bool init = false;
	if ( !init ) {
		init = true;
		s.ghost.baseEdid = QStringLiteral( "XMarkerHeading" );
		s.ghost.base = kBaseXMarkerHeading;
	}
	return s;
}

bool cellEditActive()
{
	return cellEditSession().placing;
}

namespace {

bool openSpec( CellSceneSpec & spec )
{
	return !cellWorkspacePath().isEmpty() && cellWorkspaceOpenSpec( spec );
}

void defaultTarget( CellEditSession & s, const CellSceneSpec & spec )
{
	if ( !s.target.path.isEmpty() )
		return;
	const QStringList order = orderOf( spec.plugins );
	const QString dir = order.isEmpty() ? openCellSpecDir() : QFileInfo( order.first() ).absolutePath();
	s.target.path = QDir( dir ).filePath( QStringLiteral( "WWCellEdits.esp" ) );
}

} // namespace

bool cellEditSetBase( const QString & edid )
{
	CellEditSession & s = cellEditSession();
	CellSceneSpec spec;
	if ( !openSpec( spec ) ) {
		s.ghost.baseEdid = edid.trimmed();
		s.baseValid = false;
		say( QStringLiteral( "no cell open: %1 resolves when one is" ).arg( edid.trimmed() ), true );
		changed();
		return false;
	}
	quint32 lo = 0;
	QString sig, e;
	float blo[3], bhi[3];
	if ( !cellEditResolveEdid( orderOf( spec.plugins ), edid, lo, sig, blo, bhi, &e ) ) {
		s.baseValid = false;
		s.ghost.baseEdid = edid.trimmed();
		say( e, true );
		changed();
		return false;
	}
	s.ghost.base = lo;
	s.ghost.baseEdid = edid.trimmed();
	for ( int i = 0; i < 3; i++ ) {
		s.ghost.lo[i] = blo[i];
		s.ghost.hi[i] = bhi[i];
	}
	if ( sig == QLatin1String( "STAT" ) && ( lo == kBaseXMarker || lo == kBaseXMarkerHeading ) )
		s.ghost.persistent = true;	// every vanilla marker ref is persistent
	s.baseValid = true;
	say( QStringLiteral( "base %1 = %2 %3, bounds %4..%5" ).arg( edid.trimmed(), sig, hex8( lo ) )
		.arg( QStringLiteral( "%1,%2,%3" ).arg( blo[0] ).arg( blo[1] ).arg( blo[2] ),
			QStringLiteral( "%1,%2,%3" ).arg( bhi[0] ).arg( bhi[1] ).arg( bhi[2] ) ), false );
	changed();
	return true;
}

bool cellEditStartPlace( GLView * view )
{
	CellEditSession & s = cellEditSession();
	Extra & x = extra();
	if ( view )
		x.view = view;
	CellSceneSpec spec;
	if ( !openSpec( spec ) ) {
		say( QStringLiteral( "Place: open a cell first" ), true );
		return false;
	}
	if ( !s.baseValid && !cellEditSetBase( s.ghost.baseEdid.isEmpty() ? QStringLiteral( "XMarkerHeading" ) : s.ghost.baseEdid ) )
		return false;
	defaultTarget( s, spec );
	s.placing = true;
	std::fprintf( stderr, "place mode: on (Shift+A; every mouse move reads the depth under the cursor; Esc leaves)\n" );
	s.facing = s.locked = s.turning = false;
	s.ghostValid = false;
	x.queue.clear();
	say( QStringLiteral( "Place %1: click to drop it, drag to turn it, T faces it toward a second click, Esc leaves" )
		.arg( s.ghost.baseEdid ), false );
	if ( x.view ) {
		x.view->requestActivate();
		x.view->update();
	}
	changed();
	return true;
}

void cellEditStopPlace()
{
	CellEditSession & s = cellEditSession();
	if ( s.placing )
		std::fprintf( stderr, "place mode: off\n" );
	s.placing = s.facing = s.locked = s.turning = false;
	s.ghostValid = false;
	extra().queue.clear();
	extra().lmbDown = false;
	if ( extra().view )
		extra().view->update();
	changed();
}

void cellEditUndo()
{
	CellEditSession & s = cellEditSession();
	if ( s.edits.isEmpty() )
		return;
	s.edits.removeLast();
	say( QStringLiteral( "undone: %1 edit(s) left" ).arg( s.edits.size() ), false );
	if ( extra().view )
		extra().view->update();
	changed();
}

void cellEditRemove( int index )
{
	CellEditSession & s = cellEditSession();
	if ( index < 0 || index >= s.edits.size() + s.changes.size() )
		return;
	if ( index < s.edits.size() )
		s.edits.removeAt( index );
	else
		s.changes.removeAt( index - s.edits.size() );	// the list shows the changes after the placed refs
	if ( extra().view )
		extra().view->update();
	changed();
}

void cellEditClear()
{
	cellEditSession().edits.clear();
	cellEditSession().changes.clear();
	if ( extra().view )
		extra().view->update();
	changed();
}

bool cellEditChangeRef( quint32 ref, char what, const float * v )
{
	CellEditSession & s = cellEditSession();
	const CellPickTable & t = cellPickTable();
	const int pi = ref ? t.indexOfForm( ref ) : -1;
	if ( pi < 0 ) {
		say( QStringLiteral( "Change refused: reference %1 is not in the open cell" ).arg( hex8( ref ) ), true );
		return false;
	}
	if ( what != 'd' && !v ) {
		say( QStringLiteral( "Change refused: no value" ), true );
		return false;
	}
	if ( what == 's' && !( v[0] > 0.0f && v[0] <= 10.0f ) ) {
		say( QStringLiteral( "Change refused: scale %1 is outside 0..10" ).arg( double( v[0] ) ), true );
		return false;
	}
	const CellPickEntry & e = t.at( pi );
	CellEditChange * c = nullptr;
	for ( CellEditChange & x : s.changes )
		if ( x.ref == ref )
			c = &x;
	if ( !c ) {
		CellEditChange n;
		n.ref = ref;
		for ( int k = 0; k < 3; k++ ) {
			n.pos[k] = e.pos[k];
			n.rot[k] = e.rot[k];
		}
		n.scale = e.scale;
		s.changes.append( n );
		c = &s.changes.last();
	}
	switch ( what ) {
	case 'm':
		c->move = true;
		for ( int k = 0; k < 3; k++ )
			c->pos[k] = v[k];
		break;
	case 't':
		c->turn = true;
		c->rot[2] = float( std::remainder( double( c->rot[2] + v[0] ), 2.0 * double( kPi ) ) );
		if ( c->rot[2] < 0.0f )
			c->rot[2] += 2.0f * kPi;
		break;
	case 's':
		c->resize = true;
		c->scale = v[0];
		break;
	case 'd':
		c->disable = true;
		break;
	default:
		return false;
	}
	say( QStringLiteral( "changed: " ) + cellEditChangeText( *c ), false );
	changed();
	return true;
}

bool cellEditLight( const QString & field, const QString & value )
{
	CellSceneSpec spec;
	if ( !openSpec( spec ) || !spec.interior ) {
		say( QStringLiteral( "Lighting refused: open an interior cell" ), true );
		return false;
	}
	bool known = false;
	for ( const QString & f : EspWrite::cellLightFields() )
		known = known || f.compare( field, Qt::CaseInsensitive ) == 0;
	if ( !known ) {
		say( QStringLiteral( "Lighting refused: no field %1 (fields: %2)" ).arg( field, EspWrite::cellLightFields().join( QLatin1Char( ' ' ) ) ), true );
		return false;
	}
	CellEditSession & s = cellEditSession();
	CellEditChange * c = nullptr;
	for ( CellEditChange & x : s.changes )
		if ( !x.ref )
			c = &x;
	if ( !c ) {
		s.changes.append( CellEditChange() );
		c = &s.changes.last();
	}
	bool replaced = false;
	for ( QPair<QString, QString> & kv : c->light )
		if ( kv.first.compare( field, Qt::CaseInsensitive ) == 0 ) {
			kv.second = value;
			replaced = true;
		}
	if ( !replaced )
		c->light.append( qMakePair( field, value ) );
	say( QStringLiteral( "changed: " ) + cellEditChangeText( *c ), false );
	changed();
	return true;
}

QString cellEditChangeText( const CellEditChange & c )
{
	if ( !c.ref ) {
		QStringList l;
		for ( const QPair<QString, QString> & kv : c.light )
			l << kv.first + QLatin1Char( '=' ) + kv.second;
		return QStringLiteral( "cell lighting  " ) + l.join( QLatin1Char( ' ' ) );
	}
	QStringList l;
	if ( c.move )
		l << QStringLiteral( "move (%1, %2, %3)" ).arg( double( c.pos[0] ), 0, 'f', 1 ).arg( double( c.pos[1] ), 0, 'f', 1 )
			.arg( double( c.pos[2] ), 0, 'f', 1 );
	if ( c.turn )
		l << QStringLiteral( "yaw %1" ).arg( double( c.rot[2] ) * 180.0 / double( kPi ), 0, 'f', 1 );
	if ( c.resize )
		l << QStringLiteral( "scale %1" ).arg( double( c.scale ), 0, 'f', 3 );
	if ( c.disable )
		l << QStringLiteral( "disable" );
	return hex8( c.ref ) + QStringLiteral( "  " ) + l.join( QStringLiteral( ", " ) );
}

QString cellEditRowText( const CellEditItem & it )
{
	return QStringLiteral( "%1  (%2, %3, %4)  yaw %5%6" ).arg( it.baseEdid )
		.arg( double( it.pos[0] ), 0, 'f', 1 ).arg( double( it.pos[1] ), 0, 'f', 1 ).arg( double( it.pos[2] ), 0, 'f', 1 )
		.arg( double( it.rot[2] ) * 180.0 / double( kPi ), 0, 'f', 1 )
		.arg( ( std::fabs( it.rot[0] ) > 1.0e-4f || std::fabs( it.rot[1] ) > 1.0e-4f ) ? QStringLiteral( " (flat)" ) : QString() );
}

QString cellEditSummary( bool * refusal )
{
	CellEditSession & s = cellEditSession();
	auto r = [refusal]( bool v ) {
		if ( refusal )
			*refusal = v;
	};
	CellSceneSpec spec;
	if ( !openSpec( spec ) ) {
		r( true );
		return QStringLiteral( "Open a cell to place references." );
	}
	defaultTarget( s, spec );
	if ( s.edits.isEmpty() && s.changes.isEmpty() ) {
		r( true );
		return extra().lastReopen.isEmpty() ? QStringLiteral( "Nothing to save: place or change a reference first." )
			: QStringLiteral( "Saved; nothing new to save." );
	}
	if ( s.target.path.trimmed().isEmpty() ) {
		r( true );
		return QStringLiteral( "Name the plugin to write." );
	}
	const QString tname = baseName( s.target.path );
	if ( !tname.endsWith( QLatin1String( ".esp" ), Qt::CaseInsensitive ) && !tname.endsWith( QLatin1String( ".esl" ), Qt::CaseInsensitive ) ) {
		r( true );
		return QStringLiteral( "The plugin must be an .esp or an .esl." );
	}
	if ( !s.target.append && QFileInfo::exists( s.target.path ) ) {
		r( false );
		return QStringLiteral( "Replaces %1 with %2 reference(s) (it exists: tick Add to existing to keep it)." )
			.arg( tname ).arg( s.edits.size() );
	}
	if ( s.target.append && !QFileInfo::exists( s.target.path ) ) {
		r( true );
		return QStringLiteral( "Add to existing: %1 does not exist." ).arg( tname );
	}
	r( false );
	const QString what = s.changes.isEmpty() ? QStringLiteral( "%1 reference(s)" ).arg( s.edits.size() )
		: QStringLiteral( "%1 reference(s) and %2 change(s)" ).arg( s.edits.size() ).arg( s.changes.size() );
	return s.target.append ? QStringLiteral( "Adds %1 to %2, then re-opens the cell with it." ).arg( what, tname )
		: QStringLiteral( "Writes %1 to a new %2 %3, then re-opens the cell with it." ).arg( what )
			.arg( s.target.light ? QStringLiteral( "light" ) : QStringLiteral( "full" ), tname );
}

bool cellEditSaveSession( QString * line )
{
	CellEditSession & s = cellEditSession();
	Extra & x = extra();
	CellSceneSpec spec;
	if ( !openSpec( spec ) ) {
		say( QStringLiteral( "Save: no cell open" ), true );
		return false;
	}
	defaultTarget( s, spec );
	QString reopen, l, e;
	QVector<CellEditItem> items = s.edits;
	QVector<CellEditChange> changes = s.changes;
	if ( !cellEditSave( spec, items, changes, s.target, wwEnvStr( "WW_CELL_EDIT_SPECDIR" ), &reopen, &l, &e ) ) {
		say( QStringLiteral( "Save refused: " ) + e, true );
		return false;
	}
	x.lastSaved = items;
	x.lastReopen = reopen;
	for ( const CellEditItem & it : std::as_const( items ) )
		logLine( QStringLiteral( "saved form %1 %2" ).arg( hex8( it.formOut ), cellEditGhostLine( it ) ) );
	for ( const CellEditChange & c : std::as_const( changes ) )
		logLine( QStringLiteral( "saved change %1%2" ).arg( cellEditChangeText( c ),
			!c.ref ? QString() : c.persistent ? QStringLiteral( " persistent" ) : QStringLiteral( " temporary" ) ) );
	for ( const QString & w : cellEditWarnings() )
		logLine( QStringLiteral( "warning " ) + w );
	s.edits.clear();
	s.changes.clear();
	s.target.append = true;	// the next save extends the same file
	cellEditStopPlace();
	if ( line )
		*line = l;
	say( l, false );
	emit CellEditBus::instance()->reopen( reopen );
	return true;
}

// ================================================================== the surface probe
namespace {

bool readDepth( GLView * view, Scene * scene, double x, double y, float & depth )
{
	GLint vp[4] = { 0, 0, 0, 0 };
	glGetIntegerv( GL_VIEWPORT, vp );
	const int ww = view->width(), wh = view->height();
	if ( vp[2] <= 0 || vp[3] <= 0 || ww <= 0 || wh <= 0 || x < 0 || y < 0 || x >= ww || y >= wh )
		return false;
	const int px = vp[0] + std::min( vp[2] - 1, int( std::floor( x * vp[2] / ww ) ) );
	const int py = vp[1] + vp[3] - 1 - std::min( vp[3] - 1, int( std::floor( y * vp[3] / wh ) ) );
	while ( glGetError() != GL_NO_ERROR ) {
	}
	GLint prvRead = 0, drawFbo = 0;
	glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &prvRead );
	glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo );
	Renderer * rr = scene ? scene->renderer : nullptr;
	const bool rebind = rr && rr->fn && prvRead != drawFbo;
	if ( rebind )
		rr->fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( drawFbo ) );
	float d = 1.0f;
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glReadPixels( px, py, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &d );
	const bool ok = glGetError() == GL_NO_ERROR;
	if ( rebind )
		rr->fn->glBindFramebuffer( GL_READ_FRAMEBUFFER, GLuint( prvRead ) );
	depth = d;
	return ok;
}

/*! The nearest triangle along a world ray, over the shapes whose world bound the ray passes (or, with `near`,
 *  the bound holds the point within its radius + tol). */
bool castRay( GLView * view, Scene * scene, const float o[3], const float d[3], const float * nearP, float tol, float & tBest,
	float normal[3] )
{
	bool hit = false;
	tBest = 1.0e30f;
	for ( Shape * s : scene->shapes ) {
		if ( !s || s->isHidden() || s->verts.isEmpty() || s->triangles.isEmpty() )
			continue;
		const BoundSphere b = s->bounds();
		const float c[3] = { b.center[0], b.center[1], b.center[2] };
		if ( nearP ) {
			const float dx = nearP[0] - c[0], dy = nearP[1] - c[1], dz = nearP[2] - c[2];
			const float rr = b.radius + tol;
			if ( dx * dx + dy * dy + dz * dz > rr * rr )
				continue;
		} else {
			const float oc[3] = { c[0] - o[0], c[1] - o[1], c[2] - o[2] };
			const float tc = oc[0] * d[0] + oc[1] * d[1] + oc[2] * d[2];
			const float dd = oc[0] * oc[0] + oc[1] * oc[1] + oc[2] * oc[2] - tc * tc;
			if ( dd > b.radius * b.radius || tc < -b.radius )
				continue;
		}
		const Transform wt = view->shapeRenderTrans( s );
		std::vector<Vector3> wv( size_t( s->verts.size() ) );
		bool haveW = false;
		for ( const Triangle & t : s->triangles ) {
			const int i0 = t.v1(), i1 = t.v2(), i2 = t.v3();
			if ( i0 >= s->verts.size() || i1 >= s->verts.size() || i2 >= s->verts.size() )
				continue;
			if ( !haveW ) {
				for ( qsizetype i = 0; i < s->verts.size(); i++ )
					wv[size_t( i )] = wt * s->verts[i];
				haveW = true;
			}
			const float a[3] = { wv[i0][0], wv[i0][1], wv[i0][2] };
			const float bb[3] = { wv[i1][0], wv[i1][1], wv[i1][2] };
			const float cc[3] = { wv[i2][0], wv[i2][1], wv[i2][2] };
			float tt = 0.0f;
			if ( cellSurfaceRayTri( o, d, a, bb, cc, tt ) && tt < tBest ) {
				float n[3];
				if ( cellSurfaceTriNormal( a, bb, cc, d, n ) ) {
					tBest = tt;
					normal[0] = n[0];
					normal[1] = n[1];
					normal[2] = n[2];
					hit = true;
				}
			}
		}
	}
	return hit;
}

CellSurfaceHit probe( GLView * view, Scene * scene, double x, double y )
{
	CellSurfaceHit h;
	CellSurfaceStats & st = cellSurfaceStats();
	st.probes++;
	Vector3 ro, rdv;
	view->mouseRayWorld( QPointF( x, y ), ro, rdv );
	const float len = std::sqrt( rdv[0] * rdv[0] + rdv[1] * rdv[1] + rdv[2] * rdv[2] );
	if ( !( len > 0.0f ) ) {
		st.missed++;
		return h;
	}
	const float o[3] = { ro[0], ro[1], ro[2] };
	const float d[3] = { rdv[0] / len, rdv[1] / len, rdv[2] / len };
	const bool redRay = cellSurfaceRed() == QLatin1String( "ray" );
	double nearT = 0, farT = 0;
	bool persp = true;
	cellSurfacePlanes( nearT, farT, persp );
	if ( !redRay && persp ) {
		float depth = 1.0f;
		if ( !readDepth( view, scene, x, y, depth ) ) {
			st.unreadable++;
		} else {
			h.depth = depth;
			double zEye = 0, t = 0;
			const Transform vt = view->viewTransform();
			const Vector3 fw = vt.rotation.inverted() * Vector3( 0, 0, -1 );
			const float f[3] = { fw[0], fw[1], fw[2] };
			const double sc = vt.scale != 0.0f ? double( vt.scale ) : 1.0;
			if ( !cellSurfaceEyeDistance( depth, nearT, farT, persp, zEye ) ) {
				st.sky++;
			} else if ( cellSurfaceRayParam( d, f, zEye / sc, t ) ) {
				h.tDepth = float( t );
				const float p[3] = { float( o[0] + d[0] * t ), float( o[1] + d[1] * t ), float( o[2] + d[2] * t ) };
				const float tol = cellSurfaceTolerance( float( t ) );
				float tr = 0.0f, n[3] = { 0, 0, 1 };
				if ( castRay( view, scene, o, d, p, tol, tr, n ) && std::fabs( tr - float( t ) ) <= tol ) {
					h.tRefine = tr;
					for ( int i = 0; i < 3; i++ ) {
						h.pos[i] = o[i] + d[i] * tr;
						h.normal[i] = n[i];
					}
					h.via = 1;
					h.ok = true;
					st.refined++;
					return h;
				}
				for ( int i = 0; i < 3; i++ )
					h.pos[i] = p[i];
				h.normal[0] = h.normal[1] = 0.0f;
				h.normal[2] = 1.0f;
				h.via = 2;
				h.ok = true;
				st.depthOnly++;
				return h;
			}
		}
	}
	float tr = 0.0f, n[3] = { 0, 0, 1 };
	if ( castRay( view, scene, o, d, nullptr, 0.0f, tr, n ) ) {
		h.tRefine = tr;
		for ( int i = 0; i < 3; i++ ) {
			h.pos[i] = o[i] + d[i] * tr;
			h.normal[i] = n[i];
		}
		h.via = 3;
		h.ok = true;
		st.rayFallback++;
		return h;
	}
	st.missed++;
	return h;
}

//! Ground snap: from a wall or a ceiling down to the surface under the point.
void snapDown( GLView * view, Scene * scene, float pos[3], float normal[3] )
{
	if ( normal[2] >= 0.7f )
		return;
	const float o[3] = { pos[0] + normal[0] * 2.0f, pos[1] + normal[1] * 2.0f, pos[2] + 1.0f };
	const float d[3] = { 0.0f, 0.0f, -1.0f };
	float t = 0.0f, n[3] = { 0, 0, 1 };
	if ( castRay( view, scene, o, d, nullptr, 0.0f, t, n ) ) {
		for ( int i = 0; i < 3; i++ ) {
			pos[i] = o[i] + d[i] * t;
			normal[i] = n[i];
		}
	}
}

QString hitLine( const CellSurfaceHit & h, double x, double y )
{
	return QStringLiteral( "probe px %1 %2 ok %3 via %4 pos %5 %6 %7 normal %8 %9 %10 tDepth %11 tRefine %12 depth %13" )
		.arg( x, 0, 'f', 1 ).arg( y, 0, 'f', 1 ).arg( h.ok ? 1 : 0 ).arg( h.via )
		.arg( double( h.pos[0] ), 0, 'f', 3 ).arg( double( h.pos[1] ), 0, 'f', 3 ).arg( double( h.pos[2] ), 0, 'f', 3 )
		.arg( double( h.normal[0] ), 0, 'f', 4 ).arg( double( h.normal[1] ), 0, 'f', 4 ).arg( double( h.normal[2] ), 0, 'f', 4 )
		.arg( double( h.tDepth ), 0, 'f', 3 ).arg( double( h.tRefine ), 0, 'f', 3 ).arg( double( h.depth ), 0, 'f', 7 );
}

void updatePose( CellEditSession & s )
{
	cellEditPose( s.yaw, s.flat, s.normal, s.ghost.rot );
}

void commitGhost( CellEditSession & s )
{
	if ( !s.ghostValid ) {
		say( QStringLiteral( "Place: no surface under the cursor" ), true );
		return;
	}
	updatePose( s );
	CellEditItem it = s.ghost;
	s.edits.append( it );
	logLine( QStringLiteral( "placed %1" ).arg( cellEditGhostLine( it ) ) );
	say( QStringLiteral( "placed %1 (%2 edit(s))" ).arg( cellEditRowText( it ) ).arg( s.edits.size() ), false );
	s.locked = s.facing = false;
	changed();
}

void runAction( GLView * view, Scene * scene, const Extra::Action & a )
{
	CellEditSession & s = cellEditSession();
	if ( a.kind == Extra::Action::Commit ) {
		commitGhost( s );
		return;
	}
	CellSurfaceHit h = probe( view, scene, a.x, a.y );
	extra().lastHit = h;
	if ( extra().probeLogs < 2000 ) {
		extra().probeLogs++;
		logLine( hitLine( h, a.x, a.y ) );
	}
	if ( !h.ok )
		return;
	if ( a.kind == Extra::Action::Face ) {
		if ( s.ghostValid ) {
			s.yaw = cellEditYawToward( s.ghost.pos, h.pos );
			updatePose( s );
			changed();
		}
		return;
	}
	if ( a.kind == Extra::Action::Hover && s.locked )
		return;
	float p[3] = { h.pos[0], h.pos[1], h.pos[2] }, n[3] = { h.normal[0], h.normal[1], h.normal[2] };
	if ( s.snap )
		snapDown( view, scene, p, n );
	for ( int i = 0; i < 3; i++ ) {
		s.ghost.pos[i] = p[i];
		s.normal[i] = n[i];
	}
	s.ghostValid = true;
	if ( a.kind == Extra::Action::Anchor )
		s.locked = true;
	updatePose( s );
}

// ------------------------------------------------------------------ drawing
void boxLines( const CellEditItem & it, const FloatVector4 & col, std::vector<Vector3> & p, std::vector<FloatVector4> & c )
{
	float m[3][3];
	cellEditMatrix( it.rot, m, cellEditRed() == QLatin1String( "ghost" ) );
	auto w = [&]( float lx, float ly, float lz ) {
		return Vector3( it.pos[0] + it.scale * ( m[0][0] * lx + m[0][1] * ly + m[0][2] * lz ),
			it.pos[1] + it.scale * ( m[1][0] * lx + m[1][1] * ly + m[1][2] * lz ),
			it.pos[2] + it.scale * ( m[2][0] * lx + m[2][1] * ly + m[2][2] * lz ) );
	};
	Vector3 k[8];
	for ( int i = 0; i < 8; i++ )
		k[i] = w( ( i & 1 ) ? it.hi[0] : it.lo[0], ( i & 2 ) ? it.hi[1] : it.lo[1], ( i & 4 ) ? it.hi[2] : it.lo[2] );
	static const int kEdges[12][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 },
		{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
	for ( const auto & e : kEdges ) {
		p.push_back( k[e[0]] );
		p.push_back( k[e[1]] );
		c.push_back( col );
		c.push_back( col );
	}
	// the heading: an arrow along +Y and a post up +Z
	const Vector3 o = w( 0, 0, 0 ), tip = w( 0, 48.0f / std::max( it.scale, 1.0e-3f ), 0 );
	const Vector3 l = w( -8.0f, 36.0f / std::max( it.scale, 1.0e-3f ), 0 ), r = w( 8.0f, 36.0f / std::max( it.scale, 1.0e-3f ), 0 );
	const Vector3 up = w( 0, 0, 64.0f / std::max( it.scale, 1.0e-3f ) );
	const Vector3 seg[8] = { o, tip, tip, l, tip, r, o, up };
	for ( const Vector3 & v : seg ) {
		p.push_back( v );
		c.push_back( col );
	}
}

void drawEdits( Scene * scene, const Transform & viewTrans )
{
	CellEditSession & s = cellEditSession();
	if ( s.edits.isEmpty() && !( s.placing && s.ghostValid ) )
		return;
	std::vector<Vector3> p;
	std::vector<FloatVector4> c;
	for ( const CellEditItem & it : std::as_const( s.edits ) )
		boxLines( it, FloatVector4( 0.35f, 1.0f, 0.45f, 1.0f ), p, c );
	if ( s.placing && s.ghostValid )
		boxLines( s.ghost, s.locked ? FloatVector4( 1.0f, 0.55f, 0.15f, 1.0f ) : FloatVector4( 1.0f, 0.85f, 0.2f, 1.0f ), p, c );
	scene->loadModelViewMatrix( viewTrans );
	glDisable( GL_DEPTH_TEST );	// the ghost shows through the floor it stands on
	glDisable( GL_CULL_FACE );
	scene->drawLines( p.data(), p.size(), c.data() );
	glEnable( GL_DEPTH_TEST );
}

// ------------------------------------------------------------------ the script (WW_CELL_EDIT_SCRIPT)
bool screenOf( GLView * view, float x, float y, float z, double & sx, double & sy )
{
	QPointF q;
	if ( !view->worldToScreen( Vector3( x, y, z ), q ) )
		return false;
	sx = q.x();
	sy = q.y();
	return true;
}

void readback()
{
	Extra & x = extra();
	CellSceneSpec spec;
	if ( !openSpec( spec ) ) {
		logLine( QStringLiteral( "readback FAIL no cell open" ) );
		return;
	}
	EsmWorld w;
	QString e;
	QVector<EsmRefr> refs;
	if ( spec.interior ) {
		if ( !w.loadInterior( spec.plugins, spec.interiorCell, &e ) ) {
			logLine( QStringLiteral( "readback FAIL %1" ).arg( e ) );
			return;
		}
		refs = w.interiorRefrs();
	} else {
		quint32 ws = 0;
		for ( const auto & p : EsmWorld::listWorldspaces( spec.plugins, &e ) )
			if ( p.second.compare( spec.world, Qt::CaseInsensitive ) == 0 )
				ws = p.first;
		if ( !ws || !w.load( spec.plugins, ws, &e ) ) {
			logLine( QStringLiteral( "readback FAIL %1" ).arg( e ) );
			return;
		}
		int x0, y0, x1, y1;
		spec.rect( x0, y0, x1, y1 );
		for ( int cy = y0; cy <= y1; cy++ )
			for ( int cx = x0; cx <= x1; cx++ )
				refs += w.refrs( cx, cy );
		refs += w.persistentRefrsIn( x0 * 4096.0f, y0 * 4096.0f, ( x1 + 1 ) * 4096.0f, ( y1 + 1 ) * 4096.0f );
	}
	int ok = 0, bad = 0;
	for ( const CellEditItem & it : std::as_const( x.lastSaved ) ) {
		const EsmRefr * best = nullptr;
		for ( const EsmRefr & r : std::as_const( refs ) )
			if ( r.base == it.base && std::fabs( r.pos[0] - it.pos[0] ) < 0.01f && std::fabs( r.pos[1] - it.pos[1] ) < 0.01f
				&& std::fabs( r.pos[2] - it.pos[2] ) < 0.01f )
				best = &r;
		if ( !best ) {
			bad++;
			logLine( QStringLiteral( "readback MISSING base %1 at %2 %3 %4" ).arg( hex8( it.base ) ).arg( it.pos[0] ).arg( it.pos[1] ).arg( it.pos[2] ) );
			continue;
		}
		float dr = 0.0f;
		for ( int i = 0; i < 3; i++ )
			dr = std::max( dr, std::fabs( best->rot[i] - it.rot[i] ) );
		const QString plugin = w.recordPlugin( best->formID );
		bool pick = false;
		const CellPickTable & t = cellPickTable();
		for ( int i = 0; i < t.size(); i++ )
			if ( t.at( i ).refForm == best->formID )
				pick = true;
		const bool good = dr < 1.0e-5f && plugin.compare( baseName( cellEditSession().target.path ), Qt::CaseInsensitive ) == 0;
		( good ? ok : bad )++;
		logLine( QStringLiteral( "readback %1 form %2 plugin %3 base %4 pos %5 %6 %7 rot %8 %9 %10 drot %11 pick %12" )
			.arg( good ? QStringLiteral( "ok" ) : QStringLiteral( "MISMATCH" ), hex8( best->formID ), plugin, hex8( best->base ) )
			.arg( double( best->pos[0] ), 0, 'f', 3 ).arg( double( best->pos[1] ), 0, 'f', 3 ).arg( double( best->pos[2] ), 0, 'f', 3 )
			.arg( double( best->rot[0] ), 0, 'f', 6 ).arg( double( best->rot[1] ), 0, 'f', 6 ).arg( double( best->rot[2] ), 0, 'f', 6 )
			.arg( double( dr ), 0, 'g', 3 ).arg( pick ? QStringLiteral( "yes" ) : QStringLiteral( "no (markers hidden)" ) ) );
	}
	logLine( QStringLiteral( "readback summary %1 ok %2 bad of %3" ).arg( ok ).arg( bad ).arg( x.lastSaved.size() ) );
}

//! One script step; false = wait (the step runs again next frame).
bool scriptStep( GLView * view, const QString & raw )
{
	Extra & x = extra();
	CellEditSession & s = cellEditSession();
	const QStringList a = raw.split( QLatin1Char( ' ' ), Qt::SkipEmptyParts );
	if ( a.isEmpty() || a[0].startsWith( QLatin1Char( '#' ) ) )
		return true;
	const QString v = a[0].toLower();
	auto num = [&]( int i ) { return i < a.size() ? a[i].toFloat() : 0.0f; };
	logLine( QStringLiteral( "step %1" ).arg( raw ) );
	if ( v == QLatin1String( "log" ) ) {
		x.logPath = a.value( 1 );
		logLine( QStringLiteral( "log open %1 cell %2" ).arg( QDateTime::currentDateTime().toString( Qt::ISODate ), cellWorkspacePath() ) );
	} else if ( v == QLatin1String( "base" ) ) {
		cellEditSetBase( a.value( 1 ) );
	} else if ( v == QLatin1String( "start" ) ) {
		cellEditStartPlace( view );
	} else if ( v == QLatin1String( "stop" ) ) {
		cellEditStopPlace();
	} else if ( v == QLatin1String( "wait" ) ) {
		x.waitFrames = std::max( 0, int( num( 1 ) ) );
	} else if ( v == QLatin1String( "yaw" ) ) {
		s.yaw = num( 1 ) * kPi / 180.0f;
		updatePose( s );
	} else if ( v == QLatin1String( "flat" ) ) {
		s.flat = num( 1 ) != 0.0f;
		updatePose( s );
	} else if ( v == QLatin1String( "snap" ) ) {
		s.snap = num( 1 ) != 0.0f;
	} else if ( v == QLatin1String( "persistent" ) ) {
		s.ghost.persistent = num( 1 ) != 0.0f;
	} else if ( v == QLatin1String( "target" ) ) {
		s.target.path = a.value( 1 );
		s.target.append = a.contains( QStringLiteral( "append" ) );
		s.target.light = !a.contains( QStringLiteral( "full" ) );
	} else if ( v == QLatin1String( "aimworld" ) || v == QLatin1String( "clickworld" ) || v == QLatin1String( "lockworld" )
		|| v == QLatin1String( "faceworld" ) ) {
		double sx = 0, sy = 0;
		if ( !screenOf( view, num( 1 ), num( 2 ), num( 3 ), sx, sy ) ) {
			logLine( QStringLiteral( "step FAIL %1 is off screen" ).arg( raw ) );
			return true;
		}
		logLine( QStringLiteral( "pixel %1 %2 for %3 %4 %5" ).arg( sx, 0, 'f', 2 ).arg( sy, 0, 'f', 2 ).arg( num( 1 ) ).arg( num( 2 ) ).arg( num( 3 ) ) );
		Extra::Action act;
		act.x = sx;
		act.y = sy;
		act.kind = v == QLatin1String( "aimworld" ) ? Extra::Action::Hover
			: v == QLatin1String( "faceworld" ) ? Extra::Action::Face : Extra::Action::Anchor;
		if ( act.kind == Extra::Action::Hover )
			s.locked = false;
		x.queue.push_back( act );
		if ( v == QLatin1String( "clickworld" ) || v == QLatin1String( "faceworld" ) ) {
			Extra::Action c;
			c.kind = Extra::Action::Commit;
			x.queue.push_back( c );
		}
	} else if ( v == QLatin1String( "pickref" ) ) {	// ESPWRITE3B: the ref the next move / turn / scale / disable changes
		quint32 & picked = x.scriptRef;
		picked = a.value( 1 ).toUInt( nullptr, 16 );
		const int pi = cellPickTable().indexOfForm( picked );
		logLine( pi < 0 ? QStringLiteral( "pickref FAIL %1 is not in the open cell" ).arg( hex8( picked ) )
			: QStringLiteral( "pickref %1 %2" ).arg( hex8( picked ), cellPickTable().at( pi ).baseEdid ) );
	} else if ( v == QLatin1String( "move" ) || v == QLatin1String( "turn" ) || v == QLatin1String( "scale" )
		|| v == QLatin1String( "disable" ) ) {
		const float val[3] = { num( 1 ), num( 2 ), num( 3 ) };
		const float yaw[3] = { num( 1 ) * kPi / 180.0f, 0, 0 };
		const char what = v == QLatin1String( "move" ) ? 'm' : v == QLatin1String( "turn" ) ? 't' : v == QLatin1String( "scale" ) ? 's' : 'd';
		if ( !cellEditChangeRef( x.scriptRef, what, what == 't' ? yaw : val ) )
			logLine( QStringLiteral( "step FAIL %1" ).arg( raw ) );
	} else if ( v == QLatin1String( "light" ) ) {
		if ( !cellEditLight( a.value( 1 ), a.value( 2 ) ) )
			logLine( QStringLiteral( "step FAIL %1" ).arg( raw ) );
	} else if ( v == QLatin1String( "ghost" ) ) {
		logLine( cellEditGhostLine( s.ghost ) );
	} else if ( v == QLatin1String( "save" ) ) {
		QString l;
		if ( cellEditSaveSession( &l ) ) {
			x.waitReopen = true;
			x.reopenFrames = 0;
		}
	} else if ( v == QLatin1String( "reopened" ) ) {	// wait for the saved .wwcell to be the open document
		if ( x.waitReopen ) {
			if ( QFileInfo( cellWorkspacePath() ) != QFileInfo( x.lastReopen ) ) {
				if ( ++x.reopenFrames > 20000 ) {
					logLine( QStringLiteral( "reopened FAIL timeout, open %1" ).arg( cellWorkspacePath() ) );
					x.waitReopen = false;
					return true;
				}
				return false;
			}
			x.waitReopen = false;
			logLine( QStringLiteral( "reopened %1" ).arg( cellWorkspacePath() ) );
		}
	} else if ( v == QLatin1String( "readback" ) ) {
		readback();
	} else if ( v == QLatin1String( "stats" ) ) {
		logLine( cellSurfaceStatsLine() );
	} else if ( v == QLatin1String( "quit" ) ) {
		logLine( QStringLiteral( "quit" ) );
		QTimer::singleShot( 0, qApp, &QCoreApplication::quit );
	} else {
		logLine( QStringLiteral( "step FAIL unknown verb %1" ).arg( v ) );
	}
	return true;
}

void runScript( GLView * view )
{
	Extra & x = extra();
	if ( !x.scriptLoaded ) {
		x.scriptLoaded = true;
		const QString path = wwEnvStr( "WW_CELL_EDIT_SCRIPT" ).trimmed();
		if ( path.isEmpty() )
			return;
		QFile f( path );
		if ( f.open( QIODevice::ReadOnly ) )
			x.script = QString::fromUtf8( f.readAll() ).split( QLatin1Char( '\n' ) );
	}
	if ( x.scriptAt >= x.script.size() )
		return;
	if ( x.waitFrames > 0 ) {
		x.waitFrames--;
		return;
	}
	while ( x.scriptAt < x.script.size() ) {
		const QString step = x.script[x.scriptAt].trimmed();
		if ( !scriptStep( view, step ) )
			return;
		x.scriptAt++;
		const QString v = step.section( QLatin1Char( ' ' ), 0, 0 ).toLower();
		// one probing or waiting step per frame
		if ( x.waitFrames > 0 || !x.queue.empty() || v == QLatin1String( "save" ) || v == QLatin1String( "start" ) )
			return;
	}
}

} // namespace

// ================================================================== GLView hooks
bool cellEditMouse( GLView * view, CellEditMouse kind, double x, double y, bool left, int modifiers, int wheel )
{
	CellEditSession & s = cellEditSession();
	if ( !s.placing )
		return false;
	Extra & e = extra();
	e.view = view;
	switch ( kind ) {
	case CellEditMouse::Move:
		if ( e.lmbDown && s.ghostValid && s.locked ) {
			if ( std::hypot( x - e.pressX, y - e.pressY ) > 4.0 )
				e.dragging = true;
			if ( e.dragging ) {
				Extra::Action a;
				a.kind = Extra::Action::Face;
				a.x = x;
				a.y = y;
				e.queue.push_back( a );
			}
			view->update();
			return true;
		}
		e.hoverPending = true;
		e.hoverX = x;
		e.hoverY = y;
		view->update();
		return false;	// the camera keeps the move
	case CellEditMouse::Press: {
		if ( !left )
			return false;
		Extra::Action a;
		a.x = x;
		a.y = y;
		if ( s.facing && s.ghostValid ) {
			a.kind = Extra::Action::Face;
			e.queue.push_back( a );
			Extra::Action c;
			c.kind = Extra::Action::Commit;
			e.queue.push_back( c );
			e.lmbDown = false;
		} else {
			a.kind = Extra::Action::Anchor;
			e.queue.push_back( a );
			e.lmbDown = true;
			e.dragging = false;
			e.pressX = x;
			e.pressY = y;
		}
		view->update();
		return true;
	}
	case CellEditMouse::Release:
		if ( !left || !e.lmbDown )
			return false;
		e.lmbDown = false;
		{
			Extra::Action c;
			c.kind = Extra::Action::Commit;
			e.queue.push_back( c );
		}
		view->update();
		return true;
	case CellEditMouse::Wheel:
		if ( !s.turning || wheel == 0 )
			return false;
		s.yaw += ( wheel > 0 ? 1.0f : -1.0f ) * ( ( modifiers & Qt::ControlModifier ) ? 1.0f : 15.0f ) * kPi / 180.0f;
		updatePose( s );
		view->update();
		changed();
		return true;
	}
	return false;
}

bool cellEditKey( GLView * view, int key, int modifiers )
{
	CellEditSession & s = cellEditSession();
	if ( !s.placing ) {
		if ( key == Qt::Key_A && ( modifiers & Qt::ShiftModifier ) && !( modifiers & ( Qt::ControlModifier | Qt::AltModifier ) )
			&& cellEditEnabled() && !cellWorkspacePath().isEmpty() )
			return cellEditStartPlace( view ) || true;
		return false;
	}
	extra().view = view;
	switch ( key ) {
	case Qt::Key_Escape:
		if ( s.locked || s.facing ) {
			s.locked = s.facing = false;
			extra().lmbDown = false;
			say( QStringLiteral( "Place: cancelled the drop" ), false );
		} else {
			cellEditStopPlace();
			say( QStringLiteral( "Place mode off" ), false );
		}
		break;
	case Qt::Key_T:
		if ( s.ghostValid ) {
			s.locked = s.facing = true;
			say( QStringLiteral( "Face: click the point it should face" ), false );
		}
		break;
	case Qt::Key_R:
		s.turning = !s.turning;
		say( s.turning ? QStringLiteral( "Turn: the wheel turns it (15 deg, Ctrl 1 deg); R again ends" )
			: QStringLiteral( "Turn off" ), false );
		break;
	case Qt::Key_N:
		s.flat = !s.flat;
		updatePose( s );
		say( s.flat ? QStringLiteral( "Align: flat on the surface" ) : QStringLiteral( "Align: Z up" ), false );
		break;
	case Qt::Key_G:
		s.snap = !s.snap;
		say( s.snap ? QStringLiteral( "Ground snap on" ) : QStringLiteral( "Ground snap off" ), false );
		break;
	case Qt::Key_Z:
		if ( !( modifiers & Qt::ControlModifier ) )
			return false;
		cellEditUndo();
		break;
	default:
		return false;
	}
	view->update();
	changed();
	return true;
}

void cellEditFrame( GLView * view, Scene * scene, const Transform & viewTrans )
{
	if ( !view || !scene || scene->selecting )
		return;
	if ( cellWorkspacePath().isEmpty() )
		return;
	Extra & x = extra();
	x.view = view;
	x.scene = scene;
	runScript( view );
	CellEditSession & s = cellEditSession();
	if ( s.placing ) {
		if ( x.hoverPending ) {
			x.hoverPending = false;
			Extra::Action h;
			h.kind = Extra::Action::Hover;
			h.x = x.hoverX;
			h.y = x.hoverY;
			x.queue.insert( x.queue.begin(), h );
		}
		const std::vector<Extra::Action> q = std::move( x.queue );
		x.queue.clear();
		for ( const Extra::Action & a : q )
			runAction( view, scene, a );
	} else {
		x.queue.clear();
	}
	drawEdits( scene, viewTrans );
	if ( x.scriptAt < x.script.size() || !x.queue.empty() )
		QTimer::singleShot( 0, view, [view]() { view->update(); } );
}
