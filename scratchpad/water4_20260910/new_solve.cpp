// =========================================================================
//  the solver core  (lane WATER4; the method is stated in watermark.h)
// =========================================================================

void WaterFlowGrid::build( int width, int height, const std::vector<quint8> & wet,
	const std::vector<float> & k )
{
	w = width;
	h = height;
	idx.assign( size_t( w ) * size_t( h ), -1 );
	cx.clear();
	cy.clear();
	fi.clear();
	fj.clear();
	kf.clear();
	n = 0;
	for ( int y = 0; y < h; y++ )
		for ( int x = 0; x < w; x++ ) {
			const size_t at = size_t( y ) * size_t( w ) + size_t( x );
			if ( !wet[at] )
				continue;
			idx[at] = n++;
			cx.push_back( x );
			cy.push_back( y );
		}
	auto kAt = [&]( int x, int y ) -> double {
		const size_t at = size_t( y ) * size_t( w ) + size_t( x );
		const double v = at < k.size() ? double( k[at] ) : 1.0;
		return v > 1e-9 ? v : 1e-9;
	};
	// east faces first, then north faces: velocity() relies on the order
	for ( int y = 0; y < h; y++ )
		for ( int x = 0; x + 1 < w; x++ ) {
			const int a = idx[size_t( y ) * size_t( w ) + size_t( x )];
			const int b = idx[size_t( y ) * size_t( w ) + size_t( x ) + 1];
			if ( a < 0 || b < 0 )
				continue;
			fi.push_back( a );
			fj.push_back( b );
			const double ka = kAt( x, y ), kb = kAt( x + 1, y );
			kf.push_back( 2.0 * ka * kb / ( ka + kb ) );
		}
	nE = int( fi.size() );
	for ( int y = 0; y + 1 < h; y++ )
		for ( int x = 0; x < w; x++ ) {
			const int a = idx[size_t( y ) * size_t( w ) + size_t( x )];
			const int b = idx[size_t( y + 1 ) * size_t( w ) + size_t( x )];
			if ( a < 0 || b < 0 )
				continue;
			fi.push_back( a );
			fj.push_back( b );
			const double ka = kAt( x, y ), kb = kAt( x, y + 1 );
			kf.push_back( 2.0 * ka * kb / ( ka + kb ) );
		}
	diag.assign( size_t( n ), 0.0 );
	for ( size_t f = 0; f < fi.size(); f++ ) {
		diag[size_t( fi[f] )] += kf[f];
		diag[size_t( fj[f] )] += kf[f];
	}
}

bool WaterFlowGrid::solve( const std::vector<double> & bIn, const std::vector<quint8> & dirichlet,
	std::vector<double> & phi, int & iterations, double & residual, double tol, int cap ) const
{
	iterations = 0;
	residual = 0.0;
	phi.assign( size_t( n ), 0.0 );
	if ( n <= 0 )
		return false;
	const bool haveD = !dirichlet.empty();
	std::vector<double> b( bIn );
	b.resize( size_t( n ), 0.0 );
	bool anyD = false;
	if ( haveD )
		for ( int i = 0; i < n; i++ )
			if ( dirichlet[size_t( i )] ) {
				b[size_t( i )] = 0.0;
				anyD = true;
			}
	if ( !anyD ) {
		// the pure-Neumann system: consistent only when the sources balance
		double mean = 0.0;
		for ( int i = 0; i < n; i++ )
			mean += b[size_t( i )];
		mean /= double( n );
		for ( int i = 0; i < n; i++ )
			b[size_t( i )] -= mean;
	}
	double nb = 0.0;
	for ( int i = 0; i < n; i++ )
		nb += b[size_t( i )] * b[size_t( i )];
	nb = std::sqrt( nb );
	if ( nb <= 0.0 )
		return true;                       // nothing drives it: phi = 0, u = 0
	auto matvec = [&]( const std::vector<double> & x, std::vector<double> & y ) {
		for ( int i = 0; i < n; i++ )
			y[size_t( i )] = diag[size_t( i )] * x[size_t( i )];
		for ( size_t f = 0; f < fi.size(); f++ ) {
			const size_t a = size_t( fi[f] ), c = size_t( fj[f] );
			y[a] -= kf[f] * x[c];
			y[c] -= kf[f] * x[a];
		}
		if ( anyD )
			for ( int i = 0; i < n; i++ )
				if ( dirichlet[size_t( i )] )
					y[size_t( i )] = diag[size_t( i )] * x[size_t( i )];
	};
	std::vector<double> r( b ), z( size_t( n ), 0.0 ), p( size_t( n ), 0.0 ), Ap( size_t( n ), 0.0 );
	double rz = 0.0;
	for ( int i = 0; i < n; i++ ) {
		z[size_t( i )] = r[size_t( i )] / diag[size_t( i )];
		p[size_t( i )] = z[size_t( i )];
		rz += r[size_t( i )] * z[size_t( i )];
	}
	double res = 1.0;
	int it = 0;
	while ( it < cap && res > tol ) {
		matvec( p, Ap );
		double pAp = 0.0;
		for ( int i = 0; i < n; i++ )
			pAp += p[size_t( i )] * Ap[size_t( i )];
		if ( !( std::fabs( pAp ) > 0.0 ) )
			break;
		const double alpha = rz / pAp;
		double rr = 0.0;
		for ( int i = 0; i < n; i++ ) {
			phi[size_t( i )] += alpha * p[size_t( i )];
			r[size_t( i )] -= alpha * Ap[size_t( i )];
			rr += r[size_t( i )] * r[size_t( i )];
		}
		if ( !anyD ) {
			double mean = 0.0;
			for ( int i = 0; i < n; i++ )
				mean += phi[size_t( i )];
			mean /= double( n );
			for ( int i = 0; i < n; i++ )
				phi[size_t( i )] -= mean;
		}
		double rz2 = 0.0;
		for ( int i = 0; i < n; i++ ) {
			z[size_t( i )] = r[size_t( i )] / diag[size_t( i )];
			rz2 += r[size_t( i )] * z[size_t( i )];
		}
		const double beta = rz2 / rz;
		for ( int i = 0; i < n; i++ )
			p[size_t( i )] = z[size_t( i )] + beta * p[size_t( i )];
		rz = rz2;
		it++;
		res = std::sqrt( rr ) / nb;
	}
	iterations = it;
	residual = res;
	return true;
}

void WaterFlowGrid::faceFlux( const std::vector<double> & phi, std::vector<double> & F ) const
{
	F.assign( fi.size(), 0.0 );
	for ( size_t f = 0; f < fi.size(); f++ )
		F[f] = -kf[f] * ( phi[size_t( fj[f] )] - phi[size_t( fi[f] )] );
}

void WaterFlowGrid::velocity( const std::vector<double> & phi, std::vector<double> & ux,
	std::vector<double> & uy ) const
{
	std::vector<double> F;
	faceFlux( phi, F );
	ux.assign( size_t( n ), 0.0 );
	uy.assign( size_t( n ), 0.0 );
	for ( size_t f = 0; f < fi.size(); f++ ) {
		const double v = 0.5 * F[f] / kf[f];
		if ( int( f ) < nE ) {
			ux[size_t( fi[f] )] += v;
			ux[size_t( fj[f] )] += v;
		} else {
			uy[size_t( fi[f] )] += v;
			uy[size_t( fj[f] )] += v;
		}
	}
}

