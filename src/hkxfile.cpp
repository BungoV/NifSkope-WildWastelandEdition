/* Generic Havok 2014 packfile object model (lane HKXEDIT1, 2026-09-10).
   Contract: docs/HKX_PACKFILE_MODEL.md. The independent oracle written from
   the same page is tests/spells/hkxfile_oracle.py; the two are held against
   each other and against every shipped .hkx by tests/spells/hkxfile_gates.py. */

#include "hkxfile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QtEndian>

#include <cstring>
#include <memory>

namespace Hkx {

// ---------------------------------------------------------------- types

static const char * const kTypeNames[int( Type::Count )] = {
	"VOID", "BOOL", "CHAR", "INT8", "UINT8", "INT16", "UINT16", "INT32", "UINT32", "INT64", "UINT64",
	"REAL", "VECTOR4", "QUATERNION", "MATRIX3", "ROTATION", "QSTRANSFORM", "MATRIX4", "TRANSFORM",
	"ZERO", "POINTER", "FUNCTIONPOINTER", "ARRAY", "INPLACEARRAY", "ENUM", "STRUCT", "SIMPLEARRAY",
	"HOMOGENEOUSARRAY", "VARIANT", "CSTRING", "ULONG", "FLAGS", "HALF", "STRINGPTR", "RELARRAY"
};

const char * typeName( Type t )
{
	return int( t ) < int( Type::Count ) ? kTypeNames[int( t )] : "?";
}

Type typeFromName( const QString & name, bool * ok )
{
	for ( int i = 0; i < int( Type::Count ); i++ ) {
		if ( name == QLatin1String( kTypeNames[i] ) ) {
			if ( ok ) *ok = true;
			return Type( i );
		}
	}
	if ( ok ) *ok = false;
	return Type::Void;
}

int plainSize( Type t )
{
	switch ( t ) {
	case Type::Bool: case Type::Char: case Type::Int8: case Type::UInt8: return 1;
	case Type::Int16: case Type::UInt16: case Type::Half: return 2;
	case Type::Int32: case Type::UInt32: case Type::Real: case Type::RelArray: return 4;
	case Type::Int64: case Type::UInt64: case Type::ULong: case Type::Pointer:
	case Type::FunctionPointer: case Type::CString: case Type::StringPtr: return 8;
	case Type::Vector4: case Type::Quaternion: case Type::Array: case Type::SimpleArray: case Type::Variant: return 16;
	case Type::HomogeneousArray: return 24;
	case Type::Matrix3: case Type::Rotation: case Type::QsTransform: return 48;
	case Type::Matrix4: case Type::Transform: return 64;
	default: return 0;
	}
}

static bool isIntType( Type t )
{
	switch ( t ) {
	case Type::Bool: case Type::Char: case Type::Int8: case Type::UInt8: case Type::Int16: case Type::UInt16:
	case Type::Int32: case Type::UInt32: case Type::Int64: case Type::UInt64: case Type::ULong: case Type::Half:
		return true;
	default:
		return false;
	}
}

static bool isRawType( Type t )
{
	switch ( t ) {
	case Type::Vector4: case Type::Quaternion: case Type::Matrix3: case Type::Rotation:
	case Type::QsTransform: case Type::Matrix4: case Type::Transform:
		return true;
	default:
		return false;
	}
}

static bool isStringType( Type t ) { return t == Type::CString || t == Type::StringPtr; }

static qint64 readInt( const char * p, Type t )
{
	switch ( t ) {
	case Type::Bool: case Type::Char: case Type::UInt8: return quint8( *p );
	case Type::Int8: return qint8( *p );
	case Type::Int16: return qFromLittleEndian<qint16>( p );
	case Type::UInt16: case Type::Half: return qFromLittleEndian<quint16>( p );
	case Type::Int32: return qFromLittleEndian<qint32>( p );
	case Type::UInt32: case Type::Real: return qFromLittleEndian<quint32>( p );
	case Type::Int64: return qFromLittleEndian<qint64>( p );
	case Type::UInt64: case Type::ULong: return qint64( qFromLittleEndian<quint64>( p ) );
	default: return 0;
	}
}

static void writeInt( char * p, Type t, qint64 v )
{
	switch ( t ) {
	case Type::Bool: case Type::Char: case Type::UInt8: case Type::Int8: *p = char( v ); break;
	case Type::Int16: case Type::UInt16: case Type::Half: qToLittleEndian<quint16>( quint16( v ), p ); break;
	case Type::Int32: case Type::UInt32: case Type::Real: qToLittleEndian<quint32>( quint32( v ), p ); break;
	case Type::Int64: case Type::UInt64: case Type::ULong: qToLittleEndian<quint64>( quint64( v ), p ); break;
	default: break;
	}
}

// ---------------------------------------------------------------- defs

QString EnumDef::nameOf( qint64 value ) const
{
	for ( const auto & it : items )
		if ( it.first == value )
			return it.second;
	return QString();
}

bool EnumDef::valueOf( const QString & item, qint64 * value ) const
{
	for ( const auto & it : items ) {
		if ( it.second == item ) {
			if ( value ) *value = it.first;
			return true;
		}
	}
	return false;
}

int MemberDef::enumSize() const
{
	int s = plainSize( subtype );
	return s > 0 ? s : 4;
}

int MemberDef::elementSize() const
{
	if ( subtype == Type::Struct )
		return cls ? cls->objectSize : 0;
	if ( subtype == Type::Enum || subtype == Type::Flags )
		return ( flags & Enum8 ) ? 1 : ( flags & Enum16 ) ? 2 : 4;
	return plainSize( subtype );
}

int MemberDef::inlineSize() const
{
	const int n = qMax<int>( 1, cArraySize );
	if ( type == Type::Struct )
		return ( cls ? cls->objectSize : 0 ) * n;
	if ( type == Type::Enum || type == Type::Flags )
		return enumSize() * n;
	return plainSize( type ) * n;
}

bool ClassDef::inherits( const QString & ancestor ) const
{
	for ( const ClassDef * c = this; c; c = c->parent )
		if ( c->name == ancestor )
			return true;
	return false;
}

// ---------------------------------------------------------------- ClassDb

ClassDb::~ClassDb()
{
	qDeleteAll( owned );
}

ClassDb * ClassDb::load( const QString & jsonPath, QString * error )
{
	QFile f( jsonPath );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		if ( error ) *error = QStringLiteral( "cannot open the class database %1" ).arg( jsonPath );
		return nullptr;
	}
	return loadJson( f.readAll(), error );
}

