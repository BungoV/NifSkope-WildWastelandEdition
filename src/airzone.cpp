#include "airzone.h"

#include "proberooms.h"	// GiVolumeSet (plain types; no link to proberooms.cpp)
#include "recx.h"
#include "wwswitch.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>

namespace {

const char * const kShapeNames[5] = { "box", "cylinder", "wedge", "capsule", "sphere" };

// a float as the shortest decimal that reads back (through a double, as the reader takes it) to the same float; -0 as 0
QJsonValue num( float f )
{
	if ( !std::isfinite( f ) || f == 0.0f )
		return QJsonValue( 0 );
	for ( int p = 6; p <= 9; p++ ) {
		const double d = QString::number( double( f ), 'g', p ).toDouble();
		if ( float( d ) == f )
			return QJsonValue( d );
	}
	return QJsonValue( double( f ) );
}

QJsonArray vec3( const float v[3] )
{
	return QJsonArray{ num( v[0] ), num( v[1] ), num( v[2] ) };
}

bool vec3Of( const QJsonValue & v, float out[3] )
{
	const QJsonArray a = v.toArray();
	if ( a.size() != 3 )
		return false;
	for ( int c = 0; c < 3; c++ ) {
		if ( !a.at( c ).isDouble() )
			return false;
		out[c] = float( a.at( c ).toDouble() );
	}
	return true;
}

// a number field: absent = the default; out of [lo, hi] = clamped and said
float numOf( const QJsonObject & o, const char * key, float def, float lo, float hi, const QString & who, QStringList * warn )
{
	const QJsonValue v = o.value( QLatin1String( key ) );
	if ( v.isUndefined() || v.isNull() )
		return def;
	if ( !v.isDouble() ) {
		if ( warn )
			*warn << QStringLiteral( "%1: %2 is not a number; %3 used" ).arg( who, QLatin1String( key ) ).arg( def );
		return def;
	}
	const double d = v.toDouble();
	if ( ( d < double( lo ) || d > double( hi ) ) && warn )
		*warn << QStringLiteral( "%1: %2 %3 clamped to %4..%5" ).arg( who, QLatin1String( key ) ).arg( d ).arg( lo ).arg( hi );
	return float( std::clamp( d, double( lo ), double( hi ) ) );
}

void rgbOf( const QJsonObject & o, const char * key, const char * alias, float out[3], const QString & who, QStringList * warn )
{
	QJsonValue v = o.value( QLatin1String( key ) );
	if ( v.isUndefined() )
		v = o.value( QLatin1String( alias ) );
	if ( v.isUndefined() )
		return;
	float c[3];
	if ( !vec3Of( v, c ) ) {
		if ( warn )
			*warn << QStringLiteral( "%1: %2 is not [r, g, b]; white used" ).arg( who, QLatin1String( key ) );
		return;
	}
	for ( int k = 0; k < 3; k++ )
		out[k] = std::clamp( c[k], 0.0f, 16.0f );
}

// a block: absent / false = off; {} or {...} = on
bool blockOf( const QJsonObject & v, const char * name, QJsonObject & b )
{
	b = QJsonObject();	// `true` = on with every default (never the last block's fields)
	const QJsonValue x = wwRecxBlockValue( v, QLatin1String( name ), true );
	if ( x.isObject() ) {
		b = x.toObject();
		return true;
	}
	return x.isBool() && x.toBool();
}

QString safeStem( QString s )
{
	s.replace( QRegularExpression( QStringLiteral( "[^A-Za-z0-9_+-]" ) ), QStringLiteral( "_" ) );
	return s;
}

void fnv( quint64 & h, const void * p, size_t n )
{
	const unsigned char * b = static_cast<const unsigned char *>( p );
	for ( size_t i = 0; i < n; i++ ) {
		h ^= b[i];
		h *= Q_UINT64_C( 1099511628211 );
	}
}

void fnvFloat( quint64 & h, float f )
{
	if ( f == 0.0f )
		f = 0.0f;	// -0 hashes as 0 (the writer writes it so)
	fnv( h, &f, sizeof f );
}

int cellOfX( float x )
{
	return int( std::floor( double( x ) / 4096.0 ) );
}

} // namespace

// ---- AirZoneCell

QString AirZoneCell::key() const
{
	return plugin + QStringLiteral( "|0x" ) + QStringLiteral( "%1" ).arg( localId, 8, 16, QLatin1Char( '0' ) ).toUpper();
}

QString AirZoneCell::fileStem() const
{
	return edid.isEmpty() ? QStringLiteral( "%1" ).arg( localId, 8, 16, QLatin1Char( '0' ) ).toUpper() : safeStem( edid );
}

QString AirZoneCell::label() const
{
	if ( interior )
		return edid.isEmpty() ? key() : edid;
	return QStringLiteral( "%1 %2,%3" ).arg( worldEdid.isEmpty() ? QStringLiteral( "World" ) : worldEdid ).arg( gx ).arg( gy );
}