void WaterFlowGrid::divergence( const std::vector<double> & phi, std::vector<double> & div ) const
{
	std::vector<double> F;
	faceFlux( phi, F );
	div.assign( size_t( n ), 0.0 );
	for ( size_t f = 0; f < fi.size(); f++ ) {
		div[size_t( fi[f] )] += F[f];
		div[size_t( fj[f] )] -= F[f];
	}
}

void WaterFlowGrid::dye( const std::vector<double> & phi, const std::vector<double> & held,
	double halfTexels, std::vector<double> & c ) const
{
	c.assign( size_t( n ), 0.0 );
	if ( n <= 0 )
		return;
	std::vector<double> F, ux, uy;
	faceFlux( phi, F );
	velocity( phi, ux, uy );
	// inflow lists per destination cell: (face, upwind cell, |flux|)
	std::vector<int> head( size_t( n ), -1 ), next( fi.size(), -1 ), from( fi.size(), -1 );
	for ( size_t f = 0; f < fi.size(); f++ ) {
		if ( F[f] == 0.0 )
			continue;
		const int src = F[f] > 0.0 ? fi[f] : fj[f];
		const int dst = F[f] > 0.0 ? fj[f] : fi[f];
		from[f] = src;
		next[f] = head[size_t( dst )];
		head[size_t( dst )] = int( f );
	}
	std::vector<int> order( size_t( n ), 0 );
	for ( int i = 0; i < n; i++ )
		order[size_t( i )] = i;
	std::stable_sort( order.begin(), order.end(), [&]( int a, int b ) {
		return phi[size_t( a )] > phi[size_t( b )];
	} );
	const double L = halfTexels > 1e-6 ? halfTexels : 1e-6;
	for ( int i : order ) {
		if ( i < int( held.size() ) && held[size_t( i )] >= 0.0 ) {
			c[size_t( i )] = held[size_t( i )];
			continue;
		}
		double acc = 0.0, tot = 0.0;
		for ( int f = head[size_t( i )]; f >= 0; f = next[size_t( f )] ) {
			const double m = std::fabs( F[size_t( f )] );
			acc += m * c[size_t( from[size_t( f )] )];
			tot += m;
		}
		if ( tot <= 0.0 )
			continue;
		const double sp = std::hypot( ux[size_t( i )], uy[size_t( i )] );
		// the mean chord through a unit cell along the flow: 1 / (|cos| + |sin|)
		const double chord = sp > 0.0
			? sp / ( std::fabs( ux[size_t( i )] ) + std::fabs( uy[size_t( i )] ) ) : 1.0;
		c[size_t( i )] = acc / tot * std::pow( 0.5, chord / L );
	}
}

bool WaterFlowGrid::trace( const std::vector<double> & phi, double x, double y,
	const std::vector<quint8> & stop, int maxCells ) const
{
	std::vector<double> F;
	faceFlux( phi, F );
	std::vector<double> vW( size_t( n ), 0.0 ), vE( size_t( n ), 0.0 ),
		vS( size_t( n ), 0.0 ), vN( size_t( n ), 0.0 );
	for ( size_t f = 0; f < fi.size(); f++ ) {
		const double v = F[f] / kf[f];
		if ( int( f ) < nE ) {
			vE[size_t( fi[f] )] = v;
			vW[size_t( fj[f] )] = v;
		} else {
			vN[size_t( fi[f] )] = v;
			vS[size_t( fj[f] )] = v;
		}
	}
	int ix = int( std::floor( x ) ), iy = int( std::floor( y ) );
	double fx = x - ix, fy = y - iy;
	const double inf = std::numeric_limits<double>::infinity();
	auto exitTime = [&]( double f, double v0, double v1, double & target ) -> double {
		const double v = v0 + ( v1 - v0 ) * f;
		if ( v > 0.0 )
			target = 1.0;
		else if ( v < 0.0 )
			target = 0.0;
		else
			return inf;
		const double a = v1 - v0;
		const double vt = v0 + a * target;
		if ( vt * v <= 0.0 )
			return inf;                    // the velocity turns before the face
		if ( std::fabs( a ) < 1e-14 )
			return ( target - f ) / v;
		return std::log( vt / v ) / a;
	};
	auto advance = [&]( double f, double v0, double v1, double t ) -> double {
		const double a = v1 - v0;
		if ( std::fabs( a ) < 1e-14 )
			return f + v0 * t;
		return f + ( v0 + a * f ) * ( std::exp( a * t ) - 1.0 ) / a;
	};
	for ( int step = 0; step < maxCells; step++ ) {
		if ( ix < 0 || iy < 0 || ix >= w || iy >= h )
			return false;
		const size_t at = size_t( iy ) * size_t( w ) + size_t( ix );
		const int i = idx[at];
		if ( i < 0 )
			return false;
		if ( at < stop.size() && stop[at] )
			return true;
		double gx = fx, gy = fy;
		const double tx = exitTime( fx, vW[size_t( i )], vE[size_t( i )], gx );
		const double ty = exitTime( fy, vS[size_t( i )], vN[size_t( i )], gy );
		if ( tx == inf && ty == inf )
			return false;                  // a sink or a stagnation point
		const double t = std::min( tx, ty );
		if ( tx <= ty ) {
			fy = std::min( 1.0, std::max( 0.0, advance( fy, vS[size_t( i )], vN[size_t( i )], t ) ) );
			ix += gx == 1.0 ? 1 : -1;
			fx = gx == 1.0 ? 0.0 : 1.0;
		} else {
			fx = std::min( 1.0, std::max( 0.0, advance( fx, vW[size_t( i )], vE[size_t( i )], t ) ) );
			iy += gy == 1.0 ? 1 : -1;
			fy = gy == 1.0 ? 0.0 : 1.0;
		}
	}
	return false;
}

// =========================================================================
//  the solve  (spec_water.md 4.3, as rebuilt by lane WATER4)
// =========================================================================

namespace {

//! Conductance boost inside a stroke's half-width: the soft preference.
constexpr double kStrokeBoost = 4.0;
//! The depth floor, world units: a texel is never a perfect insulator.
constexpr double kDepthFloor = 8.0;
//! A body whose bounding box exceeds this many texels is solved in a WINDOW.
constexpr qint64 kWindowMax = qint64( 1 ) << 20;
//! The window's margin around its constraints, texels (8 cells at 32).
constexpr int kWindowMargin = 256;
//! How far a contact with another body may be, texels (the writer's `near`).
constexpr int kContactReach = 64;

//! A chamfer distance (3-4, thirds of a texel) from `seed` over a grid.
void chamfer( int w, int h, const std::vector<quint8> & seed, std::vector<float> & dist )
{
	const size_t n = size_t( w ) * size_t( h );
	dist.assign( n, 1e9f );
	for ( size_t i = 0; i < n; i++ )
		if ( seed[i] )
			dist[i] = 0.0f;
	auto relax = [&]( int x, int y, int nx, int ny, float wgt ) {
		if ( nx < 0 || ny < 0 || nx >= w || ny >= h )
			return;
		const size_t at = size_t( y ) * size_t( w ) + size_t( x );
		const size_t nat = size_t( ny ) * size_t( w ) + size_t( nx );
		dist[at] = std::min( dist[at], dist[nat] + wgt );
	};
	for ( int y = 0; y < h; y++ )
		for ( int x = 0; x < w; x++ ) {
			relax( x, y, x - 1, y, 1.0f );
			relax( x, y, x, y - 1, 1.0f );
			relax( x, y, x - 1, y - 1, 1.41421f );
			relax( x, y, x + 1, y - 1, 1.41421f );
		}
	for ( int y = h - 1; y >= 0; y-- )
		for ( int x = w - 1; x >= 0; x-- ) {
			relax( x, y, x + 1, y, 1.0f );
			relax( x, y, x, y + 1, 1.0f );
			relax( x, y, x + 1, y + 1, 1.41421f );
			relax( x, y, x - 1, y + 1, 1.41421f );
		}
}

} // namespace

