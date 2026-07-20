/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
   used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENCE BLOCK *****/

#include "nifitem.h"
#include "model/basemodel.h"

#include <new>
#include <mutex>
#include <vector>
#include <memory>
#include <cstdlib>
#include <algorithm>
#ifdef _WIN32
#include <malloc.h>	// _aligned_malloc / _aligned_free
#endif

// ---- NifItem slab pool ----------------------------------------------------
// One general-purpose heap allocation per item dominated parse wall-time for
// high-poly meshes (~1.5M items / 38k-vert shape) and scattered the tree
// across the heap. Slots come from large chunks and recycle through an
// intrusive free list (the freed slot stores the next pointer). Chunks are
// never returned to the OS — edit churn reuses them via the free list, and
// the pool lives for the process. The mutex is uncontended outside the XML
// checker's worker threads.
namespace {

struct NifItemPool {
	static constexpr std::size_t slotAlign = alignof( std::max_align_t ) > 16
		? alignof( std::max_align_t ) : 16;	// FloatVector4 members need 16
	static constexpr std::size_t chunkSlotCount = 8192;

	std::mutex mutex;
	void * freeList = nullptr;
	std::vector<std::unique_ptr<char, decltype( &std::free )>> chunks;
	std::size_t slotSize = 0;

	void * allocate( std::size_t size )
	{
		std::lock_guard<std::mutex> lock( mutex );
		if ( !slotSize ) {
			slotSize = ( std::max( size, sizeof( void * ) ) + ( slotAlign - 1 ) )
				& ~( slotAlign - 1 );
		}
		// NifItem has no subclasses, so every request is sizeof(NifItem);
		// this guard only fires if that invariant is ever broken
		if ( size > slotSize )
			throw std::bad_alloc();
		if ( void * p = freeList ) {
			freeList = *static_cast<void **>( p );
			return p;
		}
		char * chunk = static_cast<char *>(
#ifdef _WIN32
			_aligned_malloc( slotSize * chunkSlotCount, slotAlign )
#else
			std::aligned_alloc( slotAlign, slotSize * chunkSlotCount )
#endif
		);
		if ( !chunk )
			throw std::bad_alloc();
#ifdef _WIN32
		chunks.emplace_back( chunk, &freeAligned );
#else
		chunks.emplace_back( chunk, &std::free );
#endif
		// thread the fresh chunk's slots 1..N-1 onto the free list, hand out slot 0
		for ( std::size_t i = chunkSlotCount - 1; i >= 1; i-- ) {
			void * slot = chunk + i * slotSize;
			*static_cast<void **>( slot ) = freeList;
			freeList = slot;
		}
		return chunk;
	}

	void deallocate( void * p ) noexcept
	{
		std::lock_guard<std::mutex> lock( mutex );
		*static_cast<void **>( p ) = freeList;
		freeList = p;
	}

#ifdef _WIN32
	static void freeAligned( void * p ) { _aligned_free( p ); }
#endif
};

NifItemPool & nifItemPool()
{
	static NifItemPool pool;
	return pool;
}

}	// namespace

void * NifItem::operator new( std::size_t size )
{
	return nifItemPool().allocate( size );
}

void NifItem::operator delete( void * p ) noexcept
{
	if ( p )
		nifItemPool().deallocate( p );
}

bool NifData::compareStrings( const QChar * s, const char * t, size_t l )
{
	for ( size_t i = 0; i < l; i++ ) {
		if ( s[i] != t[i] )
			return false;
	}
	return true;
}

bool NifItem::isDescendantOf( const NifItem * testAncestor ) const
{
	if ( testAncestor ) {
		const NifItem * ancestor = this;
		do {
			if ( ancestor == testAncestor )
				return true;
			ancestor = ancestor->parent();
		} while ( ancestor );
	}

	return false;
}

