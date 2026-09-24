/* Standalone driver for src/gltfexport.cpp (lane HKX4, 2026-09-10).

   Builds a GltfExportScene out of real files -- a skeleton NIF for the node
   hierarchy, one or more skinned NIFs for the meshes, an hkaSkeleton for the
   clip's bone names and an .hkx clip for the animation -- and writes the
   .gltf / .bin. It exists so the writer can be gated with no NifSkope.exe
   (CONSTITUTION 6; skill ww-standalone-writer-gate) while Fallout4.exe is up.

   THE NIF READING HERE IS TEST CODE, NOT THE SHIPPING PATH. In the
   application the scene is filled from the Scene/Shape objects that already
   unpack a NIF (scratchpad/hkx4_20260910/hookup.py, not applied); this file
   re-reads the container the short way so the gate has a C++ producer that
   shares no code with tests/spells/gltf_readback.py's Python reader. The two
   are held against each other on the same file, and against the file's own
   arithmetic (Data Size == verts * stride + tris * 6).

   Build (MSYS2 UCRT64, from the repo root):
     bash scratchpad/hkx4_20260910/build_dump.sh
   Usage:
     gltfexport_dump --skeleton S.nif [--mesh M.nif ...] [--clip C.hkx]
                     [--bones skeleton.hkx] [--name NAME] [--root-motion]
                     --out out.gltf
*/

#include "gltfexport.h"
#include "hkxanim.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>

#include <cmath>
#include <cstdio>
#include <cstring>

// ------------------------------------------------------------- the NIF

namespace {

struct NifNode
{
	QString name;
	int parent = -1;
	Vector3 t;
	float r[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
	float s = 1.0f;
	QVector<int> children;
};

struct NifSkinBone
{
	QString name;
	float ibm[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };   // column-major
};

struct NifShape
{
	QString name;
	int numVerts = 0, numTris = 0, stride = 0;
	quint32 va = 0;
	QVector<Vector3> verts, norms;
	QVector<Vector2> uvs;
	QVector<quint32> tris;
	QVector<GltfExportVertexSkin> skin;
	QVector<NifSkinBone> bones;
	QStringList segments;                     //!< the BSSubIndexTriShape segment table, merged into one primitive
	QString diffuse;                          //!< normalised to textures/... with forward slashes
	QString diffuseRaw;                       //!< exactly what the NIF stored
	Vector3 t;
	float r[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
	float s = 1.0f;
};

struct Nif
{
	QString error;
	QVector<NifNode> nodes;                   // in block order, remapped
	QVector<NifShape> shapes;
};

const quint32 VA_VERTEX = 0x001, VA_UV = 0x002, VA_UV2 = 0x004, VA_NORMALS = 0x008,
	VA_TANGENTS = 0x010, VA_COLORS = 0x020, VA_SKINNED = 0x040, VA_EYEDATA = 0x100,
	VA_FULLPREC = 0x400;

float halfToFloat( quint16 h )
{
	const int s = ( h >> 15 ) & 1, e = ( h >> 10 ) & 0x1F, m = h & 0x3FF;
	double v;
	if ( e == 0 )
		v = ( m / 1024.0 ) * std::pow( 2.0, -14 );
	else if ( e == 31 )
		v = m ? std::nan( "" ) : std::numeric_limits<double>::infinity();
	else
		v = ( 1.0 + m / 1024.0 ) * std::pow( 2.0, e - 15 );
	return float( s ? -v : v );
}

class Reader
{
public:
	Reader( const QByteArray & d ) : b( d ) {}
	const QByteArray & b;
	qint64 o = 0;
	bool bad = false;
	bool want( qint64 n ) { if ( o < 0 || o + n > b.size() ) { bad = true; return false; } return true; }
	quint32 u32() { if ( !want( 4 ) ) return 0; quint32 v; std::memcpy( &v, b.constData() + o, 4 ); o += 4; return v; }
	qint32 i32() { return qint32( u32() ); }
	quint16 u16() { if ( !want( 2 ) ) return 0; quint16 v; std::memcpy( &v, b.constData() + o, 2 ); o += 2; return v; }
	quint8 u8() { if ( !want( 1 ) ) return 0; return quint8( b[int( o++ )] ); }
	float f32() { const quint32 v = u32(); float f; std::memcpy( &f, &v, 4 ); return f; }
	quint64 u64() { const quint64 lo = u32(); return lo | ( quint64( u32() ) << 32 ); }
	QString sized() { const quint32 n = u32(); if ( !want( n ) ) return QString(); QString s = QString::fromLatin1( b.constData() + o, int( n ) ); o += n; return s; }
	QString shortStr() { const quint32 n = u8(); if ( !want( n ) ) return QString(); QString s = QString::fromLatin1( b.constData() + o, int( n ) ); o += n; return s; }
};

bool isNodeType( const QString & t )
{
	return t == "NiNode" || t == "BSFadeNode" || t == "BSLeafAnimNode" || t == "BSTreeNode"
		|| t == "BSOrderedNode" || t == "BSMultiBoundNode" || t == "BSValueNode";
}
bool isShapeType( const QString & t )
{
	return t == "BSTriShape" || t == "BSSubIndexTriShape" || t == "BSMeshLODTriShape" || t == "BSDynamicTriShape";
}

Nif nifLoad( const QString & path )
{
	Nif n;
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) ) {
		n.error = QStringLiteral( "cannot read %1: %2" ).arg( path, f.errorString() );
		return n;
	}
	const QByteArray data = f.readAll();
	f.close();
	const int nl = data.indexOf( '\n' );
	if ( nl < 0 ) {
		n.error = QStringLiteral( "%1: no header line" ).arg( path );
		return n;
	}
	Reader r( data );
	r.o = nl + 1;
	const quint32 ver = r.u32();
	r.u8();                                   // endian
	r.u32();                                  // user version
	const quint32 numBlocks = r.u32();
	const quint32 bsver = r.u32();
	if ( ver != 0x14020007 || bsver != 130 ) {
		n.error = QStringLiteral( "%1: NIF version %2 BS version %3, not a Fallout 4 file (20.2.0.7 / 130)" )
			.arg( path ).arg( ver, 0, 16 ).arg( bsver );
		return n;
	}
	r.shortStr(); r.shortStr(); r.shortStr(); r.shortStr();   // author, process, export, max path
	const quint32 numTypes = r.u16();
	QVector<QString> types;
	types.resize( int( numTypes ) );
	for ( quint32 i = 0; i < numTypes; i++ )
		types[int( i )] = r.sized();
	QVector<quint16> typeIndex;
	typeIndex.resize( int( numBlocks ) );
	for ( quint32 i = 0; i < numBlocks; i++ )
		typeIndex[int( i )] = r.u16();
	QVector<quint32> blockSize;
	blockSize.resize( int( numBlocks ) );
	for ( quint32 i = 0; i < numBlocks; i++ )
		blockSize[int( i )] = r.u32();
	const quint32 numStrings = r.u32();
	r.u32();                                  // max string length
	QVector<QString> strings;
	strings.resize( int( numStrings ) );
	for ( quint32 i = 0; i < numStrings; i++ )
		strings[int( i )] = r.sized();
	const quint32 numGroups = r.u32();
	for ( quint32 i = 0; i < numGroups; i++ )
		r.u32();
	if ( r.bad ) {
		n.error = QStringLiteral( "%1: the header runs past the end of the file" ).arg( path );
		return n;
	}

