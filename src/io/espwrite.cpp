// Lane ESPWRITE1 (2026-11-02): the cell-edit plugin writer. See espwrite.h for the measured rules.
#include "espwrite.h"

#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QSaveFile>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace EspWrite
{

QString red()
{
	return QString::fromLocal8Bit( qgetenv( "WW_ESPWRITE_RED" ) ).trimmed().toLower();
}

static void fail( QString * error, const QString & why )
{
	if ( error )
		*error = why;
}

static QString hex8( quint32 v )
{
	return QStringLiteral( "%1" ).arg( v, 8, 16, QLatin1Char( '0' ) ).toUpper();
}

static void putU16( QByteArray & b, quint16 v )
{
	char c[2];
	qToLittleEndian<quint16>( v, c );
	b.append( c, 2 );
}

static void putU32( QByteArray & b, quint32 v )
{
	char c[4];
	qToLittleEndian<quint32>( v, c );
	b.append( c, 4 );
}

static void putF32( QByteArray & b, float f )
{
	quint32 v;
	std::memcpy( &v, &f, 4 );
	putU32( b, v );
}

static quint32 getU32( const QByteArray & b, qsizetype at )
{
	return qFromLittleEndian<quint32>( b.constData() + at );
}

static quint16 getU16( const QByteArray & b, qsizetype at )
{
	return qFromLittleEndian<quint16>( b.constData() + at );
}

// ------------------------------------------------------------------ primitives
QByteArray sub( const char * tag, const QByteArray & data )
{
	QByteArray b;
	if ( data.size() > 0xFFFF ) {
		b.append( "XXXX", 4 );
		putU16( b, 4 );
		putU32( b, quint32( data.size() ) );
		b.append( tag, 4 );
		putU16( b, 0 );
	} else {
		b.append( tag, 4 );
		putU16( b, quint16( data.size() ) );
	}
	b.append( data );
	return b;
}

QByteArray subU32( const char * tag, quint32 v )
{
	QByteArray d;
	putU32( d, v );
	return sub( tag, d );
}

QByteArray subZString( const char * tag, const QString & text )
{
	QByteArray d = text.toLatin1();
	d.append( '\0' );
	return sub( tag, d );
}

QByteArray newRecordTail()
{
	return QByteArray::fromHex( "0000000083000000" );
}

QByteArray record( const char * sig, quint32 flags, quint32 formID, const QByteArray & payload, const QByteArray & tail )
{
	QByteArray b;
	b.append( sig, 4 );
	putU32( b, quint32( payload.size() ) );
	putU32( b, flags );
	putU32( b, formID );
	b.append( tail.left( 8 ) );
	b.append( payload );
	return b;
}

QByteArray group( quint32 label, quint32 type, const QByteArray & body )
{
	QByteArray b;
	b.append( "GRUP", 4 );
	putU32( b, quint32( 24 + body.size() ) );
	putU32( b, label );
	putU32( b, type );
	b.append( QByteArray( 8, '\0' ) );
	b.append( body );
	return b;
}

quint32 label4( const char * fourcc )
{
	return qFromLittleEndian<quint32>( fourcc );
}

quint32 recordFlags( const QByteArray & rec )
{
	return rec.size() >= 24 ? getU32( rec, 8 ) : 0;
}

quint32 recordForm( const QByteArray & rec )
{
	return rec.size() >= 24 ? getU32( rec, 12 ) : 0;
}

QByteArray recordSig( const QByteArray & rec )
{
	return rec.left( 4 );
}

QByteArray withFlags( const QByteArray & rec, quint32 set, quint32 clear )
{
	QByteArray r = rec;
	if ( r.size() >= 24 ) {
		quint32 f = ( getU32( r, 8 ) | set ) & ~clear;
		qToLittleEndian<quint32>( f, r.data() + 8 );
	}
	return r;
}

bool recordPayload( const QByteArray & rec, QByteArray & payload, QString * error )
{
	if ( rec.size() < 24 ) {
		fail( error, QStringLiteral( "record shorter than its header" ) );
		return false;
	}
	quint32 size = getU32( rec, 4 );
	if ( qsizetype( 24 + size ) > rec.size() ) {
		fail( error, QStringLiteral( "record %1 runs past its bytes" ).arg( hex8( recordForm( rec ) ) ) );
		return false;
	}
	QByteArray body = rec.mid( 24, size );
	if ( !( getU32( rec, 8 ) & kRecCompressed ) ) {
		payload = body;
		return true;
	}
	if ( body.size() < 4 ) {
		fail( error, QStringLiteral( "compressed record %1 has no size" ).arg( hex8( recordForm( rec ) ) ) );
		return false;
	}
	quint32 want = getU32( body, 0 );
	QByteArray z;    // qUncompress takes a big-endian size, then the zlib stream
	char be[4];
	qToBigEndian<quint32>( want, be );
	z.append( be, 4 );
	z.append( body.mid( 4 ) );
	payload = qUncompress( z );
	if ( quint32( payload.size() ) != want ) {
		fail( error, QStringLiteral( "compressed record %1 does not inflate to %2 bytes" )
			.arg( hex8( recordForm( rec ) ) ).arg( want ) );
		return false;
	}
	return true;
}

bool splitSubs( const QByteArray & payload, QVector<Sub> & out, QString * error )
{
	out.clear();
	qsizetype p = 0;
	qint64 big = -1;
	while ( p < payload.size() ) {
		if ( p + 6 > payload.size() ) {
			fail( error, QStringLiteral( "subrecord header past the payload" ) );
			return false;
		}
		QByteArray tag = payload.mid( p, 4 );
		qint64 n = getU16( payload, p + 4 );
		if ( tag == "XXXX" ) {
			if ( n != 4 || p + 10 > payload.size() ) {
				fail( error, QStringLiteral( "XXXX of %1 bytes" ).arg( n ) );
				return false;
			}
			big = getU32( payload, p + 6 );
			p += 10;
			continue;
		}
		if ( big >= 0 ) {
			n = big;
			big = -1;
		}
		if ( p + 6 + n > payload.size() ) {
			fail( error, QStringLiteral( "subrecord %1 runs past the payload" ).arg( QString::fromLatin1( tag ) ) );
			return false;
		}
		out.append( Sub{ tag, payload.mid( p + 6, n ) } );
		p += 6 + n;
	}
	return true;
}

QByteArray joinSubs( const QVector<Sub> & subs )
{
	QByteArray b;
	for ( const Sub & s : subs )
		b.append( sub( s.tag.constData(), s.data ) );
	return b;
}

// ------------------------------------------------------------------ label rules
quint32 interiorBlockLabel( quint32 cellForm )
{
	return ( cellForm & 0xFFFFFF ) % 10;
}

quint32 interiorSubLabel( quint32 cellForm )
{
	return ( ( cellForm & 0xFFFFFF ) / 10 ) % 10;
}

static quint32 pairLabel( int first, int second )
{
	return quint32( quint16( qint16( first ) ) ) | ( quint32( quint16( qint16( second ) ) ) << 16 );
}

quint32 exteriorBlockLabel( int x, int y )
{
	if ( red() == QLatin1String( "xy" ) )
		return pairLabel( x >> 5, y >> 5 );
	return pairLabel( y >> 5, x >> 5 );    // Y first: 38,965 / 38,965 Fallout4.esm cells
}

quint32 exteriorSubLabel( int x, int y )
{
	if ( red() == QLatin1String( "xy" ) )
		return pairLabel( x >> 3, y >> 3 );
	return pairLabel( y >> 3, x >> 3 );
}

bool cellGrid( const QByteArray & cellRec, int & x, int & y, QString * error )
{
	QByteArray payload;
	QVector<Sub> subs;
	if ( !recordPayload( cellRec, payload, error ) || !splitSubs( payload, subs, error ) )
		return false;
	for ( const Sub & s : subs ) {
		if ( s.tag == "XCLC" && s.data.size() >= 8 ) {
			x = qint32( getU32( s.data, 0 ) );
			y = qint32( getU32( s.data, 4 ) );
			return true;
		}
	}
	fail( error, QStringLiteral( "exterior CELL %1 has no XCLC" ).arg( hex8( recordForm( cellRec ) ) ) );
	return false;
}

// ------------------------------------------------------------------ pose
void yawToward( const float from[3], const float to[3], float rot[3] )
{
	double dx = double( to[0] ) - from[0], dy = double( to[1] ) - from[1];
	/* fromEuler( 0, 0, -rz ) * ( 0, 1, 0 ) = ( sin rz, cos rz, 0 ) (niftypes.cpp m[0][1] = -cosY sinZ,
	 * m[1][1] = cosX cosZ - ...), so rz = atan2( dx, dy ): clockwise from +Y seen from above. */
	double rz = std::atan2( dx, dy );
	if ( red() == QLatin1String( "yaw" ) )
		rz = -rz;
	rot[0] = 0.0f;
	rot[1] = 0.0f;
	rot[2] = float( rz );
}

// ------------------------------------------------------------------ tree
QByteArray emitNode( const Node & n, int * records, int * groups )
{
	if ( !n.isGroup ) {
		if ( records )
			++*records;
		return n.raw;
	}
	QByteArray body;
	for ( const Node & k : n.kids )
		body.append( emitNode( k, records, groups ) );
	if ( groups )
		++*groups;
	return group( n.label, n.type, body );
}

bool parseNodes( const QByteArray & file, qsizetype begin, qsizetype end, QVector<Node> & out, QString * error )
{
	qsizetype p = begin;
	while ( p < end ) {
		if ( p + 24 > end ) {
			fail( error, QStringLiteral( "header at %1 runs past its group" ).arg( p ) );
			return false;
		}
		Node n;
		if ( std::memcmp( file.constData() + p, "GRUP", 4 ) == 0 ) {
			quint32 size = getU32( file, p + 4 );
			if ( size < 24 || p + size > end ) {
				fail( error, QStringLiteral( "group at %1 (%2 bytes) overruns its parent" ).arg( p ).arg( size ) );
				return false;
			}
			n.isGroup = true;
			n.label = getU32( file, p + 8 );
			n.type = getU32( file, p + 12 );
			n.stamp = file.mid( p + 16, 8 );
			if ( !parseNodes( file, p + 24, p + size, n.kids, error ) )
				return false;
			p += size;
		} else {
			quint32 size = getU32( file, p + 4 );
			if ( p + 24 + qsizetype( size ) > end ) {
				fail( error, QStringLiteral( "record at %1 overruns its group" ).arg( p ) );
				return false;
			}
			n.raw = file.mid( p, 24 + size );
			p += 24 + size;
		}
		out.append( n );
	}
	return true;
}

// ------------------------------------------------------------------ index
bool PluginIndex::open( const QString & path, QString * error )
{
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		fail( error, QStringLiteral( "%1: cannot read" ).arg( path ) );
		return false;
	}
	data = f.readAll();
	fileName = QFileInfo( path ).fileName();
	if ( data.size() < 24 || !data.startsWith( "TES4" ) ) {
		fail( error, QStringLiteral( "%1: not a plugin" ).arg( fileName ) );
		return false;
	}
	flags = getU32( data, 8 );
	t4end = 24 + qint64( getU32( data, 4 ) );
	QVector<Sub> t4;
	if ( !splitSubs( data.mid( 24, t4end - 24 ), t4, error ) )
		return false;
	masterList.clear();
	for ( const Sub & s : t4 )
		if ( s.tag == "MAST" )
			masterList << QString::fromLatin1( s.data.constData() );
	groups.clear();
	recs.clear();
	pcells.clear();
	recs.reserve( int( data.size() / 200 ) );
	struct Open { qint64 end; int idx; };
	QVector<Open> stack;
	qint64 p = t4end, n = data.size();
	const char * d = data.constData();
	while ( p < n ) {
		while ( !stack.isEmpty() && p >= stack.last().end )
			stack.removeLast();
		if ( p + 24 > n ) {
			fail( error, QStringLiteral( "%1: header at %2 past the end" ).arg( fileName ).arg( p ) );
			return false;
		}
		int parent = stack.isEmpty() ? -1 : stack.last().idx;
		quint32 size = qFromLittleEndian<quint32>( d + p + 4 );
		if ( std::memcmp( d + p, "GRUP", 4 ) == 0 ) {
			if ( size < 24 || p + size > n ) {
				fail( error, QStringLiteral( "%1: group at %2 overruns the file" ).arg( fileName ).arg( p ) );
				return false;
			}
			Group g;
			g.label = qFromLittleEndian<quint32>( d + p + 8 );
			g.type = qFromLittleEndian<quint32>( d + p + 12 );
			g.offset = p;
			g.parent = parent;
			groups.append( g );
			stack.append( Open{ p + size, int( groups.size() - 1 ) } );
			p += 24;
			continue;
		}
		Rec r;
		r.offset = p;
		r.size = 24 + size;
		r.group = parent;
		quint32 fid = qFromLittleEndian<quint32>( d + p + 12 );
		recs.insert( fid, r );
		if ( parent >= 0 && groups[parent].type == 1 && std::memcmp( d + p, "CELL", 4 ) == 0 )
			pcells.insert( groups[parent].label, fid );
		p += 24 + qint64( size );
	}
	return true;
}

