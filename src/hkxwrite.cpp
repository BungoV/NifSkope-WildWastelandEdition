/* HkxAnimClip -> FO4 .hkx.  Lane HKX5, 2026-09-10.
   The byte contract is docs/HKX_WRITE_FORMAT.md. */

#include "hkxwrite.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QStringList>
#include <QTemporaryDir>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

// ---------------------------------------------------------------- validation
//! Everything both routes must agree on before either writes a byte.
QString validate( const HkxAnimClip & c )
{
	if ( c.numTracks <= 0 )
		return QStringLiteral( "the clip has %1 transform tracks" ).arg( c.numTracks );
	if ( c.numTracks > 65535 )
		return QStringLiteral( "the clip has %1 transform tracks; a track index is an hkInt16" ).arg( c.numTracks );
	if ( c.numFrames < 2 )
		return QStringLiteral( "the clip has %1 frames; Havok stores a one-frame pose as two" ).arg( c.numFrames );
	if ( c.frames.size() != c.numFrames )
		return QStringLiteral( "the clip says %1 frames and carries %2" ).arg( c.numFrames ).arg( c.frames.size() );
	for ( int f = 0; f < c.numFrames; f++ )
		if ( c.frames[f].size() != c.numTracks )
			return QStringLiteral( "frame %1 has %2 transforms, the clip has %3 tracks" )
				.arg( f ).arg( c.frames[f].size() ).arg( c.numTracks );
	if ( c.trackToBone.size() != c.numTracks )
		return QStringLiteral( "the binding maps %1 tracks, the clip has %2" )
			.arg( c.trackToBone.size() ).arg( c.numTracks );
	for ( int t = 0; t < c.numTracks; t++ )
		if ( c.trackToBone[t] < 0 || c.trackToBone[t] > 32767 )
			return QStringLiteral( "track %1 maps to bone %2, which is not an hkInt16 bone index" ).arg( t ).arg( c.trackToBone[t] );
	if ( !( c.frameDuration > 0.0f ) || !std::isfinite( c.frameDuration ) )
		return QStringLiteral( "frameDuration is %1 seconds" ).arg( double( c.frameDuration ) );
	const double want = double( c.numFrames - 1 ) * double( c.frameDuration );
	if ( std::fabs( want - double( c.duration ) ) > 1e-3 )
		return QStringLiteral( "duration is %1 s but %2 frames at %3 s is %4 s" )
			.arg( double( c.duration ) ).arg( c.numFrames ).arg( double( c.frameDuration ) ).arg( want );
	if ( !c.rootMotion.isEmpty() && c.rootMotion.size() != c.numFrames )
		return QStringLiteral( "the clip carries %1 root-motion samples for %2 frames" )
			.arg( c.rootMotion.size() ).arg( c.numFrames );
	if ( c.numFloatTracks )
		return QStringLiteral( "the clip has %1 float tracks; this writer writes transform tracks only" ).arg( c.numFloatTracks );
	if ( !c.annotations.isEmpty() && c.annotations.size() != c.numTracks )
		return QStringLiteral( "the clip carries annotations for %1 tracks and has %2" )
			.arg( c.annotations.size() ).arg( c.numTracks );
	for ( int f = 0; f < c.numFrames; f++ ) {
		for ( int t = 0; t < c.numTracks; t++ ) {
			const HkxTransform & h = c.frames[f][t];
			for ( int k = 0; k < 3; k++ )
				if ( !std::isfinite( h.translation[k] ) || !std::isfinite( h.scale[k] ) )
					return QStringLiteral( "frame %1 track %2 has a non-finite translation or scale" ).arg( f ).arg( t );
			const double l = std::sqrt( double( h.rotation[0] ) * h.rotation[0] + double( h.rotation[1] ) * h.rotation[1]
									  + double( h.rotation[2] ) * h.rotation[2] + double( h.rotation[3] ) * h.rotation[3] );
			if ( !std::isfinite( l ) || std::fabs( l - 1.0 ) > 1e-3 )
				return QStringLiteral( "frame %1 track %2 has a rotation of length %3" ).arg( f ).arg( t ).arg( l );
		}
	}
	return QString();
}

int blendHintValue( const QString & h )
{
	if ( !h.compare( QLatin1String( "ADDITIVE_DEPRECATED" ), Qt::CaseInsensitive ) ) return 1;
	if ( !h.compare( QLatin1String( "ADDITIVE" ), Qt::CaseInsensitive ) ) return 2;
	return 0;
}

const char * blendHintName( const QString & h )
{
	switch ( blendHintValue( h ) ) {
	case 1: return "ADDITIVE_DEPRECATED";
	case 2: return "ADDITIVE";
	default: return "NORMAL";
	}
}

bool wantsRootMotion( const HkxAnimClip & c, const HkxWriteOptions & o )
{
	return o.writeRootMotion && !c.rootMotion.isEmpty();
}

