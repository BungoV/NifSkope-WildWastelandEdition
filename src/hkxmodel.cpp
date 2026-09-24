/* The Blocks-tab model for a Havok packfile -- lane HKXEDIT1, 2026-09-10.
   See hkxmodel.h for the shape; docs/HKX_PACKFILE_MODEL.md for the file. */

#include "hkxmodel.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QUndoCommand>

#include <cmath>
#include <cstring>

using namespace Hkx;

// ---------------------------------------------------------------- naming

//! The Havok spelling of a plain type, for the Type column.
static QString hkTypeName( Type t )
{
	switch ( t ) {
	case Type::Bool: return QStringLiteral( "hkBool" );
	case Type::Char: return QStringLiteral( "hkChar" );
	case Type::Int8: return QStringLiteral( "hkInt8" );
	case Type::UInt8: return QStringLiteral( "hkUint8" );
	case Type::Int16: return QStringLiteral( "hkInt16" );
	case Type::UInt16: return QStringLiteral( "hkUint16" );
	case Type::Int32: return QStringLiteral( "hkInt32" );
	case Type::UInt32: return QStringLiteral( "hkUint32" );
	case Type::Int64: return QStringLiteral( "hkInt64" );
	case Type::UInt64: return QStringLiteral( "hkUint64" );
	case Type::ULong: return QStringLiteral( "hkUlong" );
	case Type::Real: return QStringLiteral( "hkReal" );
	case Type::Half: return QStringLiteral( "hkHalf" );
	case Type::Vector4: return QStringLiteral( "hkVector4" );
	case Type::Quaternion: return QStringLiteral( "hkQuaternion" );
	case Type::Matrix3: return QStringLiteral( "hkMatrix3" );
	case Type::Rotation: return QStringLiteral( "hkRotation" );
	case Type::QsTransform: return QStringLiteral( "hkQsTransform" );
	case Type::Matrix4: return QStringLiteral( "hkMatrix4" );
	case Type::Transform: return QStringLiteral( "hkTransform" );
	case Type::CString: return QStringLiteral( "char*" );
	case Type::StringPtr: return QStringLiteral( "hkStringPtr" );
	case Type::Pointer: return QStringLiteral( "pointer" );
	default: return QString::fromLatin1( typeName( t ) );
	}
}

static QString enumId( const MemberDef & m, bool flags )
{
	// "Class::Enum" is what the delegate looks up; the FLAGS spelling is a
	// second registration whose option values are BIT INDICES (NifCheckBoxList)
	QString owner;
	if ( m.en ) {
		// the owner is whichever class declares it; search is by pointer identity
		owner = m.enumName;
	}
	return ( flags ? QStringLiteral( "hkFlags " ) : QStringLiteral( "hkEnum " ) ) + m.enumName
		+ QStringLiteral( "@%1" ).arg( quintptr( m.en ), 0, 16 );
}

static QString memberTypeName( const MemberDef & m, const Value & v )
{
	switch ( m.type ) {
	case Type::Enum: return m.en ? enumId( m, false ) : hkTypeName( m.subtype );
	case Type::Flags: return m.en ? enumId( m, true ) : hkTypeName( m.subtype );
	case Type::Struct: return m.cls ? m.cls->name : QStringLiteral( "struct" );
	case Type::Pointer: return ( m.cls ? m.cls->name : QStringLiteral( "void" ) ) + QLatin1Char( '*' );
	case Type::Array: case Type::RelArray: {
		const QString el = m.subtype == Type::Struct ? ( m.cls ? m.cls->name : QStringLiteral( "struct" ) )
			: m.subtype == Type::Pointer ? ( m.cls ? m.cls->name : QStringLiteral( "void" ) ) + QLatin1Char( '*' )
			: hkTypeName( m.subtype );
		return ( m.type == Type::Array ? QStringLiteral( "hkArray<" ) : QStringLiteral( "hkRelArray<" ) ) + el + QLatin1Char( '>' );
	}
	default:
		Q_UNUSED( v );
		return hkTypeName( m.type );
	}
}

