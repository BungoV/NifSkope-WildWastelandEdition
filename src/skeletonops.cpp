/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "skeletontools.h"

#include "model/nifmodel.h"
#include "data/niftypes.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QSet>

/*! \file skeletonops.cpp
 * \brief Skeleton Manager bone operations — the write half.
 *
 * Kept apart from the analysis and the dock so the model-layer edits can be
 * tested (and driven from the CLI) without a window.
 *
 * **The reference sweep is the foundation.** A bone in a NIF is referenced from
 * far more places than its parent's Children array, and every one of them is a
 * corruption trap if an edit misses it:
 *
 *   - the parent NiNode's `Children` array (a link)
 *   - each skinned shape's `Bones` array (links, and the ORDER is the vertex
 *     `Bone Indices` mapping — reordering it silently rebinds the mesh)
 *   - `BSSkin::BoneData` / `NiSkinData` `Bone List`, positionally parallel to
 *     `Bones`
 *   - `BSBoneLODExtraData`, which names bones as STRINGS
 *   - `BSConnectPoint::Parents`, likewise by name
 *   - `NiControllerSequence` / `ControlledBlock` `Node Name`, by name
 *   - `bhkNPCollisionObject` hanging off the node (per-bone ragdoll collision)
 *   - `bhkRagdollSystem`, a compiled Havok packfile that binds bodies to nodes
 *
 * That last one cannot be edited from here: it is an opaque blob (53,920 bytes
 * on the vanilla brahmin skeleton). `hknpdecode` can read it, but there is no
 * proven encoder for ragdoll bodies and constraints, so any structural change to
 * a bone that owns a ragdoll body leaves the ragdoll stale. Callers are expected
 * to warn; `skeletonFileHasRagdoll()` is how they know to.
 */

// ---------------------------------------------------------------------------
// names
// ---------------------------------------------------------------------------

/*! Mirror a bone name across the body's midline.
 *
 * Covers the four conventions that actually appear in Bethesda rigs, most
 * specific first so `skin_bone_L_Eyelid` does not get mangled by the bare-prefix
 * rule that handles `LArm1`:
 *
 *   `_L_` / `_R_`      skin_bone_L_Eyelid_Top   (FO4 face rigs)
 *   `.L` / `.R`        Bone.L                   (Blender's own convention)
 *   `Left` / `Right`   LeftHand
 *   leading `L` / `R`  LArm1, RLeg2             (FO4 body rigs)
 *
 * Returns the name unchanged when it has no side, which is how callers detect
 * "this bone is on the midline" (Spine, Neck, Root).
 */
QString skeletonFlipBoneName( const QString & name )
{
	if ( name.isEmpty() )
		return name;

	// _L_ / _R_ anywhere
	if ( name.contains( QLatin1String( "_L_" ) ) )
		return QString( name ).replace( QLatin1String( "_L_" ), QLatin1String( "_R_" ) );
	if ( name.contains( QLatin1String( "_R_" ) ) )
		return QString( name ).replace( QLatin1String( "_R_" ), QLatin1String( "_L_" ) );

	// trailing .L / .R (case-insensitive, Blender style)
	if ( name.endsWith( QLatin1String( ".L" ), Qt::CaseInsensitive ) )
		return name.left( name.size() - 1 ) + name.right( 1 ).replace( QLatin1String( "L" ), QLatin1String( "R" ), Qt::CaseInsensitive );
	if ( name.endsWith( QLatin1String( ".R" ), Qt::CaseInsensitive ) )
		return name.left( name.size() - 1 ) + name.right( 1 ).replace( QLatin1String( "R" ), QLatin1String( "L" ), Qt::CaseInsensitive );

	// Left / Right words
	if ( name.contains( QLatin1String( "Left" ) ) )
		return QString( name ).replace( QLatin1String( "Left" ), QLatin1String( "Right" ) );
	if ( name.contains( QLatin1String( "Right" ) ) )
		return QString( name ).replace( QLatin1String( "Right" ), QLatin1String( "Left" ) );

	// bare leading L / R, but only when what follows is not lowercase — so LArm1
	// flips while "Leg1" and "Lower" do not. This is why it is tried last.
	if ( name.size() >= 2 ) {
		const QChar c0 = name.at( 0 );
		const QChar c1 = name.at( 1 );
		if ( !c1.isLower() ) {
			if ( c0 == QLatin1Char( 'L' ) )
				return QLatin1String( "R" ) + name.mid( 1 );
			if ( c0 == QLatin1Char( 'R' ) )
				return QLatin1String( "L" ) + name.mid( 1 );
		}
	}

	return name;
}

