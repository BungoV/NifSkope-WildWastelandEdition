/*! THE EROSION LATTICE (lane GROUND1 Part B, 2026-09-12).
 *
 *  bungo, 2026-09-11 over `cmp_msn_2024.png`: "We lose all the fluvial, erosion
 *  features and other topographical features". The LAND record holds one height
 *  per 128 units; everything finer in vanilla's far sheets came from source
 *  terrain Bethesda never shipped. Gate F1 measured what is missing on 22 of
 *  vanilla's own `_msn` sheets: 40.6 % of the gradient field's variance lives
 *  finer than our 128-unit grid, at a fine-gradient SD of 0.276, aligned ACROSS
 *  the local slope by a factor of 1.479 against a phase-twin floor of 1.025,
 *  and growing with the slope at r = +0.500 against a twin floor of -0.014.
 *  This class grows that relief back.
 *
 *  STATIC-PATH DROPLETS, and the textbook loop is deliberately not used. A
 *  droplet that re-reads the surface it is carving depends on every droplet
 *  before it AND on the extent of the array it runs in, which puts thread
 *  identity, single-chunk-versus-region identity and chunk-border seamlessness
 *  out of reach by construction. Here each droplet traces steepest descent on a
 *  STATIC field -- the shared height reconstruction plus world-seeded value
 *  noise -- so its contribution is a pure function of its world start position.
 *  The delta at a world node is then a sum of the same per-droplet terms in the
 *  same relative order whichever lattice computes it, and a lattice with a
 *  wider extent cannot perturb a narrower one's partial sums, because a droplet
 *  that never reaches a node never adds to it at all.
 *
 *  What that costs, and it is a real cost: without the incision feedback the
 *  pass cannot deepen a channel and then re-route into it, so it does not build
 *  a dendritic network the way a feedback loop does. What it does build is
 *  CONVERGENCE -- steepest descent on a noisy slope braids and collects into the
 *  same lines and the cutting concentrates there.
 *
 *  THE LATTICE STEP IS THE SHEET'S OWN TEXEL, not a fixed world size. At dim 4
 *  that is 32 units, which is exactly the resolution gate F1's numbers were
 *  measured at. It bounds the memory at every dim (a dim-32 chunk on a 32-unit
 *  lattice would be 19 million cells) and it keeps the channels the same size on
 *  screen as the LOD coarsens. The price is that the same ground carries
 *  differently scaled channels at different dims, so an LOD change can pop; that
 *  was not measured. The lattice is still world-aligned -- indices come from
 *  `floor( w / step )` on absolute world coordinates, never from an array offset
 *  -- so two chunks at the same dim agree cell for cell where they meet.
 *
 *  The seed noise NEVER reaches the output. It exists only so that steepest
 *  descent on a smooth slope has something to braid around; what leaves this
 *  class is the erosion delta and the two masks the droplets wrote.
 */
class LodgenErosionField
{
public:
	//! The droplet's hard step cap. The border below is sized from it: a droplet
	//! cannot influence anything more than this many cells from where it started,
	//! so a border of this size means every droplet that can reach the interior
	//! is present in the interior's own lattice.
	static constexpr int MAX_STEPS = 32;
	static constexpr int BORDER = MAX_STEPS;

	bool empty() const { return delta.empty(); }
	float cellSize() const { return cell; }
	const LodgenErosionCensus & census() const { return cen; }

