// lane FARPAGES1: offline gates of the far ground pages (src/gl/cellfgsched.h) -- VRAM from the code's own sizes
// against the target, and the repaint scheduler's budget, reach and layer bookkeeping.
// Build: g++ -std=c++17 -I<tree>/src tests/fgpages/fgpages.cpp -o fgpages.exe
// Reds: -DFG_RED_NOCOMPRESS (RGBA8 pools: the VRAM gate fails), -DFG_SCHED_RED_NOBUDGET, -DFG_SCHED_RED_NOFREE,
// -DFG_SCHED_RED_VISONLY (the scheduler gates fail).

#include "gl/cellfgsched.h"

#include <cmath>
#include <cstdio>
#include <set>

using namespace FgSched;

static int fails = 0, checks = 0;
static void check( bool ok, const char * what )
{
	checks++;
	if ( !ok ) {
		fails++;
		std::printf( "FAIL %s\n", what );
	}
}

#ifdef FG_RED_NOCOMPRESS
constexpr bool kCompressed = false;
#else
constexpr bool kCompressed = true;
#endif

// Concord's ring 0: 13 x 13 cells around the 5 x 5 near grid (the cut), as cellfarlod.cpp builds it for c3 / mem
struct Ring
{
	int x0 = -22, y0 = 11, x1 = -10, y1 = 23, cx0 = -18, cy0 = 15, cx1 = -14, cy1 = 19;
	std::vector<Cell> cells() const
	{
		std::vector<Cell> v;
		for ( int y = y0; y <= y1; y++ )
			for ( int x = x0; x <= x1; x++ ) {
				if ( x >= cx0 && x <= cx1 && y >= cy0 && y <= cy1 )
					continue;
				Cell c;
				c.wx = x;
				c.wy = y;
				v.push_back( c );
			}
		return v;
	}
	float camX() const { return float( cx0 + cx1 + 1 ) * 0.5f * kCell; }
	float camY() const { return float( cy0 + cy1 + 1 ) * 0.5f * kCell; }
};

static float pixAngle( float fovyDeg, int H )
{
	return 2.0f * std::tan( fovyDeg * 0.5f * 3.14159265f / 180.0f ) / float( H );
}

struct Pools
{
	int nNear, capN, capF;
	std::int64_t bytes;
};

// the pool sizing of cellfarground.cpp cachedPass
static Pools pools( const std::vector<Cell> & cells, const Ring & r, int q, float pix, bool compressed )
{
	Pools p;
	const int n = int( cells.size() );
	p.nNear = nearCount( cells, r.camX(), r.camY(), nearDistance( q, pix ) );
	poolCaps( n, p.nNear, q, p.capN, p.capF );
	p.bytes = pageBytes( kNearSide[q], compressed ) * p.capN + pageBytes( kFarSide[q], compressed ) * p.capF;
	return p;
}

static void vram()
{
	const Ring r;
	const std::vector<Cell> cells = r.cells();
	check( cells.size() == 144, "ring 0 = 144 cells" );
	check( bc1Bytes( 512 ) == 174760 && bc1Bytes( 256 ) == 43688, "BC1 sizes with mips to 4x4" );
	check( rgba8Bytes( 512 ) == 8 * bc1Bytes( 512 ), "RGBA8 = 8 x BC1" );
	static const char * const kQ[3] = { "low", "medium", "high" };
	const double target = 14.0 * 1024 * 1024;
	for ( float fov : { 60.0f, 55.0f, 45.0f, 30.0f } ) {
		const float pix = pixAngle( fov, 1026 );
		for ( int q = 0; q < 3; q++ ) {
			const Pools p = pools( cells, r, q, pix, kCompressed );
			std::printf( "vram fov %.0f (pixel %.3f mrad) %-6s: near under %.0f units = %d cells; pools %d x %d + %d x %d px = %.2f MB %s\n",
				double( fov ), double( pix ) * 1000.0, kQ[q], double( nearDistance( q, pix ) ), p.nNear, p.capN, kNearSide[q],
				p.capF, kFarSide[q], double( p.bytes ) / 1048576.0, kCompressed ? "BC1" : "RGBA8" );
			if ( q == 1 )
				check( double( p.bytes ) <= target, "Medium pools <= 14 MB" );
			if ( q == 0 )
				check( double( p.bytes ) <= target / 2, "Low pools <= 7 MB" );
		}
	}
}

