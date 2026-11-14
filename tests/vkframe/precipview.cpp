// Lane VKFAR2 (2026-11-14, row L8 slice 1): rain / snow on the present device (vkprecip.inc precipRun: a compute pass
// writes one record a particle, then instanced quads, no geometry stage) against GL's own cell_precip_gpu program
// (vert / geom / frag, the tree's sources) drawn as GL_POINTS with cellprecip.cpp's state, per sample + stencil, into the
// same 4x MSAA RGBA16F + D24S8 frame contents. fxview.cpp's helpers are reused (its main renamed).
// Cases (precipview_prep.py): snow near with an RGBA8 atlas, rain near with no texture (the round disc), snow far (the
// cell's 16-tap mean at the one-texel mip), unfogged; slice 2a: rain near and snow far again through the air
// (frag_jobs.py's volsky inputs, the volume in a shared target), each checked to change GL's own picture on >= 1000
// samples (fogOn 0 vs on); slice 3: cell_precip_look.prog (the drop unfogged, the snow through the air), each checked to
// differ from cell_precip_gpu.prog on >= 1000 samples. Unlit, the snow's normal-map split cannot change a sample (thin =
// thick, no key light): its pcTexN binding is bound but not judged until a lit fixture (slice 2b). A kept split case,
// snow near with no atlas (plain, unfogged), sits next to the open snow near row. Each run starts past particle 0
// (pcFirst) and is unlit. Every atlas and normal map is the app's kind (cellprecip.cpp loadDds): BC3 with every level,
// trilinear, CLAMP_TO_EDGE (rain_near_fog sRGB BC3); the rain cases read it too (rain_drop_look binds it, its look
// reads no texture). The rain atlas's real DXGI format was not on disk: BC3 and sRGB BC3 are the formats judged.
// GL's programs carry the loader's WW_CELL_FOGLEAK (on by default), the record pass too; each fogged case prints a
// fogleak check (GL without the define vs with it, reference only, not part of the verdict).
// Frame before the particles: a sky clear and a depth slab over the left half halfway across a gap between particle
// depths. After them a full-screen draw where stencil == 1 sets alpha 1, so the stencil the particles wrote is read too.
// Bar (farview's, set before the first run, never widened):
//   covered = samples whose colour differs from the frame before on either side (must be >= 1000);
//   flips = samples with any half past 2 ulp; EXACT = no half differs; BOUND = flips <= 0.05 % of covered, halves past
//   1 ulp outside flips <= 0.1 % of the covered halves, stencil differs on <= 0.05 % of covered samples. Else FAIL.
// Also: pcSeedS's bits as fxPack reads them (glGetUniformiv on a uint) equal the value set, or the gate fails.
// Reds (each must FAIL in at least one case): red 1 the cull radius 0, red 2 the strip's last two corners swapped,
// red 4 the near fade dropped, red 8 the air dropped (A = 1, B = 0), red 16 a look run on the plain pipelines, red 32
// the atlas and normal map dropped (the zero texture; must FAIL in EVERY atlas case but rain_drop_look). Last line `PRECIPVIEW GATE OK` or `PRECIPVIEW GATE FAILED`.
// Localisation (plain cases, printed only, never part of the verdict): GL's geometry stage is captured twice by transform
// feedback (gl_Position, gUv, gQ, gView and the flat outputs; any float differing between the two captures = GL not
// deterministic before raster), and the Vulkan run is repeated with its records read back (PrecipRun::recOut) to diff
// per particle against the capture: ulp buckets per field, and which flipped pixels lie under a differing particle.
#define main fxview_main
#include "fxview.cpp"
#undef main

namespace pv
{
using namespace fv;

const char * kSlabFs = "#version 410 core\nuniform float zd; out vec4 o;\n"
	"void main() { gl_FragDepth = zd; o = vec4( 0.6, 0.2, 0.1, 0.5 ); }\n";
const char * kRevealFs = "#version 410 core\nout vec4 o; void main() { o = vec4( 0.0, 0.0, 0.0, 1.0 ); }\n";

struct Verdict
{
	size_t covered = 0, flips = 0, past1 = 0, diff = 0, stencil = 0;
	size_t flipBy[3] = {};  // flips by their worst half: 3-8, 9-64, past 64 ulp
	size_t flipWhole = 0, flipPart = 0;  // flips in pixels whose every sample flipped (shading) / only some (coverage)
	int maxu = 0;
	bool ok() const { return covered >= 1000 && flips * 2000 <= covered && past1 * 1000 <= covered * 4 && stencil * 2000 <= covered; }
	const char * word() const { return covered < 1000 ? "FAIL" : diff == 0 && stencil == 0 ? "EXACT" : ok() ? "BOUND" : "FAIL"; }
};

Verdict judge( const std::vector<std::uint16_t> & bg, const std::vector<std::uint16_t> & a, const std::vector<std::uint16_t> & b,
	const std::vector<std::uint16_t> & ra, const std::vector<std::uint16_t> & rb )
{
	Verdict v;
	if ( a.size() != b.size() || a.size() != bg.size() || ra.size() != a.size() || rb.size() != a.size() || a.empty() )
		return v;
	std::vector<char> flipAt( a.size() / 4, 0 );  // kUnpackFs: a pixel's S = 4 samples sit side by side in a row
	for ( size_t s = 0; s < a.size() / 4; s++ ) {
		bool cov = false;
		int m = 0, p1 = 0, df = 0;
		for ( int c = 0; c < 4; c++ ) {
			const size_t i = s * 4 + size_t( c );
			cov = cov || a[i] != bg[i] || b[i] != bg[i];
			const int d = std::abs( ordered( a[i] ) - ordered( b[i] ) );
			m = std::max( m, d );
			p1 += d > 1;
			df += d != 0;
		}
		const bool sa = ra[s * 4 + 3] == 0x3C00, sb = rb[s * 4 + 3] == 0x3C00;
		cov = cov || sa || sb;
		if ( !cov )
			continue;
		v.covered++;
		v.diff += size_t( df );
		v.stencil += sa != sb;
		if ( m > 2 ) {
			v.flips++;
			v.flipBy[m <= 8 ? 0 : m <= 64 ? 1 : 2]++;
			flipAt[s] = 1;
		} else
			v.past1 += size_t( p1 );
		v.maxu = std::max( v.maxu, m > 2 ? 0 : m );
	}
	for ( size_t p = 0; p + 4 <= flipAt.size(); p += 4 ) {
		const int n = flipAt[p] + flipAt[p + 1] + flipAt[p + 2] + flipAt[p + 3];
		( n == 4 ? v.flipWhole : v.flipPart ) += size_t( n );
	}
	return v;
}

// ruling 11-14 (the near snow reds' localisation, harness only): GL's geometry-stage outputs by transform feedback
// (a copy of the program linked with these varyings; the shaders unchanged) against the Vulkan records read back
void( WINAPI * tfVaryings )( GLuint, GLsizei, const char * const *, GLenum ) = nullptr;
void( WINAPI * tfBegin )( GLenum ) = nullptr;
void( WINAPI * tfEnd )() = nullptr;
void( WINAPI * genQueries )( GLsizei, GLuint * ) = nullptr;
void( WINAPI * beginQuery )( GLenum, GLuint ) = nullptr;
void( WINAPI * endQuery )( GLenum ) = nullptr;
void( WINAPI * getQueryObjectuiv )( GLuint, GLenum, GLuint * ) = nullptr;
void( WINAPI * getBufferSubData )( GLenum, std::ptrdiff_t, std::ptrdiff_t, void * ) = nullptr;
const char * kTfVar[8] = { "gl_Position", "gUv", "gQ", "gView", "gCol", "gBox", "gA", "gB" };
const int kTfAt[9] = { 0, 4, 6, 8, 11, 15, 19, 22, 25 };  // each varying's first float in a captured vertex, 25 a vertex
const int kTfFloats = 25;
void( WINAPI * compressedTexImage2D )( GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void * ) = nullptr;

// ruling 11-14 (step 1): the atlases as the app loads them (cellprecip.cpp loadDds): BC3 (fmt bc3, or bc3srgb), every level
// from the prep's numpy encoder (mips = the level count), BASE 0, MAX_LEVEL levels - 1, trilinear, CLAMP_TO_EDGE
bool uploadBc( const std::string & dir, TexIn & t )
{
	const GLenum ifmt = t.fmt == "bc3" ? 0x83F3 : t.fmt == "bc3srgb" ? 0x8C4F : 0;  // DXT5, SRGB_ALPHA DXT5
	if ( !ifmt || t.mips < 1 || t.target != GL_TEXTURE_2D ) {
		std::printf( "%s texture %s: format %s, %d levels\n", gTag, t.name.c_str(), t.fmt.c_str(), t.mips );
		return false;
	}
	const std::string data = readFile( dir + "/" + t.file );
	GLuint x = 0;
	glGenTextures( 1, &x );
	glBindTexture( GL_TEXTURE_2D, x );
	size_t at = 0;
	for ( int l = 0; l < t.mips; l++ ) {
		const int lw = std::max( t.w >> l, 1 ), lh = std::max( t.h >> l, 1 );
		const size_t bytes = size_t( std::max( 1, ( lw + 3 ) / 4 ) ) * size_t( std::max( 1, ( lh + 3 ) / 4 ) ) * 16;
		if ( at + bytes > data.size() ) {
			std::printf( "%s texture %s: short at level %d\n", gTag, t.name.c_str(), l );
			return false;
		}
		compressedTexImage2D( GL_TEXTURE_2D, l, ifmt, lw, lh, 0, GLsizei( bytes ), data.data() + at );
		at += bytes;
	}
	if ( at != data.size() ) {
		std::printf( "%s texture %s: bytes past its levels\n", gTag, t.name.c_str() );
		return false;
	}
	glTexParameteri( GL_TEXTURE_2D, 0x813C, 0 );  // GL_TEXTURE_BASE_LEVEL
	glTexParameteri( GL_TEXTURE_2D, fMaxLevel, t.mips - 1 );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, t.mips > 1 ? GLint( fMipLinear ) : GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GLint( fClamp ) );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GLint( fClamp ) );
	glBindTexture( GL_TEXTURE_2D, 0 );
	t.gl = x;
	return glGetError() == GL_NO_ERROR;
}
bool uploadAtlas( const std::string & dir, TexIn & t )
{
	return t.fmt.compare( 0, 3, "bc3" ) == 0 ? uploadBc( dir, t ) : upload( dir, t, 0 );
}

