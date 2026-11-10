#include "cellfarhide.h"

#include "cellmodsource.h"	// lane LARGEREF2: the game's inis (bDisableLargeRefs)
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
	bool objects = true;   //!< lane LARGEREF6: a far objects shape (the keep boxes apply)
};

struct Doc
{
	CellFarHide::Hidden h;
	QHash<int, MaskInfo> masks;
	QVector<quint32> extra;   //!< lane GEOLOD1: setExtraRefs' list, counted in h.refs
	std::vector<LargeRef::Box> keep;   //!< lane LARGEREF6: setKeepBoxes' boxes
};

//! lane LARGEREF6: the cells a box reaches
void boxCells( const LargeRef::Box & b, QSet<qint64> & out )
{
	if ( !b.valid() )
		return;
	const int x0 = int( std::floor( b.x0 / 4096.0f ) ), x1 = int( std::floor( b.x1 / 4096.0f ) );
	const int y0 = int( std::floor( b.y0 / 4096.0f ) ), y1 = int( std::floor( b.y1 / 4096.0f ) );
	for ( int y = y0; y <= y1; y++ )
		for ( int x = x0; x <= x1; x++ )
			out.insert( CellFarHide::cellKey( x, y ) );
}

// one counter for every document and clear(): a set made again must never repeat a key the row caches hold
quint64 nextGen()
{
	static quint64 gens = 0;
	return ++gens;
}

//! lane PERFLOAD1 P7 round 2: per ladder REFR the steps drawn and cast (pending, and live since the last commit)
struct LadderRow
{
	quint8 draw = 0, drawLive = 0, cast = 0, castLive = 0;
};
struct Ladder
{
	QHash<quint32, LadderRow> rows;
	quint64 gen = 0, castGen = 0, fadeGen = 0;
	bool dirty = false, castDirty = false;
	int hidden = 0;	// REFRs whose live draw has no mesh step
	QHash<quint32, CellFarHide::LadderFade> fades;
	int frame = 0;
};
Ladder & ladder()
{
	static Ladder l;
	return l;
}

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
	h.gen = nextGen();
}

void clear( const void * doc )
{
	docs().erase( doc );
}

void setExtraRefs( const void * doc, const QVector<quint32> & refs )
{
	if ( !doc )
		return;
	WW_SWITCH_STATIC( bool, nohide, wwEnv( "WW_CELL_LARGEREFS_RED" ) == "nohide" );
	if ( nohide || ( refs.isEmpty() && !docOf( doc ) ) )
		return;
	Doc & d = docs()[doc];
	if ( refs.isEmpty() && d.extra.isEmpty() )
		return;
	Hidden & h = d.h;
	for ( quint32 r : d.extra ) {
		auto it = h.refs.find( r );
		if ( it != h.refs.end() && --it.value() <= 0 )
			h.refs.erase( it );
	}
	d.extra.clear();
	for ( quint32 r : refs )
		if ( r ) {
			h.refs[r]++;
			d.extra << r;
		}
	h.gen = nextGen();
}

bool largeRefsWanted()
{
	/* lane LARGEREF2 (LARGEREF-INI): WW_CELL_LARGEREFS pins it; unset, the game's inis decide as the game does:
	 * [General] bDisableLargeRefs=0 turns large refs on (the exe default is 1, off). Red WW_CELL_LARGEREFS_RED=noini:
	 * the inis are not read (LARGEREF1: off unless pinned). */
	WW_SWITCH_STATIC( bool, on, wwEnvIsSet( "WW_CELL_LARGEREFS" ) ? wwEnvInt( "WW_CELL_LARGEREFS" ) != 0
		: wwEnv( "WW_CELL_LARGEREFS_RED" ) != "noini"
			&& cellGameIniFloat( "[General]", "bDisableLargeRefs", 1.0f ) == 0.0f );
	return on;
}

bool rowHidden( const CellMeshInst & d, int row, const Hidden & h, int mode, bool caster )
{
	if ( row < d.rowStep.size() && d.rowStep[row] && row < d.rowRef.size() ) {
		const quint8 st = d.rowStep[row];
		const Ladder & l = ladder();
		const auto it = l.rows.constFind( d.rowRef[row] );
		if ( it == l.rows.constEnd() ? !( st & 0x80u ) : !( st & 0x7Fu & ( caster ? it->castLive : it->drawLive ) ) )
			return true;
	}
	if ( mode == Off )
		return false;
	if ( mode == ByRef && row < d.rowRef.size() && d.rowRef[row] )
		return h.refs.contains( d.rowRef[row] );
	return row < d.rowCell.size() && h.cells.contains( d.rowCell[row] );
}

qint64 filter( const CellMeshInst & d, const Hidden & h, int mode, QVector<CellMeshInst::Cmd> & cmds,
	QVector<CellMeshInst::Tile> & tiles, bool caster )
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
				const bool vis = i < e && !rowHidden( d, i, h, mode, caster );
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