QByteArray PluginIndex::recordBytes( quint32 formID ) const
{
	auto it = recs.constFind( formID );
	if ( it == recs.constEnd() )
		return QByteArray();
	return data.mid( it->offset, it->size );
}

QVector<PluginIndex::Group> PluginIndex::chain( quint32 formID ) const
{
	QVector<Group> c;
	auto it = recs.constFind( formID );
	if ( it == recs.constEnd() )
		return c;
	for ( int g = it->group; g >= 0; g = groups[g].parent )
		c.append( groups[g] );
	return c;
}

// ------------------------------------------------------------------ build
QString Report::line( const QString & file ) const
{
	return QStringLiteral( "wrote %1: %2 CELL, %3 REFR new, %4 override, %5 records, %6 groups, %7 bytes%8" )
		.arg( file ).arg( cells ).arg( newRefs ).arg( overrides ).arg( records ).arg( groups ).arg( bytes )
		.arg( fullDropped ? QStringLiteral( " (%1 localised FULL dropped)" ).arg( fullDropped ) : QString() );
}

/* A copied record's form ids are in its SOURCE's numbering. They read the same in ours only when the source's
 * masters are our first masters, in order, and the source itself comes right after them. Anything else needs a
 * per-field remap, which this writer refuses by name instead of guessing. */