int NifItem::ancestorLevel( const NifItem * testAncestor ) const
{
	if ( testAncestor ) {
		const NifItem * ancestor = this;
		for ( int level = 0; ; level++ ) {
			if ( ancestor == testAncestor )
				return level;
			ancestor = ancestor->parent();
			if ( !ancestor )
				break;
		}
	}

	return -1;
}

const NifItem * NifItem::ancestorAt( int testLevel ) const
{
	if ( testLevel >= 0 ) {
		const NifItem * ancestor = this;
		for ( int level = 0; ; level++ ) {
			if ( level == testLevel )
				return ancestor;
			ancestor = ancestor->parent();
			if ( !ancestor )
				break;
		}
	}

	return nullptr;
}

void NifItem::registerChild( NifItem * item, int at )
{
	int nOldChildren = childItemsSize;
	if ( nOldChildren >= childItemsCapacity ) [[unlikely]]
		reserveChildItems( nOldChildren + 1 );
	if ( at < 0 || at >= nOldChildren ) {
		at = nOldChildren;
		childItems[at] = item;
		childItemsSize++;
		item->rowIdx = at;
	} else {
		std::memmove( childItems + ( at + 1 ), childItems + at, size_t( nOldChildren - at ) * sizeof( NifItem * ) );
		childItems[at] = item;
		childItemsSize++;
		item->rowIdx = at;
		updateChildRows( at + 1 );
	}
	if ( item->isLink() || item->hasChildLinks() ) [[unlikely]]
		item->registerInParentLinkCache();
}

NifItem * NifItem::unregisterChild( int at )
{
	if ( !( at >= 0 && at < childItemsSize ) ) [[unlikely]]
		return nullptr;

	NifItem * item = childItems[at];

	int n = childItemsSize - ( at + 1 );
	if ( n > 0 )
		std::memmove( childItems + at, childItems + ( at + 1 ), size_t( n ) * sizeof( NifItem * ) );
	childItemsSize = at + n;
	updateChildRows( at );

	return item;
}

void NifItem::removeChildren( int row, int count )
{
	int iStart = std::max( row, 0 );
	int iEnd = std::min( row + count, childItemsSize );
	if ( iStart < iEnd ) [[likely]] {
		for ( int i = iStart; i < iEnd; i++ )
			delete childItems[i];
		int n = childItemsSize - iEnd;
		if ( n > 0 )
			std::memmove( childItems + iStart, childItems + iEnd, size_t( n ) * sizeof( NifItem * ) );
		childItemsSize = iStart + n;
		updateChildRows( iStart );
	}
}

size_t NifItem::findLinkRow( int n ) const
{
	size_t	i0 = 0;
	size_t	i2 = linkRowsSize;
	while ( i2 > i0 ) {
		size_t	i1 = ( i0 + i2 ) >> 1;
		if ( linkRows[i1] < n )
			i0 = i1 + 1;
		else
			i2 = i1;
	}
	// return the index of the first element of linkRows not less than 'n'
	return i0;
}

void NifItem::insertLinkRow( int n )
{
	if ( !linkRows || linkRowsSize >= (unsigned int) *( linkRows - 1 ) ) [[unlikely]] {
		size_t	linkRowsCapacity = size_t( linkRowsSize ) + 1;
		linkRowsCapacity = ( linkRowsCapacity + ( linkRowsCapacity >> 1 ) ) | 3;
		int *	tmp = new int[linkRowsCapacity + 1];
		tmp[0] = int( linkRowsCapacity );
		if ( linkRowsSize > 0 )
			std::memcpy( tmp + 1, linkRows, size_t( linkRowsSize ) * sizeof( int ) );
		if ( linkRows )
			delete[] ( linkRows - 1 );
		linkRows = tmp + 1;
	}

	size_t	i = linkRowsSize;
	if ( i && linkRows[i - 1] >= n ) {
		i = findLinkRow( n );
		if ( linkRows[i] == n )
			return;
		std::memmove( linkRows + ( i + 1 ), linkRows + i, ( size_t( linkRowsSize ) - i ) * sizeof( int ) );
	}
	if ( linkRowsSize >= 65535U )
		throw std::bad_alloc();
	linkRows[i] = n;
	linkRowsSize++;
}

