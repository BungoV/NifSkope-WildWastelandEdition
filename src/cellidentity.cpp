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
	// payloadCheck = true: the CRCs and the range partitions. This reader is
	// about to key a PICTURE on the group table, and a file whose chunk CRC is
	// wrong would draw a picture of corrupted memory with no sign that it had.
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
		// Not a failure: a v6 file is a valid file. The caller says the version
		// out loud; it must not draw an all-grey picture and call it a result.
		if ( census )
			*census = c;
		return true;
	}

	/* The file-wide id. Group ids are dense from 0 INSIDE EACH CHUNK
	 * (src/lodifile.h, LODI_VERSION_GROUP_SKY), so the chunk index has to be in
	 * the key or two different objects share one colour. */
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
		/* THE GROUP ID CANNOT OVERFLOW ITS HALF OF THE KEY, and that is a fact
		 * about the FORMAT rather than a thing to test: `LodiTable::group` is a
		 * `quint16` per instance (src/lodifile.h), so the low 16 bits of the key
		 * always hold it exactly. A refusal here would be dead code -- the
		 * compiler says so (-Wtype-limits) -- so the statement is written down
		 * instead of measured, and the CHUNK index, which is a size_t and could
		 * overflow, is the one that is actually checked. */
		static_assert( sizeof( t.group[0] ) == 2, "a .lodi group id is 16 bits" );
		if ( ci > 0xFFFFu )
			return fail( QString( "chunk index %1 does not fit the 16 bits the file-wide "
				"key reserves for it (LODI_MAX_CHUNKS is %2)" ).arg( ci ).arg( LODI_MAX_CHUNKS ) );

		for ( quint64 i = first; i < last; i++ ) {
			const size_t s = size_t( i );
			const quint32 key = ( quint32( ci ) << 16 ) | quint32( t.group[s] );
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