/*! One body's field.
 *
 *  THE STROKE IS NOT A HELD DISC ANY MORE.  Lane WATER3's fill wrote every
 *  segment's tangent over a capsule of the stroke's half-width and solved
 *  only the slivers in between; on the Charles that tiled the river with 39
 *  discs of radius 12-15 texels and 20-45 degree seams (lane WATER4's report,
 *  section 1), which is what bungo saw.  Here the stroke does three things,
 *  none of them a held value: its ends are where the water enters and leaves
 *  when nothing else says so (a pin, or the table's own outlet / source
 *  contact); its path raises the conductance underneath it (x4), so the flow
 *  prefers the channel the user drew where a body offers more than one; and
 *  its tangent is compared with the solved flow underneath it -- if the two
 *  disagree over the stroke's length, the stroke wins over the contacts and
 *  the solve is re-run with the stroke's ends alone. */
bool WaterMarkDoc::solveBody( int id, WaterMarkSolve & st, QString * note )
{
	LodtWaterBody b;
	if ( !body( id, b ) )
		return false;
	auto sayNote = [&]( const QString & m ) {
		if ( note && note->isEmpty() )
			*note = m;
	};
	const double u = worldPerTexel();
	// ---- the body's bounding box, in body-plane texels ------------------
	const int bx0 = ( int( b.x0 ) - lodl->cellMinX() ) * int( idRate );
	const int by0 = ( int( b.y0 ) - lodl->cellMinY() ) * int( idRate );
	const int bw = ( int( b.x1 ) - int( b.x0 ) + 1 ) * int( idRate );
	const int bh = ( int( b.y1 ) - int( b.y0 ) + 1 ) * int( idRate );
	if ( bw <= 0 || bh <= 0 )
		return false;

	if ( lockZero[id - 1] ) {
		/* bungo's rule, taken literally: a lake nobody connected to a river
		 * has NO flow, and that beats the form's NAM0 -- which is the only
		 * reason a still lake had a velocity at all.  No arrays: the word is
		 * 0 wherever the body is. */
		Field * F = new Field();
		F->zero = true;
		fields.insert( id, F );
		st.bodiesSolved++;
		return true;
	}

	// ---- the constraints, in world units --------------------------------
	struct Seg { double ax, ay, bx, by, halfW; };
	QVector<Seg> segs;
	QVector<QPointF> srcPins, snkPins, firsts, lasts;
	double sumSpeed = 0.0;
	int nSpeed = 0;
	double maxWidth = 0.0;
	double cx0 = 1e30, cy0 = 1e30, cx1 = -1e30, cy1 = -1e30;   // constraint bbox, world
	auto grow = [&]( double x, double y, double r ) {
		cx0 = std::min( cx0, x - r );
		cy0 = std::min( cy0, y - r );
		cx1 = std::max( cx1, x + r );
		cy1 = std::max( cy1, y + r );
	};
	for ( const WaterStroke & s : marks ) {
		if ( !s.enabled() || int( s.body ) != id || s.pts.isEmpty() )
			continue;
		const double halfW = double( s.width ) * 0.5 > 0.0 ? double( s.width ) * 0.5 : u;
		if ( s.kind == WaterStroke::Stroke || s.kind == WaterStroke::Pin ) {
			if ( s.pts.size() < 2 )
				continue;
			maxWidth = std::max( maxWidth, double( s.width ) );
			for ( int k = 0; k + 1 < s.pts.size(); k++ ) {
				segs.append( Seg{ double( s.pts[k].x ), double( s.pts[k].y ),
					double( s.pts[k + 1].x ), double( s.pts[k + 1].y ), halfW } );
				grow( s.pts[k].x, s.pts[k].y, halfW );
				grow( s.pts[k + 1].x, s.pts[k + 1].y, halfW );
			}
			firsts.append( QPointF( s.pts.first().x, s.pts.first().y ) );
			lasts.append( QPointF( s.pts.last().x, s.pts.last().y ) );
			if ( s.flags & WaterStroke::SetsSpeed ) {
				sumSpeed += double( s.speed );
				nSpeed++;
			}
			st.strokes++;
		} else if ( s.kind == WaterStroke::SourcePin ) {
			srcPins.append( QPointF( s.pts.first().x, s.pts.first().y ) );
			grow( s.pts.first().x, s.pts.first().y, halfW );
			st.strokes++;
		} else if ( s.kind == WaterStroke::OutletPin ) {
			snkPins.append( QPointF( s.pts.first().x, s.pts.first().y ) );
			grow( s.pts.first().x, s.pts.first().y, halfW );
			st.strokes++;
		} else if ( s.kind == WaterStroke::DyePin || s.kind == WaterStroke::DyeMouth ) {
			grow( s.pts.first().x, s.pts.first().y, halfW );
		}
	}

	// ---- the window: the whole bbox, or a cut around the constraints ----
	Field * F = new Field();
	F->px0 = bx0;
	F->py0 = by0;
	F->w = bw;
	F->h = bh;
	if ( qint64( bw ) * qint64( bh ) > kWindowMax ) {
		if ( !( cx1 >= cx0 ) ) {
			delete F;
			sayNote( QStringLiteral( "body %1 spans %2 x %3 texels and carries no mark to "
				"solve around" ).arg( id ).arg( bw ).arg( bh ) );
			return false;
		}
		int wx0 = 0, wy0 = 0, wx1 = 0, wy1 = 0;
		worldToTexel( cx0, cy0, wx0, wy0 );
		worldToTexel( cx1, cy1, wx1, wy1 );
		wx0 = std::max( bx0, wx0 - kWindowMargin );
		wy0 = std::max( by0, wy0 - kWindowMargin );
		wx1 = std::min( bx0 + bw - 1, wx1 + kWindowMargin );
		wy1 = std::min( by0 + bh - 1, wy1 + kWindowMargin );
		F->px0 = wx0;
		F->py0 = wy0;
		F->w = wx1 - wx0 + 1;
		F->h = wy1 - wy0 + 1;
		F->windowed = true;
	}
	const int W = F->w, H = F->h;
	const size_t n = size_t( W ) * size_t( H );
	F->mask.assign( n, 0 );
	F->vx.assign( n, 0.0f );
	F->vy.assign( n, 0.0f );
	F->conf.assign( n, 0 );
	F->speed.assign( n, 8 );
	F->dye.assign( n, 0 );
	std::vector<quint8> cut( n, 0 );
	size_t wetCount = 0;
	for ( int y = 0; y < H; y++ )
		for ( int x = 0; x < W; x++ ) {
			const size_t at = size_t( y ) * size_t( W ) + size_t( x );
			if ( idAtTexel( F->px0 + x, F->py0 + y ) != quint16( id ) )
				continue;
			F->mask[at] = 1;
			wetCount++;
			if ( F->windowed && ( x == 0 || y == 0 || x == W - 1 || y == H - 1 ) ) {
				// a wet texel on the window's edge whose neighbour beyond is the same body
				const int ox = x == 0 ? -1 : ( x == W - 1 ? 1 : 0 );
				const int oy = y == 0 ? -1 : ( y == H - 1 ? 1 : 0 );
				if ( idAtTexel( F->px0 + x + ox, F->py0 + y + oy ) == quint16( id ) )
					cut[at] = 1;
			}
		}
	if ( !wetCount ) {
		delete F;
		return false;
	}

	// ---- the conductance: the water depth, floored ----------------------
	std::vector<float> k( n, 1.0f );
	{
		const int spc = lodl->samplesPerCell();
		const int step = idRate > 0 ? std::max( 1, spc / int( idRate ) ) : 1;
		for ( int y = 0; y < H; y++ )
			for ( int x = 0; x < W; x++ ) {
				const size_t at = size_t( y ) * size_t( W ) + size_t( x );
				if ( !F->mask[at] )
					continue;
				const float hgt = lodl->height( ( F->px0 + x ) * step, ( F->py0 + y ) * step );
				const double d = double( b.waterHeight ) - double( hgt );
				k[at] = float( std::max( kDepthFloor, d ) );
			}
	}

	// ---- the strokes' paths: held for the confidence, boosted for the flow
	std::vector<quint8> held( n, 0 );
	auto texelOf = [&]( double wx, double wy, int & x, int & y ) {
		int px = 0, py = 0;
		worldToTexel( wx, wy, px, py );
		x = px - F->px0;
		y = py - F->py0;
	};
	for ( const Seg & seg : segs ) {
		int qx0 = 0, qy0 = 0, qx1 = 0, qy1 = 0;
		texelOf( std::min( seg.ax, seg.bx ) - seg.halfW, std::min( seg.ay, seg.by ) - seg.halfW, qx0, qy0 );
		texelOf( std::max( seg.ax, seg.bx ) + seg.halfW, std::max( seg.ay, seg.by ) + seg.halfW, qx1, qy1 );
		qx0 = std::max( qx0, 0 );
		qy0 = std::max( qy0, 0 );
		qx1 = std::min( qx1, W - 1 );
		qy1 = std::min( qy1, H - 1 );
		for ( int y = qy0; y <= qy1; y++ )
			for ( int x = qx0; x <= qx1; x++ ) {
				const size_t at = size_t( y ) * size_t( W ) + size_t( x );
				if ( !F->mask[at] )
					continue;
				double wx = 0, wy = 0;
				texelToWorld( F->px0 + x, F->py0 + y, wx, wy );
				double tx = 0, ty = 0;
				if ( distToSegment( wx, wy, seg.ax, seg.ay, seg.bx, seg.by, tx, ty ) > seg.halfW )
					continue;
				if ( !held[at] ) {
					held[at] = 1;
					k[at] = float( k[at] * kStrokeBoost );
				}
			}
	}
	int heldHere = 0;
	for ( size_t i = 0; i < n; i++ )
		if ( held[i] )
			heldHere++;
	st.constrained += heldHere;
	if ( !heldHere && !segs.isEmpty() )
		sayNote( QStringLiteral( "no texel of body %1 lies under its strokes, so its "
			"direction is unchanged" ).arg( id ) );

	// ---- the sources and the sinks --------------------------------------
	// nearest wet texel of this body to a world point, within `reach` texels
	auto nearestWet = [&]( double wx, double wy, int reach ) -> long {
		int x = 0, y = 0;
		texelOf( wx, wy, x, y );
		long best = -1;
		double bestD = 1e30;
		for ( int dy = -reach; dy <= reach; dy++ )
			for ( int dx = -reach; dx <= reach; dx++ ) {
				const int nx = x + dx, ny = y + dy;
				if ( nx < 0 || ny < 0 || nx >= W || ny >= H )
					continue;
				const size_t at = size_t( ny ) * size_t( W ) + size_t( nx );
				if ( !F->mask[at] )
					continue;
				const double d = double( dx ) * dx + double( dy ) * dy;
				if ( d < bestD ) {
					bestD = d;
					best = long( at );
				}
			}
		return best;
	};
	// the texels of this body that TOUCH (or nearly touch) body `other`
	auto contact = [&]( quint16 other, std::vector<size_t> & out ) {
		out.clear();
		if ( !other || other == quint16( id ) )
			return;
		const int pad = kContactReach;
		const int PW = W + 2 * pad, PH = H + 2 * pad;
		std::vector<quint8> seed( size_t( PW ) * size_t( PH ), 0 );
		bool any = false;
		for ( int y = 0; y < PH; y++ )
			for ( int x = 0; x < PW; x++ )
				if ( idAtTexel( F->px0 - pad + x, F->py0 - pad + y ) == other ) {
					seed[size_t( y ) * size_t( PW ) + size_t( x )] = 1;
					any = true;
				}
		if ( !any )
			return;
		std::vector<float> dist;
		chamfer( PW, PH, seed, dist );
		float dmin = 1e9f;
		for ( int y = 0; y < H; y++ )
			for ( int x = 0; x < W; x++ ) {
				const size_t at = size_t( y ) * size_t( W ) + size_t( x );
				if ( F->mask[at] )
					dmin = std::min( dmin, dist[size_t( y + pad ) * size_t( PW ) + size_t( x + pad )] );
			}
		if ( dmin > float( kContactReach ) )
			return;
		const float band = std::max( 2.5f, dmin + 0.5f );
		for ( int y = 0; y < H; y++ )
			for ( int x = 0; x < W; x++ ) {
				const size_t at = size_t( y ) * size_t( W ) + size_t( x );
				if ( F->mask[at]
					&& dist[size_t( y + pad ) * size_t( PW ) + size_t( x + pad )] <= band )
					out.push_back( at );
			}
	};
	std::vector<size_t> sources, sinks, contactSrc, contactSnk;
	for ( const QPointF & p : srcPins ) {
		const long at = nearestWet( p.x(), p.y(), 4 );
		if ( at >= 0 )
			sources.push_back( size_t( at ) );
	}
	for ( const QPointF & p : snkPins ) {
		const long at = nearestWet( p.x(), p.y(), 4 );
		if ( at >= 0 )
			sinks.push_back( size_t( at ) );
	}
	contact( b.outlet, contactSnk );
	contact( b.source, contactSrc );
	bool usedStrokeEnds = false;
	auto strokeEnds = [&]( std::vector<size_t> & s, std::vector<size_t> & t ) {
		s.clear();
		t.clear();
		for ( const QPointF & p : firsts ) {
			const long at = nearestWet( p.x(), p.y(), 8 );
			if ( at >= 0 )
				s.push_back( size_t( at ) );
		}
		for ( const QPointF & p : lasts ) {
			const long at = nearestWet( p.x(), p.y(), 8 );
			if ( at >= 0 )
				t.push_back( size_t( at ) );
		}
	};
	{
		std::vector<size_t> es, et;
		strokeEnds( es, et );
		if ( sinks.empty() )
			sinks = contactSnk;
		if ( sources.empty() )
			sources = contactSrc;
		if ( sinks.empty() ) {
			sinks = et;
			usedStrokeEnds = !et.empty();
		}
		if ( sources.empty() ) {
			sources = es;
			usedStrokeEnds = usedStrokeEnds || !es.empty();
		}
	}
	if ( sources.empty() && sinks.empty() ) {
		delete F;
		sayNote( QStringLiteral( "body %1 has no inflow or outflow anywhere -- no pin, no "
			"stroke, and the table names no body it drains into -- so nothing drives a "
			"flow and it keeps the direction the classifier gave it" ).arg( id ) );
		return false;
	}

	// ---- the potential ---------------------------------------------------
	WaterFlowGrid & G = F->grid;
	G.build( W, H, F->mask, k );
	std::vector<double> rhs, phi, dirichlet;
	std::vector<quint8> dset;
	bool anyCut = false;
	for ( size_t i = 0; i < n; i++ )
		if ( cut[i] )
			anyCut = true;
	if ( anyCut ) {
		dset.assign( size_t( G.n ), 0 );
		for ( size_t i = 0; i < n; i++ )
			if ( cut[i] && G.idx[i] >= 0 )
				dset[size_t( G.idx[i] )] = 1;
	}
	auto fillRhs = [&]( const std::vector<size_t> & src, const std::vector<size_t> & snk ) {
		rhs.assign( size_t( G.n ), 0.0 );
		if ( !src.empty() ) {
			for ( size_t at : src )
				if ( G.idx[at] >= 0 )
					rhs[size_t( G.idx[at] )] += 1.0 / double( src.size() );
		} else if ( !anyCut ) {
			for ( int i = 0; i < G.n; i++ )
				rhs[size_t( i )] += 1.0 / double( G.n );      // rain: the catchment
		}
		if ( !snk.empty() ) {
			for ( size_t at : snk )
				if ( G.idx[at] >= 0 )
					rhs[size_t( G.idx[at] )] -= 1.0 / double( snk.size() );
		} else if ( !anyCut ) {
			for ( int i = 0; i < G.n; i++ )
				rhs[size_t( i )] -= 1.0 / double( G.n );      // seepage everywhere
		}
	};
	int it = 0;
	double res = 0.0;
	fillRhs( sources, sinks );
	G.solve( rhs, dset, phi, it, res );
	std::vector<double> ux, uy;
	G.velocity( phi, ux, uy );

	// ---- the strokes' authority: do they agree with what the banks gave? -
	double agreeSum = 0.0;
	int agreeN = 0;
	auto agreement = [&]() {
		agreeSum = 0.0;
		agreeN = 0;
		for ( const Seg & seg : segs ) {
			const double len = std::hypot( seg.bx - seg.ax, seg.by - seg.ay );
			if ( len <= 0.0 )
				continue;
			const double tx = ( seg.bx - seg.ax ) / len, ty = ( seg.by - seg.ay ) / len;
			const int steps = std::max( 1, int( len / u ) );
			for ( int q = 0; q <= steps; q++ ) {
				const double t = double( q ) / steps;
				int x = 0, y = 0;
				texelOf( seg.ax + t * ( seg.bx - seg.ax ), seg.ay + t * ( seg.by - seg.ay ), x, y );
				if ( x < 0 || y < 0 || x >= W || y >= H )
					continue;
				const int i = G.idx[size_t( y ) * size_t( W ) + size_t( x )];
				if ( i < 0 )
					continue;
				const double m = std::hypot( ux[size_t( i )], uy[size_t( i )] );
				if ( m <= 0.0 )
					continue;
				agreeSum += ( ux[size_t( i )] * tx + uy[size_t( i )] * ty ) / m;
				agreeN++;
			}
		}
	};
	agreement();
	if ( agreeN && agreeSum / agreeN < 0.0 && !usedStrokeEnds ) {
		/* The stroke says the water goes the OTHER way from what the table's
		 * contacts gave -- a mis-detected drain, or a user who knows better.
		 * The stroke is the authority: solve again from its ends alone. */
		std::vector<size_t> es, et;
		strokeEnds( es, et );
		if ( !es.empty() && !et.empty() ) {
			fillRhs( es, et );
			G.solve( rhs, dset, phi, it, res );
			G.velocity( phi, ux, uy );
			agreement();
			usedStrokeEnds = true;
			sayNote( QStringLiteral( "body %1's stroke runs against the table's own "
				"drain; the stroke won" ).arg( id ) );
		}
	}
	if ( agreeN ) {
		const double a = agreeSum / agreeN;
		st.strokeAgreement = std::min( st.strokeAgreement, a );
	}
	st.iterations = std::max( st.iterations, it );
	st.residual = std::max( st.residual, res );
	F->phi = phi;

	// ---- direction and speed, per texel ----------------------------------
	double meanSpeed = 0.0;
	qint64 wet = 0;
	for ( int i = 0; i < G.n; i++ ) {
		meanSpeed += std::hypot( ux[size_t( i )], uy[size_t( i )] );
		wet++;
	}
	if ( wet )
		meanSpeed /= double( wet );
	double mx = 0.0, my = 0.0;
	for ( int i = 0; i < G.n; i++ ) {
		const size_t at = size_t( G.cy[size_t( i )] ) * size_t( W ) + size_t( G.cx[size_t( i )] );
		const double sp = std::hypot( ux[size_t( i )], uy[size_t( i )] );
		if ( sp > 1e-12 ) {
			F->vx[at] = float( ux[size_t( i )] / sp );
			F->vy[at] = float( uy[size_t( i )] / sp );
			mx += ux[size_t( i )] / sp;
			my += uy[size_t( i )] / sp;
		} else {
			F->vx[at] = 0.0f;
			F->vy[at] = 0.0f;
		}
		/* The speed nibble: 8 is the body's mean, 15 is 1.9x it (the format's
		 * own quantum), so a narrows that doubles the speed saturates at 15
		 * and says so here rather than wrapping. */
		const double q = meanSpeed > 0.0 ? 8.0 * sp / meanSpeed : 8.0;
		F->speed[at] = quint8( std::max( 0, std::min( 15, int( q + 0.5 ) ) ) );
	}

	// ---- confidence: the geodesic distance from the user's own marks -----
	{
		double halfLife = maxWidth;
		double hlTexels = ( halfLife > 0.0 ? halfLife : 4.0 * u ) / u;
		hlTexels = std::max( 2.0, hlTexels );
		std::vector<quint8> seed( held );
		for ( size_t at : sources )
			seed[at] = 1;
		for ( size_t at : sinks )
			seed[at] = 1;
		std::vector<float> dist( n, 1e9f );
		for ( size_t i = 0; i < n; i++ )
			if ( seed[i] && F->mask[i] )
				dist[i] = 0.0f;
		auto relax = [&]( int x, int y, int nx, int ny, float wgt ) {
			if ( nx < 0 || ny < 0 || nx >= W || ny >= H )
				return;
			const size_t at = size_t( y ) * size_t( W ) + size_t( x );
			const size_t nat = size_t( ny ) * size_t( W ) + size_t( nx );
			if ( !F->mask[at] || !F->mask[nat] )
				return;
			dist[at] = std::min( dist[at], dist[nat] + wgt );
		};
		for ( int y = 0; y < H; y++ )
			for ( int x = 0; x < W; x++ ) {
				relax( x, y, x - 1, y, 1.0f );
				relax( x, y, x, y - 1, 1.0f );
				relax( x, y, x - 1, y - 1, 1.41421f );
				relax( x, y, x + 1, y - 1, 1.41421f );
			}
		for ( int y = H - 1; y >= 0; y-- )
			for ( int x = W - 1; x >= 0; x-- ) {
				relax( x, y, x + 1, y, 1.0f );
				relax( x, y, x, y + 1, 1.0f );
				relax( x, y, x + 1, y + 1, 1.41421f );
				relax( x, y, x - 1, y + 1, 1.41421f );
			}
		for ( size_t i = 0; i < n; i++ ) {
			if ( !F->mask[i] || dist[i] > 1e8f ) {
				F->conf[i] = 0;
				continue;
			}
			const double c = 15.0 * std::pow( 0.5, double( dist[i] ) / hlTexels );
			F->conf[i] = quint8( std::max( 0, std::min( 15, int( c + 0.5 ) ) ) );
		}
	}

	// ---- the body's new mean, and the record's derived fields ------------
	double speedWorld = nSpeed ? sumSpeed / nSpeed : 0.0;
	const double meanMag = std::sqrt( double( b.flowX ) * b.flowX + double( b.flowY ) * b.flowY );
	if ( speedWorld <= 0.0 )
		speedWorld = meanMag > 0.0 ? meanMag : 0.25;
	const double mm = std::hypot( mx, my );
	LodtWaterBody & rec = table[id - 1];
	if ( mm > 1e-6 ) {
		rec.flowX = float( mx / mm * speedWorld );
		rec.flowY = float( my / mm * speedWorld );
	} else {
		rec.flowX = 0.0f;
		rec.flowY = 0.0f;
	}
	rec.flowSource = 4;
	/* Bit 1 ("flow from a stroke") only.  Bit 0 ("user-edited") belongs to
	 * the explicit setters -- name, class, colour -- because solve() cannot
	 * un-set it when the stroke is removed without wiping THEIR edit, and a
	 * bit that can be set but never cleared breaks the undo gate. */
	rec.flags |= quint8( 1u << 1 );
	{
		double cs = 0.0;
		qint64 cnt = 0;
		for ( size_t i = 0; i < n; i++ )
			if ( F->mask[i] ) {
				cs += F->conf[i];
				cnt++;
			}
		rec.confidence = quint8( std::max( 0, std::min( 255,
			int( cnt ? cs / double( cnt ) / 15.0 * 255.0 + 0.5 : 0.0 ) ) ) );
	}
	// a source/outlet pin that lands on ANOTHER body states the graph
	for ( const WaterStroke & s : marks ) {
		if ( !s.enabled() || s.pts.isEmpty() || int( s.body ) != id )
			continue;
		if ( s.kind == WaterStroke::SourcePin ) {
			const quint16 o = bodyAtWorld( double( s.pts.first().x ), double( s.pts.first().y ) );
			if ( o && int( o ) != id )
				rec.source = o;
		}
		if ( s.kind == WaterStroke::OutletPin ) {
			const quint16 o = bodyAtWorld( double( s.pts.last().x ), double( s.pts.last().y ) );
			if ( o && int( o ) != id )
				rec.outlet = o;
		}
	}
	fields.insert( id, F );
	st.bodiesSolved++;
	return true;
}