	QVector<qint64> start;
	start.resize( int( numBlocks ) );
	qint64 off = r.o;
	for ( quint32 i = 0; i < numBlocks; i++ ) {
		start[int( i )] = off;
		off += blockSize[int( i )];
	}
	{
		// the footer: Num Roots + that many root refs, and nothing else
		Reader fr( data );
		fr.o = off;
		const quint32 numRoots = fr.u32();
		for ( quint32 i = 0; i < numRoots; i++ )
			fr.u32();
		if ( fr.bad || fr.o != data.size() ) {
			n.error = QStringLiteral( "%1: the block table ends at %2 and %3 root refs take it to %4, the file is %5 bytes" )
				.arg( path ).arg( off ).arg( numRoots ).arg( fr.o ).arg( data.size() );
			return n;
		}
	}
	auto str = [&]( quint32 i ) { return i < numStrings ? strings[int( i )] : QString(); };

	// pass 1: nodes, shapes, texture sets, shader properties, skins
	QMap<int, int> nodeOfBlock;
	QVector<QVector<int>> kids;
	QVector<QStringList> texSets;
	QMap<int, int> texSetOfBlock;
	QMap<int, int> shaderTexSet;              // shader property block -> texture set block
	QMap<int, QString> shaderDirect;          // effect shader block -> its Source Texture
	QMap<int, QPair<int, QVector<int>>> skinInst;   // block -> (bone data block, bone node blocks)
	QMap<int, QVector<NifSkinBone>> boneData;       // block -> transforms (names filled later)
	QVector<int> shapeBlocks;