static NifValue::Type scalarValueType( Type t )
{
	switch ( t ) {
	case Type::Bool: return NifValue::tBool;
	case Type::Char: case Type::Int8: case Type::UInt8: return NifValue::tByte;
	case Type::Int16: return NifValue::tShort;
	case Type::UInt16: case Type::Half: return NifValue::tWord;
	case Type::Int32: return NifValue::tInt;
	case Type::UInt32: return NifValue::tUInt;
	case Type::Int64: return NifValue::tInt64;
	case Type::UInt64: case Type::ULong: return NifValue::tUInt64;
	case Type::Real: return NifValue::tFloat;
	default: return NifValue::tNone;
	}
}

static NifData leaf( const QString & name, const QString & type, NifValue::Type vt )
{
	NifData d( name, type, QString(), NifValue( vt ), QString() );
	d.setIsConditionless( true );
	return d;
}

static NifData branch( const QString & name, const QString & type, bool isArray )
{
	NifData d( name, type, QString(), NifValue( NifValue::tNone ), QString() );
	d.setIsConditionless( true );
	d.setIsCompound( !isArray );
	d.setIsArray( isArray );
	return d;
}

// ---------------------------------------------------------------- commands

class HkxSetValueCommand : public QUndoCommand
{
public:
	HkxSetValueCommand( HkxModel * m, const QModelIndex & idx, const QVariant & oldV, const QVariant & newV, const QString & what )
		: model( m ), index( idx ), oldValue( oldV ), newValue( newV )
	{
		setText( QCoreApplication::translate( "HkxModel", "Set %1" ).arg( what ) );
	}
	void redo() override { model->setDataDirect( index, newValue ); }
	void undo() override { model->setDataDirect( index, oldValue ); }
private:
	HkxModel * model;
	QPersistentModelIndex index;
	QVariant oldValue, newValue;
};

class HkxArrayResizeCommand : public QUndoCommand
{
public:
	HkxArrayResizeCommand( HkxModel * m, const QModelIndex & idx, int oldN, int newN )
		: model( m ), index( idx ), oldCount( oldN ), newCount( newN )
	{
		setText( QCoreApplication::translate( "HkxModel", "Resize %1 to %2" ).arg( idx.data().toString() ).arg( newN ) );
	}
	void redo() override { model->resizeDirect( index, newCount ); }
	void undo() override { model->resizeDirect( index, oldCount ); }
private:
	HkxModel * model;
	QPersistentModelIndex index;
	int oldCount, newCount;
};

// ---------------------------------------------------------------- model

HkxModel::HkxModel( QObject * parent ) : BaseModel( parent )
{
	undoStack = new QUndoStack( this );
	QString err;
	db = ClassDb::instance( &err );
	if ( !db )
		lastError = err;
	else
		registerEnums( *db );
	clear();
}

HkxModel::~HkxModel()
{
}

void HkxModel::registerEnums( const ClassDb & db )
{
	static bool done = false;
	if ( done ) return;
	done = true;
	for ( const QString & cn : db.names() ) {
		const ClassDef * c = db.find( cn );
		for ( const MemberDef & m : c->members ) {
			if ( !m.en || ( m.type != Type::Enum && m.type != Type::Flags ) )
				continue;
			const bool flags = m.type == Type::Flags;
			const QString id = enumId( m, flags );
			if ( NifValue::enumType( id ) != NifValue::eNone )
				continue;
			NifValue::registerEnumType( id, flags ? NifValue::eFlags : NifValue::eDefault );
			for ( const auto & it : m.en->items ) {
				if ( flags ) {
					// NifCheckBoxList tests `value & (1 << option)`: options are bit indices
					const quint32 mask = quint32( it.first );
					if ( mask && ( mask & ( mask - 1 ) ) == 0 ) {
						int bit = 0;
						while ( ( 1u << bit ) != mask ) bit++;
						NifValue::registerEnumOption( id, it.second, quint32( bit ), QString() );
					}
				} else {
					NifValue::registerEnumOption( id, it.second, quint32( it.first ), QString() );
				}
			}
		}
	}
}

