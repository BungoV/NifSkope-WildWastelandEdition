/* glTF 2.0 animation importer -> HkxAnimClip.  Lane HKX5, 2026-09-10.
   The contract is docs/GLTF_IMPORT.md; every refusal names its field. */

#include "gltfimport.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

// ---------------------------------------------------------------- quaternions
//! A quaternion in glTF / Havok order (x, y, z, w), double, so that a chain of
//! conversions does not spend the float mantissa it is being gated on.
struct Q4
{
	double x = 0.0, y = 0.0, z = 0.0, w = 1.0;
};

Q4 qmul( const Q4 & a, const Q4 & b )
{
	return Q4{ a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
			   a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
			   a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
			   a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z };
}

Q4 qconj( const Q4 & q ) { return Q4{ -q.x, -q.y, -q.z, q.w }; }

double qlen( const Q4 & q ) { return std::sqrt( q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w ); }

Q4 qnorm( const Q4 & q )
{
	const double l = qlen( q );
	if ( !( l > 0.0 ) || !std::isfinite( l ) )
		return Q4{ 0.0, 0.0, 0.0, 1.0 };
	return Q4{ q.x / l, q.y / l, q.z / l, q.w / l };
}

//! glTF's own rule for LINEAR rotation samplers: spherical linear
//! interpolation on the shortest arc (glTF 2.0 spec, "Animations").
Q4 qslerp( Q4 a, Q4 b, double t )
{
	a = qnorm( a );
	b = qnorm( b );
	double d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
	if ( d < 0.0 ) {
		b = Q4{ -b.x, -b.y, -b.z, -b.w };
		d = -d;
	}
	if ( d > 0.9995 ) {
		// the arc is shorter than the float noise: lerp and renormalise
		return qnorm( Q4{ a.x + ( b.x - a.x ) * t, a.y + ( b.y - a.y ) * t,
						  a.z + ( b.z - a.z ) * t, a.w + ( b.w - a.w ) * t } );
	}
	const double th = std::acos( d );
	const double s = std::sin( th );
	const double wa = std::sin( ( 1.0 - t ) * th ) / s;
	const double wb = std::sin( t * th ) / s;
	return qnorm( Q4{ a.x * wa + b.x * wb, a.y * wa + b.y * wb,
					  a.z * wa + b.z * wb, a.w * wa + b.w * wb } );
}

//! Rotate a vector by a quaternion (x,y,z,w).
void qrot( const Q4 & q, double v[3] )
{
	const double tx = 2.0 * ( q.y * v[2] - q.z * v[1] );
	const double ty = 2.0 * ( q.z * v[0] - q.x * v[2] );
	const double tz = 2.0 * ( q.x * v[1] - q.y * v[0] );
	const double rx = v[0] + q.w * tx + ( q.y * tz - q.z * ty );
	const double ry = v[1] + q.w * ty + ( q.z * tx - q.x * tz );
	const double rz = v[2] + q.w * tz + ( q.x * ty - q.y * tx );
	v[0] = rx;
	v[1] = ry;
	v[2] = rz;
}

//! Havok / glTF (x,y,z,w) -> NifSkope's Quat (w,x,y,z), as hkxanim.cpp does.
Quat toQuat( const Q4 & q )
{
	const Q4 n = qnorm( q );
	return Quat( float( n.w ), float( n.x ), float( n.y ), float( n.z ) );
}

// ------------------------------------------------------------------- the file
struct Node
{
	QString name;
	QVector<int> children;
	int parent = -1;
	double t[3] = { 0.0, 0.0, 0.0 };
	Q4 r;
	double s[3] = { 1.0, 1.0, 1.0 };
	bool hasMesh = false;
	bool sawMatrix = false;
};

//! One glTF animation sampler, already dequantised to float.
struct Sampler
{
	QVector<float> in;      //!< key times, seconds, strictly increasing
	QVector<float> out;     //!< count * comps (CUBICSPLINE: 3 * count * comps)
	int comps = 0;          //!< 3 (translation, scale) or 4 (rotation)
	int mode = 0;           //!< 0 LINEAR, 1 STEP, 2 CUBICSPLINE
};

const char * const MODE_NAME[3] = { "LINEAR", "STEP", "CUBICSPLINE" };

int compSize( int ct )
{
	switch ( ct ) {
	case 5120: case 5121: return 1;
	case 5122: case 5123: return 2;
	case 5125: case 5126: return 4;
	default: return 0;
	}
}

int typeComps( const QString & t )
{
	if ( t == QLatin1String( "SCALAR" ) ) return 1;
	if ( t == QLatin1String( "VEC2" ) ) return 2;
	if ( t == QLatin1String( "VEC3" ) ) return 3;
	if ( t == QLatin1String( "VEC4" ) ) return 4;
	if ( t == QLatin1String( "MAT4" ) ) return 16;
	return 0;
}

struct Reader
{
	QJsonObject root;
	QVector<QByteArray> buffers;
	QJsonArray views, accessors;
	QString error;

