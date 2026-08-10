/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "nifmerge.h"

#include "animgraph.h"
#include "nifsnapshot.h"
#include "model/nifmodel.h"
#include "spells/blocks.h"

#include <QBuffer>
#include <QDataStream>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QObject>
#include <QPointer>
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

/*! The skeleton node a Fallout 4 effect NIF asks to hang from.
 *
 *  An ArtObject's root carries a NiStringsExtraData called "AttachT" whose
 *  entries are either "NamedNode&<NodeName>" — hang me off that node — or engine
 *  hints such as "MultiTechnique", which name nothing. The X-01 Tesla legs say
 *  NamedNode&RLeg_Calf_Armor2; its torso, arms and helmet say only
 *  MultiTechnique, because their attach node lives in the ARTO record in the ESP
 *  rather than in the mesh. So an empty return means "this file does not say",
 *  not "attach at the root" — the caller has to decide, and be told.
 */
//! Where a single top-level donor branch says it belongs; -1 when it does not say.
/*! Bethesda's ArtObjects name their destination in the BRANCH, not only in
 *  AttachT: a top-level child called `NamedAttach<NodeName>` is the group of
 *  effects meant to hang on the skeleton node `<NodeName>`, and `NamedAttachRoot`
 *  means the actor root. Read off the six X-01 Tesla files, whose roots have
 *  exactly these children and nothing else:
 *
 *      torso  NamedAttachTank_Armor, NamedAttachRoot
 *      arm    NamedAttachL_Pauldron, NamedAttachRoot, NamedAttachLArm_ForeArm_Armor
 *      helmet NamedAttachHEAD
 *
 *  This matters because one file can carry effects for SEVERAL nodes — the arm
 *  has a pauldron arc and a forearm arc — and hanging the whole file on one node
 *  puts the forearm arc on the shoulder. AttachT can only ever name one place,
 *  so it cannot express that; the branch names can.
 *
 *  A name that matches no node in the target is ignored rather than treated as
 *  an error: the branch then falls back to whatever the file-level AttachT or
 *  --attach decided, which is the old behaviour.
 */
int namedAttachNode( const NifModel & donor, int donorTop,
                     const QHash<QString, int> & targetByName, int targetRoot )
{
	QModelIndex iTop = donor.getBlockIndex( donorTop );
	if ( !donor.blockInherits( iTop, "NiAVObject" ) )
		return -1;
	const QString name = donor.get<QString>( iTop, "Name" );
	static const QLatin1String prefix( "NamedAttach" );
	if ( !name.startsWith( prefix ) )
		return -1;

	const QString wanted = name.mid( prefix.size() );
	if ( wanted.isEmpty() || wanted == QLatin1String( "Root" ) )
		return targetRoot;
	auto it = targetByName.constFind( wanted );
	return it == targetByName.constEnd() ? -1 : *it;
}

/*! Split one `AttachT` entry into its technique tag and argument.
 *
 *  The engine's split is one `strchr(entry, '&')`
 *  (`BGSAttachTechniquesUtil::ProcessAttachTechniques`, 1.10.155 `0x171a1b`):
 *  everything before the ampersand is the tag, everything after is the argument,
 *  handed to the technique verbatim. An entry with no ampersand is all tag and
 *  no argument — which is why `MultiTechnique` on its own is a valid entry and
 *  names nothing.
 *
 *  Four techniques register themselves, each under a tag string stored next to
 *  its vtable: `NamedNode`, `HavokGeometry`, `MultiTechnique` and the particle
 *  array one. Only `NamedNode` takes a node name.
 */
static QString attachTag( const QString & entry, QString * arg = nullptr )
{
	const int amp = entry.indexOf( QLatin1Char( '&' ) );
	if ( amp < 0 ) {
		if ( arg )
			arg->clear();
		return entry;
	}
	if ( arg )
		*arg = entry.mid( amp + 1 );
	return entry.left( amp );
}

/*! The node name inside a `NamedNode&` argument.
 *
 *  Vanilla arguments are usually a bare node name, but not always: FO4 ships
 *  `NamedNode&C-ArmsTypeA1|0` on the Mr Handy arm assets, and the engine passes
 *  the whole thing through to `BSUtilities::GetObjectByName` without stripping
 *  the suffix. No engine code seen strips it, so what `|0` selects is not
 *  settled here — but a node called `C-ArmsTypeA1|0` does not exist in those
 *  files either, so matching the argument whole finds nothing at all.
 *
 *  Exact first, then without the suffix. That order cannot turn a match into a
 *  miss, and it is the difference between placing those effects and dropping them.
 */
static QString attachNodeArg( const QString & arg, const QHash<QString, int> * known = nullptr )
{
	if ( arg.isEmpty() )
		return arg;
	if ( known && known->contains( arg ) )
		return arg;
	const int bar = arg.indexOf( QLatin1Char( '|' ) );
	if ( bar > 0 )
		return arg.left( bar );
	return arg;
}

QString attachNodeName( const NifModel & donor, bool * isEffect = nullptr,
                        const QHash<QString, int> * targetByName = nullptr )
{
	if ( isEffect )
		*isEffect = false;
	const QList<int> roots = donor.getRootLinks();
	if ( roots.isEmpty() )
		return QString();
	QModelIndex iRoot = donor.getBlockIndex( roots.first() );
	QModelIndex iList = donor.getIndex( iRoot, "Extra Data List" );
	for ( int i = 0; i < donor.rowCount( iList ); i++ ) {
		const int link = donor.getLink( donor.getIndex( iList, i ) );
		if ( link < 0 )
			continue;
		QModelIndex iEx = donor.getBlockIndex( link );
		if ( !donor.isNiBlock( iEx, "NiStringsExtraData" )
			|| donor.get<QString>( iEx, "Name" ) != QLatin1String( "AttachT" ) )
			continue;
		if ( isEffect )
			*isEffect = true;			// it IS an ArtObject, whatever it names
		QModelIndex iData = donor.getIndex( iEx, "Data" );
		for ( int s = 0; s < donor.rowCount( iData ); s++ ) {
			QString arg;
			const QString tag = attachTag( donor.get<QString>( donor.getIndex( iData, s ) ), &arg );
			if ( tag == QLatin1String( "NamedNode" ) && !arg.isEmpty() )
				return attachNodeArg( arg, targetByName );
		}
	}
	return QString();
}

