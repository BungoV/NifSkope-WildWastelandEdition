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

	// Serialize the blocks to import.
	QByteArray blob;
	QBuffer writeBuf( &blob );
	writeBuf.open( QIODevice::WriteOnly );
	QDataStream out( &writeBuf );
	out << int( toImport.size() );
	for ( qint32 b : toImport ) {
		QModelIndex iB = donor.getBlockIndex( b );
		out << donor.itemName( iB );
		donor.saveIndex( writeBuf, iB );
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