bool AirZoneCell::sameRecord( const AirZoneCell & o ) const
{
	return localId == o.localId && plugin.compare( o.plugin, Qt::CaseInsensitive ) == 0;
}

// ---- the format

bool airZoneParse( const QJsonObject & entry, std::vector<az::Volume> * out, QString * giMode, QStringList * warn )
{
	out->clear();
	const QString gm = wwRecxBlockValue( entry, QStringLiteral( "giMode" ), true ).toString().trimmed().toLower();
	if ( giMode )
		*giMode = gm == QLatin1String( "all" ) ? gm : QStringLiteral( "rooms" );
	if ( !gm.isEmpty() && gm != QLatin1String( "all" ) && gm != QLatin1String( "rooms" ) && warn )
		*warn << QStringLiteral( "giMode \"%1\" unknown; rooms used" ).arg( gm );
	const QJsonArray arr = wwRecxBlockValue( entry, QStringLiteral( "volumes" ), true ).toArray();
	for ( int i = 0; i < arr.size(); i++ ) {
		const QJsonObject o = arr.at( i ).toObject();
		az::Volume v;
		v.name = o.value( QStringLiteral( "name" ) ).toString().toStdString();
		const QString who = QStringLiteral( "volume %1%2" ).arg( i ).arg( v.name.empty() ? QString() : QStringLiteral( " (%1)" ).arg( QString::fromStdString( v.name ) ) );
		if ( !arr.at( i ).isObject() ) {
			if ( warn )
				*warn << who + QStringLiteral( ": not an object; skipped" );
			continue;
		}
		const QJsonValue sh = o.value( QStringLiteral( "shape" ) );
		int type = -1;
		if ( sh.isString() ) {
			for ( int t = 0; t < 5; t++ )
				if ( sh.toString().compare( QLatin1String( kShapeNames[t] ), Qt::CaseInsensitive ) == 0 )
					type = t;
		} else if ( sh.isDouble() && sh.toDouble() >= 0.0 && sh.toDouble() <= 4.0 && sh.toDouble() == std::floor( sh.toDouble() ) ) {
			type = int( sh.toDouble() );
		} else if ( sh.isUndefined() ) {
			type = az::Box;
		}
		if ( type < 0 ) {
			if ( warn )
				*warn << who + QStringLiteral( ": shape not box, cylinder, wedge, capsule or sphere; skipped" );
			continue;
		}
		v.shape.type = type;
		if ( !vec3Of( o.value( QStringLiteral( "center" ) ), v.shape.center ) || !vec3Of( o.value( QStringLiteral( "size" ) ), v.shape.size ) ) {
			if ( warn )
				*warn << who + QStringLiteral( ": center and size must be [x, y, z]; skipped" );
			continue;
		}
		if ( !( v.shape.size[0] > 0.0f && v.shape.size[1] > 0.0f && v.shape.size[2] > 0.0f ) ) {
			if ( warn )
				*warn << who + QStringLiteral( ": a size is not above 0; skipped" );
			continue;
		}
		const QJsonValue rot = o.value( QStringLiteral( "rotation" ) );
		if ( !rot.isUndefined() && !vec3Of( rot, v.shape.rotation ) && warn )
			*warn << who + QStringLiteral( ": rotation is not [x, y, z] degrees; 0 used" );
		v.edge = numOf( o, "edge", 0.0f, 0.0f, 1e6f, who, warn );
		v.priority = int( numOf( o, "priority", 0.0f, -1000.0f, 1000.0f, who, warn ) );
		QJsonObject b;
		if ( blockOf( o, "gi", b ) ) {
			v.gi.on = true;
			const QJsonArray thr = b.value( QStringLiteral( "threshold" ) ).toArray();
			if ( thr.size() == 2 ) {
				v.gi.threshold = true;
				v.gi.rooms[0] = thr.at( 0 ).toInt();
				v.gi.rooms[1] = thr.at( 1 ).toInt();
				v.gi.blendWidth = numOf( b, "blendWidth", 0.0f, 0.0f, 1e5f, who, warn );
			} else if ( b.value( QStringLiteral( "room" ) ).isDouble() ) {
				v.gi.room = b.value( QStringLiteral( "room" ) ).toInt();
				if ( v.gi.room < 1 ) {
					v.gi.on = false;
					if ( warn )
						*warn << who + QStringLiteral( ": gi room must be 1 or more (0 is the outdoors, which has no shapes); gi off" );
				}
			} else {
				v.gi.on = false;
				if ( warn )
					*warn << who + QStringLiteral( ": gi needs \"room\" or \"threshold\": [a, b]; gi off" );
			}
			if ( b.value( QStringLiteral( "seq" ) ).isDouble() )
				v.gi.seq = std::max( -1, b.value( QStringLiteral( "seq" ) ).toInt() );
		}
		if ( blockOf( o, "fog", b ) ) {
			v.fog.on = true;
			const QString w = who + QStringLiteral( " fog" );
			v.fog.scale = numOf( b, "scale", 1.0f, 0.0f, 64.0f, w, warn );
			v.fog.add = numOf( b, "add", 0.0f, 0.0f, 1000.0f, w, warn );
			v.fog.heightFalloff = numOf( b, "heightFalloff", 0.0f, 0.0f, 1e6f, w, warn );
			rgbOf( b, "colour", "color", v.fog.colour, w, warn );
			v.fog.colourStrength = numOf( b, "colourStrength", b.contains( QStringLiteral( "colour" ) ) || b.contains( QStringLiteral( "color" ) ) ? 1.0f : 0.0f,
				0.0f, 1.0f, w, warn );
			v.fog.noise = numOf( b, "noise", 0.0f, 0.0f, 1.0f, w, warn );
			v.fog.noiseScale = numOf( b, "noiseScale", 512.0f, 1.0f, 1e6f, w, warn );
			const QJsonValue d = b.value( QStringLiteral( "noiseDrift" ) );
			if ( !d.isUndefined() && !vec3Of( d, v.fog.noiseDrift ) && warn )
				*warn << w + QStringLiteral( ": noiseDrift is not [x, y, z]; none used" );
		}
		if ( blockOf( o, "particles", b ) ) {
			v.part.on = true;
			const QString w = who + QStringLiteral( " particles" );
			v.part.density = numOf( b, "density", 1.0f, 0.0f, 64.0f, w, warn );
			v.part.light = numOf( b, "light", 1.0f, 0.0f, 64.0f, w, warn );
			rgbOf( b, "tint", "colour", v.part.tint, w, warn );
			v.part.tintStrength = numOf( b, "tintStrength", b.contains( QStringLiteral( "tint" ) ) ? 1.0f : 0.0f, 0.0f, 1.0f, w, warn );
			v.part.type = b.value( QStringLiteral( "type" ) ).toString().toStdString();
			v.part.size = numOf( b, "size", 1.0f, 0.0f, 64.0f, w, warn );
			const QJsonValue d = b.value( QStringLiteral( "drift" ) );
			if ( !d.isUndefined() && !vec3Of( d, v.part.drift ) && warn )
				*warn << w + QStringLiteral( ": drift is not [x, y, z]; none used" );
		}
		if ( !v.gi.on && !v.fog.on && !v.part.on && warn )
			*warn << who + QStringLiteral( ": no gi, fog or particles block; it does nothing" );
		out->push_back( v );
	}
	return !out->empty();
}

