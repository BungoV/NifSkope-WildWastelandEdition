/* FO4 Havok animation reader + spline decompressor. The law it implements is
   docs/HKX_ANIMATION_FORMAT.md; section numbers in the comments refer to it.
   Lane HKX1, 2026-09-10. */

#include "hkxanim.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QXmlStreamReader>

#include <cmath>
#include <cstring>

namespace {

// ----------------------------------------------------------------- refusal

struct Refusal
{
	QString what;
};

[[noreturn]] void refuse( const QString & s )
{
	throw Refusal{ s };
}

// ----------------------------------------------------------------- the intermediate form both routes fill

struct RawSkeleton
{
	QString name;
	QStringList boneNames;
	QVector<int> parents;
	QVector<bool> lockTranslation;
	//! 12 floats per bone as stored: t xyzw, q xyzw, s xyzw
	QVector<float> pose;
};

struct RawMotion
{
	float up[4] = { 0, 0, 1, 0 };
	float forward[4] = { 0, 1, 0, 0 };
	float duration = 0.0f;
	QVector<float> samples;   //!< 4 per frame
};

struct RawAnim
{
	QString ref;
	int type = -1;            //!< hkaAnimation::AnimationType, 3 = spline compressed
	float duration = 0.0f;
	int numTransformTracks = 0, numFloatTracks = 0;
	int numFrames = 0, numBlocks = 0, maxFramesPerBlock = 0, maskAndQuantizationSize = 0;
	float blockDuration = 0.0f, blockInverseDuration = 0.0f, frameDuration = 0.0f;
	QVector<quint32> blockOffsets, floatBlockOffsets, transformOffsets, floatOffsets;
	QByteArray data;
	int endian = 0;
	QVector<QVector<HkxAnnotation>> annotations;
	//! hkaInterleavedUncompressedAnimation ONLY (lane BUILD8): the whole
	//! hkArray<hkQsTransform> as 12 floats per element, frame-major --
	//! 12 * numTransformTracks * numFrames entries.  Empty for a spline clip,
	//! and that emptiness is what selects the decoder arm.
	QVector<float> interleaved;
	bool hasMotion = false;
	RawMotion motion;
};

struct RawBinding
{
	QString originalSkeletonName;
	QString animationRef;
	QVector<int> trackToBone;
	QVector<int> floatTrackToSlot;
	QVector<int> partitionIndices;
	QString blendHint;
};

struct Raw
{
	QVector<RawSkeleton> skeletons;
	QVector<RawAnim> anims;
	QVector<RawBinding> bindings;
};

// ----------------------------------------------------------------- 4.6 packed quaternions (Havok x y z w)

constexpr double SQRT_HALF = 0.70710678118654752440;

void insertMissing( double a, double b, double c, double d, int missing, double out[4] )
{
	double v[3] = { a, b, c };
	int k = 0;
	for ( int i = 0; i < 4; i++ )
		out[i] = ( i == missing ) ? d : v[k++];
}

void unpack40( const quint8 * p, double out[4] )
{
	quint64 v = 0;
	for ( int i = 4; i >= 0; i-- )
		v = ( v << 8 ) | p[i];
	const int a = int( v & 0xFFF ), b = int( ( v >> 12 ) & 0xFFF ), c = int( ( v >> 24 ) & 0xFFF );
	const int missing = int( ( v >> 36 ) & 3 );
	const bool negate = ( ( v >> 38 ) & 1 ) != 0;
	const double f = SQRT_HALF / 2047.0;
	const double x = ( a - 2047 ) * f, y = ( b - 2047 ) * f, z = ( c - 2047 ) * f;
	double d = std::sqrt( std::max( 0.0, 1.0 - x * x - y * y - z * z ) );
	if ( negate )
		d = -d;
	insertMissing( x, y, z, d, missing, out );
}

void unpack48( const quint8 * p, double out[4] )
{
	const unsigned w0 = p[0] | ( p[1] << 8 ), w1 = p[2] | ( p[3] << 8 ), w2 = p[4] | ( p[5] << 8 );
	const int a = int( w0 & 0x7FFF ), b = int( w1 & 0x7FFF ), c = int( w2 & 0x7FFF );
	const int missing = int( ( w0 >> 15 ) | ( ( w1 >> 15 ) << 1 ) );
	const bool negate = ( w2 >> 15 ) != 0;
	const double f = SQRT_HALF / 16383.0;
	const double x = ( a - 16383 ) * f, y = ( b - 16383 ) * f, z = ( c - 16383 ) * f;
	double d = std::sqrt( std::max( 0.0, 1.0 - x * x - y * y - z * z ) );
	if ( negate )
		d = -d;
	insertMissing( x, y, z, d, missing, out );
}

const char * rotationName( int q )
{
	static const char * names[] = { "POLAR32", "THREECOMP40", "THREECOMP48", "THREECOMP24", "STRAIGHT16", "UNCOMPRESSED" };
	return ( q >= 0 && q < 6 ) ? names[q] : "unknown";
}

// ----------------------------------------------------------------- 4.5 NURBS

int findSpan( double u, const QVector<double> & knots, int n, int degree )
{
	if ( u >= knots[n + 1] )
		return n;
	if ( u <= knots[0] )
		return degree;
	int lo = degree, hi = n + 1, mid = ( lo + hi ) / 2;
	while ( u < knots[mid] || u >= knots[mid + 1] ) {
		if ( u < knots[mid] )
			hi = mid;
		else
			lo = mid;
		mid = ( lo + hi ) / 2;
	}
	return mid;
}

//! de Boor on control points of `dim` doubles each, stored flat.
void deBoor( double u, const QVector<double> & knots, const QVector<double> & pts, int dim, int degree, int span, double * out )
{
	double d[4][4];
	for ( int j = 0; j <= degree; j++ )
		for ( int k = 0; k < dim; k++ )
			d[j][k] = pts[( span - degree + j ) * dim + k];
	for ( int r = 1; r <= degree; r++ ) {
		for ( int j = degree; j >= r; j-- ) {
			const int i = j + span - degree;
			const double denom = knots[i + degree - r + 1] - knots[i];
			const double alpha = ( denom == 0.0 ) ? 0.0 : ( u - knots[i] ) / denom;
			for ( int k = 0; k < dim; k++ )
				d[j][k] = ( 1.0 - alpha ) * d[j - 1][k] + alpha * d[j][k];
		}
	}
	for ( int k = 0; k < dim; k++ )
		out[k] = d[degree][k];
}

// ----------------------------------------------------------------- 4.4 the block walk, parsed once per block

struct Cursor
{
	const quint8 * d;
	int p, end;
	QString where;