// One simulated view: plan, "paint" every job (commit), tick; returns the frames until nothing is planned or fading.
struct Sim
{
	Sched s;
	View v;
	int frames = 0, maxJobs = 0;
	std::int64_t maxSpent = 0;
	bool order = true;
	int step()
	{
		const std::vector<Job> jobs = s.plan( v );
		std::int64_t spent = 0;
		float lastD = -1.0f;
		int lastClass = -1;
		for ( const Job & j : jobs ) {
			const Cell & c = s.cells[size_t( j.cell )];
			const int side = j.pool == 1 ? v.sideN : v.sideF;
			spent += std::int64_t( side ) * side;
			// nearest first inside a class (no page / visible first)
			const int cls = ( c.pool ? 2 : 0 ) + ( Sched::visible( v, c ) ? 0 : 1 );
			const float d = cellDistance( v.x, v.y, c.wx, c.wy );
			if ( cls < lastClass || ( cls == lastClass && d < lastD ) )
				order = false;
			lastClass = cls;
			lastD = d;
			s.commit( j );
		}
		if ( jobs.size() > 1 )
			maxSpent = std::max( maxSpent, spent );	// a lone job may exceed the budget (one page always goes)
		maxJobs = std::max( maxJobs, int( jobs.size() ) );
		const bool busy = s.tick();
		frames++;
		return int( jobs.size() ) + ( busy ? 1 : 0 );
	}
	int settle( int cap )
	{
		const int f0 = frames;
		while ( frames - f0 < cap )
			if ( !step() )
				return frames - f0;
		return -1;
	}
};

static bool layersSound( const Sched & s, const char ** why )
{
	std::set<int> usedN, usedF;
	for ( const Cell & c : s.cells ) {
		if ( !c.pool )
			continue;
		std::set<int> & u = c.pool == 1 ? usedN : usedF;
		if ( !u.insert( c.layer ).second ) {
			*why = "two cells share a layer";
			return false;
		}
	}
	for ( int l : s.freeN )
		if ( usedN.count( l ) ) {
			*why = "a used near layer is also free";
			return false;
		}
	for ( int l : s.freeF )
		if ( usedF.count( l ) ) {
			*why = "a used far layer is also free";
			return false;
		}
	if ( int( usedN.size() + s.freeN.size() ) != s.capN || int( usedF.size() + s.freeF.size() ) != s.capF ) {
		*why = "a layer leaked (used + free != capacity)";
		return false;
	}
	return true;
}

static void scheduler()
{
	const Ring r;
	for ( int q = 0; q < 3; q++ ) {
		Sim m;
		m.s.cells = r.cells();
		const float pix = pixAngle( 60.0f, 1026 );
		const Pools p = pools( m.s.cells, r, q, pix, true );
		m.s.reset( p.capN, p.capF );
		m.v.x = r.camX();
		m.v.y = r.camY();
		m.v.fx = 0.0f;
		m.v.fy = 1.0f;
		m.v.halfFov = std::atan( std::tan( 0.5236f ) * 1920.0f / 1026.0f );
		m.v.nearDist = nearDistance( q, pix );
		m.v.sideN = kNearSide[q];
		m.v.sideF = kFarSide[q];
		const int fill = m.settle( 400 );
		int noPage = 0, wrong = 0;
		for ( const Cell & c : m.s.cells ) {
			noPage += c.pool ? 0 : 1;
			wrong += c.pool == Sched::wantPool( m.v, c ) ? 0 : 1;
		}
		std::printf( "sched q%d: filled %zu cells in %d frames (max %d pages, %lld texels a frame; budget %lld); %d without a page, %d in the other pool\n",
			q, m.s.cells.size(), fill, m.maxJobs, (long long) m.maxSpent, (long long) m.v.budget, noPage, wrong );
		check( fill > 0, "the fill settles" );
		check( noPage == 0, "every cell reached (hidden ones too)" );
		check( wrong == 0, "every cell in its pool at the cut centre" );
		check( m.maxSpent <= m.v.budget, "never over the budget" );
		check( m.order, "nearest first, visible first" );
		const char * why = "";
		{
			const bool sound = layersSound( m.s, &why );
			check( sound, why );
		}
		// walk: turn around, then cross to the ring's edge and back; pages move pools, layers must stay sound
		for ( int k = 0; k < 36; k++ ) {
			const float a = float( k ) * 0.1745f;
			m.v.fx = std::sin( a );
			m.v.fy = std::cos( a );
			m.step();
		}
		for ( int k = 0; k <= 40; k++ ) {
			m.v.x = r.camX() + float( k ) * kCell * 0.1f;
			m.step();
		}
		const int back = m.settle( 400 );
		int inPool = 0;
		for ( const Cell & c : m.s.cells )
			inPool += c.pool ? 1 : 0;
		check( back > 0, "settles again after the walk" );
		check( inPool == int( m.s.cells.size() ), "every cell keeps a page after the walk" );
		check( m.maxSpent <= m.v.budget, "never over the budget on the walk" );
		{
			const bool sound = layersSound( m.s, &why );
			check( sound, why );
		}
		int nearNow = 0, wantNear = 0;
		for ( const Cell & c : m.s.cells ) {
			nearNow += c.pool == 1 ? 1 : 0;
			wantNear += Sched::wantPool( m.v, c ) == 1 ? 1 : 0;
		}
		std::printf( "sched q%d walk: 4 cells east, near pool %d of %d wanted (capacity %d), settled %d frames\n", q, nearNow, wantNear,
			m.s.capN, back );
		check( nearNow == std::min( wantNear, m.s.capN ), "the near pool holds the nearest it can" );
	}
}

int main()
{
	vram();
	scheduler();
	std::printf( "%d checks, %d failures\n%s\n", checks, fails, fails ? "FAIL" : "PASS" );
	return fails ? 1 : 0;
}
