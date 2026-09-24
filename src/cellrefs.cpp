/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellrefs.h"

QString cellBlockLabel( const CellBlockEntry & c )
{
	const QString grid = QString( "(%1,%2)" ).arg( c.cx ).arg( c.cy );
	if ( c.edid.isEmpty() )
		return grid;
	return c.edid + " " + grid;
}

void CellRefTable::clear()
{
	entries.clear();
	byForm.clear();
	cells.clear();
	byGrid.clear();
}

void CellRefTable::addCell( const CellBlockEntry & c )
{
	const QPair<int, int> key( c.cx, c.cy );
	if ( byGrid.contains( key ) )
		return;
	byGrid.insert( key, cells.size() );
	cells.append( c );
}

QString CellRefTable::labelOfGrid( int cx, int cy ) const
{
	const int i = byGrid.value( QPair<int, int>( cx, cy ), -1 );
	if ( i < 0 )
		return QString( "(%1,%2)" ).arg( cx ).arg( cy );
	return cellBlockLabel( cells.at( i ) );
}

void CellRefTable::tallyCells()
{
	for ( CellBlockEntry & c : cells ) {
		c.references = 0;
		c.drawn = 0;
	}

	for ( const CellRefEntry & e : entries ) {
		const int i = byGrid.value( QPair<int, int>( e.cellX, e.cellY ), -1 );
		if ( i < 0 )
			continue;
		cells[i].references++;
		if ( e.fate == CellRefFate::Drawn )
			cells[i].drawn++;
	}
}

int CellRefTable::append( const CellRefEntry & e )
{
	const int at = entries.size();
	entries.append( e );
	/* FIRST WINS, like the pick table's form index. A persistent reference can
	 * be returned by the cell it lives in AND by the persistent sweep of the
	 * block; the form index points at the first, so "find ref 0x...." has one
	 * answer whichever order the reader produced them in. */
	if ( e.refForm && !byForm.contains( e.refForm ) )
		byForm.insert( e.refForm, at );
	return at;
}

void CellRefTable::setFate( int i, CellRefFate fate )
{
	if ( i < 0 || i >= entries.size() )
		return;
	entries[i].fate = fate;
}

int CellRefTable::indexOfForm( quint32 refForm ) const
{
	return byForm.value( refForm, -1 );
}

int CellRefTable::countOfFate( CellRefFate fate ) const
{
	int n = 0;
	for ( const CellRefEntry & e : entries )
		if ( e.fate == fate )
			n++;
	return n;
}

QString CellRefTable::fateName( CellRefFate fate )
{
	switch ( fate ) {
	case CellRefFate::Drawn:    return QStringLiteral( "drawn" );
	case CellRefFate::Deleted:  return QStringLiteral( "deleted" );
	case CellRefFate::NoBase:   return QStringLiteral( "no base" );
	case CellRefFate::Disabled: return QStringLiteral( "disabled" );
	case CellRefFate::NoModel:  return QStringLiteral( "no model" );
	case CellRefFate::Marker:   return QStringLiteral( "marker" );
	}
	return QStringLiteral( "?" );
}

CellRefTable & cellRefTableMutable()
{
	static CellRefTable table;
	return table;
}

const CellRefTable & cellRefTable()
{
	return cellRefTableMutable();
}