	void need( int n ) const
	{
		if ( p + n > end )
			refuse( QString( "%1: the track walk leaves the block (needs %2 bytes at %3, block ends at %4)" ).arg( where ).arg( n ).arg( p ).arg( end ) );
	}
	void align( int a ) { p = ( p + a - 1 ) / a * a; }
	quint8 u8() { need( 1 ); return d[p++]; }
	quint16 u16() { need( 2 ); quint16 v = quint16( d[p] | ( d[p + 1] << 8 ) ); p += 2; return v; }
	float f32() { need( 4 ); float v; std::memcpy( &v, d + p, 4 ); p += 4; return v; }
	const quint8 * raw( int n ) { need( n ); const quint8 * r = d + p; p += n; return r; }
};

struct Curve
{
	int n = 0, degree = 0;
	QVector<double> knots;
	QVector<double> pts;      //!< flat, dim per point
	int dim = 0;
};

struct VectorTrack
{
	double value[3];          //!< static / default values
	bool spline[3] = { false, false, false };
	bool anySpline = false;
	Curve curve;
};

struct RotationTrack
{
	bool isSpline = false;
	double value[4] = { 0, 0, 0, 1 };
	Curve curve;              //!< dim 4
};

struct ParsedTrack
{
	VectorTrack translation, scale;
	RotationTrack rotation;
};

void readKnots( Cursor & c, Curve & cv )
{
	cv.n = c.u16();
	cv.degree = c.u8();
	if ( cv.degree < 1 || cv.degree > 3 )
		refuse( QString( "%1: spline degree %2, the engine evaluates 1..3 only" ).arg( c.where ).arg( cv.degree ) );
	if ( cv.n + 1 > 256 )
		refuse( QString( "%1: %2 control points, more than a 256-frame block can hold" ).arg( c.where ).arg( cv.n + 1 ) );
	const int nk = cv.n + cv.degree + 2;
	const quint8 * k = c.raw( nk );
	cv.knots.resize( nk );
	for ( int i = 0; i < nk; i++ )
		cv.knots[i] = double( k[i] );
}

void readVectorTrack( Cursor & c, int mask, int quant, const double def[3], VectorTrack & t )
{
	for ( int i = 0; i < 3; i++ )
		t.value[i] = def[i];
	if ( mask == 0 )
		return;
	if ( quant != 1 )
		refuse( QString( "%1: scalar quantization %2 is not decoded by this reader" ).arg( c.where ).arg( quant == 0 ? "BITS8" : "unknown" ) );
	bool isStatic[3];
	for ( int i = 0; i < 3; i++ ) {
		isStatic[i] = ( mask >> i ) & 1;
		t.spline[i] = ( mask >> ( 4 + i ) ) & 1;
	}
	t.anySpline = ( mask & 0xF0 ) != 0;
	if ( t.anySpline )
		readKnots( c, t.curve );
	c.align( 4 );
	double lo[3] = { 0, 0, 0 }, hi[3] = { 0, 0, 0 };
	for ( int i = 0; i < 3; i++ ) {
		if ( isStatic[i] ) {
			t.value[i] = c.f32();
		} else if ( t.spline[i] ) {
			lo[i] = c.f32();
			hi[i] = c.f32();
		}
	}
	if ( t.anySpline ) {
		c.align( 2 );
		int dim = 0;
		for ( int i = 0; i < 3; i++ )
			dim += t.spline[i] ? 1 : 0;
		t.curve.dim = dim;
		t.curve.pts.resize( ( t.curve.n + 1 ) * dim );
		for ( int j = 0; j <= t.curve.n; j++ ) {
			int k = 0;
			for ( int i = 0; i < 3; i++ ) {
				if ( t.spline[i] ) {
					const double q = c.u16();
					t.curve.pts[j * dim + k++] = lo[i] + q * ( 1.0 / 65535.0 ) * ( hi[i] - lo[i] );
				}
			}
		}
	}
}

void readRotationTrack( Cursor & c, int mask, int rq, RotationTrack & t, QString & quantSeen )
{
	if ( mask == 0 )
		return;
	if ( rq != 1 && rq != 2 )
		refuse( QString( "%1: rotation quantization %2 is not decoded by this reader" ).arg( c.where ).arg( rotationName( rq ) ) );
	const QString qn = QLatin1String( rotationName( rq ) );
	if ( !quantSeen.contains( qn ) )
		quantSeen += ( quantSeen.isEmpty() ? "" : "+" ) + qn;
	const int size = ( rq == 1 ) ? 5 : 6;
	const int alignment = ( rq == 1 ) ? 1 : 2;
	if ( mask & 0xF0 ) {
		t.isSpline = true;
		readKnots( c, t.curve );
		c.align( alignment );
		t.curve.dim = 4;
		t.curve.pts.resize( ( t.curve.n + 1 ) * 4 );
		for ( int j = 0; j <= t.curve.n; j++ ) {
			double q[4];
			if ( rq == 1 )
				unpack40( c.raw( size ), q );
			else
				unpack48( c.raw( size ), q );
			for ( int k = 0; k < 4; k++ )
				t.curve.pts[j * 4 + k] = q[k];
		}
	} else {
		c.align( alignment );
		if ( rq == 1 )
			unpack40( c.raw( size ), t.value );
		else
			unpack48( c.raw( size ), t.value );
	}
}

//! Parse one block's transform tracks (4.1, 4.3, 4.4). Returns the byte where the walk ended.
int parseBlock( const RawAnim & a, int block, QVector<ParsedTrack> & tracks, QString & quantSeen )
{
	const quint8 * data = reinterpret_cast<const quint8 *>( a.data.constData() );
	const int bo = int( a.blockOffsets[block] );
	const int mqs = a.maskAndQuantizationSize;
	const int blockEnd = bo + int( a.floatBlockOffsets[block] );
	if ( blockEnd > a.data.size() || bo + mqs > a.data.size() || bo + mqs > blockEnd )
		refuse( QString( "block %1: offsets %2 / %3 lie outside the %4-byte data" ).arg( block ).arg( bo ).arg( blockEnd ).arg( a.data.size() ) );
	Cursor c{ data, bo + mqs, blockEnd, QString() };
	static const double zero[3] = { 0, 0, 0 }, one[3] = { 1, 1, 1 };
	tracks.resize( a.numTransformTracks );
	for ( int t = 0; t < a.numTransformTracks; t++ ) {
		c.where = QString( "block %1 track %2" ).arg( block ).arg( t );
		const quint8 q = data[bo + 4 * t], pm = data[bo + 4 * t + 1], rm = data[bo + 4 * t + 2], sm = data[bo + 4 * t + 3];
		const int pq = q & 3, rq = ( q >> 2 ) & 0xF, sq = ( q >> 6 ) & 3;
		ParsedTrack & pt = tracks[t];
		readVectorTrack( c, pm, pq, zero, pt.translation );
		c.align( 4 );
		readRotationTrack( c, rm, rq, pt.rotation, quantSeen );
		c.align( 4 );
		readVectorTrack( c, sm, sq, one, pt.scale );
		c.align( 4 );
	}
	if ( c.p != blockEnd && a.numFloatTracks == 0 )
		refuse( QString( "block %1: the track walk ended at %2, the block ends at %3" ).arg( block ).arg( c.p ).arg( blockEnd ) );
	return c.p;
}

void evalVector( const VectorTrack & t, double u, double out[3] )
{
	for ( int i = 0; i < 3; i++ )
		out[i] = t.value[i];
	if ( !t.anySpline )
		return;
	double v[3];
	const int span = findSpan( u, t.curve.knots, t.curve.n, t.curve.degree );
	deBoor( u, t.curve.knots, t.curve.pts, t.curve.dim, t.curve.degree, span, v );
	int k = 0;
	for ( int i = 0; i < 3; i++ )
		if ( t.spline[i] )
			out[i] = v[k++];
}

//! Returns the pre-normalisation length.
double evalRotation( const RotationTrack & t, double u, double out[4] )
{
	if ( !t.isSpline ) {
		for ( int k = 0; k < 4; k++ )
			out[k] = t.value[k];
	} else {
		const int span = findSpan( u, t.curve.knots, t.curve.n, t.curve.degree );
		deBoor( u, t.curve.knots, t.curve.pts, 4, t.curve.degree, span, out );
	}
	const double len = std::sqrt( out[0] * out[0] + out[1] * out[1] + out[2] * out[2] + out[3] * out[3] );
	if ( len == 0.0 )
		refuse( "a zero-length rotation" );
	for ( int k = 0; k < 4; k++ )
		out[k] /= len;
	return len;
}

HkxTransform evalTrack( const ParsedTrack & pt, double u, double & qlen )
{
	double tr[3], q[4], sc[3];
	evalVector( pt.translation, u, tr );
	qlen = evalRotation( pt.rotation, u, q );
	evalVector( pt.scale, u, sc );
	HkxTransform x;
	x.translation = Vector3( float( tr[0] ), float( tr[1] ), float( tr[2] ) );
	// Havok x y z w -> NifSkope w x y z
	x.rotation = Quat( float( q[3] ), float( q[0] ), float( q[1] ), float( q[2] ) );
	x.scale = Vector3( float( sc[0] ), float( sc[1] ), float( sc[2] ) );
	return x;
}

// ----------------------------------------------------------------- 7 validation, then the decode

void validate( const Raw & r )
{
	if ( r.anims.isEmpty() && r.skeletons.isEmpty() )
		refuse( "the container holds no animation and no skeleton" );
	for ( const RawAnim & a : r.anims ) {
		/* THE INTERLEAVED ARM (lane BUILD8).  An
		 * hkaInterleavedUncompressedAnimation has no blocks, no quantization
		 * mask and no stored frame count, so none of the spline rules below
		 * apply to it and every one of them would refuse a well-formed file.
		 * What CAN be checked is checked here. */
		if ( !a.interleaved.isEmpty() ) {
			if ( a.type != 1 )
				refuse( QString( "hkaAnimation::type is %1; HK_INTERLEAVED_ANIMATION is 1" ).arg( a.type ) );
			if ( a.numTransformTracks < 1 || a.numFrames < 1 )
				refuse( QString( "numTransformTracks %1 / numFrames %2" ).arg( a.numTransformTracks ).arg( a.numFrames ) );
			if ( a.interleaved.size() != 12 * a.numTransformTracks * a.numFrames )
				refuse( QString( "transforms holds %1 floats, not 12 * %2 tracks * %3 frames" )
						.arg( a.interleaved.size() ).arg( a.numTransformTracks ).arg( a.numFrames ) );
			if ( std::fabs( a.duration - ( a.numFrames - 1 ) * a.frameDuration ) > 1e-3 * std::max( 1.0f, a.duration ) )
				refuse( QString( "duration %1 is not (numFrames-1) * frameDuration = %2" )
						.arg( a.duration ).arg( ( a.numFrames - 1 ) * a.frameDuration ) );
			if ( a.hasMotion && a.motion.samples.size() != 4 * a.numFrames )
				refuse( QString( "%1 root-motion samples for %2 frames" ).arg( a.motion.samples.size() / 4 ).arg( a.numFrames ) );
			continue;
		}
		if ( a.type != 3 )
			refuse( QString( "animation type %1 is not HK_SPLINE_COMPRESSED_ANIMATION (3)" ).arg( a.type ) );
		if ( a.endian != 0 )
			refuse( QString( "endian %1: only little-endian data is decoded" ).arg( a.endian ) );
		if ( a.maxFramesPerBlock < 2 )
			refuse( QString( "maxFramesPerBlock %1" ).arg( a.maxFramesPerBlock ) );
		if ( a.numFrames < 1 || a.numBlocks < 1 )
			refuse( QString( "numFrames %1 / numBlocks %2" ).arg( a.numFrames ).arg( a.numBlocks ) );
		if ( a.numTransformTracks < 0 || a.numFloatTracks < 0 )
			refuse( "negative track count" );
		if ( a.blockOffsets.size() != a.numBlocks || a.floatBlockOffsets.size() != a.numBlocks )
			refuse( QString( "blockOffsets has %1 entries, floatBlockOffsets %2, numBlocks is %3" ).arg( a.blockOffsets.size() ).arg( a.floatBlockOffsets.size() ).arg( a.numBlocks ) );
		if ( a.maskAndQuantizationSize != 4 * ( a.numTransformTracks + a.numFloatTracks ) )
			refuse( QString( "maskAndQuantizationSize %1 is not 4 * (%2 + %3)" ).arg( a.maskAndQuantizationSize ).arg( a.numTransformTracks ).arg( a.numFloatTracks ) );
		const int needBlocks = ( a.numFrames - 1 ) / ( a.maxFramesPerBlock - 1 ) + 1;
		if ( a.numFrames > 1 && needBlocks > a.numBlocks )
			refuse( QString( "%1 frames need %2 blocks of %3, the file has %4" ).arg( a.numFrames ).arg( needBlocks ).arg( a.maxFramesPerBlock ).arg( a.numBlocks ) );
		for ( int b = 0; b < a.numBlocks; b++ ) {
			const qint64 bo = a.blockOffsets[b], fo = a.floatBlockOffsets[b];
			if ( bo + a.maskAndQuantizationSize > a.data.size() || bo + fo > a.data.size() || fo < a.maskAndQuantizationSize )
				refuse( QString( "block %1: offset %2 / float offset %3 against %4 data bytes" ).arg( b ).arg( bo ).arg( fo ).arg( a.data.size() ) );
		}
		if ( std::fabs( a.duration - ( a.numFrames - 1 ) * a.frameDuration ) > 1e-3 * std::max( 1.0f, a.duration ) )
			refuse( QString( "duration %1 is not (numFrames-1) * frameDuration = %2" ).arg( a.duration ).arg( ( a.numFrames - 1 ) * a.frameDuration ) );
		if ( a.hasMotion && a.motion.samples.size() != 4 * a.numFrames )
			refuse( QString( "%1 root-motion samples for %2 frames" ).arg( a.motion.samples.size() / 4 ).arg( a.numFrames ) );
	}
	for ( const RawBinding & b : r.bindings ) {
		const RawAnim * anim = nullptr;
		for ( const RawAnim & a : r.anims )
			if ( a.ref == b.animationRef )
				anim = &a;
		if ( !anim )
			refuse( QString( "binding refers to animation %1 which is not in the container" ).arg( b.animationRef ) );
		/* AN EMPTY transformTrackToBoneIndices IS THE IDENTITY MAP, not a
		 * missing one (lane FIXTURE, 2026-09-10, measured on the Mixamo
		 * Collection clip Running_To_Slide_And_Back_To_Running.hkx: 95
		 * tracks, binding count 0 with no local fixup -- the array really is
		 * absent, it is not a parse failure). The engine's own consumers read
		 * track i as bone i in that case, and frame 0's per-track translation
		 * against skeleton.hkx's reference pose confirms it: 75 of 95 within
		 * 1e-3 at shift 0, only 18 at any other shift.
		 *
		 * The reader therefore ACCEPTS an empty vector and fills the identity
		 * map in decodeClip(), flagging HkxAnimClip::trackToBoneIsIdentity so
		 * the derived map is never reported as a stored one. A non-empty
		 * vector of the wrong length is still refused by name. Whether the
		 * skeleton is big enough for an identity map -- at least numTracks
		 * bones -- is the CONSUMER's gate (HkxPlayback::bind), because a clip
		 * file usually carries no skeleton at all. */
		if ( !b.trackToBone.isEmpty() && b.trackToBone.size() != anim->numTransformTracks )
			refuse( QString( "binding maps %1 tracks, the animation has %2" ).arg( b.trackToBone.size() ).arg( anim->numTransformTracks ) );
	}
	for ( const RawSkeleton & s : r.skeletons ) {
		if ( s.parents.size() != s.boneNames.size() || s.pose.size() != 12 * s.boneNames.size() )
			refuse( QString( "skeleton %1: %2 bones, %3 parents, %4 reference transforms" ).arg( s.name ).arg( s.boneNames.size() ).arg( s.parents.size() ).arg( s.pose.size() / 12 ) );
		for ( int i = 0; i < s.parents.size(); i++ )
			if ( s.parents[i] >= s.boneNames.size() || s.parents[i] < -1 )
				refuse( QString( "skeleton %1: bone %2 has parent %3" ).arg( s.name ).arg( i ).arg( s.parents[i] ) );
	}
}

HkxAnimClip decodeClip( const RawAnim & a, const RawBinding * b, const QString & name )
{
	HkxAnimClip clip;
	clip.name = name;
	clip.numFrames = a.numFrames;
	clip.numTracks = a.numTransformTracks;
	clip.numFloatTracks = a.numFloatTracks;
	clip.numBlocks = a.numBlocks;
	clip.maxFramesPerBlock = a.maxFramesPerBlock;
	clip.duration = a.duration;
	clip.frameDuration = a.frameDuration;
	clip.annotations = a.annotations;
	if ( b ) {
		clip.originalSkeletonName = b->originalSkeletonName;
		clip.blendHint = b->blendHint;
		clip.trackToBone = b->trackToBone;
	} else {
		clip.blendHint = "NORMAL";
	}
	// The identity fallback, for both arms: no binding at all, or a binding
	// whose transformTrackToBoneIndices is empty. Named in the clip, never
	// silent (CONSTITUTION 10, "a fallback is never a silent downgrade").
	if ( clip.trackToBone.isEmpty() && a.numTransformTracks > 0 ) {
		clip.trackToBoneIsIdentity = true;
		for ( int t = 0; t < a.numTransformTracks; t++ )
			clip.trackToBone.append( t );
	}
	if ( a.hasMotion ) {
		clip.rootMotionUp = Vector3( a.motion.up[0], a.motion.up[1], a.motion.up[2] );
		clip.rootMotionForward = Vector3( a.motion.forward[0], a.motion.forward[1], a.motion.forward[2] );
		for ( int f = 0; f < a.numFrames; f++ ) {
			HkxRootMotion m;
			m.translation = Vector3( a.motion.samples[4 * f], a.motion.samples[4 * f + 1], a.motion.samples[4 * f + 2] );
			m.yaw = a.motion.samples[4 * f + 3];
			clip.rootMotion.append( m );
		}
	}

	/* THE INTERLEAVED ARM (lane BUILD8): the frames ARE the file.  Nothing
	 * is evaluated, so worstQuatLengthDeviation and the block-overlap
	 * diagnostics stay at their zero defaults -- there are no blocks and no
	 * spline to be off unit. */
	if ( !a.interleaved.isEmpty() ) {
		clip.frames.resize( a.numFrames );
		for ( int f = 0; f < a.numFrames; f++ ) {
			QVector<HkxTransform> & row = clip.frames[f];
			row.resize( a.numTransformTracks );
			for ( int t = 0; t < a.numTransformTracks; t++ ) {
				const float * v = a.interleaved.constData() + 12 * ( f * a.numTransformTracks + t );
				HkxTransform & x = row[t];
				x.translation = Vector3( v[0], v[1], v[2] );		// v[3] is hkVector4's pad
				x.rotation = Quat( v[7], v[4], v[5], v[6] );		// Havok (x,y,z,w) -> Quat (w,x,y,z)
				x.scale = Vector3( v[8], v[9], v[10] );
			}
		}
		return clip;
	}

	// parse every block once
	QVector<QVector<ParsedTrack>> blocks( a.numBlocks );
	for ( int bk = 0; bk < a.numBlocks; bk++ ) {
		clip.walkEnd.append( parseBlock( a, bk, blocks[bk], clip.rotationQuantization ) );
		clip.blockEnd.append( int( a.blockOffsets[bk] + a.floatBlockOffsets[bk] ) );
	}

	// 4.2: frame F lives in block F / (maxFramesPerBlock-1) at local F - block * stride
	const int stride = a.maxFramesPerBlock - 1;
	clip.frames.resize( a.numFrames );
	for ( int f = 0; f < a.numFrames; f++ ) {
		int bk = f / stride;
		if ( bk > a.numBlocks - 1 )
			bk = a.numBlocks - 1;
		const int local = f - bk * stride;
		QVector<HkxTransform> & row = clip.frames[f];
		row.resize( a.numTransformTracks );
		for ( int t = 0; t < a.numTransformTracks; t++ ) {
			double qlen = 1.0;
			row[t] = evalTrack( blocks[bk][t], double( local ), qlen );
			clip.worstQuatLengthDeviation = std::max( clip.worstQuatLengthDeviation, float( std::fabs( 1.0 - qlen ) ) );
		}
		// gate (g): a boundary frame decoded from the previous block too
		if ( bk > 0 && local == 0 ) {
			clip.blockOverlapFrames++;
			for ( int t = 0; t < a.numTransformTracks; t++ ) {
				double qlen = 1.0;
				const HkxTransform o = evalTrack( blocks[bk - 1][t], double( stride ), qlen );
				const HkxTransform & x = row[t];
				for ( int k = 0; k < 3; k++ ) {
					clip.blockOverlapWorst = std::max( clip.blockOverlapWorst, std::fabs( o.translation[k] - x.translation[k] ) );
					clip.blockOverlapWorst = std::max( clip.blockOverlapWorst, std::fabs( o.scale[k] - x.scale[k] ) );
				}
				for ( int k = 0; k < 4; k++ )
					clip.blockOverlapWorst = std::max( clip.blockOverlapWorst, std::fabs( o.rotation[k] - x.rotation[k] ) );
			}
		}
	}
	return clip;
}

HkxAnimFile finish( const Raw & raw, const QString & name, const QString & route )
{
	HkxAnimFile out;
	out.route = route;
	validate( raw );
	for ( const RawSkeleton & s : raw.skeletons ) {
		HkxSkeleton sk;
		sk.name = s.name;
		sk.boneNames = s.boneNames;
		sk.parents = s.parents;
		sk.lockTranslation = s.lockTranslation;
		for ( int i = 0; i < s.boneNames.size(); i++ ) {
			const float * v = s.pose.constData() + 12 * i;
			HkxTransform x;
			x.translation = Vector3( v[0], v[1], v[2] );
			x.rotation = Quat( v[7], v[4], v[5], v[6] );
			x.scale = Vector3( v[8], v[9], v[10] );
			sk.referencePose.append( x );
		}
		out.skeletons.append( sk );
	}
	for ( const RawAnim & a : raw.anims ) {
		const RawBinding * b = nullptr;
		for ( const RawBinding & rb : raw.bindings )
			if ( rb.animationRef == a.ref )
				b = &rb;
		out.clips.append( decodeClip( a, b, name ) );
	}
	return out;
}

// ----------------------------------------------------------------- route A: HKXPACK XML

struct XObj
{
	QString cls, id;
	QHash<QString, QString> text;              //!< hkparam name -> text
	QHash<QString, int> numelements;           //!< hkparam name -> its numelements attribute, -1 when absent
	QHash<QString, QVector<XObj>> children;    //!< hkparam name -> nested hkobjects
};

XObj readObject( QXmlStreamReader & x )
{
	XObj o;
	o.cls = x.attributes().value( "class" ).toString();
	o.id = x.attributes().value( "name" ).toString();
	while ( !x.atEnd() ) {
		x.readNext();
		if ( x.isEndElement() && x.name() == QLatin1String( "hkobject" ) )
			return o;
		if ( x.isStartElement() && x.name() == QLatin1String( "hkparam" ) ) {
			const QString pname = x.attributes().value( "name" ).toString();
			bool hasN = false;
			const int ne = x.attributes().value( "numelements" ).toInt( &hasN );
			o.numelements.insert( pname, hasN ? ne : -1 );
			QString text;
			QVector<XObj> kids;
			while ( !x.atEnd() ) {
				x.readNext();
				if ( x.isEndElement() && x.name() == QLatin1String( "hkparam" ) )
					break;
				if ( x.isStartElement() && x.name() == QLatin1String( "hkobject" ) )
					kids.append( readObject( x ) );
				else if ( x.isStartElement() && x.name() == QLatin1String( "hkcstring" ) )
					text += x.readElementText() + "\n";
				else if ( x.isCharacters() )
					text += x.text();
			}
			o.text.insert( pname, text.trimmed() );
			if ( !kids.isEmpty() )
				o.children.insert( pname, kids );
		}
	}
	return o;
}

QVector<int> intsOf( const XObj & o, const char * name )
{
	QVector<int> v;
	const QString t = o.text.value( QLatin1String( name ) );
	for ( const QString & s : t.split( QRegularExpression( "\\s+" ), Qt::SkipEmptyParts ) ) {
		bool ok = false;
		const int i = s.toInt( &ok );
		if ( !ok )
			refuse( QString( "%1: '%2' is not an integer" ).arg( name, s ) );
		v.append( i );
	}
	return v;
}

QVector<quint32> u32sOf( const XObj & o, const char * name )
{
	QVector<quint32> v;
	for ( int i : intsOf( o, name ) )
		v.append( quint32( i ) );
	return v;
}

int intOf( const XObj & o, const char * name )
{
	bool ok = false;
	const int v = o.text.value( QLatin1String( name ) ).toInt( &ok );
	if ( !ok )
		refuse( QString( "%1 '%2' is not an integer" ).arg( name, o.text.value( QLatin1String( name ) ) ) );
	return v;
}

float floatOf( const XObj & o, const char * name )
{
	bool ok = false;
	const float v = o.text.value( QLatin1String( name ) ).toFloat( &ok );
	if ( !ok )
		refuse( QString( "%1 '%2' is not a number" ).arg( name, o.text.value( QLatin1String( name ) ) ) );
	return v;
}

//! "(a b c d)(e f g h)..." -> flat floats
QVector<float> vec4sOf( const QString & t )
{
	QVector<float> v;
	int p = 0;
	while ( ( p = t.indexOf( '(', p ) ) >= 0 ) {
		const int e = t.indexOf( ')', p );
		if ( e < 0 )
			refuse( "unterminated vector in the XML" );
		const QStringList parts = t.mid( p + 1, e - p - 1 ).split( QRegularExpression( "\\s+" ), Qt::SkipEmptyParts );
		if ( parts.size() != 4 )
			refuse( QString( "a vector with %1 components" ).arg( parts.size() ) );
		for ( const QString & s : parts ) {
			bool ok = false;
			v.append( s.toFloat( &ok ) );
			if ( !ok )
				refuse( QString( "'%1' is not a number" ).arg( s ) );
		}
		p = e + 1;
	}
	return v;
}

QStringList refsOf( const XObj & o, const char * name )
{
	return o.text.value( QLatin1String( name ) ).split( QRegularExpression( "\\s+" ), Qt::SkipEmptyParts );
}

} // namespace