//! Node names that act as SKELETON in this file: every node some skin binds to,
//! plus every ancestor of one.
/*! De-duplicating NiNodes by name is what lets several merged pieces pose as one
 *  rig — but applied to every node it is actively destructive, because effect
 *  files are authored from a shared template and reuse the same internal names.
 *  Measured: `X01_ArmLeft_Tesla_VFX` and `X01_ArmRight_Tesla_VFX` have 20 nodes
 *  each and **15 of the names are identical** — `LightningBolt_01`, `BoltGeo_01`,
 *  `LightningArcs_VFX`... Fusing those hangs the right arm's effects off the left
 *  arm's nodes, and the geometry lands over a hundred units away.
 *
 *  A bone is the one node kind that MUST be shared: two pieces skinned to
 *  "LArm_UpperArm" have to end up on one node or posing moves only one of them.
 *  Nothing else has to be, and the ancestor chain comes along because a bone is
 *  useless without the nodes that position it.
 *
 *  Both sides are consulted by the caller: the donor's bones cover armour merged
 *  into a skeleton, the target's cover a skeleton merged into an outfit.
 */
QSet<QString> skeletonNodeNames( const NifModel * nif )
{
	QSet<QString> names;
	if ( !nif )
		return names;

	const QHash<int, QList<int>> parents = parentMap( nif );
	QList<int> boneBlocks;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex idx = nif->getBlockIndex( b );
		if ( !idx.isValid() )
			continue;
		// Both skin flavours name their bones in a "Bones" link array: FO4's
		// BSSkin::Instance and the older NiSkinInstance.
		for ( const qint32 bone : nif->getLinkArray( idx, "Bones" ) )
			if ( bone >= 0 )
				boneBlocks.append( bone );
	}

	QSet<int> seen;
	while ( !boneBlocks.isEmpty() ) {
		const int b = boneBlocks.takeLast();
		if ( b < 0 || seen.contains( b ) )
			continue;
		seen << b;
		const QString name = nif->get<QString>( nif->getBlockIndex( b ), "Name" );
		if ( !name.isEmpty() )
			names << name;
		for ( int p : parents.value( b ) )
			boneBlocks.append( p );
	}
	return names;
}

//! The NiAVObject that owns \a prop, or -1. A shader-property controller targets
//! the PROPERTY, while the sequence row that drives it names the SHAPE.
int ownerOfProperty( const NifModel * nif, int prop )
{
	if ( prop < 0 )
		return -1;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex idx = nif->getBlockIndex( b );
		if ( !nif->blockInherits( idx, "NiAVObject" ) )
			continue;
		for ( const qint32 c : nif->getChildLinks( b ) )
			if ( c == prop )
				return b;
	}
	return -1;
}

/*! Give every imported effect object a name nothing else in the file uses, and
 *  re-aim the references that named it.
 *
 *  Effect files are authored from a shared template: six X-01 Tesla VFX files
 *  bring six nodes called `LightningBolt_01` and six shapes called `BoltGeo_01`,
 *  and two of the six duplicate names inside themselves. That was survivable
 *  while the merge only had to place geometry — each copy hung under its own
 *  limb, and the pointers were pointers.
 *
 *  It stops being survivable the moment the ANIMATION comes too, because a
 *  sequence addresses what it drives BY NAME, through the manager's object
 *  palette: one name, one entry, one node. Six `LightningBolt_01`s in one palette
 *  is five effects driving the helmet's node.
 *
 *  So a colliding name is qualified with its first pre-existing ancestor — on a
 *  merged rig that is the limb it attached to, giving `RLeg_Calf_Armor2_
 *  LightningBolt_01` — the same rule `qualifiedEffectName` uses when the bake
 *  writes shapes, so both halves of the pipeline name things the same way.
 *
 *  Nothing outside the file addresses these: they are the names the merge already
 *  reports as private. What DOES address them is rewritten here — the object
 *  palette by pointer, and each controlled block by way of its controller's
 *  target, falling back to the old name when the row carries no controller.
 */