	bool readAccessor( int idx, QVector<float> & out, int & comps, int & count )
	{
		if ( idx < 0 || idx >= accessors.size() ) {
			error = QStringLiteral( "accessor %1 does not exist (the file has %2)" ).arg( idx ).arg( accessors.size() );
			return false;
		}
		const QJsonObject a = accessors[idx].toObject();
		if ( a.contains( QLatin1String( "sparse" ) ) ) {
			error = QStringLiteral( "accessor %1 is sparse; this importer does not read sparse accessors" ).arg( idx );
			return false;
		}
		const int ct = a.value( QLatin1String( "componentType" ) ).toInt( -1 );
		const int cs = compSize( ct );
		comps = typeComps( a.value( QLatin1String( "type" ) ).toString() );
		count = a.value( QLatin1String( "count" ) ).toInt( -1 );
		const bool normalized = a.value( QLatin1String( "normalized" ) ).toBool( false );
		if ( !cs ) {
			error = QStringLiteral( "accessor %1 has componentType %2, which is not a glTF component type" ).arg( idx ).arg( ct );
			return false;
		}
		if ( !comps ) {
			error = QStringLiteral( "accessor %1 has type '%2', which is not a glTF accessor type" )
				.arg( idx ).arg( a.value( QLatin1String( "type" ) ).toString() );
			return false;
		}
		if ( count <= 0 ) {
			error = QStringLiteral( "accessor %1 has count %2" ).arg( idx ).arg( count );
			return false;
		}
		const int vi = a.value( QLatin1String( "bufferView" ) ).toInt( -1 );
		if ( vi < 0 || vi >= views.size() ) {
			error = QStringLiteral( "accessor %1 names bufferView %2 (the file has %3)" ).arg( idx ).arg( vi ).arg( views.size() );
			return false;
		}
		const QJsonObject v = views[vi].toObject();
		const int bi = v.value( QLatin1String( "buffer" ) ).toInt( -1 );
		if ( bi < 0 || bi >= buffers.size() ) {
			error = QStringLiteral( "bufferView %1 names buffer %2 (the file has %3)" ).arg( vi ).arg( bi ).arg( buffers.size() );
			return false;
		}
		const QByteArray & buf = buffers[bi];
		const qint64 vOff = qint64( v.value( QLatin1String( "byteOffset" ) ).toInt( 0 ) );
		const qint64 vLen = qint64( v.value( QLatin1String( "byteLength" ) ).toInt( 0 ) );
		const qint64 aOff = qint64( a.value( QLatin1String( "byteOffset" ) ).toInt( 0 ) );
		qint64 stride = qint64( v.value( QLatin1String( "byteStride" ) ).toInt( 0 ) );
		const qint64 elem = qint64( cs ) * comps;
		if ( stride <= 0 )
			stride = elem;
		if ( stride < elem ) {
			error = QStringLiteral( "bufferView %1 has byteStride %2, smaller than the %3-byte element of accessor %4" )
				.arg( vi ).arg( stride ).arg( elem ).arg( idx );
			return false;
		}
		const qint64 need = aOff + stride * ( count - 1 ) + elem;
		if ( vOff < 0 || vLen < 0 || vOff + vLen > buf.size() ) {
			error = QStringLiteral( "bufferView %1 spans %2..%3 of a %4-byte buffer" )
				.arg( vi ).arg( vOff ).arg( vOff + vLen ).arg( buf.size() );
			return false;
		}
		if ( need > vLen ) {
			error = QStringLiteral( "accessor %1 needs %2 bytes of bufferView %3, which is %4 bytes" )
				.arg( idx ).arg( need ).arg( vi ).arg( vLen );
			return false;
		}
		out.resize( count * comps );
		const uchar * p = reinterpret_cast<const uchar *>( buf.constData() ) + vOff + aOff;
		for ( int i = 0; i < count; i++ ) {
			const uchar * e = p + stride * i;
			for ( int c = 0; c < comps; c++ ) {
				const uchar * q = e + qint64( cs ) * c;
				double val = 0.0;
				switch ( ct ) {
				case 5126: {
					float f;
					memcpy( &f, q, 4 );
					val = double( f );
					break;
				}
				case 5120: {
					const qint8 s8 = qint8( q[0] );
					val = normalized ? std::fmax( double( s8 ) / 127.0, -1.0 ) : double( s8 );
					break;
				}
				case 5121:
					val = normalized ? double( q[0] ) / 255.0 : double( q[0] );
					break;
				case 5122: {
					const qint16 s16 = qint16( quint16( q[0] ) | ( quint16( q[1] ) << 8 ) );
					val = normalized ? std::fmax( double( s16 ) / 32767.0, -1.0 ) : double( s16 );
					break;
				}
				case 5123: {
					const quint16 u16 = quint16( q[0] ) | ( quint16( q[1] ) << 8 );
					val = normalized ? double( u16 ) / 65535.0 : double( u16 );
					break;
				}
				case 5125: {
					quint32 u32;
					memcpy( &u32, q, 4 );
					val = double( u32 );
					break;
				}
				default:
					break;
				}
				if ( !std::isfinite( val ) ) {
					error = QStringLiteral( "accessor %1 element %2 component %3 is not finite" ).arg( idx ).arg( i ).arg( c );
					return false;
				}
				out[i * comps + c] = float( val );
			}
		}
		return true;
	}
};

//! Sample one glTF sampler at time `t`, glTF's own rules.
void sampleAt( const Sampler & s, double t, double * v )
{
	const int n = int( s.in.size() );
	const int C = s.comps;
	const int stride = ( s.mode == 2 ) ? 3 * C : C;
	auto val = [&]( int k, int part ) -> const float * {
		// part 0 = in-tangent, 1 = value, 2 = out-tangent (CUBICSPLINE only)
		return s.out.constData() + qint64( k ) * stride + ( ( s.mode == 2 ) ? part * C : 0 );
	};
	if ( n == 0 ) {
		for ( int c = 0; c < C; c++ )
			v[c] = 0.0;
		return;
	}
	if ( n == 1 || t <= double( s.in[0] ) ) {
		const float * p = val( 0, 1 );
		for ( int c = 0; c < C; c++ )
			v[c] = double( p[c] );
		return;
	}
	if ( t >= double( s.in[n - 1] ) ) {
		const float * p = val( n - 1, 1 );
		for ( int c = 0; c < C; c++ )
			v[c] = double( p[c] );
		return;
	}
	// binary search for k with in[k] <= t < in[k+1]
	int lo = 0, hi = n - 1;
	while ( hi - lo > 1 ) {
		const int mid = ( lo + hi ) / 2;
		if ( double( s.in[mid] ) <= t )
			lo = mid;
		else
			hi = mid;
	}
	const double t0 = double( s.in[lo] ), t1 = double( s.in[lo + 1] );
	const double td = t1 - t0;
	const double u = ( td > 0.0 ) ? ( t - t0 ) / td : 0.0;
	if ( s.mode == 1 ) {                       // STEP
		const float * p = val( lo, 1 );
		for ( int c = 0; c < C; c++ )
			v[c] = double( p[c] );
		return;
	}
	if ( s.mode == 2 ) {                       // CUBICSPLINE, the spec's Hermite
		const float * p0 = val( lo, 1 );
		const float * m0 = val( lo, 2 );       // out-tangent of key lo
		const float * p1 = val( lo + 1, 1 );
		const float * m1 = val( lo + 1, 0 );   // in-tangent of key lo+1
		const double u2 = u * u, u3 = u2 * u;
		const double h00 = 2.0 * u3 - 3.0 * u2 + 1.0;
		const double h10 = u3 - 2.0 * u2 + u;
		const double h01 = -2.0 * u3 + 3.0 * u2;
		const double h11 = u3 - u2;
		for ( int c = 0; c < C; c++ )
			v[c] = h00 * double( p0[c] ) + h10 * td * double( m0[c] )
				 + h01 * double( p1[c] ) + h11 * td * double( m1[c] );
		return;
	}
	// LINEAR
	const float * p0 = val( lo, 1 );
	const float * p1 = val( lo + 1, 1 );
	if ( C == 4 ) {
		const Q4 q = qslerp( Q4{ p0[0], p0[1], p0[2], p0[3] }, Q4{ p1[0], p1[1], p1[2], p1[3] }, u );
		v[0] = q.x; v[1] = q.y; v[2] = q.z; v[3] = q.w;
		return;
	}
	for ( int c = 0; c < C; c++ )
		v[c] = double( p0[c] ) + ( double( p1[c] ) - double( p0[c] ) ) * u;
}

} // namespace

QString GltfImportReport::summary() const
{
	if ( !error.isEmpty() )
		return QStringLiteral( "refused: " ) + error;
	QString s = QStringLiteral( "%1: %2 of %3 nodes mapped to bones, %4 animated, %5 channels, %6 static tracks; %7; %8; %9" )
		.arg( container ).arg( matched.size() ).arg( gltfNodes ).arg( animatedNodes )
		.arg( channelsRead ).arg( staticTracks ).arg( upAxisArm ).arg( rateArm ).arg( mappingArm );
	if ( !unitArm.isEmpty() )
		s += QStringLiteral( "; units: " ) + unitArm;
	if ( !unmatchedNodes.isEmpty() )
		s += QStringLiteral( "; %1 node(s) reached no bone: %2" )
			.arg( unmatchedNodes.size() ).arg( unmatchedNodes.join( QLatin1String( ", " ) ) );
	if ( rootMotionExtracted )
		s += QStringLiteral( "; root motion lifted off '%1' (max %2 units, %3 deg)" )
			.arg( rootMotionNode ).arg( double( rootMotionMaxTranslation ), 0, 'f', 4 )
			.arg( double( rootMotionMaxYawDeg ), 0, 'f', 4 );
	return s;
}

