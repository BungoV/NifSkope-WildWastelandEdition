/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "animgraph.h"

#include "model/nifmodel.h"

#include <QHash>
#include <QSet>

#include <algorithm>

namespace
{

QList<int> blocksOfType( const NifModel * nif, const char * type )
{
	QList<int> out;
	for ( int b = 0; b < nif->getBlockCount(); b++ )
		if ( nif->blockInherits( nif->getBlockIndex( b ), type ) )
			out.append( b );
	return out;
}

//! The NiMultiTargetTransformController in a controller's Next Controller chain.
/*! It is a sibling of the manager, not a child of it: the root's chain reads
 *  manager -> multi-target -> whatever else, and the sequences reach the
 *  multi-target through their controlled blocks. */
int multiTargetIn( const NifModel * nif, int controller )
{
	QSet<int> seen;
	for ( int c = controller; c >= 0 && !seen.contains( c ); ) {
		seen << c;
		QModelIndex idx = nif->getBlockIndex( c );
		if ( c != controller && nif->blockInherits( idx, "NiMultiTargetTransformController" ) )
			return c;
		c = nif->getLink( idx, "Next Controller" );
	}
	return -1;
}

//! Every block whose Controller chain contains \a block, and where in it.
int ownerOfController( const NifModel * nif, int block )
{
	for ( int b = 0; b < nif->getBlockCount(); b++ ) {
		QModelIndex idx = nif->getBlockIndex( b );
		if ( !nif->blockInherits( idx, "NiObjectNET" ) )
			continue;
		QSet<int> seen;
		for ( int c = nif->getLink( idx, "Controller" ); c >= 0 && !seen.contains( c );
		      c = nif->getLink( nif->getBlockIndex( c ), "Next Controller" ) ) {
			seen << c;
			if ( c == block )
				return b;
		}
	}
	return -1;
}

//! Copy one Controlled Blocks row. Strings go through the model's own table, so
//! "no string" (-1) survives as -1 rather than becoming an empty string.
void copyControlledBlock( NifModel * nif, const QModelIndex & iDst, const QModelIndex & iSrc )
{
	for ( const char * f : { "Interpolator", "Controller" } ) {
		QModelIndex iF = nif->getIndex( iDst, f );
		if ( iF.isValid() )
			nif->setLink( iF, nif->getLink( nif->getIndex( iSrc, f ) ) );
	}
	if ( nif->getIndex( iDst, "Priority" ).isValid() )
		nif->set<int>( iDst, "Priority", nif->get<int>( iSrc, "Priority" ) );
	for ( const char * f : { "Node Name", "Property Type", "Controller Type",
	                         "Controller ID", "Interpolator ID" } ) {
		QModelIndex iF = nif->getIndex( iDst, f );
		QModelIndex iS = nif->getIndex( iSrc, f );
		if ( iF.isValid() && iS.isValid() )
			nif->assignString( iF, nif->resolveString( iS ), false );
	}
}

//! Append a link to a "Num <array>" / "<array>" pair, growing it by one.
void appendToArray( NifModel * nif, const QModelIndex & iBlock, const char * array,
                    const char * count, int link )
{
	QModelIndex iArr = nif->getIndex( iBlock, array );
	if ( !iArr.isValid() )
		return;
	const int n = nif->get<int>( iBlock, count );
	nif->set<int>( iBlock, count, n + 1 );
	nif->updateArraySize( iArr );
	nif->setLink( nif->getIndex( iArr, n ), link );
}

//! Rewrite a "Num <array>" / "<array>" link pair to exactly \a keep.
void rewriteLinkArray( NifModel * nif, const QModelIndex & iBlock, const char * array,
                       const char * count, const QList<qint32> & keep )
{
	QModelIndex iArr = nif->getIndex( iBlock, array );
	if ( !iArr.isValid() )
		return;
	nif->set<int>( iBlock, count, keep.size() );
	nif->updateArraySize( iArr );
	for ( int i = 0; i < keep.size(); i++ )
		nif->setLink( nif->getIndex( iArr, i ), keep.at( i ) );
}

} // namespace