void uniquifyEffectNames( NifModel * nif, const QList<qint32> & imported,
                          int preBlockCount, NifMergeResult & result )
{
	QSet<QString> used;
	for ( int b = 0; b < preBlockCount && b < nif->getBlockCount(); b++ ) {
		QModelIndex idx = nif->getBlockIndex( b );
		if ( !nif->blockInherits( idx, "NiAVObject" ) )
			continue;
		const QString name = nif->get<QString>( idx, "Name" );
		if ( !name.isEmpty() )
			used << name;
	}

	const QHash<int, QList<int>> parents = parentMap( nif );
	QHash<int, QString> renamed;              // block -> its new name
	QHash<QString, QList<int>> wasCalled;     // old name -> blocks renamed off it

	for ( const qint32 b : imported ) {
		QModelIndex idx = nif->getBlockIndex( b );
		if ( !idx.isValid() || !nif->blockInherits( idx, "NiAVObject" ) )
			continue;
		const QString name = nif->get<QString>( idx, "Name" );
		if ( name.isEmpty() )
			continue;
		if ( !used.contains( name ) ) {
			used << name;
			continue;
		}

		// The first ancestor that was already in the file — the attach node.
		QString prefix;
		QSet<int> seen;
		for ( int p = parentOf( parents, b ); p >= 0 && !seen.contains( p );
		      p = parentOf( parents, p ) ) {
			seen << p;
			if ( p < preBlockCount ) {
				prefix = nif->get<QString>( nif->getBlockIndex( p ), "Name" );
				break;
			}
		}
		QString candidate = prefix.isEmpty() ? name : prefix + QLatin1Char( '_' ) + name;
		const QString base = candidate;
		for ( int n = 2; used.contains( candidate ); n++ )
			candidate = base + QLatin1Char( '_' ) + QString::number( n );

		nif->set<QString>( idx, "Name", candidate );
		used << candidate;
		renamed.insert( b, candidate );
		wasCalled[name].append( b );
		result.nodesRenamed++;
	}

	if ( renamed.isEmpty() )
		return;

	for ( const qint32 b : imported ) {
		QModelIndex idx = nif->getBlockIndex( b );
		if ( !idx.isValid() )
			continue;

		if ( nif->isNiBlock( idx, "NiDefaultAVObjectPalette" ) ) {
			QModelIndex iObjs = nif->getIndex( idx, "Objs" );
			for ( int r = 0; r < nif->rowCount( iObjs ); r++ ) {
				QModelIndex iRow = nif->getIndex( iObjs, r );
				auto it = renamed.constFind( nif->getLink( iRow, "AV Object" ) );
				if ( it != renamed.constEnd() )
					nif->assignString( iRow, QStringLiteral( "Name" ), *it, false );
			}
			continue;
		}

		if ( !nif->isNiBlock( idx, "NiControllerSequence" ) )
			continue;
		QModelIndex iArr = nif->getIndex( idx, "Controlled Blocks" );
		for ( int r = 0; r < nif->rowCount( iArr ); r++ ) {
			QModelIndex iRow = nif->getIndex( iArr, r );

			/* The NAME is what identifies the row's node, and the controller link is
			 * only a shortcut — so the name is consulted first.
			 *
			 * Following the controller first was wrong in exactly the case that
			 * matters most. A row driven by the file's NiMultiTargetTransformController
			 * — which is every row that moves a NODE, as opposed to animating a
			 * property — points at that one shared controller, whose Target is the
			 * ROOT. So the lookup resolved to the root, found it unrenamed, and left
			 * the row naming a node that had just been renamed out from under it.
			 *
			 * What that produced: six merged effect files, each with a row saying
			 * "LightningBolt_01_End", all resolving through one palette to the FIRST
			 * file's node. The torso's bolt endpoints were dragged to head height and
			 * out past the shoulder, and every other limb's endpoints stopped being
			 * animated at all — which looks fine, because their bind pose is where
			 * they belong.
			 */
			int node = -1;
			const QList<int> was = wasCalled.value( nif->resolveString( iRow, "Node Name" ) );
			if ( was.size() == 1 ) {
				node = was.first();
			} else if ( !was.isEmpty() ) {
				// The donor wore that name twice; the controller's target is the only
				// thing left that can tell the two apart.
				const int ctrl = nif->getLink( iRow, "Controller" );
				if ( ctrl >= 0 ) {
					const int target = nif->getLink( nif->getBlockIndex( ctrl ), "Target" );
					const int owner = nif->blockInherits( nif->getBlockIndex( target ), "NiAVObject" )
						? target : ownerOfProperty( nif, target );
					if ( was.contains( owner ) )
						node = owner;
				}
			}
			auto it = renamed.constFind( node );
			if ( it != renamed.constEnd() )
				nif->assignString( iRow, QStringLiteral( "Node Name" ), *it, false );
		}
	}
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

namespace
{

/*! Where a weapon part is going, worked out before the splice.
 *
 *  Internal — the public entry point is nifMergeWeaponPart. Two things the plain
 *  attach path cannot express:
 *
 *   - the destination is a BLOCK, not a name. The node a connect point rides can
 *     share its name with something else once parts from two different weapons
 *     are in one file, and the resolver has already picked the exact one.
 *   - the part gets a WRAPPER node carrying `xform`, and its branches hang off
 *     that instead of off the destination directly. That node is the part's own
 *     root, and it is what makes chains work: a connect point with an empty
 *     `Parent` is expressed in its mesh's ROOT frame, so unless that frame is a
 *     real node in the assembly, the next part along (a suppressor asking the
 *     barrel for P-Muzzle) has nothing to measure from.
 */
struct WeaponAttach
{
	int attachBlock = -1;     //!< target block the wrapper hangs under
	Transform xform;          //!< the connect point transform, the wrapper's local
	QString wrapperName;      //!< what to call the wrapper
};

} // namespace

//! Merge an already-loaded \a donor model into \a target. Both nifMergeFile and
//! nifMergeData funnel here so the file and in-memory (archive) paths share one
//! splice; \a donorLabel names the source in messages and the undo step.
static bool mergeDonor( NifModel * target, NifModel & donor, const QString & donorLabel,
                        bool dedupeByName, NifMergeResult & result,
                        const QString & attachOverride = QString(),
                        const WeaponAttach * weapon = nullptr )
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

	/* ...unless the donor is an effect that names a node to hang from, or the
	 * caller names one. An ArtObject dropped at the root sits at the origin
	 * instead of on the limb it belongs to, which looks like the effect "did not
	 * import" when in fact it imported perfectly and landed at the actor's feet.
	 */
	const QHash<QString, int> byName = namedNodes( target );
	int attachBlock = targetRoot;
	const QString declared = attachNodeName( donor, &result.isEffect, &byName );
	result.attachRequested = attachOverride.isEmpty() ? declared : attachOverride;
	if ( weapon && weapon->attachBlock >= 0 ) {
		// by block, not by name — see WeaponAttach
		attachBlock = weapon->attachBlock;
		result.attachRequested.clear();
		result.attachedTo = target->get<QString>( target->getBlockIndex( attachBlock ), "Name" );
	} else if ( !result.attachRequested.isEmpty() ) {
		auto it = byName.constFind( result.attachRequested );
		if ( it == byName.constEnd() )
			return fail( QStringLiteral( "%1 attaches to \"%2\", which this file has no node for" )
				.arg( donorLabel, result.attachRequested ) );
		attachBlock = *it;
		result.attachedTo = result.attachRequested;
	}

	/* Donor top-level branches: the root's children, not the root itself. The
	 * donor root is a per-file wrapper ("Armor_Torso.nif"); importing it would
	 * nest a redundant node under the target root.
	 *
	 * Three of those children describe the FILE rather than anything in it, and
	 * are skipped when the target already has its own: the root's `AttachT` (where
	 * this file wants hanging — a question the merge has just answered, and which
	 * afterwards reads as a claim about whatever node it landed on), `BSXFlags`
	 * and `BSBehaviorGraphExtraData`. Merging twelve pieces otherwise brings
	 * twelve of each, and every vanilla file has one.
	 */
	bool targetHasBSX = false, targetHasAttachT = false;
	for ( const qint32 e : target->getLinkArray( target->getBlockIndex( targetRoot ), "Extra Data List" ) ) {
		if ( e < 0 )
			continue;
		QModelIndex iEx = target->getBlockIndex( e );
		if ( target->blockInherits( iEx, { "BSXFlags", "BSBehaviorGraphExtraData" } ) )
			targetHasBSX = true;
		if ( target->isNiBlock( iEx, "NiStringsExtraData" )
		     && target->get<QString>( iEx, "Name" ) == QLatin1String( "AttachT" ) )
			targetHasAttachT = true;
	}
	// An AttachT that would land on a BONE is worse than redundant: extra data is
	// linked into whatever node the branch attaches to, so the helmet effect's
	// "NamedNode&HEAD" ended up ON the HEAD bone, saying that the skeleton's head
	// is an ArtObject that wants attaching to itself.
	const bool skipAttachT = targetHasAttachT || attachBlock != targetRoot;

	QList<int> donorTops;
	for ( int r : donor.getRootLinks() ) {
		for ( int child : donor.getChildLinks( r ) ) {
			QModelIndex iChild = donor.getBlockIndex( child );
			if ( targetHasBSX
			     && donor.blockInherits( iChild, { "BSXFlags", "BSBehaviorGraphExtraData" } ) )
				continue;
			if ( skipAttachT && donor.isNiBlock( iChild, "NiStringsExtraData" )
			     && donor.get<QString>( iChild, "Name" ) == QLatin1String( "AttachT" ) )
				continue;
			donorTops.append( child );
		}
	}
	if ( donorTops.isEmpty() )
		return fail( QStringLiteral( "%1 has nothing under its root" ).arg( donorLabel ) );

	// Everything reachable, in a stable parents-first order.
	QList<qint32> branch;
	for ( int top : donorTops )
		collectBranch( &donor, top, branch );

	// De-duplication: a donor NiNode whose name already exists in the target
	// maps onto that block and is NOT imported. This is what makes several
	// merged pieces share one skeleton — and it is restricted to nodes that
	// ACT as skeleton, because effect files reuse internal node names wholesale
	// and fusing those puts one limb's effects on another's. See
	// skeletonNodeNames.
	const QHash<QString, int> existing = dedupeByName ? byName : QHash<QString, int>();
	/* An ArtObject's internal nodes stay PRIVATE. Effect files are authored from
	 * a shared template and reuse names wholesale — X01_ArmLeft_Tesla_VFX and
	 * X01_ArmRight_Tesla_VFX have 20 nodes each and 15 of the names are identical
	 * (LightningBolt_01, BoltGeo_01, LightningArcs_VFX...). Fusing those hangs the
	 * right arm's effects off the left arm's nodes, and the geometry lands over a
	 * hundred units away from the limb it belongs to.
	 *
	 * Only effect files are treated this way. Armour, skeletons and props keep
	 * the plain name-dedupe, which several sweep cases depend on: an FO4 armour
	 * piece stores its bones FLAT, so "Chest" is present in the file without the
	 * arm being skinned to it, and a bone-reachability rule would wrongly
	 * duplicate it. What an effect file still shares is a genuine bone — if it
	 * skins to one, it must land on the same node as everything else.
	 */
	QSet<QString> shareable;
	if ( dedupeByName && result.isEffect )
		shareable = skeletonNodeNames( &donor ) | skeletonNodeNames( target );
	QMap<qint32, qint32> map;      // donor block -> target block
	QList<qint32> toImport;
	for ( qint32 b : branch ) {
		QModelIndex iB = donor.getBlockIndex( b );
		if ( donor.isNiBlock( iB, "NiNode" ) ) {
			const QString name = donor.get<QString>( iB, "Name" );
			auto it = existing.constFind( name );
			const bool known = !name.isEmpty() && it != existing.constEnd();
			if ( known && ( !result.isEffect || shareable.contains( name ) ) ) {
				map.insert( b, *it );      // reuse the target's node
				result.nodesReused++;
				continue;
			}
			if ( known )
				result.privateNames.append( name );
			result.nodesAdded++;
		}
		toImport.append( b );
	}
	result.privateNames.removeDuplicates();

	/* The wrapper node goes in FIRST, so its number is the current block count and
	 * the imported blocks start one later. Both numbers are known here, before a
	 * single byte is written, which is what lets every link be remapped in one
	 * pass — and it is why the wrapper is created inside the snapshot below rather
	 * than here, where it would be outside the undo step. */
	const int wrapperBlock = weapon ? target->getBlockCount() : -1;

	// Imported blocks land at the end, in order, so their numbers are known
	// before the write and every link can be remapped in one pass.
	const int base = target->getBlockCount() + ( wrapperBlock >= 0 ? 1 : 0 );
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
	// target root, unless the branch names its own destination.
	QHash<qint32, qint32> attachTo;   // donor block -> target parent block
	const QSet<qint32> importedSet( toImport.constBegin(), toImport.constEnd() );
	QHash<qint32, qint32> namedAttachParent;   // donor top -> target node it named
	for ( qint32 b : toImport ) {
		if ( donorTops.contains( b ) ) {
			if ( wrapperBlock >= 0 ) {
				// A weapon part's branches belong to its own root node, and a part
				// file carries no AttachT / NamedAttach convention to consult.
				attachTo.insert( b, wrapperBlock );
				continue;
			}
			const int named = namedAttachNode( donor, b, byName, targetRoot );
			attachTo.insert( b, named >= 0 ? named : attachBlock );
			/* Rebase ONLY when the file's own AttachT named nothing. That is the
			 * signal for which space the branch is authored in, and the X-01 Tesla
			 * set has one file of each kind:
			 *
			 *   helmet  AttachT says NamedNode&HEAD  -> node-local, and it ALSO has
			 *           a NamedAttachHEAD branch. Rebasing it drove the pulse from
			 *           the head down to Z = 5, at the ankles.
			 *   torso   AttachT says only MultiTechnique -> the destination lives in
			 *   arms    the ESP's ARTO record, the branch is in actor space, rebase.
			 *
			 * An --attach override does not change the authoring space, so it is
			 * `declared` that is consulted here and not attachRequested.
			 */
			/* Rebase only a branch that is actually authored in ACTOR space, and
			 * decide that from the geometry rather than by file. The two X-01
			 * conventions sit side by side and MEASURE differently:
			 *
			 *   arm    NamedAttachR_Pauldron   translation (20.96, -7.33, 126.14)
			 *          -- a shoulder POSITION on the actor: actor space, rebase.
			 *   torso  NamedAttachTank_Armor   translation (0, 0, ~0)
			 *          -- identity, content already relative to the chest bone:
			 *             node-local, and rebasing it subtracts the bone's height,
			 *             which is what dropped the Tesla fan from Z 128 to Z 24.
			 *
			 * The discriminator: an actor-space branch's translation is expressed
			 * in the same frame as the node's world position, so it lands NEAR that
			 * position. A node-local one does not. Compare the two distances.
			 */
			if ( named >= 0 && named != targetRoot && declared.isEmpty()
			     && preWorld.contains( named ) ) {
				QModelIndex iTop = donor.getBlockIndex( b );
				if ( Transform::canConstruct( &donor, iTop ) ) {
					const Vector3 localT = Transform( &donor, iTop ).translation;
					const Vector3 nodeT = preWorld.value( named ).translation;
					if ( ( localT - nodeT ).length() < localT.length() )
						namedAttachParent.insert( b, named );
				}
			}
			if ( named >= 0 ) {
				// The root is called after the file it came from ("skeleton.nif"),
				// which reads as a node name and is not one worth reporting.
				const QString line = ( named == targetRoot )
					? QStringLiteral( "the root" )
					: target->get<QString>( target->getBlockIndex( named ), "Name" );
				if ( !result.namedAttachments.contains( line ) )
					result.namedAttachments.append( line );
			}
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
		/* The part's own root, carrying the connect point's transform. Created
		 * before anything is loaded so it takes the block number reserved for it
		 * above, and inside the snapshot so one undo takes it away with the rest. */
		if ( wrapperBlock >= 0 ) {
			QModelIndex iWrap = target->insertNiBlock( QStringLiteral( "NiNode" ) );
			if ( !iWrap.isValid() || target->getBlockNumber( iWrap ) != wrapperBlock ) {
				innerError = QStringLiteral( "could not create a node for %1" ).arg( donorLabel );
				return;
			}
			target->set<QString>( iWrap, "Name", weapon->wrapperName );
			weapon->xform.writeBack( target, iWrap );
			blockLink( target, target->getBlockIndex( attachBlock ), iWrap );
		}
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

		/* A NamedAttach branch is authored in ACTOR space, not in the space of the
		 * node it names. Measured on X01_ArmRight_Tesla_VFX:
		 *
		 *     NamedAttachR_Pauldron            translation (20.96, -7.33, 126.14)
		 *     NamedAttachRArm_ForeArm_Armor    translation (35.36, -8.90, 103.57)
		 *
		 * Those are shoulder- and elbow-height positions on the actor, not offsets
		 * from the shoulder. Hanging such a branch straight under its bone applies
		 * the bone's world transform on top, and because arm bones carry a large
		 * rotation the piece does not merely double its offset — it is flung. The
		 * X-01 Tesla arm pulse landed at X = -117 and -156 with the arms at +-25.
		 *
		 * So the branch is rebased onto the node: newLocal = nodeWorld^-1 * local,
		 * which leaves its WORLD position exactly where the file put it.
		 *
		 * The AttachT path is deliberately NOT rebased. Those files are authored
		 * node-local — X01_LegLeft_Tesla_VFX's tops sit at (-0.61, -15.44, 17.71)
		 * and the like, offsets from the calf — and rebasing them would break what
		 * currently works.
		 */
		for ( int i = 0; i < toImport.size() && i < newBlocks.size(); i++ ) {
			auto it = namedAttachParent.constFind( toImport.at( i ) );
			if ( it == namedAttachParent.constEnd() || !preWorld.contains( *it ) )
				continue;
			QModelIndex iChild = target->getBlockIndex( newBlocks.at( i ) );
			if ( !iChild.isValid() || !Transform::canConstruct( target, iChild ) )
				continue;
			const Transform local( target, iChild );
			( preWorld.value( *it ).inverted() * local ).writeBack( target, iChild );
			result.namedRebased++;
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

		/* The animation, last, because both passes renumber or rename blocks and
		 * every count above is taken from the numbers as spliced.
		 *
		 * Names first: the fold merges object palettes BY NAME, so two nodes still
		 * called LightningBolt_01 would collapse into one palette entry and five
		 * limbs' sequences would drive the helmet's node.
		 */
		if ( result.isEffect )
			uniquifyEffectNames( target, newBlocks, preBlockCount, result );

		const AnimGraphResult anim = consolidateControllerManagers( target );
		result.managersFolded = anim.managersFolded;
		result.sequencesFused = anim.sequencesFused;
		result.sequenceNames  = anim.sequenceNames;
		// An object palette entry that resolves to nothing is one the donor
		// brought: X01_Torso_Tesla_VFX ships one naming a shape it does not have.
		pruneDeadAnimLinks( target );
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
                   bool dedupeByName, NifMergeResult & result, const QString & attachTo )
{
	NifModel donor;
	if ( !donor.loadFromFile( donorPath ) ) {
		result.error = QStringLiteral( "could not load %1" ).arg( donorPath );
		return false;
	}
	return mergeDonor( target, donor, QFileInfo( donorPath ).fileName(), dedupeByName,
		result, attachTo );
}

bool nifMergeData( NifModel * target, const QByteArray & data, const QString & label,
                   bool dedupeByName, NifMergeResult & result, const QString & attachTo )
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
	                   dedupeByName, result, attachTo );
}


/* ============================================================ WEAPON PARTS ===
 *
 * The Loaded NIFs weapon mark and the two things the merge can honestly say
 * about a part before it splices it in. See nifmerge.h for the contract; the one
 * rule worth repeating here is that none of this decides whether a combination
 * is ALLOWED. Any part may be merged onto any target. What is computed is what
 * the two files contain, and the answers are notes, never refusals.
 */

//! The marked models. QPointer, so a closed document unmarks itself: the merge
//! compares raw pointers against live documents and must never match a freed one.
static QList<QPointer<NifModel>> wwWeaponParts;

static void pruneWeaponMarks()
{
	for ( int i = wwWeaponParts.size() - 1; i >= 0; i-- )
		if ( !wwWeaponParts.at( i ) )
			wwWeaponParts.removeAt( i );
}

void nifSetWeaponMark( NifModel * model, bool marked )
{
	pruneWeaponMarks();
	if ( !model )
		return;
	const bool already = nifIsWeaponMarked( model );
	if ( marked == already )
		return;
	if ( marked ) {
		wwWeaponParts.append( QPointer<NifModel>( model ) );
		return;
	}
	for ( int i = wwWeaponParts.size() - 1; i >= 0; i-- )
		if ( wwWeaponParts.at( i ).data() == model )
			wwWeaponParts.removeAt( i );
}

bool nifIsWeaponMarked( const NifModel * model )
{
	if ( !model )
		return false;
	for ( const QPointer<NifModel> & p : std::as_const( wwWeaponParts ) )
		if ( p.data() == model )
			return true;
	return false;
}

int nifWeaponMarkCount()
{
	pruneWeaponMarks();
	return int( wwWeaponParts.size() );
}

void nifClearWeaponMarks()
{
	wwWeaponParts.clear();
}

QString nifWeaponAttachNode( const NifModel * target )
{
	if ( !target )
		return QString();
	/* namedNodes() is the merge's OWN any-depth name index — the same lookup
	 * mergeDonor resolves an attach request through. Asking it here is what stops
	 * this from answering "yes, WEAPON" where the splice would then find nothing
	 * and silently drop the part at the root. */
	return namedNodes( target ).contains( QStringLiteral( "WEAPON" ) )
		? QStringLiteral( "WEAPON" ) : QString();
}

namespace
{

//! Every renderable shape's name, in block order, duplicates included.
QStringList shapeNames( const NifModel * nif )
{
	QStringList names;
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		const QModelIndex i = nif->getBlockIndex( b );
		if ( !nif->blockInherits( i, "BSTriShape" ) && !nif->blockInherits( i, "NiTriBasedGeom" ) )
			continue;
		const QString name = nif->get<QString>( i, "Name" );
		if ( !name.isEmpty() )
			names << name;
	}
	return names;
}

//! Slots a file asks to be attached at: a "C-Muzzle" point name yields "Muzzle".
/*! BSConnectPoint::Children is the part's own statement of where it belongs. It
 *  is read, never written, and never checked against a list of parts. */
QStringList declaredSlots( const NifModel * nif )
{
	QStringList asked;			// not "slots": Qt takes that word
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		const QModelIndex i = nif->getBlockIndex( b );
		if ( !nif->isNiBlock( i, "BSConnectPoint::Children" ) )
			continue;
		const QModelIndex iNames = nif->getIndex( i, "Point Name" );
		// an invalid index is the model ROOT, whose row count is the block count
		if ( !iNames.isValid() )
			continue;
		for ( int r = 0; r < nif->rowCount( iNames ); r++ ) {
			const QString point = nif->get<QString>( nif->getIndex( iNames, r ) );
			if ( point.size() > 2 && point.startsWith( QLatin1String( "C-" ), Qt::CaseInsensitive )
				 && !asked.contains( point.mid( 2 ) ) )
				asked << point.mid( 2 );
		}
	}
	return asked;
}