HkxAnimFile hkxAnimLoadXml( const QByteArray & xml, const QString & name )
{
	HkxAnimFile out;
	out.route = "xml";
	try {
		QXmlStreamReader x( xml );
		QHash<QString, XObj> objs;
		bool packfile = false;
		while ( !x.atEnd() ) {
			x.readNext();
			if ( x.isStartElement() && x.name() == QLatin1String( "hkpackfile" ) )
				packfile = true;
			else if ( x.isStartElement() && x.name() == QLatin1String( "hkobject" ) ) {
				XObj o = readObject( x );
				if ( !o.id.isEmpty() )
					objs.insert( o.id, o );
			}
		}
		if ( x.hasError() )
			refuse( QString( "not well-formed XML: %1 at line %2" ).arg( x.errorString() ).arg( x.lineNumber() ) );
		if ( !packfile )
			refuse( "not an hkpackfile XML" );
		const XObj * container = nullptr;
		for ( const XObj & o : objs )
			if ( o.cls == QLatin1String( "hkaAnimationContainer" ) )
				container = &o;
		if ( !container )
			refuse( "no hkaAnimationContainer" );
		Raw raw;
		for ( const QString & ref : refsOf( *container, "skeletons" ) ) {
			if ( !objs.contains( ref ) )
				refuse( QString( "skeleton %1 is not in the file" ).arg( ref ) );
			const XObj & o = objs[ref];
			if ( o.cls != QLatin1String( "hkaSkeleton" ) )
				refuse( QString( "skeleton %1 is a %2" ).arg( ref, o.cls ) );
			RawSkeleton s;
			s.name = o.text.value( "name" );
			for ( int p : intsOf( o, "parentIndices" ) )
				s.parents.append( p == 65535 ? -1 : p );
			for ( const XObj & b : o.children.value( "bones" ) ) {
				s.boneNames.append( b.text.value( "name" ) );
				s.lockTranslation.append( b.text.value( "lockTranslation" ) == QLatin1String( "true" ) );
			}
			s.pose = vec4sOf( o.text.value( "referencePose" ) );
			raw.skeletons.append( s );
		}
		for ( const QString & ref : refsOf( *container, "animations" ) ) {
			if ( !objs.contains( ref ) )
				refuse( QString( "animation %1 is not in the file" ).arg( ref ) );
			const XObj & o = objs[ref];
			if ( o.cls != QLatin1String( "hkaSplineCompressedAnimation" ) )
				refuse( QString( "animation %1 is a %2, not decoded by this reader" ).arg( ref, o.cls ) );
			RawAnim a;
			a.ref = ref;
			a.type = ( o.text.value( "type" ) == QLatin1String( "HK_SPLINE_COMPRESSED_ANIMATION" ) ) ? 3 : -1;
			a.duration = floatOf( o, "duration" );
			a.numTransformTracks = intOf( o, "numberOfTransformTracks" );
			a.numFloatTracks = intOf( o, "numberOfFloatTracks" );
			a.numFrames = intOf( o, "numFrames" );
			a.numBlocks = intOf( o, "numBlocks" );
			a.maxFramesPerBlock = intOf( o, "maxFramesPerBlock" );
			a.maskAndQuantizationSize = intOf( o, "maskAndQuantizationSize" );
			a.blockDuration = floatOf( o, "blockDuration" );
			a.blockInverseDuration = floatOf( o, "blockInverseDuration" );
			a.frameDuration = floatOf( o, "frameDuration" );
			a.blockOffsets = u32sOf( o, "blockOffsets" );
			a.floatBlockOffsets = u32sOf( o, "floatBlockOffsets" );
			a.transformOffsets = u32sOf( o, "transformOffsets" );
			a.floatOffsets = u32sOf( o, "floatOffsets" );
			a.endian = intOf( o, "endian" );
			for ( int v : intsOf( o, "data" ) ) {
				if ( v < 0 || v > 255 )
					refuse( QString( "data byte %1 out of range" ).arg( v ) );
				a.data.append( char( v ) );
			}
			const int ne = o.numelements.value( "data", -1 );
			if ( ne >= 0 && ne != a.data.size() )
				refuse( QString( "data holds %1 bytes, numelements says %2" ).arg( a.data.size() ).arg( ne ) );
			for ( const XObj & tr : o.children.value( "annotationTracks" ) ) {
				QVector<HkxAnnotation> lst;
				for ( const XObj & an : tr.children.value( "annotations" ) ) {
					HkxAnnotation h;
					h.time = floatOf( an, "time" );
					h.text = an.text.value( "text" );
					lst.append( h );
				}
				a.annotations.append( lst );
			}
			const QString em = o.text.value( "extractedMotion" );
			if ( !em.isEmpty() && em != QLatin1String( "null" ) ) {
				if ( !objs.contains( em ) )
					refuse( QString( "extracted motion %1 is not in the file" ).arg( em ) );
				const XObj & m = objs[em];
				if ( m.cls != QLatin1String( "hkaDefaultAnimatedReferenceFrame" ) )
					refuse( QString( "extracted motion class %1 is not decoded" ).arg( m.cls ) );
				a.hasMotion = true;
				const QVector<float> up = vec4sOf( m.text.value( "up" ) ), fw = vec4sOf( m.text.value( "forward" ) );
				if ( up.size() != 4 || fw.size() != 4 )
					refuse( "reference frame up/forward are not vectors" );
				for ( int k = 0; k < 4; k++ ) {
					a.motion.up[k] = up[k];
					a.motion.forward[k] = fw[k];
				}
				a.motion.duration = floatOf( m, "duration" );
				a.motion.samples = vec4sOf( m.text.value( "referenceFrameSamples" ) );
			}
			raw.anims.append( a );
		}
		for ( const QString & ref : refsOf( *container, "bindings" ) ) {
			if ( !objs.contains( ref ) )
				refuse( QString( "binding %1 is not in the file" ).arg( ref ) );
			const XObj & o = objs[ref];
			RawBinding b;
			b.originalSkeletonName = o.text.value( "originalSkeletonName" );
			b.animationRef = o.text.value( "animation" );
			b.trackToBone = intsOf( o, "transformTrackToBoneIndices" );
			b.floatTrackToSlot = intsOf( o, "floatTrackToFloatSlotIndices" );
			b.partitionIndices = intsOf( o, "partitionIndices" );
			b.blendHint = o.text.value( "blendHint" );
			raw.bindings.append( b );
		}
		out = finish( raw, name, "xml" );
	} catch ( const Refusal & r ) {
		out.error = r.what;
	}
	return out;
}