//! Name with any trailing digits removed — the "family" a bone belongs to, used
//! by Select Similar (LArm1/LArm2/LArm3 are one family).
static QString skeletonNameStem( const QString & name )
{
	QString stem = name;
	while ( !stem.isEmpty() && stem.at( stem.size() - 1 ).isDigit() )
		stem.chop( 1 );
	return stem;
}

QStringList skeletonSimilarNames( const NifModel * nif, const QString & name )
{
	QStringList out;
	if ( !nif || name.isEmpty() )
		return out;
	const QString stem = skeletonNameStem( name );
	if ( stem.isEmpty() || stem == name )
		return out;			// no trailing digits: no family to match
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex idx = nif->getBlockIndex( b );
		if ( !idx.isValid() || !nif->blockInherits( idx, "NiAVObject" ) )
			continue;
		const QString n = nif->get<QString>( idx, "Name" );
		if ( !n.isEmpty() && skeletonNameStem( n ) == stem )
			out << n;
	}
	return out;
}

// ---------------------------------------------------------------------------
// reference sweep
// ---------------------------------------------------------------------------

bool skeletonFileHasRagdoll( const NifModel * nif )
{
	if ( !nif )
		return false;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		if ( nif->itemName( nif->getBlockIndex( b ) ) == QLatin1String( "bhkRagdollSystem" ) )
			return true;
	}
	return false;
}

QList<SkeletonBoneRef> skeletonBoneRefs( const NifModel * nif, int boneBlock )
{
	QList<SkeletonBoneRef> refs;
	if ( !nif || boneBlock < 0 )
		return refs;
	QModelIndex iBone = nif->getBlockIndex( boneBlock );
	if ( !iBone.isValid() )
		return refs;
	const QString boneName = nif->get<QString>( iBone, "Name" );

	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex idx = nif->getBlockIndex( b );
		if ( !idx.isValid() )
			continue;
		const QString type = nif->itemName( idx );

		// parent's Children array
		if ( QModelIndex iCh = nif->getIndex( idx, "Children" ); iCh.isValid() ) {
			for ( int r = 0; r < nif->rowCount( iCh ); r++ ) {
				if ( nif->getLink( nif->getIndex( iCh, r ) ) == boneBlock )
					refs << SkeletonBoneRef{ QStringLiteral( "child of" ), b,
						QStringLiteral( "%1 [%2] Children/%3" ).arg( type ).arg( b ).arg( r ) };
			}
		}

		// a skin's Bones array — position IS the vertex bone index
		if ( QModelIndex iBones = nif->getIndex( idx, "Bones" ); iBones.isValid() ) {
			for ( int r = 0; r < nif->rowCount( iBones ); r++ ) {
				if ( nif->getLink( nif->getIndex( iBones, r ) ) == boneBlock )
					refs << SkeletonBoneRef{ QStringLiteral( "skin bone" ), b,
						QStringLiteral( "%1 [%2] Bones/%3 — vertex Bone Indices map to this slot" )
							.arg( type ).arg( b ).arg( r ) };
			}
		}

		// bone-LOD entries name bones as strings
		if ( type == QLatin1String( "BSBoneLODExtraData" ) && !boneName.isEmpty() ) {
			QModelIndex iInfo = nif->getIndex( idx, "BoneLOD Info" );
			for ( int r = 0; r < nif->rowCount( iInfo ); r++ ) {
				if ( nif->get<QString>( nif->getIndex( iInfo, r ), "Bone Name" ) == boneName )
					refs << SkeletonBoneRef{ QStringLiteral( "bone LOD" ), b,
						QStringLiteral( "BSBoneLODExtraData [%1] entry %2 (by NAME)" ).arg( b ).arg( r ) };
			}
		}

		// controller sequences address their targets by name
		if ( type == QLatin1String( "NiControllerSequence" ) && !boneName.isEmpty() ) {
			QModelIndex iBlocks = nif->getIndex( idx, "Controlled Blocks" );
			for ( int r = 0; r < nif->rowCount( iBlocks ); r++ ) {
				if ( nif->get<QString>( nif->getIndex( iBlocks, r ), "Node Name" ) == boneName )
					refs << SkeletonBoneRef{ QStringLiteral( "animated by" ), b,
						QStringLiteral( "NiControllerSequence [%1] ControlledBlock %2 (by NAME)" ).arg( b ).arg( r ) };
			}
		}

		// connect points parent by name
		if ( type.startsWith( QLatin1String( "BSConnectPoint" ) ) && !boneName.isEmpty() ) {
			QModelIndex iPts = nif->getIndex( idx, "Connect Points" );
			for ( int r = 0; r < nif->rowCount( iPts ); r++ ) {
				if ( nif->get<QString>( nif->getIndex( iPts, r ), "Parent" ) == boneName )
					refs << SkeletonBoneRef{ QStringLiteral( "connect point" ), b,
						QStringLiteral( "BSConnectPoint::Parents [%1] entry %2 (by NAME)" ).arg( b ).arg( r ) };
			}
		}
	}

	// the node's own collision object
	if ( int col = nif->getLink( iBone, "Collision Object" ); col >= 0 ) {
		refs << SkeletonBoneRef{ QStringLiteral( "collision" ), col,
			QStringLiteral( "%1 [%2] — ragdoll collision for this bone" )
				.arg( nif->itemName( nif->getBlockIndex( col ) ) ).arg( col ) };
	}

	return refs;
}

