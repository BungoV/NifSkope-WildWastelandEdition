/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellpick.h"

#include <QStringList>

#include <cmath>

void CellPickTable::clear()
{
	entries.clear();
	byForm.clear();
}

void CellPickTable::append( const CellPickEntry & e )
{
	const int i = entries.size();
	entries.append( e );
	if ( e.refForm && !byForm.contains( e.refForm ) )
		byForm.insert( e.refForm, i );
}

int CellPickTable::indexOfForm( quint32 refForm ) const
{
	auto it = byForm.constFind( refForm );
	return it == byForm.constEnd() ? -1 : it.value();
}

int CellPickTable::pick( const CellRay & ray, int * candidates, float * tOut ) const
{
	int best = -1;
	float bestT = 0.0f;
	float bestVol = 0.0f;
	int hits = 0;

	for ( int i = 0; i < entries.size(); i++ ) {
		const CellPickEntry & e = entries.at( i );
		/* Slab test. A zero component of the direction is not a special case
		 * with IEEE division -- it yields +/-inf and the min/max below reject
		 * or accept correctly -- EXCEPT when the origin is exactly on the slab,
		 * where it is 0 * inf = NaN. That is rare and it is also the one case a
		 * comparison chain silently gets wrong, so it is handled by hand. */
		float t0 = -3.0e38f, t1 = 3.0e38f;
		bool miss = false;
		for ( int k = 0; k < 3; k++ ) {
			const float d = ray.d[k];
			const float lo = e.bmin[k], hi = e.bmax[k];
			if ( std::fabs( d ) < 1.0e-12f ) {
				if ( ray.o[k] < lo || ray.o[k] > hi ) {
					miss = true;
					break;
				}
				continue;
			}
			float a = ( lo - ray.o[k] ) / d;
			float b = ( hi - ray.o[k] ) / d;
			if ( a > b ) {
				const float s = a;
				a = b;
				b = s;
			}
			if ( a > t0 )
				t0 = a;
			if ( b < t1 )
				t1 = b;
			if ( t0 > t1 ) {
				miss = true;
				break;
			}
		}
		if ( miss || t1 < 0.0f )
			continue;

		const float t = t0 > 0.0f ? t0 : 0.0f;
		hits++;
		const float vol = ( e.bmax[0] - e.bmin[0] ) * ( e.bmax[1] - e.bmin[1] )
			* ( e.bmax[2] - e.bmin[2] );
		/* Nearest entry wins; a tie goes to the SMALLER box, so a bottle inside
		 * a building is reachable. "Tie" is a real tolerance because both boxes
		 * being entered at the same face is exactly the ray-starts-inside case. */
		const bool better = best < 0
			|| t < bestT - 1.0e-3f
			|| ( t <= bestT + 1.0e-3f && vol < bestVol );
		if ( better ) {
			best = i;
			bestT = t;
			bestVol = vol;
		}
	}
	if ( candidates )
		*candidates = hits;
	if ( tOut )
		*tOut = best >= 0 ? bestT : 0.0f;
	return best;
}

QString CellPickTable::typeName( quint32 fourcc )
{
	char c[5] = { 0, 0, 0, 0, 0 };
	for ( int i = 0; i < 4; i++ ) {
		const char ch = char( ( fourcc >> ( i * 8 ) ) & 0xFF );
		if ( ch < 32 || ch > 126 )
			return QStringLiteral( "?" );
		c[i] = ch;
	}
	return QString::fromLatin1( c );
}

QString CellPickTable::formName( quint32 form )
{
	return QStringLiteral( "0x%1" ).arg( form, 8, 16, QLatin1Char( '0' ) ).toUpper()
		.replace( QLatin1String( "0X" ), QLatin1String( "0x" ) );
}