void HkxModel::clear()
{
	beginResetModel();
	fileinfo = QFileInfo();
	filename.clear();
	folder.clear();
	root->killChildren();
	hkfile = File();
	endResetModel();
	if ( undoStack ) {
		undoStack->clear();
		undoStack->setClean();
	}
}

bool HkxModel::load( QIODevice & device, const char * fileName )
{
	Q_UNUSED( fileName );
	clear();
	if ( !db ) {
		if ( lastError.isEmpty() )
			lastError = QStringLiteral( "no class database" );
		return false;
	}
	const QByteArray blob = device.readAll();
	File f;
	if ( !f.read( blob, *db ) ) {
		lastError = f.error;
		return false;
	}
	lastError.clear();
	beginResetModel();
	hkfile = f;
	buildItems();
	endResetModel();
	if ( undoStack ) {
		undoStack->clear();
		undoStack->setClean();
	}
	return true;
}

bool HkxModel::save( QIODevice & device ) const
{
	const QByteArray bytes = toBytes();
	if ( bytes.isEmpty() )
		return false;
	const bool ok = device.write( bytes ) == bytes.size();
	if ( ok && undoStack )
		undoStack->setClean();
	return ok;
}

QByteArray HkxModel::toBytes() const
{
	if ( !db || hkfile.objects.isEmpty() )
		return QByteArray();
	syncFromItems();
	const QByteArray out = hkfile.write( *db );
	if ( out.isEmpty() )
		const_cast<HkxModel *>( this )->lastError = hkfile.error;
	return out;
}

const File & HkxModel::file() const
{
	syncFromItems();
	return hkfile;
}

HkxAnimFile HkxModel::animFile() const
{
	return hkxAnimLoadPackfile( toBytes(), getFilename() );
}

// ---------------------------------------------------------------- items <- values

void HkxModel::buildItems()
{
	root->prepareInsert( hkfile.objects.size() );
	for ( int i = 0; i < hkfile.objects.size(); i++ ) {
		const Object & o = hkfile.objects[i];
		NifItem * item = buildValue( root, o.cls->name, o.body, nullptr );
		Q_UNUSED( item );
	}
}

static Vector4 vec4At( const QByteArray & b, int off )
{
	float f[4] = { 0, 0, 0, 0 };
	if ( off + 16 <= b.size() )
		std::memcpy( f, b.constData() + off, 16 );
	return Vector4( f[0], f[1], f[2], f[3] );
}

static Quat quatAt( const QByteArray & b, int off )
{
	float f[4] = { 0, 0, 0, 1 };
	if ( off + 16 <= b.size() )
		std::memcpy( f, b.constData() + off, 16 );
	return Quat( f[3], f[0], f[1], f[2] );	// file x y z w -> Quat( w, x, y, z )
}

//! One raw vector-typed value (or one element of a raw array) as items under `parent`.
static void buildRaw( NifItem * parent, Type t, const QByteArray & b, int off )
{
	auto v4 = [&]( const QString & name, int o ) {
		NifItem * it = parent->insertChild( leaf( name, QStringLiteral( "hkVector4" ), NifValue::tVector4 ) );
		it->set<Vector4>( vec4At( b, o ) );
	};
	switch ( t ) {
	case Type::Vector4:
		parent->set<Vector4>( vec4At( b, off ) );
		break;
	case Type::Quaternion:
		parent->set<Quat>( quatAt( b, off ) );
		break;
	case Type::Matrix3: case Type::Rotation:
		v4( QStringLiteral( "Column 0" ), off ); v4( QStringLiteral( "Column 1" ), off + 16 ); v4( QStringLiteral( "Column 2" ), off + 32 );
		break;
	case Type::Matrix4: case Type::Transform:
		v4( QStringLiteral( "Column 0" ), off ); v4( QStringLiteral( "Column 1" ), off + 16 );
		v4( QStringLiteral( "Column 2" ), off + 32 ); v4( QStringLiteral( "Column 3" ), off + 48 );
		break;
	case Type::QsTransform: {
		v4( QStringLiteral( "Translation" ), off );
		NifItem * q = parent->insertChild( leaf( QStringLiteral( "Rotation" ), QStringLiteral( "hkQuaternion" ), NifValue::tQuatXYZW ) );
		q->set<Quat>( quatAt( b, off + 16 ) );
		v4( QStringLiteral( "Scale" ), off + 32 );
		break;
	}
	default:
		break;
	}
}

