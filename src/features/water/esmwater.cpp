// lane MODULES1 (water, from src/esmdata.cpp): the WATR record's one-line echo (the water dump, the cell report)
#include "esmwater.h"

#include <QLatin1Char>
#include <QString>

QString wwWaterDescribe( const WwWaterRecord & r )
{
	const WwWaterMaterial m = wwWaterMaterial( r );
	QString s = QStringLiteral( "WATR %1 %2" ).arg( r.form, 8, 16, QLatin1Char( '0' ) )
		.arg( r.editorId.isEmpty() ? QStringLiteral( "-" ) : r.editorId );
	const float * blocks[] = { m.shallow, m.deep, m.reflection, m.underwater, m.lightSilt, m.darkSilt,
		m.varAmounts, m.params1, m.params2, m.params3, m.params4, m.amplitude, m.uvScale };
	for ( const float * b : blocks )
		for ( int i = 0; i < 4; i++ )
			s += QStringLiteral( " %1" ).arg( double( b[i] ), 0, 'g', 6 );
	s += QStringLiteral( " uw %1 %2 %3 wind %4 %5 %6 speed %7 %8 %9" )
		.arg( double( r.uwFogAmount ), 0, 'g', 6 ).arg( double( r.uwFogNear ), 0, 'g', 6 )
		.arg( double( r.uwFogFar ), 0, 'g', 6 )
		.arg( double( r.windDirection[0] ), 0, 'g', 6 ).arg( double( r.windDirection[1] ), 0, 'g', 6 )
		.arg( double( r.windDirection[2] ), 0, 'g', 6 )
		.arg( double( r.windSpeed[0] ), 0, 'g', 6 ).arg( double( r.windSpeed[1] ), 0, 'g', 6 )
		.arg( double( r.windSpeed[2] ), 0, 'g', 6 );
	for ( const QString & t : r.noise )
		s += QLatin1Char( ' ' ) + ( t.isEmpty() ? QStringLiteral( "-" ) : t );
	return s;
}
