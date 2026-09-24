/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "cellpicktest.h"

#include "cellclick.h"
#include "cellpick.h"

#include "model/nifmodel.h"

#include <QFile>
#include <QLabel>
#include <QTextStream>
#include <QTreeWidget>

#include <cmath>

namespace {

struct Row
{
	bool ok;
	QString what;
	QString detail;
};

//! Slab test against one box, for the INDEPENDENT answer this file derives.
bool rayHitsBox( const float o[3], const float d[3], const float lo[3],
	const float hi[3], float * tOut )
{
	float tmin = 0.0f, tmax = 3.0e38f;
	for ( int k = 0; k < 3; k++ ) {
		if ( std::fabs( d[k] ) < 1.0e-9f ) {
			if ( o[k] < lo[k] || o[k] > hi[k] )
				return false;
			continue;
		}
		const float inv = 1.0f / d[k];
		float t0 = ( lo[k] - o[k] ) * inv;
		float t1 = ( hi[k] - o[k] ) * inv;
		if ( t0 > t1 ) {
			const float s = t0;
			t0 = t1;
			t1 = s;
		}
		tmin = qMax( tmin, t0 );
		tmax = qMin( tmax, t1 );
		if ( tmin > tmax )
			return false;
	}
	if ( tOut )
		*tOut = tmin;
	return true;
}

double boxVolume( const CellPickEntry & e )
{
	double v = 1.0;
	for ( int k = 0; k < 3; k++ )
		v *= double( qMax( 0.0f, e.bmax[k] - e.bmin[k] ) );
	return v;
}

//! The highlight shape's world centre, and its radius. False when it is gone.
bool highlightCentre( NifModel * nif, float centre[3], float * radius )
{
	if ( !nif )
		return false;
	for ( qint32 b = 0; b < nif->getBlockCount(); b++ ) {
		const QModelIndex iShape = nif->getBlockIndex( b, "BSTriShape" );
		if ( !iShape.isValid() )
			continue;
		if ( nif->get<QString>( iShape, "Name" ) != QLatin1String( "!cell pick" ) )
			continue;
		const Vector3 tr = nif->get<Vector3>( iShape, "Translation" );
		const QModelIndex iBound = nif->getIndex( iShape, "Bounding Sphere" );
		if ( !iBound.isValid() )
			return false;
		const Vector3 c = nif->get<Vector3>( iBound, "Center" );
		for ( int k = 0; k < 3; k++ )
			centre[k] = tr[k] + c[k];
		if ( radius )
			*radius = nif->get<float>( iBound, "Radius" );
		return true;
	}
	return false;
}

} // namespace


