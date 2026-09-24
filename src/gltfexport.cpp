/* glTF 2.0 writer -- see src/gltfexport.h for what it is and why it is not
   src/lib/importex/gltf.cpp, and docs/GLTF_INTERCHANGE.md for the contract it
   implements.

   QtCore only. No NifModel, no Scene, no GL, no tiny_gltf: the input is the
   neutral struct in the header, the output is two files. That is what lets
   tests/gltfexport_dump.cpp link it standalone and the gates run while
   release/NifSkope.exe cannot be built (CONSTITUTION 6, skill
   ww-standalone-writer-gate). */

#include "gltfexport.h"

#include <QFile>
#include <QFileInfo>

#include <cmath>
#include <cstring>

namespace {

// ---------------------------------------------------------------- numbers

//! glTF is JSON: every number written must be finite.
bool finite1( float v ) { return std::isfinite( v ); }
bool finite3( const Vector3 & v ) { return finite1( v[0] ) && finite1( v[1] ) && finite1( v[2] ); }

//! Shortest form that round-trips a float through a double parser. Nine
//! significant digits is exactly what an IEEE-754 binary32 needs; an integral
//! value comes out with no point, which is legal JSON. Every number that
//! reaches here has already been tested finite, so 'inf' / 'nan' -- neither of
//! which is legal JSON -- cannot be produced.
QByteArray num( double v )
{
	return QByteArray::number( v, 'g', 9 );
}

QByteArray numf( float v ) { return num( double( v ) ); }

QByteArray jstr( const QString & s )
{
	QByteArray out = "\"";
	const QByteArray utf8 = s.toUtf8();
	for ( char c : utf8 ) {
		switch ( c ) {
		case '"':  out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\b': out += "\\b"; break;
		case '\f': out += "\\f"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if ( quint8( c ) < 0x20 )
				out += QByteArray( "\\u00" ) + QByteArray::number( quint8( c ), 16 ).rightJustified( 2, '0' );
			else
				out += c;
		}
	}
	return out + "\"";
}

//! A texture path out of a NIF is a Windows path; a glTF uri is a URI.
QByteArray juri( const QString & path )
{
	QString p = path;
	p.replace( '\\', '/' );
	QByteArray out;
	const QByteArray utf8 = p.toUtf8();
	for ( char c : utf8 ) {
		const quint8 u = quint8( c );
		const bool safe = ( u >= 'a' && u <= 'z' ) || ( u >= 'A' && u <= 'Z' ) || ( u >= '0' && u <= '9' )
			|| c == '-' || c == '_' || c == '.' || c == '~' || c == '/';
		if ( safe )
			out += c;
		else
			out += QByteArray( "%" ) + QByteArray::number( u, 16 ).rightJustified( 2, '0' ).toUpper();
	}
	return "\"" + out + "\"";
}

// ---------------------------------------------------------- quaternions

struct Q4 { double x, y, z, w; };

Q4 fromQuat( const Quat & q ) { return Q4{ double( q[1] ), double( q[2] ), double( q[3] ), double( q[0] ) }; }

Q4 qmul( const Q4 & a, const Q4 & b )
{
	return Q4{
		a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
		a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
		a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
		a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
	};
}

//! Rotation of `angle` radians about a unit axis, as (x, y, z, w).
Q4 qaxis( const Vector3 & axis, double angle )
{
	double n = std::sqrt( double( axis[0] ) * axis[0] + double( axis[1] ) * axis[1] + double( axis[2] ) * axis[2] );
	if ( n <= 0.0 )
		return Q4{ 0.0, 0.0, 0.0, 1.0 };
	const double s = std::sin( angle * 0.5 ) / n;
	return Q4{ axis[0] * s, axis[1] * s, axis[2] * s, std::cos( angle * 0.5 ) };
}

// -------------------------------------------------------------- buffer

struct Accessor
{
	int view = -1;
	int componentType = 5126;       // FLOAT
	int count = 0;
	QByteArray type = "SCALAR";
	bool hasBounds = false;
	QVector<double> minv, maxv;
};

struct View
{
	int offset = 0;
	int length = 0;
	int target = 0;                 // 0 = do not write `target`
};

class Buffer
{
public:
	QByteArray bytes;

