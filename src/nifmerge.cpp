/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "nifmerge.h"

#include "nifsnapshot.h"
#include "model/nifmodel.h"
#include "spells/blocks.h"

#include <QBuffer>
#include <QDataStream>
#include <QHash>
#include <QMap>
#include <QSet>

/*
 *  Cross-file merge. The splice recipe (collect branch -> serialize each block
 *  with saveIndex -> insertNiBlock + loadAndMapLinks through a block map) is the
 *  one proven by spCollisionManager::importDonorCollision; the addition here is
 *  the de-duplication pass that lets several files share one skeleton.
 */

namespace
{

//! Every block reachable from \a block, parents before children.
void collectBranch( const NifModel * model, int block, QList<qint32> & blocks )
{
	if ( block < 0 || blocks.contains( block ) )
		return;
	blocks.append( block );
	for ( int child : model->getChildLinks( block ) )
		collectBranch( model, child, blocks );
}

//! Name -> block for every named NiNode already in the model.
/*! Only NiNodes take part: shapes are meant to be added, not merged, and two
 *  armour pieces legitimately carry same-named shapes. */
QHash<QString, int> namedNodes( const NifModel * nif )
{
	QHash<QString, int> byName;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex i = nif->getBlockIndex( b );
		if ( !nif->isNiBlock( i, "NiNode" ) )
			continue;
		const QString name = nif->get<QString>( i, "Name" );
		if ( !name.isEmpty() && !byName.contains( name ) )
			byName.insert( name, b );
	}
	return byName;
}

/*! Collect every header-string value in an item subtree, in traversal order.
 *
 *  From NIF 20.1.0.3 on, names and file paths are stored as an INDEX into the
 *  file's own header string table, not as text. A spliced block carries that
 *  index verbatim, and the target's table is a DIFFERENT table — donor index 36
 *  and target index 36 are two unrelated strings. Left alone, every imported node
 *  arrives wearing some existing node's name: merging a skeleton into an outfit
 *  produced a second "LArm_UpperTwist2_skin", a "LLeg_Toe1" parented under a
 *  forearm, and a bone list that no longer described the rig.
 *
 *  Both sides walk the same block type loaded from the same bytes, so the two
 *  traversals reach the same leaves in the same order and a plain sequence
 *  carries the strings across. (moveAllNiBlocks solves this with
 *  NifModel::updateStrings, which cannot be reused here: it addresses the target
 *  item through the DONOR item's pointer, which only works when blocks are moved
 *  rather than re-created.)
 */
void collectStrings( const NifModel * nif, const NifItem * item, QStringList & out )
{
	if ( !item )
		return;
	if ( item->childCount() > 0 ) {
		for ( int i = 0; i < item->childCount(); i++ )
			collectStrings( nif, item->child( i ), out );
	} else if ( item->valueType() == NifValue::tStringIndex ) {
		out.append( nif->resolveString( item ) );
	}
}

//! Replay collectStrings' sequence into the freshly imported block, allocating
//! each string in the TARGET's table.
void applyStrings( NifModel * nif, NifItem * item, const QStringList & in, int & pos )
{
	if ( !item )
		return;
	if ( item->childCount() > 0 ) {
		for ( int i = 0; i < item->childCount(); i++ )
			applyStrings( nif, item->child( i ), in, pos );
	} else if ( item->valueType() == NifValue::tStringIndex ) {
		if ( pos < in.size() )
			nif->assignString( item, in.at( pos ), false );
		pos++;
	}
}

//! block -> EVERY block listing it as a child. Normally one; a merge can leave
//! two, which is exactly the state this file has to notice and repair.
QHash<int, QList<int>> parentMap( const NifModel * nif )
{
	QHash<int, QList<int>> parents;
	for ( int b = 0; b < nif->getBlockCount(); b++ )
		for ( int c : nif->getChildLinks( b ) )
			if ( c >= 0 && !parents[c].contains( b ) )
				parents[c].append( b );
	return parents;
}

//! The single parent of a block, or -1. First listed wins, as the scene does.
int parentOf( const QHash<int, QList<int>> & parents, int block )
{
	const QList<int> p = parents.value( block );
	return p.isEmpty() ? -1 : p.first();
}

//! World transform of every NiAVObject, by walking each block up to its root.
QHash<int, Transform> worldTransforms( const NifModel * nif, const QHash<int, QList<int>> & parents )
{
	QHash<int, Transform> world;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex ib = nif->getBlockIndex( b );
		if ( !nif->blockInherits( ib, "NiAVObject" ) )
			continue;
		Transform t;
		QSet<int> seen;			// a malformed file can loop; do not hang on it
		for ( int n = b; n >= 0 && !seen.contains( n ); n = parentOf( parents, n ) ) {
			seen.insert( n );
			QModelIndex in = nif->getBlockIndex( n );
			if ( nif->blockInherits( in, "NiAVObject" ) )
				t = Transform( nif, in ) * t;
		}
		world.insert( b, t );
	}
	return world;
}