static bool rawIsLeaf( Type t ) { return t == Type::Vector4 || t == Type::Quaternion; }

static bool isRawTypeForSync( Type t )
{
	return t == Type::Vector4 || t == Type::Quaternion || t == Type::Matrix3 || t == Type::Rotation
		|| t == Type::QsTransform || t == Type::Matrix4 || t == Type::Transform;
}

static NifValue::Type rawLeafType( Type t ) { return t == Type::Quaternion ? NifValue::tQuatXYZW : NifValue::tVector4; }

NifItem * HkxModel::buildPlainElement( NifItem * parent, const QString & name, const Value & a, int i, int at )
{
	const Type t = a.type;
	if ( t == Type::Real ) {
		NifItem * it = parent->insertChild( leaf( name, QStringLiteral( "hkReal" ), NifValue::tFloat ), at );
		it->setFloatValue( a.plainFloat( i ) );
		return it;
	}
	if ( t == Type::Vector4 || t == Type::Quaternion || t == Type::Matrix3 || t == Type::Rotation
		|| t == Type::QsTransform || t == Type::Matrix4 || t == Type::Transform ) {
		const int esz = plainSize( t );
		if ( rawIsLeaf( t ) ) {
			NifItem * it = parent->insertChild( leaf( name, hkTypeName( t ), rawLeafType( t ) ), at );
			buildRaw( it, t, a.bytes, i * esz );
			return it;
		}
		NifItem * it = parent->insertChild( branch( name, hkTypeName( t ), false ), NifValue::tNone, at );
		buildRaw( it, t, a.bytes, i * esz );
		return it;
	}
	if ( t == Type::Enum || t == Type::Flags ) {
		const Type st = a.member ? a.member->subtype : Type::Int32;
		NifItem * it = parent->insertChild( leaf( name, hkTypeName( st ), NifValue::tUInt ), at );
		it->setCountValue( quint64( a.plainInt( i ) ) );
		return it;
	}
	const NifValue::Type vt = scalarValueType( t );
	NifItem * it = parent->insertChild( leaf( name, hkTypeName( t ), vt == NifValue::tNone ? NifValue::tUInt : vt ), at );
	it->setCountValue( quint64( a.plainInt( i ) ) );
	return it;
}

NifItem * HkxModel::buildElement( NifItem * parent, const QString & name, const Value & v, const MemberDef * am, int at )
{
	// an element of an array (or a C array): typed by the array member's subtype
	MemberDef em;
	if ( am ) {
		em = *am;
		em.type = v.kind == Value::Struct ? Type::Struct : v.kind == Value::Ptr ? Type::Pointer : v.kind == Value::Str ? am->subtype : am->subtype;
		em.cArraySize = 0;
	}
	return buildValue( parent, name, v, am ? &em : nullptr, at );
}

