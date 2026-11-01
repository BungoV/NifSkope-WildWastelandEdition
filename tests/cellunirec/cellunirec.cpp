/* tests/cellunirec (lane PERFFIX1, item 1): the cell-uniform class record of src/gl/cellunirec.h, driven by a fake
 * program and a fake unit binder. A synthetic body (the shape of wwCellLightsUniforms: plain and indexed names, binds,
 * a live call between sets, a branch on the program's class) is run on the first program of a class while recording;
 * every later program of that class gets either the body itself or the replay, and both must give the same calls
 * (location, kind, values; binds; live calls) in the same order, on 3000 random frames x 6 programs with different
 * uniform sets and locations. Reds (each must FAIL): -DRED_NOBIND (the replay drops binds), -DRED_BYLOC (the replay
 * writes the recorded program's locations), -DRED_ONECLASS (one record for every class).
 * Build: g++ -std=c++17 -O1 -I<tree>/src tests/cellunirec/cellunirec.cpp -o cellunirec.exe ; exit 0 = PASS */
#include "gl/cellunirec.h"

#include <cstdio>
#include <map>
#include <random>
#include <string>
#include <vector>

struct V4
{
	float v[4];
	V4( float a, float b, float c, float d ) : v{ a, b, c, d } {}
};

struct Call
{
	std::string what;
	bool operator==( const Call & o ) const { return what == o.what; }
};

// a program: the uniforms it has (each at its own location), and every call it receives
struct FakeProg
{
	std::map<std::string, int> loc;
	std::vector<Call> * log = nullptr;
	int lastLoc = -1;	// RED_BYLOC: the location the last name resolved to
	int uniLocation( const char * n ) const
	{
		auto it = loc.find( n );
		return it == loc.end() ? -1 : it->second;
	}
	int uniLocation( const char * fmt, int k ) const
	{
		char buf[128];
		std::snprintf( buf, sizeof buf, fmt, k );
		return uniLocation( buf );
	}
	void put( int l, const std::string & s )
	{
		if ( l >= 0 )
			log->push_back( { std::to_string( l ) + ":" + s } );
	}
	static std::string fs( float x ) { char b[32]; std::snprintf( b, sizeof b, "%.9g", double( x ) ); return b; }
	void uni1b( const char * n, bool x ) { put( uniLocation( n ), std::string( "b " ) + ( x ? "1" : "0" ) ); }
	void uni1i( const char * n, int x ) { put( uniLocation( n ), "i " + std::to_string( x ) ); }
	void uni1f( const char * n, float x ) { put( uniLocation( n ), "f " + fs( x ) ); }
	void uni2f( const char * n, float x, float y ) { put( uniLocation( n ), "f2 " + fs( x ) + " " + fs( y ) ); }
	void uni3f( const char * n, float x, float y, float z ) { put( uniLocation( n ), "f3 " + fs( x ) + " " + fs( y ) + " " + fs( z ) ); }
	void uni4f( const char * n, V4 v ) { put( uniLocation( n ), "f4 " + fs( v.v[0] ) + " " + fs( v.v[3] ) ); }
	void uni4f_l( int l, V4 v ) { put( l, "f4l " + fs( v.v[0] ) + " " + fs( v.v[1] ) + " " + fs( v.v[2] ) + " " + fs( v.v[3] ) ); }
};

struct FakeUnits
{
	std::vector<Call> * log = nullptr;
	void bind( int unit, unsigned target, unsigned tex )
	{
		log->push_back( { "bind " + std::to_string( unit ) + " " + std::to_string( target ) + " " + std::to_string( tex ) } );
	}
};

using Rec = WwUniRec::Rec<FakeProg, V4>;

struct Frame
{
	float a, b, c;
	int n, tex, lights;
	bool gi;
};