QJsonObject airZoneVolumeJson( const az::Volume & v )
{
	QJsonObject o;
	if ( !v.name.empty() )
		o.insert( QStringLiteral( "name" ), QString::fromStdString( v.name ) );
	o.insert( QStringLiteral( "shape" ), QLatin1String( kShapeNames[std::clamp( v.shape.type, 0, 4 )] ) );
	o.insert( QStringLiteral( "center" ), vec3( v.shape.center ) );
	o.insert( QStringLiteral( "size" ), vec3( v.shape.size ) );
	o.insert( QStringLiteral( "rotation" ), vec3( v.shape.rotation ) );
	o.insert( QStringLiteral( "edge" ), num( v.edge ) );
	o.insert( QStringLiteral( "priority" ), v.priority );
	if ( v.gi.on ) {
		QJsonObject g;
		if ( v.gi.threshold ) {
			g.insert( QStringLiteral( "threshold" ), QJsonArray{ v.gi.rooms[0], v.gi.rooms[1] } );
			g.insert( QStringLiteral( "blendWidth" ), num( v.gi.blendWidth ) );
		} else {
			g.insert( QStringLiteral( "room" ), v.gi.room );
		}
		if ( v.gi.seq >= 0 )
			g.insert( QStringLiteral( "seq" ), v.gi.seq );
		o.insert( QStringLiteral( "gi" ), g );
	}
	if ( v.fog.on ) {
		QJsonObject f;
		f.insert( QStringLiteral( "scale" ), num( v.fog.scale ) );
		f.insert( QStringLiteral( "add" ), num( v.fog.add ) );
		f.insert( QStringLiteral( "heightFalloff" ), num( v.fog.heightFalloff ) );
		f.insert( QStringLiteral( "colour" ), vec3( v.fog.colour ) );
		f.insert( QStringLiteral( "colourStrength" ), num( v.fog.colourStrength ) );
		f.insert( QStringLiteral( "noise" ), num( v.fog.noise ) );
		f.insert( QStringLiteral( "noiseScale" ), num( v.fog.noiseScale ) );
		f.insert( QStringLiteral( "noiseDrift" ), vec3( v.fog.noiseDrift ) );
		o.insert( QStringLiteral( "fog" ), f );
	}
	if ( v.part.on ) {
		QJsonObject p;
		p.insert( QStringLiteral( "density" ), num( v.part.density ) );
		p.insert( QStringLiteral( "light" ), num( v.part.light ) );
		p.insert( QStringLiteral( "tint" ), vec3( v.part.tint ) );
		p.insert( QStringLiteral( "tintStrength" ), num( v.part.tintStrength ) );
		if ( !v.part.type.empty() )
			p.insert( QStringLiteral( "type" ), QString::fromStdString( v.part.type ) );
		p.insert( QStringLiteral( "size" ), num( v.part.size ) );
		p.insert( QStringLiteral( "drift" ), vec3( v.part.drift ) );
		o.insert( QStringLiteral( "particles" ), p );
	}
	return o;
}