//! Does the file offer ANY connect points at all?
/*! The question that separates "this part is the first thing on the node" from
 *  "this part is missing the piece that would hold it". A rig has no connect
 *  points; a weapon merged onto one brings its own along, and from then on an
 *  unmatched slot means something is genuinely absent. Without this, the BASE
 *  weapon NIF — which declares C-Receiver like every other part does — is warned
 *  about for landing exactly where it belongs. */
bool offersConnectPoints( const NifModel * nif )
{
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		const QModelIndex i = nif->getBlockIndex( b );
		if ( !nif->isNiBlock( i, "BSConnectPoint::Parents" ) )
			continue;
		const QModelIndex iPoints = nif->getIndex( i, "Connect Points" );
		if ( iPoints.isValid() && nif->rowCount( iPoints ) > 0 )
			return true;
	}
	return false;
}

//! One connect point the assembly currently publishes.
struct OfferedPoint
{
	QString name;			//!< "P-Muzzle", as the file spells it
	QString slot;			//!< "Muzzle"
	int frameBlock = -1;	//!< the node the point rides
	Transform xform;		//!< the point's own transform, in that node's frame
};

/*! Every connect point in \a target, in block order.
 *
 *  `Parent` names a node inside the mesh that published the point. When it is
 *  EMPTY the point is expressed in that mesh's ROOT frame, and the block that
 *  OWNS the BSConnectPoint::Parents extra data is exactly that root — which is
 *  the whole reason a placed part is given a wrapper node of its own. Without
 *  one, an empty-Parent point has no frame in the assembly and a two-hop chain
 *  (receiver publishes P-Barrel, the barrel publishes P-Muzzle, the suppressor
 *  asks for it) cannot be walked at all. Measured: 10mmLongBarrel.nif writes
 *  P-Muzzle with an empty Parent.
 */