// ------------------------------------------------------------------- route B
/*! The __data__ payload builder.
 *
 *  A Havok binary packfile is a memory image plus three fixup tables: a
 *  pointer in the image is written as zero and named by a fixup that says
 *  where it points (local = inside this section, global = another object).
 *  So the builder appends bytes and records fixups against the offsets it
 *  hands out; nothing is patched afterwards.
 */
struct Payload
{
	QByteArray b;
	QVector<QPair<qint32, qint32>> local;                //!< (src, dst)
	QVector<QPair<qint32, qint32>> global;               //!< (src, dst), section 2
	QVector<QPair<qint32, qint32>> virt;                 //!< (objOffset, classNameOffset)

	qint32 size() const { return qint32( b.size() ); }
	void align( int a = 16 )
	{
		while ( b.size() % a )
			b.append( char( 0 ) );
	}
	void zero( int n ) { b.append( QByteArray( n, char( 0 ) ) ); }
	void i32( qint32 v ) { b.append( reinterpret_cast<const char *>( &v ), 4 ); }
	void u32( quint32 v ) { b.append( reinterpret_cast<const char *>( &v ), 4 ); }
	void i16( qint16 v ) { b.append( reinterpret_cast<const char *>( &v ), 2 ); }
	void f32( float v ) { b.append( reinterpret_cast<const char *>( &v ), 4 ); }
	//! An hkArray<T>: `T* data` (a local fixup when the array is not empty),
	//! `int size`, `int capacityAndFlags` (size | 0x80000000, do-not-free).
	void array( qint32 payloadOffset, int n )
	{
		if ( n > 0 )
			local.append( qMakePair( size(), payloadOffset ) );
		zero( 8 );
		i32( n );
		u32( quint32( n ) | 0x80000000u );
	}
	//! An hkStringPtr: a local fixup to a NUL-terminated string, or null.
	void strptr( qint32 stringOffset, bool present )
	{
		if ( present )
			local.append( qMakePair( size(), stringOffset ) );
		zero( 8 );
	}
	void ptr( qint32 target, bool globalFixup )
	{
		if ( target >= 0 ) {
			if ( globalFixup )
				global.append( qMakePair( size(), target ) );
			else
				local.append( qMakePair( size(), target ) );
		}
		zero( 8 );
	}
	//! Reserve an aligned run of `n` bytes and return where it starts.
	qint32 reserve( int n, int a = 16 )
	{
		align( a );
		const qint32 at = size();
		zero( n );
		return at;
	}
	qint32 putString( const QString & s )
	{
		align( 16 );
		const qint32 at = size();
		b.append( s.toLatin1() );
		b.append( char( 0 ) );
		align( 16 );
		return at;
	}
};

//! `tag`, then the seven int32 offsets, then 16 bytes of 0xFF -- 0x40 bytes.
void sectionHeader( QByteArray & out, const char * tag, qint32 absStart,
					qint32 localOff, qint32 globalOff, qint32 virtOff, qint32 endOff )
{
	QByteArray t( 20, char( 0 ) );
	const int n = int( strlen( tag ) );
	memcpy( t.data(), tag, size_t( n ) );
	t[19] = char( 0xFF );
	out.append( t );
	const qint32 v[7] = { absStart, localOff, globalOff, virtOff, endOff, endOff, endOff };
	out.append( reinterpret_cast<const char *>( v ), 28 );
	out.append( QByteArray( 16, char( 0xFF ) ) );
}

struct ClassRow
{
	quint32 sig;
	const char * name;
};

} // namespace

QString HkxWriteReport::summary() const
{
	if ( !error.isEmpty() )
		return QStringLiteral( "refused: " ) + error;
	return QStringLiteral( "%1: %2 objects, %3 tracks x %4 frames = %5 transform bytes, %6 bytes on disk" )
		.arg( routeUsed ).arg( objects ).arg( tracks ).arg( frames ).arg( transformBytes ).arg( fileBytes );
}

// ---------------------------------------------------------------------------