ClassDb * ClassDb::loadJson( const QByteArray & json, QString * error )
{
	QJsonParseError perr;
	const QJsonDocument doc = QJsonDocument::fromJson( json, &perr );
	if ( doc.isNull() || !doc.isObject() ) {
		if ( error ) *error = QStringLiteral( "class database is not JSON: %1" ).arg( perr.errorString() );
		return nullptr;
	}
	const QJsonObject top = doc.object();
	if ( top.value( QLatin1String( "format" ) ).toString() != QLatin1String( "ww-hkclassdb-1" ) ) {
		if ( error ) *error = QStringLiteral( "class database format is '%1', this reader knows 'ww-hkclassdb-1'" )
			.arg( top.value( QLatin1String( "format" ) ).toString() );
		return nullptr;
	}
	std::unique_ptr<ClassDb> db( new ClassDb );
	const QJsonObject src = top.value( QLatin1String( "source" ) ).toObject();
	db->summary = QStringLiteral( "%1 build %2 sha256 %3, extracted %4; %5 classes" )
		.arg( QFileInfo( src.value( QLatin1String( "exe" ) ).toString() ).fileName(),
			src.value( QLatin1String( "build" ) ).toString(), src.value( QLatin1String( "exeSha256_16" ) ).toString(),
			src.value( QLatin1String( "extractedOn" ) ).toString() )
		.arg( top.value( QLatin1String( "classes" ) ).toArray().size() );
	const QJsonArray classes = top.value( QLatin1String( "classes" ) ).toArray();
	// pass 1: the classes and their own members / enums
	struct Pending { MemberDef * m; QString enumClass; QJsonArray inlineItems; };
	QVector<Pending> pending;
	for ( const QJsonValue & cv : classes ) {
		const QJsonObject co = cv.toObject();
		ClassDef * c = new ClassDef;
		db->owned.append( c );
		c->name = co.value( QLatin1String( "name" ) ).toString();
		c->parentName = co.value( QLatin1String( "parent" ) ).toString();
		c->objectSize = co.value( QLatin1String( "objectSize" ) ).toInt();
		c->version = co.value( QLatin1String( "version" ) ).toInt();
		c->flags = quint32( co.value( QLatin1String( "flags" ) ).toDouble() );
		c->registryIndex = co.value( QLatin1String( "registryIndex" ) ).toInt( -1 );
		c->signature = co.value( QLatin1String( "signature" ) ).toString().toUInt( nullptr, 16 );
		for ( const QJsonValue & ev : co.value( QLatin1String( "enums" ) ).toArray() ) {
			const QJsonObject eo = ev.toObject();
			EnumDef e;
			e.name = eo.value( QLatin1String( "name" ) ).toString();
			for ( const QJsonValue & iv : eo.value( QLatin1String( "items" ) ).toArray() ) {
				const QJsonArray ia = iv.toArray();
				e.items.append( qMakePair( ia.at( 0 ).toInt(), ia.at( 1 ).toString() ) );
			}
			c->enums.append( e );
		}
		const QJsonArray members = co.value( QLatin1String( "members" ) ).toArray();
		c->members.reserve( members.size() );
		for ( const QJsonValue & mv : members ) {
			const QJsonObject mo = mv.toObject();
			MemberDef m;
			m.name = mo.value( QLatin1String( "name" ) ).toString();
			m.type = typeFromName( mo.value( QLatin1String( "type" ) ).toString() );
			m.subtype = typeFromName( mo.value( QLatin1String( "subtype" ) ).toString() );
			m.cArraySize = quint16( mo.value( QLatin1String( "cArraySize" ) ).toInt() );
			m.flags = quint16( mo.value( QLatin1String( "flags" ) ).toInt() );
			m.offset = quint16( mo.value( QLatin1String( "offset" ) ).toInt() );
			m.className = mo.value( QLatin1String( "class" ) ).toString();
			m.enumName = mo.value( QLatin1String( "enum" ) ).toString();
			c->members.append( m );
			if ( !m.enumName.isEmpty() )
				pending.append( { nullptr, mo.value( QLatin1String( "enumClass" ) ).toString(),
					mo.value( QLatin1String( "enumItems" ) ).toArray() } );
		}
		// re-point the pending entries at the stored members (the vector is final now)
		int pi = pending.size() - 1;
		for ( int k = c->members.size() - 1; k >= 0 && pi >= 0; k-- ) {
			if ( !c->members[k].enumName.isEmpty() ) {
				pending[pi].m = &c->members[k];
				pi--;
			}
		}
		if ( db->classes.contains( c->name ) ) {
			if ( error ) *error = QStringLiteral( "class database names %1 twice" ).arg( c->name );
			return nullptr;
		}
		db->classes.insert( c->name, c );
	}
	// pass 2: link
	for ( ClassDef * c : db->owned ) {
		if ( !c->parentName.isEmpty() ) {
			c->parent = db->classes.value( c->parentName );
			if ( !c->parent ) {
				if ( error ) *error = QStringLiteral( "class %1 names parent %2, which is not in the database" ).arg( c->name, c->parentName );
				return nullptr;
			}
		}
		for ( MemberDef & m : c->members ) {
			if ( !m.className.isEmpty() ) {
				m.cls = db->classes.value( m.className );
				if ( !m.cls ) {
					if ( error ) *error = QStringLiteral( "%1.%2 names class %3, which is not in the database" ).arg( c->name, m.name, m.className );
					return nullptr;
				}
			}
		}
	}
	for ( const Pending & p : pending ) {
		if ( !p.m ) continue;
		const ClassDef * owner = p.enumClass.isEmpty() ? nullptr : db->classes.value( p.enumClass );
		if ( owner ) {
			for ( const EnumDef & e : owner->enums )
				if ( e.name == p.m->enumName ) { p.m->en = &e; break; }
		}
		if ( !p.m->en && !p.inlineItems.isEmpty() ) {
			// an enum declared by no registered class: carried inline by the extractor
			ClassDef * holder = new ClassDef;
			holder->name = QStringLiteral( "(inline enum %1)" ).arg( p.m->enumName );
			db->owned.append( holder );
			EnumDef e;
			e.name = p.m->enumName;
			for ( const QJsonValue & iv : p.inlineItems ) {
				const QJsonArray ia = iv.toArray();
				e.items.append( qMakePair( ia.at( 0 ).toInt(), ia.at( 1 ).toString() ) );
			}
			holder->enums.append( e );
			p.m->en = &holder->enums.first();
		}
	}
	// pass 3: the flattened member lists (parents first), checking for cycles
	for ( ClassDef * c : db->owned ) {
		QVector<const ClassDef *> chain;
		for ( const ClassDef * q = c; q; q = q->parent ) {
			if ( chain.contains( q ) ) {
				if ( error ) *error = QStringLiteral( "class %1 has a parent cycle" ).arg( c->name );
				return nullptr;
			}
			chain.prepend( q );
		}
		for ( const ClassDef * q : chain )
			for ( const MemberDef & m : q->members )
				c->allMembers.append( &m );
	}
	return db.release();
}

QStringList ClassDb::defaultPaths()
{
	QStringList out;
	const QByteArray env = qgetenv( "WW_HKCLASSDB" );
	if ( !env.isEmpty() )
		out << QString::fromLocal8Bit( env );
	const QString app = QCoreApplication::applicationDirPath();
	if ( !app.isEmpty() ) {
		out << app + QLatin1String( "/hkclasses_fo4.json" );
		out << app + QLatin1String( "/../res/hkclasses_fo4.json" );
	}
	out << QDir::currentPath() + QLatin1String( "/res/hkclasses_fo4.json" );
	return out;
}

const ClassDb * ClassDb::instance( QString * error )
{
	static ClassDb * db = nullptr;
	static QString err;
	if ( !db && err.isEmpty() ) {
		QStringList tried;
		for ( const QString & p : defaultPaths() ) {
			if ( !QFileInfo::exists( p ) ) {
				tried << p;
				continue;
			}
			QString e;
			db = load( p, &e );
			if ( db )
				break;
			tried << p + QLatin1String( " (" ) + e + QLatin1Char( ')' );
		}
		if ( !db )
			err = QStringLiteral( "no class database found; tried %1" ).arg( tried.join( QLatin1String( ", " ) ) );
	}
	if ( error ) *error = err;
	return db;
}

const ClassDef * ClassDb::find( const QString & name ) const
{
	return classes.value( name );
}

QStringList ClassDb::names() const
{
	QStringList out = classes.keys();
	out.sort();
	return out;
}

// ---------------------------------------------------------------- Value

float Value::toFloat( int i ) const
{
	const quint32 bits = quint32( toInt( i ) );
	float f;
	std::memcpy( &f, &bits, 4 );
	return f;
}

void Value::setInt( qint64 v, int i )
{
	if ( ints.size() <= i )
		ints.resize( i + 1 );
	ints[i] = v;
}

void Value::setFloat( float f, int i )
{
	quint32 bits;
	std::memcpy( &bits, &f, 4 );
	setInt( bits, i );
}

QString Value::toStringValue() const
{
	return isNull ? QString() : QString::fromLatin1( bytes );
}

void Value::setStringValue( const QString & s )
{
	bytes = s.toLatin1();
	isNull = false;
}

qint64 Value::plainInt( int i ) const
{
	const int sz = plainSize( type );
	if ( sz <= 0 || ( i + 1 ) * sz > bytes.size() )
		return 0;
	return readInt( bytes.constData() + i * sz, type );
}

float Value::plainFloat( int i ) const
{
	const quint32 bits = quint32( plainInt( i ) );
	float f;
	std::memcpy( &f, &bits, 4 );
	return f;
}

void Value::setPlainInt( int i, qint64 v )
{
	const int sz = plainSize( type );
	if ( sz <= 0 || ( i + 1 ) * sz > bytes.size() )
		return;
	writeInt( bytes.data() + i * sz, type, v );
}

void Value::setPlainFloat( int i, float f )
{
	quint32 bits;
	std::memcpy( &bits, &f, 4 );
	setPlainInt( i, bits );
}

// ---------------------------------------------------------------- defaults