// ---------------------------------------------------------------------------
// transforms
// ---------------------------------------------------------------------------

Transform skeletonWorldTransform( const NifModel * nif, int block )
{
	Transform t;
	if ( !nif )
		return t;
	// Walk to the root multiplying local transforms. Guarded against a cycle,
	// which a hand-edited file can contain.
	QList<int> chain;
	int cur = block;
	QSet<int> seen;
	while ( cur >= 0 && !seen.contains( cur ) ) {
		seen << cur;
		chain.prepend( cur );
		cur = nif->getParent( cur );
	}
	for ( int b : chain ) {
		QModelIndex idx = nif->getBlockIndex( b );
		if ( Transform::canConstruct( nif, idx ) )
			t = t * Transform( nif, idx );
	}
	return t;
}

// ---------------------------------------------------------------------------
// child-array plumbing
// ---------------------------------------------------------------------------

static QList<int> skelChildren( const NifModel * nif, int parent )
{
	QList<int> kids;
	QModelIndex iCh = nif->getIndex( nif->getBlockIndex( parent ), "Children" );
	for ( int r = 0; r < nif->rowCount( iCh ); r++ )
		kids << nif->getLink( nif->getIndex( iCh, r ) );
	return kids;
}

static bool skelSetChildren( NifModel * nif, int parent, const QList<int> & kids )
{
	QModelIndex ip = nif->getBlockIndex( parent );
	QModelIndex iCh = nif->getIndex( ip, "Children" );
	if ( !iCh.isValid() )
		return false;
	if ( QModelIndex iNum = nif->getIndex( ip, "Num Children" ); iNum.isValid() )
		nif->set<int>( iNum, kids.size() );
	nif->updateArraySize( iCh );
	for ( int i = 0; i < kids.size(); i++ )
		nif->setLink( nif->getIndex( iCh, i ), kids.at( i ) );
	return true;
}

// ---------------------------------------------------------------------------
// operations
// ---------------------------------------------------------------------------

