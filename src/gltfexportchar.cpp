/* Several NIFs + a skeleton + a clip -> one glTF. See src/gltfexportchar.h.
   Lane GLTFEXPORT1, 2026-09-19.

   The merge is BY NODE NAME, case-folded, because that is the only thing the
   files agree on: a body NIF's skin bone and skeleton.nif's node of the same
   name are different blocks in different files with the same name and the same
   bind. It is also what HkxPlayback::mapNames() already does against the scene
   (src/hkxplayback.h:218), so the exporter and the viewer bind the same way. */

#include "gltfexportchar.h"

#include "bodybuild.h"
#include "gamemanager.h"
#include "model/nifmodel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cmath>

namespace {

// ---- a 4x4 of doubles, column-major like glTF ---------------------------
struct M4
{
	double m[16];     //!< m[c*4+r]
	static M4 identity()
	{
		M4 r;
		for ( int i = 0; i < 16; i++ )
			r.m[i] = 0.0;
		r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0;
		return r;
	}
	double at( int r, int c ) const { return m[c * 4 + r]; }
	void set( int r, int c, double v ) { m[c * 4 + r] = v; }
};

M4 mul( const M4 & a, const M4 & b )
{
	M4 o;
	for ( int c = 0; c < 4; c++ )
		for ( int r = 0; r < 4; r++ ) {
			double s = 0.0;
			for ( int k = 0; k < 4; k++ )
				s += a.at( r, k ) * b.at( k, c );
			o.set( r, c, s );
		}
	return o;
}

//! TRS of one exported node as a matrix. The quat is NifSkope order (w,x,y,z).
M4 localOf( const GltfExportNode & n )
{
	const double w = n.rotation[0], x = n.rotation[1], y = n.rotation[2], z = n.rotation[3];
	const double R[9] = {
		1 - 2 * ( y * y + z * z ),     2 * ( x * y - z * w ),     2 * ( x * z + y * w ),
			2 * ( x * y + z * w ), 1 - 2 * ( x * x + z * z ),     2 * ( y * z - x * w ),
			2 * ( x * z - y * w ),     2 * ( y * z + x * w ), 1 - 2 * ( x * x + y * y )
	};
	M4 o = M4::identity();
	for ( int c = 0; c < 3; c++ )
		for ( int r = 0; r < 3; r++ )
			o.set( r, c, R[r * 3 + c] * double( n.scale[c] ) );
	o.set( 0, 3, n.translation[0] );
	o.set( 1, 3, n.translation[1] );
	o.set( 2, 3, n.translation[2] );
	return o;
}

/*! Gauss-Jordan with partial pivoting. A bone matrix is a rigid transform with
 *  a positive scale, so it is always invertible; a singular one is a defect and
 *  is REFUSED by the caller rather than papered over with an identity. */
bool invert( const M4 & in, M4 & out )
{
	double a[4][8];
	for ( int r = 0; r < 4; r++ ) {
		for ( int c = 0; c < 4; c++ )
			a[r][c] = in.at( r, c );
		for ( int c = 0; c < 4; c++ )
			a[r][4 + c] = ( r == c ) ? 1.0 : 0.0;
	}
	for ( int col = 0; col < 4; col++ ) {
		int piv = col;
		for ( int r = col + 1; r < 4; r++ )
			if ( std::fabs( a[r][col] ) > std::fabs( a[piv][col] ) )
				piv = r;
		if ( !( std::fabs( a[piv][col] ) > 1e-12 ) )
			return false;
		if ( piv != col )
			for ( int c = 0; c < 8; c++ )
				std::swap( a[col][c], a[piv][c] );
		const double d = a[col][col];
		for ( int c = 0; c < 8; c++ )
			a[col][c] /= d;
		for ( int r = 0; r < 4; r++ ) {
			if ( r == col )
				continue;
			const double f = a[r][col];
			if ( f == 0.0 )
				continue;
			for ( int c = 0; c < 8; c++ )
				a[r][c] -= f * a[col][c];
		}
	}
	for ( int r = 0; r < 4; r++ )
		for ( int c = 0; c < 4; c++ )
			out.set( r, c, a[r][4 + c] );
	return true;
}

//! World matrix per node, parents first. The scene's nodes are always emitted
//! parent-before-child by gltfExportNifScene(), and the merge below keeps
//! that, so one forward pass is enough -- asserted by the parent check.
bool worldMatrices( const GltfExportScene & scene, QVector<M4> & world, QString & error )
{
	world.resize( scene.nodes.size() );
	for ( int i = 0; i < scene.nodes.size(); i++ ) {
		const GltfExportNode & n = scene.nodes[i];
		if ( n.parent >= i ) {
			error = QStringLiteral( "node %1 '%2' has parent %3, which is not before it" )
				.arg( i ).arg( n.name ).arg( n.parent );
			return false;
		}
		world[i] = n.parent < 0 ? localOf( n ) : mul( world[n.parent], localOf( n ) );
	}
	return true;
}

M4 fromInverseBind( const QVector<float> & ib, int joint )
{
	M4 o = M4::identity();
	for ( int i = 0; i < 16; i++ )
		o.m[i] = double( ib[joint * 16 + i] );
	return o;
}

void toInverseBindFloats( const M4 & m, QVector<float> & out )
{
	for ( int i = 0; i < 16; i++ )
		out.append( float( m.m[i] ) );
}

QString keyOf( const QString & n ) { return n.trimmed().toLower(); }

} // namespace