Value File::defaultStruct( const ClassDef * cls )
{
	Value v;
	v.kind = Value::Struct;
	v.type = Type::Struct;
	v.cls = cls;
	v.bytes = QByteArray( cls ? cls->objectSize : 0, '\0' );
	if ( cls )
		for ( const MemberDef * m : cls->allMembers )
			v.elems.append( defaultValue( *m ) );
	return v;
}

Value File::defaultElement( const MemberDef & am )
{
	Value v;
	v.member = nullptr;
	if ( am.subtype == Type::Struct ) {
		v = defaultStruct( am.cls );
	} else if ( am.subtype == Type::Pointer ) {
		v.kind = Value::Ptr;
		v.type = Type::Pointer;
		v.object = -1;
		v.cls = am.cls;
	} else if ( isStringType( am.subtype ) ) {
		v.kind = Value::Str;
		v.type = am.subtype;
		v.isNull = true;
	} else {
		v.kind = Value::Ignored;
	}
	return v;
}

Value File::defaultValue( const MemberDef & m )
{
	Value v;
	v.member = &m;
	const int n = qMax<int>( 1, m.cArraySize );
	if ( m.ignored() ) {
		v.kind = Value::Ignored;
		v.bytes = QByteArray( m.inlineSize(), '\0' );
		return v;
	}
	if ( isIntType( m.type ) || m.type == Type::Real ) {
		v.kind = Value::Scalar;
		v.type = m.type;
		v.ints = QVector<qint64>( n, 0 );
		return v;
	}
	if ( m.type == Type::Enum || m.type == Type::Flags ) {
		v.kind = Value::Scalar;
		v.type = m.type;
		v.storage = m.subtype;
		v.ints = QVector<qint64>( n, 0 );
		return v;
	}
	if ( isRawType( m.type ) ) {
		v.kind = Value::Raw;
		v.type = m.type;
		v.bytes = QByteArray( plainSize( m.type ) * n, '\0' );
		if ( m.type == Type::QsTransform || m.type == Type::Transform || m.type == Type::Matrix4
			|| m.type == Type::Matrix3 || m.type == Type::Rotation || m.type == Type::Quaternion ) {
			// an identity: unit quaternion / unit scale / identity matrix
			const float one = 1.0f;
			auto putf = [&]( int off, float f ) { std::memcpy( v.bytes.data() + off, &f, 4 ); };
			for ( int k = 0; k < n; k++ ) {
				const int b = plainSize( m.type ) * k;
				switch ( m.type ) {
				case Type::Quaternion: putf( b + 12, one ); break;
				case Type::QsTransform: putf( b + 28, one ); putf( b + 32, one ); putf( b + 36, one ); putf( b + 40, one ); break;
				case Type::Matrix3: case Type::Rotation: putf( b + 0, one ); putf( b + 20, one ); putf( b + 40, one ); break;
				case Type::Matrix4: case Type::Transform: putf( b + 0, one ); putf( b + 20, one ); putf( b + 40, one ); putf( b + 60, one ); break;
				default: break;
				}
			}
		}
		return v;
	}
	if ( isStringType( m.type ) || m.type == Type::Pointer || m.type == Type::Struct ) {
		if ( n > 1 ) {
			v.kind = Value::CArray;
			v.type = m.type;
			MemberDef em = m;
			em.subtype = m.type;
			for ( int k = 0; k < n; k++ )
				v.elems.append( defaultElement( em ) );
			return v;
		}
		if ( m.type == Type::Struct ) {
			Value s = defaultStruct( m.cls );
			s.member = &m;
			return s;
		}
		if ( m.type == Type::Pointer ) {
			v.kind = Value::Ptr;
			v.type = Type::Pointer;
			v.object = -1;
			v.cls = m.cls;
			return v;
		}
		v.kind = Value::Str;
		v.type = m.type;
		v.isNull = true;
		return v;
	}
	if ( m.type == Type::Array || m.type == Type::RelArray ) {
		v.kind = m.type == Type::Array ? Value::Array : Value::RelArray;
		v.type = m.subtype;
		v.cls = m.cls;
		v.count = 0;
		v.capacity = m.type == Type::Array ? 0x80000000u : 0;
		v.hasPayload = false;
		return v;
	}
	// anything else (Zero, FunctionPointer, Variant, ...) is carried as bytes
	v.kind = Value::Ignored;
	v.bytes = QByteArray( m.inlineSize(), '\0' );
	return v;
}

// ---------------------------------------------------------------- reader

namespace {

struct Refusal
{
	QString msg;
};

[[noreturn]] void refuse( const QString & s )
{
	throw Refusal{ s };
}

struct Reader
{
	const QByteArray & blob;
	const ClassDb & db;
	File & f;
	QHash<int, int> local;   //!< absolute src -> absolute dst
	QHash<int, int> glob;
	QHash<int, int> objAt;   //!< absolute object offset -> index
	QSet<int> usedLocal, usedGlob;
	int base = 0;
	int payloadEnd = 0;

	Reader( const QByteArray & b, const ClassDb & d, File & file ) : blob( b ), db( d ), f( file ) {}

	const char * at( int off ) const { return blob.constData() + off; }

	void chunk( int off, int size, const char * kind, const QString & label )
	{
		f.chunks.append( { off, size, QString::fromLatin1( kind ), label } );
	}

	static void clearHole( QByteArray & hole, int off, int n )
	{
		if ( off >= 0 && off + n <= hole.size() )
			std::memset( hole.data() + off, 0, size_t( n ) );
	}

	int stringEnd( int d, const QString & label ) const
	{
		const int e = blob.indexOf( '\0', d );
		if ( e < 0 || e >= payloadEnd )
			refuse( QStringLiteral( "%1: the string at 0x%2 has no terminator inside the payload" ).arg( label ).arg( d - base, 0, 16 ) );
		return e;
	}

	Value readString( int q, const QString & label, bool packed )
	{
		Value v;
		v.kind = Value::Str;
		v.type = Type::StringPtr;
		auto it = local.constFind( q );
		if ( it != local.constEnd() ) {
			usedLocal.insert( q );
			const int d = it.value();
			const int e = stringEnd( d, label );
			chunk( d, e + 1 - d, packed ? "strA" : "str", label );
			v.bytes = blob.mid( d, e - d );
			v.isNull = false;
		} else {
			if ( qFromLittleEndian<quint64>( at( q ) ) != 0 )
				refuse( QStringLiteral( "%1: string pointer bytes are not zero and have no local fixup" ).arg( label ) );
			v.isNull = true;
		}
		return v;
	}

	Value readPointer( int q, const QString & label, const ClassDef * cls )
	{
		Value v;
		v.kind = Value::Ptr;
		v.type = Type::Pointer;
		v.cls = cls;
		auto it = glob.constFind( q );
		if ( it != glob.constEnd() ) {
			usedGlob.insert( q );
			auto oi = objAt.constFind( it.value() );
			if ( oi == objAt.constEnd() )
				refuse( QStringLiteral( "%1: pointer to 0x%2, which is not an object start" ).arg( label ).arg( it.value() - base, 0, 16 ) );
			v.object = oi.value();
		} else if ( local.contains( q ) ) {
			refuse( QStringLiteral( "%1: an object pointer with a LOCAL fixup (to 0x%2) is not modelled" ).arg( label ).arg( local.value( q ) - base, 0, 16 ) );
		} else {
			if ( qFromLittleEndian<quint64>( at( q ) ) != 0 )
				refuse( QStringLiteral( "%1: pointer bytes are not zero and have no fixup" ).arg( label ) );
			v.object = -1;
		}
		return v;
	}

	Value readStruct( const ClassDef * cls, int atOff, const QString & label )
	{
		const int size = cls->objectSize;
		if ( atOff + size > payloadEnd )
			refuse( QStringLiteral( "%1 at 0x%2 (%3 bytes) runs past the payload end 0x%4" )
				.arg( label ).arg( atOff - base, 0, 16 ).arg( size ).arg( payloadEnd - base, 0, 16 ) );
		Value v;
		v.kind = Value::Struct;
		v.type = Type::Struct;
		v.cls = cls;
		v.bytes = blob.mid( atOff, size );
		v.elems.reserve( cls->allMembers.size() );
		for ( const MemberDef * m : cls->allMembers )
			v.elems.append( readMember( *m, atOff, label + QLatin1Char( '.' ) + m->name, v.bytes ) );
		return v;
	}

