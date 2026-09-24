/* NifModel -> GltfExportScene. See src/gltfexportnif.h for what this is and
   docs/GLTF_INTERCHANGE.md for the contract. Lane HKX4b, 2026-09-10.

   The field names below are nif.xml's own; the reference implementation for
   the FO4 vertex and skin reads is src/gl/bsshape.cpp (BSShape::updateData),
   which this follows so the two cannot drift. Nothing here touches the Scene
   or GL, so the export works on a model that was never drawn. */

#include "gltfexportnif.h"

#include "model/nifmodel.h"

#include <QFileInfo>

namespace {

const char * const NODE_TYPE = "NiNode";
const char * const SHAPE_TYPE = "BSTriShape";

//! Row-major Matrix (v' = M v) -> NifSkope Quat (w, x, y, z). Shepperd's
//! method: the branch with the largest pivot, so no square root of a small
//! difference. The same arithmetic is in tests/gltfexport_dump.cpp and in
//! tests/spells/gltf_readback.py, which is how it is checked.
Quat matrixToQuat( const Matrix & m )
{
	const double r00 = m( 0, 0 ), r01 = m( 0, 1 ), r02 = m( 0, 2 );
	const double r10 = m( 1, 0 ), r11 = m( 1, 1 ), r12 = m( 1, 2 );
	const double r20 = m( 2, 0 ), r21 = m( 2, 1 ), r22 = m( 2, 2 );
	const double tr = r00 + r11 + r22;
	double w, x, y, z;
	if ( tr > 0.0 ) {
		const double s = std::sqrt( tr + 1.0 ) * 2.0;
		w = 0.25 * s; x = ( r21 - r12 ) / s; y = ( r02 - r20 ) / s; z = ( r10 - r01 ) / s;
	} else if ( r00 > r11 && r00 > r22 ) {
		const double s = std::sqrt( 1.0 + r00 - r11 - r22 ) * 2.0;
		w = ( r21 - r12 ) / s; x = 0.25 * s; y = ( r01 + r10 ) / s; z = ( r02 + r20 ) / s;
	} else if ( r11 > r22 ) {
		const double s = std::sqrt( 1.0 + r11 - r00 - r22 ) * 2.0;
		w = ( r02 - r20 ) / s; x = ( r01 + r10 ) / s; y = 0.25 * s; z = ( r12 + r21 ) / s;
	} else {
		const double s = std::sqrt( 1.0 + r22 - r00 - r11 ) * 2.0;
		w = ( r10 - r01 ) / s; x = ( r02 + r20 ) / s; y = ( r12 + r21 ) / s; z = 0.25 * s;
	}
	const double l = std::sqrt( w * w + x * x + y * y + z * z );
	if ( !( l > 0.0 ) )
		return Quat( 1.0f, 0.0f, 0.0f, 0.0f );
	return Quat( float( w / l ), float( x / l ), float( y / l ), float( z / l ) );
}

//! The NIF's stored bone transform, laid out as glTF wants an inverse-bind
//! matrix: COLUMN-major, rotation times scale in the upper 3x3, translation
//! in elements 12..14 and still in NIF UNITS (gltfExportWrite scales it).
void toInverseBind( const Transform & t, QVector<float> & out )
{
	float m[16] = { 0 };
	for ( int c = 0; c < 3; c++ )
		for ( int r = 0; r < 3; r++ )
			m[c * 4 + r] = t.rotation( r, c ) * t.scale;
	m[12] = t.translation[0];
	m[13] = t.translation[1];
	m[14] = t.translation[2];
	m[15] = 1.0f;
	for ( int i = 0; i < 16; i++ )
		out.append( m[i] );
}

//! A NIF texture path is a Windows path, game-relative, with or without the
//! leading "textures\", in any case. One form, so the glTF's image uris are
//! consistent; the raw string is kept in the material's extras.
QString normaliseTexture( const QString & raw )
{
	if ( raw.isEmpty() )
		return raw;
	QString p = raw;
	p.replace( '\\', '/' );
	while ( p.startsWith( '/' ) )
		p.remove( 0, 1 );
	if ( p.startsWith( QStringLiteral( "textures/" ), Qt::CaseInsensitive ) )
		return QStringLiteral( "textures/" ) + p.mid( 9 );
	return QStringLiteral( "textures/" ) + p;
}

QString diffuseOf( const NifModel * nif, const QModelIndex & iShape )
{
	const QModelIndex iShader = nif->getBlockIndex( nif->getLink( iShape, "Shader Property" ) );
	if ( !iShader.isValid() )
		return QString();
	if ( nif->blockInherits( iShader, "BSEffectShaderProperty" ) )
		return nif->get<QString>( iShader, "Source Texture" );
	const QModelIndex iSet = nif->getBlockIndex( nif->getLink( iShader, "Texture Set" ), "BSShaderTextureSet" );
	if ( !iSet.isValid() )
		return QString();
	const QStringList tex = nif->getArray<QString>( iSet, "Textures" );
	return tex.isEmpty() ? QString() : tex.first();
}

//! The BSSubIndexTriShape segment table, as sentences for the primitive's
//! extras. The segments are DRAW RANGES over the one vertex + index buffer,
//! so they all merge into a single glTF primitive; the table is recorded so
//! nothing is lost silently, and its own arithmetic is checked.
QStringList segmentsOf( const NifModel * nif, const QModelIndex & iShape, int numTris, QString & error )
{
	QStringList out;
	if ( !nif->blockInherits( iShape, "BSSubIndexTriShape" ) )
		return out;
	const QModelIndex iSegs = nif->getIndex( iShape, "Segment" );
	if ( !iSegs.isValid() )
		return out;
	const int n = nif->rowCount( iSegs );
	int covered = 0;
	for ( int s = 0; s < n; s++ ) {
		const QModelIndex iSeg = nif->getIndex( iSegs, s );
		const int start = nif->get<int>( iSeg, "Start Index" );
		const int prim = nif->get<int>( iSeg, "Num Primitives" );
		const int parent = nif->get<int>( iSeg, "Parent Array Index" );
		covered += prim;
		QString line = QStringLiteral( "segment %1: startIndex %2, %3 triangles, parentArrayIndex %4" )
			.arg( s ).arg( start ).arg( prim ).arg( parent );
		const QModelIndex iSub = nif->getIndex( iSeg, "Sub Segment" );
		int subCovered = 0;
		const int ns = iSub.isValid() ? nif->rowCount( iSub ) : 0;
		for ( int u = 0; u < ns; u++ ) {
			const QModelIndex iOne = nif->getIndex( iSub, u );
			const int us = nif->get<int>( iOne, "Start Index" );
			const int up = nif->get<int>( iOne, "Num Primitives" );
			const int ua = nif->get<int>( iOne, "Parent Array Index" );
			subCovered += up;
			line += QStringLiteral( "; sub %1: startIndex %2, %3 triangles, parentArrayIndex %4" )
				.arg( u ).arg( us ).arg( up ).arg( ua );
		}
		// A segment's sub-segments RE-DESCRIBE its own range, they do not
		// extend it (measured 2026-09-10 on BaseMaleBody:0: counting both
		// levels gave 4,351 against 2,698 real triangles).
		if ( ns && subCovered != prim ) {
			error = QStringLiteral( "shape '%1' segment %2 owns %3 triangles, its %4 sub-segments cover %5" )
				.arg( nif->get<QString>( iShape, "Name" ) ).arg( s ).arg( prim ).arg( ns ).arg( subCovered );
			return QStringList();
		}
		out << line;
	}
	if ( covered != numTris ) {
		error = QStringLiteral( "shape '%1' has %2 triangles, its %3 segments cover %4" )
			.arg( nif->get<QString>( iShape, "Name" ) ).arg( numTris ).arg( n ).arg( covered );
		return QStringList();
	}
	return out;
}

} // namespace

