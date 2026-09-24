// =========================================================================
//  the flow gates  (lane WATER4, pre-registered in its report section 0)
// =========================================================================

namespace {

constexpr double kPiD = 3.14159265358979;

/*! F1, F2, F3, F4, F6 and F7 on SYNTHETIC masks, through WaterFlowGrid alone
 *  -- no file, no body table.  Each is a known-answer control of the method:
 *  continuity in a narrows, parting round an island, a closed lake, a lake
 *  with one outlet, a river's plume into a sea, a dye pin's plume.  The
 *  numbers are the ones the lane report's section 0 registered before any of
 *  this existed. */
void waterFlowGates( const std::function<void( const QString &, bool )> & check,
	const std::function<void( const QString & )> & say )
{
	constexpr double kPi = 3.14159265358979;
	auto wetSolve = [&]( int w, int h, const std::vector<quint8> & wet,
		const std::vector<double> & rhsByTexel, const std::vector<quint8> & dirByTexel,
		WaterFlowGrid & G, std::vector<double> & phi, int & it, double & res ) {
		std::vector<float> k( size_t( w ) * size_t( h ), 1.0f );
		G.build( w, h, wet, k );
		std::vector<double> b( size_t( G.n ), 0.0 );
		std::vector<quint8> d;
		bool anyD = false;
		for ( size_t at = 0; at < wet.size(); at++ ) {
			const int i = G.idx[at];
			if ( i < 0 )
				continue;
			b[size_t( i )] = rhsByTexel.empty() ? 0.0 : rhsByTexel[at];
			if ( !dirByTexel.empty() && dirByTexel[at] )
				anyD = true;
		}
		if ( anyD ) {
			d.assign( size_t( G.n ), 0 );
			for ( size_t at = 0; at < wet.size(); at++ )
				if ( G.idx[at] >= 0 && dirByTexel[at] )
					d[size_t( G.idx[at] )] = 1;
		}
		G.solve( b, d, phi, it, res );
	};
	auto columnFlux = [&]( const WaterFlowGrid & G, const std::vector<double> & F, int x,
		int y0, int y1 ) {
		double s = 0.0;
		for ( int f = 0; f < G.nE; f++ ) {
			const int i = G.fi[size_t( f )];
			if ( G.cx[size_t( i )] == x && G.cy[size_t( i )] >= y0 && G.cy[size_t( i )] < y1 )
				s += F[size_t( f )];
		}
		return s;
	};

	// ---- F1: continuity ---------------------------------------------------
	{
		const int w = 256, h = 32;
		std::vector<quint8> wet( size_t( w ) * h, 0 );
		std::vector<double> rhs( size_t( w ) * h, 0.0 );
		for ( int y = 0; y < h; y++ )
			for ( int x = 0; x < w; x++ )
				wet[size_t( y ) * w + x] = ( x < 128 || ( y >= 8 && y < 24 ) ) ? 1 : 0;
		for ( int y = 0; y < h; y++ ) {
			rhs[size_t( y ) * w + 0] += 1.0 / 32.0;
			if ( y >= 8 && y < 24 )
				rhs[size_t( y ) * w + ( w - 1 )] -= 1.0 / 16.0;
		}
		WaterFlowGrid G;
		std::vector<double> phi, ux, uy, F;
		int it = 0;
		double res = 0.0;
		wetSolve( w, h, wet, rhs, std::vector<quint8>(), G, phi, it, res );
		G.velocity( phi, ux, uy );
		G.faceFlux( phi, F );
		double s64 = 0.0, s192 = 0.0;
		int n64 = 0, n192 = 0;
		for ( int i = 0; i < G.n; i++ ) {
			const double sp = std::hypot( ux[size_t( i )], uy[size_t( i )] );
			if ( G.cx[size_t( i )] == 64 ) {
				s64 += sp;
				n64++;
			}
			if ( G.cx[size_t( i )] == 192 ) {
				s192 += sp;
				n192++;
			}
		}
		s64 /= std::max( 1, n64 );
		s192 /= std::max( 1, n192 );
		const double ratio = s64 > 0.0 ? s192 / s64 : 0.0;
		check( QStringLiteral( "F1 continuity: a channel that narrows to half its width doubles "
			"its speed (x=64 %1, x=192 %2, ratio %3, needs 1.90..2.10; %4 iterations, residual "
			"%5)" ).arg( s64, 0, 'g', 5 ).arg( s192, 0, 'g', 5 ).arg( ratio, 0, 'f', 4 )
			.arg( it ).arg( res, 0, 'g', 2 ), ratio >= 1.90 && ratio <= 2.10 );
		double fmean = 0.0, fl[10];
		for ( int q = 0; q < 10; q++ ) {
			fl[q] = columnFlux( G, F, 16 + 24 * q, 0, h );
			fmean += fl[q];
		}
		fmean /= 10.0;
		double dev = 0.0;
		for ( int q = 0; q < 10; q++ )
			dev = std::max( dev, std::fabs( fl[q] - fmean ) / std::fabs( fmean ) );
		check( QStringLiteral( "F1 flux is the same through 10 cross-sections within 3 percent "
			"(max deviation %1)" ).arg( dev, 0, 'g', 3 ), dev < 0.03 );
	}

	// ---- F2: the island ---------------------------------------------------
	{
		const int w = 256, h = 128;
		const double R = 8.0, cxI = 128.0, cyI = 64.0;
		std::vector<quint8> wet( size_t( w ) * h, 1 );
		std::vector<double> rhs( size_t( w ) * h, 0.0 );
		for ( int y = 0; y < h; y++ )
			for ( int x = 0; x < w; x++ ) {
				const double dx = x + 0.5 - cxI, dy = y + 0.5 - cyI;
				if ( dx * dx + dy * dy < R * R )
					wet[size_t( y ) * w + x] = 0;
			}
		for ( int y = 0; y < h; y++ ) {
			rhs[size_t( y ) * w + 0] += 1.0 / h;
			rhs[size_t( y ) * w + ( w - 1 )] -= 1.0 / h;
		}
		WaterFlowGrid G;
		std::vector<double> phi, ux, uy, F, div;
		int it = 0;
		double res = 0.0;
		wetSolve( w, h, wet, rhs, std::vector<quint8>(), G, phi, it, res );
		G.velocity( phi, ux, uy );
		G.faceFlux( phi, F );
		G.divergence( phi, div );
		const double north = columnFlux( G, F, 128, 64, h ), south = columnFlux( G, F, 128, 0, 64 );
		check( QStringLiteral( "F2 the flow parts round the island and rejoins: north %1 + south "
			"%2 = %3 of the inflow 1 (within 1 percent, halves within 2 percent; %4 iterations, "
			"residual %5)" ).arg( north, 0, 'f', 4 ).arg( south, 0, 'f', 4 )
			.arg( north + south, 0, 'f', 4 ).arg( it ).arg( res, 0, 'g', 2 ),
			std::fabs( north + south - 1.0 ) < 0.01 && std::fabs( north - south ) < 0.02 );
		double worstDiv = 0.0;
		for ( int i = 0; i < G.n; i++ ) {
			const size_t at = size_t( G.cy[size_t( i )] ) * w + size_t( G.cx[size_t( i )] );
			worstDiv = std::max( worstDiv, std::fabs( div[size_t( i )] - rhs[at] ) );
		}
		check( QStringLiteral( "F2 mass balance at every wet cell to 1e-6 of the inflow (worst "
			"%1)" ).arg( worstDiv, 0, 'g', 3 ), worstDiv < 1e-6 );
		double worstNorm = 0.0;
		int bankN = 0;
		double Uinf = 0.0;
		int nInf = 0;
		for ( int i = 0; i < G.n; i++ ) {
			const int x = G.cx[size_t( i )], y = G.cy[size_t( i )];
			const double sp = std::hypot( ux[size_t( i )], uy[size_t( i )] );
			if ( std::fabs( x + 0.5 - cxI ) > 3 * R ) {
				Uinf += sp;
				nInf++;
				if ( y == 0 || y == h - 1 ) {
					worstNorm = std::max( worstNorm, sp > 0.0 ? std::fabs( uy[size_t( i )] ) / sp : 0.0 );
					bankN++;
				}
			}
		}
		Uinf /= std::max( 1, nInf );
		check( QStringLiteral( "F2 straight-bank tangency: the normal component is below sin(1 deg) "
			"at every bank texel (worst %1 over %2 texels)" ).arg( worstNorm, 0, 'g', 3 ).arg( bankN ),
			worstNorm < std::sin( kPi / 180.0 ) );
		double sumD = 0.0, maxD = 0.0;
		int nD = 0, skipped = 0;
		for ( int i = 0; i < G.n; i++ ) {
			const int x = G.cx[size_t( i )], y = G.cy[size_t( i )];
			const double dx = x + 0.5 - cxI, dy = y + 0.5 - cyI;
			if ( std::fabs( dx ) >= 2 * R || std::fabs( dy ) >= 2 * R )
				continue;
			bool bank = false;
			const int nx[4] = { x - 1, x + 1, x, x };
			const int ny[4] = { y, y, y - 1, y + 1 };
			for ( int q = 0; q < 4; q++ )
				if ( nx[q] >= 0 && ny[q] >= 0 && nx[q] < w && ny[q] < h
					&& !wet[size_t( ny[q] ) * w + nx[q]] )
					bank = true;
			if ( !bank )
				continue;
			const double sp = std::hypot( ux[size_t( i )], uy[size_t( i )] );
			if ( sp <= 0.05 * Uinf ) {
				skipped++;
				continue;
			}
			const double r = std::hypot( dx, dy ), th = std::atan2( dy, dx );
			const double ur = Uinf * ( 1.0 - R * R / ( r * r ) ) * std::cos( th );
			const double ut = -Uinf * ( 1.0 + R * R / ( r * r ) ) * std::sin( th );
			const double ax = ur * std::cos( th ) - ut * std::sin( th );
			const double ay = ur * std::sin( th ) + ut * std::cos( th );
			const double am = std::hypot( ax, ay );
			const double c = std::max( -1.0, std::min( 1.0,
				( ux[size_t( i )] * ax + uy[size_t( i )] * ay ) / ( sp * am ) ) );
			const double d = std::acos( c ) * 180.0 / kPi;
			sumD += d;
			maxD = std::max( maxD, d );
			nD++;
		}
		const double meanD = nD ? sumD / nD : 0.0;
		check( QStringLiteral( "F2 island bank direction against the analytic cylinder: mean %1, "
			"max %2 degrees over %3 bank texels (needs mean < 5, max < 15; %4 stagnation texels "
			"skipped)" ).arg( meanD, 0, 'f', 2 ).arg( maxD, 0, 'f', 2 ).arg( nD ).arg( skipped ),
			meanD < 5.0 && maxD < 15.0 );
	}

	// ---- F3 and F4: the lakes --------------------------------------------
	{
		const double R = 32.0;
		const int n = int( 2 * R + 8 );
		const double c = n / 2.0;
		std::vector<quint8> wet( size_t( n ) * n, 0 );
		for ( int y = 0; y < n; y++ )
			for ( int x = 0; x < n; x++ ) {
				const double dx = x + 0.5 - c, dy = y + 0.5 - c;
				wet[size_t( y ) * n + x] = dx * dx + dy * dy < R * R ? 1 : 0;
			}
		{
			WaterFlowGrid G;
			std::vector<double> phi, ux, uy;
			int it = 0;
			double res = 0.0;
			wetSolve( n, n, wet, std::vector<double>(), std::vector<quint8>(), G, phi, it, res );
			G.velocity( phi, ux, uy );
			double mx = 0.0;
			for ( int i = 0; i < G.n; i++ )
				mx = std::max( mx, std::hypot( ux[size_t( i )], uy[size_t( i )] ) );
			check( QStringLiteral( "F3 a lake with no outlet and no mark: max speed = 0 exactly "
				"(%1, %2 iterations)" ).arg( mx, 0, 'g', 3 ).arg( it ), mx == 0.0 );
		}
		{
			std::vector<quint8> outlet( size_t( n ) * n, 0 );
			std::vector<double> rhs( size_t( n ) * n, 0.0 );
			int nOut = 0;
			for ( int y = int( c ) - 1; y <= int( c ) + 1; y++ ) {
				int xm = -1;
				for ( int x = 0; x < n; x++ )
					if ( wet[size_t( y ) * n + x] )
						xm = x;
				if ( xm >= 0 ) {
					outlet[size_t( y ) * n + xm] = 1;
					nOut++;
				}
			}
			int nWet = 0;
			for ( size_t at = 0; at < wet.size(); at++ )
				if ( wet[at] )
					nWet++;
			for ( size_t at = 0; at < wet.size(); at++ ) {
				if ( wet[at] )
					rhs[at] += 1.0 / nWet;
				if ( outlet[at] )
					rhs[at] -= 1.0 / nOut;
			}
			WaterFlowGrid G;
			std::vector<double> phi, ux, uy;
			int it = 0;
			double res = 0.0;
			wetSolve( n, n, wet, rhs, std::vector<quint8>(), G, phi, it, res );
			G.velocity( phi, ux, uy );
			const double ox = c + R, oy = c;
			int away = 0, inner = 0;
			double cosSum = 0.0;
			for ( int i = 0; i < G.n; i++ ) {
				const size_t at = size_t( G.cy[size_t( i )] ) * n + size_t( G.cx[size_t( i )] );
				if ( outlet[at] )
					continue;
				const double tx = ox - ( G.cx[size_t( i )] + 0.5 ), ty = oy - ( G.cy[size_t( i )] + 0.5 );
				const double sp = std::hypot( ux[size_t( i )], uy[size_t( i )] ), tm = std::hypot( tx, ty );
				const double d = sp > 0.0 && tm > 0.0
					? ( ux[size_t( i )] * tx + uy[size_t( i )] * ty ) / ( sp * tm ) : 0.0;
				if ( d < 0.0 )
					away++;
				cosSum += d;
				inner++;
			}
			check( QStringLiteral( "F4 a lake with one outlet: 0 texels point away from it (%1 of %2 "
				"do; mean cosine %3; %4 iterations, residual %5)" ).arg( away ).arg( inner )
				.arg( inner ? cosSum / inner : 0.0, 0, 'f', 3 ).arg( it ).arg( res, 0, 'g', 2 ),
				away == 0 );
			int seeds = 0, reached = 0;
			for ( int sy = 0; sy < 8; sy++ )
				for ( int sx = 0; sx < 8; sx++ ) {
					const int x = int( 4 + ( n - 8 ) * sx / 7.0 ), y = int( 4 + ( n - 8 ) * sy / 7.0 );
					if ( !wet[size_t( y ) * n + x] )
						continue;
					seeds++;
					if ( G.trace( phi, x + 0.5, y + 0.5, outlet, int( 4 * 2 * R * 4 ) ) )
						reached++;
				}
			check( QStringLiteral( "F4 every streamline reaches the outlet (%1 of %2 seeds, within 4 "
				"diameters)" ).arg( reached ).arg( seeds ), seeds > 0 && reached == seeds );
		}
	}

	// ---- F7: the dye pin --------------------------------------------------
	{
		const int w = 256, h = 16;
		std::vector<quint8> wet( size_t( w ) * h, 1 );
		std::vector<double> rhs( size_t( w ) * h, 0.0 );
		for ( int y = 0; y < h; y++ ) {
			rhs[size_t( y ) * w + 0] += 1.0 / h;
			rhs[size_t( y ) * w + ( w - 1 )] -= 1.0 / h;
		}
		WaterFlowGrid G;
		std::vector<double> phi, held, c;
		int it = 0;
		double res = 0.0;
		wetSolve( w, h, wet, rhs, std::vector<quint8>(), G, phi, it, res );
		held.assign( size_t( G.n ), -1.0 );
		for ( int y = 0; y < h; y++ )
			held[size_t( G.idx[size_t( y ) * w + 32] )] = 1.0;
		G.dye( phi, held, 32.0, c );
		double c64 = 0.0, c128 = 0.0, up = 0.0;
		for ( int i = 0; i < G.n; i++ ) {
			const int x = G.cx[size_t( i )];
			if ( x == 64 )
				c64 += c[size_t( i )] / h;
			if ( x == 128 )
				c128 += c[size_t( i )] / h;
			if ( x < 32 )
				up = std::max( up, c[size_t( i )] );
		}
		check( QStringLiteral( "F7 a dye pin's weight one half-distance downstream is 1/2 (%1)" )
			.arg( c64, 0, 'f', 4 ), std::fabs( c64 - 0.5 ) < 0.05 );
		check( QStringLiteral( "F7 upstream of the pin the weight is 0 (max %1)" ).arg( up, 0, 'g', 3 ),
			up == 0.0 );
		check( QStringLiteral( "F7 three half-distances downstream it is 1/8 (%1)" )
			.arg( c128, 0, 'f', 4 ), std::fabs( c128 - 0.125 ) < 0.0125 );
	}

	// ---- F6: the plume ----------------------------------------------------
	{
		const int W = 224, H = 128;
		std::vector<quint8> sea( size_t( W ) * H, 0 ), cut( size_t( W ) * H, 0 );
		std::vector<double> rhs( size_t( W ) * H, 0.0 );
		for ( int y = 0; y < H; y++ )
			for ( int x = 96; x < W; x++ ) {
				sea[size_t( y ) * W + x] = 1;
				if ( y == 0 || y == H - 1 || x == W - 1 )
					cut[size_t( y ) * W + x] = 1;
			}
		for ( int y = 56; y < 72; y++ )
			rhs[size_t( y ) * W + 96] += 1.0 / 16.0;
		WaterFlowGrid G;
		std::vector<double> phi, ux, uy, held, c;
		int it = 0;
		double res = 0.0;
		wetSolve( W, H, sea, rhs, cut, G, phi, it, res );
		G.velocity( phi, ux, uy );
		double mx = 0.0, my = 0.0;
		for ( int i = 0; i < G.n; i++ ) {
			const double dx = G.cx[size_t( i )] + 0.5 - 96.0, dy = G.cy[size_t( i )] + 0.5 - 64.0;
			if ( std::hypot( dx, dy ) < 32.0 ) {
				mx += ux[size_t( i )];
				my += uy[size_t( i )];
			}
		}
		const double predDir = std::atan2( my, mx ) * 180.0 / kPi, predLen = 96.0;
		say( QStringLiteral( "F6 predicted from the flow before the dye: plume direction %1 degrees, "
			"1/8 length %2 texels" ).arg( predDir, 0, 'f', 1 ).arg( predLen, 0, 'f', 0 ) );
		held.assign( size_t( G.n ), -1.0 );
		for ( int y = 56; y < 72; y++ )
			held[size_t( G.idx[size_t( y ) * W + 96] )] = 1.0;
		G.dye( phi, held, 32.0, c );
		double measLen = -1.0;
		for ( int x = 97; x < W; x++ ) {
			const double c0 = c[size_t( G.idx[size_t( 64 ) * W + x - 1] )];
			const double c1 = c[size_t( G.idx[size_t( 64 ) * W + x] )];
			if ( c1 < 0.125 ) {
				const double f = ( c0 - 0.125 ) / ( c0 - c1 );
				measLen = ( x - 1 - 96 ) + f + 0.5;
				break;
			}
		}
		double tot = 0.0, cxs = 0.0, cys = 0.0;
		for ( int i = 0; i < G.n; i++ ) {
			tot += c[size_t( i )];
			cxs += c[size_t( i )] * ( G.cx[size_t( i )] + 0.5 );
			cys += c[size_t( i )] * ( G.cy[size_t( i )] + 0.5 );
		}
		const double measDir = tot > 0.0 ? std::atan2( cys / tot - 64.0, cxs / tot - 96.0 ) * 180.0 / kPi : 999.0;
		double dd = std::fabs( measDir - predDir );
		if ( dd > 180.0 )
			dd = 360.0 - dd;
		check( QStringLiteral( "F6 the plume's 1/8 length measured after the dye is within 10 percent "
			"of the prediction (%1 against %2 texels; %3 iterations, residual %4)" )
			.arg( measLen, 0, 'f', 1 ).arg( predLen, 0, 'f', 0 ).arg( it ).arg( res, 0, 'g', 2 ),
			measLen > 0.0 && std::fabs( measLen - predLen ) <= 0.1 * predLen );
		check( QStringLiteral( "F6 the plume's direction (its weight centroid, %1 degrees) is within "
			"9 degrees of the prediction (%2)" ).arg( measDir, 0, 'f', 1 ).arg( predDir, 0, 'f', 1 ),
			dd <= 9.0 );
	}
}

/*! F5's instrument, in C++: the spatial structure of the flow direction over
 *  one body as this document would WRITE it.  Seam-bounded constant-direction
 *  patches (>= 64 texels, the pre-registered definition), the 99th percentile
 *  of the angle difference between 4-adjacent wet texels, and the seam
 *  fraction.  The same numbers scratchpad/water4_20260910/disc_metric.py
 *  reads back out of the file. */
struct FlowStructure
{
	qint64 wet = 0, pairs = 0;
	double p99 = 0.0, seamFraction = 0.0;
	int patches = 0;
	double patchMaxRadius = 0.0;
};

FlowStructure flowStructure( const WaterMarkDoc & doc, const LodtWaterBody & b )
{
	FlowStructure out;
	int px0 = 0, py0 = 0, px1 = 0, py1 = 0;
	doc.worldToTexel( double( b.x0 ) * 4096.0, double( b.y0 ) * 4096.0, px0, py0 );
	doc.worldToTexel( ( double( b.x1 ) + 1.0 ) * 4096.0, ( double( b.y1 ) + 1.0 ) * 4096.0, px1, py1 );
	const int W = px1 - px0 + 1, H = py1 - py0 + 1;
	if ( W <= 0 || H <= 0 )
		return out;
	std::vector<qint16> dir( size_t( W ) * size_t( H ), -1 );
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ ) {
			double wx = 0, wy = 0;
			doc.texelToWorld( px0 + x, py0 + y, wx, wy );
			if ( doc.bodyAtWorld( wx, wy ) != b.id )
				continue;
			dir[size_t( y ) * W + x] = qint16( doc.flowWordAt( px0 + x, py0 + y ) & 0xFF );
			out.wet++;
		}
	auto adiff = [&]( int a, int c ) {
		int d = std::abs( a - c );
		d = std::min( d, 256 - d );
		return d * 360.0 / 256.0;
	};
	std::vector<double> diffs;
	qint64 seams = 0;
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ ) {
			const int a = dir[size_t( y ) * W + x];
			if ( a < 0 )
				continue;
			if ( x + 1 < W && dir[size_t( y ) * W + x + 1] >= 0 ) {
				const double d = adiff( a, dir[size_t( y ) * W + x + 1] );
				diffs.push_back( d );
				if ( d > 10.0 )
					seams++;
			}
			if ( y + 1 < H && dir[size_t( y + 1 ) * W + x] >= 0 ) {
				const double d = adiff( a, dir[size_t( y + 1 ) * W + x] );
				diffs.push_back( d );
				if ( d > 10.0 )
					seams++;
			}
		}
	out.pairs = qint64( diffs.size() );
	if ( !diffs.empty() ) {
		std::sort( diffs.begin(), diffs.end() );
		out.p99 = diffs[size_t( std::min( diffs.size() - 1, size_t( diffs.size() * 0.99 ) ) )];
		out.seamFraction = double( seams ) / double( diffs.size() );
	}
	// components of identical direction, 4-connected, by union-find
	std::vector<int> parent( size_t( W ) * size_t( H ), 0 );
	for ( size_t i = 0; i < parent.size(); i++ )
		parent[i] = int( i );
	std::function<int( int )> find = [&]( int i ) {
		while ( parent[size_t( i )] != i ) {
			parent[size_t( i )] = parent[size_t( parent[size_t( i )] )];
			i = parent[size_t( i )];
		}
		return i;
	};
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ ) {
			const int i = y * W + x;
			if ( dir[size_t( i )] < 0 )
				continue;
			if ( x + 1 < W && dir[size_t( i ) + 1] == dir[size_t( i )] )
				parent[size_t( find( i ) )] = find( i + 1 );
			if ( y + 1 < H && dir[size_t( i ) + W] == dir[size_t( i )] )
				parent[size_t( find( i ) )] = find( i + W );
		}
	std::vector<int> area( parent.size(), 0 ), bnd( parent.size(), 0 ), seamy( parent.size(), 0 );
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ ) {
			const int i = y * W + x;
			if ( dir[size_t( i )] < 0 )
				continue;
			const int r = find( i );
			area[size_t( r )]++;
			const int nx[4] = { x - 1, x + 1, x, x }, ny[4] = { y, y, y - 1, y + 1 };
			for ( int q = 0; q < 4; q++ ) {
				if ( nx[q] < 0 || ny[q] < 0 || nx[q] >= W || ny[q] >= H )
					continue;
				const int j = ny[q] * W + nx[q];
				if ( dir[size_t( j )] < 0 || find( j ) == r )
					continue;
				bnd[size_t( r )]++;
				if ( adiff( dir[size_t( i )], dir[size_t( j )] ) > 5.0 )
					seamy[size_t( r )]++;
			}
		}
	for ( size_t r = 0; r < area.size(); r++ ) {
		if ( area[r] < 64 || bnd[r] == 0 )
			continue;
		if ( double( seamy[r] ) / double( bnd[r] ) > 0.5 ) {
			out.patches++;
			out.patchMaxRadius = std::max( out.patchMaxRadius, std::sqrt( area[r] / kPiD ) );
		}
	}
	return out;
}

} // namespace