	Value readArrayPayload( const MemberDef & m, int d, int size, const QString & label, bool rel, QByteArray & hole, int holeOff, int holeLen )
	{
		Value v;
		v.kind = rel ? Value::RelArray : Value::Array;
		v.type = m.subtype;
		v.cls = m.cls;
		v.member = &m;
		v.count = size;
		clearHole( hole, holeOff, holeLen );
		const int esz = m.elementSize();
		if ( size < 0 || size > 50000000 )
			refuse( QStringLiteral( "%1: array size %2" ).arg( label ).arg( size ) );
		if ( esz <= 0 && size > 0 )
			refuse( QStringLiteral( "%1: an array of %2 has no element size" ).arg( label, QLatin1String( typeName( m.subtype ) ) ) );
		if ( d + esz * size > payloadEnd )
			refuse( QStringLiteral( "%1: %2 elements of %3 bytes at 0x%4 run past the payload" ).arg( label ).arg( size ).arg( esz ).arg( d - base, 0, 16 ) );
		chunk( d, esz * size, rel ? "rel" : "arr", label );
		v.hasPayload = true;
		if ( m.subtype == Type::Struct ) {
			v.elems.reserve( size );
			for ( int i = 0; i < size; i++ )
				v.elems.append( readStruct( m.cls, d + i * esz, QStringLiteral( "%1[%2]" ).arg( label ).arg( i ) ) );
		} else if ( m.subtype == Type::Pointer ) {
			if ( rel ) refuse( QStringLiteral( "%1: an hkRelArray of pointers is not modelled" ).arg( label ) );
			for ( int i = 0; i < size; i++ )
				v.elems.append( readPointer( d + i * 8, QStringLiteral( "%1[%2]" ).arg( label ).arg( i ), m.cls ) );
		} else if ( isStringType( m.subtype ) ) {
			if ( rel ) refuse( QStringLiteral( "%1: an hkRelArray of strings is not modelled" ).arg( label ) );
			for ( int i = 0; i < size; i++ )
				v.elems.append( readString( d + i * 8, QStringLiteral( "%1[%2]" ).arg( label ).arg( i ), true ) );
		} else if ( isIntType( m.subtype ) || m.subtype == Type::Real || isRawType( m.subtype )
			|| m.subtype == Type::Enum || m.subtype == Type::Flags ) {
			v.bytes = blob.mid( d, esz * size );
		} else {
			refuse( QStringLiteral( "%1: hkArray of %2 is not modelled" ).arg( label, QLatin1String( typeName( m.subtype ) ) ) );
		}
		return v;
	}

	Value readMember( const MemberDef & m, int atOff, const QString & label, QByteArray & hole )
	{
		const int off = m.offset;
		const int p = atOff + off;
		const int n = qMax<int>( 1, m.cArraySize );
		Value v;
		v.member = &m;
		if ( m.ignored() ) {
			v.kind = Value::Ignored;
			v.bytes = blob.mid( p, m.inlineSize() );
			return v;
		}
		if ( isIntType( m.type ) || m.type == Type::Real ) {
			const int sz = plainSize( m.type );
			v.kind = Value::Scalar;
			v.type = m.type;
			for ( int i = 0; i < n; i++ )
				v.ints.append( readInt( at( p + i * sz ), m.type ) );
			clearHole( hole, off, sz * n );
			return v;
		}
		if ( m.type == Type::Enum || m.type == Type::Flags ) {
			if ( !isIntType( m.subtype ) )
				refuse( QStringLiteral( "%1: %2 with storage %3" ).arg( label, QLatin1String( typeName( m.type ) ), QLatin1String( typeName( m.subtype ) ) ) );
			const int sz = plainSize( m.subtype );
			v.kind = Value::Scalar;
			v.type = m.type;
			v.storage = m.subtype;
			for ( int i = 0; i < n; i++ )
				v.ints.append( readInt( at( p + i * sz ), m.subtype ) );
			clearHole( hole, off, sz * n );
			return v;
		}
		if ( isRawType( m.type ) ) {
			const int sz = plainSize( m.type ) * n;
			v.kind = Value::Raw;
			v.type = m.type;
			v.bytes = blob.mid( p, sz );
			clearHole( hole, off, sz );
			return v;
		}
		if ( isStringType( m.type ) || m.type == Type::Pointer || m.type == Type::Struct ) {
			const int esz = m.type == Type::Struct ? ( m.cls ? m.cls->objectSize : 0 ) : 8;
			if ( m.type != Type::Struct )
				clearHole( hole, off, 8 * n );
			auto one = [&]( int i, const QString & lab ) -> Value {
				if ( m.type == Type::Struct ) return readStruct( m.cls, p + i * esz, lab );
				if ( m.type == Type::Pointer ) return readPointer( p + i * 8, lab, m.cls );
				return readString( p + i * 8, lab, false );
			};
			if ( n == 1 ) {
				Value s = one( 0, label );
				s.member = &m;
				return s;
			}
			v.kind = Value::CArray;
			v.type = m.type;
			for ( int i = 0; i < n; i++ )
				v.elems.append( one( i, QStringLiteral( "%1[%2]" ).arg( label ).arg( i ) ) );
			return v;
		}
		if ( m.type == Type::Array ) {
			if ( n != 1 )
				refuse( QStringLiteral( "%1: a C array of hkArray is not modelled" ).arg( label ) );
			const int size = qFromLittleEndian<qint32>( at( p + 8 ) );
			const quint32 cap = qFromLittleEndian<quint32>( at( p + 12 ) );
			auto it = local.constFind( p );
			if ( m.subtype == Type::Void ) {
				if ( it != local.constEnd() )
					refuse( QStringLiteral( "%1: hkArray<void> with a payload" ).arg( label ) );
			}
			if ( it != local.constEnd() ) {
				usedLocal.insert( p );
				Value a = readArrayPayload( m, it.value(), size, label, false, hole, off, 16 );
				a.capacity = cap;
				return a;
			}
			if ( qFromLittleEndian<quint64>( at( p ) ) != 0 )
				refuse( QStringLiteral( "%1: array pointer bytes are not zero and have no fixup" ).arg( label ) );
			if ( size != 0 )
				refuse( QStringLiteral( "%1: array size %2 with no payload" ).arg( label ).arg( size ) );
			clearHole( hole, off, 16 );
			v.kind = Value::Array;
			v.type = m.subtype;
			v.cls = m.cls;
			v.count = 0;
			v.capacity = cap;
			v.hasPayload = false;
			return v;
		}
		if ( m.type == Type::RelArray ) {
			if ( n != 1 )
				refuse( QStringLiteral( "%1: a C array of hkRelArray is not modelled" ).arg( label ) );
			const int size = qFromLittleEndian<quint16>( at( p ) );
			const int rel = qFromLittleEndian<quint16>( at( p + 2 ) );
			if ( size && rel == 0 )
				refuse( QStringLiteral( "%1: hkRelArray of %2 with a zero offset" ).arg( label ).arg( size ) );
			Value a = readArrayPayload( m, p + rel, size, label, true, hole, off, 4 );
			a.capacity = 0;
			return a;
		}
		refuse( QStringLiteral( "%1: member type %2 is not modelled by this reader" ).arg( label, QLatin1String( typeName( m.type ) ) ) );
	}