QList<OfferedPoint> offeredPoints( const NifModel * target )
{
	QList<OfferedPoint> points;
	const QHash<QString, int> byName = namedNodes( target );
	const QHash<int, QList<int>> parents = parentMap( target );
	for ( int b = 0; b < target->getBlockCount(); b++ ) {
		const QModelIndex i = target->getBlockIndex( b );
		if ( !target->isNiBlock( i, "BSConnectPoint::Parents" ) )
			continue;
		const QModelIndex iPoints = target->getIndex( i, "Connect Points" );
		if ( !iPoints.isValid() )
			continue;
		// extra data is a child link, so the owning node is just this block's parent
		const int owner = parentOf( parents, b );
		for ( int r = 0; r < target->rowCount( iPoints ); r++ ) {
			const QModelIndex iPoint = target->getIndex( iPoints, r );
			OfferedPoint p;
			p.name = target->get<QString>( iPoint, "Name" );
			if ( p.name.size() <= 2
				 || !p.name.startsWith( QLatin1String( "P-" ), Qt::CaseInsensitive ) )
				continue;
			p.slot = p.name.mid( 2 );
			const QString rides = target->get<QString>( iPoint, "Parent" );
			p.frameBlock = rides.isEmpty() ? owner : byName.value( rides, owner );
			p.xform.translation = target->get<Vector3>( iPoint, "Translation" );
			/* THE ROTATION IS NOT OPTIONAL. It is identity on 96% of vanilla
			 * connect points, which is exactly why a translation-only assembler
			 * looks right until it meets a magazine: the 10mm's P-Mag is canted
			 * 26.5 degrees, and three muzzle points are turned a full 180. */
			p.xform.rotation.fromQuat( target->get<Quat>( iPoint, "Rotation" ) );
			const float s = target->get<float>( iPoint, "Scale" );
			p.xform.scale = ( s > 0.0f ) ? s : 1.0f;
			points.append( p );
		}
	}
	return points;
}