void NifItem::registerInParentLinkCache()
{
	NifItem * c = this;
	for ( NifItem * p = parentItem; p; p = c->parentItem ) {
		bool bOldHasChildLinks = p->hasChildLinks();
		int	i = c->row();
		if ( i >= 0 )
			p->insertLinkRow( i );
		if ( bOldHasChildLinks )
			break; // Do NOT register p in its parent (again) if c is NOT a first registered child link for p
		c = p;
	}
}

void NifItem::unregisterInParentLinkCache()
{
	NifItem * c = this;
	for ( NifItem * p = parentItem; p; p = c->parentItem ) {
		p->removeLinkRow( c->row() );
		if ( p->linkRowsSize )
			break; // Do NOT unregister p in its parent if p still has other registered child links
		c = p;
	}
}

void NifItem::removeLinkRow( int n )
{
	size_t	i = findLinkRow( n );
	if ( i < linkRowsSize && linkRows[i] == n ) {
		if ( ( i + 1 ) < linkRowsSize )
			std::memmove( linkRows + i, linkRows + ( i + 1 ), ( size_t( linkRowsSize ) - ( i + 1 ) ) * sizeof( int ) );
		linkRowsSize--;
	}
}

void NifItem::updateChildRows( int iStartChild )
{
	for ( int i = iStartChild; i < childItemsSize; i++ ) {
		NifItem *	c = childItems[i];
		if ( c ) [[likely]]
			c->rowIdx = i;
	}
	if ( !hasChildLinks() ) [[likely]]
		return;
	updateLinkRows( iStartChild );
}

void NifItem::updateLinkRows( int iStartChild )
{
	size_t	i = linkRowsSize;
	while ( i > 0 && linkRows[i - 1] >= iStartChild )
		i--;
	if ( i >= linkRowsSize )
		return;
	linkRowsSize = i;
	for ( int n = iStartChild; n < childItemsSize; n++ ) {
		NifItem *	c = childItems[n];
		if ( c && ( c->isLink() || c->hasChildLinks() ) )
			insertLinkRow( n );
	}
	if ( !( isLink() || hasChildLinks() ) )
		unregisterInParentLinkCache();
}

int NifItem::updateRowIndex() const
{
	if ( !parentItem ) {
		rowIdx = 0;
		return 0;
	}
	const NifItem * const *	p = parentItem->childItems;
	qsizetype	n = parentItem->childItemsSize;
	for ( qsizetype i = 0; i < n; i++ ) {
		if ( p[i] == this ) {
			rowIdx = int( i );
			return rowIdx;
		}
	}
	rowIdx = -1;
	return -1;
}

void NifItem::reserveChildItems( int n )
{
	if ( n <= childItemsCapacity ) [[unlikely]]
		return;
	n = std::max< int >( n, ( ( childItemsCapacity + ( childItemsCapacity >> 1 ) ) | 1 ) + 1 );
	void *	tmp = std::realloc( childItems, size_t( n ) * sizeof( NifItem * ) );
	if ( !tmp )
		throw std::bad_alloc();
	childItems = reinterpret_cast< NifItem ** >( tmp );
	childItemsCapacity = n;
}

void NifItem::deleteChildItems()
{
	for ( auto c : children() )
		delete c;
	childItemsSize = 0;
}

void NifItem::onParentItemChange()
{
	parentModel     = parentItem->parentModel;
	vercondStatus   = -1;
	conditionStatus = -1;

	for ( auto c : children() )
		c->onParentItemChange();
}

QString NifItem::repr() const
{
	return parentModel->itemRepr( this );
}

void NifItem::reportError( const QString & msg ) const
{
	parentModel->reportError( this, msg );
}

void NifItem::reportError( const QString & funcName, const QString & msg ) const
{
	parentModel->reportError( this, funcName, msg );
}