	/*! `ringW`/`ringS` are the world coordinates of the height grid's own
	 *  south-west corner, so `( wx - ringW ) / 128` is the grid coordinate the
	 *  shared reconstruction takes -- the same expression both `_msn` writers
	 *  already use. `wx0`/`wy0`/`w`/`h` are the sheet's own texel rect; the
	 *  lattice is grown by BORDER cells on every side of it. */
	void build( const std::vector<float> & hgt, int hn,
		float ringW, float ringS, float step,
		float wx0, float wy0, int w, int h,
		int dropsPerCell, quint32 seed )
	{
		if ( step <= 0.0f || w <= 0 || h <= 0 || dropsPerCell <= 0 )
			return;
		cell = step;
		gx0 = int( std::floor( double( wx0 ) / double( step ) ) ) - BORDER;
		gy0 = int( std::floor( double( wy0 ) / double( step ) ) ) - BORDER;
		gw = w + 2 * BORDER;
		gh = h + 2 * BORDER;
		const size_t n = size_t( gw ) * size_t( gh );
		field.assign( n, 0.0f );
		delta.assign( n, 0.0f );
		flow.assign( n, 0.0f );
		depo.assign( n, 0.0f );

		/* The static field: the surface the `_msn` already encodes, plus the
		 * nucleation noise. The noise period is 3 cells because F1 read
		 * vanilla's channel spacing at 3.5 texels -- a number that sits ON its
		 * own phase-twin floor, so it is used here as a fit target and never as
		 * evidence that vanilla has channels of that size. */
		for ( int j = 0; j < gh; j++ ) {
			for ( int i = 0; i < gw; i++ ) {
				const float wx = ( float( gx0 + i ) + 0.5f ) * cell;
				const float wy = ( float( gy0 + j ) + 0.5f ) * cell;
				const float h0 = lodgenTerrainHeightAt( hgt, hn,
					( wx - ringW ) / 128.0f, ( wy - ringS ) / 128.0f );
				field[size_t( j ) * gw + i] = h0
					+ noise( gx0 + i, gy0 + j, seed, 3 ) * cell * 0.45f;
			}
		}

		cen.cells = qint64( n );
		cen.step = cell;
		/* WORLD ORDER, south to north then west to east then droplet index.
		 * Two lattices that overlap iterate their shared squares in the same
		 * relative order, which is the whole identity argument. */
		for ( int j = 0; j < gh; j++ ) {
			for ( int i = 0; i < gw; i++ ) {
				for ( int k = 0; k < dropsPerCell; k++ )
					drop( gx0 + i, gy0 + j, quint32( k ), seed );
			}
		}

		double sum = 0.0;
		qint64 moved = 0;
		for ( size_t t = 0; t < n; t++ ) {
			const float d = delta[t];
			if ( d != 0.0f ) {
				moved++;
				sum += std::fabs( double( d ) );
				cen.maxCut = qMin( cen.maxCut, double( d ) );
				cen.maxFill = qMax( cen.maxFill, double( d ) );
			}
		}
		cen.moved = moved;
		cen.meanAbs = moved ? sum / double( moved ) : 0.0;
	}

	//! The erosion height delta in world units, bilinear, 0 outside the lattice.
	float deltaAt( float wx, float wy ) const { return tap( delta, wx, wy ); }
	//! How much water passed, in droplet-water units. 0 outside.
	float flowAt( float wx, float wy ) const { return tap( flow, wx, wy ); }
	//! How much sediment was laid down. 0 outside.
	float depoAt( float wx, float wy ) const { return tap( depo, wx, wy ); }