/*! The plumes.
 *
 *  bungo: *"a factory that's releasing toxic sludge into a river, or river
 *  flowing into an ocean and the river and the ocean may have slightly
 *  different color"*.  A DyePin's plume rides its body's own solved field
 *  from the pin downstream; a DyeMouth mark carries a river's water into
 *  the body it drains to, which gets a field of its own cut around the mouth
 *  -- the mouth as the source, the window's cut edges as the far field --
 *  that writes DYE ONLY: its flow words stay the writer's, its record is not
 *  touched, so a sea that nobody stroked keeps its zero flow and the undo
 *  gate keeps its byte identity. */
void WaterMarkDoc::solveDye( WaterMarkSolve & st )
{
	const double u = worldPerTexel();
	const double halfTexels = dyeHalfDistance() / u;
	int pinIndex = 0;
	auto texelIn = [&]( const Field * F, double wx, double wy, int & x, int & y ) {
		int px = 0, py = 0;
		worldToTexel( wx, wy, px, py );
		x = px - F->px0;
		y = py - F->py0;
		return x >= 0 && y >= 0 && x < F->w && y < F->h;
	};
	auto paint = [&]( Field * F, const std::vector<double> & c, quint32 source ) {
		qint64 painted = 0;
		for ( int i = 0; i < F->grid.n; i++ ) {
			const size_t at = size_t( F->grid.cy[size_t( i )] ) * size_t( F->w )
				+ size_t( F->grid.cx[size_t( i )] );
			const int wgt = int( std::max( 0.0, std::min( 1.0, c[size_t( i )] ) ) * 255.0 + 0.5 );
			if ( wgt <= 0 )
				continue;
			const int have = int( ( F->dye[at] >> 16 ) & 0xFF );
			if ( wgt > have ) {
				F->dye[at] = ( source & 0xFFFF ) | ( quint32( wgt ) << 16 );
				painted++;
			}
		}
		return painted;
	};
	for ( const WaterStroke & s : marks ) {
		if ( !s.enabled() )
			continue;
		if ( s.kind == WaterStroke::DyePin ) {
			const int n = pinIndex++;
			if ( s.pts.isEmpty() || !s.body )
				continue;
			auto it = fields.find( int( s.body ) );
			if ( it == fields.end() ) {
				QString note;
				if ( !solveBody( int( s.body ), st, &note ) ) {
					if ( st.note.isEmpty() )
						st.note = QStringLiteral( "dye pin %1: %2" ).arg( n ).arg( note );
					continue;
				}
				it = fields.find( int( s.body ) );
			}
			Field * F = it.value();
			if ( F->zero || F->phi.empty() )
				continue;
			std::vector<double> held( size_t( F->grid.n ), -1.0 );
			int x = 0, y = 0;
			if ( !texelIn( F, s.pts.first().x, s.pts.first().y, x, y ) )
				continue;
			const double strength = std::max( 0.0, std::min( 1.0, double( s.speed ) ) );
			const int r = std::max( 1, int( double( s.width ) * 0.5 / u + 0.5 ) );
			int seeded = 0;
			for ( int dy = -r; dy <= r; dy++ )
				for ( int dx = -r; dx <= r; dx++ ) {
					if ( dx * dx + dy * dy > r * r )
						continue;
					const int nx = x + dx, ny = y + dy;
					if ( nx < 0 || ny < 0 || nx >= F->w || ny >= F->h )
						continue;
					const int i = F->grid.idx[size_t( ny ) * size_t( F->w ) + size_t( nx )];
					if ( i < 0 )
						continue;
					held[size_t( i )] = strength;
					seeded++;
				}
			if ( !seeded )
				continue;
			std::vector<double> c;
			F->grid.dye( F->phi, held, halfTexels, c );
			const qint64 painted = paint( F, c, quint32( 0x8000 | ( n & 0x7FFF ) ) );
			if ( painted ) {
				st.dyeBodies++;
				st.dyeTexels += painted;
			}
		} else if ( s.kind == WaterStroke::DyeMouth ) {
			const int river = int( s.body );
			auto rit = fields.find( river );
			if ( rit == fields.end() || rit.value()->zero || rit.value()->mask.empty() )
				continue;
			const Field * R = rit.value();
			LodtWaterBody rb;
			body( river, rb );
			const quint16 other = rb.outlet;
			if ( !other || int( other ) == river ) {
				if ( st.note.isEmpty() )
					st.note = QStringLiteral( "body %1 drains into nothing the table knows, so "
						"its dye has nowhere to go" ).arg( river );
				continue;
			}
			// the receiving texels: `other`'s texels within 2.5 texels of the river
			std::vector<std::pair<int, int>> mouth;   // plane texels of `other`
			int mx0 = 1 << 30, my0 = 1 << 30, mx1 = -1, my1 = -1;
			for ( int y = 0; y < R->h; y++ )
				for ( int x = 0; x < R->w; x++ ) {
					if ( !R->mask[size_t( y ) * size_t( R->w ) + size_t( x )] )
						continue;
					for ( int dy = -2; dy <= 2; dy++ )
						for ( int dx = -2; dx <= 2; dx++ ) {
							if ( dx * dx + dy * dy > 6 )
								continue;
							const int px = R->px0 + x + dx, py = R->py0 + y + dy;
							if ( idAtTexel( px, py ) != other )
								continue;
							mouth.push_back( std::make_pair( px, py ) );
							mx0 = std::min( mx0, px );
							my0 = std::min( my0, py );
							mx1 = std::max( mx1, px );
							my1 = std::max( my1, py );
						}
				}
			if ( mouth.empty() ) {
				if ( st.note.isEmpty() )
					st.note = QStringLiteral( "body %1 and the body it drains into (%2) do not "
						"touch, so its dye has no mouth to leave by" ).arg( river ).arg( other );
				continue;
			}
			Field * O = nullptr;
			auto oit = fields.find( int( other ) );
			bool covers = false;
			if ( oit != fields.end() && !oit.value()->zero && !oit.value()->phi.empty() ) {
				O = oit.value();
				covers = O->holds( mx0, my0 ) && O->holds( mx1, my1 );
			}
			if ( !covers ) {
				if ( O && !O->dyeOnly )
					continue;        // the user's own field on the receiver does not reach the mouth
				// a DYE-ONLY field for the receiver, cut around the mouth
				delete O;
				fields.remove( int( other ) );
				O = new Field();
				O->dyeOnly = true;
				LodtWaterBody ob;
				body( int( other ), ob );
				const int obx0 = ( int( ob.x0 ) - lodl->cellMinX() ) * int( idRate );
				const int oby0 = ( int( ob.y0 ) - lodl->cellMinY() ) * int( idRate );
				const int obx1 = obx0 + ( int( ob.x1 ) - int( ob.x0 ) + 1 ) * int( idRate ) - 1;
				const int oby1 = oby0 + ( int( ob.y1 ) - int( ob.y0 ) + 1 ) * int( idRate ) - 1;
				const int margin = int( 4.0 * halfTexels ) + 16;
				O->px0 = std::max( obx0, mx0 - margin );
				O->py0 = std::max( oby0, my0 - margin );
				O->w = std::min( obx1, mx1 + margin ) - O->px0 + 1;
				O->h = std::min( oby1, my1 + margin ) - O->py0 + 1;
				O->windowed = ( O->px0 > obx0 || O->py0 > oby0
					|| O->px0 + O->w - 1 < obx1 || O->py0 + O->h - 1 < oby1 );
				const size_t on = size_t( O->w ) * size_t( O->h );
				O->mask.assign( on, 0 );
				O->vx.assign( on, 0.0f );
				O->vy.assign( on, 0.0f );
				O->conf.assign( on, 0 );
				O->speed.assign( on, 8 );
				O->dye.assign( on, 0 );
				std::vector<quint8> cut( on, 0 );
				std::vector<float> k( on, 1.0f );
				const int spc = lodl->samplesPerCell();
				const int step = idRate > 0 ? std::max( 1, spc / int( idRate ) ) : 1;
				for ( int y = 0; y < O->h; y++ )
					for ( int x = 0; x < O->w; x++ ) {
						const size_t at = size_t( y ) * size_t( O->w ) + size_t( x );
						if ( idAtTexel( O->px0 + x, O->py0 + y ) != other )
							continue;
						O->mask[at] = 1;
						const float hgt = lodl->height( ( O->px0 + x ) * step, ( O->py0 + y ) * step );
						k[at] = float( std::max( kDepthFloor, double( ob.waterHeight ) - double( hgt ) ) );
						if ( O->windowed && ( x == 0 || y == 0 || x == O->w - 1 || y == O->h - 1 ) ) {
							const int ox = x == 0 ? -1 : ( x == O->w - 1 ? 1 : 0 );
							const int oy = y == 0 ? -1 : ( y == O->h - 1 ? 1 : 0 );
							if ( idAtTexel( O->px0 + x + ox, O->py0 + y + oy ) == other )
								cut[at] = 1;
						}
					}
				O->grid.build( O->w, O->h, O->mask, k );
				std::vector<double> rhs( size_t( O->grid.n ), 0.0 );
				std::vector<quint8> dset;
				bool anyCut = false;
				for ( size_t i = 0; i < on; i++ )
					if ( cut[i] && O->grid.idx[i] >= 0 )
						anyCut = true;
				if ( anyCut ) {
					dset.assign( size_t( O->grid.n ), 0 );
					for ( size_t i = 0; i < on; i++ )
						if ( cut[i] && O->grid.idx[i] >= 0 )
							dset[size_t( O->grid.idx[i] )] = 1;
				}
				int seeded = 0;
				for ( const auto & m : mouth ) {
					const int x = m.first - O->px0, y = m.second - O->py0;
					if ( x < 0 || y < 0 || x >= O->w || y >= O->h )
						continue;
					const int i = O->grid.idx[size_t( y ) * size_t( O->w ) + size_t( x )];
					if ( i >= 0 ) {
						rhs[size_t( i )] += 1.0;
						seeded++;
					}
				}
				if ( !seeded || O->grid.n <= 0 ) {
					delete O;
					continue;
				}
				for ( int i = 0; i < O->grid.n; i++ )
					rhs[size_t( i )] /= double( seeded );
				if ( !anyCut )
					for ( int i = 0; i < O->grid.n; i++ )
						rhs[size_t( i )] -= 1.0 / double( O->grid.n );   // a lake: seepage everywhere
				int it = 0;
				double res = 0.0;
				O->grid.solve( rhs, dset, O->phi, it, res );
				st.iterations = std::max( st.iterations, it );
				st.residual = std::max( st.residual, res );
				fields.insert( int( other ), O );
			}
			std::vector<double> held( size_t( O->grid.n ), -1.0 );
			const double strength = std::max( 0.0, std::min( 1.0, double( s.speed ) ) );
			int seeded = 0;
			for ( const auto & m : mouth ) {
				const int x = m.first - O->px0, y = m.second - O->py0;
				if ( x < 0 || y < 0 || x >= O->w || y >= O->h )
					continue;
				const int i = O->grid.idx[size_t( y ) * size_t( O->w ) + size_t( x )];
				if ( i >= 0 ) {
					held[size_t( i )] = strength;
					seeded++;
				}
			}
			if ( !seeded )
				continue;
			std::vector<double> c;
			O->grid.dye( O->phi, held, halfTexels, c );
			const qint64 painted = paint( O, c, quint32( river ) );
			if ( painted ) {
				st.dyeBodies++;
				st.dyeTexels += painted;
			}
		}
	}
}