	for ( quint32 i = 0; i < numBlocks; i++ ) {
		const QString & t = types[typeIndex[int( i )]];
		Reader br( data );
		br.o = start[int( i )];
		if ( isNodeType( t ) ) {
			NifNode nd;
			nd.name = str( br.u32() );
			const quint32 ne = br.u32();
			br.o += 4 * ne;
			br.u32();                          // controller
			br.u32();                          // flags
			// Three reads in one argument list are UNSEQUENCED: g++ evaluates
			// right to left and the vector comes out reversed. Measured
			// 2026-09-10 by gate R2 -- every bone's x and z were swapped.
			const float ntx = br.f32(), nty = br.f32(), ntz = br.f32();
			nd.t = Vector3( ntx, nty, ntz );
			for ( int k = 0; k < 9; k++ )
				nd.r[k] = br.f32();
			nd.s = br.f32();
			br.u32();                          // collision
			const quint32 nc = br.u32();
			QVector<int> ch;
			for ( quint32 c = 0; c < nc; c++ )
				ch.append( int( br.i32() ) );
			nodeOfBlock[int( i )] = int( n.nodes.size() );
			n.nodes.append( nd );
			kids.append( ch );
		} else if ( isShapeType( t ) ) {
			shapeBlocks.append( int( i ) );
		} else if ( t == "BSShaderTextureSet" ) {
			const quint32 nt = br.u32();
			QStringList lst;
			for ( quint32 k = 0; k < nt; k++ )
				lst << br.sized();
			texSetOfBlock[int( i )] = int( texSets.size() );
			texSets.append( lst );
		} else if ( t == "BSSkin::Instance" ) {
			br.u32();                          // skeleton root
			const int dataRef = br.i32();
			const quint32 nb = br.u32();
			QVector<int> bones;
			for ( quint32 k = 0; k < nb; k++ )
				bones.append( int( br.i32() ) );
			skinInst[int( i )] = qMakePair( dataRef, bones );
		} else if ( t == "BSSkin::BoneData" ) {
			const quint32 nb = br.u32();
			QVector<NifSkinBone> lst;
			for ( quint32 k = 0; k < nb; k++ ) {
				for ( int q = 0; q < 4; q++ )
					br.f32();                  // bounding sphere
				float rot[9];
				for ( int q = 0; q < 9; q++ )
					rot[q] = br.f32();
				const float tx = br.f32(), ty = br.f32(), tz = br.f32(), sc = br.f32();
				NifSkinBone sb;
				// row-major 3x3 (v' = R v) -> glTF column-major 4x4
				for ( int c = 0; c < 3; c++ )
					for ( int rw = 0; rw < 3; rw++ )
						sb.ibm[c * 4 + rw] = rot[rw * 3 + c] * sc;
				sb.ibm[12] = tx; sb.ibm[13] = ty; sb.ibm[14] = tz; sb.ibm[15] = 1.0f;
				sb.ibm[3] = sb.ibm[7] = sb.ibm[11] = 0.0f;
				lst.append( sb );
			}
			boneData[int( i )] = lst;
		} else if ( t == "BSLightingShaderProperty" || t == "BSEffectShaderProperty" ) {
			// The FO4 (BS version 130) shader-property prefix, from nif.xml:
			// NiObjectNET carries a leading `Shader Type` uint that exists
			// ONLY on BSLightingShaderProperty (onlyT=, vercond
			// #BS_GTE_SKY# #AND# #NI_BS_LTE_FO4#), then Name, Num Extra Data
			// List + that many refs, Controller. BSShaderProperty adds
			// nothing at 130 (all of its fields are #NI_BS_LTE_FO3#), then
			// Shader Flags 1/2, UV Offset, UV Scale, and finally the
			// Texture Set ref (lighting) or the Source Texture sized string
			// (effect). See docs/GLTF_INTERCHANGE.md, "the diffuse".
			if ( t == "BSLightingShaderProperty" )
				br.u32();                      // Shader Type
			br.u32();                          // Name
			const quint32 nx = br.u32();       // Num Extra Data List
			br.o += 4 * qint64( nx );
			br.u32();                          // Controller
			br.u32(); br.u32();                // Shader Flags 1, 2
			br.o += 16;                        // UV Offset, UV Scale
			if ( t == "BSLightingShaderProperty" ) {
				const int ts = br.i32();
				if ( ts >= 0 && ts < int( numBlocks ) && types[typeIndex[ts]] == "BSShaderTextureSet" )
					shaderTexSet[int( i )] = ts;
				else if ( ts >= 0 )
					std::printf( "  shader block %u names block %d as its texture set, which is a %s\n",
						i, ts, qPrintable( ts < int( numBlocks ) ? types[typeIndex[ts]] : QStringLiteral( "block out of range" ) ) );
			} else {
				const QString src = br.sized();
				if ( !src.isEmpty() )
					shaderDirect[int( i )] = src;
			}
		}
		if ( br.bad ) {
			n.error = QStringLiteral( "%1: block %2 (%3) runs past the end of the file" ).arg( path ).arg( i ).arg( t );
			return n;
		}
	}
	for ( auto it = nodeOfBlock.constBegin(); it != nodeOfBlock.constEnd(); ++it ) {
		for ( int c : kids[it.value()] )
			if ( nodeOfBlock.contains( c ) )
				n.nodes[nodeOfBlock[c]].parent = it.value();
	}
	for ( int i = 0; i < n.nodes.size(); i++ )
		if ( i < kids.size() )
			n.nodes[i].children = kids[i];