	/*! The gradient the `_msn` writers add to their own, as a CENTRAL DIFFERENCE
	 *  AT `d` WORLD UNITS -- the sheet's own texel size, floored at the lattice
	 *  step. That one choice anti-aliases the term: at dim 4 a texel is the
	 *  lattice step and the sheet sees the channels at full amplitude; at dim 16
	 *  a texel is 128 units and a central difference over 128 units averages
	 *  112-unit channels away by itself, which is the right answer, because
	 *  relief finer than a texel cannot be shown on that texel. */
	void gradAt( float wx, float wy, float d, float * dgx, float * dgy ) const
	{
		const float s = qMax( d, cell );
		*dgx = ( deltaAt( wx + s, wy ) - deltaAt( wx - s, wy ) ) / ( 2.0f * s );
		*dgy = ( deltaAt( wx, wy + s ) - deltaAt( wx, wy - s ) ) / ( 2.0f * s );
	}

private:
	//! Value noise on a lattice `period` cells coarse, quintic-eased, in [-1,1].
	static float noise( int gx, int gy, quint32 seed, int period )
	{
		const float fx = float( gx ) / float( period );
		const float fy = float( gy ) / float( period );
		const int ix = int( std::floor( fx ) ), iy = int( std::floor( fy ) );
		auto ease = []( float t ) {
			return t * t * t * ( t * ( t * 6.0f - 15.0f ) + 10.0f );
		};
		const float tx = ease( fx - float( ix ) ), ty = ease( fy - float( iy ) );
		auto at = []( int x, int y, quint32 s ) {
			quint32 h = s;
			h ^= quint32( x ) * 0x9E3779B1U;
			h = ( h << 13 ) | ( h >> 19 );
			h *= 0x85EBCA77U;
			h ^= quint32( y ) * 0xC2B2AE3DU;
			h = ( h << 17 ) | ( h >> 15 );
			h *= 0x27D4EB2FU;
			h ^= h >> 15;
			h *= 0x2545F491U;
			h ^= h >> 13;
			return float( h & 0x00FFFFFFU ) / float( 0x00800000U ) - 1.0f;
		};
		const float a = at( ix, iy, seed ), b = at( ix + 1, iy, seed );
		const float c = at( ix, iy + 1, seed ), e = at( ix + 1, iy + 1, seed );
		return ( a * ( 1.0f - tx ) + b * tx ) * ( 1.0f - ty )
			+ ( c * ( 1.0f - tx ) + e * tx ) * ty;
	}

	static quint32 hash3( int x, int y, quint32 k, quint32 seed )
	{
		quint32 h = seed ^ 0x9E3779B9U;
		h ^= quint32( x ) * 0x85EBCA77U;
		h = ( h << 11 ) | ( h >> 21 );
		h ^= quint32( y ) * 0xC2B2AE3DU;
		h = ( h << 7 ) | ( h >> 25 );
		h ^= k * 0x27D4EB2FU;
		h ^= h >> 16;
		h *= 0x7FEB352DU;
		h ^= h >> 15;
		h *= 0x846CA68BU;
		h ^= h >> 16;
		return h;
	}

	float tap( const std::vector<float> & f, float wx, float wy ) const
	{
		if ( f.empty() )
			return 0.0f;
		const float px = float( double( wx ) / double( cell ) ) - float( gx0 ) - 0.5f;
		const float py = float( double( wy ) / double( cell ) ) - float( gy0 ) - 0.5f;
		if ( px < 0.0f || py < 0.0f || px >= float( gw - 1 ) || py >= float( gh - 1 ) )
			return 0.0f;
		const int i = int( px ), j = int( py );
		const float tx = px - float( i ), ty = py - float( j );
		const size_t o = size_t( j ) * gw + i;
		return ( f[o] * ( 1.0f - tx ) + f[o + 1] * tx ) * ( 1.0f - ty )
			+ ( f[o + gw] * ( 1.0f - tx ) + f[o + gw + 1] * tx ) * ty;
	}

	//! The static field and its gradient at a fractional LATTICE position.
	bool sample( float px, float py, float * h, float * gx, float * gy ) const
	{
		if ( px < 0.0f || py < 0.0f || px >= float( gw - 1 ) || py >= float( gh - 1 ) )
			return false;
		const int i = int( px ), j = int( py );
		const float tx = px - float( i ), ty = py - float( j );
		const size_t o = size_t( j ) * gw + i;
		const float a = field[o], b = field[o + 1];
		const float c = field[o + gw], e = field[o + gw + 1];
		*h = ( a * ( 1.0f - tx ) + b * tx ) * ( 1.0f - ty )
			+ ( c * ( 1.0f - tx ) + e * tx ) * ty;
		*gx = ( b - a ) * ( 1.0f - ty ) + ( e - c ) * ty;
		*gy = ( c - a ) * ( 1.0f - tx ) + ( e - b ) * tx;
		return true;
	}