// ---------------------------------------------------------------------------

int gltfExportCountHelpers( const QStringList & nodeNames, QStringList * helpers )
{
	int n = 0;
	for ( const QString & s : nodeNames )
		if ( gltfExportIsHelperBone( s ) ) {
			n++;
			if ( helpers )
				*helpers << s;
		}
	return n;
}

QString gltfExportFindSkeleton( const QString & nifPath, const QString & dataRoot,
								QStringList * looked )
{
	QStringList cand;
	const QFileInfo fi( nifPath );
	const QString dir = fi.absolutePath();
	cand << dir + QStringLiteral( "/skeleton.nif" );
	cand << dir + QStringLiteral( "/CharacterAssets/skeleton.nif" );
	cand << QFileInfo( dir + QStringLiteral( "/.." ) ).absoluteFilePath()
			+ QStringLiteral( "/CharacterAssets/skeleton.nif" );

	/* THE GAME-WIDE CANDIDATES ARE ONLY FOR SOMETHING THAT IS A CHARACTER.
	 *
	 * The three paths above are derived from the NIF itself: they find a
	 * skeleton only where one is actually kept beside the mesh it rigs. The two
	 * below are the installed game's own skeleton, which exists on this machine
	 * whatever is open -- so asking for them unconditionally would hang a full
	 * human rig on an alarm clock, and `auto` became the DEFAULT on 2026-09-19
	 * 09:45. That is a worse failure than finding nothing, because it is a
	 * plausible-looking file.
	 *
	 * The gate is the NIF's own path: under Actors/Character (which is where
	 * CharacterAssets lives) it is a character part and the shipped skeleton is
	 * the right answer; anywhere else the search simply ends and the caller
	 * falls back to the file's own nodes and says so. This is a NAME test on a
	 * path, so it is a heuristic, not a proof: the refuter is a character mesh
	 * kept outside Actors/Character, which gets no skeleton and one log line
	 * saying exactly that. */
	const QString abs = fi.absoluteFilePath().replace( QLatin1Char( '\\' ), QLatin1Char( '/' ) );
	const bool looksLikeCharacter =
		abs.contains( QLatin1String( "/CharacterAssets/" ), Qt::CaseInsensitive )
		|| abs.contains( QLatin1String( "/Actors/Character/" ), Qt::CaseInsensitive );
	const QString rel = QStringLiteral( "/Meshes/Actors/Character/CharacterAssets/skeleton.nif" );
	if ( looksLikeCharacter ) {
		if ( !dataRoot.isEmpty() )
			cand << dataRoot + rel;
		for ( const QString & f : Game::GameManager::folders( Game::FALLOUT_4 ) )
			cand << f + rel;
	}

	for ( const QString & c : cand ) {
		if ( looked )
			*looked << c;
		if ( QFile::exists( c ) )
			return QFileInfo( c ).absoluteFilePath();
	}
	return QString();
}

// ---------------------------------------------------------------------------

namespace {

//! Where the RACE data is read from: the first Fallout4.esm the game manager
//! serves, or `dataRoot`/Fallout4.esm. A miss is a refusal with the list.
QString findFallout4Esm( const QString & dataRoot, QStringList * looked )
{
	QStringList cand;
	if ( !dataRoot.isEmpty() )
		cand << dataRoot + QStringLiteral( "/Fallout4.esm" );
	for ( const QString & f : Game::GameManager::folders( Game::FALLOUT_4 ) )
		cand << f + QStringLiteral( "/Fallout4.esm" );
	for ( const QString & c : cand ) {
		if ( looked )
			*looked << c;
		if ( QFile::exists( c ) )
			return c;
	}
	return QString();
}

} // namespace