//! Drop one entry from a node's Children array, keeping Num Children in step.
void unlinkChild( NifModel * nif, int parentBlock, int childBlock )
{
	QModelIndex ip = nif->getBlockIndex( parentBlock );
	QModelIndex iSize = nif->getIndex( ip, "Num Children" );
	QModelIndex iArray = nif->getIndex( ip, "Children" );
	if ( !iSize.isValid() || !iArray.isValid() )
		return;
	QVector<qint32> keep;
	const int n = nif->get<int>( iSize );
	for ( int c = 0; c < n && c < nif->rowCount( iArray ); c++ ) {
		const qint32 l = nif->getLink( nif->getIndex( iArray, c ) );
		if ( l != childBlock )
			keep.append( l );
	}
	if ( keep.size() == n )
		return;
	nif->set<int>( iSize, keep.size() );
	nif->updateArraySize( iArray );
	for ( int c = 0; c < keep.size(); c++ )
		nif->setLink( nif->getIndex( iArray, c ), keep.at( c ) );
}

//! Names carried by more than one NiNode. See NifMergeResult::duplicateNames.
QStringList duplicateNodeNames( const NifModel * nif )
{
	QHash<QString, int> seen;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex i = nif->getBlockIndex( b );
		if ( !nif->isNiBlock( i, "NiNode" ) )
			continue;
		const QString name = nif->get<QString>( i, "Name" );
		if ( !name.isEmpty() )
			seen[name]++;
	}
	QStringList dupes;
	for ( auto it = seen.constBegin(); it != seen.constEnd(); it++ )
		if ( it.value() > 1 )
			dupes << it.key();
	dupes.sort();
	return dupes;
}

} // namespace