NifItem * HkxModel::buildValue( NifItem * parent, const QString & name, const Value & v, const MemberDef * m, int at )
{
	switch ( v.kind ) {
	case Value::Ignored:
		return nullptr;	// not serialised: nothing to show, nothing to edit
	case Value::Scalar: {
		const bool isEnum = v.type == Type::Enum || v.type == Type::Flags;
		const QString tn = m ? memberTypeName( *m, v ) : hkTypeName( v.type );
		NifValue::Type vt = isEnum ? NifValue::tUInt : scalarValueType( v.type );
		if ( vt == NifValue::tNone ) vt = NifValue::tUInt;
		if ( v.ints.size() <= 1 ) {
			NifItem * it = parent->insertChild( leaf( name, tn, vt ), at );
			if ( v.type == Type::Real ) it->setFloatValue( v.toFloat() );
			else it->setCountValue( quint64( v.toInt() ) );
			return it;
		}
		NifItem * br = parent->insertChild( branch( name, tn + QStringLiteral( "[%1]" ).arg( v.ints.size() ), true ), NifValue::tNone, at );
		for ( int i = 0; i < v.ints.size(); i++ ) {
			NifItem * it = br->insertChild( leaf( QStringLiteral( "[%1]" ).arg( i ), tn, vt ) );
			if ( v.type == Type::Real ) it->setFloatValue( v.toFloat( i ) );
			else it->setCountValue( quint64( v.ints[i] ) );
		}
		return br;
	}
	case Value::Raw: {
		const int esz = plainSize( v.type );
		const int n = esz ? v.bytes.size() / esz : 0;
		if ( n <= 1 ) {
			if ( rawIsLeaf( v.type ) ) {
				NifItem * it = parent->insertChild( leaf( name, hkTypeName( v.type ), rawLeafType( v.type ) ), at );
				buildRaw( it, v.type, v.bytes, 0 );
				return it;
			}
			NifItem * br = parent->insertChild( branch( name, hkTypeName( v.type ), false ), NifValue::tNone, at );
			buildRaw( br, v.type, v.bytes, 0 );
			return br;
		}
		NifItem * arr = parent->insertChild( branch( name, hkTypeName( v.type ) + QStringLiteral( "[%1]" ).arg( n ), true ), NifValue::tNone, at );
		for ( int i = 0; i < n; i++ ) {
			if ( rawIsLeaf( v.type ) ) {
				NifItem * it = arr->insertChild( leaf( QStringLiteral( "[%1]" ).arg( i ), hkTypeName( v.type ), rawLeafType( v.type ) ) );
				buildRaw( it, v.type, v.bytes, i * esz );
			} else {
				NifItem * br = arr->insertChild( branch( QStringLiteral( "[%1]" ).arg( i ), hkTypeName( v.type ), false ), NifValue::tNone );
				buildRaw( br, v.type, v.bytes, i * esz );
			}
		}
		return arr;
	}
	case Value::Str: {
		NifItem * it = parent->insertChild( leaf( name, m ? memberTypeName( *m, v ) : hkTypeName( v.type ), NifValue::tSizedString ), at );
		it->set<QString>( v.toStringValue() );
		return it;
	}
	case Value::Ptr: {
		NifItem * it = parent->insertChild( leaf( name, m ? memberTypeName( *m, v ) : QStringLiteral( "pointer" ), NifValue::tLink ), at );
		it->set<int>( v.object );
		return it;
	}
	case Value::Struct: {
		NifItem * br = parent->insertChild( branch( name, v.cls ? v.cls->name : QStringLiteral( "struct" ), false ), NifValue::tNone, at );
		if ( v.cls ) {
			br->prepareInsert( v.elems.size() );
			for ( int i = 0; i < v.cls->allMembers.size() && i < v.elems.size(); i++ )
				buildValue( br, v.cls->allMembers[i]->name, v.elems[i], v.cls->allMembers[i] );
		}
		return br;
	}
	case Value::CArray: {
		NifItem * br = parent->insertChild( branch( name, ( m ? memberTypeName( *m, v ) : QString() ) + QStringLiteral( "[%1]" ).arg( v.elems.size() ), true ), NifValue::tNone, at );
		for ( int i = 0; i < v.elems.size(); i++ )
			buildElement( br, QStringLiteral( "[%1]" ).arg( i ), v.elems[i], m );
		return br;
	}
	case Value::Array: case Value::RelArray: {
		const QString tn = m ? memberTypeName( *m, v ) : QStringLiteral( "hkArray" );
		if ( v.type == Type::UInt8 || v.type == Type::Int8 || v.type == Type::Char ) {
			// a byte blob (the spline data, cloth buffers): one item, not one per byte
			NifItem * it = parent->insertChild( leaf( name, tn, NifValue::tByteArray ), at );
			it->set<QByteArray>( v.bytes.left( v.count ) );
			return it;
		}
		NifItem * arr = parent->insertChild( branch( name, tn, true ), NifValue::tNone, at );
		arr->prepareInsert( v.count );
		if ( !v.elems.isEmpty() ) {
			for ( int i = 0; i < v.elems.size(); i++ )
				buildElement( arr, QStringLiteral( "[%1]" ).arg( i ), v.elems[i], m );
		} else {
			for ( int i = 0; i < v.count; i++ )
				buildPlainElement( arr, QStringLiteral( "[%1]" ).arg( i ), v, i );
		}
		return arr;
	}
	}
	return nullptr;
}