bool gltfExportNifScene( const NifModel * nif, const QModelIndex & iRoot,
						 GltfExportScene & scene, QHash<QString, int> & nodeByName,
						 GltfExportReport & report, QString & error )
{
	error.clear();
	if ( !nif ) {
		error = QStringLiteral( "no model" );
		return false;
	}
	if ( nif->getBSVersion() < 130 ) {
		error = QStringLiteral( "this exporter reads Fallout 4 files (BS version 130 and up); this one is %1" )
			.arg( nif->getBSVersion() );
		return false;
	}

	// ---- the node tree, depth first from the chosen root(s) -------------
	QVector<QModelIndex> queueIdx;
	QVector<int> queueParent;
	if ( iRoot.isValid() ) {
		queueIdx.append( iRoot );
		queueParent.append( -1 );
	} else {
		for ( const auto & link : nif->getRootLinks() ) {
			const QModelIndex i = nif->getBlockIndex( link );
			if ( i.isValid() ) {
				queueIdx.append( i );
				queueParent.append( -1 );
			}
		}
	}
	if ( queueIdx.isEmpty() ) {
		error = QStringLiteral( "the file has no root block to export" );
		return false;
	}

	QVector<QModelIndex> shapeIdx;
	QVector<int> shapeParent;
	QHash<qint32, int> nodeOfBlock;

	while ( !queueIdx.isEmpty() ) {
		const QModelIndex idx = queueIdx.takeFirst();
		const int parent = queueParent.takeFirst();
		if ( nif->blockInherits( idx, SHAPE_TYPE ) ) {
			shapeIdx.append( idx );
			shapeParent.append( parent );
			continue;
		}
		if ( !nif->blockInherits( idx, NODE_TYPE ) )
			continue;
		const Transform t( nif, idx );
		GltfExportNode gn;
		gn.name = nif->get<QString>( idx, "Name" );
		gn.parent = parent;
		gn.translation = t.translation;
		gn.rotation = matrixToQuat( t.rotation );
		gn.scale = Vector3( t.scale, t.scale, t.scale );
		const int self = int( scene.nodes.size() );
		scene.nodes.append( gn );
		nodeOfBlock.insert( nif->getBlockNumber( idx ), self );
		const QString key = gn.name.toLower();
		if ( !nodeByName.contains( key ) )
			nodeByName.insert( key, self );
		// children last-first so the queue keeps the file's own order
		const auto kids = nif->getLinkArray( idx, "Children" );
		int at = 0;
		for ( const auto & link : kids ) {
			const QModelIndex ci = nif->getBlockIndex( link );
			if ( !ci.isValid() )
				continue;
			queueIdx.insert( at, ci );
			queueParent.insert( at, self );
			at++;
		}
	}
	report.nodes = int( scene.nodes.size() );
	if ( scene.nodes.isEmpty() ) {
		error = QStringLiteral( "the chosen root is not an NiNode and holds no node to hang the export on" );
		return false;
	}
	const int sceneRoot = 0;

	// ---- the shapes -----------------------------------------------------
	for ( int si = 0; si < shapeIdx.size(); si++ ) {
		const QModelIndex idx = shapeIdx[si];
		GltfExportMesh gm;
		gm.name = nif->get<QString>( idx, "Name" );

		const QModelIndex iData = nif->getIndex( idx, "Vertex Data" );
		const int nv = iData.isValid() ? nif->rowCount( iData ) : 0;
		if ( nv < 1 ) {
			report.notes << QStringLiteral( "shape '%1' carries no vertex data and was not exported" ).arg( gm.name );
			continue;
		}
		gm.positions.reserve( nv );
		gm.normals.reserve( nv );
		gm.texCoords.reserve( nv );
		bool anyNormal = false, anyUv = false, anySkin = false;
		for ( int v = 0; v < nv; v++ ) {
			const QModelIndex iv = nif->getIndex( iData, v );
			gm.positions.append( nif->get<Vector3>( iv, "Vertex" ) );
			const QModelIndex iN = nif->getIndex( iv, "Normal" );
			if ( iN.isValid() ) {
				gm.normals.append( nif->get<ByteVector3>( iN ) );
				anyNormal = true;
			} else {
				gm.normals.append( Vector3( 0.0f, 0.0f, 1.0f ) );
			}
			const QModelIndex iU = nif->getIndex( iv, "UV" );
			if ( iU.isValid() ) {
				gm.texCoords.append( nif->get<HalfVector2>( iU ) );
				anyUv = true;
			} else {
				gm.texCoords.append( Vector2( 0.0f, 0.0f ) );
			}
			GltfExportVertexSkin vs;
			const QVector<float> w = nif->getArray<float>( iv, "Bone Weights" );
			const QVector<quint8> b = nif->getArray<quint8>( iv, "Bone Indices" );
			if ( w.size() >= 4 && b.size() >= 4 ) {
				for ( int k = 0; k < 4; k++ ) {
					vs.weights[k] = w[k];
					vs.joints[k] = b[k];
				}
				anySkin = true;
			}
			gm.skin.append( vs );
		}
		if ( !anyNormal )
			gm.normals.clear();
		if ( !anyUv )
			gm.texCoords.clear();
		if ( !anySkin )
			gm.skin.clear();

		const QModelIndex iTri = nif->getIndex( idx, "Triangles" );
		const QVector<Triangle> tris = iTri.isValid() ? nif->getArray<Triangle>( iTri ) : QVector<Triangle>();
		gm.indices.reserve( tris.size() * 3 );
		for ( const Triangle & t : tris ) {
			gm.indices.append( t[0] );
			gm.indices.append( t[1] );
			gm.indices.append( t[2] );
		}
		if ( gm.indices.isEmpty() ) {
			report.notes << QStringLiteral( "shape '%1' has no triangles and was not exported" ).arg( gm.name );
			continue;
		}
		gm.mergedPartitions = segmentsOf( nif, idx, int( tris.size() ), error );
		if ( !error.isEmpty() )
			return false;

		gm.diffuseSourcePath = diffuseOf( nif, idx );
		gm.diffuseUri = normaliseTexture( gm.diffuseSourcePath );
		gm.materialName = gm.name + QStringLiteral( "_material" );

		// the node the mesh hangs on. A skinned shape's node transform is
		// ignored by the glTF spec, so it is written without one.
		GltfExportNode gn;
		gn.name = gm.name;
		gn.parent = shapeParent[si] >= 0 ? shapeParent[si] : sceneRoot;
		if ( !anySkin ) {
			const Transform t( nif, idx );
			gn.translation = t.translation;
			gn.rotation = matrixToQuat( t.rotation );
			gn.scale = Vector3( t.scale, t.scale, t.scale );
		}
		gm.node = int( scene.nodes.size() );
		scene.nodes.append( gn );

		if ( anySkin ) {
			const QModelIndex iSkin = nif->getBlockIndex( nif->getLink( idx, "Skin" ), "BSSkin::Instance" );
			const QModelIndex iBoneData = iSkin.isValid()
				? nif->getBlockIndex( nif->getLink( iSkin, "Data" ), "BSSkin::BoneData" ) : QModelIndex();
			const auto boneLinks = iSkin.isValid() ? nif->getLinkArray( iSkin, "Bones" ) : QVector<qint32>();
			const QModelIndex iList = iBoneData.isValid() ? nif->getIndex( iBoneData, "Bone List" ) : QModelIndex();
			const int nb = iList.isValid() ? nif->rowCount( iList ) : 0;
			if ( boneLinks.size() != nb ) {
				error = QStringLiteral( "shape '%1' names %2 skin bones, its bone data has %3" )
					.arg( gm.name ).arg( boneLinks.size() ).arg( nb );
				return false;
			}
			for ( int k = 0; k < nb; k++ ) {
				const QModelIndex iBoneNode = nif->getBlockIndex( boneLinks[k] );
				const QString bname = iBoneNode.isValid() ? nif->get<QString>( iBoneNode, "Name" )
					: QStringLiteral( "?block%1" ).arg( boneLinks[k] );
				int j = nodeOfBlock.value( boneLinks[k], -1 );
				if ( j < 0 )
					j = nodeByName.value( bname.toLower(), -1 );
				if ( j < 0 ) {
					// A skin bone outside the exported subtree. It becomes a
					// child of the scene root at its bind position so the mesh
					// still skins, and it is NAMED -- never silently dropped.
					GltfExportNode bn;
					bn.name = bname;
					bn.parent = sceneRoot;
					if ( iBoneNode.isValid() ) {
						const Transform bt( nif, iBoneNode );
						bn.translation = bt.translation;
						bn.rotation = matrixToQuat( bt.rotation );
						bn.scale = Vector3( bt.scale, bt.scale, bt.scale );
					}
					j = int( scene.nodes.size() );
					scene.nodes.append( bn );
					nodeByName.insert( bname.toLower(), j );
					report.bonesAdded++;
					report.notes << QStringLiteral( "skin bone '%1' of shape '%2' is not under the exported "
						"root; it was added at the root, unposed" ).arg( bname, gm.name );
				}
				gm.joints.append( j );
				toInverseBind( Transform( nif, nif->getIndex( iList, k ) ), gm.inverseBind );
			}
			report.skinnedShapes++;
		}
		report.shapes++;
		report.vertices += int( gm.positions.size() );
		report.triangles += int( gm.indices.size() / 3 );
		scene.meshes.append( gm );
	}
	report.nodes = int( scene.nodes.size() );
	return true;
}