// ----------------------------------------------------------------- route B: the packfile (section 1)

namespace {

struct Blob
{
	const quint8 * d;
	int n;
	quint32 u32( qint64 o ) const
	{
		if ( o < 0 || o + 4 > n )
			refuse( QString( "read of 4 bytes at %1 leaves the %2-byte file" ).arg( o ).arg( n ) );
		return quint32( d[o] | ( d[o + 1] << 8 ) | ( d[o + 2] << 16 ) | ( quint32( d[o + 3] ) << 24 ) );
	}
	qint32 i32( qint64 o ) const { return qint32( u32( o ) ); }
	qint16 i16( qint64 o ) const
	{
		if ( o < 0 || o + 2 > n )
			refuse( QString( "read of 2 bytes at %1 leaves the file" ).arg( o ) );
		return qint16( d[o] | ( d[o + 1] << 8 ) );
	}
	quint8 u8( qint64 o ) const
	{
		if ( o < 0 || o + 1 > n )
			refuse( QString( "read at %1 leaves the file" ).arg( o ) );
		return d[o];
	}
	float f32( qint64 o ) const
	{
		const quint32 v = u32( o );
		float f;
		std::memcpy( &f, &v, 4 );
		return f;
	}
};

struct Packfile
{
	Blob b;
	qint64 base = 0;
	QHash<qint64, qint64> local, global;
	QVector<QPair<qint64, QString>> objects;
	QHash<qint64, QString> classOf;