View view( const CellMeshInst & d, bool caster )
{
	View v;
	v.cmds = &d.cmds;
	v.tiles = &d.tiles;
	const int m = mode();
	const Hidden * h = hiddenFor( d.doc );
	const Ladder & l = ladder();
	const bool lad = !d.rowStep.isEmpty();	// lane PERFLOAD1 P7 round 2: only a bucket with ladder rows sees the ladder
	if ( !lad && ( m == Off || !h || ( h->cells.isEmpty() && h->refs.isEmpty() ) ) )
		return v;
	static const Hidden none;
	const quint64 lg = lad ? ( caster ? l.castGen : l.gen ) : 0u;
	const quint64 want = ( ( ( h ? h->gen : 0u ) * 1000003ull ) ^ ( lg << 40 ) ) << 2 | quint64( m );
	quint64 & visFor = caster ? d.castFor : d.visFor;
	quint64 & visKey = caster ? d.castKey : d.visKey;
	qint64 & visHidden = caster ? d.castHidden : d.visHidden;
	QVector<CellMeshInst::Cmd> & visCmds = caster ? d.castCmds : d.visCmds;
	QVector<CellMeshInst::Tile> & visTiles = caster ? d.castTiles : d.visTiles;
	if ( visFor != want ) {
		visFor = want;
		visHidden = filter( d, h ? *h : none, m, visCmds, visTiles, caster );
		// the key changes only when the rows drawn change (a far-away attach leaves the shadow cache alone)
		quint64 k = 1469598103934665603ull;
		auto mix = [&k]( quint64 x ) {
			k ^= x;
			k *= 1099511628211ull;
		};
		if ( visHidden ) {
			for ( const CellMeshInst::Cmd & c : visCmds ) {
				mix( c.baseInstance );
				mix( c.instanceCount );
			}
			visKey = k | 1u;
		} else {
			visKey = 0;
		}
	}
	if ( visHidden ) {
		v.cmds = &visCmds;
		v.tiles = &visTiles;
	}
	v.key = visKey;
	v.hiddenRows = visHidden;
	return v;
}

int setLadder( quint32 ref, quint8 draw, quint8 cast )
{
	if ( !ref )
		return 0;
	Ladder & l = ladder();
	LadderRow & r = l.rows[ref];
	if ( r.draw != draw ) {
		r.draw = draw;
		l.dirty = true;
	}
	if ( r.cast != cast ) {
		r.cast = cast;
		l.castDirty = true;
	}
	if ( draw & ~r.drawLive )
		return 1;
	return ( r.drawLive & ~draw ) ? 2 : 0;
}

bool ladderDirty()
{
	return ladder().dirty;
}

void ladderCommit()
{
	Ladder & l = ladder();
	l.hidden = 0;
	for ( LadderRow & r : l.rows ) {
		r.drawLive = r.draw;
		if ( !( r.drawLive & 0x7Fu ) )
			l.hidden++;
	}
	l.gen = nextGen();
	l.dirty = false;
}

bool castDirty()
{
	return ladder().castDirty;
}

void castCommit()
{
	Ladder & l = ladder();
	for ( LadderRow & r : l.rows )
		r.castLive = r.cast;
	l.castGen = nextGen();
	l.castDirty = false;
}

quint64 ladderCastGen()
{
	return ladder().castGen;
}

quint8 ladderCast( quint32 ref )
{
	const Ladder & l = ladder();
	const auto it = l.rows.constFind( ref );
	return it == l.rows.constEnd() ? quint8( 0xFF ) : it->castLive;
}

void ladderClear()
{
	Ladder & l = ladder();
	if ( l.rows.isEmpty() && l.fades.isEmpty() )
		return;
	l.rows.clear();
	l.fades.clear();
	l.hidden = 0;
	l.gen = nextGen();
	l.castGen = nextGen();
	l.fadeGen++;
	l.dirty = false;
	l.castDirty = false;
}

int ladderHiddenCount()
{
	return ladder().hidden;
}

int ladderCount()
{
	return int( ladder().rows.size() );
}

void ladderFadesBegin( int frame )
{
	Ladder & l = ladder();
	l.frame = frame;
	if ( !l.fades.isEmpty() ) {
		l.fades.clear();
		l.fadeGen++;
	}
}

void ladderFade( quint32 ref, const TreeLadder::State & st, quint8 draw )
{
	Ladder & l = ladder();
	if ( l.fades.isEmpty() )
		l.fadeGen++;
	LadderFade & f = l.fades[ref];
	f.st = st;
	f.draw = draw;
}

const QHash<quint32, LadderFade> & ladderFades()
{
	return ladder().fades;
}

quint64 ladderFadeGen()
{
	return ladder().fadeGen;
}

int ladderFrame()
{
	return ladder().frame;
}

qint64 & ladderDitherRows()
{
	static qint64 n = 0;
	return n;
}

void registerMask( const void * doc, int block, int cx0, int cy0, int cx1, int cy1, bool objects )
{
	MaskInfo & mi = docs()[doc].masks[block];
	mi.cx0 = cx0;
	mi.cy0 = cy0;
	mi.cx1 = cx1;
	mi.cy1 = cy1;
	mi.objects = objects;
}

QSet<qint64> setKeepBoxes( const void * doc, const std::vector<LargeRef::Box> & boxes )
{
	QSet<qint64> cells;
	if ( !doc )
		return cells;
	Doc & d = docs()[doc];
	for ( const LargeRef::Box & b : d.keep )
		boxCells( b, cells );
	for ( const LargeRef::Box & b : boxes )
		boxCells( b, cells );
	d.keep = boxes;
	d.h.gen = nextGen();
	return cells;
}

int keepBoxCount( const void * doc )
{
	const Doc * d = docOf( doc );
	return d ? int( d->keep.size() ) : 0;
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
	return maskPure( world, verts, tris, cells, std::vector<LargeRef::Box>() );
}

int maskPure( const Transform & world, const QVector<Vector3> & verts, QVector<Triangle> & tris,
	const QSet<qint64> & cells, const std::vector<LargeRef::Box> & keep )
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
		if ( !keep.empty() && LargeRef::inBox( c[0], c[1], keep ) )   // lane LARGEREF6: inside a large ref's far footprint
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
	m.hidden = maskPure( world, verts, tris, mine, m.objects ? d->keep : std::vector<LargeRef::Box>() );
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
