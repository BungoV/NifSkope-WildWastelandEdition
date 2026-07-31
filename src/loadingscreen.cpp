/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "loadingscreen.h"

#include "animgraph.h"
#include "model/nifmodel.h"
#include "nifsnapshot.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <cmath>

/*! \file loadingscreen.cpp
 * \brief Evaluate a posed rig into flat, static loading-screen geometry.
 *
 * The skin evaluation here is the same arithmetic as `Shape::skinVertex` — the
 * weighted matrix built in `vertexSkinMatrix`, normalised by the weight sum —
 * so what gets baked is what the viewport was showing. The one difference is
 * the space: the renderer works in the shape's local space and multiplies its
 * world transform back on afterwards, which cancels out, so the vertex we want
 * is simply
 *
 *     world = ( Σ wᵢ · boneWorldᵢ · boneBindᵢ ) / Σ wᵢ  ·  v
 *
 * with `v` the stored vertex and `boneBind` the `BSSkinBoneTrans` for that bone.
 */

namespace
{

// ---------------------------------------------------------------------------
// transforms
// ---------------------------------------------------------------------------

//! Every block's transform relative to the file root, by block number.
/*! Walks children rather than parents so one pass covers the whole tree, and a
 *  block reachable twice keeps the first path — the same choice the renderer
 *  makes when a node is shared. */
QHash<int, Transform> worldTransforms( const NifModel * nif )
{
	QHash<int, Transform> world;
	QList<int> queue;
	for ( int root : nif->getRootLinks() )
		queue.append( root );

	while ( !queue.isEmpty() ) {
		const int block = queue.takeFirst();
		QModelIndex idx = nif->getBlockIndex( block );
		if ( !idx.isValid() )
			continue;
		if ( !world.contains( block ) ) {
			Transform local;
			if ( Transform::canConstruct( nif, idx ) )
				local = Transform( nif, idx );
			world.insert( block, local );
		}
		const Transform parent = world.value( block );

		for ( const qint32 child : nif->getLinkArray( idx, "Children" ) ) {
			if ( child < 0 || world.contains( child ) )
				continue;
			QModelIndex ci = nif->getBlockIndex( child );
			if ( !ci.isValid() )
				continue;
			Transform local;
			if ( Transform::canConstruct( nif, ci ) )
				local = Transform( nif, ci );
			world.insert( child, parent * local );
			queue.append( child );
		}
	}
	return world;
}

// ---------------------------------------------------------------------------
// live effect branches
// ---------------------------------------------------------------------------

//! child -> parent over "Children" links, the same edges worldTransforms walks.
QHash<int, int> childParents( const NifModel * nif )
{
	QHash<int, int> parent;
	for ( int b = 0; b < nif->getBlockCount(); b++ )
		for ( const qint32 c : nif->getLinkArray( nif->getBlockIndex( b ), "Children" ) )
			if ( c >= 0 && !parent.contains( c ) )
				parent.insert( c, b );
	return parent;
}

//! Every block some skin binds to. These are the skeleton, whatever they are called.
QSet<int> skinBones( const NifModel * nif )
{
	QSet<int> bones;
	for ( int b = 0; b < nif->getBlockCount(); b++ )
		for ( const qint32 bone : nif->getLinkArray( nif->getBlockIndex( b ), "Bones" ) )
			if ( bone >= 0 )
				bones << bone;
	return bones;
}

//! Everything reachable from \a block by Ref links — the whole branch, including
//! its properties, controllers, interpolators and data.
void collectRefs( const NifModel * nif, int block, QSet<int> & out )
{
	if ( block < 0 || out.contains( block ) )
		return;
	out << block;
	for ( const int c : nif->getChildLinks( block ) )
		collectRefs( nif, c, out );
}

//! Is there anything RIGGED under this block?
/*! The stop condition for the climb. A branch that swallows a skinned shape
 *  would carry it past the point where the skeleton is deleted, leaving a skin
 *  bound to bones that no longer exist — and the climb does reach that far: a
 *  helmet effect seeded at NamedAttachHEAD walks HEAD, then Neck, and would take
 *  the whole head with it, because a skin's Bones array names Chest and not
 *  every node between. */
bool subtreeIsRigged( const NifModel * nif, int block )
{
	QSet<int> sub;
	collectRefs( nif, block, sub );
	for ( const int b : std::as_const( sub ) ) {
		QModelIndex idx = nif->getBlockIndex( b );
		if ( nif->getLink( idx, "Skin" ) >= 0
		     || nif->blockInherits( idx, { "NiSkinInstance", "BSSkin::Instance" } ) )
			return true;
	}
	return false;
}

/*! Does this block, or anything under it, generate something at runtime?
 *
 *  Three markers, and each one is a thing that CANNOT be flattened:
 *
 *    - `NiParticleSystem` — its geometry does not exist in the file at all.
 *    - `BSProceduralLightningController` — likewise; it fills an empty shape.
 *    - an `AttachT` NiStringsExtraData — the ArtObject marker. A `NamedAttach`
 *      branch carries its own, naming the node it belongs on, so this is the
 *      file saying "these blocks are an effect" in its own words.
 *
 *  Shader-property animation is deliberately NOT a marker: a
 *  BSEffectShaderPropertyFloatController hangs off the property, follows the
 *  shape wherever it goes, and keeps animating after the flatten. 13 of the 173
 *  vanilla screens rely on exactly that.
 */
bool hasAttachT( const NifModel * nif, int block )
{
	QModelIndex idx = nif->getBlockIndex( block );
	if ( !idx.isValid() )
		return false;
	for ( const qint32 e : nif->getLinkArray( idx, "Extra Data List" ) ) {
		QModelIndex iEx = nif->getBlockIndex( e );
		if ( nif->isNiBlock( iEx, "NiStringsExtraData" )
		     && nif->get<QString>( iEx, "Name" ) == QLatin1String( "AttachT" ) )
			return true;
	}
	return false;
}

bool isEffectSeed( const NifModel * nif, int block )
{
	QModelIndex idx = nif->getBlockIndex( block );
	if ( !idx.isValid() )
		return false;
	if ( nif->blockInherits( idx, { "NiParticleSystem", "BSProceduralLightningController" } ) )
		return true;
	return hasAttachT( nif, block );
}

// ---------------------------------------------------------------------------
// skin evaluation
// ---------------------------------------------------------------------------

//! An affine 3x4, accumulated as three rows — the shape `boneTransforms` uses.
struct Affine
{
	float r[3][4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } };

	static Affine zero()
	{
		Affine a;
		for ( int i = 0; i < 3; i++ )
			for ( int j = 0; j < 4; j++ )
				a.r[i][j] = 0.0f;
		return a;
	}

	static Affine from( const Transform & t )
	{
		Affine a;
		for ( int i = 0; i < 3; i++ ) {
			for ( int j = 0; j < 3; j++ )
				a.r[i][j] = t.rotation( i, j ) * t.scale;
			a.r[i][3] = t.translation[i];
		}
		return a;
	}

	void addScaled( const Affine & o, float w )
	{
		for ( int i = 0; i < 3; i++ )
			for ( int j = 0; j < 4; j++ )
				r[i][j] += o.r[i][j] * w;
	}

	void scale( float s )
	{
		for ( int i = 0; i < 3; i++ )
			for ( int j = 0; j < 4; j++ )
				r[i][j] *= s;
	}

	Vector3 point( const Vector3 & v ) const
	{
		return Vector3( r[0][0] * v[0] + r[0][1] * v[1] + r[0][2] * v[2] + r[0][3],
		                r[1][0] * v[0] + r[1][1] * v[1] + r[1][2] * v[2] + r[1][3],
		                r[2][0] * v[0] + r[2][1] * v[1] + r[2][2] * v[2] + r[2][3] );
	}

	//! Directions take the 3x3 only, and come back normalised.
	/*! Skipping this is what makes a bake look right until you find a curved
	 *  surface: the positions move and the lighting does not follow. */
	Vector3 direction( const Vector3 & v ) const
	{
		Vector3 d( r[0][0] * v[0] + r[0][1] * v[1] + r[0][2] * v[2],
		           r[1][0] * v[0] + r[1][1] * v[1] + r[1][2] * v[2],
		           r[2][0] * v[0] + r[2][1] * v[1] + r[2][2] * v[2] );
		const float len = d.length();
		return len > 1.0e-8f ? d / len : v;
	}
};

//! Per-bone `boneWorld * boneBind` for a skinned shape; empty when not skinned.
QVector<Affine> skinBoneMatrices( const NifModel * nif, const QModelIndex & shape,
                                  const QHash<int, Transform> & world, int * unresolved = nullptr )
{
	QVector<Affine> out;
	QModelIndex iSkin = nif->getBlockIndex( nif->getLink( shape, "Skin" ), "BSSkin::Instance" );
	if ( !iSkin.isValid() )
		return out;
	QModelIndex iData = nif->getBlockIndex( nif->getLink( iSkin, "Data" ), "BSSkin::BoneData" );
	if ( !iData.isValid() )
		return out;

	const QVector<qint32> bones = nif->getLinkArray( iSkin, "Bones" );
	QModelIndex iList = nif->getIndex( iData, "Bone List" );
	const int n = int( std::min<qsizetype>( bones.size(), nif->rowCount( iList ) ) );
	out.reserve( n );

	for ( int b = 0; b < n; b++ ) {
		QModelIndex iBT = nif->getIndex( iList, b );
		Transform bind;
		bind.rotation = nif->get<Matrix>( iBT, "Rotation" );
		bind.translation = nif->get<Vector3>( iBT, "Translation" );
		bind.scale = nif->get<float>( iBT, "Scale" );
		/* A bone with no world transform falls back to identity. That used to be
		 * SILENT, which is the worst behaviour here: the shapes weighted to it go
		 * somewhere wrong while every other shape stays right, so the output looks
		 * mostly fine. Counted now, and reported by the caller.
		 */
		if ( !world.contains( bones.at( b ) ) && unresolved )
			( *unresolved )++;
		const Transform boneWorld = world.value( bones.at( b ), Transform() );
		out.append( Affine::from( boneWorld * bind ) );
	}
	return out;
}

// ---------------------------------------------------------------------------
// geometry
// ---------------------------------------------------------------------------

bool isGeometry( const NifModel * nif, const QModelIndex & idx )
{
	return nif->blockInherits( idx, "BSTriShape" );
}