GLuint program3( const std::string & vs, const std::string & gs, const std::string & fs, const char * tag, bool tf = false )
{
	const GLuint a = shader( fVert, vs, tag ), b = shader( 0x8DD9, gs, tag ), c = shader( fFrag, fs, tag );  // GL_GEOMETRY_SHADER
	if ( !a || !b || !c )
		return 0;
	const GLuint p = f.createProgram();
	f.attachShader( p, a );
	f.attachShader( p, b );
	f.attachShader( p, c );
	if ( tf )
		tfVaryings( p, 8, kTfVar, 0x8C8C );  // GL_INTERLEAVED_ATTRIBS
	f.linkProgram( p );
	GLint ok = 0;
	f.getProgramiv( p, fLinked, &ok );
	if ( !ok ) {
		std::vector<char> log( 65536 );
		f.getProgramInfoLog( p, GLsizei( log.size() ), nullptr, log.data() );
		std::printf( "%s link failed (%s): %.1500s\n", gTag, tag, log.data() );
		return 0;
	}
	return p;
}

long long ulpOf( float x, float y )
{
	std::int32_t i = 0, k = 0;
	std::memcpy( &i, &x, 4 );
	std::memcpy( &k, &y, 4 );
	const long long a = i < 0 ? -static_cast<long long>( i & 0x7FFFFFFF ) : i, b = k < 0 ? -static_cast<long long>( k & 0x7FFFFFFF ) : k;
	return std::llabs( a - b );
}

// precip_view_vs.hlsl spelled on the CPU from a record (c, axes, ext at floats 0 / 4 / 8): corner v's GL clip position
// (before the depth move), uv and view position; M = GL's pcProj as glGetUniformfv reads it (column i at 4 i = row i there)
void vsCorner( const float * r, int v, const float * M, const float * atlas, float pos[4], float uv[2], float view[3] )
{
	const float qx = ( v & 1 ) ? 1.0f : -1.0f, qy = ( v & 2 ) ? 1.0f : -1.0f;
	const float xh = qx * r[8], yh = qy * r[9];
	const float offY0 = r[6] * yh, offY1 = r[7] * yh;
	const float off0 = std::fma( r[4], xh, offY0 ), off1 = std::fma( r[5], xh, offY1 );
	view[0] = r[0] + off0;
	view[1] = r[1] + off1;
	view[2] = r[2] + 0.0f;
	uv[0] = std::fma( std::fma( qx, 0.5f, 0.5f ), atlas[0], r[10] );
	uv[1] = std::fma( std::fma( qy, 0.5f, 0.5f ), atlas[1], r[11] );
	for ( int i = 0; i < 4; i++ ) {
		const float c0 = view[1] * M[4 + i];
		const float c1 = std::fma( view[0], M[i], c0 );
		const float c2 = std::fma( view[2], M[8 + i], c1 );
		pos[i] = c2 + M[12 + i];
	}
}