bool skeletonRenameBone( NifModel * nif, int block, const QString & newName, QString * error )
{
	auto fail = [error]( const QString & m ) { if ( error ) *error = m; return false; };
	if ( !nif || block < 0 )
		return fail( QCoreApplication::translate( "skeleton", "invalid bone" ) );
	QModelIndex iBone = nif->getBlockIndex( block );
	if ( !iBone.isValid() )
		return fail( QCoreApplication::translate( "skeleton", "invalid bone" ) );
	const QString oldName = nif->get<QString>( iBone, "Name" );
	if ( newName.isEmpty() )
		return fail( QCoreApplication::translate( "skeleton", "a bone name cannot be empty" ) );
	if ( newName == oldName )
		return true;

	// Rename the node, then every by-NAME reference. The by-link references
	// (Children, skin Bones) need no change — they point at the block.
	nif->set<QString>( iBone, "Name", newName );

	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex idx = nif->getBlockIndex( b );
		const QString type = nif->itemName( idx );

		if ( type == QLatin1String( "BSBoneLODExtraData" ) ) {
			QModelIndex iInfo = nif->getIndex( idx, "BoneLOD Info" );
			for ( int r = 0; r < nif->rowCount( iInfo ); r++ ) {
				QModelIndex e = nif->getIndex( iInfo, r );
				if ( nif->get<QString>( e, "Bone Name" ) == oldName )
					nif->set<QString>( e, "Bone Name", newName );
			}
		} else if ( type == QLatin1String( "NiControllerSequence" ) ) {
			QModelIndex iBlocks = nif->getIndex( idx, "Controlled Blocks" );
			for ( int r = 0; r < nif->rowCount( iBlocks ); r++ ) {
				QModelIndex e = nif->getIndex( iBlocks, r );
				if ( nif->get<QString>( e, "Node Name" ) == oldName )
					nif->set<QString>( e, "Node Name", newName );
			}
		} else if ( type.startsWith( QLatin1String( "BSConnectPoint" ) ) ) {
			QModelIndex iPts = nif->getIndex( idx, "Connect Points" );
			for ( int r = 0; r < nif->rowCount( iPts ); r++ ) {
				QModelIndex e = nif->getIndex( iPts, r );
				if ( nif->get<QString>( e, "Parent" ) == oldName )
					nif->set<QString>( e, "Parent", newName );
			}
		}
	}
	return true;
}

int skeletonAddChildBone( NifModel * nif, int parentBlock, const QString & name, float length )
{
	if ( !nif || parentBlock < 0 )
		return -1;
	QModelIndex iParent = nif->getBlockIndex( parentBlock );
	if ( !iParent.isValid() || !nif->getIndex( iParent, "Children" ).isValid() )
		return -1;

	QModelIndex iNew = nif->insertNiBlock( QStringLiteral( "NiNode" ) );
	if ( !iNew.isValid() )
		return -1;
	const int newBlock = nif->getBlockNumber( iNew );

	nif->set<QString>( iNew, "Name", name );
	// Blender's extrude puts the new bone's head at the parent's tail, pointing
	// the same way. In NIF terms: offset along the parent's local +Y by the
	// characteristic bone length. Local, so it inherits the parent's orientation.
	nif->set<Vector3>( iNew, "Translation", Vector3( 0.0f, length, 0.0f ) );
	nif->set<float>( iNew, "Scale", 1.0f );

	// insertNiBlock may have shifted block numbers at or after the insertion
	// point, so re-read the parent rather than trusting the old index.
	QList<int> kids = skelChildren( nif, nif->getBlockNumber( nif->getBlockIndex( parentBlock ) ) );
	kids << newBlock;
	if ( !skelSetChildren( nif, parentBlock, kids ) )
		return -1;
	return newBlock;
}