	void align4()
	{
		while ( bytes.size() % 4 )
			bytes.append( '\0' );
	}
	int addView( const void * src, int len, int target )
	{
		align4();
		View v;
		v.offset = int( bytes.size() );
		v.length = len;
		v.target = target;
		bytes.append( reinterpret_cast<const char *>( src ), len );
		views.append( v );
		return int( views.size() ) - 1;
	}
	int addFloats( const QVector<float> & f, const QByteArray & type, int target, bool bounds, int comps )
	{
		Accessor a;
		a.view = addView( f.constData(), int( f.size() ) * 4, target );
		a.componentType = 5126;
		a.type = type;
		a.count = comps > 0 ? int( f.size() ) / comps : int( f.size() );
		if ( bounds && a.count > 0 ) {
			a.hasBounds = true;
			a.minv.resize( comps );
			a.maxv.resize( comps );
			for ( int c = 0; c < comps; c++ ) {
				a.minv[c] = f[c];
				a.maxv[c] = f[c];
			}
			for ( int i = 0; i < a.count; i++ )
				for ( int c = 0; c < comps; c++ ) {
					const double v = f[i * comps + c];
					a.minv[c] = qMin( a.minv[c], v );
					a.maxv[c] = qMax( a.maxv[c], v );
				}
		}
		accessors.append( a );
		return int( accessors.size() ) - 1;
	}
	int addU16( const QVector<quint16> & d, const QByteArray & type, int target, int comps )
	{
		Accessor a;
		a.view = addView( d.constData(), int( d.size() ) * 2, target );
		a.componentType = 5123;
		a.type = type;
		a.count = int( d.size() ) / comps;
		accessors.append( a );
		return int( accessors.size() ) - 1;
	}
	int addU32( const QVector<quint32> & d, const QByteArray & type, int target, int comps )
	{
		Accessor a;
		a.view = addView( d.constData(), int( d.size() ) * 4, target );
		a.componentType = 5125;
		a.type = type;
		a.count = int( d.size() ) / comps;
		accessors.append( a );
		return int( accessors.size() ) - 1;
	}

	QVector<View> views;
	QVector<Accessor> accessors;
};

} // namespace

QString gltfExportBinPath( const QString & gltfPath )
{
	QFileInfo fi( gltfPath );
	return fi.path() + "/" + fi.completeBaseName() + ".bin";
}