/* Vertex records store the same NAMED field at two different precisions, and
 * nif.xml calls both of them "Vertex" (likewise Normal and Tangent). So a
 * set<Vector3>() on a half-precision record does nothing and returns false —
 * which is exactly the bug that shipped: the node translations moved to their
 * centroids while the vertices stayed where they were, displacing every shape by
 * its own centroid. Different shapes, different offsets, everything out of place.
 *
 * These dispatch on the item's real type, the way spells/transform.cpp has always
 * done it, and they RETURN whether the write happened so the caller can refuse to
 * claim success.
 */
bool setVec3Field( NifModel * nif, const QModelIndex & iRow, const char * field, const Vector3 & p )
{
	NifItem * item = nif->getItem( iRow, field );
	if ( !item )
		return false;
	if ( item->hasValueType( NifValue::tHalfVector3 ) )
		return nif->set<HalfVector3>( item, HalfVector3( p ) );
	if ( item->hasValueType( NifValue::tByteVector3 ) )
		return nif->set<ByteVector3>( item, ByteVector3( p ) );
	return nif->set<Vector3>( item, p );
}

//! Recompute the bounding sphere from local vertices — Ritter is overkill for
//! this; centre-of-extent plus the farthest vertex is what the format wants.
void writeBounds( NifModel * nif, const QModelIndex & shape, const QVector<Vector3> & local )
{
	QModelIndex iB = nif->getIndex( shape, "Bounding Sphere" );
	if ( !iB.isValid() || local.isEmpty() )
		return;
	Vector3 lo = local.first(), hi = local.first();
	for ( const Vector3 & v : local ) {
		for ( int a = 0; a < 3; a++ ) {
			lo[a] = std::min( lo[a], v[a] );
			hi[a] = std::max( hi[a], v[a] );
		}
	}
	const Vector3 centre = ( lo + hi ) / 2.0f;
	float radius = 0.0f;
	for ( const Vector3 & v : local )
		radius = std::max( radius, ( v - centre ).length() );
	nif->set<Vector3>( iB, "Center", centre );
	nif->set<float>( iB, "Radius", radius );
}

} // namespace

// ---------------------------------------------------------------------------
// public
// ---------------------------------------------------------------------------