	void run()
	{
		static const char kMagic[8] = { 0x57, char( 0xe0 ), char( 0xe0 ), 0x57, 0x10, char( 0xc0 ), char( 0xc0 ), 0x10 };
		if ( blob.size() < 0x40 || std::memcmp( blob.constData(), kMagic, 8 ) != 0 )
			refuse( QStringLiteral( "not a Havok packfile: magic %1" ).arg( QString::fromLatin1( blob.left( 8 ).toHex() ) ) );
		const quint32 fileVersion = qFromLittleEndian<quint32>( at( 0x0c ) );
		if ( fileVersion != 11 )
			refuse( QStringLiteral( "fileVersion %1, this reader knows 11" ).arg( fileVersion ) );
		const quint8 bytesInPointer = quint8( blob[0x10] ), littleEndian = quint8( blob[0x11] );
		if ( bytesInPointer != 8 || littleEndian != 1 )
			refuse( QStringLiteral( "layout: bytesInPointer %1 littleEndian %2, this reader knows 8/1" ).arg( bytesInPointer ).arg( littleEndian ) );
		const int numSections = qFromLittleEndian<qint32>( at( 0x14 ) );
		const int contentsSection = qFromLittleEndian<qint32>( at( 0x18 ) );
		const int contentsOffset = qFromLittleEndian<qint32>( at( 0x1c ) );
		const int cnSection = qFromLittleEndian<qint32>( at( 0x20 ) );
		const int cnOffset = qFromLittleEndian<qint32>( at( 0x24 ) );
		const int pad = qFromLittleEndian<quint16>( at( 0x3e ) );
		const int shs = 0x40 + pad;
		if ( numSections < 1 || numSections > 16 || shs + numSections * 0x40 > blob.size() )
			refuse( QStringLiteral( "numSections %1 with section headers at 0x%2 does not fit %3 bytes" ).arg( numSections ).arg( shs, 0, 16 ).arg( blob.size() ) );
		f.head = blob.left( shs );
		struct Sec { QString tag; int abs, local, glob, virt, exports, imports, end; };
		QVector<Sec> secs;
		for ( int s = 0; s < numSections; s++ ) {
			const int o = shs + s * 0x40;
			Sec sc;
			sc.tag = QString::fromLatin1( blob.mid( o, 19 ).split( '\0' ).first() );
			const char * q = at( o + 20 );
			sc.abs = qFromLittleEndian<qint32>( q ); sc.local = qFromLittleEndian<qint32>( q + 4 );
			sc.glob = qFromLittleEndian<qint32>( q + 8 ); sc.virt = qFromLittleEndian<qint32>( q + 12 );
			sc.exports = qFromLittleEndian<qint32>( q + 16 ); sc.imports = qFromLittleEndian<qint32>( q + 20 );
			sc.end = qFromLittleEndian<qint32>( q + 24 );
			secs.append( sc );
		}
		const Sec * cn = nullptr; const Sec * dt = nullptr;
		for ( const Sec & s : secs ) {
			if ( s.tag == QLatin1String( "__classnames__" ) ) cn = &s;
			if ( s.tag == QLatin1String( "__data__" ) ) dt = &s;
		}
		if ( !cn || !dt )
			refuse( QStringLiteral( "no __classnames__ / __data__ section among %1" ).arg( numSections ) );
		if ( secs.size() != 3 || secs[0].tag != QLatin1String( "__classnames__" ) || secs[1].tag != QLatin1String( "__types__" ) || secs[2].tag != QLatin1String( "__data__" ) )
			refuse( QStringLiteral( "section order is not __classnames__ / __types__ / __data__" ) );
		if ( dt->abs < 0 || dt->end < 0 || dt->abs + dt->end > blob.size() )
			refuse( QStringLiteral( "__data__ end 0x%1 past the file (%2 bytes)" ).arg( dt->abs + dt->end, 0, 16 ).arg( blob.size() ) );
		if ( dt->local < 0 || dt->glob < dt->local || dt->virt < dt->glob || dt->exports < dt->virt || dt->end < dt->exports )
			refuse( QStringLiteral( "__data__ fixup offsets are not ascending (%1 %2 %3 %4 %5)" ).arg( dt->local ).arg( dt->glob ).arg( dt->virt ).arg( dt->exports ).arg( dt->end ) );
		// class names
		QHash<int, QString> byOff;
		{
			int p = cn->abs;
			const int endp = cn->abs + cn->local;
			if ( endp > blob.size() )
				refuse( QStringLiteral( "__classnames__ ends at 0x%1 past the file" ).arg( endp, 0, 16 ) );
			while ( p + 5 <= endp && quint8( blob[p + 4] ) == 0x09 ) {
				const quint32 sig = qFromLittleEndian<quint32>( at( p ) );
				const int e = blob.indexOf( '\0', p + 5 );
				if ( e < 0 || e > endp )
					refuse( QStringLiteral( "class name at 0x%1 has no terminator" ).arg( p, 0, 16 ) );
				const QString name = QString::fromLatin1( blob.mid( p + 5, e - p - 5 ) );
				byOff.insert( p + 5 - cn->abs, name );
				f.classNames.append( qMakePair( sig, name ) );
				p = e + 1;
			}
		}
		base = dt->abs;
		payloadEnd = base + dt->local;
		for ( int q = base + dt->local; q + 8 <= base + dt->glob; q += 8 ) {
			const int src = qFromLittleEndian<qint32>( at( q ) ), dst = qFromLittleEndian<qint32>( at( q + 4 ) );
			if ( src == -1 ) continue;
			if ( src < 0 || dst < 0 || src + 8 > dt->local || dst > dt->local )
				refuse( QStringLiteral( "local fixup 0x%1 -> 0x%2 is outside the payload" ).arg( src, 0, 16 ).arg( dst, 0, 16 ) );
			local.insert( base + src, base + dst );
		}
		for ( int q = base + dt->glob; q + 12 <= base + dt->virt; q += 12 ) {
			const int src = qFromLittleEndian<qint32>( at( q ) ), sec = qFromLittleEndian<qint32>( at( q + 4 ) ), dst = qFromLittleEndian<qint32>( at( q + 8 ) );
			if ( src == -1 ) continue;
			if ( sec != 2 )
				refuse( QStringLiteral( "global fixup at src 0x%1 points into section %2, only __data__ (2) is known" ).arg( src, 0, 16 ).arg( sec ) );
			if ( src < 0 || dst < 0 || src + 8 > dt->local || dst >= dt->local )
				refuse( QStringLiteral( "global fixup 0x%1 -> 0x%2 is outside the payload" ).arg( src, 0, 16 ).arg( dst, 0, 16 ) );
			glob.insert( base + src, base + dst );
		}
		QVector<QPair<int, QString>> objs;
		for ( int q = base + dt->virt; q + 12 <= base + dt->exports; q += 12 ) {
			const int off = qFromLittleEndian<qint32>( at( q ) ), cno = qFromLittleEndian<qint32>( at( q + 8 ) );
			if ( off == -1 ) continue;
			auto it = byOff.constFind( cno );
			if ( it == byOff.constEnd() )
				refuse( QStringLiteral( "virtual fixup at 0x%1 names class-name offset %2, which is not a class name" ).arg( off, 0, 16 ).arg( cno ) );
			if ( off < 0 || off >= dt->local )
				refuse( QStringLiteral( "virtual fixup object at 0x%1 is outside the payload" ).arg( off, 0, 16 ) );
			objs.append( qMakePair( base + off, it.value() ) );
		}
		if ( objs.isEmpty() )
			refuse( QStringLiteral( "no virtual fixups: the file has no objects" ) );
		if ( cnSection != 0 || !byOff.contains( cnOffset ) )
			refuse( QStringLiteral( "contents class name (section %1 offset %2) is not a class name" ).arg( cnSection ).arg( cnOffset ) );
		for ( int i = 0; i < objs.size(); i++ )
			objAt.insert( objs[i].first, i );
		if ( contentsSection != 2 || !objAt.contains( base + contentsOffset ) )
			refuse( QStringLiteral( "contents object (section %1 offset %2) is not an object" ).arg( contentsSection ).arg( contentsOffset ) );
		f.root = objAt.value( base + contentsOffset );
		f.dataStart = base;
		f.payloadLength = dt->local;
		for ( int i = 0; i < objs.size(); i++ ) {
			const ClassDef * cls = db.find( objs[i].second );
			if ( !cls )
				refuse( QStringLiteral( "class %1 is not in the class database (%2 classes)" ).arg( objs[i].second ).arg( db.count() ) );
			chunk( objs[i].first, cls->objectSize, "obj", QStringLiteral( "%1#%2" ).arg( cls->name ).arg( i ) );
			Object o;
			o.cls = cls;
			o.fileOffset = objs[i].first - base;
			o.body = readStruct( cls, objs[i].first, QStringLiteral( "%1#%2" ).arg( cls->name ).arg( i ) );
			f.objects.append( o );
		}
		if ( usedLocal.size() != local.size() ) {
			for ( auto it = local.constBegin(); it != local.constEnd(); ++it )
				if ( !usedLocal.contains( it.key() ) )
					refuse( QStringLiteral( "%1 local fixups were not consumed by any modelled member (one at src 0x%2)" ).arg( local.size() - usedLocal.size() ).arg( it.key() - base, 0, 16 ) );
		}
		if ( usedGlob.size() != glob.size() ) {
			for ( auto it = glob.constBegin(); it != glob.constEnd(); ++it )
				if ( !usedGlob.contains( it.key() ) )
					refuse( QStringLiteral( "%1 global fixups were not consumed (one at src 0x%2)" ).arg( glob.size() - usedGlob.size() ).arg( it.key() - base, 0, 16 ) );
		}
	}
};

// ---------------------------------------------------------------- writer

struct Extra
{
	enum K : quint8 { Str, StrA, Pad16, Ptr, Arr, Rel } k;
	int src = 0;
	const Value * v = nullptr;
	const MemberDef * m = nullptr;
	int object = -1;
};

struct Writer
{
	const File & f;
	const ClassDb & db;
	QByteArray data;
	QVector<QPair<int, int>> localFix;
	QVector<QPair<int, int>> globFix;      //!< (src, object index) until placed
	QVector<QPair<int, QString>> virtFix;
	QHash<int, int> placed;