bool WaterMarkDoc::solve( WaterMarkSolve * out, QString * error )
{
	WaterMarkSolve st;
	if ( error )
		error->clear();
	if ( !opened ) {
		if ( error )
			*error = QStringLiteral( "no landscape file is open" );
		return false;
	}
	qDeleteAll( fields );
	fields.clear();

	/* PUT BACK WHAT AN EARLIER SOLVE DERIVED.  The strokes are the source, so
	 * flowX, flowY, flowSource, confidence, source, outlet and flag bit 1 are
	 * re-derived from scratch every time and never accumulated.  Without this
	 * a body that was marked once kept its stroke's mean for the rest of the
	 * document's life, and since the flow plane is derived from that mean,
	 * removing the stroke could not reproduce the file it started from -- gate
	 * P3 failed by 1,021,405 bytes, the sea's share of the plane.  The fields
	 * the PANEL owns -- name, class, colour, water form and flag bits 0, 2
	 * and 5 -- are deliberately not touched here. */
	for ( int i = 0; i < table.size() && i < tableAtOpen.size(); i++ ) {
		const LodtWaterBody & o = tableAtOpen[i];
		LodtWaterBody & r = table[i];
		r.flowX = o.flowX;
		r.flowY = o.flowY;
		r.flowSource = o.flowSource;
		r.confidence = o.confidence;
		r.source = o.source;
		r.outlet = o.outlet;
		r.flags = quint8( ( r.flags & ~quint8( 1u << 1 ) ) | ( o.flags & quint8( 1u << 1 ) ) );
	}

	// which bodies carry a constraint at all (the dye knob names no body)
	QSet<int> want;
	for ( const WaterStroke & s : marks )
		if ( s.enabled() && s.body && s.kind != WaterStroke::DyeKnob )
			want.insert( int( s.body ) );
	for ( int i = 1; i <= lockZero.size(); i++ )
		if ( lockZero[i - 1] )
			want.insert( i );

	QElapsedTimer clock;
	clock.start();
	QString note;
	QList<int> ids = want.values();
	std::sort( ids.begin(), ids.end() );
	for ( int id : ids )
		solveBody( id, st, &note );
	solveDye( st );
	st.solveSeconds = double( clock.nsecsElapsed() ) / 1e9;
	if ( st.note.isEmpty() )
		st.note = note;

	// how many texels the field actually moved, over the marked bodies
	for ( auto it = fields.constBegin(); it != fields.constEnd(); ++it ) {
		const Field * F = it.value();
		if ( F->dyeOnly )
			continue;
		const quint16 autoWord = automaticWord( quint16( it.key() ) );
		if ( F->zero ) {
			if ( autoWord )
				st.changedTexels += qint64( table[it.key() - 1].area );
			continue;
		}
		for ( int y = 0; y < F->h; y++ )
			for ( int x = 0; x < F->w; x++ ) {
				const size_t at = size_t( y ) * size_t( F->w ) + size_t( x );
				if ( !F->mask[at] )
					continue;
				if ( flowWordAt( F->px0 + x, F->py0 + y ) != autoWord )
					st.changedTexels++;
			}
	}
	if ( st.note.isEmpty() ) {
		st.note = st.bodiesSolved
			? QStringLiteral( "%1 stroke(s) on %2 body(ies): %3 texels under them, %4 changed, "
				"%5 iterations, residual %6, %7 s%8" )
				.arg( st.strokes ).arg( st.bodiesSolved ).arg( st.constrained )
				.arg( st.changedTexels ).arg( st.iterations )
				.arg( st.residual, 0, 'g', 3 ).arg( st.solveSeconds, 0, 'f', 2 )
				.arg( st.dyeTexels ? QStringLiteral( "; dye on %1 texels" ).arg( st.dyeTexels )
					: QString() )
			: QStringLiteral( "no strokes: every body keeps the direction the classifier gave it" );
	}
	dirty = true;
	if ( out )
		*out = st;
	return true;
}