	// pass 2: the shapes
	for ( int bi : shapeBlocks ) {
		const QString & t = types[typeIndex[bi]];
		Reader br( data );
		br.o = start[bi];
		NifShape sh;
		sh.name = str( br.u32() );
		const quint32 ne = br.u32();
		br.o += 4 * ne;
		br.u32();                              // controller
		br.u32();                              // flags
		const float stx = br.f32(), sty = br.f32(), stz = br.f32();   // unsequenced, see nd.t above
		sh.t = Vector3( stx, sty, stz );
		for ( int k = 0; k < 9; k++ )
			sh.r[k] = br.f32();
		sh.s = br.f32();
		br.u32();                              // collision
		br.o += 16;                            // bounding sphere
		const int skinRef = br.i32();
		const int shaderRef = br.i32();
		br.i32();                              // alpha property
		const quint64 desc = br.u64();
		sh.numTris = int( br.u32() );
		sh.numVerts = int( br.u16() );
		const quint32 dataSize = br.u32();
		sh.stride = int( desc & 0xF ) * 4;
		sh.va = quint32( ( desc >> 44 ) & 0xFFF );
		const quint32 want = quint32( sh.numVerts * sh.stride + sh.numTris * 6 );
		if ( dataSize != want ) {
			n.error = QStringLiteral( "%1: shape '%2' Data Size %3, but %4 vertices x %5 + %6 triangles x 6 = %7" )
				.arg( path, sh.name ).arg( dataSize ).arg( sh.numVerts ).arg( sh.stride ).arg( sh.numTris ).arg( want );
			return n;
		}
		const qint64 base = br.o;
		for ( int v = 0; v < sh.numVerts; v++ ) {
			Reader vr( data );
			vr.o = base + qint64( v ) * sh.stride;
			if ( sh.va & VA_VERTEX ) {
				if ( sh.va & VA_FULLPREC ) {
					const float vx = vr.f32(), vy = vr.f32(), vz = vr.f32();   // unsequenced, see nd.t above
					sh.verts.append( Vector3( vx, vy, vz ) );
					vr.u32();
				} else {
					const float x = halfToFloat( vr.u16() ), y = halfToFloat( vr.u16() ), z = halfToFloat( vr.u16() );
					sh.verts.append( Vector3( x, y, z ) );
					vr.u16();
				}
			}
			if ( sh.va & VA_UV ) {
				const float u = halfToFloat( vr.u16() ), vv = halfToFloat( vr.u16() );
				sh.uvs.append( Vector2( u, vv ) );
			}
			if ( sh.va & VA_UV2 )
				vr.u32();
			if ( sh.va & VA_NORMALS ) {
				const float nx = vr.u8() / 255.0f * 2.0f - 1.0f;
				const float ny = vr.u8() / 255.0f * 2.0f - 1.0f;
				const float nz = vr.u8() / 255.0f * 2.0f - 1.0f;
				sh.norms.append( Vector3( nx, ny, nz ) );
				vr.u8();
			}
			if ( sh.va & VA_TANGENTS )
				vr.u32();
			if ( sh.va & VA_COLORS )
				vr.u32();
			if ( sh.va & VA_SKINNED ) {
				GltfExportVertexSkin vs;
				for ( int k = 0; k < 4; k++ )
					vs.weights[k] = halfToFloat( vr.u16() );
				for ( int k = 0; k < 4; k++ )
					vs.joints[k] = vr.u8();
				sh.skin.append( vs );
			}
			if ( sh.va & VA_EYEDATA )
				vr.u32();
			if ( vr.bad || vr.o - ( base + qint64( v ) * sh.stride ) != sh.stride ) {
				n.error = QStringLiteral( "%1: shape '%2' vertex %3 walked %4 of %5 bytes (attributes %6)" )
					.arg( path, sh.name ).arg( v ).arg( vr.o - ( base + qint64( v ) * sh.stride ) )
					.arg( sh.stride ).arg( sh.va, 0, 16 );
				return n;
			}
		}
		br.o = base + qint64( sh.numVerts ) * sh.stride;
		for ( int k = 0; k < sh.numTris * 3; k++ )
			sh.tris.append( br.u16() );
		if ( br.bad ) {
			n.error = QStringLiteral( "%1: shape '%2' triangle list runs past the end of the file" ).arg( path, sh.name );
			return n;
		}
		if ( t == "BSSubIndexTriShape" && dataSize > 0 ) {
			// The segments are DRAW RANGES over the one vertex + index buffer
			// already read, so they all merge into a single glTF primitive.
			// The table is read anyway, checked against Num Triangles, and
			// written into the primitive's extras. nif.xml: Num Primitives,
			// Num Segments, Total Segments, then per segment {Start Index,
			// Num Primitives, Parent Array Index, Num Sub Segments,
			// subs x {Start Index, Num Primitives, Parent Array Index,
			// Unused}}, then, only when Num Segments < Total Segments, the
			// shared block {Num Segments, Total Segments, Segment Starts[],
			// per-segment {User Index, Bone ID, Num Cut Offsets, floats[]},
			// SSF File (u16-sized)}.
			const quint32 nPrim = br.u32();
			const quint32 nSeg = br.u32();
			const quint32 nTot = br.u32();
			quint32 covered = 0;
			for ( quint32 s = 0; s < nSeg && !br.bad; s++ ) {
				const quint32 si = br.u32(), sp = br.u32(), pa = br.u32(), nsub = br.u32();
				covered += sp;
				quint32 subCovered = 0;
				QString line = QStringLiteral( "segment %1: startIndex %2, %3 triangles, parentArrayIndex %4" )
					.arg( s ).arg( si ).arg( sp ).arg( pa );
				for ( quint32 u = 0; u < nsub && !br.bad; u++ ) {
					const quint32 ui = br.u32(), up = br.u32(), upa = br.u32();
					br.u32();                  // Unused
					subCovered += up;
					line += QStringLiteral( "; sub %1: startIndex %2, %3 triangles, parentArrayIndex %4" )
						.arg( u ).arg( ui ).arg( up ).arg( upa );
				}
				// A segment's sub-segments RE-DESCRIBE that segment's own
				// range, they do not extend it: their primitive counts sum
				// back to the segment's own. (Measured, 2026-09-10: counting
				// both levels gave 4,351 against 2,698 real triangles.)
				if ( nsub && subCovered != sp ) {
					n.error = QStringLiteral( "%1: shape '%2' segment %3 owns %4 triangles, its %5 "
						"sub-segments cover %6" ).arg( path, sh.name ).arg( s ).arg( sp ).arg( nsub ).arg( subCovered );
					return n;
				}
				sh.segments << line;
			}
			if ( nSeg < nTot && !br.bad ) {
				br.u32(); br.u32();            // Num Segments, Total Segments (repeated)
				for ( quint32 s = 0; s < nSeg; s++ )
					br.u32();                  // Segment Starts
				for ( quint32 s = 0; s < nTot && !br.bad; s++ ) {
					br.u32(); br.u32();        // User Index, Bone ID
					const quint32 nc = br.u32();
					br.o += 4 * qint64( nc );  // Cut Offsets
				}
				const quint32 ssfLen = br.u16();
				QString ssf;
				if ( br.want( ssfLen ) ) {
					ssf = QString::fromLatin1( data.constData() + br.o, int( ssfLen ) );
					br.o += ssfLen;
				}
				sh.segments << QStringLiteral( "SSF file: %1" ).arg( ssf.isEmpty() ? QStringLiteral( "(none)" ) : ssf );
			}
			if ( br.bad || br.o != start[bi] + qint64( blockSize[bi] ) ) {
				n.error = QStringLiteral( "%1: shape '%2' segment table ends at %3, the block ends at %4" )
					.arg( path, sh.name ).arg( br.o ).arg( start[bi] + qint64( blockSize[bi] ) );
				return n;
			}
			// The invariant that fails on a wrong walk: the top-level segments
			// partition the index buffer, so their Num Primitives sum to the
			// shape's triangle count and to the header's Num Primitives.
			if ( covered != quint32( sh.numTris ) || nPrim != quint32( sh.numTris ) ) {
				n.error = QStringLiteral( "%1: shape '%2' has %3 triangles, its header says %4 primitives and "
					"its %5 segments cover %6" )
					.arg( path, sh.name ).arg( sh.numTris ).arg( nPrim ).arg( nSeg ).arg( covered );
				return n;
			}
		}
		if ( skinInst.contains( skinRef ) ) {
			const auto & si = skinInst[skinRef];
			const QVector<NifSkinBone> & bd = boneData.value( si.first );
			if ( bd.size() != si.second.size() ) {
				n.error = QStringLiteral( "%1: shape '%2' skin names %3 bones, its bone data has %4" )
					.arg( path, sh.name ).arg( si.second.size() ).arg( bd.size() );
				return n;
			}
			for ( int k = 0; k < bd.size(); k++ ) {
				NifSkinBone sb = bd[k];
				const int blk = si.second[k];
				sb.name = nodeOfBlock.contains( blk ) ? n.nodes[nodeOfBlock[blk]].name
					: QStringLiteral( "?block%1" ).arg( blk );
				sh.bones.append( sb );
			}
		} else if ( !sh.skin.isEmpty() ) {
			n.error = QStringLiteral( "%1: shape '%2' carries weights but no BSSkin::Instance" ).arg( path, sh.name );
			return n;
		}
		if ( shaderTexSet.contains( shaderRef ) ) {
			const QStringList & tex = texSets[texSetOfBlock[shaderTexSet[shaderRef]]];
			if ( !tex.isEmpty() )
				sh.diffuse = tex.first();
		} else if ( shaderDirect.contains( shaderRef ) ) {
			sh.diffuse = shaderDirect[shaderRef];
		}
		sh.diffuseRaw = sh.diffuse;
		// The NIF stores a game-relative path, sometimes with the leading
		// "textures\" and sometimes without, with either slash and any case.
		// Normalise it to one form so the glTF's image uris are consistent;
		// the raw NIF string is kept in the material's extras by the writer.
		if ( !sh.diffuse.isEmpty() ) {
			QString p = sh.diffuse;
			p.replace( '\\', '/' );
			while ( p.startsWith( '/' ) )
				p.remove( 0, 1 );
			if ( p.startsWith( QStringLiteral( "textures/" ), Qt::CaseInsensitive ) )
				p = QStringLiteral( "textures/" ) + p.mid( 9 );
			else
				p = QStringLiteral( "textures/" ) + p;
			sh.diffuse = p;
		}
		n.shapes.append( sh );
	}
	return n;
}

//! Row-major 3x3 (v' = R v) -> NifSkope Quat (w, x, y, z). Shepperd's method.
Quat matToQuat( const float * r )
{
	const double tr = double( r[0] ) + r[4] + r[8];
	double w, x, y, z;
	if ( tr > 0.0 ) {
		double s = std::sqrt( tr + 1.0 ) * 2.0;
		w = 0.25 * s;
		x = ( double( r[7] ) - r[5] ) / s;
		y = ( double( r[2] ) - r[6] ) / s;
		z = ( double( r[3] ) - r[1] ) / s;
	} else if ( r[0] > r[4] && r[0] > r[8] ) {
		double s = std::sqrt( 1.0 + double( r[0] ) - r[4] - r[8] ) * 2.0;
		w = ( double( r[7] ) - r[5] ) / s;
		x = 0.25 * s;
		y = ( double( r[1] ) + r[3] ) / s;
		z = ( double( r[2] ) + r[6] ) / s;
	} else if ( r[4] > r[8] ) {
		double s = std::sqrt( 1.0 + double( r[4] ) - r[0] - r[8] ) * 2.0;
		w = ( double( r[2] ) - r[6] ) / s;
		x = ( double( r[1] ) + r[3] ) / s;
		y = 0.25 * s;
		z = ( double( r[5] ) + r[7] ) / s;
	} else {
		double s = std::sqrt( 1.0 + double( r[8] ) - r[0] - r[4] ) * 2.0;
		w = ( double( r[3] ) - r[1] ) / s;
		x = ( double( r[2] ) + r[6] ) / s;
		y = ( double( r[5] ) + r[7] ) / s;
		z = 0.25 * s;
	}
	const double l = std::sqrt( w * w + x * x + y * y + z * z );
	return Quat( float( w / l ), float( x / l ), float( y / l ), float( z / l ) );
}

} // namespace