// the synthetic body: per-frame values, the program's class (ext) and a live call between the sets
static void body( Rec & rec, FakeProg * p, FakeUnits & u, const Frame & fr, bool ext, std::vector<Call> & log )
{
	rec.b( p, "cellOn", true );
	rec.bind( u, 10, 0x8C2A, unsigned( fr.tex ) );
	rec.i( p, "cellLights", 10 );
	rec.bind( u, 12, 0x806F, fr.gi ? unsigned( fr.tex + 1 ) : 0u );
	rec.b( p, "cellGiOn", fr.gi );
	if ( fr.gi ) {
		rec.f3( p, "cellGiOrigin", fr.a, fr.b, fr.c );
		rec.f( p, "cellGiVoxel", fr.a * 0.5f );
	}
	rec.live( 1 );
	log.push_back( { "live 1" } );	// the program-specific call, made live by the body
	for ( int k = 0; k < fr.n; k++ )
		rec.f4l( p, "cellRow[%d]", k, V4( fr.a + float( k ), fr.b, fr.c, 1.0f ) );
	rec.f4l( p, "cellTint", -1, V4( fr.c, fr.b, fr.a, 0.5f ) );
	rec.f2( p, "cellBand", fr.a, fr.c );
	float dir[3] = { fr.a, fr.b, fr.c };
	if ( ext )
		for ( float & d : dir )
			d *= 2.0f;
	rec.f3( p, "cellDirColor", dir[0], dir[1], dir[2] );
	rec.f4( p, "fogSun", V4( dir[0], 0.0f, 0.0f, 1.0f ) );
	rec.cluster( nullptr, fr.lights, 7 );
	log.push_back( { "cluster " + std::to_string( fr.lights ) } );
	if ( fr.lights == 0 )
		return;
	rec.i( p, "cellLightCount", fr.lights );
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	std::mt19937 rng( 1101 );
	const char * names[] = { "cellOn", "cellLights", "cellGiOn", "cellGiOrigin", "cellGiVoxel", "cellRow[0]", "cellRow[1]",
		"cellRow[2]", "cellTint", "cellBand", "cellDirColor", "fogSun", "cellLightCount" };
	long long frames = 0, calls = 0, bad = 0, replays = 0;
	for ( int f = 0; f < 3000; f++ ) {
		std::uniform_real_distribution<float> U( -100.0f, 100.0f );
		Frame fr { U( rng ), U( rng ), U( rng ), int( rng() % 4 ), int( rng() % 50 ), int( rng() % 3 ), ( rng() & 1 ) != 0 };
		// six programs: random uniform subsets and locations, classes ext / not ext
		std::vector<FakeProg> progs( 6 );
		std::vector<bool> ext( 6 );
		for ( int k = 0; k < 6; k++ ) {
			int l = int( rng() % 7 );
			for ( const char * n : names )
				if ( ( rng() % 5 ) != 0 || std::string( n ) == "cellOn" )
					progs[size_t( k )].loc[n] = l += 1 + int( rng() % 3 );
			ext[size_t( k )] = ( rng() % 3 ) == 0;
		}
		std::map<int, Rec> recs;
		std::map<int, FakeProg *> recBy;
		for ( int k = 0; k < 6; k++ ) {
			FakeProg & p = progs[size_t( k )];
#ifdef RED_ONECLASS
			const int cls = 0;
#else
			const int cls = ext[size_t( k )] ? 1 : 0;
#endif
			std::vector<Call> direct, replayed;
			FakeUnits u { &direct };
			p.log = &direct;
			Rec idle;
			body( idle, &p, u, fr, ext[size_t( k )], direct );	// what the old path gives this program
			calls++;
			Rec & r = recs[cls];
			if ( !r.valid ) {	// the class's first call: record (its calls are the direct ones)
				std::vector<Call> rlog;
				FakeUnits ru { &rlog };
				FakeProg q = p;
				q.log = &rlog;
				r.on = true;
				body( r, &q, ru, fr, ext[size_t( k )], rlog );
				r.on = false;
				r.valid = true;
				recBy[cls] = &progs[size_t( k )];
				continue;
			}
			replays++;
			FakeUnits ru { &replayed };
			FakeProg q = p;
			q.log = &replayed;
#ifdef RED_BYLOC
			// the red: the recorded program's locations for this one (what a location-keyed replay would write)
			q.loc = recBy[cls]->loc;
#endif
			WwUniRec::replay( r, &q, ru, [&]( const WwUniRec::Op & o ) {
				if ( o.kind == WwUniRec::Op::Cluster )
					replayed.push_back( { "cluster " + std::to_string( r.clCount ) } );
				else
					replayed.push_back( { "live " + std::to_string( o.arg ) } );
			} );
#ifdef RED_NOBIND
			std::vector<Call> kept;
			for ( const Call & c : replayed )
				if ( c.what.compare( 0, 5, "bind " ) != 0 )
					kept.push_back( c );
			replayed = kept;
#endif
			if ( !( replayed == direct ) ) {
				if ( bad < 3 ) {
					std::printf( "differ: frame %d program %d (class %d): direct %d calls, replay %d\n", f, k, cls,
						int( direct.size() ), int( replayed.size() ) );
					for ( size_t i = 0; i < direct.size() && i < replayed.size(); i++ )
						if ( !( direct[i] == replayed[i] ) ) {
							std::printf( "  first at %d: %s | %s\n", int( i ), direct[i].what.c_str(), replayed[i].what.c_str() );
							break;
						}
				}
				bad++;
			}
		}
		frames++;
	}
	std::printf( "frames %lld calls %lld replays %lld differ %lld\n", frames, calls, replays, bad );
	const bool ok = bad == 0 && replays > 0;
	std::printf( "CELLUNIREC %s\n", ok ? "PASS" : "FAIL" );
	return ok ? 0 : 1;
}