	Writer( const File & file, const ClassDb & d ) : f( file ), db( d ) {}

	void align16() { while ( data.size() % 16 ) data.append( '\0' ); }
	void padFF() { while ( data.size() % 16 ) data.append( char( 0xFF ) ); }

	void encodeStruct( const Value & v, int atOff, QVector<Extra> & extras )
	{
		const ClassDef * cls = v.cls;
		const int size = cls ? cls->objectSize : 0;
		if ( v.bytes.size() == size )
			std::memcpy( data.data() + atOff, v.bytes.constData(), size_t( size ) );
		else
			std::memset( data.data() + atOff, 0, size_t( size ) );
		if ( !cls ) return;
		if ( v.elems.size() != cls->allMembers.size() )
			refuse( QStringLiteral( "a %1 value carries %2 members, the class has %3" ).arg( cls->name ).arg( v.elems.size() ).arg( cls->allMembers.size() ) );
		for ( int i = 0; i < cls->allMembers.size(); i++ )
			encodeMember( *cls->allMembers[i], v.elems[i], atOff, extras );
	}

	void encodeOne( const MemberDef & m, const Value & fv, int p, QVector<Extra> & extras )
	{
		switch ( fv.kind ) {
		case Value::Str:
			if ( !fv.isNull ) extras.append( { Extra::Str, p, &fv, &m, -1 } );
			return;
		case Value::Ptr:
			if ( fv.object >= 0 ) extras.append( { Extra::Ptr, p, &fv, &m, fv.object } );
			return;
		case Value::Struct:
			encodeStruct( fv, p, extras );
			return;
		case Value::Array:
			qToLittleEndian<qint32>( fv.count, data.data() + p + 8 );
			qToLittleEndian<quint32>( fv.capacity, data.data() + p + 12 );
			if ( fv.hasPayload ) extras.append( { Extra::Arr, p, &fv, &m, -1 } );
			return;
		case Value::RelArray:
			extras.append( { Extra::Rel, p, &fv, &m, -1 } );
			return;
		default:
			refuse( QStringLiteral( "%1: cannot encode a value of kind %2" ).arg( m.name ).arg( int( fv.kind ) ) );
		}
	}

	void encodeMember( const MemberDef & m, const Value & fv, int atOff, QVector<Extra> & extras )
	{
		const int p = atOff + m.offset;
		switch ( fv.kind ) {
		case Value::Ignored:
			if ( !fv.bytes.isEmpty() )
				std::memcpy( data.data() + p, fv.bytes.constData(), size_t( qMin( fv.bytes.size(), m.inlineSize() ) ) );
			return;
		case Value::Scalar: {
			const Type st = ( fv.type == Type::Enum || fv.type == Type::Flags ) ? fv.storage : fv.type;
			const int sz = plainSize( st );
			for ( int i = 0; i < fv.ints.size(); i++ )
				writeInt( data.data() + p + i * sz, st, fv.ints[i] );
			return;
		}
		case Value::Raw:
			std::memcpy( data.data() + p, fv.bytes.constData(), size_t( fv.bytes.size() ) );
			return;
		case Value::CArray: {
			const int esz = m.type == Type::Struct ? ( m.cls ? m.cls->objectSize : 0 ) : 8;
			for ( int i = 0; i < fv.elems.size(); i++ )
				encodeOne( m, fv.elems[i], p + i * esz, extras );
			return;
		}
		default:
			encodeOne( m, fv, p, extras );
		}
	}

	void writePlain( const Value & fv, int start, int esz )
	{
		const int n = esz * fv.count;
		if ( fv.bytes.size() >= n )
			std::memcpy( data.data() + start, fv.bytes.constData(), size_t( n ) );
		else
			std::memcpy( data.data() + start, fv.bytes.constData(), size_t( fv.bytes.size() ) );
	}

	QVector<int> flush( const QVector<Extra> & extras )
	{
		QVector<int> pointees;
		for ( const Extra & ex : extras ) {
			switch ( ex.k ) {
			case Extra::Str:
				localFix.append( qMakePair( ex.src, data.size() ) );
				data.append( ex.v->bytes );
				data.append( '\0' );
				align16();
				break;
			case Extra::StrA:
				if ( data.size() % 2 ) data.append( '\0' );
				localFix.append( qMakePair( ex.src, data.size() ) );
				data.append( ex.v->bytes );
				data.append( '\0' );
				break;
			case Extra::Pad16:
				align16();
				break;
			case Extra::Ptr:
				globFix.append( qMakePair( ex.src, ex.object ) );
				pointees.append( ex.object );
				break;
			case Extra::Arr: {
				const Value & fv = *ex.v;
				const MemberDef & m = *ex.m;
				align16();
				localFix.append( qMakePair( ex.src, data.size() ) );
				const int esz = m.elementSize();
				const int start = data.size();
				data.append( QByteArray( esz * fv.count, '\0' ) );
				QVector<QVector<Extra>> sub;
				if ( m.subtype == Type::Struct ) {
					if ( fv.elems.size() != fv.count )
						refuse( QStringLiteral( "%1: array count %2 but %3 elements" ).arg( m.name ).arg( fv.count ).arg( fv.elems.size() ) );
					for ( int i = 0; i < fv.count; i++ ) {
						QVector<Extra> e;
						encodeStruct( fv.elems[i], start + i * esz, e );
						sub.append( e );
					}
				} else if ( m.subtype == Type::Pointer ) {
					for ( int i = 0; i < fv.elems.size() && i < fv.count; i++ )
						if ( fv.elems[i].object >= 0 )
							sub.append( { { Extra::Ptr, start + i * 8, &fv.elems[i], &m, fv.elems[i].object } } );
					align16();
				} else if ( isStringType( m.subtype ) ) {
					bool any = false;
					for ( int i = 0; i < fv.elems.size() && i < fv.count; i++ ) {
						if ( !fv.elems[i].isNull ) {
							sub.append( { { Extra::StrA, start + i * 8, &fv.elems[i], &m, -1 } } );
							any = true;
						}
					}
					if ( any )
						sub.append( { { Extra::Pad16, 0, nullptr, nullptr, -1 } } );
				} else {
					writePlain( fv, start, esz );
					align16();
				}
				for ( const QVector<Extra> & s : sub )
					pointees += flush( s );
				break;
			}
			case Extra::Rel:
				refuse( QStringLiteral( "an hkRelArray inside an array element is not modelled" ) );
			}
		}
		return pointees;
	}