int main( int argc, char ** argv )
{
	QCoreApplication app( argc, argv );
	const QStringList args = app.arguments();
	QString skeletonPath, clipPath, bonesPath, outPath, animName;
	QStringList meshPaths;
	bool rootMotion = false;
	for ( int i = 1; i < args.size(); i++ ) {
		const QString & a = args[i];
		auto next = [&]() { return i + 1 < args.size() ? args[++i] : QString(); };
		if ( a == "--skeleton" ) skeletonPath = next();
		else if ( a == "--mesh" ) meshPaths << next();
		else if ( a == "--clip" ) clipPath = next();
		else if ( a == "--bones" ) bonesPath = next();
		else if ( a == "--out" ) outPath = next();
		else if ( a == "--name" ) animName = next();
		else if ( a == "--root-motion" ) rootMotion = true;
		else {
			std::fprintf( stderr, "unknown argument %s\n", qPrintable( a ) );
			return 2;
		}
	}
	if ( skeletonPath.isEmpty() || outPath.isEmpty() ) {
		std::fprintf( stderr, "usage: gltfexport_dump --skeleton S.nif [--mesh M.nif ...] "
			"[--clip C.hkx --bones skeleton.hkx] [--name NAME] [--root-motion] --out out.gltf\n" );
		return 2;
	}

	GltfExportScene scene;
	scene.generator = QStringLiteral( "NifSkope Wild Wasteland Edition, gltfexport (lane HKX4 standalone driver)" );

	// ---- the skeleton --------------------------------------------------
	const Nif sk = nifLoad( skeletonPath );
	if ( !sk.error.isEmpty() ) {
		std::printf( "REFUSED: %s\n", qPrintable( sk.error ) );
		return 2;
	}
	QHash<QString, int> nodeByName;
	for ( int i = 0; i < sk.nodes.size(); i++ ) {
		GltfExportNode gn;
		gn.name = sk.nodes[i].name;
		gn.parent = sk.nodes[i].parent;
		gn.translation = sk.nodes[i].t;
		gn.rotation = matToQuat( sk.nodes[i].r );
		gn.scale = Vector3( sk.nodes[i].s, sk.nodes[i].s, sk.nodes[i].s );
		scene.nodes.append( gn );
		const QString key = gn.name.toLower();
		if ( !nodeByName.contains( key ) )
			nodeByName.insert( key, i );
	}
	int sceneRoot = 0;
	for ( int i = 0; i < scene.nodes.size(); i++ )
		if ( scene.nodes[i].parent < 0 ) { sceneRoot = i; break; }
	std::printf( "skeleton %s: %d nodes, root '%s'\n", qPrintable( QFileInfo( skeletonPath ).fileName() ),
		int( scene.nodes.size() ), qPrintable( scene.nodes[sceneRoot].name ) );

	// ---- the meshes ----------------------------------------------------
	int addedBoneNodes = 0;
	for ( const QString & mp : meshPaths ) {
		const Nif mn = nifLoad( mp );
		if ( !mn.error.isEmpty() ) {
			std::printf( "REFUSED: %s\n", qPrintable( mn.error ) );
			return 2;
		}
		for ( const NifShape & sh : mn.shapes ) {
			GltfExportMesh gm;
			gm.name = sh.name;
			gm.positions = sh.verts;
			gm.normals = sh.norms;
			gm.texCoords = sh.uvs;
			gm.indices = sh.tris;
			gm.skin = sh.skin;
			gm.materialName = sh.name + "_material";
			gm.diffuseUri = sh.diffuse;
			gm.diffuseSourcePath = sh.diffuseRaw;
			gm.mergedPartitions = sh.segments;

			GltfExportNode gn;
			gn.name = sh.name;
			gn.parent = sceneRoot;
			if ( sh.skin.isEmpty() ) {
				gn.translation = sh.t;
				gn.rotation = matToQuat( sh.r );
				gn.scale = Vector3( sh.s, sh.s, sh.s );
			}
			gm.node = int( scene.nodes.size() );
			scene.nodes.append( gn );

			for ( const NifSkinBone & sb : sh.bones ) {
				int j = nodeByName.value( sb.name.toLower(), -1 );
				if ( j < 0 ) {
					// FALLBACK, named in the output: a bone the skeleton file
					// does not carry becomes a child of the scene root at its
					// bind position, so the mesh still skins.
					GltfExportNode bn;
					bn.name = sb.name;
					bn.parent = sceneRoot;
					j = int( scene.nodes.size() );
					scene.nodes.append( bn );
					nodeByName.insert( sb.name.toLower(), j );
					addedBoneNodes++;
					std::printf( "  bone '%s' is not in the skeleton file: added at the root, unposed\n",
						qPrintable( sb.name ) );
				}
				gm.joints.append( j );
				for ( int k = 0; k < 16; k++ )
					gm.inverseBind.append( sb.ibm[k] );
			}
			double wmin = 9e9, wmax = -9e9;
			for ( const GltfExportVertexSkin & vs : gm.skin ) {
				const double s = double( vs.weights[0] ) + vs.weights[1] + vs.weights[2] + vs.weights[3];
				wmin = qMin( wmin, s );
				wmax = qMax( wmax, s );
			}
			std::printf( "mesh '%s': %d vertices, %d triangles, %d joints, %d segment rows merged into one "
				"primitive, weight sums %.6f..%.6f, diffuse '%s' (NIF stored '%s')\n",
				qPrintable( gm.name ), int( gm.positions.size() ), int( gm.indices.size() / 3 ),
				int( gm.joints.size() ), int( gm.mergedPartitions.size() ),
				gm.skin.isEmpty() ? 1.0 : wmin, gm.skin.isEmpty() ? 1.0 : wmax,
				qPrintable( gm.diffuseUri ), qPrintable( gm.diffuseSourcePath ) );
			scene.meshes.append( gm );
		}
	}

	// ---- the clip ------------------------------------------------------
	if ( !clipPath.isEmpty() ) {
		QStringList boneNames;
		if ( !bonesPath.isEmpty() ) {
			const HkxAnimFile bf = hkxAnimLoad( bonesPath );
			if ( !bf.ok() ) {
				std::printf( "REFUSED (bones): %s\n", qPrintable( bf.error ) );
				return 2;
			}
			if ( bf.skeletons.isEmpty() ) {
				std::printf( "REFUSED: %s carries no hkaSkeleton\n", qPrintable( bonesPath ) );
				return 2;
			}
			boneNames = bf.skeletons.first().boneNames;
			std::printf( "bone names: %d from %s ('%s')\n", int( boneNames.size() ),
				qPrintable( QFileInfo( bonesPath ).fileName() ), qPrintable( bf.skeletons.first().name ) );
		}
		const HkxAnimFile cf = hkxAnimLoad( clipPath );
		if ( !cf.ok() ) {
			std::printf( "REFUSED (clip): %s\n", qPrintable( cf.error ) );
			return 2;
		}
		if ( cf.clips.isEmpty() ) {
			std::printf( "REFUSED: %s carries no clip\n", qPrintable( clipPath ) );
			return 2;
		}
		const HkxAnimClip & c = cf.clips.first();
		GltfExportAnimation an;
		an.name = animName.isEmpty() ? QFileInfo( clipPath ).completeBaseName() : animName;
		an.numFrames = c.numFrames;
		an.frameDuration = c.frameDuration;

		int matched = 0;
		for ( int t = 0; t < c.numTracks; t++ ) {
			const int bone = t < c.trackToBone.size() ? c.trackToBone[t] : t;
			QString name = ( bone >= 0 && bone < boneNames.size() ) ? boneNames[bone] : QString();
			if ( name.isEmpty() ) {
				an.unmatchedTracks << QStringLiteral( "track %1 (bone index %2, no name)" ).arg( t ).arg( bone );
				continue;
			}
			const int node = nodeByName.value( name.toLower(), -1 );
			if ( node < 0 ) {
				an.unmatchedTracks << name;
				continue;
			}
			GltfExportChannel ch;
			ch.node = node;
			ch.translations.reserve( c.numFrames );
			ch.rotations.reserve( c.numFrames );
			ch.scales.reserve( c.numFrames );
			for ( int fr = 0; fr < c.numFrames; fr++ ) {
				const HkxTransform & x = c.frames[fr][t];
				ch.translations.append( x.translation );
				ch.rotations.append( x.rotation );
				ch.scales.append( x.scale );
			}
			an.channels.append( ch );
			matched++;
		}
		if ( !c.rootMotion.isEmpty() ) {
			int rn = -1;
			if ( !boneNames.isEmpty() )
				rn = nodeByName.value( boneNames.first().toLower(), -1 );
			if ( rn < 0 )
				rn = sceneRoot;
			an.rootMotionNode = rn;
			an.applyRootMotion = rootMotion;
			for ( const HkxRootMotion & rm : c.rootMotion ) {
				an.rootMotionTranslation.append( rm.translation );
				an.rootMotionYaw.append( rm.yaw );
			}
			an.rootMotionUp = c.rootMotionUp;
			// a root-motion node needs channels of its own to be composed onto
			bool have = false;
			for ( const GltfExportChannel & ch : an.channels )
				if ( ch.node == rn ) { have = true; break; }
			if ( rootMotion && !have ) {
				GltfExportChannel ch;
				ch.node = rn;
				for ( int fr = 0; fr < c.numFrames; fr++ ) {
					ch.translations.append( scene.nodes[rn].translation );
					ch.rotations.append( scene.nodes[rn].rotation );
				}
				an.channels.append( ch );
			}
		}
		std::printf( "clip '%s': %d frames at %.4f s (%.2f fps), %d tracks, %d matched, %d unmatched, "
			"root motion %s%s\n",
			qPrintable( an.name ), an.numFrames, double( an.frameDuration ),
			an.frameDuration > 0.0f ? 1.0 / double( an.frameDuration ) : 0.0,
			c.numTracks, matched, int( an.unmatchedTracks.size() ),
			c.rootMotion.isEmpty() ? "absent" : "present", rootMotion ? ", APPLIED" : ", omitted" );
		for ( const QString & u : an.unmatchedTracks )
			std::printf( "  unmatched track: %s\n", qPrintable( u ) );
		scene.animations.append( an );
	}

	QString error;
	if ( !gltfExportWrite( scene, outPath, error ) ) {
		std::printf( "REFUSED: %s\n", qPrintable( error ) );
		return 2;
	}
	const QFileInfo gi( outPath ), bi( gltfExportBinPath( outPath ) );
	std::printf( "wrote %s (%lld bytes) and %s (%lld bytes); %d nodes, %d meshes, %d animations, "
		"%d bone nodes added\n",
		qPrintable( gi.fileName() ), gi.size(), qPrintable( bi.fileName() ), bi.size(),
		int( scene.nodes.size() ), int( scene.meshes.size() ), int( scene.animations.size() ), addedBoneNodes );
	return 0;
}