QJsonObject airZoneEntryJson( const AirZoneCell & cell, const std::vector<az::Volume> & v, const QString & giMode )
{
	QJsonObject e;
	e.insert( QStringLiteral( "name" ), cell.edid.isEmpty() ? cell.label() : cell.edid );
	e.insert( QStringLiteral( "cell" ), cell.key() );
	if ( !cell.interior ) {
		if ( !cell.worldPlugin.isEmpty() )
			e.insert( QStringLiteral( "worldspace" ), cell.worldPlugin + QStringLiteral( "|0x" )
				+ QStringLiteral( "%1" ).arg( cell.worldLocalId, 8, 16, QLatin1Char( '0' ) ).toUpper() );
		e.insert( QStringLiteral( "grid" ), QJsonArray{ cell.gx, cell.gy } );
	}
	e.insert( QStringLiteral( "giMode" ), giMode == QLatin1String( "all" ) ? giMode : QStringLiteral( "rooms" ) );
	QJsonArray a;
	for ( const az::Volume & x : v )
		a.append( airZoneVolumeJson( x ) );
	e.insert( QStringLiteral( "volumes" ), a );
	return e;
}

bool airZoneWriteEntry( const QString & path, const AirZoneCell & cell, const std::vector<az::Volume> & v,
	const QString & giMode, QString * err )
{
	QJsonObject root;
	QFile in( path );
	if ( in.exists() ) {
		if ( !in.open( QIODevice::ReadOnly ) ) {
			if ( err )
				*err = QStringLiteral( "%1: unreadable" ).arg( path );
			return false;
		}
		QJsonParseError pe;
		const QJsonDocument d = QJsonDocument::fromJson( in.readAll(), &pe );
		in.close();
		if ( pe.error != QJsonParseError::NoError || !d.isObject() ) {
			if ( err )
				*err = QStringLiteral( "%1: not JSON (%2); not overwritten" ).arg( path, pe.errorString() );
			return false;
		}
		root = d.object();
	}
	QJsonArray entries = root.value( QStringLiteral( "entries" ) ).toArray();
	const QJsonObject mine = airZoneEntryJson( cell, v, giMode );
	bool replaced = false;
	for ( int i = 0; i < entries.size(); i++ ) {
		QJsonObject e = entries.at( i ).toObject();
		const QString k = e.value( QStringLiteral( "cell" ) ).toString();
		const int bar = k.indexOf( QLatin1Char( '|' ) );
		bool ok = false;
		const quint32 id = bar > 0 ? k.mid( bar + 1 ).trimmed().toUInt( &ok, 0 ) : 0;
		if ( ok && id == cell.localId && k.left( bar ).trimmed().compare( cell.plugin, Qt::CaseInsensitive ) == 0 ) {
			for ( auto it = mine.constBegin(); it != mine.constEnd(); ++it )	// the entry's other fields are kept
				e.insert( it.key(), it.value() );
			entries.replace( i, e );
			replaced = true;
			break;
		}
	}
	if ( !replaced )
		entries.append( mine );
	root.insert( QStringLiteral( "entries" ), entries );
	if ( !QDir().mkpath( QFileInfo( path ).absolutePath() ) ) {
		if ( err )
			*err = QStringLiteral( "%1: cannot make the folder" ).arg( QFileInfo( path ).absolutePath() );
		return false;
	}
	QSaveFile out( path );
	if ( !out.open( QIODevice::WriteOnly ) ) {
		if ( err )
			*err = QStringLiteral( "%1: cannot write" ).arg( path );
		return false;
	}
	out.write( QJsonDocument( root ).toJson( QJsonDocument::Indented ) );
	if ( !out.commit() ) {
		if ( err )
			*err = QStringLiteral( "%1: the write did not finish" ).arg( path );
		return false;
	}
	return true;
}

QString airZoneFilePath( const QString & dataRoot, const QString & provider, const AirZoneCell & cell )
{
	return QDir( dataRoot ).filePath( QStringLiteral( "Plugins/%1/Cells/%2.recx" ).arg( provider, cell.fileStem() ) );
}