	void writeObject( int i )
	{
		if ( placed.contains( i ) )
			return;
		align16();
		const Object & obj = f.objects[i];
		const ClassDef * cls = obj.cls;
		const int off = data.size();
		placed.insert( i, off );
		virtFix.append( qMakePair( off, cls->name ) );
		data.append( QByteArray( cls->objectSize, '\0' ) );
		QVector<Extra> extras;
		encodeStruct( obj.body, off, extras );
		// hkRelArray payloads live inside the object's chunk, right after the
		// body, each 16-aligned, in member order; the u16 offset counts from
		// the member's own address.
		QVector<Extra> rest;
		for ( const Extra & ex : extras ) {
			if ( ex.k != Extra::Rel ) {
				rest.append( ex );
				continue;
			}
			const Value & fv = *ex.v;
			const MemberDef & m = *ex.m;
			align16();
			const int start = data.size();
			const int esz = m.elementSize();
			data.append( QByteArray( esz * fv.count, '\0' ) );
			if ( m.subtype == Type::Struct ) {
				for ( int k = 0; k < fv.count && k < fv.elems.size(); k++ ) {
					QVector<Extra> e;
					encodeStruct( fv.elems[k], start + k * esz, e );
					if ( !e.isEmpty() )
						refuse( QStringLiteral( "%1: an hkRelArray element with pointers or strings is not modelled" ).arg( m.name ) );
				}
			} else {
				writePlain( fv, start, esz );
			}
			qToLittleEndian<quint16>( quint16( fv.count ), data.data() + ex.src );
			qToLittleEndian<quint16>( quint16( fv.count ? start - ex.src : 0 ), data.data() + ex.src + 2 );
		}
		align16();
		const QVector<int> pointees = flush( rest );
		for ( int p : pointees )
			writeObject( p );
	}

	QByteArray run()
	{
		if ( f.root < 0 || f.root >= f.objects.size() )
			refuse( QStringLiteral( "no root object" ) );
		writeObject( f.root );
		for ( int i = 0; i < f.objects.size(); i++ )
			writeObject( i );
		align16();
		const int localOff = data.size();
		for ( const auto & lf : localFix ) {
			QByteArray e( 8, '\0' );
			qToLittleEndian<qint32>( lf.first, e.data() );
			qToLittleEndian<qint32>( lf.second, e.data() + 4 );
			data.append( e );
		}
		padFF();
		const int globOff = data.size();
		for ( const auto & gf : globFix ) {
			QByteArray e( 12, '\0' );
			qToLittleEndian<qint32>( gf.first, e.data() );
			qToLittleEndian<qint32>( 2, e.data() + 4 );
			qToLittleEndian<qint32>( placed.value( gf.second ), e.data() + 8 );
			data.append( e );
		}
		padFF();
		const int virtOff = data.size();
		// class names: the file's order, then any class used for the first time
		QVector<QPair<quint32, QString>> names = f.classNames;
		QSet<QString> have;
		for ( const auto & n : names ) have.insert( n.second );
		for ( const auto & vf : virtFix ) {
			if ( !have.contains( vf.second ) ) {
				const ClassDef * c = db.find( vf.second );
				names.append( qMakePair( c ? c->signature : 0u, vf.second ) );
				have.insert( vf.second );
			}
		}
		QByteArray cn;
		QHash<QString, int> nameOff;
		for ( const auto & n : names ) {
			QByteArray e( 4, '\0' );
			qToLittleEndian<quint32>( n.first, e.data() );
			cn.append( e );
			cn.append( '\x09' );
			nameOff.insert( n.second, cn.size() );
			cn.append( n.second.toLatin1() );
			cn.append( '\0' );
		}
		while ( cn.size() % 16 ) cn.append( char( 0xFF ) );
		for ( const auto & vf : virtFix ) {
			QByteArray e( 12, '\0' );
			qToLittleEndian<qint32>( vf.first, e.data() );
			qToLittleEndian<qint32>( 0, e.data() + 4 );
			qToLittleEndian<qint32>( nameOff.value( vf.second ), e.data() + 8 );
			data.append( e );
		}
		padFF();
		const int endOff = data.size();
		// header
		QByteArray head = f.head;
		if ( head.size() < 0x40 )
			refuse( QStringLiteral( "the file header is %1 bytes, at least 0x40 are needed" ).arg( head.size() ) );
		const int shs = head.size();
		qToLittleEndian<qint32>( 3, head.data() + 0x14 );
		qToLittleEndian<qint32>( 2, head.data() + 0x18 );
		qToLittleEndian<qint32>( placed.value( f.root ), head.data() + 0x1c );
		qToLittleEndian<qint32>( 0, head.data() + 0x20 );
		qToLittleEndian<qint32>( nameOff.value( f.objects[f.root].cls->name ), head.data() + 0x24 );
		qToLittleEndian<quint16>( quint16( shs - 0x40 ), head.data() + 0x3e );
		QByteArray out = head;
		const int cnAbs = shs + 3 * 0x40;
		const int dtAbs = cnAbs + cn.size();
		auto sechdr = [&]( const char * tag, int absStart, int l, int g, int v, int e ) {
			QByteArray h( 20, '\0' );
			std::memcpy( h.data(), tag, std::strlen( tag ) );
			h[19] = char( 0xFF );
			QByteArray nums( 28, '\0' );
			const int vals[7] = { absStart, l, g, v, e, e, e };
			for ( int i = 0; i < 7; i++ ) qToLittleEndian<qint32>( vals[i], nums.data() + 4 * i );
			h.append( nums );
			h.append( QByteArray( 16, char( 0xFF ) ) );
			return h;
		};
		out.append( sechdr( "__classnames__", cnAbs, cn.size(), cn.size(), cn.size(), cn.size() ) );
		out.append( sechdr( "__types__", dtAbs, 0, 0, 0, 0 ) );
		out.append( sechdr( "__data__", dtAbs, localOff, globOff, virtOff, endOff ) );
		out.append( cn );
		out.append( data );
		return out;
	}
};

} // namespace

// ---------------------------------------------------------------- File

bool File::read( const QByteArray & blob, const ClassDb & db )
{
	*this = File();
	try {
		Reader r( blob, db, *this );
		r.run();
	} catch ( const Refusal & e ) {
		const QVector<Chunk> keep = chunks;
		*this = File();
		error = e.msg;
		return false;
	}
	return true;
}

QByteArray File::write( const ClassDb & db ) const
{
	try {
		Writer w( *this, db );
		return w.run();
	} catch ( const Refusal & e ) {
		const_cast<File *>( this )->error = e.msg;
		return QByteArray();
	}
}

qint64 File::roundTripDiff( const QByteArray & blob, const ClassDb & db, QString * error, QByteArray * out )
{
	File f;
	if ( !f.read( blob, db ) ) {
		if ( error ) *error = f.error;
		return -2;
	}
	const QByteArray w = f.write( db );
	if ( w.isEmpty() ) {
		if ( error ) *error = f.error;
		return -3;
	}
	if ( out ) *out = w;
	const int n = qMin( blob.size(), w.size() );
	for ( int i = 0; i < n; i++ )
		if ( blob[i] != w[i] )
			return i;
	return blob.size() == w.size() ? -1 : n;
}

Value * File::find( const QString & path, QString * err )
{
	return const_cast<Value *>( const_cast<const File *>( this )->find( path, err ) );
}

const Value * File::find( const QString & path, QString * err ) const
{
	auto fail = [&]( const QString & s ) -> const Value * { if ( err ) *err = s; return nullptr; };
	if ( !path.startsWith( QLatin1Char( '#' ) ) )
		return fail( QStringLiteral( "path %1 does not start with #object" ).arg( path ) );
	const int dot = path.indexOf( QLatin1Char( '.' ) );
	bool ok = false;
	const int oi = path.mid( 1, dot < 0 ? -1 : dot - 1 ).toInt( &ok );
	if ( !ok || oi < 0 || oi >= objects.size() )
		return fail( QStringLiteral( "path %1: no object %2" ).arg( path ).arg( oi ) );
	const Value * v = &objects[oi].body;
	if ( dot < 0 )
		return v;
	const QStringList parts = path.mid( dot + 1 ).split( QLatin1Char( '.' ) );
	for ( const QString & part : parts ) {
		const int br = part.indexOf( QLatin1Char( '[' ) );
		const QString name = br < 0 ? part : part.left( br );
		if ( v->kind != Value::Struct || !v->cls )
			return fail( QStringLiteral( "path %1: %2 is not a struct" ).arg( path, name ) );
		int mi = -1;
		for ( int k = 0; k < v->cls->allMembers.size(); k++ )
			if ( v->cls->allMembers[k]->name == name ) { mi = k; break; }
		if ( mi < 0 )
			return fail( QStringLiteral( "path %1: %2 has no member %3" ).arg( path, v->cls->name, name ) );
		v = &v->elems[mi];
		QString idx = br < 0 ? QString() : part.mid( br );
		while ( !idx.isEmpty() ) {
			const int close = idx.indexOf( QLatin1Char( ']' ) );
			const int i = idx.mid( 1, close - 1 ).toInt( &ok );
			if ( !ok || i < 0 || i >= v->elems.size() )
				return fail( QStringLiteral( "path %1: index %2 is outside %3 elements" ).arg( path ).arg( idx.mid( 1, close - 1 ) ).arg( v->elems.size() ) );
			v = &v->elems[i];
			idx = idx.mid( close + 1 );
		}
	}
	return v;
}

