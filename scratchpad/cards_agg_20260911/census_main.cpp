/* Lane CARDS-AGG -- the forested-cell census, BEFORE any code.
 *
 * Standalone: links src/esmdata.cpp and the vendored ESM container against
 * Qt6Core only (ww-standalone-writer-gate), so it costs no NifSkope build and
 * holds no exe.  It answers one question from the ESM alone, no bake:
 *
 *   for a region and for the whole Commonwealth, how many exterior cells hold
 *   at least N tree placements, how many trees each such cell holds, and what
 *   one aggregate card set per such cell would cost.
 *
 * THE TREE TEST IS THE SHIPPED ONE, copied verbatim from
 * src/lodgen.cpp:2031 (lodgenIsTreeModel) plus the TREE record type, the same
 * two tests src/nifcli.cpp:3333 uses for `--list-impostor-candidates
 * --candidates trees`.  It is a COPY because lodgen.cpp cannot be linked
 * standalone; the copy is CHECKED against the shipped exe's own candidate list
 * on the same region by census_check.sh, which is the gate on this file.
 *
 * The SCOL expansion and its transform are copied from src/lodgen.cpp:3389
 * (fromEuler of the NEGATED eulers, then rp + rm * (local * refScale)); the
 * two matrix routines are copied from src/data/niftypes.{h,cpp} because that
 * translation unit pulls in NifModel and cannot be linked alone either.
 */

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QString>
#include <QTextStream>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "esmdata.h"

namespace
{

QTextStream & out()
{
	static QTextStream s( stdout );
	return s;
}

// --- copied from src/data/niftypes.cpp:215 and niftypes.h:1005 -------------
struct Mat3
{
	float m[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };

	void fromEuler( float x, float y, float z )
	{
		const float sinX = std::sin( x ), cosX = std::cos( x );
		const float sinY = std::sin( y ), cosY = std::cos( y );
		const float sinZ = std::sin( z ), cosZ = std::cos( z );
		m[0][0] = cosY * cosZ;
		m[0][1] = -cosY * sinZ;
		m[0][2] = sinY;
		m[1][0] = sinX * sinY * cosZ + sinZ * cosX;
		m[1][1] = cosX * cosZ - sinX * sinY * sinZ;
		m[1][2] = -sinX * cosY;
		m[2][0] = sinX * sinZ - cosX * sinY * cosZ;
		m[2][1] = cosX * sinY * sinZ + sinX * cosZ;
		m[2][2] = cosX * cosY;
	}

	void apply( const float v[3], float o[3] ) const
	{
		for ( int i = 0; i < 3; i++ )
			o[i] = m[i][0] * v[0] + m[i][1] * v[1] + m[i][2] * v[2];
	}
};

// --- copied from src/lodgen.cpp:2031 --------------------------------------
bool isTreeModel( const QString & model )
{
	const int slash = qMax( model.lastIndexOf( QChar( '\\' ) ), model.lastIndexOf( QChar( '/' ) ) );
	const QString modelFile = model.mid( slash + 1 ).toLower();
	return model.contains( QLatin1String( "\\trees\\" ), Qt::CaseInsensitive )
		|| model.contains( QLatin1String( "/trees/" ), Qt::CaseInsensitive )
		|| modelFile.startsWith( QLatin1String( "tree" ) );
}

struct BaseVerdict
{
	bool tree = false;
	bool hasLod = false;
	QString source;
};

} // namespace