bool gltfExportNifClip( const HkxAnimClip & clip, const QStringList & boneNames,
						const QHash<QString, int> & nodeByName, bool applyRootMotion,
						GltfExportScene & scene, GltfExportReport & report, QString & error )
{
	error.clear();
	if ( clip.numFrames < 1 || !( clip.frameDuration > 0.0f ) ) {
		error = QStringLiteral( "clip '%1' has %2 frames at %3 s" )
			.arg( clip.name ).arg( clip.numFrames ).arg( double( clip.frameDuration ) );
		return false;
	}
	GltfExportAnimation an;
	an.name = clip.name;
	an.numFrames = clip.numFrames;
	an.frameDuration = clip.frameDuration;

	for ( int t = 0; t < clip.numTracks; t++ ) {
		const int bone = t < clip.trackToBone.size() ? clip.trackToBone[t] : t;
		const QString name = ( bone >= 0 && bone < boneNames.size() ) ? boneNames[bone] : QString();
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
		ch.translations.reserve( clip.numFrames );
		ch.rotations.reserve( clip.numFrames );
		ch.scales.reserve( clip.numFrames );
		for ( int f = 0; f < clip.numFrames; f++ ) {
			const HkxTransform & x = clip.frames[f][t];
			ch.translations.append( x.translation );
			ch.rotations.append( x.rotation );
			ch.scales.append( x.scale );
		}
		an.channels.append( ch );
		report.tracksMatched++;
	}
	report.tracksUnmatched = int( an.unmatchedTracks.size() );
	if ( an.channels.isEmpty() ) {
		error = QStringLiteral( "clip '%1' has %2 tracks and not one of them names a node of this file" )
			.arg( clip.name ).arg( clip.numTracks );
		return false;
	}

	if ( !clip.rootMotion.isEmpty() ) {
		int rn = boneNames.isEmpty() ? -1 : nodeByName.value( boneNames.first().toLower(), -1 );
		if ( rn < 0 )
			rn = 0;
		an.rootMotionNode = rn;
		an.applyRootMotion = applyRootMotion;
		for ( const HkxRootMotion & rm : clip.rootMotion ) {
			an.rootMotionTranslation.append( rm.translation );
			an.rootMotionYaw.append( rm.yaw );
		}
		an.rootMotionUp = clip.rootMotionUp;
		bool have = false;
		for ( const GltfExportChannel & ch : an.channels )
			if ( ch.node == rn ) {
				have = true;
				break;
			}
		if ( applyRootMotion && !have ) {
			GltfExportChannel ch;
			ch.node = rn;
			for ( int f = 0; f < clip.numFrames; f++ ) {
				ch.translations.append( scene.nodes[rn].translation );
				ch.rotations.append( scene.nodes[rn].rotation );
			}
			an.channels.append( ch );
		}
		report.notes << QStringLiteral( "clip '%1' carries root motion; it was %2" )
			.arg( clip.name, applyRootMotion ? QStringLiteral( "composed onto '%1'" ).arg( scene.nodes[rn].name )
				: QStringLiteral( "left out, so the clip plays in place" ) );
	}
	for ( const QString & u : an.unmatchedTracks )
		report.notes << QStringLiteral( "clip track '%1' has no node in this file and was not exported" ).arg( u );
	scene.animations.append( an );
	return true;
}

namespace {
GltfExportClipProvider s_clipProvider = nullptr;
}

void gltfExportSetClipProvider( GltfExportClipProvider fn )
{
	s_clipProvider = fn;
}

GltfExportClipProvider gltfExportClipProvider()
{
	return s_clipProvider;
}

bool gltfExportNifWrite( const NifModel * nif, const QModelIndex & iRoot,
						 const HkxAnimClip * clip, const QStringList & boneNames,
						 bool applyRootMotion, const QString & gltfPath,
						 GltfExportReport & report, QString & error )
{
	GltfExportScene scene;
	scene.generator = QStringLiteral( "NifSkope Wild Wasteland Edition, gltfexport" );
	QHash<QString, int> nodeByName;
	if ( !gltfExportNifScene( nif, iRoot, scene, nodeByName, report, error ) )
		return false;
	if ( clip && !gltfExportNifClip( *clip, boneNames, nodeByName, applyRootMotion, scene, report, error ) )
		return false;
	return gltfExportWrite( scene, gltfPath, error );
}