static bool sameNumbering( const Plan & plan, const Copied & c, QString * error )
{
	int k = c.sourceMasters.size();
	bool ok = k < plan.masters.size() && plan.masters.at( k ).compare( c.source, Qt::CaseInsensitive ) == 0;
	for ( int i = 0; ok && i < k; ++i )
		ok = plan.masters.at( i ).compare( c.sourceMasters.at( i ), Qt::CaseInsensitive ) == 0;
	if ( !ok )
		fail( error, QStringLiteral( "record %1 is copied from %2, whose form ids do not read the same in this "
			"plugin's master list (a remap is not supported)" ).arg( hex8( recordForm( c.raw ) ) ).arg( c.source ) );
	return ok;
}

//! The bytes a copied CELL/WRLD is written as: verbatim, unless a localised FULL must leave a non-localised plugin.
static bool copyBytes( const Copied & c, QByteArray & out, Report & rep, QString * error )
{
	out = c.raw;
	if ( !c.sourceLocalized )
		return true;
	QByteArray payload;
	QVector<Sub> subs;
	if ( !recordPayload( c.raw, payload, error ) || !splitSubs( payload, subs, error ) )
		return false;
	int at = -1;
	for ( int i = 0; i < subs.size(); ++i )
		if ( subs[i].tag == "FULL" )
			at = i;
	if ( at < 0 )
		return true;
	if ( c.full.isEmpty() ) {
		subs.removeAt( at );
		++rep.fullDropped;
	} else {
		subs[at].data = c.full.toLatin1() + QByteArray( 1, '\0' );
		++rep.fullInlined;
	}
	QByteArray sig = recordSig( c.raw );
	out = record( sig.constData(), recordFlags( c.raw ) & ~kRecCompressed, recordForm( c.raw ), joinSubs( subs ),
		c.raw.mid( 16, 8 ) );
	return true;
}

