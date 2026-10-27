#include "cellfarhide.h"

#include "wwswitch.h"
#include "data/niftypes.h"

#include <climits>
#include <cmath>
#include <map>

namespace
{

struct MaskInfo
{
	int cx0 = 0, cy0 = 0, cx1 = -1, cy1 = -1;
	qint64 hidden = 0;
};

struct Doc
{
	CellFarHide::Hidden h;
	QHash<int, MaskInfo> masks;
};

std::map<const void *, Doc> & docs()
{
	static std::map<const void *, Doc> m;
	return m;
}

Doc * docOf( const void * doc )
{
	auto & m = docs();
	auto it = m.find( doc );
	return it == m.end() ? nullptr : &it->second;
}

}

namespace CellFarHide
{

int mode()
{
	WW_SWITCH_STATIC( QByteArray, red, wwEnv( "WW_CELL_FARHIDE_RED" ) );
	if ( red == "off" )
		return Off;
	if ( red == "bypos" )
		return ByPosition;
	return ByRef;
}

bool shadowKeyed()
{
	WW_SWITCH_STATIC( bool, on, wwEnv( "WW_CELL_FARHIDE_RED" ) != "noshadow" );
	return on;
}

const Hidden * hiddenFor( const void * doc )
{
	const Doc * d = docOf( doc );
	return d ? &d->h : nullptr;
}

void setCell( const void * doc, int x, int y, bool hide, const QVector<quint32> & refs )
{
	if ( !doc )
		return;
	Hidden & h = docs()[doc].h;
	const qint64 k = cellKey( x, y );
	if ( hide == h.cells.contains( k ) )
		return;
	if ( hide ) {
		h.cells.insert( k );
		for ( quint32 r : refs )
			if ( r )
				h.refs[r]++;
	} else {
		h.cells.remove( k );
		for ( quint32 r : refs ) {
			auto it = h.refs.find( r );
			if ( it != h.refs.end() && --it.value() <= 0 )
				h.refs.erase( it );
		}
	}
	// one counter for every document and clear(): a set made again must never repeat a key the row caches hold
	static quint64 gens = 0;
	h.gen = ++gens;
}

void clear( const void * doc )
{
	docs().erase( doc );
}

bool rowHidden( const CellMeshInst & d, int row, const Hidden & h, int mode )
{
	if ( mode == Off )
		return false;
	if ( mode == ByRef && row < d.rowRef.size() && d.rowRef[row] )
		return h.refs.contains( d.rowRef[row] );
	return row < d.rowCell.size() && h.cells.contains( d.rowCell[row] );
}

qint64 filter( const CellMeshInst & d, const Hidden & h, int mode, QVector<CellMeshInst::Cmd> & cmds,
	QVector<CellMeshInst::Tile> & tiles )
{
	cmds.clear();
	tiles.clear();
	qint64 hidden = 0;
	for ( const CellMeshInst::Tile & t : d.tiles ) {
		CellMeshInst::Tile nt = t;
		nt.firstCmd = cmds.size();
		nt.cmdCount = 0;
		for ( int j = 0; j < t.cmdCount; j++ ) {
			const CellMeshInst::Cmd & m = d.cmds[t.firstCmd + j];
			const int b = int( m.baseInstance ), e = b + int( m.instanceCount );
			int run = -1;
			for ( int i = b; i <= e; i++ ) {
				const bool vis = i < e && !rowHidden( d, i, h, mode );
				if ( i < e && !vis )
					hidden++;
				if ( vis && run < 0 ) {
					run = i;
				} else if ( !vis && run >= 0 ) {
					CellMeshInst::Cmd c = m;
					c.baseInstance = quint32( run );
					c.instanceCount = quint32( i - run );
					cmds.append( c );
					nt.cmdCount++;
					run = -1;
				}
			}
		}
		if ( nt.cmdCount )
			tiles.append( nt );
	}
	return hidden;
}

View view( const CellMeshInst & d )
{
	View v;
	v.cmds = &d.cmds;
	v.tiles = &d.tiles;
	const int m = mode();
	const Hidden * h = hiddenFor( d.doc );
	if ( m == Off || !h || ( h->cells.isEmpty() && h->refs.isEmpty() ) )
		return v;
	const quint64 want = ( h->gen << 2 ) | quint64( m );
	if ( d.visFor != want ) {
		d.visFor = want;
		d.visHidden = filter( d, *h, m, d.visCmds, d.visTiles );
		// the key changes only when the rows drawn change (a far-away attach leaves the shadow cache alone)
		quint64 k = 1469598103934665603ull;
		auto mix = [&k]( quint64 x ) {
			k ^= x;
			k *= 1099511628211ull;
		};
		if ( d.visHidden ) {
			for ( const CellMeshInst::Cmd & c : d.visCmds ) {
				mix( c.baseInstance );
				mix( c.instanceCount );
			}
			d.visKey = k | 1u;
		} else {
			d.visKey = 0;
		}
	}
	if ( d.visHidden ) {
		v.cmds = &d.visCmds;
		v.tiles = &d.visTiles;
	}
	v.key = d.visKey;
	v.hiddenRows = d.visHidden;
	return v;
}

void registerMask( const void * doc, int block, int cx0, int cy0, int cx1, int cy1 )
{
	MaskInfo & mi = docs()[doc].masks[block];
	mi.cx0 = cx0;
	mi.cy0 = cy0;
	mi.cx1 = cx1;
	mi.cy1 = cy1;
}

bool maskRegistered( const void * doc, int block )
{
	const Doc * d = docOf( doc );
	return d && d->masks.contains( block );
}

QList<int> maskBlocks( const void * doc, int x, int y )
{
	QList<int> out;
	const Doc * d = docOf( doc );
	if ( !d )
		return out;
	for ( auto it = d->masks.cbegin(); it != d->masks.cend(); ++it ) {
		const MaskInfo & m = it.value();
		if ( x == INT_MIN || ( x >= m.cx0 && x <= m.cx1 && y >= m.cy0 && y <= m.cy1 ) )
			out << it.key();
	}
	return out;
}

int maskPure( const Transform & world, const QVector<Vector3> & verts, QVector<Triangle> & tris,
	const QSet<qint64> & cells )
{
	if ( cells.isEmpty() || tris.isEmpty() )
		return 0;
	int n = 0;
	const qsizetype nv = verts.size();
	for ( qsizetype i = 0; i < tris.size(); i++ ) {
		const Triangle & t = tris.at( i );
		if ( t[0] >= nv || t[1] >= nv || t[2] >= nv || ( t[0] == t[1] && t[1] == t[2] ) )
			continue;
		const Vector3 c = world * ( ( verts.at( t[0] ) + verts.at( t[1] ) + verts.at( t[2] ) ) / 3.0f );
		if ( !cells.contains( cellKey( int( std::floor( c[0] / 4096.0f ) ), int( std::floor( c[1] / 4096.0f ) ) ) ) )
			continue;
		const quint16 v = t[0];
		tris[i] = Triangle( v, v, v );
		n++;
	}
	return n;
}

int applyMask( const void * doc, int block, const Transform & world, const QVector<Vector3> & verts,
	QVector<Triangle> & tris )
{
	Doc * d = docOf( doc );
	if ( !d )
		return 0;
	auto it = d->masks.find( block );
	if ( it == d->masks.end() )
		return 0;
	MaskInfo & m = it.value();
	m.hidden = 0;
	if ( mode() == Off )
		return 0;
	QSet<qint64> mine;
	for ( qint64 k : d->h.cells ) {
		const int x = int( k >> 32 ), y = int( qint32( quint32( k & 0xFFFFFFFFu ) ) );
		if ( x >= m.cx0 && x <= m.cx1 && y >= m.cy0 && y <= m.cy1 )
			mine.insert( k );
	}
	m.hidden = maskPure( world, verts, tris, mine );
	return int( m.hidden );
}

qint64 maskedTris( const void * doc )
{
	const Doc * d = docOf( doc );
	qint64 n = 0;
	if ( d )
		for ( const MaskInfo & m : d->masks )
			n += m.hidden;
	return n;
}

}