namespace LoadingScreen
{

Result convert( NifModel * nif, bool addZoomTarget, bool keepParticles, bool keepEffects,
                QString * error )
{
	Result res;
	if ( !nif ) {
		if ( error )
			*error = QStringLiteral( "no model" );
		return res;
	}

	const QList<int> roots = nif->getRootLinks();
	if ( roots.isEmpty() ) {
		if ( error )
			*error = QStringLiteral( "this file has no root block" );
		return res;
	}
	const int rootBlock = roots.first();

	QSet<QString> noteSeen;
	auto note = [&]( const QString & line ) {
		if ( !noteSeen.contains( line ) ) {
			noteSeen << line;
			res.notes << line;
		}
	};

	nifSnapshotOp( nif, QStringLiteral( "Convert to loading screen" ), [&]() {

		const QHash<int, Transform> world = worldTransforms( nif );

		/* --- 0. the branches that stay alive --------------------------------
		 *
		 * A particle system's geometry is not in the file and a procedural arc's
		 * is not either: flattening an effect gives a node with nothing in it.
		 * The alternative to baking a still (freezeanim.h) is to keep the branch
		 * exactly as authored and let it run.
		 *
		 * A branch is climbed from its seed up to the highest ancestor that is
		 * neither the root nor a bone, which lands on the node the merge attached
		 * — `NamedAttachTank_Armor` for the torso, the effect's own top-level
		 * nodes for the legs, whose file names no NamedAttach at all.
		 *
		 * The bone it hung from is then kept as a STUB carrying its full world
		 * transform. That is the whole placement problem: the branch is authored
		 * relative to its bone, the skeleton is about to be deleted, and putting
		 * the bone's world transform on a node of the same name leaves every
		 * position, rotation and scale under it exactly where it was — with no
		 * arithmetic applied to the branch itself, so there is nothing to get
		 * subtly wrong.
		 */
		QSet<int> live;                 // every block inside a preserved branch
		QList<int> liveRoots;           // the branch roots, parents-first order
		QMap<int, QList<int>> stubs;    // attach node -> the branches under it
		if ( keepEffects ) {
			const QHash<int, int> parents = childParents( nif );
			const QSet<int> bones = skinBones( nif );
			QSet<int> seen;
			QList<int> seeds;
			for ( int b = 0; b < nif->getBlockCount(); b++ )
				if ( isEffectSeed( nif, b ) )
					seeds.append( b );

			/* Grown as it is walked. A live block can DEPEND on a block the Ref walk
			 * never reaches, through a Ptr — a NiPSysMeshEmitter names the mesh it
			 * emits from that way. Flattening that mesh does not stop the emitter, it
			 * moves it: the mesh's node takes the world centroid and its vertices are
			 * re-centred to match, and the leg sprites came out 21 units from where
			 * they belong, by exactly that offset. So a Ptr into flattenable geometry
			 * makes that geometry part of the effect, and it is seeded here. */
			for ( int s = 0; s < seeds.size(); s++ ) {
				const int b = seeds.at( s );
				// a controller is not in the Children tree; climb from its target
				int at = b;
				if ( !nif->blockInherits( nif->getBlockIndex( b ), "NiAVObject" ) ) {
					at = nif->getLink( nif->getBlockIndex( b ), "Target" );
					if ( at < 0 )
						continue;
				}
				int marked = hasAttachT( nif, at ) ? at : -1;
				while ( parents.contains( at ) ) {
					const int p = parents.value( at );
					// Not past the root, not past a bone, and never far enough to
					// swallow rigged geometry: the armour has to stay flattenable.
					if ( p == rootBlock || bones.contains( p ) || subtreeIsRigged( nif, p ) )
						break;
					at = p;
					if ( hasAttachT( nif, at ) )
						marked = at;
				}
				/* An AttachT on the way up beats where the climb happened to stop.
				 * It is the file saying "the effect starts here" — `NamedAttachHEAD`
				 * rather than the `Neck` two nodes above it, which the climb reaches
				 * only because no skin binds to anything in between. Files whose
				 * branches carry no marker (the X-01 legs) keep the climb's answer,
				 * which lands on the bone their file-level AttachT names anyway. */
				if ( marked >= 0 )
					at = marked;
				if ( at == rootBlock || seen.contains( at ) )
					continue;
				seen << at;
				liveRoots.append( at );
				const int before = live.size();
				collectRefs( nif, at, live );

				// Anything this branch points AT and does not contain becomes a seed
				// of its own, so it is kept and placed rather than flattened.
				if ( live.size() != before ) {
					for ( const int inside : std::as_const( live ) ) {
						for ( const int dep : nif->getParentLinks( inside ) ) {
							if ( dep < 0 || dep == rootBlock || live.contains( dep )
							     || bones.contains( dep ) || seeds.contains( dep ) )
								continue;
							if ( nif->blockInherits( nif->getBlockIndex( dep ), "NiAVObject" )
							     && !subtreeIsRigged( nif, dep ) )
								seeds.append( dep );
						}
					}
				}
			}
			/* A root that turned out to sit inside another root's branch is not a
			 * branch of its own — the outer one already carries it, and treating it
			 * as one would put its attach node in the stub map, where the stub's
			 * children get cut down to the roots that named it. That is how the leg
			 * pulse meshes were deleted: they are live, but they are not seeds, so
			 * they were not in the list the stub kept. */
			for ( auto it = liveRoots.begin(); it != liveRoots.end(); ) {
				bool nested = false;
				for ( int p = parents.value( *it, -1 ); p >= 0 && !nested;
				      p = parents.value( p, -1 ) )
					nested = live.contains( p );
				it = nested ? liveRoots.erase( it ) : it + 1;
			}
			for ( const int r : std::as_const( liveRoots ) )
				stubs[parents.value( r, rootBlock )].append( r );

			res.effectBranches = liveRoots.size();
			res.effectBlocks = live.size();
			// Which branch went where, one line each. Off by default because a full
			// rig keeps sixteen of them; on when a placement has to be traced.
			if ( qEnvironmentVariableIsSet( "WW_LIVEFX_DEBUG" ) ) {
				const QHash<int, int> p2 = childParents( nif );
				for ( const int r : std::as_const( liveRoots ) ) {
					const int attach = p2.value( r, rootBlock );
					note( QStringLiteral( "branch '%1' (%2) on '%3'" )
						.arg( nif->get<QString>( nif->getBlockIndex( r ), "Name" ),
						      nif->itemName( nif->getBlockIndex( r ) ),
						      attach == rootBlock ? QStringLiteral( "the root" )
						                          : nif->get<QString>( nif->getBlockIndex( attach ), "Name" ) ) );
				}
			}
		}

		// --- 1. every shape becomes static geometry around its own origin ----
		QList<int> shapes;
		for ( int b = 0; b < nif->getBlockCount(); b++ )
			if ( isGeometry( nif, nif->getBlockIndex( b ) ) )
				shapes.append( b );

		// Shapes with no vertices at all. A BSProceduralLightningController
		// generates its arc into one of these at runtime (X01 Tesla's Bolt_01 and
		// Bolt_02: Num Vertices 0, Vertex Desc 0), so an empty shape and the
		// controller that fills it live or die together.
		QSet<int> emptyShapes;

		// World bounds of everything baked, for the zoom target.
		bool haveBounds = false;
		Vector3 boundsLo, boundsHi;
		auto growBounds = [&]( const Vector3 & p ) {
			if ( !haveBounds ) {
				boundsLo = boundsHi = p;
				haveBounds = true;
				return;
			}
			for ( int a = 0; a < 3; a++ ) {
				boundsLo[a] = std::min( boundsLo[a], p[a] );
				boundsHi[a] = std::max( boundsHi[a], p[a] );
			}
		};

		for ( const int b : std::as_const( shapes ) ) {
			QModelIndex shape = nif->getBlockIndex( b );
			QModelIndex iVD = nif->getIndex( shape, "Vertex Data" );
			const int nv = nif->rowCount( iVD );

			/* Inside a live branch: left exactly as authored. Its node chain is
			 * being kept, so folding the world transform into the vertices would
			 * apply it a second time. It still has to count towards the bounds,
			 * or the camera frames the armour and misses the effects. */
			if ( live.contains( b ) ) {
				const Affine m = Affine::from( world.value( b, Transform() ) );
				for ( int v = 0; v < nv; v++ )
					growBounds( m.point( nif->get<Vector3>( nif->getIndex( iVD, v ), "Vertex" ) ) );
				continue;
			}

			if ( !iVD.isValid() || nv < 1 ) {
				emptyShapes << b;
				continue;
			}

			int unresolvedBones = 0;
			const QVector<Affine> bones = skinBoneMatrices( nif, shape, world, &unresolvedBones );
			const bool skinned = !bones.isEmpty();
			if ( unresolvedBones > 0 )
				note( QStringLiteral( "%1: %2 of %3 skin bone(s) have no world transform — those "
					"vertices fall back to the bind pose and will be in the wrong place" )
					.arg( nif->get<QString>( shape, "Name" ) )
					.arg( unresolvedBones ).arg( bones.size() ) );
			/* A shape with no world transform silently became identity, which put
			 * the X-01 Tesla fan on the ground: its own local translation is
			 * (0,0,0) and the ~104 units of height come entirely from its parent
			 * chain, so losing that chain leaves it at the effect file's origin.
			 * Same silent default as the bone case just above — and note that
			 * worldTransforms() walks only "Children" while the reachability sweep
			 * further down follows every Ref, so a shape can survive the sweep and
			 * still be missing from the transform walk.
			 */
			if ( !world.contains( b ) )
				note( QStringLiteral( "%1: no world transform from the Children walk — baked as if "
					"its parents were identity, so it will be in the wrong place" )
					.arg( nif->get<QString>( shape, "Name" ) ) );
			const Affine shapeWorld = Affine::from( world.value( b, Transform() ) );

			QVector<Vector3> pos( nv );
			QVector<Affine> perVertex( nv );

			for ( int v = 0; v < nv; v++ ) {
				QModelIndex iVertex = nif->getIndex( iVD, v );
				const Vector3 stored = nif->get<Vector3>( iVertex, "Vertex" );

				Affine m = shapeWorld;
				if ( skinned ) {
					QModelIndex iW = nif->getIndex( iVertex, "Bone Weights" );
					QModelIndex iI = nif->getIndex( iVertex, "Bone Indices" );
					Affine acc = Affine::zero();
					float wSum = 0.0f;
					// NOT `slots` — Qt #defines that to nothing, and the error it
					// produces points at the '=' rather than the name.
					const int weightSlots = std::min( nif->rowCount( iW ), nif->rowCount( iI ) );
					for ( int s = 0; s < weightSlots; s++ ) {
						const float w = nif->get<float>( nif->getIndex( iW, s ) );
						const int bone = nif->get<quint8>( nif->getIndex( iI, s ) );
						if ( !( w > 0.0f ) || bone < 0 || bone >= bones.size() )
							continue;
						acc.addScaled( bones.at( bone ), w );
						wSum += w;
					}
					// An unweighted vertex on a skinned shape follows the shape's
					// own node, which is what the renderer falls back to.
					if ( wSum > 0.0f ) {
						acc.scale( 1.0f / wSum );
						m = acc;
					}
				}
				perVertex[v] = m;
				pos[v] = m.point( stored );
			}

			// The centroid is what keeps the coordinates small enough for half
			// floats. Mean, not bounding-box centre: an outlier vertex should not
			// drag the whole piece off its own origin.
			Vector3 centre;
			for ( const Vector3 & p : std::as_const( pos ) )
				centre += p;
			centre /= float( nv );
			for ( const Vector3 & p : std::as_const( pos ) )
				growBounds( p );

			QVector<Vector3> local( nv );
			for ( int v = 0; v < nv; v++ )
				local[v] = pos[v] - centre;

			// Write positions and rotate every direction channel by the same
			// matrix the position used.
			int posWritten = 0;
			nif->setState( BaseModel::Processing );
			for ( int v = 0; v < nv; v++ ) {
				QModelIndex iVertex = nif->getIndex( iVD, v );
				if ( setVec3Field( nif, iVertex, "Vertex", local[v] ) )
					posWritten++;

				const Affine & m = perVertex[v];
				if ( NifItem * n = nif->getItem( iVertex, "Normal" ) )
					setVec3Field( nif, iVertex, "Normal", m.direction( nif->get<Vector3>( n ) ) );
				if ( NifItem * t = nif->getItem( iVertex, "Tangent" ) )
					setVec3Field( nif, iVertex, "Tangent", m.direction( nif->get<Vector3>( t ) ) );
				// The bitangent is stored split across three fields of two
				// different types, so it has to be reassembled to be rotated.
				QModelIndex bx = nif->getIndex( iVertex, "Bitangent X" );
				QModelIndex by = nif->getIndex( iVertex, "Bitangent Y" );
				QModelIndex bz = nif->getIndex( iVertex, "Bitangent Z" );
				if ( bx.isValid() && by.isValid() && bz.isValid() ) {
					const Vector3 rotated = m.direction( Vector3( nif->get<float>( bx ),
						nif->get<float>( by ), nif->get<float>( bz ) ) );
					nif->set<float>( bx, rotated[0] );
					nif->set<float>( by, rotated[1] );
					nif->set<float>( bz, rotated[2] );
				}
			}
			nif->restoreState();

			// Refuse to report a shape as baked when its positions did not move.
			// The absence of this check is what let a wholly broken converter ship.
			if ( posWritten != nv )
				note( QStringLiteral( "%1: wrote only %2 of %3 vertex position(s) — this shape is "
					"NOT baked and its geometry no longer matches its node" )
					.arg( nif->get<QString>( shape, "Name" ) ).arg( posWritten ).arg( nv ) );

			// The node now places the piece, with no rotation — the vanilla shape.
			nif->set<Vector3>( shape, "Translation", centre );
			nif->set<Matrix>( shape, "Rotation", Matrix() );
			nif->set<float>( shape, "Scale", 1.0f );
			writeBounds( nif, shape, local );

			if ( skinned ) {
				// Drop the skin: clear the link, clear the Skinned attribute, and
				// move Vertex Desc and Data Size together. They are one unit — the
				// desc says how wide a vertex is and Data Size is derived from it,
				// so changing one without the other writes a file that reads back
				// as garbage.
				nif->setLink( shape, "Skin", -1 );
				BSVertexDesc desc = nif->get<BSVertexDesc>( shape, "Vertex Desc" );
				desc.RemoveFlag( VertexAttribute::VA_SKINNING );
				desc.ResetAttributeOffsets( nif->getBSVersion() );
				nif->set<BSVertexDesc>( shape, "Vertex Desc", desc );
				const quint32 numVerts = nif->get<quint32>( shape, "Num Vertices" );
				const quint32 numTris = nif->get<quint32>( shape, "Num Triangles" );
				if ( QModelIndex ds = nif->getIndex( shape, "Data Size" ); ds.isValid() )
					nif->set<quint32>( ds, desc.GetVertexSize() * numVerts + 6U * numTris );
				nif->updateArraySize( iVD );
				res.shapesBaked++;
			} else {
				res.shapesFolded++;
			}
		}

		// --- 2. everything that is not geometry or its properties ------------
		// Reparent every shape straight under the root: its node now carries the
		// full world placement, so the chain it hung from has no work left.
		QModelIndex iRoot = nif->getBlockIndex( rootBlock );
		QList<int> keptChildren;
		for ( const int b : std::as_const( shapes ) )
			if ( ( keepParticles || !emptyShapes.contains( b ) ) && !live.contains( b ) )
				keptChildren.append( b );

		/* The stubs. Each attach node keeps its name and gets its WORLD transform
		 * as its local one, since the chain that used to place it is being deleted;
		 * its children are cut down to the branches that asked for it. Anything
		 * else it carried is skeleton debris — a ragdoll collision object, bone LOD
		 * extra data — and would otherwise be dragged back in by reachability. */
		QList<int> keptStubs;
		for ( auto it = stubs.constBegin(); it != stubs.constEnd(); it++ ) {
			const int attach = it.key();
			if ( attach == rootBlock ) {
				keptChildren.append( it.value() );      // already where it belongs
				continue;
			}
			QModelIndex iAttach = nif->getBlockIndex( attach );
			if ( !iAttach.isValid() )
				continue;
			world.value( attach, Transform() ).writeBack( nif, iAttach );
			nif->setLink( iAttach, "Collision Object", -1 );
			if ( QModelIndex iEx = nif->getIndex( iAttach, "Extra Data List" ); iEx.isValid() ) {
				nif->set<uint>( iAttach, "Num Extra Data List", 0 );
				nif->updateArraySize( iEx );
			}
			// Kept children = the ones that are LIVE, read off the node itself rather
			// than from the branch list. Everything else it had was flattened and now
			// hangs from the root, so dropping it here is what stops it being drawn
			// twice; anything live that is not a branch root — an effect's emitter
			// mesh, say — would be orphaned by a narrower rule and swept away.
			if ( QModelIndex iCh = nif->getIndex( iAttach, "Children" ); iCh.isValid() ) {
				QList<qint32> keep;
				for ( const qint32 c : nif->getLinkArray( iCh ) )
					if ( c >= 0 && live.contains( c ) )
						keep.append( c );
				nif->set<uint>( iAttach, "Num Children", quint32( keep.size() ) );
				nif->updateArraySize( iCh );
				for ( int i = 0; i < keep.size(); i++ )
					nif->setLink( nif->getIndex( iCh, i ), keep.at( i ) );
			}
			keptStubs.append( attach );
			res.attachNodes << nif->get<QString>( iAttach, "Name" );
		}
		keptChildren.append( keptStubs );

		QSet<int> doomed;
		// emptyShapes cannot contain a live one: a shape in a live branch leaves the
		// loop above before it is classified, which is what keeps the empty shapes a
		// procedural arc fills at runtime alive alongside the controller that fills them.
		if ( !keepParticles && !emptyShapes.isEmpty() ) {
			doomed += emptyShapes;
			note( QStringLiteral( "removed %1 empty shape(s) — they hold no vertices of their own; "
				"a runtime controller generated their geometry, and that controller is gone" )
				.arg( emptyShapes.size() ) );
		}
		// One BSXFlags and one BSBehaviorGraphExtraData, not one per merged file.
		// A merge of twelve pieces brings twelve; every vanilla loading screen that
		// has them has exactly one.
		QSet<QString> keptOnce;
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex idx = nif->getBlockIndex( b );
			if ( !nif->blockInherits( idx, { "BSXFlags", "BSBehaviorGraphExtraData" } ) )
				continue;
			const QString type = nif->itemName( idx );
			if ( keptOnce.contains( type ) )
				doomed << b;
			else
				keptOnce << type;
		}

		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			// Shapes are kept — except the empty ones already condemned above.
			if ( b == rootBlock || ( shapes.contains( b ) && !doomed.contains( b ) ) )
				continue;
			// A live branch is kept WHOLE: its nodes, its particle systems, its
			// controllers, and the stub its attach node became.
			if ( live.contains( b ) || keptStubs.contains( b ) )
				continue;
			QModelIndex idx = nif->getBlockIndex( b );
			if ( !idx.isValid() )
				continue;
			if ( doomed.contains( b ) )
				continue;      // already condemned (empty shape, duplicate extra data)

			// The skeleton, and anything that only existed to hold it together.
			if ( nif->blockInherits( idx, "NiNode" )
			     || nif->itemName( idx ).startsWith( QLatin1String( "BSConnectPoint" ) ) ) {
				doomed << b;
				continue;
			}
			// Extra data that describes a skeleton there no longer is. BSXFlags
			// and BSBehaviorGraphExtraData are NOT in this list on purpose — 38
			// and 9 vanilla loading screens carry them.
			if ( nif->blockInherits( idx, { "BSBoneLODExtraData", "BSBound" } ) ) {
				doomed << b;
				continue;
			}
			// AttachT told the merge where to hang this file. Nothing reads it in
			// a finished screen.
			if ( nif->blockInherits( idx, "NiStringsExtraData" )
			     && nif->get<QString>( idx, "Name" ) == QLatin1String( "AttachT" ) ) {
				doomed << b;
				continue;
			}
			// 0 of 173 vanilla loading screens contain any of this.
			if ( !keepParticles && !keepEffects
			     && ( nif->blockInherits( idx, { "NiParticleSystem", "NiPSysData", "NiPSysModifier",
			                                     "NiPSysModifierCtlr", "BSPositionData" } )
			          || nif->itemName( idx ).startsWith( QLatin1String( "NiPSys" ) )
			          || nif->itemName( idx ).startsWith( QLatin1String( "BSPSys" ) )
			          || nif->itemName( idx ) == QLatin1String( "BSProceduralLightningController" ) ) ) {
				doomed << b;
				continue;
			}
		}

		if ( !doomed.isEmpty() ) {
			int particles = 0, lightning = 0;
			for ( const int b : std::as_const( doomed ) ) {
				const QString t = nif->itemName( nif->getBlockIndex( b ) );
				if ( t == QLatin1String( "NiParticleSystem" ) ) particles++;
				if ( t == QLatin1String( "BSProceduralLightningController" ) ) lightning++;
			}
			if ( particles || lightning )
				note( QStringLiteral( "dropped %1 particle system(s) and %2 procedural lightning "
					"controller(s) — 0 of the 173 vanilla loading screens contain either. Their "
					"geometry is generated at runtime, so there is nothing in the file to keep. "
					"To keep the LOOK, run Freeze Animation on the file first with Effects set "
					"to 'Snapshot as static geometry': that captures the instant as an ordinary "
					"effect-shader mesh, which then converts like any other shape." )
					.arg( particles ).arg( lightning ) );
		}

		// Root keeps exactly the shapes, in the order they were found.
		if ( QModelIndex iChildren = nif->getIndex( iRoot, "Children" ); iChildren.isValid() ) {
			nif->set<uint>( iRoot, "Num Children", quint32( keptChildren.size() ) );
			nif->updateArraySize( iChildren );
			for ( int i = 0; i < keptChildren.size(); i++ )
				nif->setLink( nif->getIndex( iChildren, i ), keptChildren.at( i ) );
		}
		// The root itself must not carry a transform any more; the shapes do.
		nif->set<Vector3>( iRoot, "Translation", Vector3() );
		nif->set<Matrix>( iRoot, "Rotation", Matrix() );
		nif->set<float>( iRoot, "Scale", 1.0f );

		// --- 3. the camera focus point ---------------------------------------
		// insertNiBlock with no row appends, and appending does NOT renumber, so
		// every block number gathered above stays valid.
		int zoomBlock = -1;
		if ( addZoomTarget && haveBounds ) {
			QModelIndex iZoom = nif->insertNiBlock( QStringLiteral( "NiNode" ) );
			if ( iZoom.isValid() ) {
				zoomBlock = nif->getBlockNumber( iZoom );
				nif->set<QString>( iZoom, "Name", QStringLiteral( "LoadingMenuZoomTarget" ) );
				// Middle of the actual geometry, high up it. Vanilla's X-01 screen
				// points at (0, 19.08, 139.89) on a figure whose torso node sits at
				// Z = 111.27 — head height, not the middle. This is a starting
				// point to drag, not a derived truth: there is no rule in the
				// corpus for where the camera should look.
				const Vector3 target( ( boundsLo[0] + boundsHi[0] ) * 0.5f,
				                      ( boundsLo[1] + boundsHi[1] ) * 0.5f,
				                      boundsLo[2] + ( boundsHi[2] - boundsLo[2] ) * 0.85f );
				nif->set<Vector3>( iZoom, "Translation", target );
				nif->set<float>( iZoom, "Scale", 1.0f );
				res.zoomTargetAdded = true;
				note( QStringLiteral( "LoadingMenuZoomTarget placed at (%1, %2, %3) — the middle of "
					"the geometry, 85% of the way up it. It is where the camera looks; move it if "
					"the framing is wrong." )
					.arg( target[0], 0, 'f', 2 ).arg( target[1], 0, 'f', 2 ).arg( target[2], 0, 'f', 2 ) );
			}
		}

		if ( zoomBlock >= 0 ) {
			if ( QModelIndex iChildren = nif->getIndex( iRoot, "Children" ); iChildren.isValid() ) {
				nif->set<uint>( iRoot, "Num Children", quint32( keptChildren.size() + 1 ) );
				nif->updateArraySize( iChildren );
				for ( int i = 0; i < keptChildren.size(); i++ )
					nif->setLink( nif->getIndex( iChildren, i ), keptChildren.at( i ) );
				nif->setLink( nif->getIndex( iChildren, keptChildren.size() ), zoomBlock );
			}
		}

		// --- 4. remove, descending so renumbering cannot bite ----------------
		QList<int> order = doomed.values();
		std::sort( order.begin(), order.end(), std::greater<int>() );
		for ( const int b : std::as_const( order ) ) {
			if ( nif->blockInherits( nif->getBlockIndex( b ), "NiNode" ) )
				res.nodesRemoved++;
			nif->removeNiBlock( b );
		}
		res.blocksRemoved = order.size();

		// --- 5. sweep whatever the removals orphaned -------------------------
		// Deleting the skeleton leaves its ragdoll collision, its BSSkin::Instance
		// and BoneData, and every controller that drove a bone with nothing
		// pointing at them. NifSkope's rootLinks cannot tell those from a real
		// root — it computes "root" as "nothing refers to it" — so reachability
		// has to be walked from the block we actually kept.
		//
		// Ref links only. A Ptr is a weak back-reference (Skeleton Root, a
		// controller's Target); following those would keep exactly the debris
		// this is here to remove.
		// The root moved down by however many removed blocks sat below it.
		int rootNow = rootBlock;
		for ( const int b : std::as_const( order ) )
			if ( b < rootBlock )
				rootNow--;

		QSet<int> reachable;
		QList<int> queue{ rootNow };
		while ( !queue.isEmpty() ) {
			const int b = queue.takeFirst();
			if ( b < 0 || reachable.contains( b ) )
				continue;
			reachable << b;
			for ( const int c : nif->getChildLinks( b ) )
				if ( !reachable.contains( c ) )
					queue.append( c );
		}
		// One descending pass is enough: removing an unreachable block cannot make
		// another block reachable, so the set does not need recomputing.
		for ( int b = nif->getBlockCount() - 1; b >= 0; b-- ) {
			if ( reachable.contains( b ) )
				continue;
			if ( nif->blockInherits( nif->getBlockIndex( b ), "NiNode" ) )
				res.nodesRemoved++;
			nif->removeNiBlock( b );
			res.blocksRemoved++;
		}

		/* --- 5b. the animation the kept branches need -------------------------
		 * The manager, its sequences and its palette hang off the root's Controller
		 * and survive the sweep on their own. What they are left holding is every
		 * entry that named a bone, because the skeleton is gone — and a palette
		 * full of holes is what a name lookup walks through.
		 */
		if ( keepEffects ) {
			const int pruned = pruneDeadAnimLinks( nif );
			if ( pruned > 0 )
				note( QStringLiteral( "pruned %1 animation reference(s) to blocks the convert "
					"removed — object palette entries and multi-target targets that named "
					"skeleton bones" ).arg( pruned ) );

			// BSXFlags bit 0 is Animated. The merged rig inherits the skeleton's 198,
			// which does not have it; CreatureBloatfly.nif, the one vanilla loading
			// screen with a controller manager, is 513 — bit 0 set.
			for ( int b = 0; b < nif->getBlockCount(); b++ ) {
				QModelIndex idx = nif->getBlockIndex( b );
				if ( !nif->isNiBlock( idx, "BSXFlags" ) )
					continue;
				const uint flags = nif->get<uint>( idx, "Integer Data" );
				if ( !( flags & 1u ) ) {
					nif->set<uint>( idx, "Integer Data", flags | 1u );
					note( QStringLiteral( "BSXFlags %1 -> %2: bit 0 (Animated) set, because the "
						"file now carries effects that have to be stepped" )
						.arg( flags ).arg( flags | 1u ) );
				}
				break;
			}
		}

		// --- 6. compact the link arrays the removals hollowed out --------------
		// removeNiBlock rewrites a link to a deleted block as -1 but leaves the
		// ARRAY ENTRY behind, so a root that carried the skeleton's extra data ends
		// up with "Num Extra Data List = 31" and 30 of them -1. It loads, but it is
		// debris, and nothing in the corpus looks like that.
		for ( int b = 0; b < nif->getBlockCount(); b++ ) {
			QModelIndex idx = nif->getBlockIndex( b );
			if ( !idx.isValid() )
				continue;
			for ( const auto & field : { std::pair<const char *, const char *>{ "Extra Data List", "Num Extra Data List" },
			                             { "Children", "Num Children" } } ) {
				QModelIndex iArr = nif->getIndex( idx, field.first );
				if ( !iArr.isValid() )
					continue;
				QList<qint32> kept;
				for ( const qint32 l : nif->getLinkArray( iArr ) )
					if ( l >= 0 )
						kept.append( l );
				if ( kept.size() == nif->rowCount( iArr ) )
					continue;
				nif->set<uint>( idx, field.second, quint32( kept.size() ) );
				nif->updateArraySize( iArr );
				for ( int i = 0; i < kept.size(); i++ )
					nif->setLink( nif->getIndex( iArr, i ), kept.at( i ) );
			}
		}
	} );

	res.ok = true;
	return res;
}

} // namespace LoadingScreen
