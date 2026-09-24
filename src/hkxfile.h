/* Generic Havok 2014 binary packfile object model for Fallout 4 .hkx files
   (lane HKXEDIT1, 2026-09-10): any packfile the game's own class registry
   describes -- animation clips, skeletons, ragdolls, behaviour graphs -- read
   into a graph of typed objects whose fields are driven by the hkClass
   reflection extracted from the 1.10.155 exe (res/hkclasses_fo4.json, made by
   tools/hkclassdb_extract.py), and written back BYTE-IDENTICALLY when nothing
   was changed.

   The contract is docs/HKX_PACKFILE_MODEL.md. There is no per-class code
   here: every object is walked from its class's member list, and the only
   types that get their own branch are the container types Havok itself
   special-cases (hkArray, hkRelArray, hkStringPtr / char*, object pointers,
   enums and flags, inline structs and C arrays). The animation clip's spline
   blob stays an hkArray<hkUint8> of bytes; src/hkxanim.cpp decodes it as a
   VIEW when the Animation workspace needs frames (HkxModel::animFile()).

   Deliberately QtCore-only, so the standalone gate binary
   (tests/hkxfile_gate.cpp) links it without the rest of NifSkope. */
#ifndef HKXFILE_H
#define HKXFILE_H

#include <QByteArray>
#include <QHash>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Hkx {

//! hkClassMember::Type, the numbers the exe's member arrays store.
enum class Type : quint8
{
	Void = 0, Bool, Char, Int8, UInt8, Int16, UInt16, Int32, UInt32, Int64, UInt64,
	Real, Vector4, Quaternion, Matrix3, Rotation, QsTransform, Matrix4, Transform,
	Zero, Pointer, FunctionPointer, Array, InplaceArray, Enum, Struct, SimpleArray,
	HomogeneousArray, Variant, CString, ULong, Flags, Half, StringPtr, RelArray,
	Count
};

const char * typeName( Type t );
Type typeFromName( const QString & name, bool * ok = nullptr );
//! Byte width of one plain value of type t on the 64-bit layout (0 for the
//! types whose width is the class's or the subtype's: Struct, Enum, Flags).
int plainSize( Type t );

//! hkClassMember::FlagValues
enum MemberFlag : quint16
{
	PointerOptional = 1, PointerVoidStar = 2, Enum8 = 8, Enum16 = 16, Enum32 = 32,
	ArrayRawData = 64, Align8 = 128, Align16 = 256, NotOwned = 512,
	SerializeIgnored = 1024, Align32 = 2048
};

struct ClassDef;

struct EnumDef
{
	QString name;
	QVector<QPair<int, QString>> items;
	QString nameOf( qint64 value ) const;
	bool valueOf( const QString & item, qint64 * value ) const;
};

struct MemberDef
{
	QString name;
	Type type = Type::Void;
	Type subtype = Type::Void;
	quint16 cArraySize = 0;
	quint16 flags = 0;
	quint16 offset = 0;
	QString className;      //!< the struct class of an inline struct / struct array / pointer target
	QString enumName;       //!< the enum of an Enum / Flags member
	const ClassDef * cls = nullptr;
	const EnumDef * en = nullptr;
	bool ignored() const { return ( flags & SerializeIgnored ) != 0; }
	//! how many bytes the member occupies inline in its struct
	int inlineSize() const;
	//! the storage width of an Enum / Flags member (its subtype)
	int enumSize() const;
	//! the element size of an Array / RelArray member (its subtype, or the class)
	int elementSize() const;
};

struct ClassDef
{
	QString name;
	QString parentName;
	const ClassDef * parent = nullptr;
	int objectSize = 0;
	int version = 0;
	quint32 signature = 0;
	quint32 flags = 0;
	int registryIndex = -1;
	QVector<EnumDef> enums;
	QVector<MemberDef> members;          //!< declared by this class only
	QVector<const MemberDef *> allMembers; //!< parent chain first, then own (filled by ClassDb)
	bool inherits( const QString & ancestor ) const;
};

/*! The class database: res/hkclasses_fo4.json.
 *
 *  Found beside the executable (the .pro copies it next to nif.xml), or at
 *  WW_HKCLASSDB, or given explicitly. One instance per process. */
class ClassDb
{
public:
	//! Load from a JSON file; on failure returns null and fills `error`.
	static ClassDb * load( const QString & jsonPath, QString * error );
	//! Load from JSON bytes.
	static ClassDb * loadJson( const QByteArray & json, QString * error );
	//! The process-wide database, loaded on first use from the default
	//! locations; null (with the reason in `error`) when none can be read.
	static const ClassDb * instance( QString * error = nullptr );
	//! Default search list, in order.
	static QStringList defaultPaths();

	const ClassDef * find( const QString & name ) const;
	int count() const { return classes.size(); }
	QStringList names() const;
	//! provenance stamps out of the JSON (exe sha, extraction date, cross-check)
	QString sourceSummary() const { return summary; }

private:
	ClassDb() {}
	QHash<QString, ClassDef *> classes;
	QVector<ClassDef *> owned;
	QString summary;
	friend struct ClassDbDeleter;
public:
	~ClassDb();
};

