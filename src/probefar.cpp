#include "probefar.h"

#include "lodifile.h"
#include "lodtfile.h"
#include "lodtsheets.h"
#include "probebake.h"
#include "io/lodvfile.h"

#include <QDir>
#include <QFileInfo>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

/* THE FAR MAP (lane PRTPFAR; docs/PRTP_PLAN.md 2h). See probefar.h. */

namespace {

constexpr float kCell = 4096.0f;

float srgbToLinear( float c )
{
	return c <= 0.04045f ? c / 12.92f : std::pow( ( c + 0.055f ) / 1.055f, 2.4f );
}

quint8 toByte( float lin )
{
	return quint8( std::clamp( int( std::lround( lin * 255.0f ) ), 0, 255 ) );
}

// sRGB 110, 100, 85: the bake's own stated dirt tone when no colour is known (probealbedo.cpp)
const quint8 kDirt[3] = { toByte( srgbToLinear( 110 / 255.0f ) ), toByte( srgbToLinear( 100 / 255.0f ) ),
	toByte( srgbToLinear( 85 / 255.0f ) ) };
// open water seen from above: dark, slightly blue-green (sRGB 40, 60, 66)
const quint8 kWater[3] = { toByte( srgbToLinear( 40 / 255.0f ) ), toByte( srgbToLinear( 60 / 255.0f ) ),
	toByte( srgbToLinear( 66 / 255.0f ) ) };
// a building box: weathered concrete and brick, sRGB 120, 112, 102
const quint8 kBuilding[3] = { toByte( srgbToLinear( 120 / 255.0f ) ), toByte( srgbToLinear( 112 / 255.0f ) ),
	toByte( srgbToLinear( 102 / 255.0f ) ) };

struct CellRec
{
	float lo = 0, hi = 0, waterH = 0, roof = -1.0e30f;
	bool ok = false, water = false;
};

}   // namespace