// ---------------------------------------------------------------- values <- items

static void putVec4( QByteArray & b, int off, const Vector4 & v )
{
	if ( off + 16 > b.size() ) return;
	const float f[4] = { v[0], v[1], v[2], v[3] };
	std::memcpy( b.data() + off, f, 16 );
}

static void putQuat( QByteArray & b, int off, const Quat & q )
{
	if ( off + 16 > b.size() ) return;
	const float f[4] = { q[1], q[2], q[3], q[0] };	// Quat( w, x, y, z ) -> file x y z w
	std::memcpy( b.data() + off, f, 16 );
}

static void syncRaw( QByteArray & b, int off, Type t, const NifItem * item )
{
	if ( !item ) return;
	switch ( t ) {
	case Type::Vector4: putVec4( b, off, item->get<Vector4>() ); break;
	case Type::Quaternion: putQuat( b, off, item->get<Quat>() ); break;
	case Type::Matrix3: case Type::Rotation:
		for ( int c = 0; c < 3 && c < item->childCount(); c++ ) putVec4( b, off + 16 * c, item->child( c )->get<Vector4>() );
		break;
	case Type::Matrix4: case Type::Transform:
		for ( int c = 0; c < 4 && c < item->childCount(); c++ ) putVec4( b, off + 16 * c, item->child( c )->get<Vector4>() );
		break;
	case Type::QsTransform:
		if ( item->childCount() >= 3 ) {
			putVec4( b, off, item->child( 0 )->get<Vector4>() );
			putQuat( b, off + 16, item->child( 1 )->get<Quat>() );
			putVec4( b, off + 32, item->child( 2 )->get<Vector4>() );
		}
		break;
	default: break;
	}
}

void HkxModel::syncPlainElement( Value & a, int i, const NifItem * item ) const
{
	const Type t = a.type;
	if ( t == Type::Real ) { a.setPlainFloat( i, item->getFloatValue() ); return; }
	if ( isRawTypeForSync( t ) ) { syncRaw( a.bytes, i * plainSize( t ), t, item ); return; }
	a.setPlainInt( i, qint64( item->getCountValue() ) );
}

void HkxModel::syncElement( Value & v, const NifItem * item, const MemberDef * am ) const
{
	Q_UNUSED( am );
	syncValue( v, item );
}