std::vector<AirZoneCellVolumes> airZoneLoad( const QStringList & roots, const QStringList & pluginFiles,
	const std::vector<AirZoneCell> & cells, const QString & red )
{
	std::vector<AirZoneCellVolumes> out;
	const bool novol = red.split( QLatin1Char( ',' ), Qt::SkipEmptyParts ).contains( QStringLiteral( "novol" ) );
	for ( const AirZoneCell & c : cells ) {
		AirZoneCellVolumes cv;
		cv.cell = c;
		if ( !novol && !c.plugin.isEmpty() ) {
			const WwRecxHit h = wwRecxFind( roots, pluginFiles, QStringLiteral( "Cells" ), QStringLiteral( "cell" ), c.plugin, c.localId,
				c.edid, QStringLiteral( "volumes" ), true, true );
			if ( h.ok ) {
				cv.file = h.file;
				cv.provider = h.provider;
				cv.rung = h.rung;
				cv.overridden = h.overridden;
				airZoneParse( h.entry, &cv.volumes, &cv.giMode, &cv.warnings );
				const QJsonArray g = h.entry.value( QStringLiteral( "grid" ) ).toArray();
				if ( !c.interior && g.size() == 2 && ( g.at( 0 ).toInt() != c.gx || g.at( 1 ).toInt() != c.gy ) )
					cv.warnings << QStringLiteral( "grid says %1,%2 but the cell %3 is %4,%5: the cell key wins" ).arg( g.at( 0 ).toInt() )
						.arg( g.at( 1 ).toInt() ).arg( c.key() ).arg( c.gx ).arg( c.gy );
			}
		}
		out.push_back( cv );
	}
	return out;
}

// ---- GI

QString airZoneGiMerge( GiVolumeSet & set, const std::vector<AirZoneCellVolumes> & cells )
{
	struct Hand
	{
		long long key;
		GiVolumeShape s;
	};
	struct HandT
	{
		long long key;
		GiThreshold t;
	};
	std::vector<Hand> hs;
	std::vector<HandT> ht;
	QSet<int> rooms;
	QSet<quint64> pairs;
	QSet<quint64> allCells;	// exterior grid (x, y) of the "all" cells
	bool allInterior = false;
	int files = 0, modeAll = 0;
	long long load = 0;
	const long long nS = static_cast<long long>( set.shapes.size() ), nT = static_cast<long long>( set.thresholds.size() );
	auto pairKey = []( int a, int b ) { return ( quint64( quint32( std::min( a, b ) ) ) << 32 ) | quint64( quint32( std::max( a, b ) ) ); };
	auto gridKey = []( int x, int y ) { return ( quint64( quint32( x ) ) << 32 ) | quint64( quint32( y ) ); };
	for ( const AirZoneCellVolumes & cv : cells ) {
		bool any = false;
		const bool all = cv.giMode == QLatin1String( "all" );
		for ( const az::Volume & v : cv.volumes ) {
			if ( !v.gi.on )
				continue;
			any = true;
			GiVolumeShape s;
			s.type = v.shape.type;
			for ( int c = 0; c < 3; c++ ) {
				s.center[c] = v.shape.center[c];
				s.size[c] = v.shape.size[c];
				s.rotation[c] = v.shape.rotation[c];
			}
			if ( v.gi.threshold ) {
				GiThreshold t;
				t.shape = s;
				t.shape.room = 1;	// GiThreshold's shape room is unused; the struct's default
				t.rooms[0] = v.gi.rooms[0];
				t.rooms[1] = v.gi.rooms[1];
				t.blendWidth = v.gi.blendWidth;
				ht.push_back( { v.gi.seq >= 0 ? v.gi.seq : nT + load, t } );
				if ( !all )
					pairs.insert( pairKey( t.rooms[0], t.rooms[1] ) );
			} else {
				s.room = v.gi.room;
				hs.push_back( { v.gi.seq >= 0 ? v.gi.seq : nS + load, s } );
				if ( !all )
					rooms.insert( s.room );
			}
			load++;
		}
		if ( !any )
			continue;
		files++;
		if ( all ) {
			modeAll++;
			if ( cv.cell.interior )
				allInterior = true;
			else
				allCells.insert( gridKey( cv.cell.gx, cv.cell.gy ) );
		}
	}
	if ( !files )
		return QString();
	const QString before = airZoneGiHash( set );
	auto dropByCell = [&]( const float c[3] ) {
		return allInterior || allCells.contains( gridKey( cellOfX( c[0] ), cellOfX( c[1] ) ) );
	};
	// kept today's at their places (key = index), the hand ones at seq (or after); a stable sort keeps kept-first on a tie
	std::vector<Hand> outS;
	int keptS = 0, keptT = 0;
	for ( long long i = 0; i < nS; i++ ) {
		const GiVolumeShape & s = set.shapes[size_t( i )];
		if ( dropByCell( s.center ) || rooms.contains( s.room ) )
			continue;
		outS.push_back( { i, s } );
		keptS++;
	}
	outS.insert( outS.end(), hs.begin(), hs.end() );
	std::stable_sort( outS.begin(), outS.end(), []( const Hand & a, const Hand & b ) { return a.key < b.key; } );
	std::vector<HandT> outT;
	for ( long long i = 0; i < nT; i++ ) {
		const GiThreshold & t = set.thresholds[size_t( i )];
		if ( dropByCell( t.shape.center ) || pairs.contains( pairKey( t.rooms[0], t.rooms[1] ) ) )
			continue;
		outT.push_back( { i, t } );
		keptT++;
	}
	outT.insert( outT.end(), ht.begin(), ht.end() );
	std::stable_sort( outT.begin(), outT.end(), []( const HandT & a, const HandT & b ) { return a.key < b.key; } );
	set.shapes.clear();
	for ( const Hand & h : outS )
		set.shapes.push_back( h.s );
	set.thresholds.clear();
	for ( const HandT & h : outT )
		set.thresholds.push_back( h.t );
	const QString line = QStringLiteral( "air zones gi: %1 file(s) (%2 all, %3 rooms), hand shapes %4 thresholds %5; today's kept shapes %6 of %7, "
		"thresholds %8 of %9; volumes %10 -> %11" ).arg( files ).arg( modeAll ).arg( files - modeAll ).arg( hs.size() ).arg( ht.size() )
		.arg( keptS ).arg( nS ).arg( keptT ).arg( nT ).arg( before, airZoneGiHash( set ) );
	set.census += QStringLiteral( "; " ) + line;
	return line;
}