int cellPickSelfTest( NifModel * nif, QWidget * panel, const QString & reportPath )
{
	QVector<Row> rows;
	auto add = [&rows]( const char * what, bool ok, const QString & detail = QString() ) {
		rows.append( Row{ ok, QString::fromLatin1( what ), detail } );
	};

	const CellPickTable & table = cellPickTable();
	QTreeWidget * tree = panel ? panel->findChild<QTreeWidget *>(
		QStringLiteral( "CellPickRows" ) ) : nullptr;
	QLabel * summary = panel ? panel->findChild<QLabel *>(
		QStringLiteral( "CellPickSummary" ) ) : nullptr;

	add( "a cell scene is open and its placement table is filled",
		table.size() > 0, QString::number( table.size() ) + " entries" );
	add( "the dock exists, with the flat Name|Value tree and the summary line",
		tree != nullptr && summary != nullptr );

	if ( table.size() == 0 || !tree || !summary ) {
		// Everything below would measure nothing; say so ONCE, as a failure,
		// rather than printing a page of passes about an empty scene.
		add( "REFUSED: the rest of this test needs a filled table and the dock",
			false );
	} else {
		/* THE TARGET: the SMALLEST box in the scene. The rule under test is
		 * "nearest, ties to the smaller", and the smallest box is the one a
		 * table that ignored the rule would be least likely to return. */
		int tgt = -1;
		double smallest = 0.0;
		for ( int i = 0; i < table.size(); i++ ) {
			const double v = boxVolume( table.at( i ) );
			if ( v <= 0.0 )
				continue;
			if ( tgt < 0 || v < smallest ) {
				tgt = i;
				smallest = v;
			}
		}
		add( "at least one placement has a box with volume", tgt >= 0 );

		if ( tgt >= 0 ) {
			const CellPickEntry & te = table.at( tgt );
			float o[3], d[3] = { 0.0f, 0.0f, -1.0f };
			for ( int k = 0; k < 2; k++ )
				o[k] = ( te.bmin[k] + te.bmax[k] ) * 0.5f;
			o[2] = te.bmax[2] + 100000.0f;   // straight down, from well above

			// the INDEPENDENT answer, from the same fields, by this file's rule
			int want = -1;
			int entered = 0;
			float wantT = 0.0f;
			double wantVol = 0.0;
			for ( int i = 0; i < table.size(); i++ ) {
				const CellPickEntry & e = table.at( i );
				float t = 0.0f;
				if ( !rayHitsBox( o, d, e.bmin, e.bmax, &t ) )
					continue;
				entered++;
				const double v = boxVolume( e );
				if ( want < 0 || t < wantT - 0.5f
					|| ( std::fabs( t - wantT ) <= 0.5f && v < wantVol ) ) {
					want = i;
					wantT = t;
					wantVol = v;
				}
			}

			const bool owned = cellPickClick( nif, o, d );
			const int got = cellPickCurrent();
			add( "the click is OWNED by the cell view while the master is on", owned );
			add( "the ray picks a placement", got >= 0,
				QStringLiteral( "index %1 of %2, %3 boxes entered" )
					.arg( got ).arg( table.size() ).arg( entered ) );
			add( "the pick is the one an independent walk of the same boxes gives "
				"(nearest, ties to the smaller)", got == want,
				QStringLiteral( "picked %1, independent answer %2" )
					.arg( got ).arg( want ) );

			if ( got >= 0 ) {
				const CellPickEntry & ge = table.at( got );
				const QVector<QPair<QString, QString>> want_rows = table.rowsFor( got );
				add( "the dock shows exactly the rows rowsFor() returns",
					tree->topLevelItemCount() == want_rows.size(),
					QStringLiteral( "%1 in the dock, %2 from rowsFor()" )
						.arg( tree->topLevelItemCount() ).arg( want_rows.size() ) );
				bool first = tree->topLevelItemCount() > 0
					&& tree->topLevelItem( 0 )->text( 1 )
						== CellPickTable::formName( ge.refForm );
				add( "the dock's first row carries the PICKED reference's form id",
					first, tree->topLevelItemCount() > 0
						? tree->topLevelItem( 0 )->text( 0 ) + " = "
							+ tree->topLevelItem( 0 )->text( 1 )
						: QStringLiteral( "no rows" ) );
				add( "the dock says how many boxes were under the cursor",
					summary->text().contains( QLatin1String( "under the cursor" ) ),
					summary->text() );

				float hc[3] = { 0, 0, 0 };
				float hr = 0.0f;
				const bool haveHl = highlightCentre( nif, hc, &hr );
				add( "the highlight shape is in the document", haveHl );
				bool moved = haveHl;
				for ( int k = 0; k < 3 && moved; k++ ) {
					const float c = ( ge.bmin[k] + ge.bmax[k] ) * 0.5f;
					if ( std::fabs( hc[k] - c ) > 4.0f )
						moved = false;
				}
				add( "the highlight MOVED onto the picked placement's box", moved,
					haveHl ? QStringLiteral( "centre %1 %2 %3, radius %4" )
						.arg( hc[0] ).arg( hc[1] ).arg( hc[2] ).arg( hr )
						: QStringLiteral( "no shape" ) );

				/* RED CONTROL 1: THE MISS. Same origin, pointed AWAY. A test
				 * that cannot make this code report "nothing" is not measuring
				 * the ray at all. */
				float up[3] = { 0.0f, 0.0f, 1.0f };
				const bool ownedMiss = cellPickClick( nif, o, up );
				const int missed = cellPickCurrent();
				add( "RED: a ray pointed away picks nothing", missed < 0,
					QStringLiteral( "index %1" ).arg( missed ) );
				add( "RED: the miss is still owned, so the welded bucket is "
					"never selected instead", ownedMiss );
				add( "RED: the dock says so in one line",
					summary->text().contains( QLatin1String( "Nothing under the cursor" ) ),
					summary->text() );
				float mc[3] = { 0, 0, 0 };
				float mr = -1.0f;
				const bool haveMiss = highlightCentre( nif, mc, &mr );
				add( "RED: the highlight collapsed to a point (the block stays)",
					haveMiss && mr < 1.0f, QStringLiteral( "radius %1" ).arg( mr ) );

				/* RED CONTROL 2: THE MASTER. Everything above is what a user who
				 * ticks the row gets; this is what everybody else must keep. */
				cellPickSetEnabled( false );
				const bool offOwned = cellPickClick( nif, o, d );
				cellPickSetEnabled( true );
				add( "RED: with the master row unticked the click is NOT taken",
					!offOwned );

				/* LEAVE THE WINDOW IN THE STATE THE ROWS ABOVE PROVED (lane
				 * CELLVIEW2B). The last two red controls deliberately end on a
				 * MISS and on a refused click, so without this the window a
				 * picture is taken of (`WW_UI_SHOT=1
				 * WW_UI_SHOT_DOCK=CellPickDock`) shows a dock reading "Nothing
				 * under the cursor" and a collapsed highlight -- the opposite
				 * of what the test just measured. Re-issuing the honest click
				 * asserts nothing and adds no row: it only restores the picked
				 * state so the dock and the highlight can be photographed. */
				cellPickClick( nif, o, d );
			}
		}
	}

	int failures = 0;
	for ( const Row & r : rows ) {
		if ( !r.ok )
			failures++;
	}

	QFile f( reportPath );
	if ( f.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
		QTextStream s( &f );
		s << "# WW_CELLPICK_TEST -- lane CELLVIEW2\n";
		for ( const Row & r : rows ) {
			s << ( r.ok ? "PASS  " : "FAIL  " ) << r.what;
			if ( !r.detail.isEmpty() )
				s << "   [" << r.detail << "]";
			s << "\n";
		}
		s << "rows " << rows.size() << " failures " << failures << "\n";
	}
	return failures;
}