QByteArray hkxWritePackfile( const HkxAnimClip & clip, const HkxWriteOptions & opt, QString & error )
{
	error = validate( clip );
	if ( !error.isEmpty() )
		return QByteArray();

	const bool rm = wantsRootMotion( clip, opt );
	const bool res = opt.writeResourceContainer;
	const QString skel = opt.skeletonName.isEmpty() ? clip.originalSkeletonName : opt.skeletonName;
	const int nT = clip.numTracks, nF = clip.numFrames;

	// __classnames__ : the four meta classes every Havok packfile carries,
	// then exactly the classes this file uses, in the order HKXPACK emits.
	QVector<ClassRow> classes;
	classes.append( { 0x33d42383u, "hkClass" } );
	classes.append( { 0xb0efa719u, "hkClassMember" } );
	classes.append( { 0x8a3609cfu, "hkClassEnum" } );
	classes.append( { 0xce6f8a6cu, "hkClassEnumItem" } );
	classes.append( { 0x2772c11eu, "hkRootLevelContainer" } );
	classes.append( { 0x26859f4cu, "hkaAnimationContainer" } );
	classes.append( { 0xa5eff3f2u, "hkaInterleavedUncompressedAnimation" } );
	if ( rm )
		classes.append( { 0x60f8e0b8u, "hkaDefaultAnimatedReferenceFrame" } );
	classes.append( { 0x0faf9150u, "hkaAnimationBinding" } );
	if ( res )
		classes.append( { 0x1de13a73u, "hkMemoryResourceContainer" } );

	QByteArray cn;
	QHash<QByteArray, qint32> classOff;
	for ( const ClassRow & c : classes ) {
		const quint32 sig = c.sig;
		cn.append( reinterpret_cast<const char *>( &sig ), 4 );
		cn.append( char( 0x09 ) );
		classOff.insert( QByteArray( c.name ), qint32( cn.size() ) );
		cn.append( c.name );
		cn.append( char( 0 ) );
	}
	while ( cn.size() % 16 )
		cn.append( char( 0xFF ) );
	const qint32 rootClassNameOffset = classOff.value( "hkRootLevelContainer" );

	// ---- __data__ ---------------------------------------------------------
	Payload p;
	const int nVariants = res ? 2 : 1;

	// 1. hkRootLevelContainer: one hkArray, then the NamedVariant array (0x18
	//    each: name, className, variant) and the four strings after it.
	const qint32 oRoot = 0;
	const qint32 vArr = 0x10;                       // right after the 0x10 body
	const qint32 sMerged = vArr + 0x18 * nVariants; // strings follow, 16-aligned
	p.virt.append( qMakePair( oRoot, rootClassNameOffset ) );
	p.array( vArr, nVariants );
	// the NamedVariant entries are filled once the strings exist; reserve them
	Q_ASSERT( p.size() == vArr );
	p.zero( 0x18 * nVariants );
	Q_ASSERT( p.size() == sMerged );

	const qint32 strMerged = p.putString( QStringLiteral( "Merged Animation Container" ) );
	const qint32 strAnimCont = p.putString( QStringLiteral( "hkaAnimationContainer" ) );
	qint32 strResData = -1, strResCont = -1;
	if ( res ) {
		strResData = p.putString( QStringLiteral( "Resource Data" ) );
		strResCont = p.putString( QStringLiteral( "hkMemoryResourceContainer" ) );
	}

	// 2. hkaAnimationContainer (body 0x60), then its two one-pointer arrays
	const qint32 oCont = p.reserve( 0x60 );
	p.virt.append( qMakePair( oCont, classOff.value( "hkaAnimationContainer" ) ) );
	const qint32 pAnims = p.reserve( 8 );
	const qint32 pBinds = p.reserve( 8 );

	// 3. hkaInterleavedUncompressedAnimation (body 0x58 -> 0x60 aligned),
	//    then the annotation tracks and the transforms.
	const qint32 oAnim = p.reserve( 0x60 );
	p.virt.append( qMakePair( oAnim, classOff.value( "hkaInterleavedUncompressedAnimation" ) ) );
	const qint32 pAnnTracks = p.reserve( 0x18 * nT );
	// annotation payloads (only the tracks that have any)
	QVector<qint32> annPayload( nT, -1 );
	QVector<QVector<qint32>> annText( nT );
	if ( opt.writeAnnotations && !clip.annotations.isEmpty() ) {
		for ( int t = 0; t < nT; t++ ) {
			const int n = int( clip.annotations[t].size() );
			if ( !n )
				continue;
			annPayload[t] = p.reserve( 16 * n );
			annText[t].resize( n );
			for ( int a = 0; a < n; a++ )
				annText[t][a] = clip.annotations[t][a].text.isEmpty() ? -1 : p.putString( clip.annotations[t][a].text );
		}
	}
	const qint32 pTransforms = p.reserve( 48 * nT * nF );

	// 4. hkaDefaultAnimatedReferenceFrame (body 0x58 -> 0x60), then its samples
	qint32 oMotion = -1, pSamples = -1;
	if ( rm ) {
		oMotion = p.reserve( 0x60 );
		p.virt.append( qMakePair( oMotion, classOff.value( "hkaDefaultAnimatedReferenceFrame" ) ) );
		pSamples = p.reserve( 16 * nF );
	}

	// 5. hkaAnimationBinding (body 0x58 -> 0x60), the skeleton name, the indices
	const qint32 oBind = p.reserve( 0x60 );
	p.virt.append( qMakePair( oBind, classOff.value( "hkaAnimationBinding" ) ) );
	const qint32 strSkel = skel.isEmpty() ? -1 : p.putString( skel );
	const qint32 pIndices = p.reserve( 2 * nT );

	// 6. hkMemoryResourceContainer (body 0x40)
	qint32 oRes = -1;
	if ( res ) {
		oRes = p.reserve( 0x40 );
		p.virt.append( qMakePair( oRes, classOff.value( "hkMemoryResourceContainer" ) ) );
	}
	p.align( 16 );

	// ---- now fill the reserved bodies, recording their fixups -------------
	auto at = [&p]( qint32 off ) -> char * { return p.b.data() + off; };
	auto putI32 = [&]( qint32 off, qint32 v ) { memcpy( at( off ), &v, 4 ); };
	auto putU32 = [&]( qint32 off, quint32 v ) { memcpy( at( off ), &v, 4 ); };
	auto putF32 = [&]( qint32 off, float v ) { memcpy( at( off ), &v, 4 ); };
	auto putArr = [&]( qint32 off, qint32 payload, int n ) {
		if ( n > 0 )
			p.local.append( qMakePair( off, payload ) );
		putI32( off + 8, n );
		putU32( off + 12, quint32( n ) | 0x80000000u );
	};
	auto putStr = [&]( qint32 off, qint32 s ) {
		if ( s >= 0 )
			p.local.append( qMakePair( off, s ) );
	};
	auto putGlobal = [&]( qint32 off, qint32 target ) {
		if ( target >= 0 )
			p.global.append( qMakePair( off, target ) );
	};

	// NamedVariant 0 = the animation container
	putStr( vArr + 0x00, strMerged );
	putStr( vArr + 0x08, strAnimCont );
	putGlobal( vArr + 0x10, oCont );
	if ( res ) {
		putStr( vArr + 0x18, strResData );
		putStr( vArr + 0x20, strResCont );
		putGlobal( vArr + 0x28, oRes );
	}

	// hkaAnimationContainer
	putArr( oCont + 0x10, -1, 0 );                 // skeletons
	putArr( oCont + 0x20, pAnims, 1 );             // animations
	putArr( oCont + 0x30, pBinds, 1 );             // bindings
	putArr( oCont + 0x40, -1, 0 );                 // attachments
	putArr( oCont + 0x50, -1, 0 );                 // skins
	putGlobal( pAnims, oAnim );
	putGlobal( pBinds, oBind );

	// hkaInterleavedUncompressedAnimation
	putI32( oAnim + 0x10, 1 );                     // HK_INTERLEAVED_ANIMATION
	putF32( oAnim + 0x14, clip.duration );
	putI32( oAnim + 0x18, nT );
	putI32( oAnim + 0x1c, 0 );                     // numberOfFloatTracks
	putGlobal( oAnim + 0x20, oMotion );            // extractedMotion (null when absent)
	putArr( oAnim + 0x28, pAnnTracks, nT );
	putArr( oAnim + 0x38, pTransforms, nT * nF );
	putArr( oAnim + 0x48, -1, 0 );                 // floats

	for ( int t = 0; t < nT; t++ ) {
		const qint32 e = pAnnTracks + 0x18 * t;
		putStr( e + 0x00, -1 );                    // trackName: empty on FO4
		const int n = ( annPayload[t] >= 0 ) ? int( clip.annotations[t].size() ) : 0;
		putArr( e + 0x08, annPayload[t], n );
		for ( int a = 0; a < n; a++ ) {
			putF32( annPayload[t] + 16 * a, clip.annotations[t][a].time );
			putStr( annPayload[t] + 16 * a + 8, annText[t][a] );
		}
	}

	// the transforms: hkQsTransform = translation(xyzw) rotation(xyzw)
	// scale(xyzw); the two w's are padding and HKXPACK writes 0 in both.
	// FRAME MAJOR: transforms[frame * numberOfTransformTracks + track].
	for ( int f = 0; f < nF; f++ ) {
		for ( int t = 0; t < nT; t++ ) {
			const HkxTransform & h = clip.frames[f][t];
			const qint32 o = pTransforms + 48 * ( f * nT + t );
			putF32( o + 0, h.translation[0] );
			putF32( o + 4, h.translation[1] );
			putF32( o + 8, h.translation[2] );
			putF32( o + 12, 0.0f );
			// HkxTransform::rotation is NifSkope's (w,x,y,z); the file is Havok's (x,y,z,w)
			putF32( o + 16, h.rotation[1] );
			putF32( o + 20, h.rotation[2] );
			putF32( o + 24, h.rotation[3] );
			putF32( o + 28, h.rotation[0] );
			putF32( o + 32, h.scale[0] );
			putF32( o + 36, h.scale[1] );
			putF32( o + 40, h.scale[2] );
			putF32( o + 44, 0.0f );
		}
	}

	// hkaDefaultAnimatedReferenceFrame
	if ( rm ) {
		for ( int k = 0; k < 3; k++ ) {
			putF32( oMotion + 0x20 + 4 * k, clip.rootMotionUp[k] );
			putF32( oMotion + 0x30 + 4 * k, clip.rootMotionForward[k] );
		}
		putF32( oMotion + 0x40, clip.duration );
		putArr( oMotion + 0x48, pSamples, nF );
		for ( int f = 0; f < nF; f++ ) {
			const HkxRootMotion & s = clip.rootMotion[f];
			putF32( pSamples + 16 * f + 0, s.translation[0] );
			putF32( pSamples + 16 * f + 4, s.translation[1] );
			putF32( pSamples + 16 * f + 8, s.translation[2] );
			putF32( pSamples + 16 * f + 12, s.yaw );
		}
	}

	// hkaAnimationBinding
	putStr( oBind + 0x10, strSkel );
	putGlobal( oBind + 0x18, oAnim );
	putArr( oBind + 0x20, pIndices, nT );
	putArr( oBind + 0x30, -1, 0 );                 // floatTrackToFloatSlotIndices
	putArr( oBind + 0x40, -1, 0 );                 // partitionIndices
	p.b[oBind + 0x50] = char( blendHintValue( clip.blendHint ) );
	for ( int t = 0; t < nT; t++ ) {
		const qint16 v = qint16( clip.trackToBone[t] );
		memcpy( at( pIndices + 2 * t ), &v, 2 );
	}

	// hkMemoryResourceContainer
	if ( res ) {
		putStr( oRes + 0x10, -1 );                 // name
		putArr( oRes + 0x20, -1, 0 );              // resourceHandles
		putArr( oRes + 0x30, -1, 0 );              // children
	}

	// ---- the fixup tables --------------------------------------------------
	std::sort( p.local.begin(), p.local.end() );
	std::sort( p.global.begin(), p.global.end() );
	std::sort( p.virt.begin(), p.virt.end() );

	QByteArray tables;
	const qint32 payloadEnd = p.size();
	for ( const auto & f : p.local ) {
		tables.append( reinterpret_cast<const char *>( &f.first ), 4 );
		tables.append( reinterpret_cast<const char *>( &f.second ), 4 );
	}
	while ( tables.size() % 16 )
		tables.append( char( 0xFF ) );
	const qint32 globalOff = payloadEnd + qint32( tables.size() );
	for ( const auto & f : p.global ) {
		const qint32 sec = 2;
		tables.append( reinterpret_cast<const char *>( &f.first ), 4 );
		tables.append( reinterpret_cast<const char *>( &sec ), 4 );
		tables.append( reinterpret_cast<const char *>( &f.second ), 4 );
	}
	while ( tables.size() % 16 )
		tables.append( char( 0xFF ) );
	const qint32 virtOff = payloadEnd + qint32( tables.size() );
	for ( const auto & f : p.virt ) {
		const qint32 sec = 0;
		tables.append( reinterpret_cast<const char *>( &f.first ), 4 );
		tables.append( reinterpret_cast<const char *>( &sec ), 4 );
		tables.append( reinterpret_cast<const char *>( &f.second ), 4 );
	}
	while ( tables.size() % 16 )
		tables.append( char( 0xFF ) );
	const qint32 dataEnd = payloadEnd + qint32( tables.size() );

	// ---- the header --------------------------------------------------------
	QByteArray out;
	out.append( "\x57\xE0\xE0\x57\x10\xC0\xC0\x10", 8 );
	const qint32 zero = 0, eleven = 11, three = 3, two = 2, zeroI = 0, flags = 0;
	out.append( reinterpret_cast<const char *>( &zero ), 4 );          // userTag
	out.append( reinterpret_cast<const char *>( &eleven ), 4 );        // fileVersion
	out.append( "\x08\x01\x00\x01", 4 );                               // layout
	out.append( reinterpret_cast<const char *>( &three ), 4 );         // numSections
	out.append( reinterpret_cast<const char *>( &two ), 4 );           // contentsSectionIndex
	out.append( reinterpret_cast<const char *>( &zeroI ), 4 );         // contentsSectionOffset
	out.append( reinterpret_cast<const char *>( &zeroI ), 4 );         // contentsClassNameSectionIndex
	out.append( reinterpret_cast<const char *>( &rootClassNameOffset ), 4 );
	{
		QByteArray v( 16, char( 0 ) );
		memcpy( v.data(), "hk_2014.1.0-r1", 14 );
		v[15] = char( 0xFF );
		out.append( v );
	}
	out.append( reinterpret_cast<const char *>( &flags ), 4 );
	const quint16 maxpred = 0x15, predpad = 0x10;
	out.append( reinterpret_cast<const char *>( &maxpred ), 2 );
	out.append( reinterpret_cast<const char *>( &predpad ), 2 );
	{	// the predicate array: 0x14, then twelve zero bytes (measured on
		// interleaved.hkx AND on the shipped jog.hkx -- identical)
		QByteArray pred( 16, char( 0 ) );
		pred[0] = char( 0x14 );
		out.append( pred );
	}
	Q_ASSERT( out.size() == 0x50 );

	const qint32 cnStart = 0x50 + 3 * 0x40;
	const qint32 dtStart = cnStart + qint32( cn.size() );
	sectionHeader( out, "__classnames__", cnStart, qint32( cn.size() ), qint32( cn.size() ),
				   qint32( cn.size() ), qint32( cn.size() ) );
	sectionHeader( out, "__types__", dtStart, 0, 0, 0, 0 );
	sectionHeader( out, "__data__", dtStart, payloadEnd, globalOff, virtOff, dataEnd );
	out.append( cn );
	out.append( p.b );
	out.append( tables );
	return out;
}