std::vector<az::Volume> airZoneFromGi( const GiVolumeSet & set, const AirZoneCell & cell )
{
	std::vector<az::Volume> out;
	auto mine = [&]( const float c[3] ) { return cell.interior || ( cellOfX( c[0] ) == cell.gx && cellOfX( c[1] ) == cell.gy ); };
	for ( size_t i = 0; i < set.shapes.size(); i++ ) {
		const GiVolumeShape & s = set.shapes[i];
		if ( !mine( s.center ) )
			continue;
		az::Volume v;
		v.name = QStringLiteral( "gi room %1 #%2" ).arg( s.room ).arg( i ).toStdString();
		v.shape.type = s.type;
		for ( int c = 0; c < 3; c++ ) {
			v.shape.center[c] = s.center[c];
			v.shape.size[c] = s.size[c];
			v.shape.rotation[c] = s.rotation[c];
		}
		v.gi.on = true;
		v.gi.room = s.room;
		v.gi.seq = int( i );
		out.push_back( v );
	}
	for ( size_t i = 0; i < set.thresholds.size(); i++ ) {
		const GiThreshold & t = set.thresholds[i];
		if ( !mine( t.shape.center ) )
			continue;
		az::Volume v;
		v.name = QStringLiteral( "gi threshold %1-%2 #%3" ).arg( t.rooms[0] ).arg( t.rooms[1] ).arg( i ).toStdString();
		v.shape.type = t.shape.type;
		for ( int c = 0; c < 3; c++ ) {
			v.shape.center[c] = t.shape.center[c];
			v.shape.size[c] = t.shape.size[c];
			v.shape.rotation[c] = t.shape.rotation[c];
		}
		v.gi.on = true;
		v.gi.threshold = true;
		v.gi.rooms[0] = t.rooms[0];
		v.gi.rooms[1] = t.rooms[1];
		v.gi.blendWidth = t.blendWidth;
		v.gi.seq = int( i );
		out.push_back( v );
	}
	return out;
}

QString airZoneGiHash( const GiVolumeSet & set )
{
	quint64 h = Q_UINT64_C( 14695981039346656037 );
	for ( const GiVolumeShape & s : set.shapes ) {	// probegi.cpp giVolumesInto's 12 floats
		const float v[12] = { s.center[0], s.center[1], s.center[2], float( s.type ), s.size[0], s.size[1], s.size[2],
			float( s.room ), s.rotation[0], s.rotation[1], s.rotation[2], 0.0f };
		for ( float f : v )
			fnvFloat( h, f );
	}
	for ( const GiThreshold & t : set.thresholds ) {
		const float v[12] = { t.shape.center[0], t.shape.center[1], t.shape.center[2], float( t.shape.type ), t.shape.size[0],
			t.shape.size[1], t.shape.size[2], float( t.rooms[0] ), t.shape.rotation[0], t.shape.rotation[1], t.shape.rotation[2],
			float( t.rooms[1] ) };
		for ( float f : v )
			fnvFloat( h, f );
		fnvFloat( h, t.blendWidth );
	}
	return QStringLiteral( "%1 (shapes %2 thresholds %3)" ).arg( h, 16, 16, QLatin1Char( '0' ) ).arg( set.shapes.size() ).arg( set.thresholds.size() );
}