bool gltfExportApplyBodyBuild( GltfExportScene & scene, const QHash<QString, int> & byName,
							   const GltfExportOptions & opts,
							   GltfExportCharReport & cr, QString & error )
{
	QStringList looked;
	const QString esm = findFallout4Esm( opts.dataRoot, &looked );
	if ( esm.isEmpty() ) {
		error = QStringLiteral( "the body build needs Fallout4.esm and none was found; looked in %1" )
			.arg( looked.join( QStringLiteral( ", " ) ) );
		return false;
	}

	quint32 form = BODYBUILD_HUMAN_RACE;
	if ( !opts.buildRaceEditorId.isEmpty() ) {
		QVector<QPair<quint32, QString>> races;
		QString e;
		if ( !bodyBuildListRaces( esm, races, e ) ) {
			error = e;
			return false;
		}
		bool found = false;
		for ( const auto & r : races )
			if ( r.second.compare( opts.buildRaceEditorId, Qt::CaseInsensitive ) == 0 ) {
				form = r.first;
				found = true;
				break;
			}
		if ( !found ) {
			error = QStringLiteral( "no RACE called '%1' carries Bone Scale Data in %2" )
				.arg( opts.buildRaceEditorId, esm );
			return false;
		}
	}

	BodyBuildTable table;
	if ( !bodyBuildLoadRace( esm, form, table, error ) )
		return false;
	const BodyBuildSet * set = table.set( opts.buildGender );
	if ( !set ) {
		error = QStringLiteral( "RACE '%1' has no %2 bone scale set" )
			.arg( table.editorId, opts.buildGender == 1 ? QStringLiteral( "female" )
														: QStringLiteral( "male" ) );
		return false;
	}

	// ---- the static bake ---------------------------------------------------
	QVector<int> boneNode( set->bones.size(), -1 );
	for ( int i = 0; i < set->bones.size(); i++ ) {
		const int n = byName.value( keyOf( set->bones[i].name ), -1 );
		boneNode[i] = n;
		if ( n < 0 )
			cr.unmatchedBones << set->bones[i].name;
	}
	if ( cr.unmatchedBones.size() == set->bones.size() ) {
		error = QStringLiteral( "not one of the %1 build bones of RACE '%2' is a node of this export; "
								"the *_skin bones live in skeleton.nif -- try --skeleton auto" )
			.arg( set->bones.size() ).arg( table.editorId );
		return false;
	}

	if ( opts.bakeBodyBuild ) {
		for ( int i = 0; i < set->bones.size(); i++ ) {
			if ( boneNode[i] < 0 )
				continue;
			const Vector3 s = bodyBuildScale( set->bones[i], opts.buildThin,
											  opts.buildMuscular, opts.buildFat );
			GltfExportNode & n = scene.nodes[boneNode[i]];
			n.scale = Vector3( n.scale[0] * s[0], n.scale[1] * s[1], n.scale[2] * s[2] );
			cr.buildBonesScaled++;
		}
		cr.notes << QStringLiteral( "body build baked into %1 bone scales at thin %2 / muscular %3 / fat %4 "
									"(k = %5); %6 of the table's bones are not in this file" )
			.arg( cr.buildBonesScaled )
			.arg( double( opts.buildThin ), 0, 'f', 3 ).arg( double( opts.buildMuscular ), 0, 'f', 3 )
			.arg( double( opts.buildFat ), 0, 'f', 3 )
			.arg( double( bodyBuildCentroidK( opts.buildThin, opts.buildMuscular, opts.buildFat ) ), 0, 'f', 4 )
			.arg( cr.unmatchedBones.size() );
	}

	// ---- the cycle clip ----------------------------------------------------
	if ( opts.buildCycleClip ) {
		const QVector<BodyBuildCycleKey> keys = bodyBuildCycleKeys();
		GltfExportAnimation an;
		an.name = QStringLiteral( "%1_%2_BuildCycle" )
			.arg( table.editorId, opts.buildGender == 1 ? QStringLiteral( "Female" )
														: QStringLiteral( "Male" ) );
		// The writer samples per frame at a constant rate, so the four linear
		// key times become 30 fps frames -- 181 of them, 0..6 s, which is the
		// README's own count for build_build_cycle.py.
		const float fps = 30.0f;
		an.frameDuration = 1.0f / fps;
		an.numFrames = int( keys.last().time * fps ) + 1;
		for ( int i = 0; i < set->bones.size(); i++ ) {
			if ( boneNode[i] < 0 || set->bones[i].neutral() )
				continue;
			GltfExportChannel ch;
			ch.node = boneNode[i];
			const GltfExportNode & bn = scene.nodes[boneNode[i]];
			for ( int f = 0; f < an.numFrames; f++ ) {
				const float t = float( f ) / fps;
				// which segment of THIN -> MUSC -> FAT -> THIN
				int seg = 0;
				while ( seg + 2 < keys.size() && t >= keys[seg + 1].time )
					seg++;
				const float span = keys[seg + 1].time - keys[seg].time;
				const float u = span > 0.0f ? qBound( 0.0f, ( t - keys[seg].time ) / span, 1.0f ) : 0.0f;
				float w0[3] = { 0, 0, 0 }, w1[3] = { 0, 0, 0 };
				bodyBuildCornerWeights( keys[seg].corner, w0[0], w0[1], w0[2] );
				bodyBuildCornerWeights( keys[seg + 1].corner, w1[0], w1[1], w1[2] );
				const Vector3 s = bodyBuildScale( set->bones[i],
												  w0[0] + ( w1[0] - w0[0] ) * u,
												  w0[1] + ( w1[1] - w0[1] ) * u,
												  w0[2] + ( w1[2] - w0[2] ) * u );
				ch.translations.append( bn.translation );
				ch.rotations.append( bn.rotation );
				ch.scales.append( Vector3( bn.scale[0] * s[0], bn.scale[1] * s[1], bn.scale[2] * s[2] ) );
			}
			an.channels.append( ch );
			cr.buildCycleChannels++;
		}
		if ( an.channels.isEmpty() ) {
			error = QStringLiteral( "the build cycle has no bone to animate in this file" );
			return false;
		}
		scene.animations.append( an );
		cr.notes << QStringLiteral( "body build cycle: %1 bones, %2 frames at %3 fps, 6 s, LINEAR "
									"between THIN, MUSCULAR, FAT and THIN again. This is a glTF "
									"animation only -- skeleton.hkx carries none of the *_skin bones, "
									"so it cannot be written as a game-valid .hkx" )
			.arg( cr.buildCycleChannels ).arg( an.numFrames ).arg( double( fps ) );
	}
	return true;
}