/*! One field value. The kinds mirror what a packfile can hold:
 *   Scalar   ints, bools, chars, enums, flags, halves: `ints` (one per
 *            C-array element); reals keep their exact 32-bit pattern in
 *            `ints` too (a NaN payload survives a round trip)
 *   Raw      the vector types (Vector4 .. Transform) as bytes
 *   Str      hkStringPtr / char*: `bytes` (no terminator); `isNull` = no fixup
 *   Ptr      an object pointer: `object` (index into File::objects, -1 null)
 *   Struct   an inline struct: `elems` = one Value per member of `cls`
 *            (parents first), `bytes` = the struct's HOLE bytes (every byte
 *            no serialised member covers: vtable slot, refcount, padding),
 *            written back verbatim so an unmodified file is reproduced
 *   Array    hkArray<T>: `count`, `capacity` (the raw capacityAndFlags),
 *            `hasPayload` (a null data pointer with count 0 is legal and
 *            distinct from an empty payload); elements in `elems` for
 *            Struct / Pointer / string subtypes, or packed in `bytes` for
 *            plain subtypes (count * elementSize)
 *   RelArray hkRelArray<T>: like Array but stored inside the object's chunk
 *   CArray   a member with cArraySize > 1 of Struct / Pointer / string type
 *            (plain C arrays are one Scalar / Raw with several entries)
 *   Ignored  a SERIALIZE_IGNORED member: its bytes, kept verbatim */
struct Value
{
	enum Kind : quint8 { Ignored, Scalar, Raw, Str, Ptr, Struct, Array, RelArray, CArray };
	Kind kind = Ignored;
	Type type = Type::Void;          //!< Scalar/Raw: the type; Array/RelArray: the element type; Enum/Flags keep type=Enum/Flags and the storage in `storage`
	Type storage = Type::Void;       //!< Enum / Flags storage width type
	const MemberDef * member = nullptr; //!< the member this value fills (null for array elements)
	const ClassDef * cls = nullptr;  //!< Struct: its class; Array/RelArray of structs: the element class
	QVector<qint64> ints;
	QByteArray bytes;
	bool isNull = false;
	int object = -1;
	int count = 0;
	quint32 capacity = 0;
	bool hasPayload = false;
	QVector<Value> elems;

	// convenience
	qint64 toInt( int i = 0 ) const { return i < ints.size() ? ints[i] : 0; }
	float toFloat( int i = 0 ) const;
	void setInt( qint64 v, int i = 0 );
	void setFloat( float f, int i = 0 );
	QString toStringValue() const;         //!< Str: latin-1; null -> ""
	void setStringValue( const QString & s ); //!< makes the string non-null
	bool isPlainArray() const { return ( kind == Array || kind == RelArray ) && elems.isEmpty() && count > 0 && !bytes.isEmpty(); }
	//! for plain arrays: the i-th element as an integer / float
	qint64 plainInt( int i ) const;
	float plainFloat( int i ) const;
	void setPlainInt( int i, qint64 v );
	void setPlainFloat( int i, float f );
};

struct Object
{
	const ClassDef * cls = nullptr;
	Value body;          //!< kind Struct
	int fileOffset = -1; //!< within __data__ as read (-1 for a new object)
};

/*! A whole packfile. read() and write() are exact inverses on every FO4 file
 *  the census covers (docs/HKX_PACKFILE_MODEL.md section 6 has the numbers). */
class File
{
public:
	QString error;             //!< empty on success, else a sentence naming field and value
	QByteArray head;           //!< bytes 0 .. section headers (userTag, version, layout, predicate array), kept verbatim
	QVector<QPair<quint32, QString>> classNames; //!< __classnames__ in file order (signature, name)
	QVector<Object> objects;   //!< in file (virtual fixup) order
	int root = -1;             //!< the contents object
	int dataStart = 0;         //!< absolute offset of __data__ as read
	int payloadLength = 0;     //!< __data__ payload length as read

	bool ok() const { return error.isEmpty(); }
	//! Parse. Every refusal is a sentence; the file is left empty on refusal.
	bool read( const QByteArray & blob, const ClassDb & db );
	//! Serialise with the canonical layout.
	QByteArray write( const ClassDb & db ) const;
	//! read() then write() and compare: returns -1 when identical, else the first differing offset.
	static qint64 roundTripDiff( const QByteArray & blob, const ClassDb & db, QString * error, QByteArray * out = nullptr );

	// Editing
	//! Find a value by path: "#2.numFrames", "#2.annotationTracks[0].annotations[1].text".
	Value * find( const QString & path, QString * error = nullptr );
	const Value * find( const QString & path, QString * error = nullptr ) const;
	//! Resize an Array / RelArray value (new elements default-constructed for the member).
	bool resizeArray( Value & array, int newCount, QString * error = nullptr );
	//! Append a new object of class `cls`; returns its index. Its fields are default (zero / null / empty).
	int addObject( const ClassDef * cls );
	//! Build a default value for a member (used by resizeArray and addObject).
	static Value defaultValue( const MemberDef & m );
	static Value defaultElement( const MemberDef & arrayMember );
	static Value defaultStruct( const ClassDef * cls );

	//! One line per object: "#i class @offset".
	QStringList objectSummary() const;
	//! Names of every class used by an object, in first-use order.
	QStringList classesUsed() const;
	//! The chunk list as read: (absolute offset, size, kind, label), for the layout study.
	struct Chunk { int offset; int size; QString kind; QString label; };
	QVector<Chunk> chunks;
};

//! Describe a value in one line (for dumps and tests).
QString describe( const Value & v, const File * file = nullptr );
//! Text dump of a whole file, one object per paragraph.
QString dump( const File & file );

} // namespace Hkx

#endif // HKXFILE_H