// ---------------------------------------------------------------------------

bool gltfImportParse( const QByteArray & blob, const QString & name,
					  const GltfImportOptions & optIn, HkxAnimClip & clip,
					  GltfImportReport & rep,
					  const QHash<QString, QByteArray> & externalBuffers )
{
	rep = GltfImportReport();
	clip = HkxAnimClip();

	// A LOCAL copy, because one field of it -- the unit scale -- may be
	// overruled by the file itself a few dozen lines below (finding (9)). The
	// caller's struct is never touched.
	GltfImportOptions opt = optIn;

	auto refuse = [&rep]( const QString & why ) -> bool {
		rep.error = why;
		return false;
	};

	if ( !( opt.unitScale > 0.0f ) || !std::isfinite( opt.unitScale ) )
		return refuse( QStringLiteral( "unit scale is %1 metres per unit" ).arg( double( opt.unitScale ) ) );
	if ( !opt.preserveSourceRate && !( opt.targetFps > 0.0f && opt.targetFps <= 1000.0f ) )
		return refuse( QStringLiteral( "target rate is %1 frames per second" ).arg( double( opt.targetFps ) ) );

	// ---- container: .glb or .gltf ----------------------------------------
	QByteArray json;
	QByteArray glbBin;
	if ( blob.size() >= 12 && quint8( blob[0] ) == 'g' && quint8( blob[1] ) == 'l'
		 && quint8( blob[2] ) == 'T' && quint8( blob[3] ) == 'F' ) {
		quint32 ver = 0, len = 0;
		memcpy( &ver, blob.constData() + 4, 4 );
		memcpy( &len, blob.constData() + 8, 4 );
		if ( ver != 2 )
			return refuse( QStringLiteral( "the .glb header says version %1; this importer reads glTF 2" ).arg( ver ) );
		if ( qint64( len ) > blob.size() )
			return refuse( QStringLiteral( "the .glb header says %1 bytes, the file is %2" ).arg( len ).arg( blob.size() ) );
		qint64 p = 12;
		while ( p + 8 <= qint64( len ) ) {
			quint32 clen = 0, ctype = 0;
			memcpy( &clen, blob.constData() + p, 4 );
			memcpy( &ctype, blob.constData() + p + 4, 4 );
			const qint64 cdata = p + 8;
			if ( cdata + qint64( clen ) > qint64( len ) )
				return refuse( QStringLiteral( "a .glb chunk at %1 claims %2 bytes, past the %3-byte file" )
							   .arg( p ).arg( clen ).arg( len ) );
			if ( ctype == 0x4E4F534A )          // 'JSON'
				json = blob.mid( int( cdata ), int( clen ) );
			else if ( ctype == 0x004E4942 )     // 'BIN\0'
				glbBin = blob.mid( int( cdata ), int( clen ) );
			p = cdata + ( ( qint64( clen ) + 3 ) & ~qint64( 3 ) );
		}
		if ( json.isEmpty() )
			return refuse( QStringLiteral( "the .glb carries no JSON chunk" ) );
		rep.container = QStringLiteral( "glb" );
	} else {
		json = blob;
		rep.container = QStringLiteral( "gltf+bin" );
	}

	QJsonParseError perr;
	const QJsonDocument doc = QJsonDocument::fromJson( json, &perr );
	if ( doc.isNull() || !doc.isObject() )
		return refuse( QStringLiteral( "the JSON does not parse: %1 at offset %2" ).arg( perr.errorString() ).arg( perr.offset ) );

	Reader rd;
	rd.root = doc.object();
	{
		const QJsonObject asset = rd.root.value( QLatin1String( "asset" ) ).toObject();
		const QString v = asset.value( QLatin1String( "version" ) ).toString();
		if ( !v.startsWith( QLatin1String( "2." ) ) )
			return refuse( QStringLiteral( "asset.version is '%1'; this importer reads glTF 2.x" ).arg( v ) );

		// ---- (9) the round trip: the file states its own unit scale --------
		const QJsonValue mpu = asset.value( QLatin1String( "extras" ) ).toObject()
									.value( QLatin1String( "metresPerUnit" ) );
		if ( !opt.unitScaleFromFile ) {
			rep.unitArm = QStringLiteral( "the caller's unit scale was forced (%1 m/unit); "
										  "asset.extras.metresPerUnit was not consulted" )
				.arg( double( opt.unitScale ), 0, 'g', 10 );
		} else if ( mpu.isDouble() ) {
			const double m = mpu.toDouble();
			if ( !( m > 0.0 ) || !std::isfinite( m ) )
				return refuse( QStringLiteral( "asset.extras.metresPerUnit is %1; it must be a positive number" ).arg( m ) );
			opt.unitScale = float( m );
			rep.unitArm = QStringLiteral( "the file states %1 m/unit (%2 units per metre)" )
				.arg( m, 0, 'g', 10 ).arg( 1.0 / m, 0, 'f', 6 );
		} else {
			rep.unitArm = QStringLiteral( "the file states no asset.extras.metresPerUnit, so the "
										  "default %1 m/unit was used" )
				.arg( double( opt.unitScale ), 0, 'g', 10 );
		}
		rep.unitScaleUsed = opt.unitScale;
	}
	rd.views = rd.root.value( QLatin1String( "bufferViews" ) ).toArray();
	rd.accessors = rd.root.value( QLatin1String( "accessors" ) ).toArray();

	// ---- buffers ---------------------------------------------------------
	{
		const QJsonArray ba = rd.root.value( QLatin1String( "buffers" ) ).toArray();
		for ( int i = 0; i < ba.size(); i++ ) {
			const QJsonObject b = ba[i].toObject();
			const QString uri = b.value( QLatin1String( "uri" ) ).toString();
			const int wantLen = b.value( QLatin1String( "byteLength" ) ).toInt( -1 );
			QByteArray bytes;
			if ( uri.isEmpty() ) {
				if ( glbBin.isEmpty() )
					return refuse( QStringLiteral( "buffer %1 has no uri and the file carries no GLB binary chunk" ).arg( i ) );
				bytes = glbBin;
			} else if ( uri.startsWith( QLatin1String( "data:" ) ) ) {
				const int comma = uri.indexOf( QLatin1Char( ',' ) );
				if ( comma < 0 || !uri.left( comma ).contains( QLatin1String( "base64" ) ) )
					return refuse( QStringLiteral( "buffer %1 has a data: uri that is not base64" ).arg( i ) );
				bytes = QByteArray::fromBase64( uri.mid( comma + 1 ).toLatin1() );
				rep.container = QStringLiteral( "gltf (embedded base64)" );
			} else {
				const QString key = QUrl::fromPercentEncoding( uri.toUtf8() );
				if ( !externalBuffers.contains( key ) && !externalBuffers.contains( uri ) )
					return refuse( QStringLiteral( "buffer %1 needs the external file '%2', which was not supplied" ).arg( i ).arg( key ) );
				bytes = externalBuffers.contains( key ) ? externalBuffers.value( key ) : externalBuffers.value( uri );
			}
			if ( wantLen >= 0 && bytes.size() < wantLen )
				return refuse( QStringLiteral( "buffer %1 says byteLength %2, %3 bytes were read" ).arg( i ).arg( wantLen ).arg( bytes.size() ) );
			rd.buffers.append( bytes );
		}
	}

	// ---- nodes -----------------------------------------------------------
	const QJsonArray ja = rd.root.value( QLatin1String( "nodes" ) ).toArray();
	const int nn = ja.size();
	if ( nn <= 0 )
		return refuse( QStringLiteral( "the file has no nodes" ) );
	rep.gltfNodes = nn;
	QVector<Node> nodes( nn );
	for ( int i = 0; i < nn; i++ ) {
		const QJsonObject o = ja[i].toObject();
		Node & n = nodes[i];
		n.name = o.value( QLatin1String( "name" ) ).toString();
		if ( n.name.isEmpty() )
			n.name = QStringLiteral( "node%1" ).arg( i );
		n.hasMesh = o.contains( QLatin1String( "mesh" ) );
		for ( const QJsonValue & c : o.value( QLatin1String( "children" ) ).toArray() ) {
			const int ci = c.toInt( -1 );
			if ( ci < 0 || ci >= nn )
				return refuse( QStringLiteral( "node %1 '%2' names child %3 (the file has %4 nodes)" ).arg( i ).arg( n.name ).arg( ci ).arg( nn ) );
			if ( ci == i )
				return refuse( QStringLiteral( "node %1 '%2' is its own child" ).arg( i ).arg( n.name ) );
			n.children.append( ci );
		}
		if ( o.contains( QLatin1String( "matrix" ) ) ) {
			const QJsonArray m = o.value( QLatin1String( "matrix" ) ).toArray();
			if ( m.size() != 16 )
				return refuse( QStringLiteral( "node %1 '%2' has a matrix of %3 numbers" ).arg( i ).arg( n.name ).arg( m.size() ) );
			double M[16];
			for ( int k = 0; k < 16; k++ )
				M[k] = m[k].toDouble();
			// column-major: columns 0,1,2 are the basis, column 3 the translation
			n.t[0] = M[12]; n.t[1] = M[13]; n.t[2] = M[14];
			double col[3][3];
			for ( int c = 0; c < 3; c++ )
				for ( int r = 0; r < 3; r++ )
					col[c][r] = M[c * 4 + r];
			for ( int c = 0; c < 3; c++ ) {
				n.s[c] = std::sqrt( col[c][0] * col[c][0] + col[c][1] * col[c][1] + col[c][2] * col[c][2] );
				if ( !( n.s[c] > 1e-12 ) )
					return refuse( QStringLiteral( "node %1 '%2' has a matrix with a zero-length axis %3" ).arg( i ).arg( n.name ).arg( c ) );
				for ( int r = 0; r < 3; r++ )
					col[c][r] /= n.s[c];
			}
			const double det = col[0][0] * ( col[1][1] * col[2][2] - col[1][2] * col[2][1] )
							 - col[1][0] * ( col[0][1] * col[2][2] - col[0][2] * col[2][1] )
							 + col[2][0] * ( col[0][1] * col[1][2] - col[0][2] * col[1][1] );
			if ( det < 0.0 )
				return refuse( QStringLiteral( "node %1 '%2' has a mirroring matrix (determinant %3); a bone transform cannot be mirrored" )
							   .arg( i ).arg( n.name ).arg( det ) );
			// orthonormality: a sheared matrix is not a TRS and must not be guessed at
			for ( int a = 0; a < 3; a++ ) {
				for ( int b = a + 1; b < 3; b++ ) {
					const double d = col[a][0] * col[b][0] + col[a][1] * col[b][1] + col[a][2] * col[b][2];
					if ( std::fabs( d ) > 1e-4 )
						return refuse( QStringLiteral( "node %1 '%2' has a sheared matrix (axes %3 and %4 dot to %5)" )
									   .arg( i ).arg( n.name ).arg( a ).arg( b ).arg( d ) );
				}
			}
			const double tr = col[0][0] + col[1][1] + col[2][2];
			if ( tr > 0.0 ) {
				const double sq = std::sqrt( tr + 1.0 ) * 2.0;
				n.r = Q4{ ( col[1][2] - col[2][1] ) / sq, ( col[2][0] - col[0][2] ) / sq,
						  ( col[0][1] - col[1][0] ) / sq, 0.25 * sq };
			} else if ( col[0][0] > col[1][1] && col[0][0] > col[2][2] ) {
				const double sq = std::sqrt( 1.0 + col[0][0] - col[1][1] - col[2][2] ) * 2.0;
				n.r = Q4{ 0.25 * sq, ( col[1][0] + col[0][1] ) / sq,
						  ( col[2][0] + col[0][2] ) / sq, ( col[1][2] - col[2][1] ) / sq };
			} else if ( col[1][1] > col[2][2] ) {
				const double sq = std::sqrt( 1.0 + col[1][1] - col[0][0] - col[2][2] ) * 2.0;
				n.r = Q4{ ( col[1][0] + col[0][1] ) / sq, 0.25 * sq,
						  ( col[2][1] + col[1][2] ) / sq, ( col[2][0] - col[0][2] ) / sq };
			} else {
				const double sq = std::sqrt( 1.0 + col[2][2] - col[0][0] - col[1][1] ) * 2.0;
				n.r = Q4{ ( col[2][0] + col[0][2] ) / sq, ( col[2][1] + col[1][2] ) / sq,
						  0.25 * sq, ( col[0][1] - col[1][0] ) / sq };
			}
			n.r = qnorm( n.r );
			n.sawMatrix = true;
		} else {
			const QJsonArray t = o.value( QLatin1String( "translation" ) ).toArray();
			if ( t.size() == 3 )
				for ( int k = 0; k < 3; k++ )
					n.t[k] = t[k].toDouble();
			const QJsonArray r = o.value( QLatin1String( "rotation" ) ).toArray();
			if ( r.size() == 4 )
				n.r = qnorm( Q4{ r[0].toDouble(), r[1].toDouble(), r[2].toDouble(), r[3].toDouble() } );
			const QJsonArray s = o.value( QLatin1String( "scale" ) ).toArray();
			if ( s.size() == 3 )
				for ( int k = 0; k < 3; k++ )
					n.s[k] = s[k].toDouble();
		}
		for ( int k = 0; k < 3; k++ )
			if ( !std::isfinite( n.t[k] ) || !std::isfinite( n.s[k] ) )
				return refuse( QStringLiteral( "node %1 '%2' has a non-finite translation or scale" ).arg( i ).arg( n.name ) );
	}
	for ( int i = 0; i < nn; i++ ) {
		for ( int c : nodes[i].children ) {
			if ( nodes[c].parent >= 0 )
				return refuse( QStringLiteral( "node %1 '%2' has two parents (%3 and %4)" )
							   .arg( c ).arg( nodes[c].name ).arg( nodes[c].parent ).arg( i ) );
			nodes[c].parent = i;
		}
	}
	{	// a cycle would make "root" meaningless
		for ( int i = 0; i < nn; i++ ) {
			int p = nodes[i].parent, guard = 0;
			while ( p >= 0 ) {
				if ( p == i || ++guard > nn )
					return refuse( QStringLiteral( "the node hierarchy has a cycle through node %1 '%2'" ).arg( i ).arg( nodes[i].name ) );
				p = nodes[p].parent;
			}
		}
	}

	QVector<int> roots;
	{
		const QJsonArray scenes = rd.root.value( QLatin1String( "scenes" ) ).toArray();
		const int si = rd.root.value( QLatin1String( "scene" ) ).toInt( 0 );
		if ( si >= 0 && si < scenes.size() ) {
			for ( const QJsonValue & v : scenes[si].toObject().value( QLatin1String( "nodes" ) ).toArray() ) {
				const int r = v.toInt( -1 );
				if ( r < 0 || r >= nn )
					return refuse( QStringLiteral( "scene %1 names node %2 (the file has %3 nodes)" ).arg( si ).arg( r ).arg( nn ) );
				roots.append( r );
			}
		}
		if ( roots.isEmpty() )
			for ( int i = 0; i < nn; i++ )
				if ( nodes[i].parent < 0 )
					roots.append( i );
		if ( roots.isEmpty() )
			return refuse( QStringLiteral( "the file has no root node" ) );
	}

	// ---- the animation ---------------------------------------------------
	const QJsonArray anims = rd.root.value( QLatin1String( "animations" ) ).toArray();
	if ( anims.isEmpty() )
		return refuse( QStringLiteral( "the file carries no animation" ) );
	if ( opt.animationIndex < 0 || opt.animationIndex >= anims.size() )
		return refuse( QStringLiteral( "animation %1 was asked for; the file has %2" ).arg( opt.animationIndex ).arg( anims.size() ) );
	const QJsonObject an = anims[opt.animationIndex].toObject();
	const QJsonArray jsamp = an.value( QLatin1String( "samplers" ) ).toArray();
	const QJsonArray jchan = an.value( QLatin1String( "channels" ) ).toArray();
	if ( jchan.isEmpty() )
		return refuse( QStringLiteral( "animation %1 has no channels" ).arg( opt.animationIndex ) );

	// per node, the sampler index of each path (-1 = none)
	QVector<int> chT( nn, -1 ), chR( nn, -1 ), chS( nn, -1 );
	QSet<int> usedSamplers;
	QStringList ignoredPaths;
	for ( int c = 0; c < jchan.size(); c++ ) {
		const QJsonObject ch = jchan[c].toObject();
		const int si = ch.value( QLatin1String( "sampler" ) ).toInt( -1 );
		const QJsonObject tg = ch.value( QLatin1String( "target" ) ).toObject();
		const QString path = tg.value( QLatin1String( "path" ) ).toString();
		if ( !tg.contains( QLatin1String( "node" ) ) )
			continue;                       // the spec allows a target with no node; nothing to drive
		const int node = tg.value( QLatin1String( "node" ) ).toInt( -1 );
		if ( node < 0 || node >= nn )
			return refuse( QStringLiteral( "channel %1 targets node %2 (the file has %3 nodes)" ).arg( c ).arg( node ).arg( nn ) );
		if ( si < 0 || si >= jsamp.size() )
			return refuse( QStringLiteral( "channel %1 names sampler %2 (the animation has %3)" ).arg( c ).arg( si ).arg( jsamp.size() ) );
		if ( path == QLatin1String( "translation" ) )
			chT[node] = si;
		else if ( path == QLatin1String( "rotation" ) )
			chR[node] = si;
		else if ( path == QLatin1String( "scale" ) )
			chS[node] = si;
		else {
			const QString w = QStringLiteral( "%1 on '%2'" ).arg( path.isEmpty() ? QStringLiteral( "(no path)" ) : path ).arg( nodes[node].name );
			if ( !ignoredPaths.contains( w ) )
				ignoredPaths.append( w );
			continue;
		}
		usedSamplers.insert( si );
		rep.channelsRead++;
	}
	if ( !usedSamplers.size() )
		return refuse( QStringLiteral( "animation %1 drives no node's translation, rotation or scale" ).arg( opt.animationIndex ) );

	QHash<int, Sampler> samplers;
	for ( int si : usedSamplers ) {
		const QJsonObject o = jsamp[si].toObject();
		Sampler s;
		const QString ip = o.value( QLatin1String( "interpolation" ) ).toString( QStringLiteral( "LINEAR" ) );
		if ( ip == QLatin1String( "LINEAR" ) )
			s.mode = 0;
		else if ( ip == QLatin1String( "STEP" ) )
			s.mode = 1;
		else if ( ip == QLatin1String( "CUBICSPLINE" ) )
			s.mode = 2;
		else
			return refuse( QStringLiteral( "sampler %1 has interpolation '%2'; glTF 2.0 defines LINEAR, STEP and CUBICSPLINE" ).arg( si ).arg( ip ) );
		if ( !rep.interpolations.contains( QLatin1String( MODE_NAME[s.mode] ) ) )
			rep.interpolations.append( QLatin1String( MODE_NAME[s.mode] ) );
		int ic = 0, icount = 0;
		if ( !rd.readAccessor( o.value( QLatin1String( "input" ) ).toInt( -1 ), s.in, ic, icount ) )
			return refuse( QStringLiteral( "sampler %1 input: %2" ).arg( si ).arg( rd.error ) );
		if ( ic != 1 )
			return refuse( QStringLiteral( "sampler %1 input has %2 components; key times are SCALAR" ).arg( si ).arg( ic ) );
		int oc = 0, ocount = 0;
		if ( !rd.readAccessor( o.value( QLatin1String( "output" ) ).toInt( -1 ), s.out, oc, ocount ) )
			return refuse( QStringLiteral( "sampler %1 output: %2" ).arg( si ).arg( rd.error ) );
		s.comps = oc;
		const int want = ( s.mode == 2 ) ? 3 * icount : icount;
		if ( ocount != want )
			return refuse( QStringLiteral( "sampler %1 has %2 key times and %3 output elements; %4 wants %5" )
						   .arg( si ).arg( icount ).arg( ocount ).arg( QLatin1String( MODE_NAME[s.mode] ) ).arg( want ) );
		for ( int k = 1; k < s.in.size(); k++ )
			if ( !( s.in[k] > s.in[k - 1] ) )
				return refuse( QStringLiteral( "sampler %1 key time %2 is %3, not after %4" )
							   .arg( si ).arg( k ).arg( double( s.in[k] ) ).arg( double( s.in[k - 1] ) ) );
		samplers.insert( si, s );
	}
	rep.interpolations.sort();

	// every sampler must have the component count its path needs
	for ( int i = 0; i < nn; i++ ) {
		if ( chT[i] >= 0 && samplers[chT[i]].comps != 3 )
			return refuse( QStringLiteral( "the translation sampler of node '%1' has %2 components, not 3" ).arg( nodes[i].name ).arg( samplers[chT[i]].comps ) );
		if ( chR[i] >= 0 && samplers[chR[i]].comps != 4 )
			return refuse( QStringLiteral( "the rotation sampler of node '%1' has %2 components, not 4" ).arg( nodes[i].name ).arg( samplers[chR[i]].comps ) );
		if ( chS[i] >= 0 && samplers[chS[i]].comps != 3 )
			return refuse( QStringLiteral( "the scale sampler of node '%1' has %2 components, not 3" ).arg( nodes[i].name ).arg( samplers[chS[i]].comps ) );
		if ( chT[i] >= 0 || chR[i] >= 0 || chS[i] >= 0 )
			rep.animatedNodes++;
	}

	// ---- the up axis ------------------------------------------------------
	const double S = std::sqrt( 0.5 );
	const Q4 EXPORT_UP = Q4{ -S, 0.0, 0.0, S };            // what gltfexport writes
	const Q4 UNDO_UP = Q4{ S, 0.0, 0.0, S };               // +90 deg about X
	int consumed = -1;
	if ( opt.convertUpAxis ) {
		for ( int r : roots ) {
			const Node & n = nodes[r];
			const bool byName = !opt.upAxisNodeName.isEmpty() && n.name == opt.upAxisNodeName;
			const bool byShape = !n.children.isEmpty() && !n.hasMesh
				&& std::fabs( n.t[0] ) < 1e-6 && std::fabs( n.t[1] ) < 1e-6 && std::fabs( n.t[2] ) < 1e-6
				&& std::fabs( n.s[0] - 1.0 ) < 1e-6 && std::fabs( n.s[1] - 1.0 ) < 1e-6 && std::fabs( n.s[2] - 1.0 ) < 1e-6
				&& std::fabs( n.r.x - EXPORT_UP.x ) < 1e-5 && std::fabs( n.r.y ) < 1e-5
				&& std::fabs( n.r.z ) < 1e-5 && std::fabs( n.r.w - EXPORT_UP.w ) < 1e-5;
			if ( !byName && !byShape )
				continue;
			if ( consumed >= 0 )
				return refuse( QStringLiteral( "two scene roots look like the up-axis node ('%1' and '%2'); which one converts the axis cannot be guessed" )
							   .arg( nodes[consumed].name ).arg( n.name ) );
			if ( chT[r] >= 0 || chR[r] >= 0 || chS[r] >= 0 )
				return refuse( QStringLiteral( "the up-axis node '%1' carries animation channels; consuming it would drop them" ).arg( n.name ) );
			consumed = r;
		}
	}
	QVector<int> nifRoots;
	if ( consumed >= 0 ) {
		nifRoots = nodes[consumed].children;
		rep.upAxisArm = QStringLiteral( "consumed the up-axis node '%1' (%2 root%3 under it)" )
			.arg( nodes[consumed].name ).arg( nifRoots.size() ).arg( nifRoots.size() == 1 ? QString() : QStringLiteral( "s" ) );
	} else {
		nifRoots = roots;
		rep.upAxisArm = opt.convertUpAxis
			? QStringLiteral( "no up-axis node in the file: applied +90 degrees about X to %1 scene root%2" )
				.arg( nifRoots.size() ).arg( nifRoots.size() == 1 ? QString() : QStringLiteral( "s" ) )
			: QStringLiteral( "axis conversion off: the glTF's own Y-up frame is kept" );
	}
	QVector<bool> isNifRoot( nn, false );
	for ( int r : nifRoots )
		isNifRoot[r] = true;

	// which nodes take part at all (everything under a NIF root, up-axis node out)
	QVector<bool> inTree( nn, false );
	{
		QVector<int> stack = nifRoots;
		while ( !stack.isEmpty() ) {
			const int i = stack.takeLast();
			if ( inTree[i] )
				continue;
			inTree[i] = true;
			for ( int c : nodes[i].children )
				stack.append( c );
		}
	}

	// ---- node -> bone -----------------------------------------------------
	QStringList boneNames = opt.skeletonBoneNames;
	QVector<int> nodeBone( nn, -1 );
	if ( boneNames.isEmpty() ) {
		for ( int i = 0; i < nn; i++ )
			if ( inTree[i] ) {
				nodeBone[i] = boneNames.size();
				boneNames.append( nodes[i].name );
			}
		rep.mappingArm = QStringLiteral( "no target skeleton was given: the glTF's own %1 node names are the bone names, mapping is the identity" )
			.arg( boneNames.size() );
	} else {
		QVector<int> boneNode( boneNames.size(), -1 );
		int exact = 0, folded = 0, partial = 0;

		/* THREE ARMS, EACH FINISHED BEFORE THE NEXT BEGINS.  Doing them
		   per node instead lets a partial match claim a bone that a later
		   node matches EXACTLY: on skeleton.nif, 'CamTargetParent' contains
		   'CamTarget', and node order put it first, so the real CamTarget
		   node lost its bone (measured in round trip 2, 2026-09-10).  An
		   ANIMATED node also outranks a still one for the same bone, because
		   a still one carries no more than the bind pose. */
		QVector<int> order;
		for ( int i = 0; i < nn; i++ )
			if ( inTree[i] && !nodes[i].name.isEmpty() )
				order.append( i );
		auto animatedNode = [&]( int i ) { return chT[i] >= 0 || chR[i] >= 0 || chS[i] >= 0; };
		std::stable_sort( order.begin(), order.end(), [&]( int a, int b ) {
			return animatedNode( a ) && !animatedNode( b );
		} );
		QVector<bool> taken( nn, false );

		auto claim = [&]( int i, int b, int arm ) {
			boneNode[b] = i;
			nodeBone[i] = b;
			taken[i] = true;
			const QString nm = nodes[i].name;
			if ( arm == 1 ) exact++;
			else if ( arm == 2 ) { folded++; rep.caseFolded.append( QStringLiteral( "%1 -> %2" ).arg( nm ).arg( boneNames[b] ) ); }
			else { partial++; rep.partialMatched.append( QStringLiteral( "%1 -> %2" ).arg( nm ).arg( boneNames[b] ) ); }
		};

		for ( int arm = 1; arm <= 3; arm++ ) {
			for ( int i : order ) {
				if ( taken[i] )
					continue;
				const QString nm = nodes[i].name;
				if ( arm == 1 ) {
					for ( int b = 0; b < boneNames.size(); b++ )
						if ( boneNode[b] < 0 && boneNames[b] == nm ) { claim( i, b, 1 ); break; }
				} else if ( arm == 2 ) {
					for ( int b = 0; b < boneNames.size(); b++ )
						if ( boneNode[b] < 0 && !boneNames[b].compare( nm, Qt::CaseInsensitive ) ) { claim( i, b, 2 ); break; }
				} else {
					int cand = -1, ncand = 0;
					for ( int b = 0; b < boneNames.size(); b++ ) {
						if ( boneNode[b] >= 0 )
							continue;
						if ( boneNames[b].contains( nm, Qt::CaseInsensitive ) || nm.contains( boneNames[b], Qt::CaseInsensitive ) ) {
							cand = b;
							ncand++;
						}
					}
					if ( ncand == 1 )
						claim( i, cand, 3 );
					else if ( ncand > 1 ) {
						rep.ambiguous.append( nm );
						taken[i] = true;
						rep.unmatchedNodes.append( QStringLiteral( "%1 (matches %2 free bones partially)" ).arg( nm ).arg( ncand ) );
					}
				}
			}
		}
		for ( int i : order )
			if ( !taken[i] )
				rep.unmatchedNodes.append( nodes[i].name );
		for ( int b = 0; b < boneNames.size(); b++ )
			if ( boneNode[b] < 0 )
				rep.unmatchedBones.append( boneNames[b] );
		rep.mappingArm = QStringLiteral( "%1 bone(s) by name, %2 by case, %3 partially, of %4 skeleton bones" )
			.arg( exact ).arg( folded ).arg( partial ).arg( boneNames.size() );
		if ( !exact && !folded && !partial )
			return refuse( QStringLiteral( "not one of the %1 glTF node names reaches a bone of the %2-bone skeleton" )
						   .arg( rep.gltfNodes ).arg( boneNames.size() ) );
	}

	// tracks: one per mapped node, ordered by bone index (the binding wants that)
	QVector<QPair<int, int>> tracks;             // (bone, node)
	for ( int i = 0; i < nn; i++ ) {
		if ( nodeBone[i] < 0 )
			continue;
		const bool animated = ( chT[i] >= 0 || chR[i] >= 0 || chS[i] >= 0 );
		if ( !animated && !opt.includeStaticTracks )
			continue;
		tracks.append( qMakePair( nodeBone[i], i ) );
		if ( !animated )
			rep.staticTracks++;
	}
	if ( tracks.isEmpty() )
		return refuse( QStringLiteral( "no glTF node both maps to a bone and carries an animation channel" ) );
	std::sort( tracks.begin(), tracks.end() );
	for ( const auto & tr : tracks )
		rep.matched.append( QStringLiteral( "%1 -> %2" ).arg( nodes[tr.second].name ).arg( boneNames[tr.first] ) );

	// ---- the frame grid ---------------------------------------------------
	double t0 = 0.0, t1 = 0.0;
	bool first = true;
	for ( auto it = samplers.constBegin(); it != samplers.constEnd(); ++it ) {
		if ( it->in.isEmpty() )
			continue;
		const double a = double( it->in.first() ), b = double( it->in.last() );
		if ( first ) { t0 = a; t1 = b; first = false; }
		else { t0 = std::fmin( t0, a ); t1 = std::fmax( t1, b ); }
	}
	if ( first )
		return refuse( QStringLiteral( "every sampler the animation uses has no key times" ) );
	rep.sourceFirstKey = float( t0 );
	rep.sourceLastKey = float( t1 );

	// is every used sampler on ONE uniform grid?
	{
		const Sampler ref = samplers.constBegin().value();
		rep.sourceKeys = int( ref.in.size() );
		bool same = ref.in.size() >= 2;
		for ( auto it = samplers.constBegin(); same && it != samplers.constEnd(); ++it ) {
			if ( it->in.size() != ref.in.size() ) { same = false; break; }
			for ( int k = 0; k < ref.in.size(); k++ )
				if ( std::fabs( double( it->in[k] ) - double( ref.in[k] ) ) > 1e-6 ) { same = false; break; }
		}
		if ( same ) {
			const double step = double( ref.in[1] ) - double( ref.in[0] );
			bool uniform = step > 1e-9;
			for ( int k = 1; uniform && k + 1 < ref.in.size(); k++ )
				if ( std::fabs( ( double( ref.in[k + 1] ) - double( ref.in[k] ) ) - step ) > 1e-5 * std::fmax( 1.0, step ) )
					uniform = false;
			rep.sourceUniform = uniform;
			if ( uniform )
				rep.sourceFps = float( 1.0 / step );
		}
	}

	int numFrames = 0;
	double frameDuration = 0.0;
	QVector<double> times;
	if ( opt.preserveSourceRate && rep.sourceUniform ) {
		const Sampler ref = samplers.constBegin().value();
		numFrames = int( ref.in.size() );
		frameDuration = ( double( ref.in.last() ) - double( ref.in.first() ) ) / double( numFrames - 1 );
		for ( int i = 0; i < numFrames; i++ )
			times.append( double( ref.in[i] ) );
		rep.rateArm = QStringLiteral( "the source's own %1 fps kept: %2 key times used unchanged, nothing resampled" )
			.arg( double( rep.sourceFps ), 0, 'f', 4 ).arg( numFrames );
	} else {
		const double fps = double( opt.targetFps );
		frameDuration = 1.0 / fps;
		const double span = std::fmax( 0.0, t1 - t0 );
		numFrames = int( std::lround( span * fps ) ) + 1;
		if ( numFrames < 2 )
			numFrames = 2;              // FO4 stores a one-frame pose as two frames
		if ( numFrames > 65535 )
			return refuse( QStringLiteral( "%1 seconds at %2 fps is %3 frames; the writer caps a clip at 65535" )
						   .arg( span ).arg( fps ).arg( numFrames ) );
		for ( int i = 0; i < numFrames; i++ )
			times.append( t0 + double( i ) * frameDuration );
		rep.durationDelta = float( std::fabs( double( numFrames - 1 ) * frameDuration - span ) );
		rep.rateArm = QStringLiteral( "resampled %1 s of keys to %2 frames at %3 fps%4" )
			.arg( span, 0, 'f', 6 ).arg( numFrames ).arg( fps, 0, 'f', 4 )
			.arg( opt.preserveSourceRate
				  ? QStringLiteral( " (the source rate was asked for but its key times are not one uniform grid)" )
				  : QString() );
	}

	// ---- sample -----------------------------------------------------------
	const int nT = int( tracks.size() );
	clip.name = an.value( QLatin1String( "name" ) ).toString();
	if ( clip.name.isEmpty() )
		clip.name = name;
	clip.originalSkeletonName = opt.originalSkeletonName;
	clip.blendHint = QStringLiteral( "NORMAL" );
	clip.rotationQuantization = QStringLiteral( "none (uncompressed, from glTF)" );
	clip.numFrames = numFrames;
	clip.numTracks = nT;
	clip.numFloatTracks = 0;
	clip.numBlocks = 0;
	clip.maxFramesPerBlock = 0;
	clip.frameDuration = float( frameDuration );
	clip.duration = float( double( numFrames - 1 ) * frameDuration );
	clip.trackToBone.resize( nT );
	clip.annotations.resize( nT );
	for ( int k = 0; k < nT; k++ )
		clip.trackToBone[k] = tracks[k].first;
	clip.rootMotionUp = opt.rootMotionUp;
	clip.rootMotionForward = opt.rootMotionForward;

	const double U = double( opt.unitScale );
	clip.frames.resize( numFrames );
	for ( int f = 0; f < numFrames; f++ ) {
		clip.frames[f].resize( nT );
		for ( int k = 0; k < nT; k++ ) {
			const int i = tracks[k].second;
			const Node & nd = nodes[i];
			double tv[3] = { nd.t[0], nd.t[1], nd.t[2] };
			double sv[3] = { nd.s[0], nd.s[1], nd.s[2] };
			Q4 rv = nd.r;
			if ( chT[i] >= 0 )
				sampleAt( samplers[chT[i]], times[f], tv );
			if ( chS[i] >= 0 )
				sampleAt( samplers[chS[i]], times[f], sv );
			if ( chR[i] >= 0 ) {
				double q[4];
				sampleAt( samplers[chR[i]], times[f], q );
				rv = qnorm( Q4{ q[0], q[1], q[2], q[3] } );
			}
			if ( opt.convertUpAxis && consumed < 0 && isNifRoot[i] ) {
				qrot( UNDO_UP, tv );
				rv = qnorm( qmul( UNDO_UP, rv ) );
			}
			HkxTransform & h = clip.frames[f][k];
			h.translation = Vector3( float( tv[0] / U ), float( tv[1] / U ), float( tv[2] / U ) );
			h.rotation = toQuat( rv );
			h.scale = Vector3( float( sv[0] ), float( sv[1] ), float( sv[2] ) );
		}
	}

	// ---- root motion ------------------------------------------------------
	if ( opt.extractRootMotion ) {
		int rootNode = -1;
		if ( !opt.rootNodeName.isEmpty() ) {
			for ( int i = 0; i < nn; i++ )
				if ( inTree[i] && !nodes[i].name.compare( opt.rootNodeName, Qt::CaseInsensitive ) ) {
					rootNode = i;
					break;
				}
			if ( rootNode < 0 )
				return refuse( QStringLiteral( "root motion was asked for from '%1'; no node of that name is in the tree" ).arg( opt.rootNodeName ) );
		} else if ( nifRoots.size() == 1 ) {
			rootNode = nifRoots.first();
		} else {
			return refuse( QStringLiteral( "root motion was asked for and the file has %1 scene roots; name one with rootNodeName" ).arg( nifRoots.size() ) );
		}
		int rootTrack = -1;
		for ( int k = 0; k < nT; k++ )
			if ( tracks[k].second == rootNode ) {
				rootTrack = k;
				break;
			}
		if ( rootTrack < 0 )
			return refuse( QStringLiteral( "root motion was asked for from '%1', which has no track (it reached no bone)" ).arg( nodes[rootNode].name ) );
		const int rf = qBound( 0, opt.rootMotionReferenceFrame, numFrames - 1 );
		const HkxTransform ref = clip.frames[rf][rootTrack];
		const Q4 refQ = qnorm( Q4{ ref.rotation[1], ref.rotation[2], ref.rotation[3], ref.rotation[0] } );
		double up[3] = { double( opt.rootMotionUp[0] ), double( opt.rootMotionUp[1] ), double( opt.rootMotionUp[2] ) };
		const double un = std::sqrt( up[0] * up[0] + up[1] * up[1] + up[2] * up[2] );
		if ( !( un > 1e-9 ) )
			return refuse( QStringLiteral( "the root-motion up axis is (%1, %2, %3)" ).arg( up[0] ).arg( up[1] ).arg( up[2] ) );
		for ( int c = 0; c < 3; c++ )
			up[c] /= un;
		clip.rootMotion.resize( numFrames );
		for ( int f = 0; f < numFrames; f++ ) {
			const HkxTransform & cur = clip.frames[f][rootTrack];
			HkxRootMotion & rm = clip.rootMotion[f];
			rm.translation = Vector3( cur.translation[0] - ref.translation[0],
									  cur.translation[1] - ref.translation[1],
									  cur.translation[2] - ref.translation[2] );
			const Q4 cq = qnorm( Q4{ cur.rotation[1], cur.rotation[2], cur.rotation[3], cur.rotation[0] } );
			const Q4 d = qnorm( qmul( cq, qconj( refQ ) ) );
			const double axisDot = d.x * up[0] + d.y * up[1] + d.z * up[2];
			rm.yaw = float( 2.0 * std::atan2( axisDot, d.w ) );
			clip.frames[f][rootTrack] = ref;      // the track plays in place
			rep.rootMotionMaxTranslation = std::fmax( rep.rootMotionMaxTranslation,
				std::sqrt( rm.translation[0] * rm.translation[0] + rm.translation[1] * rm.translation[1]
						   + rm.translation[2] * rm.translation[2] ) );
			rep.rootMotionMaxYawDeg = float( std::fmax( double( rep.rootMotionMaxYawDeg ),
				std::fabs( double( rm.yaw ) ) * 180.0 / M_PI ) );
		}
		rep.rootMotionExtracted = true;
		rep.rootMotionNode = nodes[rootNode].name;
	}

	if ( !ignoredPaths.isEmpty() )
		rep.unmatchedNodes.append( QStringLiteral( "channels not carried: %1" ).arg( ignoredPaths.join( QLatin1String( ", " ) ) ) );
	return true;
}