// ---------------------------------------------------------------------------

namespace {

/*! Merge `src` (one NIF's scene) into `dst`, by node name.
 *
 *  A node `dst` already has is NOT duplicated: the part's copy is dropped and
 *  everything that pointed at it points at the existing one. That is the whole
 *  of finding (2) and finding (5) -- the body's flat *_skin list lands on the
 *  skeleton's hierarchy, and part two's head lands on the same bones as part
 *  one's body, so one clip drives all of them. */
void mergeScene( GltfExportScene & dst, QHash<QString, int> & dstByName,
				 const GltfExportScene & src, GltfExportCharReport & cr )
{
	QVector<int> map( src.nodes.size(), -1 );
	QVector<bool> wasMatched( src.nodes.size(), false );
	/* Only a merge that HAS a baseline can say a bone was missing from it. The
	 * first merge into an empty dst is the baseline itself (skeleton: none),
	 * and calling all 59 of its own bones "not carried by the skeleton" would
	 * be a sentence that is true of every export and therefore tells no one
	 * anything. */
	const bool haveBaseline = !dst.nodes.isEmpty();
	for ( int i = 0; i < src.nodes.size(); i++ ) {
		const GltfExportNode & sn = src.nodes[i];
		// A MESH node carries no name a bone could collide with meaningfully
		// (it is the shape's name) and there is one per shape, so it is always
		// appended. Bone nodes are matched.
		bool isMeshNode = false;
		for ( const GltfExportMesh & m : src.meshes )
			if ( m.node == i ) {
				isMeshNode = true;
				break;
			}
		const QString k = keyOf( sn.name );
		if ( !isMeshNode && !k.isEmpty() && dstByName.contains( k ) ) {
			map[i] = dstByName.value( k );
			wasMatched[i] = true;
			cr.nodesMatchedByName++;
			continue;
		}
		GltfExportNode nn = sn;
		nn.parent = sn.parent >= 0 ? map[sn.parent] : -1;
		if ( nn.parent < 0 && !dst.nodes.isEmpty() )
			nn.parent = 0;                     // hang a part's own root on the skeleton root
		map[i] = int( dst.nodes.size() );
		dst.nodes.append( nn );
		if ( !isMeshNode && !k.isEmpty() && !dstByName.contains( k ) )
			dstByName.insert( k, map[i] );
		if ( !isMeshNode )
			cr.nodesAdded++;
	}
	for ( const GltfExportMesh & sm : src.meshes ) {
		GltfExportMesh m = sm;
		m.node = map[sm.node];
		for ( int j = 0; j < m.joints.size(); j++ ) {
			/* NAME the bones the SKELETON did not carry. A skin bone that the
			 * destination already had was matched; one that had to be appended
			 * came from the part's own flat list and the skeleton knows nothing
			 * about it -- which on the right skeleton is a handful of names and
			 * on the WRONG skeleton is nearly all of them. That is the gate's
			 * red control, and a count alone could not tell the two apart, so
			 * this records the names. Silence here was the whole defect:
			 * "it exported fine" while the hierarchy was not there. */
			const int srcJoint = m.joints[j];
			if ( haveBaseline && srcJoint >= 0 && srcJoint < map.size()
				 && !wasMatched.value( srcJoint, false ) ) {
				const QString nm = src.nodes[srcJoint].name;
				if ( !nm.isEmpty() && !cr.unmatchedBones.contains( nm ) )
					cr.unmatchedBones << nm;
			}
			m.joints[j] = map[m.joints[j]];
		}
		dst.meshes.append( m );
	}
}

/*! Option (3): every skin gets the SAME joint list -- every bone node of the
 *  character, in tree order -- so Blender builds ONE armature and no loose
 *  empties. A joint with no stored inverse bind gets
 *      IBM(j) = inverse(world(j)) * bindToWorld(mesh)
 *  where bindToWorld is recovered from a joint this skin DID store:
 *      bindToWorld = world(j0) * IBM(j0).
 *  That is exactly the arithmetic of gltf_unify_skin.py, moved in here. */
bool unifyJoints( GltfExportScene & scene, GltfExportCharReport & cr, QString & error )
{
	QVector<M4> world;
	if ( !worldMatrices( scene, world, error ) )
		return false;

	QSet<int> meshNodes;
	for ( const GltfExportMesh & m : scene.meshes )
		meshNodes.insert( m.node );

	QVector<int> joints;
	for ( int i = 0; i < scene.nodes.size(); i++ )
		if ( !meshNodes.contains( i ) )
			joints.append( i );
	if ( joints.isEmpty() ) {
		error = QStringLiteral( "--joints whole: the scene has no bone node at all" );
		return false;
	}

	QHash<int, int> jpos;
	for ( int k = 0; k < joints.size(); k++ )
		jpos.insert( joints[k], k );

	for ( GltfExportMesh & m : scene.meshes ) {
		if ( m.skin.isEmpty() )
			continue;
		if ( m.joints.isEmpty() || m.inverseBind.size() != m.joints.size() * 16 ) {
			error = QStringLiteral( "shape '%1' has %2 joints and %3 inverse-bind floats" )
				.arg( m.name ).arg( m.joints.size() ).arg( m.inverseBind.size() );
			return false;
		}
		// bindToWorld from this skin's first stored joint
		M4 bindToWorld = mul( world[m.joints[0]], fromInverseBind( m.inverseBind, 0 ) );
		QHash<int, int> have;
		for ( int k = 0; k < m.joints.size(); k++ )
			have.insert( m.joints[k], k );

		QVector<float> ib;
		ib.reserve( joints.size() * 16 );
		for ( int j : joints ) {
			if ( have.contains( j ) ) {
				toInverseBindFloats( fromInverseBind( m.inverseBind, have.value( j ) ), ib );
				continue;
			}
			M4 inv;
			if ( !invert( world[j], inv ) ) {
				error = QStringLiteral( "bone '%1' has a singular world matrix; "
										"--joints whole cannot derive its inverse bind" )
					.arg( scene.nodes[j].name );
				return false;
			}
			toInverseBindFloats( mul( inv, bindToWorld ), ib );
			cr.inverseBindsDerived++;
		}

		// remap every vertex's joint index into the shared list
		QVector<int> remap( m.joints.size(), 0 );
		for ( int k = 0; k < m.joints.size(); k++ )
			remap[k] = jpos.value( m.joints[k], 0 );
		for ( GltfExportVertexSkin & vs : m.skin )
			for ( int c = 0; c < 4; c++ )
				vs.joints[c] = quint16( vs.joints[c] < remap.size() ? remap[vs.joints[c]] : 0 );

		m.joints = joints;
		m.inverseBind = ib;
		cr.skinsUnified++;
	}
	cr.jointsPerSkin = int( joints.size() );
	return true;
}

/*! Option (6). A helper is dropped only when NOTHING needs it: no skin weights
 *  it and no kept node descends from it.
 *
 *  The clip is built BEFORE this runs, so a helper CAN be carrying channels --
 *  that is exactly what happens once a skeleton supplies the Camera / AnimObject
 *  / Weapon nodes that the body's own file never had. Those channels drive a node
 *  that is about to stop existing, so they are dropped here, COUNTED, and named
 *  in the report. Mapping them to -1 and writing the file is what produced
 *  "animation 0 channel 27 drives node -1"; the validator was right. */
bool dropHelpers( GltfExportScene & scene, GltfExportCharReport & cr, QString & error )
{
	QSet<int> needed;
	for ( const GltfExportMesh & m : scene.meshes ) {
		needed.insert( m.node );
		for ( int j : m.joints )
			needed.insert( j );
	}
	// a weighted helper is kept, and SAID
	QVector<bool> drop( scene.nodes.size(), false );
	for ( int i = 0; i < scene.nodes.size(); i++ ) {
		if ( !gltfExportIsHelperBone( scene.nodes[i].name ) )
			continue;
		if ( needed.contains( i ) ) {
			cr.helpersKeptWeighted++;
			cr.notes << QStringLiteral( "helper bone '%1' is weighted by a shape, so it was kept" )
				.arg( scene.nodes[i].name );
			continue;
		}
		drop[i] = true;
	}
	// a helper that has a kept descendant stays (children first, so walk back)
	for ( int i = int( scene.nodes.size() ) - 1; i >= 0; i-- ) {
		const int p = scene.nodes[i].parent;
		if ( p >= 0 && !drop[i] )
			drop[p] = false;
	}
	int n = 0;
	for ( int i = 0; i < scene.nodes.size(); i++ )
		if ( drop[i] )
			n++;
	if ( n == 0 )
		return true;

	QVector<int> map( scene.nodes.size(), -1 );
	QVector<GltfExportNode> kept;
	for ( int i = 0; i < scene.nodes.size(); i++ ) {
		if ( drop[i] )
			continue;
		GltfExportNode nn = scene.nodes[i];
		nn.parent = nn.parent >= 0 ? map[nn.parent] : -1;
		map[i] = int( kept.size() );
		kept.append( nn );
	}
	if ( kept.isEmpty() ) {
		error = QStringLiteral( "--bones-only body dropped every node; the name rule is wrong for this file" );
		return false;
	}
	for ( GltfExportMesh & m : scene.meshes ) {
		m.node = map[m.node];
		for ( int & j : m.joints )
			j = map[j];
	}
	int lostTracks = 0;
	QStringList lostNames;
	for ( GltfExportAnimation & a : scene.animations ) {
		QVector<GltfExportChannel> live;
		live.reserve( a.channels.size() );
		for ( GltfExportChannel & c : a.channels ) {
			if ( c.node < 0 || map[c.node] < 0 ) {
				lostTracks++;
				if ( c.node >= 0 )
					lostNames << scene.nodes[c.node].name;
				continue;
			}
			c.node = map[c.node];
			live.append( c );
		}
		a.channels = live;
	}
	// an animation that drove NOTHING but helpers has nothing left to say
	QVector<GltfExportAnimation> liveAnims;
	for ( const GltfExportAnimation & a : scene.animations ) {
		if ( a.channels.isEmpty() ) {
			cr.notes << QStringLiteral( "--bones-only body: the clip '%1' drove only helper bones, "
										"so it was dropped with them" ).arg( a.name );
			continue;
		}
		liveAnims.append( a );
	}
	scene.animations = liveAnims;
	if ( lostTracks > 0 ) {
		cr.helperTracksDropped = lostTracks;
		cr.notes << QStringLiteral( "--bones-only body: %1 clip track(s) lost their bone and were "
									"dropped with it: %2" ).arg( lostTracks ).arg( lostNames.join( QStringLiteral( ", " ) ) );
	}
	scene.nodes = kept;
	cr.helpersDropped = n;
	return true;
}

//! Option (7) copy. `diffuseUri` is game-relative; the file is looked for under
//! the data root and beside the NIF, and a miss is COUNTED and named, never a
//! silent reference to a file that is not there.
void copyTextures( GltfExportScene & scene, const QString & gltfPath,
				   const QString & dataRoot, const QStringList & extraRoots,
				   GltfExportCharReport & cr )
{
	const QDir outDir( QFileInfo( gltfPath ).absolutePath() );
	QHash<QString, QString> doneUri;
	for ( GltfExportMesh & m : scene.meshes ) {
		if ( m.diffuseUri.isEmpty() )
			continue;
		if ( doneUri.contains( m.diffuseUri ) ) {
			m.diffuseUri = doneUri.value( m.diffuseUri );
			continue;
		}
		const QString rel = m.diffuseUri;
		QStringList roots = extraRoots;
		if ( !dataRoot.isEmpty() )
			roots.prepend( dataRoot );
		QString found;
		for ( const QString & r : roots ) {
			const QString p = r + QLatin1Char( '/' ) + rel;
			if ( QFile::exists( p ) ) {
				found = p;
				break;
			}
		}
		if ( found.isEmpty() ) {
			cr.texturesMissing++;
			cr.notes << QStringLiteral( "texture '%1' was not found under any data folder; "
										"the glTF still references it by that path" ).arg( rel );
			doneUri.insert( rel, rel );
			continue;
		}
		const QString base = QFileInfo( found ).fileName();
		const QString dest = outDir.absoluteFilePath( base );
		if ( !QFile::exists( dest ) && !QFile::copy( found, dest ) ) {
			cr.texturesMissing++;
			cr.notes << QStringLiteral( "texture '%1' could not be copied to '%2'" ).arg( found, dest );
			doneUri.insert( rel, rel );
			continue;
		}
		cr.texturesCopied++;
		doneUri.insert( rel, base );
		m.diffuseUri = base;
	}
}

} // namespace