	//! hkArray at `at`: payload offset (-1 when empty) and size
	qint64 arr( qint64 at, int esize, int & size ) const
	{
		size = b.i32( at + 8 );
		const qint64 p = local.value( at, -1 );
		if ( size < 0 )
			refuse( QString( "hkArray at %1: negative size" ).arg( at ) );
		if ( size && p < 0 )
			refuse( QString( "hkArray at %1: size %2 but no payload fixup" ).arg( at ).arg( size ) );
		if ( p >= 0 && p + qint64( size ) * esize > b.n )
			refuse( QString( "hkArray at %1: payload leaves the file" ).arg( at ) );
		return p;
	}
	QString cstr( qint64 at ) const
	{
		const qint64 p = local.value( at, -1 );
		if ( p < 0 )
			return QString();
		qint64 e = p;
		while ( e < b.n && b.d[e] != 0 )
			e++;
		return QString::fromLatin1( reinterpret_cast<const char *>( b.d + p ), int( e - p ) );
	}
};

void walkPackfile( Packfile & pf )
{
	const Blob & b = pf.b;
	if ( b.n < 0x50 || b.u32( 0 ) != 0x57E0E057 || b.u32( 4 ) != 0x10C0C010 )
		refuse( "not a Havok binary packfile (magic)" );
	const int numSections = b.i32( 20 );
	if ( numSections < 2 || numSections > 8 )
		refuse( QString( "packfile: %1 sections" ).arg( numSections ) );
	// 0x40-byte header, then the predicate array whose padded size is the u16 at 0x3e
	const qint64 sechdr = 0x40 + ( b.u8( 0x3e ) | ( b.u8( 0x3f ) << 8 ) );
	qint64 cnStart = -1, cnLen = 0;
	qint64 dataStart = -1, localOff = 0, globalOff = 0, virtOff = 0, expOff = 0, endOff = 0;
	for ( int s = 0; s < numSections; s++ ) {
		const qint64 off = sechdr + s * 0x40;
		if ( off + 0x30 > b.n )
			refuse( QString( "packfile: section header %1 lies outside the file" ).arg( s ) );
		char tag[20] = {};
		for ( int i = 0; i < 19; i++ )
			tag[i] = char( b.d[off + i] );
		if ( std::strcmp( tag, "__classnames__" ) == 0 ) {
			cnStart = b.u32( off + 20 );
			cnLen = b.u32( off + 24 );
		} else if ( std::strcmp( tag, "__data__" ) == 0 ) {
			dataStart = b.u32( off + 20 );
			localOff = b.u32( off + 24 );
			globalOff = b.u32( off + 28 );
			virtOff = b.u32( off + 32 );
			expOff = b.u32( off + 36 );
			endOff = b.u32( off + 44 );
		}
	}
	if ( cnStart < 0 || dataStart < 0 )
		refuse( "packfile: missing __classnames__ / __data__ section" );
	if ( dataStart + endOff > b.n || localOff > globalOff || globalOff > virtOff || virtOff > expOff || expOff > endOff )
		refuse( "packfile: __data__ fixup tables are truncated or out of order" );
	QHash<qint64, QString> classNames;
	for ( qint64 p = cnStart; p + 5 < cnStart + cnLen && p + 5 < b.n; ) {
		if ( b.d[p + 4] != 0x09 )
			break;
		qint64 e = p + 5;
		while ( e < b.n && b.d[e] != 0 )
			e++;
		classNames.insert( p + 5 - cnStart, QString::fromLatin1( reinterpret_cast<const char *>( b.d + p + 5 ), int( e - p - 5 ) ) );
		p = e + 1;
	}
	pf.base = dataStart;
	for ( qint64 p = dataStart + localOff; p + 8 <= dataStart + globalOff; p += 8 ) {
		const qint32 src = b.i32( p ), dst = b.i32( p + 4 );
		if ( src != -1 )
			pf.local.insert( dataStart + src, dataStart + dst );
	}
	for ( qint64 p = dataStart + globalOff; p + 12 <= dataStart + virtOff; p += 12 ) {
		const qint32 src = b.i32( p ), dst = b.i32( p + 8 );
		if ( src != -1 )
			pf.global.insert( dataStart + src, dataStart + dst );
	}
	for ( qint64 p = dataStart + virtOff; p + 12 <= dataStart + expOff; p += 12 ) {
		const qint32 src = b.i32( p ), cno = b.i32( p + 8 );
		if ( src != -1 ) {
			const QString cls = classNames.value( cno, "?" );
			pf.objects.append( { dataStart + src, cls } );
			pf.classOf.insert( dataStart + src, cls );
		}
	}
}

QVector<quint32> u32Array( const Packfile & pf, qint64 at )
{
	int n = 0;
	const qint64 p = pf.arr( at, 4, n );
	QVector<quint32> v( n );
	for ( int i = 0; i < n; i++ )
		v[i] = pf.b.u32( p + 4 * i );
	return v;
}

QVector<int> i16Array( const Packfile & pf, qint64 at )
{
	int n = 0;
	const qint64 p = pf.arr( at, 2, n );
	QVector<int> v( n );
	for ( int i = 0; i < n; i++ )
		v[i] = pf.b.i16( p + 2 * i );
	return v;
}

} // namespace