// ---- the dye marks and the dye plane ---------------------------------------

void WaterMarkDoc::setBodyDyeMouth( int id, bool on, float strength )
{
	LodtWaterBody b;
	if ( !body( id, b ) )
		return;
	for ( int i = marks.size() - 1; i >= 0; i-- )
		if ( marks[i].kind == WaterStroke::DyeMouth && int( marks[i].body ) == id )
			marks.removeAt( i );
	if ( on ) {
		WaterStroke s;
		s.body = quint16( id );
		s.kind = WaterStroke::DyeMouth;
		s.flags = WaterStroke::SetsSpeed;
		s.speed = std::max( 0.0f, std::min( 1.0f, strength ) );
		s.width = 0.0f;
		WaterStrokePoint p;
		p.x = float( ( double( b.x0 ) + double( b.x1 ) + 1.0 ) * 0.5 * kCellUnits );
		p.y = float( ( double( b.y0 ) + double( b.y1 ) + 1.0 ) * 0.5 * kCellUnits );
		s.pts.append( p );
		marks.append( s );
	}
	dirty = true;
}

bool WaterMarkDoc::bodyDyeMouth( int id, float * strength ) const
{
	for ( const WaterStroke & s : marks )
		if ( s.kind == WaterStroke::DyeMouth && s.enabled() && int( s.body ) == id ) {
			if ( strength )
				*strength = s.speed;
			return true;
		}
	return false;
}