// ---- the documents

namespace {

struct Doc
{
	std::vector<AirZoneCell> cells;
	QStringList plugins, roots;
	QString dataDir;
	quint64 scanGen = ~Q_UINT64_C( 0 );
	QString red;
	bool loadedOnce = false;
	std::vector<AirZoneCellVolumes> loaded;
	std::map<int, AirZoneCellVolumes> edits;
	std::shared_ptr<const std::vector<AirZoneCellVolumes>> view;
	quint64 version = 0;
	std::shared_ptr<const GiVolumeSet> giBase;
};

QMutex & docsMutex()
{
	static QMutex m;
	return m;
}

QHash<const void *, Doc> & docs()
{
	static QHash<const void *, Doc> d;
	return d;
}

bool onMainThread()
{
	return QCoreApplication::instance() && QThread::currentThread() == QCoreApplication::instance()->thread();
}

void rebuildView( Doc & d )
{
	auto v = std::make_shared<std::vector<AirZoneCellVolumes>>( d.loaded );
	for ( const auto & e : d.edits )
		if ( e.first >= 0 && size_t( e.first ) < v->size() )
			( *v )[size_t( e.first )] = e.second;
	d.view = v;
	d.version++;
}

// re-read when the Cells files changed or the red moved (main thread only: recx.cpp's tables have no lock)
void refresh( Doc & d )
{
	const QString red = wwAirZoneRed();
	if ( d.loadedOnce && !onMainThread() )
		return;
	const quint64 g = wwRecxScan( d.roots, QStringLiteral( "Cells" ) );
	if ( d.loadedOnce && g == d.scanGen && red == d.red )
		return;
	d.scanGen = g;
	d.red = red;
	d.loaded = airZoneLoad( d.roots, d.plugins, d.cells, red );
	d.loadedOnce = true;
	for ( const AirZoneCellVolumes & cv : d.loaded ) {	// the recx census rule: echo which file and rung each record took
		std::fprintf( stderr, "air zones: %s %s -> %s\n", qPrintable( cv.cell.label() ), qPrintable( cv.cell.key() ),
			cv.file.isEmpty() ? ( red.contains( QStringLiteral( "novol" ) ) ? "none(red novol)" : "none" )
				: qPrintable( QStringLiteral( "%1:%2 (%3 volume(s))" ).arg( cv.rung, cv.file ).arg( cv.volumes.size() ) ) );
		for ( const QString & w : cv.warnings )
			std::fprintf( stderr, "air zones: %s: %s\n", qPrintable( cv.file ), qPrintable( w ) );
	}
	rebuildView( d );
}

} // namespace

QString wwAirZoneRed()
{
	return QString::fromLatin1( wwEnv( "WW_CELL_AIRZONE_RED" ) ).trimmed().toLower();
}

void wwAirZoneCellsPublish( const void * nif, const std::vector<AirZoneCell> & cells, const QStringList & pluginFiles,
	const QString & dataDir )
{
	if ( !nif )
		return;	// a streamed chunk (no document)
	QMutexLocker lock( &docsMutex() );
	Doc d;
	d.cells = cells;
	d.plugins = pluginFiles;
	d.dataDir = dataDir;
	d.roots = wwRecxRoots( pluginFiles, dataDir );
	const quint64 was = docs().contains( nif ) ? docs().value( nif ).version : 0;
	d.version = was + 1;
	docs().insert( nif, d );
}

std::shared_ptr<const std::vector<AirZoneCellVolumes>> wwAirZoneFor( const void * nif, quint64 * version )
{
	QMutexLocker lock( &docsMutex() );
	auto it = docs().find( nif );
	if ( it == docs().end() )
		return nullptr;
	refresh( *it );
	if ( version )
		*version = it->version;
	return it->view;
}

void wwAirZoneEdit( const void * nif, int index, const std::vector<az::Volume> & v, const QString & giMode, bool clear )
{
	QMutexLocker lock( &docsMutex() );
	auto it = docs().find( nif );
	if ( it == docs().end() )
		return;
	refresh( *it );
	if ( index < 0 || size_t( index ) >= it->loaded.size() )
		return;
	if ( clear ) {
		it->edits.erase( index );
	} else {
		AirZoneCellVolumes cv = it->loaded[size_t( index )];
		cv.volumes = v;
		cv.giMode = giMode;
		cv.edited = true;
		it->edits[index] = cv;
	}
	rebuildView( *it );
}