bool gltfExportWrite( const GltfExportScene & scene, const QString & gltfPath, QString & error )
{
	error.clear();
	const int nn = int( scene.nodes.size() );
	if ( nn < 1 ) {
		error = QStringLiteral( "the scene has no nodes" );
		return false;
	}
	if ( !( scene.unitScale > 0.0f ) || !std::isfinite( scene.unitScale ) ) {
		error = QStringLiteral( "unit scale is %1 metres per unit" ).arg( double( scene.unitScale ) );
		return false;
	}

	// ---- validation: everything the JSON will claim ---------------------
	for ( int i = 0; i < nn; i++ ) {
		const GltfExportNode & n = scene.nodes[i];
		if ( n.parent < -1 || n.parent >= nn ) {
			error = QStringLiteral( "node %1 '%2' has parent %3, outside 0..%4" )
				.arg( i ).arg( n.name ).arg( n.parent ).arg( nn - 1 );
			return false;
		}
		if ( n.parent == i ) {
			error = QStringLiteral( "node %1 '%2' is its own parent" ).arg( i ).arg( n.name );
			return false;
		}
		if ( !finite3( n.translation ) || !finite3( n.scale ) ) {
			error = QStringLiteral( "node %1 '%2' has a non-finite translation or scale" ).arg( i ).arg( n.name );
			return false;
		}
		const double ql = std::sqrt( double( n.rotation[0] ) * n.rotation[0] + double( n.rotation[1] ) * n.rotation[1]
			+ double( n.rotation[2] ) * n.rotation[2] + double( n.rotation[3] ) * n.rotation[3] );
		if ( !( ql > 1e-6 ) || !std::isfinite( ql ) ) {
			error = QStringLiteral( "node %1 '%2' has rotation of length %3" ).arg( i ).arg( n.name ).arg( ql );
			return false;
		}
	}
	// a parent chain that does not reach a root is a cycle
	for ( int i = 0; i < nn; i++ ) {
		int p = scene.nodes[i].parent, steps = 0;
		while ( p >= 0 ) {
			if ( ++steps > nn ) {
				error = QStringLiteral( "node %1 '%2' is inside a parent cycle" ).arg( i ).arg( scene.nodes[i].name );
				return false;
			}
			p = scene.nodes[p].parent;
		}
	}
	QVector<int> meshOfNode( nn, -1 );
	for ( int m = 0; m < scene.meshes.size(); m++ ) {
		const GltfExportMesh & mesh = scene.meshes[m];
		const int nv = int( mesh.positions.size() );
		if ( mesh.node < 0 || mesh.node >= nn ) {
			error = QStringLiteral( "mesh %1 '%2' hangs on node %3, outside 0..%4" )
				.arg( m ).arg( mesh.name ).arg( mesh.node ).arg( nn - 1 );
			return false;
		}
		if ( meshOfNode[mesh.node] >= 0 ) {
			error = QStringLiteral( "mesh %1 '%2' and mesh %3 both hang on node %4" )
				.arg( m ).arg( mesh.name ).arg( meshOfNode[mesh.node] ).arg( mesh.node );
			return false;
		}
		meshOfNode[mesh.node] = m;
		if ( nv < 1 ) {
			error = QStringLiteral( "mesh %1 '%2' has no vertices" ).arg( m ).arg( mesh.name );
			return false;
		}
		if ( !mesh.normals.isEmpty() && mesh.normals.size() != nv ) {
			error = QStringLiteral( "mesh %1 '%2': %3 normals for %4 vertices" )
				.arg( m ).arg( mesh.name ).arg( mesh.normals.size() ).arg( nv );
			return false;
		}
		if ( !mesh.texCoords.isEmpty() && mesh.texCoords.size() != nv ) {
			error = QStringLiteral( "mesh %1 '%2': %3 texture coordinates for %4 vertices" )
				.arg( m ).arg( mesh.name ).arg( mesh.texCoords.size() ).arg( nv );
			return false;
		}
		if ( mesh.indices.isEmpty() || mesh.indices.size() % 3 ) {
			error = QStringLiteral( "mesh %1 '%2': %3 indices, not a whole number of triangles" )
				.arg( m ).arg( mesh.name ).arg( mesh.indices.size() );
			return false;
		}
		for ( quint32 idx : mesh.indices )
			if ( int( idx ) >= nv ) {
				error = QStringLiteral( "mesh %1 '%2': index %3 against %4 vertices" )
					.arg( m ).arg( mesh.name ).arg( idx ).arg( nv );
				return false;
			}
		for ( int v = 0; v < nv; v++ )
			if ( !finite3( mesh.positions[v] ) ) {
				error = QStringLiteral( "mesh %1 '%2': vertex %3 is not finite" ).arg( m ).arg( mesh.name ).arg( v );
				return false;
			}
		if ( !mesh.skin.isEmpty() ) {
			if ( mesh.skin.size() != nv ) {
				error = QStringLiteral( "mesh %1 '%2': %3 skin rows for %4 vertices" )
					.arg( m ).arg( mesh.name ).arg( mesh.skin.size() ).arg( nv );
				return false;
			}
			if ( mesh.joints.isEmpty() ) {
				error = QStringLiteral( "mesh %1 '%2' is skinned but names no joints" ).arg( m ).arg( mesh.name );
				return false;
			}
			if ( mesh.inverseBind.size() != 16 * mesh.joints.size() ) {
				error = QStringLiteral( "mesh %1 '%2': %3 inverse-bind floats for %4 joints (want %5)" )
					.arg( m ).arg( mesh.name ).arg( mesh.inverseBind.size() )
					.arg( mesh.joints.size() ).arg( 16 * mesh.joints.size() );
				return false;
			}
			for ( int j = 0; j < mesh.joints.size(); j++ )
				if ( mesh.joints[j] < 0 || mesh.joints[j] >= nn ) {
					error = QStringLiteral( "mesh %1 '%2': joint %3 is node %4, outside 0..%5" )
						.arg( m ).arg( mesh.name ).arg( j ).arg( mesh.joints[j] ).arg( nn - 1 );
					return false;
				}
			for ( float f : mesh.inverseBind )
				if ( !finite1( f ) ) {
					error = QStringLiteral( "mesh %1 '%2': a non-finite inverse-bind matrix element" )
						.arg( m ).arg( mesh.name );
					return false;
				}
			for ( int v = 0; v < nv; v++ )
				for ( int k = 0; k < 4; k++ ) {
					if ( mesh.skin[v].joints[k] >= quint16( mesh.joints.size() ) ) {
						error = QStringLiteral( "mesh %1 '%2': vertex %3 influence %4 is joint %5 of %6" )
							.arg( m ).arg( mesh.name ).arg( v ).arg( k )
							.arg( mesh.skin[v].joints[k] ).arg( mesh.joints.size() );
						return false;
					}
					if ( !finite1( mesh.skin[v].weights[k] ) || mesh.skin[v].weights[k] < 0.0f ) {
						error = QStringLiteral( "mesh %1 '%2': vertex %3 influence %4 has weight %5" )
							.arg( m ).arg( mesh.name ).arg( v ).arg( k ).arg( double( mesh.skin[v].weights[k] ) );
						return false;
					}
				}
		}
	}
	for ( int a = 0; a < scene.animations.size(); a++ ) {
		const GltfExportAnimation & an = scene.animations[a];
		if ( an.numFrames < 1 ) {
			error = QStringLiteral( "animation %1 '%2' has %3 frames" ).arg( a ).arg( an.name ).arg( an.numFrames );
			return false;
		}
		if ( !( an.frameDuration > 0.0f ) || !std::isfinite( an.frameDuration ) ) {
			error = QStringLiteral( "animation %1 '%2' has a frame duration of %3 s" )
				.arg( a ).arg( an.name ).arg( double( an.frameDuration ) );
			return false;
		}
		for ( int c = 0; c < an.channels.size(); c++ ) {
			const GltfExportChannel & ch = an.channels[c];
			if ( ch.node < 0 || ch.node >= nn ) {
				error = QStringLiteral( "animation %1 channel %2 drives node %3, outside 0..%4" )
					.arg( a ).arg( c ).arg( ch.node ).arg( nn - 1 );
				return false;
			}
			const int t = int( ch.translations.size() ), r = int( ch.rotations.size() ), s = int( ch.scales.size() );
			if ( ( t && t != an.numFrames ) || ( r && r != an.numFrames ) || ( s && s != an.numFrames ) ) {
				error = QStringLiteral( "animation %1 channel %2 (node '%3'): %4/%5/%6 translation/rotation/scale samples for %7 frames" )
					.arg( a ).arg( c ).arg( scene.nodes[ch.node].name ).arg( t ).arg( r ).arg( s ).arg( an.numFrames );
				return false;
			}
			if ( !t && !r && !s ) {
				error = QStringLiteral( "animation %1 channel %2 (node '%3') carries no samples" )
					.arg( a ).arg( c ).arg( scene.nodes[ch.node].name );
				return false;
			}
		}
		if ( an.applyRootMotion ) {
			if ( an.rootMotionNode < 0 || an.rootMotionNode >= nn ) {
				error = QStringLiteral( "animation %1 '%2' applies root motion to node %3, outside 0..%4" )
					.arg( a ).arg( an.name ).arg( an.rootMotionNode ).arg( nn - 1 );
				return false;
			}
			if ( an.rootMotionTranslation.size() != an.numFrames || an.rootMotionYaw.size() != an.numFrames ) {
				error = QStringLiteral( "animation %1 '%2': %3 root-motion translations and %4 yaws for %5 frames" )
					.arg( a ).arg( an.name ).arg( an.rootMotionTranslation.size() )
					.arg( an.rootMotionYaw.size() ).arg( an.numFrames );
				return false;
			}
		}
	}

	// ---- the buffer -----------------------------------------------------
	Buffer buf;
	const float U = scene.unitScale;

	struct MeshAcc { int pos = -1, nrm = -1, uv = -1, joints = -1, weights = -1, idx = -1, ibm = -1, material = -1; };
	QVector<MeshAcc> ma( int( scene.meshes.size() ) );

	for ( int m = 0; m < scene.meshes.size(); m++ ) {
		const GltfExportMesh & mesh = scene.meshes[m];
		const int nv = int( mesh.positions.size() );

		QVector<float> f( nv * 3 );
		for ( int v = 0; v < nv; v++ ) {
			f[v * 3 + 0] = mesh.positions[v][0] * U;
			f[v * 3 + 1] = mesh.positions[v][1] * U;
			f[v * 3 + 2] = mesh.positions[v][2] * U;
		}
		ma[m].pos = buf.addFloats( f, "VEC3", 34962, true, 3 );

		if ( !mesh.normals.isEmpty() ) {
			for ( int v = 0; v < nv; v++ ) {
				// a NIF byte normal can arrive slightly off unit; glTF wants unit
				double x = mesh.normals[v][0], y = mesh.normals[v][1], z = mesh.normals[v][2];
				const double l = std::sqrt( x * x + y * y + z * z );
				if ( l > 1e-6 ) { x /= l; y /= l; z /= l; } else { x = 0.0; y = 0.0; z = 1.0; }
				f[v * 3 + 0] = float( x );
				f[v * 3 + 1] = float( y );
				f[v * 3 + 2] = float( z );
			}
			ma[m].nrm = buf.addFloats( f, "VEC3", 34962, false, 3 );
		}
		if ( !mesh.texCoords.isEmpty() ) {
			QVector<float> uv( nv * 2 );
			for ( int v = 0; v < nv; v++ ) {
				uv[v * 2 + 0] = mesh.texCoords[v][0];
				uv[v * 2 + 1] = mesh.texCoords[v][1];
			}
			ma[m].uv = buf.addFloats( uv, "VEC2", 34962, false, 2 );
		}
		if ( !mesh.skin.isEmpty() ) {
			QVector<quint16> j( nv * 4 );
			QVector<float> w( nv * 4 );
			for ( int v = 0; v < nv; v++ )
				for ( int k = 0; k < 4; k++ ) {
					j[v * 4 + k] = mesh.skin[v].joints[k];
					w[v * 4 + k] = mesh.skin[v].weights[k];
				}
			ma[m].joints = buf.addU16( j, "VEC4", 34962, 4 );
			ma[m].weights = buf.addFloats( w, "VEC4", 34962, false, 4 );

			QVector<float> ibm = mesh.inverseBind;
			for ( int j2 = 0; j2 < mesh.joints.size(); j2++ ) {
				ibm[j2 * 16 + 12] *= U;
				ibm[j2 * 16 + 13] *= U;
				ibm[j2 * 16 + 14] *= U;
			}
			ma[m].ibm = buf.addFloats( ibm, "MAT4", 0, false, 16 );
		}
		if ( nv <= 65535 ) {
			QVector<quint16> idx( int( mesh.indices.size() ) );
			for ( int i = 0; i < mesh.indices.size(); i++ )
				idx[i] = quint16( mesh.indices[i] );
			ma[m].idx = buf.addU16( idx, "SCALAR", 34963, 1 );
		} else {
			ma[m].idx = buf.addU32( mesh.indices, "SCALAR", 34963, 1 );
		}
	}

	// animations: one shared time accessor per animation, then the channels
	struct ChanAcc { int node = -1, t = -1, r = -1, s = -1; };
	QVector<QVector<ChanAcc>> aa( int( scene.animations.size() ) );
	QVector<int> timeAcc( int( scene.animations.size() ), -1 );

	for ( int a = 0; a < scene.animations.size(); a++ ) {
		const GltfExportAnimation & an = scene.animations[a];
		QVector<float> t( an.numFrames );
		for ( int i = 0; i < an.numFrames; i++ )
			t[i] = float( i ) * an.frameDuration;
		timeAcc[a] = buf.addFloats( t, "SCALAR", 0, true, 1 );

		aa[a].resize( int( an.channels.size() ) );
		for ( int c = 0; c < an.channels.size(); c++ ) {
			const GltfExportChannel & ch = an.channels[c];
			aa[a][c].node = ch.node;
			const bool rootHere = an.applyRootMotion && ch.node == an.rootMotionNode;

			if ( !ch.translations.isEmpty() ) {
				QVector<float> v( an.numFrames * 3 );
				for ( int i = 0; i < an.numFrames; i++ ) {
					Vector3 p = ch.translations[i];
					if ( rootHere )
						p = Vector3( p[0] + an.rootMotionTranslation[i][0],
									 p[1] + an.rootMotionTranslation[i][1],
									 p[2] + an.rootMotionTranslation[i][2] );
					if ( !finite3( p ) ) {
						error = QStringLiteral( "animation %1 channel %2 (node '%3') frame %4 has a non-finite translation" )
							.arg( a ).arg( c ).arg( scene.nodes[ch.node].name ).arg( i );
						return false;
					}
					v[i * 3 + 0] = p[0] * U;
					v[i * 3 + 1] = p[1] * U;
					v[i * 3 + 2] = p[2] * U;
				}
				aa[a][c].t = buf.addFloats( v, "VEC3", 0, false, 3 );
			}
			if ( !ch.rotations.isEmpty() ) {
				QVector<float> v( an.numFrames * 4 );
				for ( int i = 0; i < an.numFrames; i++ ) {
					Q4 q = fromQuat( ch.rotations[i] );
					if ( rootHere )
						q = qmul( qaxis( an.rootMotionUp, double( an.rootMotionYaw[i] ) ), q );
					const double l = std::sqrt( q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w );
					if ( !( l > 1e-6 ) || !std::isfinite( l ) ) {
						error = QStringLiteral( "animation %1 channel %2 (node '%3') frame %4 has a rotation of length %5" )
							.arg( a ).arg( c ).arg( scene.nodes[ch.node].name ).arg( i ).arg( l );
						return false;
					}
					v[i * 4 + 0] = float( q.x / l );
					v[i * 4 + 1] = float( q.y / l );
					v[i * 4 + 2] = float( q.z / l );
					v[i * 4 + 3] = float( q.w / l );
				}
				aa[a][c].r = buf.addFloats( v, "VEC4", 0, false, 4 );
			}
			if ( !ch.scales.isEmpty() ) {
				QVector<float> v( an.numFrames * 3 );
				for ( int i = 0; i < an.numFrames; i++ ) {
					if ( !finite3( ch.scales[i] ) ) {
						error = QStringLiteral( "animation %1 channel %2 (node '%3') frame %4 has a non-finite scale" )
							.arg( a ).arg( c ).arg( scene.nodes[ch.node].name ).arg( i );
						return false;
					}
					v[i * 3 + 0] = ch.scales[i][0];
					v[i * 3 + 1] = ch.scales[i][1];
					v[i * 3 + 2] = ch.scales[i][2];
				}
				aa[a][c].s = buf.addFloats( v, "VEC3", 0, false, 3 );
			}
		}
	}
	buf.align4();

	// ---- the JSON -------------------------------------------------------
	// glTF node 0 is the synthetic up-axis root; scene node i is glTF node i+1.
	const int OFF = 1;
	QByteArray j;
	j += "{\n";
	j += "\"asset\":{\"version\":\"2.0\",\"generator\":" + jstr( scene.generator.isEmpty()
		? QStringLiteral( "NifSkope Wild Wasteland Edition, gltfexport" ) : scene.generator );
	if ( !scene.copyright.isEmpty() )
		j += ",\"copyright\":" + jstr( scene.copyright );
	j += ",\"extras\":{\"metresPerUnit\":" + numf( U )
		+ ",\"sourceUpAxis\":\"Z\",\"upAxisNode\":" + jstr( scene.upAxisNodeName ) + "}},\n";
	j += "\"scene\":0,\n\"scenes\":[{\"nodes\":[0]}],\n";

	// nodes
	j += "\"nodes\":[\n";
	{
		// the up-axis root: rotate Z-up (NIF) into Y-up (glTF) = -90 deg about X
		QByteArray kids;
		for ( int i = 0; i < nn; i++ )
			if ( scene.nodes[i].parent < 0 ) {
				if ( !kids.isEmpty() )
					kids += ",";
				kids += QByteArray::number( i + OFF );
			}
		if ( kids.isEmpty() ) {
			error = QStringLiteral( "the scene has no root node" );
			return false;
		}
		const double s = std::sqrt( 0.5 );
		j += "{\"name\":" + jstr( scene.upAxisNodeName )
			+ ",\"rotation\":[" + num( -s ) + ",0,0," + num( s ) + "]"
			+ ",\"children\":[" + kids + "]}";
	}
	for ( int i = 0; i < nn; i++ ) {
		const GltfExportNode & n = scene.nodes[i];
		j += ",\n{\"name\":" + jstr( n.name );
		const bool skinned = meshOfNode[i] >= 0 && !scene.meshes[meshOfNode[i]].skin.isEmpty();
		if ( !skinned ) {
			// a skinned mesh node's transform is ignored by the spec, and the
			// NIF's shape transform is folded into the inverse-bind matrices
			const double ql = std::sqrt( double( n.rotation[0] ) * n.rotation[0] + double( n.rotation[1] ) * n.rotation[1]
				+ double( n.rotation[2] ) * n.rotation[2] + double( n.rotation[3] ) * n.rotation[3] );
			j += ",\"translation\":[" + numf( n.translation[0] * U ) + "," + numf( n.translation[1] * U )
				+ "," + numf( n.translation[2] * U ) + "]";
			j += ",\"rotation\":[" + num( n.rotation[1] / ql ) + "," + num( n.rotation[2] / ql ) + ","
				+ num( n.rotation[3] / ql ) + "," + num( n.rotation[0] / ql ) + "]";
			j += ",\"scale\":[" + numf( n.scale[0] ) + "," + numf( n.scale[1] ) + "," + numf( n.scale[2] ) + "]";
		}
		if ( meshOfNode[i] >= 0 ) {
			j += ",\"mesh\":" + QByteArray::number( meshOfNode[i] );
			if ( skinned )
				j += ",\"skin\":" + QByteArray::number( meshOfNode[i] );
		}
		QByteArray kids;
		for ( int c = 0; c < nn; c++ )
			if ( scene.nodes[c].parent == i ) {
				if ( !kids.isEmpty() )
					kids += ",";
				kids += QByteArray::number( c + OFF );
			}
		if ( !kids.isEmpty() )
			j += ",\"children\":[" + kids + "]";
		j += "}";
	}
	j += "\n],\n";

	// meshes + materials + textures. An EMPTY glTF array is a spec violation
	// ("EMPTY_ENTITY"), so an animation-only export -- a clip with no
	// character -- writes neither `meshes` nor `materials` rather than a pair
	// of empty ones. They are built aside and appended only when non-empty.
	QByteArray images, textures, materials, meshesJson;
	int nImages = 0;
	for ( int m = 0; m < scene.meshes.size(); m++ ) {
		const GltfExportMesh & mesh = scene.meshes[m];
		if ( m )
			meshesJson += ",\n";
		meshesJson += "{\"name\":" + jstr( mesh.name ) + ",\"primitives\":[{\"attributes\":{\"POSITION\":"
			+ QByteArray::number( ma[m].pos );
		if ( ma[m].nrm >= 0 )
			meshesJson += ",\"NORMAL\":" + QByteArray::number( ma[m].nrm );
		if ( ma[m].uv >= 0 )
			meshesJson += ",\"TEXCOORD_0\":" + QByteArray::number( ma[m].uv );
		if ( ma[m].joints >= 0 )
			meshesJson += ",\"JOINTS_0\":" + QByteArray::number( ma[m].joints )
				+ ",\"WEIGHTS_0\":" + QByteArray::number( ma[m].weights );
		meshesJson += "},\"indices\":" + QByteArray::number( ma[m].idx )
			+ ",\"mode\":4,\"material\":" + QByteArray::number( m ) + "}]";
		if ( !mesh.mergedPartitions.isEmpty() ) {
			meshesJson += ",\"extras\":{\"mergedPartitions\":[";
			for ( int p = 0; p < mesh.mergedPartitions.size(); p++ )
				meshesJson += ( p ? "," : "" ) + jstr( mesh.mergedPartitions[p] );
			meshesJson += "]}";
		}
		meshesJson += "}";

		// material m, one per shape
		if ( m )
			materials += ",\n";
		materials += "{\"name\":" + jstr( mesh.materialName.isEmpty()
			? ( mesh.name + QStringLiteral( "_material" ) ) : mesh.materialName )
			+ ",\"doubleSided\":true,\"pbrMetallicRoughness\":{";
		if ( !mesh.diffuseUri.isEmpty() ) {
			if ( nImages )
				images += ",";
			images += "{\"uri\":" + juri( mesh.diffuseUri ) + "}";
			if ( nImages )
				textures += ",";
			textures += "{\"sampler\":0,\"source\":" + QByteArray::number( nImages ) + "}";
			materials += "\"baseColorTexture\":{\"index\":" + QByteArray::number( nImages ) + "},";
			nImages++;
		} else {
			materials += "\"baseColorFactor\":[0.8,0.8,0.8,1],";
		}
		materials += "\"metallicFactor\":0,\"roughnessFactor\":1}";
		if ( !mesh.diffuseSourcePath.isEmpty() )
			materials += ",\"extras\":{\"nifTexturePath\":" + jstr( mesh.diffuseSourcePath ) + "}";
		materials += "}";
	}
	if ( !meshesJson.isEmpty() ) {
		j += "\"meshes\":[\n" + meshesJson + "\n],\n";
		j += "\"materials\":[\n" + materials + "\n],\n";
	}
	if ( nImages ) {
		j += "\"images\":[" + images + "],\n";
		j += "\"textures\":[" + textures + "],\n";
		j += "\"samplers\":[{\"magFilter\":9729,\"minFilter\":9987,\"wrapS\":10497,\"wrapT\":10497}],\n";
	}

	// skins
	QByteArray skins;
	int nSkins = 0;
	for ( int m = 0; m < scene.meshes.size(); m++ ) {
		const GltfExportMesh & mesh = scene.meshes[m];
		if ( mesh.skin.isEmpty() )
			continue;
		if ( nSkins++ )
			skins += ",\n";
		skins += "{\"name\":" + jstr( mesh.name + QStringLiteral( "_skin" ) )
			+ ",\"inverseBindMatrices\":" + QByteArray::number( ma[m].ibm ) + ",\"joints\":[";
		for ( int k = 0; k < mesh.joints.size(); k++ )
			skins += ( k ? "," : "" ) + QByteArray::number( mesh.joints[k] + OFF );
		skins += "]}";
	}
	if ( nSkins ) {
		if ( nSkins != int( scene.meshes.size() ) ) {
			// skin index == mesh index is only legal when every mesh is skinned
			error = QStringLiteral( "%1 of %2 meshes are skinned; this writer requires all or none" )
				.arg( nSkins ).arg( scene.meshes.size() );
			return false;
		}
		j += "\"skins\":[\n" + skins + "\n],\n";
	}

	// animations
	if ( !scene.animations.isEmpty() ) {
		j += "\"animations\":[\n";
		for ( int a = 0; a < scene.animations.size(); a++ ) {
			const GltfExportAnimation & an = scene.animations[a];
			if ( a )
				j += ",\n";
			QByteArray samplers, channels;
			int ns = 0;
			for ( int c = 0; c < an.channels.size(); c++ ) {
				const ChanAcc & ca = aa[a][c];
				const struct { int acc; const char * path; } paths[3] = {
					{ ca.t, "translation" }, { ca.r, "rotation" }, { ca.s, "scale" }
				};
				for ( const auto & p : paths ) {
					if ( p.acc < 0 )
						continue;
					if ( ns )
						samplers += ",", channels += ",";
					samplers += "{\"input\":" + QByteArray::number( timeAcc[a] )
						+ ",\"interpolation\":\"LINEAR\",\"output\":" + QByteArray::number( p.acc ) + "}";
					channels += "{\"sampler\":" + QByteArray::number( ns )
						+ ",\"target\":{\"node\":" + QByteArray::number( ca.node + OFF )
						+ ",\"path\":\"" + QByteArray( p.path ) + "\"}}";
					ns++;
				}
			}
			if ( !ns ) {
				error = QStringLiteral( "animation %1 '%2' has no channels" ).arg( a ).arg( an.name );
				return false;
			}
			j += "{\"name\":" + jstr( an.name ) + ",\"samplers\":[" + samplers + "],\"channels\":[" + channels + "]";
			j += ",\"extras\":{\"frames\":" + QByteArray::number( an.numFrames )
				+ ",\"frameDuration\":" + numf( an.frameDuration )
				+ ",\"framesPerSecond\":" + numf( 1.0f / an.frameDuration )
				+ ",\"rootMotion\":\"" + QByteArray( an.applyRootMotion ? "applied to the root node's channels"
					: ( an.rootMotionTranslation.isEmpty() ? "none in the clip" : "present in the clip, omitted on request" ) ) + "\"";
			if ( !an.unmatchedTracks.isEmpty() ) {
				j += ",\"unmatchedTracks\":[";
				for ( int u = 0; u < an.unmatchedTracks.size(); u++ )
					j += ( u ? "," : "" ) + jstr( an.unmatchedTracks[u] );
				j += "]";
			}
			j += "}}";
		}
		j += "\n],\n";
	}

	// accessors, bufferViews, buffer. A node-only export (no mesh, no clip)
	// has none of the three; empty arrays are a spec violation, so the whole
	// tail is dropped and the previous member's comma trimmed with it.
	const QString binPath = gltfExportBinPath( gltfPath );
	if ( buf.accessors.isEmpty() ) {
		while ( j.endsWith( ",\n" ) )
			j.chop( 2 );
		j += "\n}\n";
		QFile gonly( gltfPath );
		if ( !gonly.open( QIODevice::WriteOnly ) ) {
			error = QStringLiteral( "cannot write %1: %2" ).arg( gltfPath, gonly.errorString() );
			return false;
		}
		if ( gonly.write( j ) != j.size() ) {
			error = QStringLiteral( "short write of %1" ).arg( gltfPath );
			return false;
		}
		gonly.close();
		return true;
	}
	j += "\"accessors\":[\n";
	for ( int i = 0; i < buf.accessors.size(); i++ ) {
		const Accessor & a = buf.accessors[i];
		j += ( i ? ",\n" : "" );
		j += "{\"bufferView\":" + QByteArray::number( a.view )
			+ ",\"componentType\":" + QByteArray::number( a.componentType )
			+ ",\"count\":" + QByteArray::number( a.count )
			+ ",\"type\":\"" + a.type + "\"";
		if ( a.hasBounds ) {
			j += ",\"min\":[";
			for ( int c = 0; c < a.minv.size(); c++ )
				j += ( c ? "," : "" ) + num( a.minv[c] );
			j += "],\"max\":[";
			for ( int c = 0; c < a.maxv.size(); c++ )
				j += ( c ? "," : "" ) + num( a.maxv[c] );
			j += "]";
		}
		j += "}";
	}
	j += "\n],\n\"bufferViews\":[\n";
	for ( int i = 0; i < buf.views.size(); i++ ) {
		const View & v = buf.views[i];
		j += ( i ? ",\n" : "" );
		j += "{\"buffer\":0,\"byteOffset\":" + QByteArray::number( v.offset )
			+ ",\"byteLength\":" + QByteArray::number( v.length );
		if ( v.target )
			j += ",\"target\":" + QByteArray::number( v.target );
		j += "}";
	}
	j += "\n],\n\"buffers\":[{\"uri\":" + juri( QFileInfo( binPath ).fileName() )
		+ ",\"byteLength\":" + QByteArray::number( buf.bytes.size() ) + "}]\n}\n";

	// ---- write ----------------------------------------------------------
	QFile bin( binPath );
	if ( !bin.open( QIODevice::WriteOnly ) ) {
		error = QStringLiteral( "cannot write %1: %2" ).arg( binPath, bin.errorString() );
		return false;
	}
	if ( bin.write( buf.bytes ) != buf.bytes.size() ) {
		error = QStringLiteral( "short write of %1" ).arg( binPath );
		return false;
	}
	bin.close();

	QFile gf( gltfPath );
	if ( !gf.open( QIODevice::WriteOnly ) ) {
		error = QStringLiteral( "cannot write %1: %2" ).arg( gltfPath, gf.errorString() );
		return false;
	}
	if ( gf.write( j ) != j.size() ) {
		error = QStringLiteral( "short write of %1" ).arg( gltfPath );
		return false;
	}
	gf.close();
	return true;
}