// ---------------------------------------------------------------------------

bool gltfExportCharacter( const NifModel * nif, const QModelIndex & iRoot,
						  const HkxAnimClip * clip, const QStringList & boneNames,
						  const GltfExportOptions & opts, const QString & gltfPath,
						  GltfExportReport & report, GltfExportCharReport & charReport,
						  QString & error )
{
	error.clear();
	const bool applyRoot = ( opts.rootMotion == GltfExportOptions::RootMotion::Root );

	// THE WAY BACK: with every ruled option back at its pre-2026-09-19 value --
	// which is what `--legacy-defaults` assigns -- this is the old call,
	// unchanged, so those bytes stay reachable and stay provable. The gate's
	// byte-identity row runs exactly this path against the rung exe.
	if ( gltfExportOptionsAreLegacy( opts ) )
		return gltfExportNifWrite( nif, iRoot, opts.includeClip ? clip : nullptr,
								   boneNames, applyRoot, gltfPath, report, error );

	GltfExportScene scene;
	scene.generator = QStringLiteral( "NifSkope Wild Wasteland Edition, gltfexport" );
	QHash<QString, int> byName;

	// ---- (2) the skeleton first, so its hierarchy is what everything joins --
	QString skelPath = opts.skeletonPath;
	if ( opts.skeleton == GltfExportOptions::Skeleton::Auto ) {
		QStringList looked;
		skelPath = gltfExportFindSkeleton( nif ? nif->getFileInfo().absoluteFilePath() : QString(),
										   opts.dataRoot, &looked );
		/* AUTO MISSING IS NOT AN ERROR. `auto` became the DEFAULT when bungo
		 * ruled the Blender-ready defaults (2026-09-19 09:45), and most of what
		 * anyone opens in NifSkope is not a character: a static, a weapon, a
		 * piece of furniture has no CharacterAssets beside it and never will.
		 * Refusing those would turn a default into a wall. It falls back to the
		 * file's own nodes -- exactly the pre-ruling export -- and SAYS SO,
		 * because a silent fallback is how someone comes to believe a body was
		 * exported with its skeleton when it was not.
		 *
		 * An explicit --skeleton PATH that cannot be read still fails, below:
		 * there the caller named a file and is owed the truth about it. */
		if ( skelPath.isEmpty() ) {
			charReport.notes << QStringLiteral(
				"skeleton: auto found no skeleton.nif, so the file's own nodes were used "
				"(this is the pre-ruling export). Looked in %1" )
				.arg( looked.join( QStringLiteral( ", " ) ) );
			charReport.skeletonAutoMissed = true;
		} else {
			charReport.notes << QStringLiteral( "skeleton auto-found: %1" ).arg( skelPath );
		}
	}
	if ( !skelPath.isEmpty() ) {
		NifModel sk;
		QFile f( skelPath );
		if ( !f.open( QIODevice::ReadOnly ) || !sk.loadFromFile( skelPath ) ) {
			error = QStringLiteral( "the skeleton '%1' could not be read" ).arg( skelPath );
			return false;
		}
		GltfExportReport sr;
		GltfExportScene ss;
		QHash<QString, int> sb;
		if ( !gltfExportNifScene( &sk, QModelIndex(), ss, sb, sr, error ) )
			return false;
		mergeScene( scene, byName, ss, charReport );
		charReport.skeletonNodes = int( scene.nodes.size() );
		charReport.nodesAdded = 0;             // the skeleton IS the baseline
		charReport.nodesMatchedByName = 0;
	}

	// ---- the open NIF, then (5) every extra part ---------------------------
	{
		GltfExportScene ms;
		QHash<QString, int> mb;
		if ( !gltfExportNifScene( nif, iRoot, ms, mb, report, error ) )
			return false;
		mergeScene( scene, byName, ms, charReport );
	}
	for ( const QString & partPath : opts.parts ) {
		NifModel pm;
		if ( !pm.loadFromFile( partPath ) ) {
			error = QStringLiteral( "part '%1' could not be read" ).arg( partPath );
			return false;
		}
		GltfExportScene ps;
		QHash<QString, int> pb;
		GltfExportReport pr;
		if ( !gltfExportNifScene( &pm, QModelIndex(), ps, pb, pr, error ) )
			return false;
		mergeScene( scene, byName, ps, charReport );
		report.shapes += pr.shapes;
		report.skinnedShapes += pr.skinnedShapes;
		report.vertices += pr.vertices;
		report.triangles += pr.triangles;
		report.notes += pr.notes;
		charReport.partsMerged++;
	}
	report.nodes = int( scene.nodes.size() );

	// ---- Part 2: the body build, baked into the bone scales ---------------
	if ( opts.bakeBodyBuild || opts.buildCycleClip ) {
		if ( !gltfExportApplyBodyBuild( scene, byName, opts, charReport, error ) )
			return false;
	}

	// ---- the clip ---------------------------------------------------------
	if ( clip && opts.includeClip ) {
		if ( !gltfExportNifClip( *clip, boneNames, byName, applyRoot, scene, report, error ) )
			return false;
		if ( opts.rootMotion == GltfExportOptions::RootMotion::Object && !scene.animations.isEmpty() ) {
			// the travel goes onto the first MESH node instead of the root bone,
			// so in Blender the object moves and the armature's bones do not.
			GltfExportAnimation & a = scene.animations.last();
			if ( !scene.meshes.isEmpty() ) {
				a.applyRootMotion = true;
				a.rootMotionNode = scene.meshes.first().node;
				charReport.notes << QStringLiteral( "root motion was baked onto the object node '%1'" )
					.arg( scene.nodes[a.rootMotionNode].name );
			} else {
				charReport.notes << QStringLiteral( "--root-motion-mode object: this file has no mesh "
													"to carry the travel; it was stripped instead" );
			}
		}
	}

	// ---- (6) then (3): drop first, so the joint list is of what is left ----
	if ( opts.bones == GltfExportOptions::Bones::Body && !dropHelpers( scene, charReport, error ) )
		return false;
	if ( opts.joints == GltfExportOptions::Joints::Whole && !unifyJoints( scene, charReport, error ) )
		return false;

	// ---- (4) units ---------------------------------------------------------
	scene.unitScale = float( opts.metresPerUnit() );

	// ---- (7) textures ------------------------------------------------------
	if ( opts.textures == GltfExportOptions::Textures::Copy )
		copyTextures( scene, gltfPath, opts.dataRoot,
					  Game::GameManager::folders( Game::FALLOUT_4 ), charReport );

	report.nodes = int( scene.nodes.size() );
	return gltfExportWrite( scene, gltfPath, error );
}