bool File::resizeArray( Value & a, int n, QString * err )
{
	if ( a.kind != Value::Array && a.kind != Value::RelArray ) {
		if ( err ) *err = QStringLiteral( "not an array" );
		return false;
	}
	if ( n < 0 ) {
		if ( err ) *err = QStringLiteral( "negative size %1" ).arg( n );
		return false;
	}
	const MemberDef * m = a.member;
	if ( !m ) {
		if ( err ) *err = QStringLiteral( "the array has no member definition" );
		return false;
	}
	const int esz = m->elementSize();
	if ( m->subtype == Type::Struct || m->subtype == Type::Pointer || isStringType( m->subtype ) ) {
		while ( a.elems.size() > n ) a.elems.removeLast();
		while ( a.elems.size() < n ) a.elems.append( defaultElement( *m ) );
	} else {
		a.bytes.resize( esz * n );
		if ( a.bytes.size() > esz * a.count )
			std::memset( a.bytes.data() + esz * a.count, 0, size_t( a.bytes.size() - esz * a.count ) );
	}
	a.count = n;
	if ( a.kind == Value::Array ) {
		a.capacity = quint32( n ) | 0x80000000u;
		a.hasPayload = n > 0 || a.hasPayload;
	}
	return true;
}

int File::addObject( const ClassDef * cls )
{
	Object o;
	o.cls = cls;
	o.body = defaultStruct( cls );
	o.fileOffset = -1;
	objects.append( o );
	return objects.size() - 1;
}

QStringList File::objectSummary() const
{
	QStringList out;
	for ( int i = 0; i < objects.size(); i++ )
		out << QStringLiteral( "#%1 %2 @0x%3" ).arg( i ).arg( objects[i].cls->name ).arg( objects[i].fileOffset, 0, 16 );
	return out;
}

QStringList File::classesUsed() const
{
	QStringList out;
	for ( const Object & o : objects )
		if ( !out.contains( o.cls->name ) )
			out << o.cls->name;
	return out;
}

// ---------------------------------------------------------------- dump

static QString floatsOf( const QByteArray & b )
{
	QStringList s;
	for ( int i = 0; i + 4 <= b.size(); i += 4 ) {
		float f;
		std::memcpy( &f, b.constData() + i, 4 );
		s << QString::number( f, 'g', 7 );
	}
	return QLatin1Char( '(' ) + s.join( QLatin1Char( ' ' ) ) + QLatin1Char( ')' );
}

QString describe( const Value & v, const File * file )
{
	switch ( v.kind ) {
	case Value::Ignored: return QStringLiteral( "<ignored %1 bytes>" ).arg( v.bytes.size() );
	case Value::Scalar: {
		QStringList s;
		for ( int i = 0; i < v.ints.size(); i++ ) {
			if ( v.type == Type::Real ) s << QString::number( v.toFloat( i ), 'g', 9 );
			else s << QString::number( v.ints[i] );
		}
		QString out = s.join( QLatin1Char( ' ' ) );
		if ( v.member && v.member->en && v.ints.size() == 1 ) {
			const QString n = v.member->en->nameOf( v.ints[0] );
			if ( !n.isEmpty() ) out += QLatin1String( " (" ) + n + QLatin1Char( ')' );
		}
		return out;
	}
	case Value::Raw: return floatsOf( v.bytes );
	case Value::Str: return v.isNull ? QStringLiteral( "null" ) : QLatin1Char( '"' ) + QString::fromLatin1( v.bytes ) + QLatin1Char( '"' );
	case Value::Ptr:
		if ( v.object < 0 ) return QStringLiteral( "null" );
		return QStringLiteral( "#%1%2" ).arg( v.object ).arg( file && v.object < file->objects.size() ? QLatin1Char( ' ' ) + file->objects[v.object].cls->name : QString() );
	case Value::Struct: return QStringLiteral( "%1 {%2 members}" ).arg( v.cls ? v.cls->name : QStringLiteral( "?" ) ).arg( v.elems.size() );
	case Value::Array: case Value::RelArray:
		return QStringLiteral( "%1<%2> n=%3 cap=0x%4%5" ).arg( v.kind == Value::Array ? QStringLiteral( "hkArray" ) : QStringLiteral( "hkRelArray" ),
			v.cls ? v.cls->name : QLatin1String( typeName( v.type ) ) ).arg( v.count ).arg( v.capacity, 0, 16 )
			.arg( v.hasPayload ? QString() : QStringLiteral( " (no payload)" ) );
	case Value::CArray: return QStringLiteral( "[%1 x %2]" ).arg( v.elems.size() ).arg( QLatin1String( typeName( v.type ) ) );
	}
	return QStringLiteral( "?" );
}

static void dumpValue( QString & out, const Value & v, const File & file, const QString & indent )
{
	if ( v.kind == Value::Struct && v.cls ) {
		out += v.cls->name + QLatin1String( " {\n" );
		for ( int i = 0; i < v.cls->allMembers.size() && i < v.elems.size(); i++ ) {
			out += indent + QLatin1String( "  " ) + v.cls->allMembers[i]->name + QLatin1String( " = " );
			dumpValue( out, v.elems[i], file, indent + QLatin1String( "  " ) );
		}
		out += indent + QLatin1String( "}\n" );
		return;
	}
	if ( ( v.kind == Value::Array || v.kind == Value::RelArray ) ) {
		out += describe( v, &file );
		if ( !v.elems.isEmpty() ) {
			out += QLatin1Char( '\n' );
			for ( int i = 0; i < v.elems.size() && i < 2000; i++ ) {
				out += indent + QStringLiteral( "  [%1] " ).arg( i );
				dumpValue( out, v.elems[i], file, indent + QLatin1String( "  " ) );
			}
		} else if ( v.isPlainArray() ) {
			QStringList s;
			const int n = qMin( v.count, 64 );
			if ( isRawType( v.type ) ) {
				const int esz = plainSize( v.type );
				for ( int i = 0; i < qMin( v.count, 8 ); i++ ) s << floatsOf( v.bytes.mid( i * esz, esz ) );
			} else {
				for ( int i = 0; i < n; i++ )
					s << ( v.type == Type::Real ? QString::number( v.plainFloat( i ), 'g', 7 ) : QString::number( v.plainInt( i ) ) );
			}
			out += QLatin1Char( ' ' ) + s.join( QLatin1Char( ' ' ) ) + ( v.count > n ? QStringLiteral( " ..." ) : QString() ) + QLatin1Char( '\n' );
		} else {
			out += QLatin1Char( '\n' );
		}
		return;
	}
	if ( v.kind == Value::CArray ) {
		out += QLatin1Char( '[' );
		for ( int i = 0; i < v.elems.size(); i++ ) {
			if ( i ) out += QLatin1String( ", " );
			if ( v.elems[i].kind == Value::Struct ) {
				dumpValue( out, v.elems[i], file, indent + QLatin1String( "  " ) );
				out.chop( 1 );
			} else {
				out += describe( v.elems[i], &file );
			}
		}
		out += QLatin1String( "]\n" );
		return;
	}
	out += describe( v, &file ) + QLatin1Char( '\n' );
}

QString dump( const File & file )
{
	QString out;
	QStringList cn;
	for ( const auto & n : file.classNames )
		cn << QStringLiteral( "%1 0x%2" ).arg( n.second ).arg( n.first, 8, 16, QLatin1Char( '0' ) );
	out += QLatin1String( "classnames: " ) + cn.join( QLatin1String( ", " ) ) + QLatin1Char( '\n' );
	for ( int i = 0; i < file.objects.size(); i++ ) {
		out += QStringLiteral( "#%1 @0x%2 " ).arg( i ).arg( file.objects[i].fileOffset, 0, 16 );
		dumpValue( out, file.objects[i].body, file, QString() );
		out += QLatin1Char( '\n' );
	}
	return out;
}

} // namespace Hkx
