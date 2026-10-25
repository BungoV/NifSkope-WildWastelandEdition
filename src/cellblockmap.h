#ifndef CELLBLOCKMAP_H
#define CELLBLOCKMAP_H

#include <QHash>
#include <QSet>

#include <functional>
#include <utility>

/* lane LIVESET3: a cell document's blocks renumbered by a removal or move in the middle (a live set). Every side
 * table keyed by block number goes through one of these, so the document after a live set equals a fresh open's. */

//! old block number -> new one; -1 = the block is gone
using WwBlockMap = std::function<int( int )>;

//! blocks first..first+count-1 removed: later ones move down by count
inline WwBlockMap wwBlockMapRemove( int first, int count )
{
	return [first, count]( int b ) { return b < first ? b : b < first + count ? -1 : b - count; };
}

//! blocks first..first+count-1 moved to start at dst (< first): the ones between move up by count
inline WwBlockMap wwBlockMapMove( int first, int count, int dst )
{
	return [first, count, dst]( int b ) {
		if ( b < dst || b >= first + count )
			return b;
		return b >= first ? dst + ( b - first ) : b + count;
	};
}

template <class T>
void wwRemapKeys( QHash<int, T> & h, const WwBlockMap & m )
{
	QHash<int, T> o;
	o.reserve( h.size() );
	for ( auto it = h.begin(); it != h.end(); ++it ) {
		const int b = m( it.key() );
		if ( b >= 0 )
			o.insert( b, std::move( it.value() ) );
	}
	h = std::move( o );
}

inline void wwRemapKeys( QSet<int> & s, const WwBlockMap & m )
{
	QSet<int> o;
	o.reserve( s.size() );
	for ( const int k : s ) {
		const int b = m( k );
		if ( b >= 0 )
			o.insert( b );
	}
	s = std::move( o );
}

#endif // CELLBLOCKMAP_H