//! The point \a donor should be placed on, if the assembly publishes one.
/*! A mesh may declare several required points — vanilla's CombatRifle.nif asks
 *  for both `C-Receiver` and the misspelled `C-Reciever` — and the list means ANY
 *  of these, not all. Where two placed parts publish the same point the LAST one
 *  wins, which is the deepest in the chain and what the engine ends up showing. */
bool resolveWeaponPoint( const NifModel * target, const NifModel * donor,
                         OfferedPoint & out, QStringList & declaredOut )
{
	declaredOut = declaredSlots( donor );
	const QList<OfferedPoint> offered = offeredPoints( target );
	for ( const QString & slot : std::as_const( declaredOut ) ) {
		int found = -1;
		for ( int i = 0; i < offered.size(); i++ )
			if ( offered.at( i ).slot.compare( slot, Qt::CaseInsensitive ) == 0 )
				found = i;
		if ( found >= 0 ) {
			out = offered.at( found );
			return true;
		}
	}
	return false;
}

//! A node name neither \a target nor the part \a donor is about to bring is using.
/*! The donor side matters as much as the target side. HuntingRifleReceiver.nif is
 *  the case: the file is called that AND carries an inner node of the same name,
 *  which its own P-Grip point rides. Naming the wrapper after the file would put
 *  two nodes of that name in the assembly and leave the point's Parent lookup
 *  picking whichever came first — right by luck here, wrong the moment the two
 *  frames differ. The donor's ROOT name is not counted, because the root is a
 *  per-file wrapper the merge drops. */
