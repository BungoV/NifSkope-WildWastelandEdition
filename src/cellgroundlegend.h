#ifndef CELLGROUNDLEGEND_H
#define CELLGROUNDLEGEND_H

#include <QRegularExpression>
#include <QString>

/*! lane DUNESAND3: the ground legend's TX07 clause. Both forms carry the wanted count, so a live
 *  WW_TERRBLEND_RED set can rewrite the clause the open wrote without rebuilding the ground. */
inline QString cellGroundSpecClause( bool nospec, int want, int total, const QString & what )
{
	return ( nospec
		? QStringLiteral( "; RED CONTROL WW_TERRBLEND_RED=nospec: no TX07 map on the ground (%1 of %2 %3 name one)" )
		: QStringLiteral( "; %1 of %2 %3 carry their TX07 _s map" ) ).arg( want ).arg( total ).arg( what );
}

//! The notes with every TX07 clause in the form `nospec` asks for; unchanged when they hold none
inline QString cellGroundSpecClauseLive( const QString & notes, bool nospec )
{
	static const QRegularExpression re( QStringLiteral(
		"; (?:RED CONTROL WW_TERRBLEND_RED=nospec: no TX07 map on the ground \\((\\d+) of (\\d+) (passes|mosaic buckets) name one\\)"
		"|(\\d+) of (\\d+) (passes|mosaic buckets) carry their TX07 _s map)" ) );
	QString out = notes;
	QList<QRegularExpressionMatch> hits;
	for ( auto it = re.globalMatch( notes ); it.hasNext(); )
		hits.prepend( it.next() );	// back to front, so earlier offsets stay valid
	for ( const QRegularExpressionMatch & m : hits ) {
		const int k = m.capturedLength( 1 ) ? 1 : 4;
		out.replace( m.capturedStart(), m.capturedLength(),
			cellGroundSpecClause( nospec, m.captured( k ).toInt(), m.captured( k + 1 ).toInt(), m.captured( k + 2 ) ) );
	}
	return out;
}

#endif // CELLGROUNDLEGEND_H