bool skeletonReparent( NifModel * nif, int block, int newParent, bool keepTransform, QString * error )
{
	auto fail = [error]( const QString & m ) { if ( error ) *error = m; return false; };
	if ( !nif || block < 0 )
		return fail( QCoreApplication::translate( "skeleton", "invalid bone" ) );
	if ( block == newParent )
		return fail( QCoreApplication::translate( "skeleton", "a bone cannot parent to itself" ) );

	// Refuse a cycle: newParent must not be a descendant of block.
	for ( int p = newParent; p >= 0; p = nif->getParent( p ) ) {
		if ( p == block )
			return fail( QCoreApplication::translate( "skeleton",
				"that would put the bone inside its own subtree" ) );
	}

	const Transform worldBefore = skeletonWorldTransform( nif, block );
	const int oldParent = nif->getParent( block );

	if ( oldParent >= 0 ) {
		QList<int> kids = skelChildren( nif, oldParent );
		kids.removeAll( block );
		if ( !skelSetChildren( nif, oldParent, kids ) )
			return fail( QCoreApplication::translate( "skeleton", "could not detach from the old parent" ) );
	}
	if ( newParent >= 0 ) {
		QList<int> kids = skelChildren( nif, newParent );
		if ( !kids.contains( block ) )
			kids << block;
		if ( !skelSetChildren( nif, newParent, kids ) )
			return fail( QCoreApplication::translate( "skeleton", "could not attach to the new parent" ) );
	}

	if ( keepTransform ) {
		// Keep Transform: the bone must not move in world space, so its local
		// transform becomes inverse(newParentWorld) * oldWorld. Keep Offset (the
		// other choice) deliberately leaves the local transform alone, which DOES
		// move the bone — that is what makes it "keep the offset".
		const Transform parentWorld = ( newParent >= 0 )
			? skeletonWorldTransform( nif, newParent ) : Transform();
		const Transform local = parentWorld.inverted() * worldBefore;
		local.writeBack( nif, nif->getBlockIndex( block ) );
	}
	return true;
}

bool skeletonDissolve( NifModel * nif, int block, QString * error )
{
	auto fail = [error]( const QString & m ) { if ( error ) *error = m; return false; };
	if ( !nif || block < 0 )
		return fail( QCoreApplication::translate( "skeleton", "invalid bone" ) );
	const int parent = nif->getParent( block );
	if ( parent < 0 )
		return fail( QCoreApplication::translate( "skeleton",
			"the root cannot be dissolved — it has no parent to adopt its children" ) );

	// Blender's dissolve: the children survive and are adopted by the grandparent,
	// keeping their world position. Reparent first, then remove, so the removal
	// cannot orphan anything.
	const QList<int> kids = skelChildren( nif, block );
	for ( int c : kids ) {
		if ( c < 0 )
			continue;
		QString e;
		if ( !skeletonReparent( nif, c, parent, true, &e ) )
			return fail( e );
	}

	QList<int> pkids = skelChildren( nif, parent );
	pkids.removeAll( block );
	if ( !skelSetChildren( nif, parent, pkids ) )
		return fail( QCoreApplication::translate( "skeleton", "could not detach the bone" ) );
	nif->removeNiBlock( block );
	return true;
}

bool skeletonDeleteSubtree( NifModel * nif, int block, QString * error )
{
	auto fail = [error]( const QString & m ) { if ( error ) *error = m; return false; };
	if ( !nif || block < 0 )
		return fail( QCoreApplication::translate( "skeleton", "invalid bone" ) );

	// Gather the subtree first, then remove deepest-first: removeNiBlock
	// renumbers blocks, so collecting as we go would chase moving targets.
	QList<int> doomed;
	QList<int> stack;
	stack << block;
	while ( !stack.isEmpty() ) {
		const int b = stack.takeLast();
		if ( doomed.contains( b ) )
			continue;
		doomed << b;
		for ( int c : skelChildren( nif, b ) )
			if ( c >= 0 )
				stack << c;
	}

	const int parent = nif->getParent( block );
	if ( parent >= 0 ) {
		QList<int> pkids = skelChildren( nif, parent );
		pkids.removeAll( block );
		skelSetChildren( nif, parent, pkids );
	}

	// Descending block order keeps the not-yet-removed numbers valid.
	std::sort( doomed.begin(), doomed.end(), std::greater<int>() );
	for ( int b : doomed )
		nif->removeNiBlock( b );
	return true;
}