QString freeNodeName( const NifModel * target, const NifModel * donor, const QString & wanted )
{
	QSet<QString> taken;
	const QHash<QString, int> byName = namedNodes( target );
	for ( auto it = byName.constBegin(); it != byName.constEnd(); ++it )
		taken.insert( it.key() );
	if ( donor ) {
		const QList<int> roots = donor->getRootLinks();
		for ( int b = 0; b < donor->getBlockCount(); b++ ) {
			if ( roots.contains( b ) )
				continue;
			const QModelIndex i = donor->getBlockIndex( b );
			if ( donor->isNiBlock( i, "NiNode" ) )
				taken.insert( donor->get<QString>( i, "Name" ) );
		}
	}
	const QString base = wanted.isEmpty() ? QStringLiteral( "WeaponPart" ) : wanted;
	if ( !taken.contains( base ) )
		return base;
	for ( int n = 1; n < 1000; n++ ) {
		const QString candidate = QStringLiteral( "%1_%2" ).arg( base ).arg( n, 2, 10, QLatin1Char( '0' ) );
		if ( !taken.contains( candidate ) )
			return candidate;
	}
	return base;
}

/*! The point at the very END of the assembled barrel chain, for a part that
 *  declares no connect point of its own.
 *
 *  A muzzle flash is the case this exists for. MiniGunMuzzeFlash.nif carries no
 *  BSConnectPoint::Children AND no ::Parents — measured — so there is nothing for
 *  the ordinary C-/P- match to work with, and hanging it on the WEAPON bone puts
 *  the fireball at the shooter's fist. Where it belongs is one node past
 *  everything else on the barrel: receiver > barrel > muzzle device > flash, and
 *  with no muzzle device, receiver > barrel > flash.
 *
 *  The ladder is the chain read backwards, and every rung is a point some placed
 *  mesh actually published — nothing is invented, and when nothing is published
 *  the caller falls back to the bone and says so:
 *
 *    1. P-ProjectileNode — where the round leaves. A muzzle device publishes one
 *       (10mmSuppressor.nif and HuntingRifleSilencer.nif both do), a barrel
 *       publishes one, and a bare receiver publishes one for its baked-in barrel.
 *       That is exactly "as deep as the chain currently goes", so several
 *       candidates are normal and the FARTHEST from the gun's own origin wins.
 *    2. P-Muzzle, for a chain whose end publishes no projectile node.
 *    3. The P-Flash family. The Minigun ships P-FlashShort / P-FlashMid /
 *       P-FlashFar for its barrel lengths and no muzzle point at all; farthest
 *       along the bore wins there too.
 *    4. P-Barrel — the last thing that is still on the barrel line.
 *
 *  Distance from the gun's origin rather than a Y coordinate, because the target
 *  may be a posed rig where the gun points anywhere. On a weapon the grip is the
 *  origin and the barrel is the far end, so "farthest" IS "furthest forward".
 */
bool resolveChainEnd( const NifModel * target, OfferedPoint & out )
{
	const QList<OfferedPoint> offered = offeredPoints( target );
	if ( offered.isEmpty() )
		return false;
	const QHash<int, QList<int>> parents = parentMap( target );
	const QHash<int, Transform> world = worldTransforms( target, parents );

	// the gun's own origin: its WEAPON bone if the assembly has one, else the root
	Vector3 anchor;
	const QHash<QString, int> byName = namedNodes( target );
	if ( int bone = byName.value( QStringLiteral( "WEAPON" ), -1 ); bone >= 0 )
		anchor = world.value( bone ).translation;
	else if ( const QList<int> roots = target->getRootLinks(); !roots.isEmpty() )
		anchor = world.value( roots.first() ).translation;

	auto tierOf = []( const QString & slot ) {
		if ( slot.compare( QLatin1String( "ProjectileNode" ), Qt::CaseInsensitive ) == 0 )
			return 0;
		if ( slot.compare( QLatin1String( "Muzzle" ), Qt::CaseInsensitive ) == 0 )
			return 1;
		if ( slot.startsWith( QLatin1String( "Flash" ), Qt::CaseInsensitive ) )
			return 2;
		if ( slot.compare( QLatin1String( "Barrel" ), Qt::CaseInsensitive ) == 0 )
			return 3;
		return 99;
	};

	int bestTier = 99;
	float bestReach = -1;
	bool found = false;
	for ( const OfferedPoint & point : offered ) {
		const int tier = tierOf( point.slot );
		if ( tier == 99 )
			continue;
		const Transform at = world.value( point.frameBlock ) * point.xform;
		const float reach = ( at.translation - anchor ).length();
		if ( tier > bestTier || ( tier == bestTier && reach <= bestReach ) )
			continue;
		bestTier = tier;
		bestReach = reach;
		out = point;
		found = true;
	}
	return found;
}