bool probeFarBuild( const ProbeFarSpec & spec, ProbeSoup & soup, std::vector<ProbePoint> & probes,
	ProbeFarResult * out )
{
	ProbeFarResult R;
	auto fail = [&]( const QString & m ) {
		R.error = m;
		if ( out )
			*out = R;
		return false;
	};
	LodtFile f;
	QString err;
	if ( !f.open( spec.lodl, &err ) )
		return fail( QStringLiteral( "%1: %2" ).arg( spec.lodl, err ) );
	const int spc = f.samplesPerCell();
	const int step = std::clamp( spec.step, 1, spc );
	if ( spc % step )
		return fail( QStringLiteral( "--step %1 does not divide %2 samples a cell" ).arg( step ).arg( spc ) );
	int x0 = f.cellMinX(), y0 = f.cellMinY(), x1 = f.cellMaxX(), y1 = f.cellMaxY();
	if ( spec.region ) {
		x0 = std::max( x0, spec.x0 );
		y0 = std::max( y0, spec.y0 );
		x1 = std::min( x1, spec.x1 );
		y1 = std::min( y1, spec.y1 );
	}
	if ( x0 > x1 || y0 > y1 )
		return fail( QStringLiteral( "the region holds no cell of %1" ).arg( spec.lodl ) );
	// the ground reaches past the probes' cells, so an edge probe still sees ground to its side
	const int ring = spec.region ? int( std::ceil( spec.ringCells ) ) : 0;
	const int gx0 = std::max( f.cellMinX(), x0 - ring ), gy0 = std::max( f.cellMinY(), y0 - ring );
	const int gx1 = std::min( f.cellMaxX(), x1 + ring ), gy1 = std::min( f.cellMaxY(), y1 + ring );
	const int W = gx1 - gx0 + 1, H = gy1 - gy0 + 1;

	std::vector<CellRec> cells( size_t( W ) * size_t( H ) );
	auto at = [&]( int cx, int cy ) -> CellRec & { return cells[size_t( cy - gy0 ) * size_t( W ) + size_t( cx - gx0 )]; };
	for ( int cy = gy0; cy <= gy1; cy++ )
		for ( int cx = gx0; cx <= gx1; cx++ ) {
			CellRec & c = at( cx, cy );
			quint16 wt = 0, fl = 0;
			c.ok = f.cell( cx, cy, c.lo, c.hi, c.waterH, wt, fl );
			c.water = c.ok && ( fl & 1u ) && c.waterH > c.lo;
			c.roof = c.hi;
			if ( c.water )
				c.roof = std::max( c.roof, c.waterH );
		}

	// --- the ground: one quad every `step` samples; heights read in .lodl sample space
	const int shift = spec.red == QLatin1String( "shift" ) ? spc : 0;
	const int maxGx = f.cellsX() * spc - 1, maxGy = f.cellsY() * spc - 1;
	auto h = [&]( int cx, int cy, int i, int j ) {
		const int gx = std::clamp( ( cx - f.cellMinX() ) * spc + i + shift, 0, maxGx );
		const int gy = std::clamp( ( cy - f.cellMinY() ) * spc + j, 0, maxGy );
		return f.height( gx, gy );
	};
	const float q = f.heightQuantum();
	const int n = spc / step;
	const float qs = kCell / float( n );

	// the colour sheet: decoded one tile at a time, quads laid tile by tile
	LodtSheets sheets;
	bool haveSheet = false;
	if ( spec.sheetDim > 0 ) {
		const QByteArray old = qgetenv( "WW_LODL_SHEET_DIM" );
		qputenv( "WW_LODL_SHEET_DIM", QByteArray::number( spec.sheetDim ) );
		QString why;
		haveSheet = sheets.open( spec.lodl, &why ) && sheets.hasRole( LODV_ROLE_COLOR );
		if ( old.isEmpty() )
			qunsetenv( "WW_LODL_SHEET_DIM" );
		else
			qputenv( "WW_LODL_SHEET_DIM", old );
		R.sheet = haveSheet ? sheets.containerPath() : QStringLiteral( "none (%1)" ).arg( why.isEmpty()
			? QStringLiteral( "no colour sheet" ) : why );
	} else {
		R.sheet = QStringLiteral( "none (--sheet-dim 0)" );
	}

	// cells grouped by the sheet tile that holds them (one group when there is no sheet)
	std::map<std::pair<int, int>, std::vector<std::pair<int, int>>> byTile;
	for ( int cy = gy0; cy <= gy1; cy++ )
		for ( int cx = gx0; cx <= gx1; cx++ ) {
			if ( !at( cx, cy ).ok )
				continue;
			int tx = -1, ty = -1;
			if ( haveSheet && !sheets.tileOfCell( cx, cy, &tx, &ty ) )
				tx = ty = -1;
			byTile[{ tx, ty }].push_back( { cx, cy } );
		}
	for ( const auto & grp : byTile ) {
		const int tx = grp.first.first, ty = grp.first.second;
		std::vector<quint8> ch[3];
		bool colour = tx >= 0;
		if ( colour ) {
			QString why;
			for ( int k = 0; k < 3 && colour; k++ )
				colour = sheets.sheetChannel( LODV_ROLE_COLOR, tx, ty, k, ch[k], &why );
		}
		const int st = colour ? sheets.storedTexels() : 0;
		const int dim = colour ? sheets.levelDim() : 1;
		for ( const auto & cc : grp.second ) {
			const int cx = cc.first, cy = cc.second;
			const CellRec & c = at( cx, cy );
			// the gate: every sample this cell reads lies in its own stored lo..hi
			for ( int j = 0; j <= spc; j += step )
				for ( int i = 0; i <= spc; i += step ) {
					if ( ( i == spc && cx == f.cellMaxX() ) || ( j == spc && cy == f.cellMaxY() ) )
						continue;
					if ( i == spc || j == spc )
						continue;   // the next cell's first sample: its range, not this one's
					const float z = h( cx, cy, i, j );
					if ( z < c.lo - q || z > c.hi + q )
						R.heightOutside++;
				}
			for ( int b = 0; b < n; b++ )
				for ( int a = 0; a < n; a++ ) {
					const float wx0 = cx * kCell + a * qs, wy0 = cy * kCell + b * qs;
					const float z00 = h( cx, cy, a * step, b * step ), z10 = h( cx, cy, ( a + 1 ) * step, b * step );
					const float z01 = h( cx, cy, a * step, ( b + 1 ) * step );
					const float z11 = h( cx, cy, ( a + 1 ) * step, ( b + 1 ) * step );
					quint8 rgb[3] = { kDirt[0], kDirt[1], kDirt[2] };
					if ( colour ) {
						// this quad's footprint on the tile, v from the NORTH edge (docs/LODGEN_TERRAIN_VT.md 2.2)
						const float u0 = ( float( cx - sheets.west() - tx * dim ) + float( a ) / n ) / dim;
						const float u1 = u0 + 1.0f / ( float( n ) * dim );
						const float v0 = ( float( sheets.north() - cy - ty * dim ) + 1.0f - float( b + 1 ) / n ) / dim;
						const float v1 = v0 + 1.0f / ( float( n ) * dim );
						auto tex = [&]( float t ) { return ( sheets.uvBias() + t * sheets.uvScale() ) * st; };
						const int sx0 = std::clamp( int( tex( u0 ) ), 0, st - 1 );
						const int sx1 = std::clamp( int( std::ceil( tex( u1 ) ) ), sx0 + 1, st );
						const int sy0 = std::clamp( int( tex( v0 ) ), 0, st - 1 );
						const int sy1 = std::clamp( int( std::ceil( tex( v1 ) ) ), sy0 + 1, st );
						double sum[3] = { 0, 0, 0 };
						int cnt = 0;
						for ( int y = sy0; y < sy1; y++ )
							for ( int x = sx0; x < sx1; x++ ) {
								for ( int k = 0; k < 3; k++ )
									sum[k] += srgbToLinear( ch[k][size_t( y ) * size_t( st ) + size_t( x )] / 255.0f );
								cnt++;
							}
						if ( cnt ) {
							for ( int k = 0; k < 3; k++ )
								rgb[k] = toByte( float( sum[k] / cnt ) );
							R.albedoQuads++;
						}
					}
					const float p00[3] = { wx0, wy0, z00 }, p10[3] = { wx0 + qs, wy0, z10 };
					const float p01[3] = { wx0, wy0 + qs, z01 }, p11[3] = { wx0 + qs, wy0 + qs, z11 };
					soup.addTri( p00, p10, p11, rgb );
					soup.addTri( p00, p11, p01, rgb );
					R.quads++;
					if ( c.water && std::min( { z00, z10, z01, z11 } ) < c.waterH ) {
						const float w00[3] = { wx0, wy0, c.waterH }, w10[3] = { wx0 + qs, wy0, c.waterH };
						const float w01[3] = { wx0, wy0 + qs, c.waterH }, w11[3] = { wx0 + qs, wy0 + qs, c.waterH };
						soup.addTri( w00, w10, w11, kWater );
						soup.addTri( w00, w11, w01, kWater );
						R.waterQuads++;
					}
				}
		}
	}

	// --- the buildings: the .lodi occluder boxes (inside each object's own LOD mesh)
	if ( !spec.lodi.isEmpty() ) {
		LodiHeader lh;
		LodiTable lt;
		if ( !lodiRead( spec.lodi, &lh, &lt, false, &err ) )
			return fail( QStringLiteral( "%1: %2" ).arg( spec.lodi, err ) );
		std::vector<char> seen( cells.size(), 0 );
		for ( const LodiOccluder & o : lt.occluders ) {
			float quat[4], m[9];
			lodiUnpackRotation( o.rot, quat, m );
			float v[8][3];
			for ( int k = 0; k < 8; k++ ) {
				const float s[3] = { ( k & 1 ) ? o.halfExtent[0] : -o.halfExtent[0],
					( k & 2 ) ? o.halfExtent[1] : -o.halfExtent[1], ( k & 4 ) ? o.halfExtent[2] : -o.halfExtent[2] };
				for ( int r = 0; r < 3; r++ )
					v[k][r] = o.centre[r] + m[r * 3 + 0] * s[0] + m[r * 3 + 1] * s[1] + m[r * 3 + 2] * s[2];
			}
			static const int F[6][4] = { { 0, 2, 3, 1 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 }, { 2, 6, 7, 3 },
				{ 0, 4, 6, 2 }, { 1, 3, 7, 5 } };
			/* The roof of EVERY cell the box's footprint crosses, not only the cell of its centre: a
			 * long building over a cell border stood 1583 units over its neighbour's probe (the
			 * gate's roofline check, 2026-10-01). */
			float lo[3] = { v[0][0], v[0][1], v[0][2] }, hi[3] = { v[0][0], v[0][1], v[0][2] };
			for ( const auto & p : v )
				for ( int r = 0; r < 3; r++ ) {
					lo[r] = std::min( lo[r], p[r] );
					hi[r] = std::max( hi[r], p[r] );
				}
			const int cx0 = std::max( gx0, int( std::floor( lo[0] / kCell ) ) ), cx1 = std::min( gx1, int( std::floor( hi[0] / kCell ) ) );
			const int cy0 = std::max( gy0, int( std::floor( lo[1] / kCell ) ) ), cy1 = std::min( gy1, int( std::floor( hi[1] / kCell ) ) );
			if ( cx0 > cx1 || cy0 > cy1 )
				continue;
			for ( const auto & fc : F ) {
				soup.addTri( v[fc[0]], v[fc[1]], v[fc[2]], kBuilding );
				soup.addTri( v[fc[0]], v[fc[2]], v[fc[3]], kBuilding );
			}
			R.boxes++;
			for ( int cy = cy0; cy <= cy1; cy++ )
				for ( int cx = cx0; cx <= cx1; cx++ ) {
					at( cx, cy ).roof = std::max( at( cx, cy ).roof, hi[2] );
					const size_t si = size_t( cy - gy0 ) * size_t( W ) + size_t( cx - gx0 );
					if ( !seen[si] ) {
						seen[si] = 1;
						R.boxCells++;
					}
				}
		}
	}

	// --- one probe per cell, hoisted over the roofline at the cell's middle
	double hoistSum = 0;
	for ( int cy = y0; cy <= y1; cy++ )
		for ( int cx = x0; cx <= x1; cx++ ) {
			const CellRec & c = at( cx, cy );
			if ( !c.ok )
				continue;
			R.cells++;
			ProbePoint p;
			p.pos[0] = ( float( cx ) + 0.5f ) * kCell;
			p.pos[1] = ( float( cy ) + 0.5f ) * kCell;
			p.pos[2] = c.roof + spec.hoist;
			p.cellX = cx;
			p.cellY = cy;
			probes.push_back( p );
			hoistSum += double( p.pos[2] ) - double( h( cx, cy, spc / 2, spc / 2 ) );
			R.roofMax = R.probes ? std::max( R.roofMax, c.roof ) : c.roof;
			R.probes++;
		}
	R.hoistMean = R.probes ? hoistSum / R.probes : 0.0;
	if ( out )
		*out = R;
	return true;
}

