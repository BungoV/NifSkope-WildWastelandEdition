/* THE ROUNDED OUTLINE (lane TERRLIVE2): see LodgenOutlineField in lodgen.h. */
float lodgenOutlineOffset()
{
	// the blur's worst shift (mean of a Rayleigh, sigma sqrt(pi/2)) + two 512-unit half-diagonals
	return LODGEN_OUTLINE_SIGMA * 1.2533141f + 2.0f * 362.03867f;
}

int lodgenOutlineReachQuads( float band )
{
	return int( std::ceil( ( band + 2.0f * lodgenOutlineOffset() ) / 2048.0f ) ) + 1;
}

//! Felzenszwalb's 1-D squared distance transform, in place over `n` values at `stride`.
static void lodgenOutlineEdt1( float * f, int n, int stride, std::vector<float> & d, std::vector<int> & v,
	std::vector<float> & z )
{
	d.resize( n );
	v.resize( n );
	z.resize( n + 1 );
	auto at = [&]( int q ) { return double( f[size_t( q ) * stride] ) + double( q ) * q; };
	int k = 0;
	v[0] = 0;
	z[0] = -1e30f;
	z[1] = 1e30f;
	for ( int q = 1; q < n; q++ ) {
		double s = ( at( q ) - at( v[k] ) ) / double( 2 * ( q - v[k] ) );
		while ( s <= double( z[k] ) ) {
			k--;
			s = ( at( q ) - at( v[k] ) ) / double( 2 * ( q - v[k] ) );
		}
		k++;
		v[k] = q;
		z[k] = float( s );
		z[k + 1] = 1e30f;
	}
	k = 0;
	for ( int q = 0; q < n; q++ ) {
		while ( z[k + 1] < float( q ) )
			k++;
		const float dq = float( q - v[k] );
		d[q] = dq * dq + f[size_t( v[k] ) * stride];
	}
	for ( int q = 0; q < n; q++ )
		f[size_t( q ) * stride] = d[q];
}

//! Squared grid distance to the nearest point with `seed[o] != 0` (1e20 with none).
static void lodgenOutlineEdt( const std::vector<quint8> & seed, int nx, int ny, std::vector<float> & out )
{
	out.assign( size_t( nx ) * ny, 0.0f );
	for ( size_t o = 0; o < out.size(); o++ )
		out[o] = seed[o] ? 0.0f : 1e20f;
	std::vector<float> d, z;
	std::vector<int> v;
	for ( int j = 0; j < ny; j++ )
		lodgenOutlineEdt1( out.data() + size_t( j ) * nx, nx, 1, d, v, z );
	for ( int i = 0; i < nx; i++ )
		lodgenOutlineEdt1( out.data() + i, ny, nx, d, v, z );
}

void lodgenOutlineBuild( const std::function<bool( int, int )> & paintedQ, int qx0, int qy0, int qx1, int qy1,
	LodgenOutlineField & out )
{
	out = LodgenOutlineField();
	if ( qx1 < qx0 || qy1 < qy0 )
		return;
	const float res = LODGEN_OUTLINE_RES;
	const int nx = ( qx1 - qx0 + 1 ) * 4, ny = ( qy1 - qy0 + 1 ) * 4;
	std::vector<quint8> in( size_t( nx ) * ny ), outside( size_t( nx ) * ny );
	for ( int j = 0; j < ny; j++ )
		for ( int i = 0; i < nx; i++ ) {
			const bool p = paintedQ( qx0 + i / 4, qy0 + j / 4 );
			in[size_t( j ) * nx + i] = p ? 1 : 0;
			outside[size_t( j ) * nx + i] = p ? 0 : 1;
		}
	// signed distance to the staircase: a grid point sits 256 units (half a step) from the line at best
	std::vector<float> dOut, dIn;
	lodgenOutlineEdt( outside, nx, ny, dOut );  // painted points: to the nearest unpainted point
	lodgenOutlineEdt( in, nx, ny, dIn );         // unpainted points: to the nearest painted point
	std::vector<float> raw( size_t( nx ) * ny );
	const float half = 0.5f * res;
	for ( size_t o = 0; o < raw.size(); o++ )
		raw[o] = in[o] ? qMin( std::sqrt( dOut[o] ) * res - half, 1e6f )
		               : -qMin( std::sqrt( dIn[o] ) * res - half, 1e6f );
	// separable Gaussian, clamped at the grid's edge (the edge rows are the reach's own margin)
	const float sg = LODGEN_OUTLINE_SIGMA / res;
	const int rad = int( std::ceil( 3.0f * sg ) );
	std::vector<float> ker( size_t( 2 * rad + 1 ) );
	float ks = 0.0f;
	for ( int t = -rad; t <= rad; t++ )
		ks += ker[size_t( t + rad )] = std::exp( -0.5f * float( t * t ) / ( sg * sg ) );
	for ( float & k : ker )
		k /= ks;
	std::vector<float> tmp( raw.size() );
	for ( int j = 0; j < ny; j++ )
		for ( int i = 0; i < nx; i++ ) {
			float a = 0.0f;
			for ( int t = -rad; t <= rad; t++ )
				a += ker[size_t( t + rad )] * raw[size_t( j ) * nx + qBound( 0, i + t, nx - 1 )];
			tmp[size_t( j ) * nx + i] = a;
		}
	out.f.assign( raw.size(), 0.0f );
	for ( int j = 0; j < ny; j++ )
		for ( int i = 0; i < nx; i++ ) {
			float a = 0.0f;
			for ( int t = -rad; t <= rad; t++ )
				a += ker[size_t( t + rad )] * tmp[size_t( qBound( 0, j + t, ny - 1 ) ) * nx + i];
			out.f[size_t( j ) * nx + i] = a;
			if ( !in[size_t( j ) * nx + i] )
				out.maxUnpainted = qMax( out.maxUnpainted, a );
		}
	out.qx0 = qx0;
	out.qy0 = qy0;
	out.nx = nx;
	out.ny = ny;
}

float LodgenOutlineField::at( float wx, float wy ) const
{
	const float gx = wx / LODGEN_OUTLINE_RES - float( qx0 * 4 ) - 0.5f;
	const float gy = wy / LODGEN_OUTLINE_RES - float( qy0 * 4 ) - 0.5f;
	const float cx = qBound( 0.0f, gx, float( nx - 1 ) ), cy = qBound( 0.0f, gy, float( ny - 1 ) );
	const int i0 = qMin( int( cx ), nx - 2 < 0 ? 0 : nx - 2 ), j0 = qMin( int( cy ), ny - 2 < 0 ? 0 : ny - 2 );
	const int i1 = qMin( i0 + 1, nx - 1 ), j1 = qMin( j0 + 1, ny - 1 );
	const float fx = cx - float( i0 ), fy = cy - float( j0 );
	const float a = f[size_t( j0 ) * nx + i0] + ( f[size_t( j0 ) * nx + i1] - f[size_t( j0 ) * nx + i0] ) * fx;
	const float b = f[size_t( j1 ) * nx + i0] + ( f[size_t( j1 ) * nx + i1] - f[size_t( j1 ) * nx + i0] ) * fx;
	return a + ( b - a ) * fy;
}

float LodgenOutlineField::weight( float wx, float wy, float band ) const
{
	if ( f.empty() )
		return 1.0f;
	const float t = qBound( 0.0f, ( at( wx, wy ) - lodgenOutlineOffset() ) / qMax( band, 1.0f ), 1.0f );
	return t * t * ( 3.0f - 2.0f * t );
}