double WaterMarkDoc::dyeHalfDistance() const
{
	for ( const WaterStroke & s : marks )
		if ( s.kind == WaterStroke::DyeKnob && s.enabled() && s.width > 0.0f )
			return double( s.width );
	return kDyeHalfDistanceDefault;
}

void WaterMarkDoc::setDyeHalfDistance( double worldUnits )
{
	for ( int i = marks.size() - 1; i >= 0; i-- )
		if ( marks[i].kind == WaterStroke::DyeKnob )
			marks.removeAt( i );
	if ( worldUnits > 0.0 && std::fabs( worldUnits - kDyeHalfDistanceDefault ) > 0.5 ) {
		WaterStroke s;
		s.body = 0;
		s.kind = WaterStroke::DyeKnob;
		s.flags = 0;
		s.speed = 0.0f;
		s.width = float( worldUnits );
		marks.append( s );
	}
	dirty = true;
}

bool WaterMarkDoc::hasDye() const
{
	for ( const WaterStroke & s : marks )
		if ( s.enabled() && ( s.kind == WaterStroke::DyePin || s.kind == WaterStroke::DyeMouth ) )
			return true;
	return false;
}

quint32 WaterMarkDoc::dyeWordOf( int px, int py, quint16 id ) const
{
	if ( !id )
		return 0;
	auto it = fields.constFind( int( id ) );
	if ( it == fields.constEnd() )
		return 0;
	const Field * F = it.value();
	if ( F->zero || F->dye.empty() || !F->holds( px, py ) )
		return 0;
	const size_t at = F->at( px, py );
	return F->mask[at] ? F->dye[at] : 0;
}

quint32 WaterMarkDoc::dyeWordAt( int px, int py ) const
{
	return dyeWordOf( px, py, idAtTexel( px, py ) );
}