	//! A 3x3 weighted splat, so no single cell takes a whole droplet's cut.
	void splat( std::vector<float> & f, float px, float py, float amount )
	{
		const int i = int( px + 0.5f ), j = int( py + 0.5f );
		for ( int dy = -1; dy <= 1; dy++ ) {
			for ( int dx = -1; dx <= 1; dx++ ) {
				const int x = i + dx, y = j + dy;
				if ( x < 0 || y < 0 || x >= gw || y >= gh )
					continue;
				const float wgt = ( dx == 0 && dy == 0 ) ? 0.25f
					: ( ( dx == 0 || dy == 0 ) ? 0.125f : 0.0625f );
				f[size_t( y ) * gw + x] += amount * wgt;
			}
		}
	}

	void drop( int wcx, int wcy, quint32 k, quint32 seed )
	{
		const quint32 hs = hash3( wcx, wcy, k, seed );
		float px = float( wcx - gx0 ) + float( hs & 0xFFFFU ) / 65536.0f;
		float py = float( wcy - gy0 ) + float( ( hs >> 16 ) & 0xFFFFU ) / 65536.0f;
		float dirx = 0.0f, diry = 0.0f;
		float speed = 1.0f, water = 1.0f, sediment = 0.0f;
		float h0 = 0.0f, gx = 0.0f, gy = 0.0f;
		if ( !sample( px, py, &h0, &gx, &gy ) )
			return;
		for ( int s = 0; s < MAX_STEPS; s++ ) {
			dirx = dirx * INERTIA - gx * ( 1.0f - INERTIA );
			diry = diry * INERTIA - gy * ( 1.0f - INERTIA );
			const float len = std::sqrt( dirx * dirx + diry * diry );
			if ( len < 1.0e-6f )
				break;
			dirx /= len;
			diry /= len;
			const float nx = px + dirx, ny = py + diry;
			float h1 = 0.0f, ngx = 0.0f, ngy = 0.0f;
			if ( !sample( nx, ny, &h1, &ngx, &ngy ) )
				break;
			const float dh = h1 - h0;
			const float cap = qMax( -dh, MIN_SLOPE ) * speed * water * CAPACITY;
			if ( dh > 0.0f || sediment > cap ) {
				const float amount = ( dh > 0.0f )
					? qMin( dh, sediment )
					: ( sediment - cap ) * DEPOSIT;
				if ( amount > 0.0f ) {
					sediment -= amount;
					splat( delta, px, py, amount * cell );
					splat( depo, px, py, amount );
				}
			} else {
				const float amount = qMin( ( cap - sediment ) * ERODE, -dh );
				if ( amount > 0.0f ) {
					sediment += amount;
					splat( delta, px, py, -amount * cell );
				}
			}
			speed = std::sqrt( qMax( 0.0f, speed * speed + ( -dh ) * GRAVITY ) );
			water *= ( 1.0f - EVAPORATE );
			splat( flow, nx, ny, water );
			px = nx;
			py = ny;
			h0 = h1;
			gx = ngx;
			gy = ngy;
			if ( water < 0.01f )
				break;
		}
	}

	/* The droplet constants. They are ordinary hydraulic-erosion parameters and
	 * none of them was invented here; what IS this lane's is the fit of the one
	 * knob in front of them (`--erosion`), reported in section B3. */
	static constexpr float INERTIA = 0.05f;
	static constexpr float CAPACITY = 4.0f;
	static constexpr float MIN_SLOPE = 0.01f;
	static constexpr float ERODE = 0.3f;
	static constexpr float DEPOSIT = 0.3f;
	static constexpr float GRAVITY = 4.0f;
	static constexpr float EVAPORATE = 0.02f;

	std::vector<float> field, delta, flow, depo;
	int gx0 = 0, gy0 = 0, gw = 0, gh = 0;
	float cell = 32.0f;
	LodgenErosionCensus cen;
};