AnimGraphResult consolidateControllerManagers( NifModel * nif )
{
	AnimGraphResult res;
	if ( !nif )
		return res;

	const QList<int> managers = blocksOfType( nif, "NiControllerManager" );
	if ( managers.size() < 2 )
		return res;

	const QList<int> roots = nif->getRootLinks();
	if ( roots.isEmpty() )
		return res;
	const int root = roots.first();

	/* The survivor is a manager already on the root if there is one — that is
	 * where every vanilla file puts it, and it is the one anything looking for
	 * "the" manager will find. Otherwise the first is adopted and moved there. */
	int keeper = -1;
	for ( const int m : managers ) {
		if ( nif->getLink( nif->getBlockIndex( m ), "Target" ) == root ) {
			keeper = m;
			break;
		}
	}
	if ( keeper < 0 )
		keeper = managers.first();

	QModelIndex iKeeper = nif->getBlockIndex( keeper );
	int keeperPalette = nif->getLink( iKeeper, "Object Palette" );
	int keeperMulti   = multiTargetIn( nif, keeper );

	QSet<int> doomed;

	// The sequences the survivor already owns, by name.
	QHash<QString, int> keeperSeqs;
	{
		QModelIndex iArr = nif->getIndex( iKeeper, "Controller Sequences" );
		for ( int r = 0; r < nif->rowCount( iArr ); r++ ) {
			const int s = nif->getLink( nif->getIndex( iArr, r ) );
			if ( s >= 0 )
				keeperSeqs.insert( nif->get<QString>( nif->getBlockIndex( s ), "Name" ), s );
		}
	}

	// Palette entries already present, by name.
	QSet<QString> paletteNames;
	if ( keeperPalette >= 0 ) {
		QModelIndex iObjs = nif->getIndex( nif->getBlockIndex( keeperPalette ), "Objs" );
		for ( int r = 0; r < nif->rowCount( iObjs ); r++ )
			paletteNames << nif->resolveString( nif->getIndex( iObjs, r ), "Name" );
	}

	// Multi-target controllers that fold away, so the controlled blocks pointing
	// at them can be re-aimed at the survivor's.
	QHash<int, int> multiRemap;

	for ( const int m : managers ) {
		if ( m == keeper )
			continue;
		QModelIndex iM = nif->getBlockIndex( m );

		// --- the multi-target controller ---------------------------------
		const int mMulti = multiTargetIn( nif, m );
		if ( mMulti >= 0 ) {
			if ( keeperMulti < 0 ) {
				keeperMulti = mMulti;       // the survivor had none; adopt this one
			} else {
				QModelIndex iSrc = nif->getBlockIndex( mMulti );
				QModelIndex iDst = nif->getBlockIndex( keeperMulti );
				QList<qint32> targets;
				QModelIndex iArr = nif->getIndex( iDst, "Extra Targets" );
				for ( int r = 0; r < nif->rowCount( iArr ); r++ )
					targets.append( nif->getLink( nif->getIndex( iArr, r ) ) );
				// its own Target is a target too: a manager that sat on HEAD was
				// driving HEAD, and the survivor sits on the root instead.
				QList<qint32> incoming;
				const int ownTarget = nif->getLink( iSrc, "Target" );
				if ( ownTarget >= 0 && ownTarget != root )
					incoming.append( ownTarget );
				QModelIndex iSrcArr = nif->getIndex( iSrc, "Extra Targets" );
				for ( int r = 0; r < nif->rowCount( iSrcArr ); r++ )
					incoming.append( nif->getLink( nif->getIndex( iSrcArr, r ) ) );
				for ( const qint32 t : incoming ) {
					if ( t < 0 || targets.contains( t ) )
						continue;
					targets.append( t );
					res.extraTargets++;
				}
				rewriteLinkArray( nif, iDst, "Extra Targets", "Num Extra Targets", targets );
				multiRemap.insert( mMulti, keeperMulti );
				doomed << mMulti;
			}
		}

		// --- the object palette -------------------------------------------
		const int mPalette = nif->getLink( iM, "Object Palette" );
		if ( mPalette >= 0 ) {
			if ( keeperPalette < 0 ) {
				keeperPalette = mPalette;
				nif->setLink( iKeeper, "Object Palette", mPalette );
			} else {
				QModelIndex iDst = nif->getBlockIndex( keeperPalette );
				QModelIndex iSrcObjs = nif->getIndex( nif->getBlockIndex( mPalette ), "Objs" );
				QModelIndex iDstObjs = nif->getIndex( iDst, "Objs" );
				for ( int r = 0; r < nif->rowCount( iSrcObjs ); r++ ) {
					QModelIndex iRow = nif->getIndex( iSrcObjs, r );
					const QString name = nif->resolveString( iRow, "Name" );
					const int av = nif->getLink( iRow, "AV Object" );
					if ( name.isEmpty() || av < 0 || paletteNames.contains( name ) )
						continue;
					const int n = nif->get<int>( iDst, "Num Objs" );
					nif->set<int>( iDst, "Num Objs", n + 1 );
					nif->updateArraySize( iDstObjs );
					QModelIndex iNew = nif->getIndex( iDstObjs, n );
					nif->assignString( iNew, QStringLiteral( "Name" ), name, false );
					nif->setLink( iNew, "AV Object", av );
					paletteNames << name;
					res.paletteEntries++;
				}
				doomed << mPalette;
			}
		}

		// --- the sequences --------------------------------------------------
		QModelIndex iSeqArr = nif->getIndex( iM, "Controller Sequences" );
		for ( int r = 0; r < nif->rowCount( iSeqArr ); r++ ) {
			const int s = nif->getLink( nif->getIndex( iSeqArr, r ) );
			if ( s < 0 )
				continue;
			QModelIndex iS = nif->getBlockIndex( s );
			const QString name = nif->get<QString>( iS, "Name" );
			auto it = keeperSeqs.constFind( name );

			if ( it == keeperSeqs.constEnd() ) {
				// A name the survivor does not have yet: re-register it as it is.
				appendToArray( nif, iKeeper, "Controller Sequences",
				               "Num Controller Sequences", s );
				nif->setLink( iS, "Manager", keeper );
				keeperSeqs.insert( name, s );
				res.sequencesMoved++;
				continue;
			}

			// Same name: fuse, because whatever plays "autoPlay" has to drive
			// every limb's effects and can only ask for one sequence.
			QModelIndex iInto = nif->getBlockIndex( *it );
			QModelIndex iIntoArr = nif->getIndex( iInto, "Controlled Blocks" );
			QModelIndex iFromArr = nif->getIndex( iS, "Controlled Blocks" );
			const int have = nif->get<int>( iInto, "Num Controlled Blocks" );
			const int add  = nif->rowCount( iFromArr );
			if ( add > 0 ) {
				nif->set<int>( iInto, "Num Controlled Blocks", have + add );
				nif->updateArraySize( iIntoArr );
				for ( int i = 0; i < add; i++ )
					copyControlledBlock( nif, nif->getIndex( iIntoArr, have + i ),
					                     nif->getIndex( iFromArr, i ) );
			}
			nif->set<float>( iInto, "Stop Time",
				std::max( nif->get<float>( iInto, "Stop Time" ), nif->get<float>( iS, "Stop Time" ) ) );
			nif->set<float>( iInto, "Start Time",
				std::min( nif->get<float>( iInto, "Start Time" ), nif->get<float>( iS, "Start Time" ) ) );
			const int text = nif->getLink( iS, "Text Keys" );
			if ( text >= 0 && nif->getLink( iInto, "Text Keys" ) >= 0 )
				doomed << text;            // one set of annotations is enough
			doomed << s;
			res.sequencesFused++;
		}

		doomed << m;
		res.managersFolded++;
	}

	/* Move the survivor onto the root if it was not there. Its Target is what the
	 * engine reads as "the scene this manages", and a manager on HEAD manages the
	 * helmet, not the figure. */
	if ( nif->getLink( iKeeper, "Target" ) != root ) {
		nif->setLink( iKeeper, "Target", root );
		if ( keeperMulti >= 0 )
			nif->setLink( nif->getBlockIndex( keeperMulti ), "Target", root );
	}
	if ( keeperPalette >= 0 )
		nif->setLink( nif->getBlockIndex( keeperPalette ), "Scene", root );

	// Controlled blocks that named a folded multi-target controller.
	if ( !multiRemap.isEmpty() ) {
		for ( const int s : blocksOfType( nif, "NiControllerSequence" ) ) {
			QModelIndex iArr = nif->getIndex( nif->getBlockIndex( s ), "Controlled Blocks" );
			for ( int r = 0; r < nif->rowCount( iArr ); r++ ) {
				QModelIndex iC = nif->getIndex( nif->getIndex( iArr, r ), "Controller" );
				auto it = multiRemap.constFind( nif->getLink( iC ) );
				if ( it != multiRemap.constEnd() )
					nif->setLink( iC, *it );
			}
		}
	}

	/* Re-splice every controller chain the fold emptied. Removing a block rewrites
	 * links to it as -1, which would cut the chain at that point and orphan every
	 * controller after it — including ones that have nothing to do with the fold. */
	QList<int> chainOwners;
	for ( int b = 0; b < nif->getBlockCount(); b++ )
		if ( nif->getIndex( nif->getBlockIndex( b ), "Controller" ).isValid() )
			chainOwners.append( b );

	for ( const int owner : chainOwners ) {
		QModelIndex iOwner = nif->getBlockIndex( owner );
		QList<int> chain;
		QSet<int> seen;
		for ( int c = nif->getLink( iOwner, "Controller" ); c >= 0 && !seen.contains( c );
		      c = nif->getLink( nif->getBlockIndex( c ), "Next Controller" ) ) {
			seen << c;
			chain.append( c );
		}
		QList<int> keep;
		for ( const int c : chain )
			if ( !doomed.contains( c ) )
				keep.append( c );
		if ( keep == chain )
			continue;
		nif->setLink( iOwner, "Controller", keep.value( 0, -1 ) );
		for ( int i = 0; i < keep.size(); i++ )
			nif->setLink( nif->getBlockIndex( keep.at( i ) ), "Next Controller",
			              keep.value( i + 1, -1 ) );
	}

	// ...and put the survivor at the head of the root's chain if the fold, or the
	// file it came from, left it hanging off some limb.
	if ( ownerOfController( nif, keeper ) != root ) {
		if ( const int oldOwner = ownerOfController( nif, keeper ); oldOwner >= 0 ) {
			QModelIndex iOld = nif->getBlockIndex( oldOwner );
			QList<int> chain;
			QSet<int> seen;
			for ( int c = nif->getLink( iOld, "Controller" ); c >= 0 && !seen.contains( c );
			      c = nif->getLink( nif->getBlockIndex( c ), "Next Controller" ) ) {
				seen << c;
				chain.append( c );
			}
			QList<int> keep;
			for ( const int c : chain )
				if ( c != keeper && c != keeperMulti )
					keep.append( c );
			nif->setLink( iOld, "Controller", keep.value( 0, -1 ) );
			for ( int i = 0; i < keep.size(); i++ )
				nif->setLink( nif->getBlockIndex( keep.at( i ) ), "Next Controller",
				              keep.value( i + 1, -1 ) );
		}
		QModelIndex iRoot = nif->getBlockIndex( root );
		const int wasFirst = nif->getLink( iRoot, "Controller" );
		nif->setLink( iRoot, "Controller", keeper );
		if ( keeperMulti >= 0 ) {
			nif->setLink( nif->getBlockIndex( keeper ), "Next Controller", keeperMulti );
			nif->setLink( nif->getBlockIndex( keeperMulti ), "Next Controller", wasFirst );
		} else {
			nif->setLink( nif->getBlockIndex( keeper ), "Next Controller", wasFirst );
		}
	}

	// The survivor's own sequence list, with any hole the fold left compacted out.
	{
		QModelIndex iArr = nif->getIndex( iKeeper, "Controller Sequences" );
		QList<qint32> keep;
		for ( int r = 0; r < nif->rowCount( iArr ); r++ ) {
			const qint32 s = nif->getLink( nif->getIndex( iArr, r ) );
			if ( s >= 0 && !doomed.contains( s ) && !keep.contains( s ) )
				keep.append( s );
		}
		rewriteLinkArray( nif, iKeeper, "Controller Sequences", "Num Controller Sequences", keep );
		for ( const qint32 s : keep )
			res.sequenceNames << nif->get<QString>( nif->getBlockIndex( s ), "Name" );
	}

	// Descending, so a removal cannot renumber a block still to be removed.
	QList<int> order = doomed.values();
	std::sort( order.begin(), order.end(), std::greater<int>() );
	for ( const int b : std::as_const( order ) )
		nif->removeNiBlock( b );
	res.blocksRemoved = order.size();

	return res;
}