// one case's particles: GL's captured triangles (6 vertices a particle, kTfFloats each) against the Vulkan records (28
// floats each: c axes ext col box a b; c.w 0 = culled), per field and bucket; then which flipped pixels lie under a
// particle that differs past 4 ulp anywhere (its GL quad's pixel box)
void particleDiff( const std::string & name, const std::vector<float> & gl, unsigned glPrims, const std::vector<std::uint8_t> & rec, int n,
	const float * M, const float * atlas, int W, int H, const std::vector<std::uint16_t> & a, const std::vector<std::uint16_t> & ref )
{
	std::vector<int> drawn;
	for ( int i = 0; i < n; i++ ) {
		float cw = 0.0f;
		std::memcpy( &cw, rec.data() + size_t( i ) * 112 + 12, 4 );
		if ( cw != 0.0f )
			drawn.push_back( i );
	}
	std::printf( "PRECIPVIEW %s particles: GL wrote %u triangles (%s 2 x the %zu records Vulkan drew of %d)\n", name.c_str(), glPrims,
		glPrims == 2 * drawn.size() ? "=" : "NOT", drawn.size(), n );
	if ( glPrims != 2 * drawn.size() )
		return;  // a cull difference: the finding itself (no pairing past it)
	const char * fname[7] = { "clip position", "uv", "view position", "gCol", "gBox", "gA", "gB" };
	size_t bucket[7][4] = {};  // exact / 1-4 / 5-64 / past 64 ulp, particles by their worst component
	long long fmax[7] = {};
	size_t glFlatSplit = 0, cornerBad = 0;
	std::vector<char> mark( size_t( W ) * H, 0 );
	size_t shown = 0, differing = 0;
	for ( size_t k = 0; k < drawn.size(); k++ ) {
		const float * r = reinterpret_cast<const float *>( rec.data() + size_t( drawn[k] ) * 112 );
		long long worst[7] = {};
		float lo[2] = { 1e30f, 1e30f }, hi[2] = { -1e30f, -1e30f };
		const float * first = &gl[k * 6 * kTfFloats];
		for ( int e = 0; e < 6; e++ ) {
			const float * g = first + e * kTfFloats;
			const float qx = g[kTfAt[2]], qy = g[kTfAt[2] + 1];
			if ( std::fabs( qx ) != 1.0f || std::fabs( qy ) != 1.0f ) {
				cornerBad++;
				continue;
			}
			const int v = ( qx > 0.0f ? 1 : 0 ) + ( qy > 0.0f ? 2 : 0 );
			float pos[4], uv[2], view[3];
			vsCorner( r, v, M, atlas, pos, uv, view );
			for ( int c = 0; c < 4; c++ )
				worst[0] = std::max( worst[0], ulpOf( g[kTfAt[0] + c], pos[c] ) );
			for ( int c = 0; c < 2; c++ )
				worst[1] = std::max( worst[1], ulpOf( g[kTfAt[1] + c], uv[c] ) );
			for ( int c = 0; c < 3; c++ )
				worst[2] = std::max( worst[2], ulpOf( g[kTfAt[3] + c], view[c] ) );
			for ( int c = 0; c < kTfAt[8] - kTfAt[4]; c++ )  // GL's flat outputs the same on every vertex (written once before the loop)
				glFlatSplit += std::memcmp( &g[kTfAt[4] + c], &first[kTfAt[4] + c], 4 ) != 0;
			if ( v >= 2 ) {  // a triangle's last vertex (the provoking one, GL's default): its flat values are the ones drawn
				for ( int c = 0; c < 4; c++ ) {
					worst[3] = std::max( worst[3], ulpOf( g[kTfAt[4] + c], r[12 + c] ) );
					worst[4] = std::max( worst[4], ulpOf( g[kTfAt[5] + c], r[16 + c] ) );
				}
				for ( int c = 0; c < 3; c++ ) {
					worst[5] = std::max( worst[5], ulpOf( g[kTfAt[6] + c], r[20 + c] ) );
					worst[6] = std::max( worst[6], ulpOf( g[kTfAt[7] + c], r[24 + c] ) );
				}
			}
			const float w = g[3];
			for ( int c = 0; c < 2 && w > 0.0f; c++ ) {
				const float s = ( g[c] / w * 0.5f + 0.5f ) * float( c ? H : W );
				lo[c] = std::min( lo[c], s );
				hi[c] = std::max( hi[c], s );
			}
		}
		bool diff = false;
		for ( int fi = 0; fi < 7; fi++ ) {
			bucket[fi][worst[fi] == 0 ? 0 : worst[fi] <= 4 ? 1 : worst[fi] <= 64 ? 2 : 3]++;
			fmax[fi] = std::max( fmax[fi], worst[fi] );
			diff = diff || worst[fi] > 4;
		}
		if ( !diff )
			continue;
		differing++;
		for ( int y = std::max( int( std::floor( lo[1] ) ), 0 ); y <= std::min( int( std::ceil( hi[1] ) ), H - 1 ); y++ )
			for ( int x = std::max( int( std::floor( lo[0] ) ), 0 ); x <= std::min( int( std::ceil( hi[0] ) ), W - 1 ); x++ )
				mark[size_t( y ) * W + x] = 1;
		if ( shown < 6 ) {
			shown++;
			std::printf( "PRECIPVIEW %s particle %d (GL primitive pair %zu) differs:", name.c_str(), drawn[k], k );
			for ( int fi = 0; fi < 7; fi++ )
				if ( worst[fi] > 4 )
					std::printf( " %s %lld ulp", fname[fi], worst[fi] );
			std::printf( " (GL gCol %.9g %.9g %.9g %.9g, Vulkan %.9g %.9g %.9g %.9g; GL gA %.9g, Vulkan %.9g)\n", first[kTfAt[4]], first[kTfAt[4] + 1],
				first[kTfAt[4] + 2], first[kTfAt[4] + 3], r[12], r[13], r[14], r[15], first[kTfAt[6]], r[20] );
		}
	}
	for ( int fi = 0; fi < 7; fi++ )
		std::printf( "PRECIPVIEW %s field %s: particles exact / 1-4 / 5-64 / past 64 ulp: %zu / %zu / %zu / %zu (max %lld)\n", name.c_str(), fname[fi],
			bucket[fi][0], bucket[fi][1], bucket[fi][2], bucket[fi][3], fmax[fi] );
	std::printf( "PRECIPVIEW %s GL flat outputs that differ between a particle's vertices: %zu floats; captured vertices off a corner: %zu\n",
		name.c_str(), glFlatSplit, cornerBad );
	// the flipped pixels (any sample's half past 2 ulp, as judge's flips) under / outside the differing particles' boxes
	size_t flipIn = 0, flipOut = 0, marked = 0;
	for ( size_t p = 0; p < mark.size(); p++ ) {
		marked += size_t( mark[p] );
		bool fl = false;
		for ( int s = 0; s < 4 && !fl; s++ )
			for ( int c = 0; c < 4 && !fl; c++ ) {
				const size_t i = ( p * 4 + size_t( s ) ) * 4 + size_t( c );
				fl = std::abs( ordered( a[i] ) - ordered( ref[i] ) ) > 2;
			}
		if ( fl )
			( mark[p] ? flipIn : flipOut )++;
	}
	std::printf( "PRECIPVIEW %s particles differing past 4 ulp: %zu of %zu; flipped pixels under their quads %zu, outside every one %zu "
		"(their quads cover %zu of %zu pixels)\n", name.c_str(), differing, drawn.size(), flipIn, flipOut, marked, mark.size() );
}