QVector<QPair<QString, QString>> CellPickTable::rowsFor( int i ) const
{
	QVector<QPair<QString, QString>> rows;
	if ( i < 0 || i >= entries.size() )
		return rows;
	const CellPickEntry & e = entries.at( i );
	auto add = []( QVector<QPair<QString, QString>> & r, const char * n, const QString & v ) {
		r.append( qMakePair( QString::fromLatin1( n ), v ) );
	};
	auto num = []( float f ) { return QString::number( f, 'f', 2 ); };

	add( rows, "Reference", formName( e.refForm ) );
	add( rows, "Base", formName( e.baseForm ) );
	add( rows, "Editor ID", e.baseEdid.isEmpty() ? QStringLiteral( "-" ) : e.baseEdid );
	add( rows, "Record", typeName( e.baseType ) );
	if ( e.scolPart >= 0 )
		add( rows, "Collection part", QString::number( e.scolPart ) );
	add( rows, "Model", e.model.isEmpty() ? QStringLiteral( "-" ) : e.model );
	add( rows, "Cell", QStringLiteral( "%1, %2" ).arg( e.cellX ).arg( e.cellY ) );
	add( rows, "Persistent", e.persistent ? QStringLiteral( "yes" ) : QStringLiteral( "no" ) );
	add( rows, "Position", QStringLiteral( "%1  %2  %3" )
		.arg( num( e.pos[0] ) ).arg( num( e.pos[1] ) ).arg( num( e.pos[2] ) ) );
	add( rows, "Rotation", QStringLiteral( "%1  %2  %3" )
		.arg( num( e.rot[0] * 57.2957795f ) ).arg( num( e.rot[1] * 57.2957795f ) )
		.arg( num( e.rot[2] * 57.2957795f ) ) );
	add( rows, "Scale", QString::number( e.scale, 'f', 3 ) );
	add( rows, "Bounds", QStringLiteral( "%1  %2  %3" )
		.arg( num( e.bmax[0] - e.bmin[0] ) ).arg( num( e.bmax[1] - e.bmin[1] ) )
		.arg( num( e.bmax[2] - e.bmin[2] ) ) );
	add( rows, "Triangles", QString::number( e.triangles ) );
	add( rows, "Layer", e.layer
		? ( e.layerEdid.isEmpty() ? formName( e.layer )
			: QStringLiteral( "%1  %2" ).arg( e.layerEdid, formName( e.layer ) ) )
		: QStringLiteral( "-" ) );
	add( rows, "Enable parent", e.enableParent
		? QStringLiteral( "%1%2" ).arg( formName( e.enableParent ),
			e.enableParentOpposite ? QStringLiteral( "  (opposite)" ) : QString() )
		: QStringLiteral( "-" ) );
	add( rows, "Disabled", e.disabled ? QStringLiteral( "yes" ) : QStringLiteral( "no" ) );
	add( rows, "Marker", e.marker ? QStringLiteral( "yes" ) : QStringLiteral( "no" ) );
	add( rows, "Precombined", e.precombined ? QStringLiteral( "yes" ) : QStringLiteral( "no" ) );
	{
		// NOT `slots`: that is a Qt keyword macro, and a variable named after it
		// stops declaring anything at all. The compiler says so; nothing else does.
		QStringList lodSlots;
		for ( int k = 0; k < 4; k++ ) {
			if ( !e.lodModels[k].isEmpty() )
				lodSlots.append( QStringLiteral( "%1: %2" ).arg( 4 << k ).arg( e.lodModels[k] ) );
		}
		add( rows, "Authored LOD", e.hasLod && !lodSlots.isEmpty()
			? lodSlots.join( QLatin1String( "\n" ) ) : QStringLiteral( "none" ) );
	}
	add( rows, "Identity group", e.haveGroup
		? QStringLiteral( "%1  (%2 placements)" ).arg( e.group ).arg( e.groupSize )
		: QStringLiteral( "no bake loaded" ) );
	return rows;
}

static CellPickTable s_table;

const CellPickTable & cellPickTable()
{
	return s_table;
}

CellPickTable & cellPickTableMutable()
{
	return s_table;
}