bool wwAirZoneSave( const void * nif, int index, const QString & dataRoot, const QString & provider, QString * path, QString * err )
{
	QMutexLocker lock( &docsMutex() );
	auto it = docs().find( nif );
	if ( it == docs().end() || index < 0 || size_t( index ) >= it->loaded.size() ) {
		if ( err )
			*err = QStringLiteral( "no cell open" );
		return false;
	}
	const auto e = it->edits.find( index );
	const AirZoneCellVolumes & cv = e != it->edits.end() ? e->second : it->loaded[size_t( index )];
	// its own file when it came from one (the provider that won keeps it), else a new one under the save root
	const QString p = !cv.file.isEmpty() ? cv.file : airZoneFilePath( dataRoot, provider.isEmpty() ? QStringLiteral( "FO4CS" ) : provider, cv.cell );
	if ( !airZoneWriteEntry( p, cv.cell, cv.volumes, cv.giMode, err ) )
		return false;
	if ( path )
		*path = p;
	it->edits.erase( index );
	it->scanGen = ~Q_UINT64_C( 0 );	// the next read takes the file (the scan sees its new time within 500 ms)
	return true;
}

QString wwAirZoneGiApply( const void * nif, GiVolumeSet & set, bool on )
{
	if ( !nif )
		return QString();
	QMutexLocker lock( &docsMutex() );
	auto it = docs().find( nif );
	if ( it == docs().end() )
		return QString();
	it->giBase = std::make_shared<const GiVolumeSet>( set );	// the converter's input, kept whether or not the row merges
	QString line;
	const QByteArray hashPin = wwEnv( "WW_CELL_AIRZONE_GIHASH" );
	if ( hashPin == "1" )
		line = QStringLiteral( "air zones gi: today's volumes %1" ).arg( airZoneGiHash( set ) );
	if ( !on )
		return line;
	refresh( *it );
	const QString merged = it->view ? airZoneGiMerge( set, *it->view ) : QString();
	if ( !merged.isEmpty() )
		line = merged;
	return line;
}

QStringList wwAirZoneGiExport( const void * nif, const QString & dataRoot, const QString & provider, QString * err )
{
	QStringList written;
	QMutexLocker lock( &docsMutex() );
	auto it = docs().find( nif );
	if ( it == docs().end() || !it->giBase ) {
		if ( err )
			*err = QStringLiteral( "no GI volumes kept: open a cell with the GI on (PRTP) first" );
		return written;
	}
	refresh( *it );
	for ( size_t i = 0; i < it->loaded.size(); i++ ) {
		const AirZoneCellVolumes & cv = it->loaded[i];
		std::vector<az::Volume> v;
		for ( const az::Volume & x : cv.volumes )
			if ( !x.gi.on )
				v.push_back( x );	// the cell's fog and particle volumes are kept
		const std::vector<az::Volume> g = airZoneFromGi( *it->giBase, cv.cell );
		if ( g.empty() && v.size() == cv.volumes.size() )
			continue;	// nothing of today's GI in this cell, nothing to replace
		v.insert( v.end(), g.begin(), g.end() );
		const QString p = airZoneFilePath( dataRoot, provider.isEmpty() ? QStringLiteral( "FO4CS" ) : provider, cv.cell );
		QString e;
		if ( !airZoneWriteEntry( p, cv.cell, v, QStringLiteral( "all" ), &e ) ) {
			if ( err )
				*err = e;
			return written;
		}
		written << p;
	}
	it->scanGen = ~Q_UINT64_C( 0 );
	std::fprintf( stderr, "air zones gi: exported today's volumes %s into %d file(s) under %s\n", qPrintable( airZoneGiHash( *it->giBase ) ),
		int( written.size() ), qPrintable( dataRoot ) );
	return written;
}

QString wwAirZoneCensus( const void * nif )
{
	quint64 ver = 0;
	const auto v = wwAirZoneFor( nif, &ver );
	if ( !v )
		return QStringLiteral( "air zones: no cell" );
	int files = 0, vols = 0, gi = 0, fog = 0, part = 0, warn = 0, edited = 0;
	for ( const AirZoneCellVolumes & cv : *v ) {
		files += cv.file.isEmpty() ? 0 : 1;
		edited += cv.edited ? 1 : 0;
		warn += int( cv.warnings.size() );
		for ( const az::Volume & x : cv.volumes ) {
			vols++;
			gi += x.gi.on ? 1 : 0;
			fog += x.fog.on ? 1 : 0;
			part += x.part.on ? 1 : 0;
		}
	}
	const QString red = wwAirZoneRed();
	return QStringLiteral( "air zones: %1 cell(s), %2 file(s), %3 volume(s) (gi %4, fog %5, particles %6), %7 warning(s)%8%9" )
		.arg( v->size() ).arg( files ).arg( vols ).arg( gi ).arg( fog ).arg( part ).arg( warn )
		.arg( edited ? QStringLiteral( ", %1 unsaved" ).arg( edited ) : QString() )
		.arg( red.isEmpty() ? QString() : QStringLiteral( ", RED %1" ).arg( red ) );
}