//! Is this one of the slots that means "I am the gun", misspelling included?
/*! Vanilla ships `C-Reciever` alongside `C-Receiver` in several files. Both are
 *  matched here, and neither is a whitelist of parts: it is one slot NAME. */
bool isReceiverSlot( const QString & slot )
{
	return slot.compare( QLatin1String( "Receiver" ), Qt::CaseInsensitive ) == 0
		|| slot.compare( QLatin1String( "Reciever" ), Qt::CaseInsensitive ) == 0;
}

} // namespace

bool nifMergeWeaponPart( NifModel * target, const QByteArray & data, const QString & label,
                         NifMergeResult & result, NifWeaponPlacement & placement )
{
	if ( !target ) {
		result.error = QStringLiteral( "no target model" );
		return false;
	}
	NifModel donor;
	QByteArray bytes = data;					// load() consumes a QIODevice
	QBuffer device( &bytes );
	if ( !device.open( QIODevice::ReadOnly ) || !donor.load( device ) ) {
		result.error = QStringLiteral( "could not parse %1" )
			.arg( label.isEmpty() ? QStringLiteral( "the NIF" ) : label );
		return false;
	}
	const QString name = label.isEmpty() ? QStringLiteral( "NIF" ) : label;

	/* --- redundancy -----------------------------------------------------------
	 * The merge shares NiNodes by name but imports shapes whatever they are
	 * called, so a shape name already present means that geometry is arriving
	 * twice. Reported, never refused: two of something is a thing people do on
	 * purpose, and parts may come from any weapon at all.
	 */
	const QStringList bring = shapeNames( &donor );
	const QStringList already = shapeNames( target );
	const QSet<QString> have( already.cbegin(), already.cend() );
	QStringList collided;
	for ( const QString & shape : bring )
		if ( have.contains( shape ) && !collided.contains( shape ) )
			collided << shape;
	if ( !collided.isEmpty() )
		placement.notes << QObject::tr( "%1: %2 of its %3 shape name(s) are already in the target "
			"(%4). It may be redundant, or a second one of a part the weapon already has. "
			"Merged anyway." )
			.arg( name ).arg( collided.size() ).arg( bring.size() )
			.arg( collided.mid( 0, 6 ).join( QStringLiteral( ", " ) ) );

	/* --- where it goes --------------------------------------------------------
	 * One rule: a part asking for C-X is placed on whatever already-placed part
	 * publishes P-X, at that point's own transform. No table of weapons, no legal
	 * combinations — if the names line up across two different guns, it assembles.
	 */
	OfferedPoint point;
	bool resolved = resolveWeaponPoint( target, &donor, point, placement.declared );
	/* A PART THAT ASKS FOR NOTHING GOES AT THE END OF THE BARREL.
	 *
	 * Declaring no connect point is the muzzle-flash signature — the flash meshes
	 * carry neither ::Children nor ::Parents, so there is no name to match on —
	 * and a flash belongs one node past everything else on the barrel line rather
	 * than on the bone the gun hangs from. See resolveChainEnd for the ladder.
	 *
	 * It also catches a whole SECOND gun merged onto an assembled one, which has
	 * no connect point to ask for either. That is a strange thing to do and the
	 * summary says exactly where it went; on a rig — which publishes nothing —
	 * both cases fall through to the WEAPON bone as before.
	 */
	if ( !resolved && placement.declared.isEmpty() && resolveChainEnd( target, point ) ) {
		resolved = true;
		placement.chainEnd = true;
	}

	WeaponAttach attach;
	attach.wrapperName = freeNodeName( target, &donor, QFileInfo( name ).completeBaseName() );

	if ( resolved && point.frameBlock >= 0 ) {
		attach.attachBlock = point.frameBlock;
		attach.xform = point.xform;
		placement.placed = true;
		placement.slot = point.slot;
		placement.point = point.name;
		placement.provider = target->get<QString>(
			target->getBlockIndex( point.frameBlock ), "Name" );
	} else {
		/* Nothing publishes what it asks for. Do NOT invent a placement — hang it
		 * on the weapon bone, which is where a gun goes, and say what is missing.
		 *
		 * Two cases are silent because nothing IS missing: a part that declares no
		 * connect point at all is an assembly root (Minigun.nif), and a receiver
		 * arriving at a target that publishes no points at all has landed on a RIG
		 * rather than on a half-built gun — it is the thing that defines the frame
		 * everything else lands in.
		 */
		const QString bone = nifWeaponAttachNode( target );
		const QHash<QString, int> byName = namedNodes( target );
		const QList<int> roots = target->getRootLinks();
		attach.attachBlock = bone.isEmpty() ? ( roots.isEmpty() ? -1 : roots.first() )
		                                    : byName.value( bone, -1 );
		if ( attach.attachBlock < 0 ) {
			result.error = QStringLiteral( "target has no root block" );
			return false;
		}
		const bool rig = !offersConnectPoints( target );
		bool receiverOnRig = false;
		for ( const QString & slot : std::as_const( placement.declared ) )
			if ( rig && isReceiverSlot( slot ) )
				receiverOnRig = true;
		if ( bone.isEmpty() )
			placement.notes << QObject::tr( "%1 is marked as a weapon part, but the target has no "
				"WEAPON bone — merged at root." ).arg( name );
		else if ( !placement.declared.isEmpty() && !receiverOnRig )
			placement.notes << QObject::tr( "%1 asks to attach at a %2 connect point, and nothing "
				"in the assembly publishes P-%2. Merge the part that provides it first — for a "
				"muzzle device that is usually the barrel — and this one after it. For now it "
				"hangs on %3 at the weapon's origin." )
				.arg( name ).arg( placement.declared.first() ).arg( bone );
	}

	const bool merged = mergeDonor( target, donor, name, true, result, QString(), &attach );
	placement.attachedTo = result.attachedTo;
	placement.wrapper = attach.wrapperName;
	return merged;
}

static QString wwLastMergeSummary;

void nifSetLastMergeSummary( const QString & text )
{
	wwLastMergeSummary = text;
}

QString nifLastMergeSummary()
{
	return wwLastMergeSummary;
}