// ------------------------------------------------------------------- route A

QByteArray hkxWriteXml( const HkxAnimClip & clip, const HkxWriteOptions & opt, QString & error )
{
	error = validate( clip );
	if ( !error.isEmpty() )
		return QByteArray();

	const bool rm = wantsRootMotion( clip, opt );
	const bool res = opt.writeResourceContainer;
	const QString skel = opt.skeletonName.isEmpty() ? clip.originalSkeletonName : opt.skeletonName;
	const int nT = clip.numTracks, nF = clip.numFrames;

	// object names: HKXPACK only needs them unique and referenced consistently
	const QString idRoot = QStringLiteral( "#0010" ), idCont = QStringLiteral( "#0011" ),
				  idAnim = QStringLiteral( "#0012" ), idMotion = QStringLiteral( "#0013" ),
				  idBind = QStringLiteral( "#0014" ), idRes = QStringLiteral( "#0015" );

	auto f9 = []( double v ) { return QString::number( v, 'f', 9 ); };
	auto esc = []( const QString & s ) {
		QString o = s;
		o.replace( QLatin1Char( '&' ), QLatin1String( "&amp;" ) );
		o.replace( QLatin1Char( '<' ), QLatin1String( "&lt;" ) );
		o.replace( QLatin1Char( '>' ), QLatin1String( "&gt;" ) );
		return o;
	};

	QString x;
	x += QLatin1String( "<?xml version=\"1.0\" encoding=\"ASCII\" standalone=\"no\"?>\n" );
	x += QLatin1String( "<hkpackfile classversion=\"11\" contentsversion=\"hk_2014.1.0-r1\">\n" );
	x += QLatin1String( "    <hksection name=\"__data__\">\n" );

	x += QStringLiteral( "        <hkobject class=\"hkRootLevelContainer\" name=\"%1\" signature=\"0x2772c11e\">\n" ).arg( idRoot );
	x += QStringLiteral( "            <hkparam name=\"namedVariants\" numelements=\"%1\">\n" ).arg( res ? 2 : 1 );
	x += QLatin1String( "                <hkobject>\n"
						"                    <hkparam name=\"name\">Merged Animation Container</hkparam>\n"
						"                    <hkparam name=\"className\">hkaAnimationContainer</hkparam>\n" );
	x += QStringLiteral( "                    <hkparam name=\"variant\">%1</hkparam>\n                </hkobject>\n" ).arg( idCont );
	if ( res ) {
		x += QLatin1String( "                <hkobject>\n"
							"                    <hkparam name=\"name\">Resource Data</hkparam>\n"
							"                    <hkparam name=\"className\">hkMemoryResourceContainer</hkparam>\n" );
		x += QStringLiteral( "                    <hkparam name=\"variant\">%1</hkparam>\n                </hkobject>\n" ).arg( idRes );
	}
	x += QLatin1String( "            </hkparam>\n        </hkobject>\n" );

	x += QStringLiteral( "        <hkobject class=\"hkaAnimationContainer\" name=\"%1\" signature=\"0x26859f4c\">\n" ).arg( idCont );
	x += QLatin1String( "            <hkparam name=\"skeletons\" numelements=\"0\">\n</hkparam>\n" );
	x += QStringLiteral( "            <hkparam name=\"animations\" numelements=\"1\">\n%1\n</hkparam>\n" ).arg( idAnim );
	x += QStringLiteral( "            <hkparam name=\"bindings\" numelements=\"1\">\n%1\n</hkparam>\n" ).arg( idBind );
	x += QLatin1String( "            <hkparam name=\"attachments\" numelements=\"0\">\n</hkparam>\n" );
	x += QLatin1String( "            <hkparam name=\"skins\" numelements=\"0\">\n</hkparam>\n        </hkobject>\n" );

	x += QStringLiteral( "        <hkobject class=\"hkaInterleavedUncompressedAnimation\" name=\"%1\" signature=\"0xa5eff3f2\">\n" ).arg( idAnim );
	x += QLatin1String( "            <hkparam name=\"type\">HK_INTERLEAVED_ANIMATION</hkparam>\n" );
	x += QStringLiteral( "            <hkparam name=\"duration\">%1</hkparam>\n" ).arg( f9( clip.duration ) );
	x += QStringLiteral( "            <hkparam name=\"numberOfTransformTracks\">%1</hkparam>\n" ).arg( nT );
	x += QLatin1String( "            <hkparam name=\"numberOfFloatTracks\">0</hkparam>\n" );
	x += QStringLiteral( "            <hkparam name=\"extractedMotion\">%1</hkparam>\n" ).arg( rm ? idMotion : QStringLiteral( "null" ) );
	x += QStringLiteral( "            <hkparam name=\"annotationTracks\" numelements=\"%1\">\n" ).arg( nT );
	for ( int t = 0; t < nT; t++ ) {
		const int n = ( opt.writeAnnotations && t < clip.annotations.size() ) ? int( clip.annotations[t].size() ) : 0;
		x += QLatin1String( "<hkobject>\n<hkparam name=\"trackName\"/>\n" );
		if ( !n ) {
			x += QLatin1String( "<hkparam name=\"annotations\" numelements=\"0\"/>\n" );
		} else {
			x += QStringLiteral( "<hkparam name=\"annotations\" numelements=\"%1\">\n" ).arg( n );
			for ( int a = 0; a < n; a++ )
				x += QStringLiteral( "<hkobject>\n<hkparam name=\"time\">%1</hkparam>\n<hkparam name=\"text\">%2</hkparam>\n</hkobject>\n" )
					.arg( f9( clip.annotations[t][a].time ) ).arg( esc( clip.annotations[t][a].text ) );
			x += QLatin1String( "</hkparam>\n" );
		}
		x += QLatin1String( "</hkobject>\n" );
	}
	x += QLatin1String( "</hkparam>\n" );
	x += QStringLiteral( "            <hkparam name=\"transforms\" numelements=\"%1\">" ).arg( nT * nF );
	for ( int f = 0; f < nF; f++ ) {
		for ( int t = 0; t < nT; t++ ) {
			const HkxTransform & h = clip.frames[f][t];
			if ( f || t )
				x += QLatin1Char( '\n' );
			x += QStringLiteral( "(%1 %2 %3)(%4 %5 %6 %7)(%8 %9 %10)" )
				.arg( f9( h.translation[0] ) ).arg( f9( h.translation[1] ) ).arg( f9( h.translation[2] ) )
				.arg( f9( h.rotation[1] ) ).arg( f9( h.rotation[2] ) ).arg( f9( h.rotation[3] ) ).arg( f9( h.rotation[0] ) )
				.arg( f9( h.scale[0] ) ).arg( f9( h.scale[1] ) ).arg( f9( h.scale[2] ) );
		}
	}
	x += QLatin1String( "</hkparam>\n            <hkparam name=\"floats\" numelements=\"0\"/>\n        </hkobject>\n" );

	if ( rm ) {
		x += QStringLiteral( "        <hkobject class=\"hkaDefaultAnimatedReferenceFrame\" name=\"%1\" signature=\"0x60f8e0b8\">\n" ).arg( idMotion );
		x += QStringLiteral( "            <hkparam name=\"up\">(%1 %2 %3 0.0)</hkparam>\n" )
			.arg( f9( clip.rootMotionUp[0] ) ).arg( f9( clip.rootMotionUp[1] ) ).arg( f9( clip.rootMotionUp[2] ) );
		x += QStringLiteral( "            <hkparam name=\"forward\">(%1 %2 %3 0.0)</hkparam>\n" )
			.arg( f9( clip.rootMotionForward[0] ) ).arg( f9( clip.rootMotionForward[1] ) ).arg( f9( clip.rootMotionForward[2] ) );
		x += QStringLiteral( "            <hkparam name=\"duration\">%1</hkparam>\n" ).arg( f9( clip.duration ) );
		x += QStringLiteral( "            <hkparam name=\"referenceFrameSamples\" numelements=\"%1\">" ).arg( nF );
		for ( int f = 0; f < nF; f++ ) {
			const HkxRootMotion & s = clip.rootMotion[f];
			x += QStringLiteral( "%1(%2 %3 %4 %5)" ).arg( f ? QStringLiteral( " " ) : QString() )
				.arg( f9( s.translation[0] ) ).arg( f9( s.translation[1] ) ).arg( f9( s.translation[2] ) ).arg( f9( s.yaw ) );
		}
		x += QLatin1String( "</hkparam>\n        </hkobject>\n" );
	}

	x += QStringLiteral( "        <hkobject class=\"hkaAnimationBinding\" name=\"%1\" signature=\"0xfaf9150\">\n" ).arg( idBind );
	x += QStringLiteral( "            <hkparam name=\"originalSkeletonName\">%1</hkparam>\n" ).arg( esc( skel ) );
	x += QStringLiteral( "            <hkparam name=\"animation\">%1</hkparam>\n" ).arg( idAnim );
	x += QStringLiteral( "            <hkparam name=\"transformTrackToBoneIndices\" numelements=\"%1\">" ).arg( nT );
	for ( int t = 0; t < nT; t++ )
		x += QStringLiteral( "%1%2" ).arg( t ? QStringLiteral( " " ) : QString() ).arg( clip.trackToBone[t] );
	x += QLatin1String( "</hkparam>\n" );
	x += QLatin1String( "            <hkparam name=\"floatTrackToFloatSlotIndices\" numelements=\"0\"/>\n" );
	x += QLatin1String( "            <hkparam name=\"partitionIndices\" numelements=\"0\"/>\n" );
	x += QStringLiteral( "            <hkparam name=\"blendHint\">%1</hkparam>\n        </hkobject>\n" ).arg( QLatin1String( blendHintName( clip.blendHint ) ) );

	if ( res ) {
		x += QStringLiteral( "        <hkobject class=\"hkMemoryResourceContainer\" name=\"%1\" signature=\"0x1de13a73\">\n" ).arg( idRes );
		x += QLatin1String( "            <hkparam name=\"name\"/>\n" );
		x += QLatin1String( "            <hkparam name=\"resourceHandles\" numelements=\"0\">\n</hkparam>\n" );
		x += QLatin1String( "            <hkparam name=\"children\" numelements=\"0\">\n</hkparam>\n        </hkobject>\n" );
	}
	x += QLatin1String( "    </hksection>\n</hkpackfile>\n" );
	return x.toLatin1();
}