int pruneDeadAnimLinks( NifModel * nif )
{
	if ( !nif )
		return 0;
	int dropped = 0;

	for ( const int b : blocksOfType( nif, "NiDefaultAVObjectPalette" ) ) {
		QModelIndex iPal = nif->getBlockIndex( b );
		QModelIndex iObjs = nif->getIndex( iPal, "Objs" );
		QList<QPair<QString, qint32>> keep;
		for ( int r = 0; r < nif->rowCount( iObjs ); r++ ) {
			QModelIndex iRow = nif->getIndex( iObjs, r );
			const qint32 av = nif->getLink( iRow, "AV Object" );
			if ( av >= 0 )
				keep.append( { nif->resolveString( iRow, "Name" ), av } );
		}
		if ( keep.size() == nif->rowCount( iObjs ) )
			continue;
		dropped += nif->rowCount( iObjs ) - keep.size();
		nif->set<int>( iPal, "Num Objs", keep.size() );
		nif->updateArraySize( iObjs );
		for ( int r = 0; r < keep.size(); r++ ) {
			QModelIndex iRow = nif->getIndex( iObjs, r );
			nif->assignString( iRow, QStringLiteral( "Name" ), keep.at( r ).first, false );
			nif->setLink( iRow, "AV Object", keep.at( r ).second );
		}
	}

	for ( const int b : blocksOfType( nif, "NiMultiTargetTransformController" ) ) {
		QModelIndex iMulti = nif->getBlockIndex( b );
		QModelIndex iArr = nif->getIndex( iMulti, "Extra Targets" );
		QList<qint32> keep;
		for ( int r = 0; r < nif->rowCount( iArr ); r++ ) {
			const qint32 t = nif->getLink( nif->getIndex( iArr, r ) );
			if ( t >= 0 )
				keep.append( t );
		}
		if ( keep.size() == nif->rowCount( iArr ) )
			continue;
		dropped += nif->rowCount( iArr ) - keep.size();
		rewriteLinkArray( nif, iMulti, "Extra Targets", "Num Extra Targets", keep );
	}

	return dropped;
}