int main( int argc, char ** argv )
{
	QCoreApplication app( argc, argv );
	QStringList args;
	for ( int i = 1; i < argc; i++ )
		args << QString::fromLocal8Bit( argv[i] );

	QString esm = QStringLiteral( "X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm" );
	int region[4] = { 0, 0, -1, -1 };   // empty = whole worldspace
	QString csvPath;
	QString basesPath;
	QString extentsPath;
	for ( int i = 0; i < args.size(); i++ ) {
		if ( args[i] == QLatin1String( "--esm" ) && i + 1 < args.size() )
			esm = args[++i];
		else if ( args[i] == QLatin1String( "--region" ) && i + 4 < args.size() ) {
			for ( int k = 0; k < 4; k++ )
				region[k] = args[i + 1 + k].toInt();
			i += 4;
		} else if ( args[i] == QLatin1String( "--csv" ) && i + 1 < args.size() )
			csvPath = args[++i];
		else if ( args[i] == QLatin1String( "--bases" ) && i + 1 < args.size() )
			basesPath = args[++i];
		else if ( args[i] == QLatin1String( "--extents" ) && i + 1 < args.size() )
			extentsPath = args[++i];
	}

	EsmWorld world;
	QString error;
	if ( !world.load( esm, 0x3CU, &error ) ) {
		out() << "error: " << error << Qt::endl;
		out().flush();
		return 1;
	}

	/* `--list-impostor-candidates` output, `formid extent model`: the extent is
	 * the LARGER of the model's horizontal radius and its half-height, in world
	 * units (src/nifcli.cpp:3341).  It is the only size the ESM alone cannot
	 * give, so it is read from the shipped exe's own listing rather than
	 * re-derived. */
	QHash<quint32, float> extents;
	if ( !extentsPath.isEmpty() ) {
		QFile f( extentsPath );
		if ( f.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
			while ( !f.atEnd() ) {
				const QStringList t = QString::fromLatin1( f.readLine() ).trimmed()
					.split( QChar( ' ' ), Qt::SkipEmptyParts );
				if ( t.size() >= 2 )
					extents.insert( t[0].toUInt( nullptr, 16 ), t[1].toFloat() );
			}
		}
	}

	int minX = 0, minY = 0, maxX = 0, maxY = 0;
	world.cellBounds( minX, minY, maxX, maxY );
	const bool wholeWorld = region[2] < region[0];
	if ( wholeWorld ) {
		region[0] = minX; region[1] = minY; region[2] = maxX; region[3] = maxY;
	}

	out() << "worldspace " << world.worldspaceEdid()
		  << "  indexed cells " << world.cellCount()
		  << "  grid " << minX << ".." << maxX << " x " << minY << ".." << maxY << Qt::endl;
	out() << "census region  " << region[0] << " " << region[1] << " "
		  << region[2] << " " << region[3]
		  << "  (" << ( region[2] - region[0] + 1 ) * ( region[3] - region[1] + 1 )
		  << " cells in the rectangle)" << Qt::endl;

	QHash<quint32, BaseVerdict> verdicts;
	auto verdictFor = [&]( quint32 baseId ) -> const BaseVerdict & {
		auto it = verdicts.find( baseId );
		if ( it != verdicts.end() )
			return *it;
		BaseVerdict v;
		const EsmLodBase & b = world.lodBase( baseId );
		v.hasLod = b.hasLod;
		QString source = b.model;
		for ( int l = 0; l < 4 && source.isEmpty(); l++ )
			source = b.models[l];
		v.source = source;
		v.tree = ( std::memcmp( &b.type, "TREE", 4 ) == 0 ) || isTreeModel( source );
		return *verdicts.insert( baseId, v );
	};

	// per-cell counters, keyed (cx,cy)
	QHash<qint64, int> treesPerCell;       // tree placements with LOD
	QHash<qint64, int> allPerCell;         // every placement with LOD
	QHash<qint64, QHash<quint32, int>> basesPerCell;   // which tree bases, and how many
	QSet<quint32> treeBases;
	QHash<qint64, float> maxExtentPerCell;   // the biggest tree extent standing in the cell

	qint64 refrsSeen = 0, scolExpanded = 0, treesTotal = 0, placedTotal = 0;
	qint64 treesNoLod = 0;

	auto key = []( int cx, int cy ) { return ( qint64( cx ) << 32 ) ^ quint32( cy ); };

	auto record = [&]( quint32 baseId, const float pos[3] ) {
		const BaseVerdict & v = verdictFor( baseId );
		if ( !v.hasLod ) {
			if ( v.tree )
				treesNoLod++;
			return;
		}
		const int cx = int( std::floor( pos[0] / 4096.0f ) );
		const int cy = int( std::floor( pos[1] / 4096.0f ) );
		if ( cx < region[0] || cx > region[2] || cy < region[1] || cy > region[3] )
			return;
		placedTotal++;
		allPerCell[key( cx, cy )]++;
		if ( !v.tree )
			return;
		treesTotal++;
		treesPerCell[key( cx, cy )]++;
		basesPerCell[key( cx, cy )][baseId]++;
		treeBases.insert( baseId );
		float & e = maxExtentPerCell[key( cx, cy )];
		e = qMax( e, extents.value( baseId, 0.0f ) );
	};

	for ( int cy = region[1]; cy <= region[3]; cy++ ) {
		for ( int cx = region[0]; cx <= region[2]; cx++ ) {
			const QVector<EsmRefr> refs = world.refrs( cx, cy );
			for ( const EsmRefr & r : refs ) {
				if ( r.initiallyDisabled || r.deleted || !r.base )
					continue;
				refrsSeen++;
				if ( std::memcmp( &r.baseType, "SCOL", 4 ) == 0 ) {
					Mat3 rm;
					rm.fromEuler( -r.rot[0], -r.rot[1], -r.rot[2] );
					for ( const EsmScolPart & part : world.scolParts( r.base ) ) {
						for ( const EsmScolPlacement & pl : part.placements ) {
							float local[3] = { pl.pos[0] * r.scale, pl.pos[1] * r.scale,
								pl.pos[2] * r.scale };
							float rot[3];
							rm.apply( local, rot );
							const float p[3] = { r.pos[0] + rot[0], r.pos[1] + rot[1],
								r.pos[2] + rot[2] };
							scolExpanded++;
							record( part.base, p );
						}
					}
					continue;
				}
				record( r.base, r.pos );
			}
		}
	}

	out() << Qt::endl << "== placements" << Qt::endl;
	out() << "REFRs read (enabled, not deleted, with a base) : " << refrsSeen << Qt::endl;
	out() << "SCOL parts expanded                            : " << scolExpanded << Qt::endl;
	out() << "placements with a LOD base, inside the region  : " << placedTotal << Qt::endl;
	out() << "of those, TREE placements                      : " << treesTotal << Qt::endl;
	out() << "tree placements whose base has NO LOD (dropped): " << treesNoLod << Qt::endl;
	out() << "distinct tree bases                            : " << treeBases.size() << Qt::endl;
	out() << "cells holding at least one placement           : " << allPerCell.size() << Qt::endl;
	out() << "cells holding at least one tree                : " << treesPerCell.size() << Qt::endl;

	// the forest threshold sweep
	out() << Qt::endl << "== forested cells by threshold N (cells with >= N tree placements)" << Qt::endl;
	out() << "   N   cells   trees in them   share of all trees   distinct tree bases in the worst cell" << Qt::endl;
	const int thresholds[] = { 1, 2, 4, 6, 8, 12, 16, 24, 32, 48, 64, 128 };
	for ( int n : thresholds ) {
		qint64 cells = 0, trees = 0;
		int worstBases = 0;
		for ( auto it = treesPerCell.constBegin(); it != treesPerCell.constEnd(); ++it ) {
			if ( it.value() < n )
				continue;
			cells++;
			trees += it.value();
			worstBases = qMax( worstBases, basesPerCell[it.key()].size() );
		}
		out() << QString( "%1 %2 %3 %4 %5" )
			.arg( n, 4 ).arg( cells, 8 ).arg( trees, 14 )
			.arg( treesTotal ? QString::number( 100.0 * double( trees ) / double( treesTotal ), 'f', 1 ) + "%"
				: QStringLiteral( "-" ), 20 )
			.arg( worstBases, 12 ) << Qt::endl;
	}

	// histogram of trees per cell over the cells that hold any
	out() << Qt::endl << "== trees per cell, over cells that hold at least one" << Qt::endl;
	QMap<int, int> hist;   // bucket lower bound -> cells
	int maxTrees = 0;
	qint64 sum = 0;
	QVector<int> counts;
	counts.reserve( treesPerCell.size() );
	for ( auto it = treesPerCell.constBegin(); it != treesPerCell.constEnd(); ++it ) {
		const int c = it.value();
		counts.append( c );
		sum += c;
		maxTrees = qMax( maxTrees, c );
		int b = 1;
		while ( b * 2 <= c )
			b *= 2;
		hist[b]++;
	}
	std::sort( counts.begin(), counts.end() );
	for ( auto it = hist.constBegin(); it != hist.constEnd(); ++it )
		out() << QString( "  %1 .. %2 : %3 cells" ).arg( it.key(), 6 )
			.arg( it.key() * 2 - 1, 6 ).arg( it.value(), 7 ) << Qt::endl;
	if ( !counts.isEmpty() ) {
		auto pct = [&]( double p ) { return counts[qBound( 0, int( p * counts.size() ), counts.size() - 1 )]; };
		out() << QString( "  min %1  p50 %2  p90 %3  p99 %4  max %5  mean %6" )
			.arg( counts.first() ).arg( pct( 0.50 ) ).arg( pct( 0.90 ) ).arg( pct( 0.99 ) )
			.arg( maxTrees )
			.arg( double( sum ) / double( counts.size() ), 0, 'f', 1 ) << Qt::endl;
	}

	if ( !basesPath.isEmpty() ) {
		QFile f( basesPath );
		if ( f.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QTextStream bs( &f );
			QList<quint32> ids = treeBases.values();
			std::sort( ids.begin(), ids.end() );
			for ( quint32 id : ids )
				bs << QString( "%1" ).arg( id, 8, 16, QChar( '0' ) ) << " "
				   << verdicts[id].source << Qt::endl;
			out() << "tree base list written: " << basesPath << " (" << ids.size() << ")" << Qt::endl;
		}
	}

	if ( !csvPath.isEmpty() ) {
		QFile f( csvPath );
		if ( f.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
			QTextStream cs( &f );
			cs << "cx,cy,trees,placements,treeBases,landMin,landMax,relief,maxTreeExtent\n";
			QList<qint64> keys = treesPerCell.keys();
			std::sort( keys.begin(), keys.end() );
			for ( qint64 k : keys ) {
				const int cx = int( k >> 32 );
				const int cy = int( qint32( quint32( k & 0xFFFFFFFFU ) ) );
				float lo = 0.0f, hi = 0.0f;
				EsmLand ld;
				if ( world.land( cx, cy, ld ) && ld.valid ) {
					lo = hi = ld.heights[0][0];
					for ( int r = 0; r < 33; r++ )
						for ( int c = 0; c < 33; c++ ) {
							lo = qMin( lo, ld.heights[r][c] );
							hi = qMax( hi, ld.heights[r][c] );
						}
				}
				cs << cx << "," << cy << "," << treesPerCell[k] << ","
				   << allPerCell.value( k ) << "," << basesPerCell[k].size() << ","
				   << QString::number( lo, 'f', 1 ) << "," << QString::number( hi, 'f', 1 ) << ","
				   << QString::number( hi - lo, 'f', 1 ) << ","
				   << QString::number( maxExtentPerCell.value( k, 0.0f ), 'f', 1 ) << "\n";
			}
			out() << Qt::endl << "per-cell table written: " << csvPath << Qt::endl;
		}
	}

	out().flush();
	return 0;
}