namespace
{
struct CellOut
{
	QByteArray rec;
	QVector<QByteArray> pers, temp;
};
struct WorldOut
{
	QByteArray rec, pcell;
	quint32 pcellForm = 0;
	QVector<QByteArray> pers;
	QMap<quint32, CellOut> grid;
};
}

static Node recordNode( const QByteArray & raw )
{
	Node n;
	n.raw = raw;
	return n;
}

static Node groupNode( quint32 label, quint32 type )
{
	Node n;
	n.isGroup = true;
	n.label = label;
	n.type = type;
	return n;
}

static Node childrenNode( quint32 cellForm, const QVector<QByteArray> & pers, const QVector<QByteArray> & temp )
{
	Node six = groupNode( cellForm, 6 );
	if ( !pers.isEmpty() ) {
		Node g = groupNode( cellForm, 8 );
		for ( const QByteArray & r : pers )
			g.kids.append( recordNode( r ) );
		six.kids.append( g );
	}
	if ( !temp.isEmpty() ) {
		Node g = groupNode( cellForm, 9 );
		for ( const QByteArray & r : temp )
			g.kids.append( recordNode( r ) );
		six.kids.append( g );
	}
	return six;
}

bool build( Plan & plan, QByteArray & out, Report * report, QString * error )
{
	Report rep;
	const QString r = red();
	if ( plan.masters.isEmpty() ) {
		fail( error, QStringLiteral( "no master: a cell edit names the plugin it edits" ) );
		return false;
	}
	for ( const QString & m : plan.masters ) {
		if ( !plan.target.isEmpty() && m.compare( plan.target, Qt::CaseInsensitive ) == 0 ) {
			fail( error, QStringLiteral( "%1 is a master of the load order: never written" ).arg( m ) );
			return false;
		}
	}
	if ( plan.target.endsWith( QLatin1String( ".esm" ), Qt::CaseInsensitive ) ) {
		fail( error, QStringLiteral( "%1: the writer makes .esp / .esl files only" ).arg( plan.target ) );
		return false;
	}
	if ( plan.cells.isEmpty() ) {
		fail( error, QStringLiteral( "no cell edits" ) );
		return false;
	}
	const quint32 nm = quint32( plan.masters.size() );
	if ( nm > 0xFE ) {
		fail( error, QStringLiteral( "%1 masters: past the 254 a plugin can name" ).arg( nm ) );
		return false;
	}
	quint32 local = kLightFirst;
	QVector<quint32> onam;
	QMap<quint32, CellOut> interior;
	QMap<quint32, WorldOut> worlds;

	for ( CellEdit & c : plan.cells ) {
		if ( c.cell.raw.isEmpty() || recordSig( c.cell.raw ) != "CELL" ) {
			fail( error, QStringLiteral( "no cell open: the edit names no CELL record" ) );
			return false;
		}
		if ( !sameNumbering( plan, c.cell, error ) )
			return false;
		quint32 cf = recordForm( c.cell.raw );
		QVector<QByteArray> addP, addT, ovP, ovT;
		for ( RefAdd & a : c.adds ) {
			if ( ( a.base >> 24 ) >= nm ) {
				fail( error, QStringLiteral( "base %1 names no master of this plugin" ).arg( hex8( a.base ) ) );
				return false;
			}
			if ( plan.light && local > kLightLast ) {
				fail( error, QStringLiteral( "light plugin full: 2048 references; untick ESL" ) );
				return false;
			}
			if ( local > 0xFFFFFF ) {
				fail( error, QStringLiteral( "plugin full: 16,777,216 ids" ) );
				return false;
			}
			a.formOut = ( nm << 24 ) | local++;
			QByteArray payload = subU32( "NAME", a.base );
			if ( a.scale != 1.0f ) {
				QByteArray s;
				putF32( s, a.scale );
				payload += sub( "XSCL", s );
			}
			QByteArray d;
			for ( float v : a.pos )
				putF32( d, v );
			for ( float v : a.rot )
				putF32( d, v );
			payload += sub( "DATA", d );
			quint32 fl = ( a.persistent ? kRefPersistent : 0 ) | ( a.disabled ? kRefDisabled : 0 );
			QByteArray rec = record( "REFR", fl, a.formOut, payload );
			bool toPers = a.persistent && r != QLatin1String( "temp" );
			( toPers ? addP : addT ).append( rec );
			++rep.newRefs;
		}
		for ( const RefOverride & o : c.overrides ) {
			if ( recordSig( o.rec.raw ) != "REFR" && recordSig( o.rec.raw ) != "ACHR" ) {
				fail( error, QStringLiteral( "override is not a REFR / ACHR" ) );
				return false;
			}
			if ( !sameNumbering( plan, o.rec, error ) )
				return false;
			( o.persistent ? ovP : ovT ).append( o.rec.raw );
			quint32 of = recordForm( o.rec.raw );
			if ( !o.persistent && ( of >> 24 ) < nm )
				onam.append( of );
			++rep.overrides;
		}
		if ( c.world ) {
			if ( recordSig( c.worldRec.raw ) != "WRLD" || recordForm( c.worldRec.raw ) != c.world ) {
				fail( error, QStringLiteral( "exterior edit without its WRLD %1" ).arg( hex8( c.world ) ) );
				return false;
			}
			if ( !sameNumbering( plan, c.worldRec, error ) )
				return false;
			WorldOut & w = worlds[c.world];
			if ( w.rec.isEmpty() && !copyBytes( c.worldRec, w.rec, rep, error ) )
				return false;
			if ( !addP.isEmpty() || !ovP.isEmpty() ) {
				if ( recordSig( c.persistentCell.raw ) != "CELL" ) {
					fail( error, QStringLiteral( "a persistent exterior ref needs the world's persistent cell" ) );
					return false;
				}
				if ( !sameNumbering( plan, c.persistentCell, error ) )
					return false;
				if ( w.pcell.isEmpty() ) {
					if ( !copyBytes( c.persistentCell, w.pcell, rep, error ) )
						return false;
					w.pcellForm = recordForm( c.persistentCell.raw );
				}
				w.pers += addP;
				w.pers += ovP;
			}
			if ( !addT.isEmpty() || !ovT.isEmpty() ) {
				CellOut & g = w.grid[cf];
				if ( g.rec.isEmpty() && !copyBytes( c.cell, g.rec, rep, error ) )
					return false;
				g.temp += addT;
				g.temp += ovT;
			}
		} else {
			CellOut & g = interior[cf];
			if ( g.rec.isEmpty() && !copyBytes( c.cell, g.rec, rep, error ) )
				return false;
			g.pers += addP;
			g.pers += ovP;
			g.temp += addT;
			g.temp += ovT;
		}
	}

	QVector<Node> top;
	if ( !interior.isEmpty() ) {
		QMap<quint32, QMap<quint32, QVector<quint32>>> blocks;   // label as u32 ascending, cells by form id
		for ( auto it = interior.constBegin(); it != interior.constEnd(); ++it )
			blocks[interiorBlockLabel( it.key() )][interiorSubLabel( it.key() )].append( it.key() );
		Node cellTop = groupNode( label4( "CELL" ), 0 );
		for ( auto b = blocks.constBegin(); b != blocks.constEnd(); ++b ) {
			Node bn = groupNode( b.key(), 2 );
			for ( auto s = b->constBegin(); s != b->constEnd(); ++s ) {
				Node sn = groupNode( s.key(), 3 );
				for ( quint32 cf : *s ) {
					const CellOut & g = interior[cf];
					sn.kids.append( recordNode( g.rec ) );
					sn.kids.append( childrenNode( cf, g.pers, g.temp ) );
					++rep.cells;
					++rep.interiorCells;
				}
				bn.kids.append( sn );
			}
			cellTop.kids.append( bn );
		}
		top.append( cellTop );
	}
	if ( !worlds.isEmpty() ) {
		Node wrldTop = groupNode( label4( "WRLD" ), 0 );
		for ( auto w = worlds.constBegin(); w != worlds.constEnd(); ++w ) {
			wrldTop.kids.append( recordNode( w->rec ) );
			Node one = groupNode( w.key(), 1 );
			if ( !w->pers.isEmpty() ) {
				one.kids.append( recordNode( w->pcell ) );
				one.kids.append( childrenNode( w->pcellForm, w->pers, {} ) );
				++rep.cells;
			}
			QMap<quint32, QMap<quint32, QVector<quint32>>> blocks;
			for ( auto g = w->grid.constBegin(); g != w->grid.constEnd(); ++g ) {
				int x = 0, y = 0;
				if ( !cellGrid( g->rec, x, y, error ) )
					return false;
				blocks[exteriorBlockLabel( x, y )][exteriorSubLabel( x, y )].append( g.key() );
			}
			for ( auto b = blocks.constBegin(); b != blocks.constEnd(); ++b ) {
				Node bn = groupNode( b.key(), 4 );
				for ( auto s = b->constBegin(); s != b->constEnd(); ++s ) {
					Node sn = groupNode( s.key(), 5 );
					for ( quint32 cf : *s ) {
						const CellOut & g = w->grid[cf];
						sn.kids.append( recordNode( g.rec ) );
						sn.kids.append( childrenNode( cf, {}, g.temp ) );
						++rep.cells;
					}
					bn.kids.append( sn );
				}
				one.kids.append( bn );
			}
			wrldTop.kids.append( one );
		}
		top.append( wrldTop );
	}

	QByteArray body;
	for ( const Node & n : top )
		body.append( emitNode( n, &rep.records, &rep.groups ) );
	std::sort( onam.begin(), onam.end() );
	rep.onam = onam;
	rep.nextId = local;

	QByteArray t4;
	{
		QByteArray h;
		putF32( h, 1.0f );
		putU32( h, quint32( rep.records + rep.groups ) + ( r == QLatin1String( "count" ) ? 1 : 0 ) );
		putU32( h, local );
		t4 += sub( "HEDR", h );
	}
	t4 += subZString( "CNAM", plan.author );
	for ( const QString & m : plan.masters ) {
		t4 += subZString( "MAST", m );
		t4 += sub( "DATA", QByteArray( 8, '\0' ) );
	}
	if ( !onam.isEmpty() && r != QLatin1String( "onam" ) ) {
		QByteArray o;
		for ( quint32 f : onam )
			putU32( o, f );
		t4 += sub( "ONAM", o );
	}
	t4 += subU32( "INTV", 1 );
	t4 += subU32( "INCC", quint32( rep.interiorCells ) );
	out = record( "TES4", plan.light ? kTes4Light : 0, 0, t4 ) + body;
	rep.bytes = out.size();
	if ( report )
		*report = rep;
	return true;
}

bool write( const QString & path, Plan & plan, Report * report, QString * error )
{
	if ( plan.target.isEmpty() )
		plan.target = QFileInfo( path ).fileName();
	QByteArray bytes;
	if ( !build( plan, bytes, report, error ) )
		return false;
	QSaveFile f( path );
	if ( !f.open( QIODevice::WriteOnly ) ) {
		fail( error, QStringLiteral( "%1: cannot open for writing (%2)" ).arg( path, f.errorString() ) );
		return false;
	}
	if ( f.write( bytes ) != bytes.size() ) {
		fail( error, QStringLiteral( "%1: write failed (%2)" ).arg( path, f.errorString() ) );
		f.cancelWriting();
		return false;
	}
	if ( !f.commit() ) {
		fail( error, QStringLiteral( "%1: commit failed, the file may be open in another program (%2)" )
			.arg( path, f.errorString() ) );
		return false;
	}
	return true;
}

} // namespace EspWrite