bool gltfImportRead( const QString & path, const GltfImportOptions & opt,
					 HkxAnimClip & clip, GltfImportReport & rep )
{
	rep = GltfImportReport();
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		rep.error = QStringLiteral( "cannot open '%1': %2" ).arg( path ).arg( f.errorString() );
		return false;
	}
	const QByteArray blob = f.readAll();
	f.close();
	if ( blob.isEmpty() ) {
		rep.error = QStringLiteral( "'%1' is empty" ).arg( path );
		return false;
	}

	// resolve every external buffer uri relative to the .gltf itself
	QHash<QString, QByteArray> ext;
	if ( !( blob.size() >= 4 && blob.startsWith( "glTF" ) ) ) {
		const QJsonDocument doc = QJsonDocument::fromJson( blob );
		if ( doc.isObject() ) {
			const QDir dir = QFileInfo( path ).absoluteDir();
			for ( const QJsonValue & bv : doc.object().value( QLatin1String( "buffers" ) ).toArray() ) {
				const QString uri = bv.toObject().value( QLatin1String( "uri" ) ).toString();
				if ( uri.isEmpty() || uri.startsWith( QLatin1String( "data:" ) ) )
					continue;
				const QString key = QUrl::fromPercentEncoding( uri.toUtf8() );
				QFile bf( dir.absoluteFilePath( key ) );
				if ( !bf.open( QIODevice::ReadOnly ) ) {
					rep.error = QStringLiteral( "'%1' needs the buffer file '%2', which cannot be opened: %3" )
						.arg( path ).arg( key ).arg( bf.errorString() );
					return false;
				}
				ext.insert( key, bf.readAll() );
			}
		}
	}
	return gltfImportParse( blob, QFileInfo( path ).completeBaseName(), opt, clip, rep, ext );
}