//! Merge an already-loaded \a donor model into \a target. Both nifMergeFile and
//! nifMergeData funnel here so the file and in-memory (archive) paths share one
//! splice; \a donorLabel names the source in messages and the undo step.
static bool mergeDonor( NifModel * target, NifModel & donor, const QString & donorLabel,
                        bool dedupeByName, NifMergeResult & result )
{
	auto fail = [&result]( const QString & msg ) {
		result.error = msg;
		return false;
	};

	if ( !target )
		return fail( QStringLiteral( "no target model" ) );

	if ( donor.getBSVersion() != target->getBSVersion() )
		return fail( QStringLiteral( "%1 is BS version %2, target is %3" )
			.arg( donorLabel ).arg( donor.getBSVersion() ).arg( target->getBSVersion() ) );

	// Only names the merge INTRODUCES are worth reporting; a target may already
	// carry a duplicate of its own, and that is not this merge's doing.
	const QStringList dupesBeforeList = duplicateNodeNames( target );
	const QSet<QString> dupesBefore( dupesBeforeList.constBegin(), dupesBeforeList.constEnd() );

	/* Where every existing node SITS, before anything is spliced in.
	 *
	 * An armour or clothing NIF stores its bones flat under the root, each
	 * carrying that bone's WORLD transform — the game re-parents them onto the
	 * real skeleton at runtime. A skeleton NIF stores the same bones nested, each
	 * carrying a LOCAL transform. Merging the two, the skeleton's parent-child
	 * edges land on the target's flat bones (a donor node's Children remap onto
	 * the de-duplicated target blocks), so a bone that was a root child becomes a
	 * grandchild of one — and its world transform is now composed on top of its
	 * new ancestors' instead of standing alone. That moved 1871 of an outfit's
	 * 3147 vertices: the reported heap.
	 *
	 * The hierarchy is what the merge is FOR, so it stays; what gets fixed is the
	 * transform. Every pre-existing node is rebased afterwards so its world
	 * transform is exactly what it was, which leaves the skin untouched and still
	 * lets a pose propagate down the new chain.
	 */
	const QHash<int, QList<int>> preParents = parentMap( target );
	const QHash<int, Transform> preWorld = worldTransforms( target, preParents );
	const int preBlockCount = target->getBlockCount();

	// The target root everything lands under.
	const QList<int> targetRoots = target->getRootLinks();
	if ( targetRoots.isEmpty() )
		return fail( QStringLiteral( "target has no root block" ) );
	const int targetRoot = targetRoots.first();

	// Donor top-level branches: the root's children, not the root itself. The
	// donor root is a per-file wrapper ("Armor_Torso.nif"); importing it would
	// nest a redundant node under the target root.
	QList<int> donorTops;
	for ( int r : donor.getRootLinks() ) {
		for ( int child : donor.getChildLinks( r ) )
			donorTops.append( child );
	}
	if ( donorTops.isEmpty() )
		return fail( QStringLiteral( "%1 has nothing under its root" ).arg( donorLabel ) );

	// Everything reachable, in a stable parents-first order.
	QList<qint32> branch;
	for ( int top : donorTops )
		collectBranch( &donor, top, branch );

	// De-duplication: a donor NiNode whose name already exists in the target
	// maps onto that block and is NOT imported. This is what makes several
	// merged pieces share one skeleton.
	const QHash<QString, int> existing = dedupeByName ? namedNodes( target )
	                                                  : QHash<QString, int>();
	QMap<qint32, qint32> map;      // donor block -> target block
	QList<qint32> toImport;
	for ( qint32 b : branch ) {
		QModelIndex iB = donor.getBlockIndex( b );
		if ( donor.isNiBlock( iB, "NiNode" ) ) {
			const QString name = donor.get<QString>( iB, "Name" );
			auto it = existing.constFind( name );
			if ( !name.isEmpty() && it != existing.constEnd() ) {
				map.insert( b, *it );      // reuse the target's node
				result.nodesReused++;
				continue;
			}
			result.nodesAdded++;
		}
		toImport.append( b );
	}

	// Imported blocks land at the end, in order, so their numbers are known
	// before the write and every link can be remapped in one pass.
	const int base = target->getBlockCount();
	for ( int i = 0; i < toImport.size(); i++ )
		map.insert( toImport.at( i ), base + i );

	// Serialize the blocks to import, and capture their header strings alongside:
	// the bytes carry string INDICES, which mean nothing in the target's table.
	QByteArray blob;
	QList<QStringList> stringsPerBlock;
	QBuffer writeBuf( &blob );
	writeBuf.open( QIODevice::WriteOnly );
	QDataStream out( &writeBuf );
	out << int( toImport.size() );
	for ( qint32 b : toImport ) {
		QModelIndex iB = donor.getBlockIndex( b );
		out << donor.itemName( iB );
		donor.saveIndex( writeBuf, iB );
		QStringList strs;
		collectStrings( &donor, donor.getItem( iB ), strs );
		stringsPerBlock.append( strs );
	}
	writeBuf.close();

	// Which imported blocks need linking under an existing target node: any
	// whose donor parent de-duplicated away (its Children array lives in the
	// target and knows nothing about the newcomer). Donor tops attach to the
	// target root.
	QHash<qint32, qint32> attachTo;   // donor block -> target parent block
	const QSet<qint32> importedSet( toImport.constBegin(), toImport.constEnd() );
	for ( qint32 b : toImport ) {
		if ( donorTops.contains( b ) ) {
			attachTo.insert( b, targetRoot );
			continue;
		}
		const int dParent = donor.getParent( b );
		if ( dParent >= 0 && !importedSet.contains( dParent ) ) {
			auto it = map.constFind( dParent );
			if ( it != map.constEnd() )
				attachTo.insert( b, *it );
		}
	}

	QString innerError;
	nifSnapshotOp( target, QStringLiteral( "Merge %1" ).arg( donorLabel ), [&]() {
		// A merge is a bulk load into a live model. loadIndex/loadAndMapLinks do
		// NOT suppress signals for you, and holdUpdates does not stop per-leaf
		// dataChanged — only the model state does. Without this a 38k-vertex
		// shape takes tens of seconds and looks like a hang.
		target->setState( BaseModel::Loading );
		target->holdUpdates( true );

		QBuffer readBuf( &blob );
		readBuf.open( QIODevice::ReadOnly );
		QDataStream in( &readBuf );
		int count = 0;
		in >> count;

		QList<qint32> newBlocks;
		for ( int i = 0; i < count; i++ ) {
			QString type;
			in >> type;
			QModelIndex iNew = target->insertNiBlock( type );
			if ( !iNew.isValid() ) {
				innerError = QStringLiteral( "could not create a %1" ).arg( type );
				break;
			}
			newBlocks.append( target->getBlockNumber( iNew ) );
			if ( !target->loadAndMapLinks( readBuf, iNew, map ) ) {
				innerError = QStringLiteral( "failed reading a %1 from the donor" ).arg( type );
				break;
			}
		}

		target->holdUpdates( false );
		target->restoreState();

		if ( !innerError.isEmpty() )
			return;

		// Re-allocate every header string in the target's own table. Done after
		// restoreState, because assignString is an ordinary edit and wants the
		// model out of Loading, and before the parenting below so the newcomers
		// are already correctly named if anything reports on them.
		for ( int i = 0; i < newBlocks.size() && i < stringsPerBlock.size(); i++ ) {
			int pos = 0;
			applyStrings( target, target->getItem( target->getBlockIndex( newBlocks.at( i ) ) ),
				stringsPerBlock.at( i ), pos );
		}

		// Parent the newcomers. blockLink appends to the right array for the
		// block type and keeps Num Children in step.
		for ( int i = 0; i < toImport.size() && i < newBlocks.size(); i++ ) {
			auto it = attachTo.constFind( toImport.at( i ) );
			if ( it == attachTo.constEnd() )
				continue;
			QModelIndex iParent = target->getBlockIndex( *it );
			QModelIndex iChild  = target->getBlockIndex( newBlocks.at( i ) );
			if ( iParent.isValid() && iChild.isValid() ) {
				blockLink( target, iParent, iChild );
				result.reparented++;
			}
		}

		/* Re-rig. An existing node adopted by an imported one now has two parents
		 * listed — the old one never let go — so the stale edge is cut first, and
		 * then the node is rebased onto its new chain so its WORLD transform is
		 * unchanged. Nodes whose parent did not move are left strictly alone: a
		 * rewrite that should be a no-op still costs float precision.
		 */
		const QHash<int, QList<int>> postParents = parentMap( target );
		QList<int> rebased;
		for ( auto it = postParents.constBegin(); it != postParents.constEnd(); it++ ) {
			const int block = it.key();
			if ( block >= preBlockCount || !preWorld.contains( block ) )
				continue;					// imported, or not an NiAVObject
			// the adoption shows up as a SECOND parent: the old edge is still
			// there, so comparing "the" parent before and after sees no change
			const int oldParent = parentOf( preParents, block );
			int newParent = -1;
			for ( int p : it.value() )
				if ( p != oldParent && p >= preBlockCount )
					newParent = p;
			if ( newParent < 0 )
				continue;
			if ( oldParent >= 0 )
				unlinkChild( target, oldParent, block );
			rebased.append( block );
		}
		const QHash<int, QList<int>> finalParents = parentMap( target );
		for ( int block : rebased ) {
			// Walk up for the new parent chain's world transform. A PRE-EXISTING
			// ancestor's final world is, by this very pass, exactly what it was
			// before the merge — so the walk stops there and reads preWorld rather
			// than the model. That makes each rebase independent of whether its
			// ancestors have been rewritten yet, and the order stops mattering.
			Transform parentWorld;
			QSet<int> seen;
			for ( int n = parentOf( finalParents, block ); n >= 0 && !seen.contains( n );
			      n = parentOf( finalParents, n ) ) {
				seen.insert( n );
				QModelIndex in = target->getBlockIndex( n );
				if ( !target->blockInherits( in, "NiAVObject" ) )
					continue;
				if ( n < preBlockCount && preWorld.contains( n ) ) {
					parentWorld = preWorld.value( n ) * parentWorld;
					break;
				}
				parentWorld = Transform( target, in ) * parentWorld;
			}
			( parentWorld.inverted() * preWorld.value( block ) )
				.writeBack( target, target->getBlockIndex( block ) );
		}
		result.rebased = rebased.size();

		result.blocksAdded = newBlocks.size();
		for ( qint32 b : newBlocks ) {
			if ( target->blockInherits( target->getBlockIndex( b ), "NiAVObject" )
				 && !target->isNiBlock( target->getBlockIndex( b ), "NiNode" ) )
				result.shapesAdded++;
		}
	} );

	if ( !innerError.isEmpty() )
		return fail( innerError );
	if ( result.blocksAdded == 0 )
		return fail( QStringLiteral( "nothing was merged" ) );

	for ( const QString & d : duplicateNodeNames( target ) )
		if ( !dupesBefore.contains( d ) )
			result.duplicateNames << d;
	return true;
}

bool nifMergeFile( NifModel * target, const QString & donorPath,
                   bool dedupeByName, NifMergeResult & result )
{
	NifModel donor;
	if ( !donor.loadFromFile( donorPath ) ) {
		result.error = QStringLiteral( "could not load %1" ).arg( donorPath );
		return false;
	}
	return mergeDonor( target, donor, QFileInfo( donorPath ).fileName(), dedupeByName, result );
}

bool nifMergeData( NifModel * target, const QByteArray & data, const QString & label,
                   bool dedupeByName, NifMergeResult & result )
{
	NifModel donor;
	QByteArray bytes = data;                 // load() consumes a QIODevice
	QBuffer device( &bytes );
	if ( !device.open( QIODevice::ReadOnly ) || !donor.load( device ) ) {
		result.error = QStringLiteral( "could not parse %1" )
			.arg( label.isEmpty() ? QStringLiteral( "the NIF" ) : label );
		return false;
	}
	return mergeDonor( target, donor, label.isEmpty() ? QStringLiteral( "NIF" ) : label,
	                   dedupeByName, result );
}