// ---------------------------------------------------------------------------

bool hkxWrite( const HkxAnimClip & clip, const QString & path,
			   const HkxWriteOptions & opt, HkxWriteReport & rep )
{
	rep = HkxWriteReport();
	rep.tracks = clip.numTracks;
	rep.frames = clip.numFrames;
	rep.transformBytes = qint64( 48 ) * clip.numTracks * clip.numFrames;

	if ( opt.route == HkxWriteOptions::RouteDirect ) {
		QString err;
		const QByteArray blob = hkxWritePackfile( clip, opt, err );
		if ( blob.isEmpty() ) {
			rep.error = err.isEmpty() ? QStringLiteral( "the packfile emitter produced no bytes" ) : err;
			return false;
		}
		QFile f( path );
		if ( !f.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
			rep.error = QStringLiteral( "cannot write '%1': %2" ).arg( path ).arg( f.errorString() );
			return false;
		}
		if ( f.write( blob ) != blob.size() ) {
			rep.error = QStringLiteral( "short write to '%1': %2" ).arg( path ).arg( f.errorString() );
			f.close();
			return false;
		}
		f.close();
		rep.routeUsed = QStringLiteral( "direct packfile" );
		rep.objects = 4 + ( wantsRootMotion( clip, opt ) ? 1 : 0 ) + ( opt.writeResourceContainer ? 1 : 0 );
		rep.fileBytes = blob.size();
		return true;
	}

	// route A
	QString err;
	const QByteArray xml = hkxWriteXml( clip, opt, err );
	if ( xml.isEmpty() ) {
		rep.error = err.isEmpty() ? QStringLiteral( "the XML writer produced no bytes" ) : err;
		return false;
	}
	QString jar = opt.hkxpackJar;
	if ( jar.isEmpty() )
		jar = QStringLiteral( "E:/Tools/Fallout 4/HKXPACK/hkxpack-cli.jar" );
	if ( !QFileInfo::exists( jar ) ) {
		rep.error = QStringLiteral( "route A needs HKXPACK; '%1' does not exist" ).arg( jar );
		return false;
	}
	QTemporaryDir tmp;
	if ( !tmp.isValid() ) {
		rep.error = QStringLiteral( "cannot make a temporary directory for the XML: %1" ).arg( tmp.errorString() );
		return false;
	}
	const QString xmlPath = opt.xmlKeepPath.isEmpty()
		? tmp.filePath( QFileInfo( path ).completeBaseName() + QStringLiteral( ".xml" ) )
		: opt.xmlKeepPath;
	{
		QFile xf( xmlPath );
		if ( !xf.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
			rep.error = QStringLiteral( "cannot write the intermediate XML '%1': %2" ).arg( xmlPath ).arg( xf.errorString() );
			return false;
		}
		xf.write( xml );
		xf.close();
	}
	const QStringList args = QStringList() << QStringLiteral( "-jar" ) << jar
		<< QStringLiteral( "pack" ) << xmlPath << QStringLiteral( "-o" ) << path;
	rep.command = opt.javaExe + QLatin1Char( ' ' ) + args.join( QLatin1Char( ' ' ) );
	QProcess proc;
	proc.start( opt.javaExe, args );
	if ( !proc.waitForStarted( 20000 ) ) {
		rep.error = QStringLiteral( "cannot start '%1': %2" ).arg( opt.javaExe ).arg( proc.errorString() );
		return false;
	}
	if ( !proc.waitForFinished( opt.packTimeoutMs ) ) {
		proc.kill();
		rep.error = QStringLiteral( "`%1` did not finish in %2 ms" ).arg( rep.command ).arg( opt.packTimeoutMs );
		return false;
	}
	if ( proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0 ) {
		rep.error = QStringLiteral( "`%1` exited %2: %3" ).arg( rep.command ).arg( proc.exitCode() )
			.arg( QString::fromLocal8Bit( proc.readAllStandardError() ).trimmed() );
		return false;
	}
	if ( !QFileInfo::exists( path ) ) {
		rep.error = QStringLiteral( "`%1` reported success but wrote no '%2'" ).arg( rep.command ).arg( path );
		return false;
	}
	rep.routeUsed = QStringLiteral( "HKXPACK XML + pack" );
	rep.objects = 4 + ( wantsRootMotion( clip, opt ) ? 1 : 0 ) + ( opt.writeResourceContainer ? 1 : 0 );
	rep.fileBytes = QFileInfo( path ).size();
	return true;
}
