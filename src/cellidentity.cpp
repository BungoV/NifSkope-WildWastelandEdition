/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellidentity.h"

#include "lodifile.h"

#include <QFileInfo>


bool cellIdentityLoad( const QString & lodiPath, QHash<quint32, CellIdentity> & index,
	CellIdentityCensus * census, QString * error )
{
	auto fail = [error]( const QString & m ) {
		if ( error )
			*error = m;
		return false;
	};

	index.clear();
	CellIdentityCensus c;
	c.path = lodiPath;

	if ( lodiPath.isEmpty() )
		return fail( QStringLiteral( "no `.lodi` path was given" ) );
	if ( !QFileInfo::exists( lodiPath ) )
		return fail( QString( "no such file: %1" ).arg( lodiPath ) );

	LodiHeader h;
	LodiTable t;
	QString readError;
	// payloadCheck = true: the CRCs and range partitions, so a corrupted file never draws a picture
	if ( !lodiRead( lodiPath, &h, &t, true, &readError ) )
		return fail( QString( "%1: %2" ).arg( QFileInfo( lodiPath ).fileName(), readError ) );

	c.version = h.version;
	c.instances = quint32( t.instances.size() );
	c.hasGroups = ( h.version >= LODI_VERSION_GROUP_SKY )
		&& ( t.group.size() == t.instances.size() )
		&& !t.group.empty();

	if ( t.cold.size() != t.instances.size() )
		return fail( QString( "the cold blob has %1 rows and the instance blob has %2; "
			"they are parallel by contract (src/lodifile.h, LodiCold)" )
			.arg( t.cold.size() ).arg( t.instances.size() ) );

	if ( !c.hasGroups ) {
		// not a failure: a pre-v7 file is valid; the caller must name the version, not draw all grey
		if ( census )
			*census = c;
		return true;
	}

	QHash<quint32, int> groupSize;
	QHash<quint32, quint32> firstOf;    // refForm -> file-wide group
	QHash<quint32, bool> treeOf;
	QHash<quint32, int> partsOf;

	for ( size_t ci = 0; ci < t.chunks.size(); ci++ ) {
		const LodiChunk & ch = t.chunks[ci];
		if ( !ch.instanceCount )
			continue;
		const quint64 first = ch.instanceFirst;
		const quint64 last = quint64( ch.instanceFirst ) + ch.instanceCount;
		if ( last > quint64( t.instances.size() ) )
			return fail( QString( "chunk %1 names instances %2..%3 and the blob holds %4" )
				.arg( ci ).arg( first ).arg( last ).arg( t.instances.size() ) );
		/* `LodiTable::group` is a file-wide u32 for every version (the reader offsets older per-chunk
		 * ids), so the id is the key and a group cut by a chunk line is one key */
		for ( quint64 i = first; i < last; i++ ) {
			const size_t s = size_t( i );
			const quint32 key = t.group[s];
			groupSize[key]++;
			const quint32 form = t.cold[s].refFormId;
			if ( !form )
				continue;   // NOLIB / synthetic rows carry no REFR to join to
			const bool tree = t.instances[s].seed != 0;
			if ( !firstOf.contains( form ) ) {
				firstOf.insert( form, key );
				treeOf.insert( form, tree );
				partsOf.insert( form, 1 );
			} else {
				partsOf[form]++;
				if ( tree )
					treeOf[form] = true;
			}
			if ( tree )
				c.trees++;
		}
	}

	for ( auto it = firstOf.constBegin(); it != firstOf.constEnd(); ++it ) {
		CellIdentity id;
		id.group = it.value();
		id.size = groupSize.value( it.value(), 0 );
		id.tree = treeOf.value( it.key(), false );
		id.parts = partsOf.value( it.key(), 1 );
		index.insert( it.key(), id );
	}

	c.refs = quint32( index.size() );
	c.groups = quint32( groupSize.size() );
	for ( auto it = groupSize.constBegin(); it != groupSize.constEnd(); ++it ) {
		if ( it.value() == 1 )
			c.singletons++;
		c.largest = qMax( c.largest, quint32( it.value() ) );
	}
	for ( auto it = index.constBegin(); it != index.constEnd(); ++it ) {
		if ( it.value().tree && it.value().size == 1 )
			c.treeSingletons++;
	}

	if ( census )
		*census = c;
	return true;
}

QString cellIdentityLegend( const CellIdentityCensus & c )
{
	if ( !c.hasGroups ) {
		return QString( "%1 is version %2 and carries NO group table (the table arrived at "
			"version %3), so the identity overlay has nothing to colour by -- REFUSED by "
			"version, not drawn grey" )
			.arg( QFileInfo( c.path ).fileName() ).arg( c.version )
			.arg( LODI_VERSION_GROUP_SKY );
	}
	return QString( "%1 v%2: %L3 instances, %L4 references indexed, %L5 groups, "
		"%L6 of them singletons, biggest %L7; %L8 tree instances, %L9 of those alone "
		"in their group" )
		.arg( QFileInfo( c.path ).fileName() ).arg( c.version )
		.arg( c.instances ).arg( c.refs ).arg( c.groups ).arg( c.singletons )
		.arg( c.largest ).arg( c.trees ).arg( c.treeSingletons );
}