QString probeFarCensusText( const ProbeFarResult & r )
{
	QString s;
	QTextStream t( &s );
	t << "far: " << r.probes << " probes over " << r.cells << " cells, hoisted " << QString::number( r.hoistMean, 'f', 0 )
	  << " over the ground on average, highest roof " << QString::number( r.roofMax, 'f', 0 ) << "\n";
	t << "far: ground quads " << r.quads << " (colour from the sheet " << r.albedoQuads << "), water quads "
	  << r.waterQuads << ", building boxes " << r.boxes << " in " << r.boxCells << " cells\n";
	t << "far: height samples outside their cell's stored range " << r.heightOutside << "; colour sheet " << r.sheet << "\n";
	return s;
}

int probeFarCli( const QStringList & args )
{
	ProbeFarSpec fs;
	ProbeBakeSpec bs;
	/* Measured on the whole Commonwealth (2026-10-01): surfel cell 512 / one file a cell = 731 MB;
	 * 1024 / 16 x 16 cells a file = 265 MB at irradiance 0.027 / 0.046 against the brute-force
	 * reference; 2048 = 161 MB at 0.053 / 0.094. */
	bs.surfelCell = 1024.0f;
	bs.sector = 16384.0f;
	QString outDir, soupOut, probesOut;
	for ( int i = 0; i < args.size(); i++ ) {
		const QString & a = args[i];
		const QString nx = i + 1 < args.size() ? args[i + 1] : QString();
		if ( a == QLatin1String( "--lodl" ) ) { fs.lodl = nx; i++; }
		else if ( a == QLatin1String( "--lodi" ) ) { fs.lodi = nx; i++; }
		else if ( a == QLatin1String( "--out" ) ) { outDir = nx; i++; }
		else if ( a == QLatin1String( "--soup-out" ) ) { soupOut = nx; i++; }
		else if ( a == QLatin1String( "--probes-out" ) ) { probesOut = nx; i++; }
		else if ( a == QLatin1String( "--step" ) ) { fs.step = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--hoist" ) ) { fs.hoist = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--sheet-dim" ) ) { fs.sheetDim = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--red" ) ) { fs.red = nx; i++; }
		else if ( a == QLatin1String( "--cells" ) ) {
			const QStringList c = nx.split( ',' );
			if ( c.size() == 4 ) {
				fs.region = true;
				fs.x0 = c[0].toInt(); fs.y0 = c[1].toInt(); fs.x1 = c[2].toInt(); fs.y1 = c[3].toInt();
			}
			i++;
		}
		else if ( a == QLatin1String( "--rays" ) ) { bs.rays = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--threads" ) ) { bs.threads = nx.toInt(); i++; }
		else if ( a == QLatin1String( "--sector" ) ) { bs.sector = std::max( 4096.0f, nx.toFloat() ); i++; }
		else if ( a == QLatin1String( "--surfel-cell" ) ) { bs.surfelCell = nx.toFloat(); i++; }
		else if ( a == QLatin1String( "--max-links" ) ) { bs.maxLinks = quint32( qBound( 8, nx.toInt(), 4096 ) ); i++; }
	}
	if ( fs.lodl.isEmpty() || ( outDir.isEmpty() && soupOut.isEmpty() ) ) {
		std::fprintf( stderr, "usage: probefar --lodl <world.lodl> [--lodi <world.lodi>] --out <dir> [--cells x0,y0,x1,y1] "
			"[--step n] [--hoist u] [--sheet-dim n] [--sector u] [--rays n] [--threads n] [--surfel-cell u] [--max-links n] "
			"[--soup-out f.psp] [--probes-out f.txt] [--red shift]\n" );
		return 2;
	}
	ProbeSoup soup;
	std::vector<ProbePoint> probes;
	ProbeFarResult fr;
	if ( !probeFarBuild( fs, soup, probes, &fr ) ) {
		std::fprintf( stderr, "probefar: %s\n", qPrintable( fr.error ) );
		return 1;
	}
	std::fputs( qPrintable( probeFarCensusText( fr ) ), stdout );
	std::fprintf( stdout, "far: soup %lld triangles\n", soup.triCount() );
	QString err;
	if ( !soupOut.isEmpty() && !probeSoupWrite( soupOut, soup, &err ) ) {
		std::fprintf( stderr, "probefar: %s\n", qPrintable( err ) );
		return 1;
	}
	if ( !probesOut.isEmpty() ) {
		if ( FILE * pf = std::fopen( qPrintable( QDir::toNativeSeparators( probesOut ) ), "w" ) ) {
			for ( const ProbePoint & p : probes )
				std::fprintf( pf, "%.4f %.4f %.4f\n", p.pos[0], p.pos[1], p.pos[2] );
			std::fclose( pf );
		}
	}
	if ( outDir.isEmpty() )
		return 0;
	ProbeBakeResult br;
	if ( !probeBake( soup, probes, bs, outDir, &br ) ) {
		std::fprintf( stderr, "probefar: %s\n", qPrintable( br.error ) );
		return 1;
	}
	std::fputs( qPrintable( probeBakeCensusText( br ) ), stdout );
	std::fprintf( stdout, "far: rays %.0f ms, write %.0f ms\n", br.msRays, br.msWrite );
	/* The file's square is not in the `.tbk` header (FO4CS's near bake is always one cell), so the
	 * far folder states it: sector_X_Y.tbk holds the probes with floor(x / sector) = X. */
	QFile man( QDir( outDir ).filePath( QStringLiteral( "far.txt" ) ) );
	if ( man.open( QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text ) ) {
		QTextStream m( &man );
		m << "PRTP far map (NifSkope probefar)\n"
		  << "sector " << bs.sector << "\n"
		  << "surfel_cell " << bs.surfelCell << "\n"
		  << "hoist " << fs.hoist << "\n"
		  << "step " << fs.step << "\n"
		  << "probes " << br.probes << "\n"
		  << "world " << QFileInfo( fs.lodl ).completeBaseName() << "\n";
	}
	return 0;
}