void HkxModel::syncValue( Value & v, const NifItem * item ) const
{
	if ( !item ) return;
	switch ( v.kind ) {
	case Value::Ignored:
		return;
	case Value::Scalar:
		if ( v.ints.size() <= 1 ) {
			if ( v.type == Type::Real ) v.setFloat( item->getFloatValue() );
			else v.setInt( qint64( item->getCountValue() ) );
		} else {
			for ( int i = 0; i < v.ints.size() && i < item->childCount(); i++ ) {
				if ( v.type == Type::Real ) v.setFloat( item->child( i )->getFloatValue(), i );
				else v.setInt( qint64( item->child( i )->getCountValue() ), i );
			}
		}
		return;
	case Value::Raw: {
		const int esz = plainSize( v.type );
		const int n = esz ? v.bytes.size() / esz : 0;
		if ( n <= 1 ) syncRaw( v.bytes, 0, v.type, item );
		else for ( int i = 0; i < n && i < item->childCount(); i++ ) syncRaw( v.bytes, i * esz, v.type, item->child( i ) );
		return;
	}
	case Value::Str: {
		const QString s = item->get<QString>();
		// an empty string keeps its nullness (null vs "" cannot be told apart in a cell)
		if ( s.isEmpty() && v.isNull ) return;
		v.setStringValue( s );
		return;
	}
	case Value::Ptr:
		v.object = item->get<int>();
		if ( v.object < -1 || v.object >= hkfile.objects.size() ) v.object = -1;
		return;
	case Value::Struct:
		if ( v.cls ) {
			// items exist only for serialised members: walk both lists in step, skipping ignored ones
			int row = 0;
			for ( int i = 0; i < v.elems.size(); i++ ) {
				if ( v.elems[i].kind == Value::Ignored ) continue;
				if ( row >= item->childCount() ) break;
				syncValue( v.elems[i], item->child( row++ ) );
			}
		}
		return;
	case Value::CArray:
		for ( int i = 0; i < v.elems.size() && i < item->childCount(); i++ )
			syncValue( v.elems[i], item->child( i ) );
		return;
	case Value::Array: case Value::RelArray: {
		if ( v.type == Type::UInt8 || v.type == Type::Int8 || v.type == Type::Char ) {
			const QByteArray b = item->get<QByteArray>();
			if ( b.size() != v.count ) {
				QString e;
				const_cast<File &>( hkfile ).resizeArray( v, b.size(), &e );
			}
			v.bytes = b;
			return;
		}
		const int n = item->childCount();
		if ( n != v.count ) {
			QString e;
			const_cast<File &>( hkfile ).resizeArray( v, n, &e );
		}
		if ( !v.elems.isEmpty() || v.count == 0 ) {
			for ( int i = 0; i < v.elems.size() && i < n; i++ )
				syncValue( v.elems[i], item->child( i ) );
		} else {
			for ( int i = 0; i < v.count && i < n; i++ )
				syncPlainElement( v, i, item->child( i ) );
		}
		return;
	}
	}
}

void HkxModel::syncFromItems() const
{
	for ( int i = 0; i < hkfile.objects.size() && i < root->childCount(); i++ )
		syncValue( hkfile.objects[i].body, root->child( i ) );
}

// ---------------------------------------------------------------- Qt model

int HkxModel::objectOf( const QModelIndex & index ) const
{
	const NifItem * top = getTopItem( index );
	return top ? top->row() : -1;
}

QString HkxModel::objectClass( int i ) const
{
	return i >= 0 && i < hkfile.objects.size() ? hkfile.objects[i].cls->name : QString();
}

QVariant HkxModel::data( const QModelIndex & index, int role ) const
{
	const NifItem * item = getItem( index );
	if ( !item )
		return QVariant();
	if ( ( role == Qt::DisplayRole || role == NifSkopeDisplayRole ) && index.column() == ValueCol && item->valueType() == NifValue::tLink ) {
		const int o = item->get<int>();
		if ( o < 0 ) return QStringLiteral( "None" );
		return QStringLiteral( "%1 (%2)" ).arg( o ).arg( objectClass( o ) );
	}
	if ( role == Qt::DisplayRole && index.column() == NameCol && isTopItem( item ) )
		return QStringLiteral( "%1 [%2]" ).arg( item->name() ).arg( item->row() );
	return BaseModel::data( index, role );
}

Qt::ItemFlags HkxModel::flags( const QModelIndex & index ) const
{
	Qt::ItemFlags f = BaseModel::flags( index );
	const NifItem * item = getItem( index );
	if ( item && index.column() == ValueCol && item->valueType() == NifValue::tByteArray )
		f &= ~Qt::ItemIsEditable;	// the blob is the animation layer's to edit
	return f;
}

bool HkxModel::setDataDirect( const QModelIndex & index, const QVariant & value )
{
	inCommand = true;
	const bool ok = BaseModel::setData( index, value, Qt::EditRole );
	inCommand = false;
	if ( ok )
		emit dataChanged( index, index );
	return ok;
}