HkxAnimFile hkxAnimLoadPackfile( const QByteArray & blob, const QString & name )
{
	HkxAnimFile out;
	out.route = "packfile";
	try {
		Packfile pf;
		pf.b = Blob{ reinterpret_cast<const quint8 *>( blob.constData() ), int( blob.size() ) };
		walkPackfile( pf );
		qint64 co = -1;
		for ( const auto & o : pf.objects )
			if ( o.second == QLatin1String( "hkaAnimationContainer" ) )
				co = o.first;
		if ( co < 0 )
			refuse( "no hkaAnimationContainer" );
		Raw raw;
		int n = 0;
		// skeletons (+0x10)
		qint64 sp = pf.arr( co + 0x10, 8, n );
		for ( int i = 0; i < n; i++ ) {
			const qint64 so = pf.global.value( sp + 8 * i, -1 );
			if ( so < 0 || pf.classOf.value( so ) != QLatin1String( "hkaSkeleton" ) )
				refuse( QString( "skeleton %1 is a %2" ).arg( i ).arg( pf.classOf.value( so, "missing object" ) ) );
			RawSkeleton s;
			s.name = pf.cstr( so + 0x10 );
			s.parents = i16Array( pf, so + 0x18 );
			int bn = 0;
			const qint64 bp = pf.arr( so + 0x28, 16, bn );
			for ( int k = 0; k < bn; k++ ) {
				s.boneNames.append( pf.cstr( bp + 16 * k ) );
				s.lockTranslation.append( pf.b.u8( bp + 16 * k + 8 ) != 0 );
			}
			int rn = 0;
			const qint64 rp = pf.arr( so + 0x38, 48, rn );
			s.pose.resize( 12 * rn );
			for ( int k = 0; k < 12 * rn; k++ )
				s.pose[k] = pf.b.f32( rp + 4 * k );
			raw.skeletons.append( s );
		}
		// animations (+0x20)
		const qint64 ap = pf.arr( co + 0x20, 8, n );
		for ( int i = 0; i < n; i++ ) {
			const qint64 ao = pf.global.value( ap + 8 * i, -1 );
			if ( ao < 0 )
				refuse( QString( "animation %1 has no object" ).arg( i ) );
			const QString cls = pf.classOf.value( ao );
			// lane BUILD8: the reader now serves BOTH classes FO4 uses. Every
			// other class is refused with exactly the sentence it was before.
			const bool isInterleaved = ( cls == QLatin1String( "hkaInterleavedUncompressedAnimation" ) );
			if ( cls != QLatin1String( "hkaSplineCompressedAnimation" ) && !isInterleaved )
				refuse( QString( "animation %1 is a %2, not decoded by this reader" ).arg( i ).arg( cls ) );
			RawAnim a;
			a.ref = QString( "#%1" ).arg( ao - pf.base );
			// hkaAnimation's OWN members, shared by both subclasses: type +0x10,
			// duration +0x14, numberOfTransformTracks +0x18, numberOfFloatTracks
			// +0x1c, extractedMotion +0x20, annotationTracks +0x28. Only +0x38
			// onward is subclass territory.
			a.type = pf.b.i32( ao + 0x10 );
			a.duration = pf.b.f32( ao + 0x14 );
			a.numTransformTracks = pf.b.i32( ao + 0x18 );
			a.numFloatTracks = pf.b.i32( ao + 0x1c );
			if ( isInterleaved ) {
				/* +0x38 hkArray<hkQsTransform>, 48 bytes each, FRAME-MAJOR:
				 * element = data + 48 * (frame * numberOfTransformTracks + track).
				 * The object states its frame count NOWHERE -- the engine
				 * divides transforms.size by numberOfTransformTracks
				 * (hkaInterleavedUncompressedAnimation::transformTrack, rva
				 * 0x01fa1ac0; docs/HKX_WRITE_FORMAT.md section 3.1), and so
				 * does this. */
				if ( a.numTransformTracks < 1 )
					refuse( QString( "animation %1 has %2 transform tracks" ).arg( i ).arg( a.numTransformTracks ) );
				int xn = 0;
				const qint64 xp = pf.arr( ao + 0x38, 48, xn );
				if ( xn % a.numTransformTracks )
					refuse( QString( "transforms has %1 elements, not a whole multiple of the %2 transform tracks" )
							.arg( xn ).arg( a.numTransformTracks ) );
				a.numFrames = xn / a.numTransformTracks;
				a.numBlocks = 0;			// an interleaved clip has no blocks
				a.maxFramesPerBlock = 0;
				a.frameDuration = a.numFrames > 1 ? a.duration / float( a.numFrames - 1 ) : 0.0f;
				a.interleaved.resize( 12 * xn );
				for ( int k = 0; k < 12 * xn; k++ )
					a.interleaved[k] = pf.b.f32( xp + 4 * k );
			} else {
			a.numFrames = pf.b.i32( ao + 0x38 );
			a.numBlocks = pf.b.i32( ao + 0x3c );
			a.maxFramesPerBlock = pf.b.i32( ao + 0x40 );
			a.maskAndQuantizationSize = pf.b.i32( ao + 0x44 );
			a.blockDuration = pf.b.f32( ao + 0x48 );
			a.blockInverseDuration = pf.b.f32( ao + 0x4c );
			a.frameDuration = pf.b.f32( ao + 0x50 );
			a.blockOffsets = u32Array( pf, ao + 0x58 );
			a.floatBlockOffsets = u32Array( pf, ao + 0x68 );
			a.transformOffsets = u32Array( pf, ao + 0x78 );
			a.floatOffsets = u32Array( pf, ao + 0x88 );
			int dn = 0;
			const qint64 dp = pf.arr( ao + 0x98, 1, dn );
			if ( dn )
				a.data = QByteArray( reinterpret_cast<const char *>( pf.b.d + dp ), dn );
			a.endian = pf.b.i32( ao + 0xa8 );
			}
			int tn = 0;
			const qint64 tp = pf.arr( ao + 0x28, 0x18, tn );
			for ( int k = 0; k < tn; k++ ) {
				QVector<HkxAnnotation> lst;
				int an = 0;
				const qint64 anp = pf.arr( tp + k * 0x18 + 8, 16, an );
				for ( int j = 0; j < an; j++ ) {
					HkxAnnotation h;
					h.time = pf.b.f32( anp + 16 * j );
					h.text = pf.cstr( anp + 16 * j + 8 );
					lst.append( h );
				}
				a.annotations.append( lst );
			}
			const qint64 em = pf.global.value( ao + 0x20, -1 );
			if ( em >= 0 ) {
				if ( pf.classOf.value( em ) != QLatin1String( "hkaDefaultAnimatedReferenceFrame" ) )
					refuse( QString( "extracted motion class %1 is not decoded" ).arg( pf.classOf.value( em ) ) );
				a.hasMotion = true;
				for ( int k = 0; k < 4; k++ ) {
					a.motion.up[k] = pf.b.f32( em + 0x20 + 4 * k );
					a.motion.forward[k] = pf.b.f32( em + 0x30 + 4 * k );
				}
				a.motion.duration = pf.b.f32( em + 0x40 );
				int rn = 0;
				const qint64 rp = pf.arr( em + 0x48, 16, rn );
				a.motion.samples.resize( 4 * rn );
				for ( int k = 0; k < 4 * rn; k++ )
					a.motion.samples[k] = pf.b.f32( rp + 4 * k );
			}
			raw.anims.append( a );
		}
		// bindings (+0x30)
		const qint64 bp = pf.arr( co + 0x30, 8, n );
		for ( int i = 0; i < n; i++ ) {
			const qint64 bo = pf.global.value( bp + 8 * i, -1 );
			if ( bo < 0 || pf.classOf.value( bo ) != QLatin1String( "hkaAnimationBinding" ) )
				refuse( QString( "binding %1 is a %2" ).arg( i ).arg( pf.classOf.value( bo, "missing object" ) ) );
			RawBinding b;
			b.originalSkeletonName = pf.cstr( bo + 0x10 );
			const qint64 ref = pf.global.value( bo + 0x18, -1 );
			b.animationRef = ( ref >= 0 ) ? QString( "#%1" ).arg( ref - pf.base ) : QString();
			b.trackToBone = i16Array( pf, bo + 0x20 );
			b.floatTrackToSlot = i16Array( pf, bo + 0x30 );
			b.partitionIndices = i16Array( pf, bo + 0x40 );
			static const char * hints[] = { "NORMAL", "ADDITIVE_DEPRECATED", "ADDITIVE" };
			const int h = pf.b.u8( bo + 0x50 );
			b.blendHint = ( h >= 0 && h < 3 ) ? QString( hints[h] ) : QString::number( h );
			raw.bindings.append( b );
		}
		out = finish( raw, name, "packfile" );
	} catch ( const Refusal & r ) {
		out.error = r.what;
	}
	return out;
}

HkxAnimFile hkxAnimLoad( const QString & path )
{
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		HkxAnimFile out;
		out.error = QString( "cannot open %1" ).arg( path );
		return out;
	}
	const QByteArray bytes = f.readAll();
	const QString name = QFileInfo( path ).completeBaseName();
	if ( path.endsWith( ".xml", Qt::CaseInsensitive ) )
		return hkxAnimLoadXml( bytes, name );
	return hkxAnimLoadPackfile( bytes, name );
}