struct Job
{
	std::string name;
	int w = 0, h = 0, first = 0, n = 0, hasTex = 0, farT = 0, fog = 0, look = 0;  // (windows.h defines far)
	unsigned seed = 0;
	float zd = 0.5f;
	UniMap uni;
	std::vector<TexIn> tex;
	int atlasAt = -1, airAt[5] = { -1, -1, -1, -1, -1 }, normAt = -1;  // into tex
	bool hasN() const { return look == 2 && hasTex; }
};
const int kNormUnit = 8;  // slice 3: the look's pcTexN
// slice 2a: the air's textures, in precipRun's tex[2..6] order, their GL units and fxTexOf types
const char * kAirTex[5] = { "atApTex", "volTex", "volTile", "volAirTerr", "volAirRainMap" };
const int kAirType[5] = { 1, 1, 0, 2, 0 };
const int kAirUnit = 1;  // units 1..5 (0 the atlas, 7 pcRoof)
}  // namespace pv

int main( int argc, char ** argv )
{
	using namespace fv;
	using namespace pv;
	gTag = "PRECIPVIEW";
	if ( argc < 2 ) {
		std::printf( "usage: precipview <prep dir>\nPRECIPVIEW GATE FAILED\n" );
		return 2;
	}
	const std::string dir = argv[1];
	auto fatal = [&]( const std::string & why ) {
		std::printf( "PRECIPVIEW GATE FAILED (%s)\n", why.c_str() );
		return 2;
	};
	// the cases
	std::vector<Job> jobs;
	{
		std::ifstream in( dir + "/cases.txt" );
		std::string nm;
		while ( in >> nm ) {
			Job j;
			j.name = nm;
			std::ifstream jf( dir + "/" + nm + "/job.txt" );
			if ( !( jf >> j.w >> j.h >> j.first >> j.n >> j.hasTex >> j.seed >> j.farT >> j.zd >> j.fog >> j.look ) )
				return fatal( nm + "/job.txt" );
			j.uni = readUni( dir + "/" + nm + "/uni.txt" );
			if ( j.hasTex || j.fog ) {
				j.tex = readTex( dir + "/" + nm );
				for ( size_t k = 0; k < j.tex.size(); k++ ) {
					if ( j.tex[k].name == "pcTex" )
						j.atlasAt = int( k );
					if ( j.tex[k].name == "pcTexN" )
						j.normAt = int( k );
					for ( int a = 0; a < 5; a++ )
						if ( j.tex[k].name == kAirTex[a] && j.tex[k].type == kAirType[a] )
							j.airAt[a] = int( k );
				}
				if ( j.tex.size() != size_t( ( j.hasTex ? 1 : 0 ) + ( j.fog ? 5 : 0 ) + ( j.hasN() ? 1 : 0 ) ) || ( j.hasTex && j.atlasAt < 0 )
					|| ( j.hasN() && j.normAt < 0 ) )
					return fatal( nm + "/tex.txt" );
				for ( int a = 0; a < 5 && j.fog; a++ )
					if ( j.airAt[a] < 0 )
						return fatal( nm + "/tex.txt: " + kAirTex[a] );
			}
			jobs.push_back( j );
		}
	}
	if ( jobs.empty() )
		return fatal( "cases.txt" );
	const int W = jobs[0].w, H = jobs[0].h, S = 4, WS = W * S;
	for ( const Job & j : jobs )
		if ( j.w != W || j.h != H )
			return fatal( "the cases' frame sizes differ" );

	gOpenGl = LoadLibraryA( "opengl32.dll" );
	WNDCLASSA wc {};
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = wndProc;
	wc.hInstance = GetModuleHandleA( nullptr );
	wc.lpszClassName = "wwprecipview";
	RegisterClassA( &wc );
	HWND dummy = CreateWindowA( "wwprecipview", "dummy", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr );
	HDC ddc = GetDC( dummy );
	PIXELFORMATDESCRIPTOR pfd { sizeof( pfd ), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
	SetPixelFormat( ddc, ChoosePixelFormat( ddc, &pfd ), &pfd );
	HGLRC drc = wglCreateContext( ddc );
	wglMakeCurrent( ddc, drc );
	using ChooseFmt = BOOL( WINAPI * )( HDC, const int *, const float *, UINT, int *, UINT * );
	auto choose = reinterpret_cast<ChooseFmt>( wglGetProcAddress( "wglChoosePixelFormatARB" ) );
	RECT rc { 0, 0, 320, 180 };
	AdjustWindowRectEx( &rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOACTIVATE );
	HWND wnd = CreateWindowExA( WS_EX_NOACTIVATE, "wwprecipview", "precip view harness", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, 40, 40,
		rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, wc.hInstance, nullptr );
	HDC dc = GetDC( wnd );
	const int attr[] = { 0x2001, 1, 0x2010, 1, 0x2011, 1, 0x2003, 0x2027, 0x2013, 0x202B, 0x2015, 8, 0x2017, 8, 0x2019, 8,
		0x201B, 8, 0x2022, 24, 0x2023, 8, 0x2041, 1, 0x2042, 4, 0 };
	int fmt = 0;
	UINT nf = 0;
	if ( !choose || !choose( dc, attr, nullptr, 1, &fmt, &nf ) || !nf )
		return fatal( "no 4x MSAA pixel format" );
	PIXELFORMATDESCRIPTOR got {};
	DescribePixelFormat( dc, fmt, sizeof( got ), &got );
	SetPixelFormat( dc, fmt, &got );
	HGLRC glrc = wglCreateContext( dc );
	wglMakeCurrent( dc, glrc );
	wglDeleteContext( drc );
	ReleaseDC( dummy, ddc );
	DestroyWindow( dummy );
	ShowWindow( wnd, SW_SHOWNOACTIVATE );
	SetWindowPos( wnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
	pump();
	void( WINAPI * genVertexArrays )( GLsizei, GLuint * ) = nullptr;
	void( WINAPI * bindVertexArray )( GLuint ) = nullptr;
	if ( !loadAll() || !load( genVertexArrays, "glGenVertexArrays" ) || !load( bindVertexArray, "glBindVertexArray" )
		|| !load( tfVaryings, "glTransformFeedbackVaryings" ) || !load( tfBegin, "glBeginTransformFeedback" )
		|| !load( tfEnd, "glEndTransformFeedback" ) || !load( genQueries, "glGenQueries" ) || !load( beginQuery, "glBeginQuery" )
		|| !load( endQuery, "glEndQuery" ) || !load( getQueryObjectuiv, "glGetQueryObjectuiv" ) || !load( getBufferSubData, "glGetBufferSubData" )
		|| !load( compressedTexImage2D, "glCompressedTexImage2D" ) )
		return fatal( "GL entry points" );
	std::printf( "PRECIPVIEW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	const GLuint pUnpack = program( kTriVs, kUnpackFs, "unpack" ), pSlab = program( kTriVs, kSlabFs, "slab" ),
		pReveal = program( kTriVs, kRevealFs, "reveal" );
	const GLuint pPrecip = program3( readFile( dir + "/prog.vert.glsl" ), readFile( dir + "/prog.geom.glsl" ), readFile( dir + "/prog.frag.glsl" ),
		"cell_precip_gpu" );
	const GLuint pLook = program3( readFile( dir + "/prog_look.vert.glsl" ), readFile( dir + "/prog_look.geom.glsl" ),
		readFile( dir + "/prog_look.frag.glsl" ), "cell_precip_look" );
	// the localisation's copy of cell_precip_gpu.prog, its geometry-stage outputs captured (the plain cases)
	const GLuint pTf = program3( readFile( dir + "/prog.vert.glsl" ), readFile( dir + "/prog.geom.glsl" ), readFile( dir + "/prog.frag.glsl" ),
		"cell_precip_gpu (captured)", true );
	// the fogleak check's reference (step 1, 11-14): both programs without the loader's WW_CELL_FOGLEAK, fogged cases only
	const GLuint pNoFl = program3( readFile( dir + "/prog_nofl.vert.glsl" ), readFile( dir + "/prog_nofl.geom.glsl" ),
		readFile( dir + "/prog_nofl.frag.glsl" ), "cell_precip_gpu (no FOGLEAK)" );
	const GLuint pLookNoFl = program3( readFile( dir + "/prog_look_nofl.vert.glsl" ), readFile( dir + "/prog_look_nofl.geom.glsl" ),
		readFile( dir + "/prog_look_nofl.frag.glsl" ), "cell_precip_look (no FOGLEAK)" );
	if ( !pUnpack || !pSlab || !pReveal || !pPrecip || !pLook || !pTf || !pNoFl || !pLookNoFl )
		return fatal( "GL programs" );
	for ( const GLuint prog : { pPrecip, pLook, pTf, pNoFl, pLookNoFl } ) {  // every sampler its own empty unit from 20 (unit 0's default mixes sampler
		// types: GL_INVALID_OPERATION at the draw once the air's textures are bound); bindJob then sets the read ones (farcards.cpp's rule)
		f.useProgram( prog );
		GLint n = 0, next = 20;
		f.getProgramiv( prog, fActiveUnis, &n );
		for ( GLint i = 0; i < n; i++ ) {
			char nm[256] = {};
			GLint sz = 0;
			GLenum ty = 0;
			f.getActiveUniform( prog, GLuint( i ), 256, nullptr, &sz, &ty, nm );
			const bool smp = ( ty >= 0x8B5D && ty <= 0x8B64 ) || ( ty >= 0x8DC0 && ty <= 0x8DD8 && !( ty >= 0x8DC6 && ty <= 0x8DC8 ) )
				|| ( ty >= 0x900C && ty <= 0x900F ) || ( ty >= 0x9108 && ty <= 0x910D );
			if ( !smp )
				continue;
			std::vector<GLint> units( size_t( std::max( sz, 1 ) ) );
			for ( GLint & u : units )
				u = next++;
			f.uniform1iv( f.getUniformLocation( prog, nm ), GLsizei( units.size() ), units.data() );
		}
	}
	std::string why;
	unsigned fc = 0, fds = 0;
	if ( !WwVkPresent::viewBegin( W, H, S, glProc, why ) )
		return fatal( "the view: " + why );
	if ( !WwVkPresent::toneFrame( W, H, S, fc, fds, why ) )
		return fatal( "toneFrame: " + why );
	const GLuint fbFrame = fbo( fT2DMS, fc, 0, fds );
	const GLuint refCol = msTex( W, H, fRgba16f, S ), refDs = msTex( W, H, fD24s8, S );
	const GLuint fbRef = fbo( fT2DMS, refCol, 0, refDs );
	const GLuint unp = tex2d( WS, H, fRgba16f, GL_RGBA, GL_FLOAT, nullptr, false );
	const GLuint fbUnp = fbo( GL_TEXTURE_2D, unp, 0, 0 );
	if ( !fbFrame || !fbRef || !fbUnp )
		return fatal( "GL targets" );
	// uniforms.glsl's block (the geometry stage includes it; unlit and unfogged it reads none of it): zeros
	std::vector<char> zeros( 336, 0 );
	GLuint ubo = 0;
	f.genBuffers( 1, &ubo );
	f.bindBuffer( fUbo, ubo );
	f.bufferData( fUbo, std::ptrdiff_t( zeros.size() ), zeros.data(), fStatic );
	for ( const GLuint prog : { pPrecip, pLook, pTf, pNoFl, pLookNoFl } ) {
		const GLuint bi = f.getUniformBlockIndex( prog, "globalUniforms" );
		if ( bi != 0xFFFFFFFFu )
			f.uniformBlockBinding( prog, bi, 0 );
	}
	GLuint vao = 0;
	genVertexArrays( 1, &vao );
	static char twinKeys[8], normKeys[8];  // one atlas twin a case (the atlases' sizes differ), one normal-map twin a case
	static char airKeys[8][5], volKeys[8];  // the air's twins a case, its volume's shared target a case
	if ( jobs.size() > 8 )
		return fatal( "more than 8 cases" );
	size_t jobAt = 0;

	auto readSamples = [&]( GLuint msColor ) {
		std::vector<std::uint16_t> out( size_t( WS ) * H * 4 );
		f.bindFramebuffer( fFb, fbUnp );
		glViewport( 0, 0, WS, H );
		drawState0();
		f.useProgram( pUnpack );
		bindUnit( 15, fT2DMS, msColor );
		u1i( pUnpack, "src", 15 );
		u1i( pUnpack, "S", S );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		glReadPixels( 0, 0, WS, H, GL_RGBA, fHalf, out.data() );
		return out;
	};
	auto before = [&]( GLuint fb, float zd ) {
		f.bindFramebuffer( fFb, fb );
		glViewport( 0, 0, W, H );
		drawState0();
		glDepthMask( GL_TRUE );
		glStencilMask( 0xFF );
		glClearColor( 0.3f, 0.45f, 0.7f, 0.5f );
		glClearDepth( 1.0 );
		glClearStencil( 0 );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
		glEnable( GL_SCISSOR_TEST );
		glScissor( 0, 0, W / 2, H );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_ALWAYS );
		f.useProgram( pSlab );
		f.uniform1fv( f.getUniformLocation( pSlab, "zd" ), 1, &zd );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		drawState0();
		glDepthFunc( GL_LESS );
	};
	auto reveal = [&]( GLuint fb ) {
		f.bindFramebuffer( fFb, fb );
		glViewport( 0, 0, W, H );
		drawState0();
		glEnable( GL_STENCIL_TEST );
		glStencilMask( 0 );
		glStencilFunc( GL_EQUAL, 1, 0xFF );
		glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		f.useProgram( pReveal );
		glDrawArrays( GL_TRIANGLES, 0, 3 );
		drawState0();
	};
	bool ok = true;
	struct RedSeen
	{
		const char * name;
		int red;
		bool seen;
	} reds[6] = { { "cull (radius 0)", 1, false }, { "corners (the strip's last two swapped)", 2, false }, { "near fade (dropped)", 4, false },
		{ "air (dropped)", 8, false }, { "look (dropped: the plain pipelines)", 16, false }, { "atlas (dropped: the zero texture)", 32, false } };
	for ( Job & j : jobs ) {
		if ( j.hasTex && !uploadAtlas( dir + "/" + j.name, j.tex[size_t( j.atlasAt )] ) )
			return fatal( j.name + ": the atlas upload" );
		const GLuint atlas = j.hasTex ? j.tex[size_t( j.atlasAt )].gl : 0;
		if ( j.hasN() && !uploadAtlas( dir + "/" + j.name, j.tex[size_t( j.normAt )] ) )
			return fatal( j.name + ": the normal map upload" );
		const GLuint normMap = j.hasN() ? j.tex[size_t( j.normAt )].gl : 0;
		const GLuint pJob = j.look ? pLook : pPrecip;
		for ( int a = 0; a < 5 && j.fog; a++ ) {  // the volume in a shared target, as the app's
			TexIn & t = j.tex[size_t( j.airAt[a] )];
			GLuint into = 0;
			if ( a == 1 && !WwVkPresent::sharedTarget( reinterpret_cast<std::uintptr_t>( &volKeys[jobAt] ), 1, t.w, t.h, t.d, fRgba16f, into, why ) )
				return fatal( j.name + ": the shared volume: " + why );
			if ( !upload( dir + "/" + j.name, t, into ) )
				return fatal( j.name + ": upload " + t.name );
		}
		auto bindJob = [&]( GLuint prog ) {
			f.useProgram( prog );
			setUnis( prog, j.uni );
			bindUnit( 0, GL_TEXTURE_2D, atlas );
			u1i( prog, "pcTex", 0 );
			u1i( prog, "pcRoof", 7 );  // nothing bound there (pcRoofOn 0)
			for ( int a = 0; a < 5; a++ ) {  // unfogged: nothing bound (fogOn 0, atAp off)
				const TexIn * t = j.fog ? &j.tex[size_t( j.airAt[a] )] : nullptr;
				bindUnit( kAirUnit + a, kAirType[a] == 1 ? fT3D : kAirType[a] == 2 ? fT2DA : GLenum( GL_TEXTURE_2D ), t ? t->gl : 0 );
				u1i( prog, kAirTex[a], kAirUnit + a );
			}
			if ( !j.fog ) {  // an unfogged case: the air fully off (a program keeps the last fogged case's uniforms)
				static const float zero4[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				u1i( prog, "fogOn", 0 );
				u1i( prog, "volOn", 0 );
				f.uniform4fv( f.getUniformLocation( prog, "atAp" ), 1, zero4 );
			}
			bindUnit( kNormUnit, GL_TEXTURE_2D, normMap );
			u1i( prog, "pcTexN", kNormUnit );  // (not in the plain program: no location, nothing set)
			f.useProgram( prog );
			f.bindBufferBase( fUbo, 0, ubo );
		};
		// GL: cellprecip.cpp's state, its points
		std::vector<std::uint16_t> bg, refA, refB, refAn[4], refBn[4];
		std::vector<std::uint16_t> & refA2 = refAn[0], & refB2 = refBn[0];
		before( fbRef, j.zd );
		bg = readSamples( refCol );
		double glMs = 0.0;
		// pass 0 warms the driver (its first-use cost is not the draw's), pass 1 is timed and judged, passes 2-5 repeat it
		// (pass 2 is the repeat line's; passes 1-5 are the GL noise reference line's five draws)
		// (a fogged case's pass 6 draws with the air off: the air must change the picture, or the case does not test it)
		// (a look case's last pass draws cell_precip_gpu.prog instead: the look must change the picture too)
		// (a fogged case's last pass draws the same program without WW_CELL_FOGLEAK: the fogleak check, printed only)
		std::vector<std::uint16_t> refOff, refPlain, refNoFl;
		if ( const GLenum e = glGetError() )
			return fatal( j.name + ": a GL error before the reference draw (0x" + std::to_string( unsigned( e ) ) + ")" );
		const int passAir = j.fog ? 6 : -1, passPlain = j.look ? 6 + ( j.fog ? 1 : 0 ) : -1;
		const int passNoFl = j.fog ? 6 + 1 + ( j.look ? 1 : 0 ) : -1;
		for ( int pass = 0; pass < 6 + ( j.fog ? 2 : 0 ) + ( j.look ? 1 : 0 ); pass++ ) {
			before( fbRef, j.zd );
			bindJob( pass == passPlain ? pPrecip : pass == passNoFl ? ( j.look ? pLookNoFl : pNoFl ) : pJob );
			if ( pass == passAir ) {  // the air off: fogOn 0 alone leaves the volume and the aerial perspective on
				static const float zero4[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				u1i( pJob, "fogOn", 0 );
				u1i( pJob, "volOn", 0 );
				f.uniform4fv( f.getUniformLocation( pJob, "atAp" ), 1, zero4 );
			}
			f.bindFramebuffer( fFb, fbRef );
			glViewport( 0, 0, W, H );
			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_LEQUAL );
			glDepthMask( GL_FALSE );
			glDisable( GL_CULL_FACE );
			glEnable( GL_BLEND );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			glEnable( GL_STENCIL_TEST );
			glStencilMask( 0x03 );
			glStencilFunc( GL_ALWAYS, 1, 0xFF );
			glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
			bindVertexArray( vao );
			const double g0 = nowMs();
			glDrawArrays( GL_POINTS, j.first, pass ? j.n : 1 );
			glFinish();
			if ( pass == 1 )
				glMs = nowMs() - g0;
			bindVertexArray( 0 );
			drawState0();
			glDepthFunc( GL_LESS );
			if ( pass == passAir )
				refOff = readSamples( refCol );
			else if ( pass == passPlain )
				refPlain = readSamples( refCol );
			else if ( pass == passNoFl )
				refNoFl = readSamples( refCol );
			else if ( pass ) {
				( pass == 1 ? refA : refAn[pass - 2] ) = readSamples( refCol );
				reveal( fbRef );
				( pass == 1 ? refB : refBn[pass - 2] ) = readSamples( refCol );
			}
		}
		if ( const GLenum e = glGetError() )
			return fatal( j.name + ": the GL reference draw (GL error 0x" + [&] { char s[16]; std::snprintf( s, sizeof( s ), "%04X", unsigned( e ) ); return std::string( s ); }() + ")" );
		std::printf( "PRECIPVIEW %s job: %dx%d x%d, run %d + %d, atlas %s, far %d, slab depth %.7f, GL draw %.2f ms\n", j.name.c_str(), W, H, S,
			j.first, j.n, j.hasTex ? ( j.tex[size_t( j.atlasAt )].fmt + ", every level, trilinear clamp" ).c_str() : "none", j.farT, double( j.zd ), glMs );
		if ( j.fog ) {  // the fixture's check, not the bar: samples the air changes past 2 ulp in GL's own picture
			const Verdict o = judge( bg, refOff, refA, refB, refB );
			std::printf( "PRECIPVIEW %s air check: GL with the air off differs from GL with the air on %zu of %zu covered samples (%s)\n", j.name.c_str(),
				o.flips, o.covered, o.flips >= 1000 ? "the air is seen" : "TOO FEW: the case does not test the air" );
			ok = ok && o.flips >= 1000;
			const Verdict n = judge( bg, refNoFl, refA, refB, refB );  // reference only: the define's reach in this fixture
			std::printf( "PRECIPVIEW %s fogleak check: GL without WW_CELL_FOGLEAK differs from GL with it %zu of %zu covered samples%s\n",
				j.name.c_str(), n.flips, n.covered, n.flips ? "" : " (0: this fixture does not reach the one-tail shape)" );
		}
		if ( j.look ) {  // the same: cell_precip_gpu.prog against cell_precip_look.prog on the same uniforms
			const Verdict o = judge( bg, refPlain, refA, refB, refB );
			std::printf( "PRECIPVIEW %s look check: GL's plain program differs from its look program %zu of %zu covered samples (%s)\n", j.name.c_str(),
				o.flips, o.covered, o.flips >= 1000 ? "the look is seen" : "TOO FEW: the case does not test the look" );
			ok = ok && o.flips >= 1000;
		}

		auto runVk = [&]( int red, std::vector<std::uint16_t> & a, std::vector<std::uint16_t> & b, bool checkSeed, std::uint8_t * recOut = nullptr ) -> bool {
			before( fbFrame, j.zd );
			bindJob( pJob );
			f.bindFramebuffer( fFb, fbFrame );
			glViewport( 0, 0, W, H );
			WwVkPresent::PrecipRun t;
			t.w = W;
			t.h = H;
			glGetIntegerv( GL_VIEWPORT, t.vp );
			t.color = fc;
			t.first = j.first;
			t.n = j.n;
			// t0 pcRoof: none (pcRoofOn 0; its sampler shares unit 0's atlas in GL's state, so it is not read from there)
			if ( !WwVkPresent::fxTexOf( &ga, pJob, "pcTex", 0, t.tex[1], why, true ) ) {
				std::printf( "PRECIPVIEW fxTexOf refused: %s\n", why.c_str() );
				return false;
			}
			t.tex[1].twin = reinterpret_cast<std::uintptr_t>( &twinKeys[jobAt] );
			t.fogged = j.fog != 0;
			for ( int a = 0; a < 5 && j.fog; a++ ) {
				if ( !WwVkPresent::fxTexOf( &ga, pJob, kAirTex[a], kAirType[a], t.tex[2 + a], why ) ) {
					std::printf( "PRECIPVIEW fxTexOf refused (%s): %s\n", kAirTex[a], why.c_str() );
					return false;
				}
				t.tex[2 + a].twin = reinterpret_cast<std::uintptr_t>( &airKeys[jobAt][a] );
			}
			WwVkPresent::fxPack( &ga, pJob, WwVkPresent::kT_precipcs, int( std::size( WwVkPresent::kT_precipcs ) ), t.cs );
			WwVkPresent::fxPack( &ga, pJob, WwVkPresent::kT_precipps, int( std::size( WwVkPresent::kT_precipps ) ), t.ps );
			if ( j.look ) {  // slice 3: cell_precip_look.prog's run (its fields, pcTexN at tex[7])
				t.look = true;
				WwVkPresent::fxPack( &ga, pJob, WwVkPresent::kT_preciplookcs, int( std::size( WwVkPresent::kT_preciplookcs ) ), t.cs );
				WwVkPresent::fxPack( &ga, pJob, WwVkPresent::kT_preciplookps, int( std::size( WwVkPresent::kT_preciplookps ) ), t.ps );
				if ( !WwVkPresent::fxTexOf( &ga, pJob, "pcTexN", 0, t.tex[7], why, true ) ) {
					std::printf( "PRECIPVIEW fxTexOf refused (pcTexN): %s\n", why.c_str() );
					return false;
				}
				t.tex[7].twin = reinterpret_cast<std::uintptr_t>( &normKeys[jobAt] );
			}
			if ( checkSeed ) {
				std::uint32_t seed = 0;
				std::memcpy( &seed, t.cs, 4 );  // pcSeedS at 0
				std::printf( "PRECIPVIEW %s pcSeedS: set %u, fxPack read %u (%s)\n", j.name.c_str(), j.seed, seed, seed == j.seed ? "same bits" : "DIFFERS" );
				if ( seed != j.seed )
					return false;
			}
			t.red = red;
			t.recOut = recOut;
			f.bindFramebuffer( fFb, fbFrame );
			if ( !WwVkPresent::precipRun( t, why ) ) {
				std::printf( "PRECIPVIEW precipRun refused: %s\n", why.c_str() );
				return false;
			}
			a = readSamples( fc );
			reveal( fbFrame );
			b = readSamples( fc );
			return glGetError() == GL_NO_ERROR;
		};
		{
			std::vector<std::uint16_t> a, b;
			if ( !runVk( 0, a, b, true ) ) {
				std::printf( "PRECIPVIEW %s FAIL (the Vulkan run failed)\n", j.name.c_str() );
				ok = false;
			} else {
				const Verdict v = judge( bg, a, refA, b, refB );
				std::printf( "PRECIPVIEW %s %s (%zu samples covered, %zu halves differ, %zu edge flips, %zu halves past 1 ulp outside them, "
					"max %d ulp outside them, stencil differs on %zu; flips by worst half 3-8 / 9-64 / past 64 ulp: %zu / %zu / %zu; "
					"in whole / partly flipped pixels: %zu / %zu)\n", j.name.c_str(), v.word(), v.covered, v.diff, v.flips, v.past1, v.maxu,
					v.stencil, v.flipBy[0], v.flipBy[1], v.flipBy[2], v.flipWhole, v.flipPart );
				ok = ok && v.ok();
				// repeatability: GL against its own second draw, Vulkan against a second run (each must be EXACT)
				std::vector<std::uint16_t> a2, b2;
				const Verdict g = judge( bg, refA2, refA, refB2, refB );
				const bool vkRan = runVk( 0, a2, b2, false );
				const Verdict w = vkRan ? judge( bg, a2, a, b2, b ) : Verdict();
				std::printf( "PRECIPVIEW %s repeat: GL vs GL %s (%zu halves differ, %zu flips, stencil %zu), Vulkan vs Vulkan %s (%zu halves differ, "
					"%zu flips, stencil %zu)\n", j.name.c_str(), g.word(), g.diff, g.flips, g.stencil, vkRan ? w.word() : "FAIL (run failed)", w.diff,
					w.flips, w.stencil );
				ok = ok && g.diff == 0 && g.stencil == 0 && vkRan && w.diff == 0 && w.stencil == 0;
				// reference only, never the bar: GL's own run-to-run noise, draws 2-5 each judged against draw 1
				std::size_t nFl[4], nPa[4], nDi[4], nFlMax = 0, nPaMax = 0;
				for ( int k = 0; k < 4; k++ ) {
					const Verdict n = judge( bg, refAn[k], refA, refBn[k], refB );
					nFl[k] = n.flips; nPa[k] = n.past1; nDi[k] = n.diff;
					nFlMax = std::max( nFlMax, n.flips ); nPaMax = std::max( nPaMax, n.past1 );
				}
				std::printf( "PRECIPVIEW %s GL noise (reference, not the bar; 5 draws, 2-5 vs 1): flips %zu %zu %zu %zu (max %zu = %.4f %% of covered), "
					"halves past 1 ulp %zu %zu %zu %zu (max %zu = %.4f %%), halves differ %zu %zu %zu %zu\n", j.name.c_str(), nFl[0], nFl[1], nFl[2],
					nFl[3], nFlMax, v.covered ? 100.0 * double( nFlMax ) / double( v.covered ) : 0.0, nPa[0], nPa[1], nPa[2], nPa[3], nPaMax,
					v.covered ? 100.0 * double( nPaMax ) / double( v.covered ) : 0.0, nDi[0], nDi[1], nDi[2], nDi[3] );
				// the localisation (reference, not the bar): GL's geometry stage captured twice (run to run the same?), then
				// per particle against the Vulkan records read back (a plain case: the look program is not captured)
				if ( !j.look ) {
					static GLuint tfBuf = 0, tfQ = 0;
					if ( !tfBuf ) {
						f.genBuffers( 1, &tfBuf );
						genQueries( 1, &tfQ );
					}
					const std::ptrdiff_t tfBytes = std::ptrdiff_t( j.n ) * 6 * kTfFloats * 4;
					auto capture = [&]( std::vector<float> & out, GLuint & prims ) {
						bindJob( pTf );
						glEnable( 0x8C89 );  // GL_RASTERIZER_DISCARD
						f.bindBuffer( 0x8C8E, tfBuf );  // GL_TRANSFORM_FEEDBACK_BUFFER
						f.bufferData( 0x8C8E, tfBytes, nullptr, 0x88EA );  // GL_DYNAMIC_COPY
						f.bindBufferBase( 0x8C8E, 0, tfBuf );
						beginQuery( 0x8C88, tfQ );  // GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN
						tfBegin( GL_TRIANGLES );
						bindVertexArray( vao );
						glDrawArrays( GL_POINTS, j.first, j.n );
						tfEnd();
						endQuery( 0x8C88 );
						bindVertexArray( 0 );
						glDisable( 0x8C89 );
						prims = 0;
						getQueryObjectuiv( tfQ, 0x8866, &prims );  // GL_QUERY_RESULT
						out.assign( size_t( prims ) * 3 * kTfFloats, 0.0f );
						if ( !out.empty() )
							getBufferSubData( 0x8C8E, 0, std::ptrdiff_t( out.size() * 4 ), out.data() );
						f.bindBufferBase( 0x8C8E, 0, 0 );
					};
					std::vector<float> tf1, tf2;
					GLuint pr1 = 0, pr2 = 0;
					capture( tf1, pr1 );
					capture( tf2, pr2 );
					size_t tfDiff = 0, tfVar[8] = {};
					for ( size_t i = 0; i < std::min( tf1.size(), tf2.size() ); i++ )
						if ( std::memcmp( &tf1[i], &tf2[i], 4 ) != 0 ) {
							tfDiff++;
							const int at = int( i % size_t( kTfFloats ) );
							for ( int q = 0; q < 8; q++ )
								tfVar[q] += at >= kTfAt[q] && at < kTfAt[q + 1];
						}
					std::printf( "PRECIPVIEW %s GL geometry stage captured twice: %u / %u triangles, %zu floats differ (gl_Position %zu, gUv %zu, gQ %zu, "
						"gView %zu, gCol %zu, gBox %zu, gA %zu, gB %zu)%s\n", j.name.c_str(), pr1, pr2, tfDiff, tfVar[0], tfVar[1], tfVar[2], tfVar[3],
						tfVar[4], tfVar[5], tfVar[6], tfVar[7], glGetError() == GL_NO_ERROR ? "" : " (a GL error in the capture)" );
					std::vector<std::uint8_t> rec( size_t( j.n ) * 112 );
					std::vector<std::uint16_t> a3, b3;
					if ( !runVk( 0, a3, b3, false, rec.data() ) )
						std::printf( "PRECIPVIEW %s particles: the Vulkan run with the read-back failed\n", j.name.c_str() );
					else {
						std::printf( "PRECIPVIEW %s particles: the read-back run against the judged one %s\n", j.name.c_str(), judge( bg, a3, a, b3, b ).word() );
						bindJob( pJob );
						float M[16] = {}, atl[4] = {};
						f.getUniformfv( pJob, f.getUniformLocation( pJob, "pcProj" ), M );
						f.getUniformfv( pJob, f.getUniformLocation( pJob, "pcAtlas" ), atl );
						particleDiff( j.name, tf1, pr1, rec, j.n, M, atl, W, H, a, refA );
					}
				}
			}
		}
		for ( RedSeen & r : reds ) {
			if ( ( r.red == 16 && !j.look ) || ( r.red == 32 && !j.hasTex ) )
				continue;  // a plain run has no look to drop, a case with no atlas no atlas
			std::vector<std::uint16_t> a, b;
			if ( !runVk( r.red, a, b, false ) ) {
				std::printf( "PRECIPVIEW %s red %s: the run failed (counts as a FAIL)\n", j.name.c_str(), r.name );
				r.seen = true;
				continue;
			}
			const Verdict v = judge( bg, a, refA, b, refB );
			std::printf( "PRECIPVIEW %s red %s: %s (%zu covered, %zu flips, %zu past 1 ulp, stencil differs on %zu)\n", j.name.c_str(), r.name,
				v.word(), v.covered, v.flips, v.past1, v.stencil );
			r.seen = r.seen || !v.ok();
			if ( r.red == 32 && v.ok() ) {  // every case that reads its atlas must see it dropped (the drop, pcLook 1, reads none)
				std::printf( "PRECIPVIEW %s red atlas %s\n", j.name.c_str(), j.look == 1 ? "PASSED: bound, not read (the drop's look reads no texture)"
					: "PASSED: the case does not see its atlas" );
				ok = ok && j.look == 1;
			}
		}
		jobAt++;
	}
	for ( const RedSeen & r : reds )
		if ( !r.seen ) {
			std::printf( "PRECIPVIEW red %s PASSED in every case: the gate cannot see it\n", r.name );
			ok = false;
		}
	const WwVkPresent::FxStat st = WwVkPresent::takePrecipStat();
	std::printf( "PRECIPVIEW stat: %d runs, %d failed (%s), %d pipelines built in %.1f ms, %d input copies, drawing thread %.3f ms a run "
		"(max %.3f; includes the harness's hand-off wait, not a frame cost)\n", st.runs, st.fails, st.failWhy.c_str(), st.pipelines, st.buildMs,
		st.copies, st.runs ? st.cpuMs / st.runs : 0.0, st.maxMs );
	for ( char & k : twinKeys )  // a BC atlas is a farTwin, an uncompressed one a shared target
		WwVkPresent::farTwinDrop( reinterpret_cast<std::uintptr_t>( &k ) ), WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &k ) );
	for ( char & k : normKeys )
		WwVkPresent::farTwinDrop( reinterpret_cast<std::uintptr_t>( &k ) ), WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &k ) );
	for ( int c = 0; c < 8; c++ ) {
		WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &volKeys[c] ) );
		for ( char & k : airKeys[c] )
			WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &k ) );
	}
	f.bindFramebuffer( fFb, 0 );
	WwVkPresent::toneFrameDrop();
	WwVkPresent::shutdown();
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( glrc );
	DestroyWindow( wnd );
	std::printf( "%s\n", ok ? "PRECIPVIEW GATE OK" : "PRECIPVIEW GATE FAILED" );
	return ok ? 0 : 1;
}