bool HkxModel::setData( const QModelIndex & index, const QVariant & value, int role )
{
	if ( role != Qt::EditRole )
		return false;
	if ( inCommand || !undoStack || index.column() != ValueCol )
		return BaseModel::setData( index, value, role );
	const QVariant old = index.data( Qt::EditRole );
	if ( old == value )
		return true;
	const NifItem * item = getItem( index );
	undoStack->push( new HkxSetValueCommand( this, index, old, value, item ? item->name() : QString() ) );
	return true;
}

bool HkxModel::updateArraySizeImpl( NifItem * array )
{
	Q_UNUSED( array );
	return true;	// sizes are driven by setArraySize, not by a condition expression
}

bool HkxModel::setHeaderString( const QString &, uint )
{
	return false;
}

bool HkxModel::evalVersionImpl( const NifItem * ) const
{
	return true;
}

QString HkxModel::ver2str( quint32 ) const
{
	return getVersion();
}

quint32 HkxModel::str2ver( QString ) const
{
	return getVersionNumber();
}

bool HkxModel::resizeDirect( const QModelIndex & arrayIndex, int newCount )
{
	NifItem * arr = getItem( arrayIndex );
	if ( !arr || !arr->isArray() || newCount < 0 )
		return false;
	// the array's Value: located by walking the item path down the File
	QVector<int> path;
	for ( const NifItem * it = arr; it && it != root; it = it->parent() )
		path.prepend( it->row() );
	if ( path.isEmpty() || path[0] >= hkfile.objects.size() )
		return false;
	Value * v = &hkfile.objects[path[0]].body;
	for ( int d = 1; d < path.size(); d++ ) {
		// children of a struct skip its ignored members
		int row = -1;
		Value * next = nullptr;
		for ( Value & e : v->elems ) {
			if ( v->kind == Value::Struct && e.kind == Value::Ignored ) continue;
			if ( ++row == path[d] ) { next = &e; break; }
		}
		if ( !next ) return false;
		v = next;
	}
	if ( v->kind != Value::Array && v->kind != Value::RelArray )
		return false;
	const int oldCount = arr->childCount();
	if ( newCount == oldCount )
		return true;
	// sync the current elements first, then let the File grow / shrink the Value
	syncValue( *v, arr );
	QString e;
	if ( !hkfile.resizeArray( *v, newCount, &e ) )
		return false;
	const MemberDef * m = v->member;
	if ( newCount > oldCount ) {
		beginInsertRows( arrayIndex, oldCount, newCount - 1 );
		arr->prepareInsert( newCount - oldCount );
		for ( int i = oldCount; i < newCount; i++ ) {
			if ( !v->elems.isEmpty() ) buildElement( arr, QStringLiteral( "[%1]" ).arg( i ), v->elems[i], m );
			else buildPlainElement( arr, QStringLiteral( "[%1]" ).arg( i ), *v, i );
		}
		endInsertRows();
	} else {
		beginRemoveRows( arrayIndex, newCount, oldCount - 1 );
		arr->removeChildren( newCount, oldCount - newCount );
		endRemoveRows();
	}
	emit dataChanged( arrayIndex, arrayIndex );
	return true;
}

bool HkxModel::setArraySize( const QModelIndex & arrayIndex, int newCount )
{
	NifItem * arr = getItem( arrayIndex );
	if ( !arr || !arr->isArray() || newCount < 0 )
		return false;
	if ( !undoStack )
		return resizeDirect( arrayIndex, newCount );
	if ( arr->childCount() == newCount )
		return true;
	undoStack->push( new HkxArrayResizeCommand( this, arrayIndex, arr->childCount(), newCount ) );
	return true;
}

int HkxModel::addObject( const QString & className, QString * error )
{
	if ( !db ) {
		if ( error ) *error = lastError;
		return -1;
	}
	const ClassDef * cls = db->find( className );
	if ( !cls ) {
		if ( error ) *error = QStringLiteral( "class %1 is not in the class database" ).arg( className );
		return -1;
	}
	syncFromItems();
	const int row = hkfile.objects.size();
	beginInsertRows( QModelIndex(), row, row );
	const int i = hkfile.addObject( cls );
	buildValue( root, cls->name, hkfile.objects[i].body, nullptr );
	endInsertRows();
	return i;
}
