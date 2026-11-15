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
// Reference (coordinator ruling 11-14: Vulkan's per-sample blend is right; NVIDIA GL's 4x blend at times writes a pixel's
// mean to all its samples, a driver quirk not reproduced): per particle, in draw order, GL's own program drawn with blending
// off into a 4x RGBA32F target (alpha -1 = not written; a particle's two triangles cover a sample once), then blended per
// sample by GL's own blend unit at 1x (a W*S x H RGBA16F target, one texel a sample, SRC_ALPHA / ONE_MINUS_SRC_ALPHA). The
// stencil is GL's 4x one (not blended). Every verdict and red is judged against it; GL's 4x frame is an info line.
// Reference checks (bars set before the first run, each must read 0 halves): R1 a constant full-screen blend at 1x vs 4x;
// R2 the same loop with the 1x blend off vs one GL 4x draw with blending off; R3 the reference vs GL's 4x frame on pixels
// no particle ever split (a capture with 1-3 samples written, or 4 unequal), and on GL's draws 2-5 there too (added after
// the first run, E1: GL's own run-to-run variation must lie on split pixels). Info: GL's merged pixels (GL one value, the
// reference split), how many hold the reference's mean. The repeat clause's GL side is the reference built twice (EXACT)
// and GL's 4x stencil drawn twice (EXACT); GL's 4x colour drawn twice is info (its merge varies run to run). Red 64 (harness only): Vulkan's 4 samples averaged per pixel
// (the reveal's samples kept); must FAIL in every near case.
// Lit (slice 2b, plan L8 (12); bars set before the first lit run, never widened): the sun alone (volTerms 1; the lamps, GI,
// ambient, CSM, the drift, the terrain air, the local sun law and an interior are refused by precipRun, GL draws them),
// through precip_lit_view_cs. Cases rain_near_lit (rain_near_disc's run, unfogged) and snow_far_lit (snow_far_fog's run
// through the air, the veil on, the air's column on the sun): cellOn 1, cellHasDir 1, a non-grey sun (1.6, 1.2, 0.75) off
// the view axis, the phase (volPhase) and its isotropic tint (volGi) set, pcTrans 0.5. The same verdict, reference
// checks, repeat clause and red 64 as the unlit cases. Each lit case must also show (else the gate fails): GL lit vs GL
// with cellHasDir 0 differs on >= 1000 covered samples; GL with pcTrans 0.5 vs 1.0 differs on >= 1000 (thin vs thick);
// no NaN / Inf half in GL's lit frame; every name the sun branch reads is found in GL's program (packed, not zero);
// rain_near_lit's captured gA (unfogged: the light) holds >= 100 distinct particles with unequal channels. Red 128 the
// light dropped (pcRed 4: lit = 1); must FAIL in every lit case.
// Fixture fix after the first lit run (11-15, the bar kept): rain_near_lit's sun gave N.to -0.13 on its view axis, so
// thick / thin = 0.99 and pcTrans 1.0 vs 0.5 moved 51 samples (the case did not test the mix); it takes its own sun
// (precipview_prep.py LIT_SUN, N.to 0.72). The sun's inputs are also checked packed and non-zero in the light block.
// Globals check (11-15, bar set before its first run): precipRun zeroes globalUniforms (b0) and so does this harness for
// GL, so a sun-path read of it would pass EXACT here and be wrong in the app. Each lit case captures GL's geometry stage
// (transform feedback, bit-exact run to run) with b0 zero and with b0 filled non-zero: 0 floats may differ.
// Pack-order check (11-15, bar set before its first run): after the stat line, a lit run with volTerms 2 (a refused term)
// read and asked as precipDraw does (precipLitRead, precipAdmits, then the packs) must be refused with 0 packs on refused
// runs. Red: the same run packed first and handed to precipRun must be refused with its 3 packs counted (> 0), or FAIL.
// Fixture move (11-15, the bar kept): the probe's refused switch is volPartRed 1 (refused for good), not volTerms 2 (the
// lamps, lifted below).
// Lamps (L8 term 2a, 11-15; bar set before its first run, never widened): cases rain_near_lamps (rain_near_lit's run,
// unfogged) and snow_far_lamps (snow_far_lit's run through the air), volTerms 3 (the sun + the lamps), a cellLights texture
// buffer this harness uploads (RGBA32F, 8 texels a light, celllights.cpp's layout) with cellLightCount 5: an omni, a
// spot, a hemisphere, a box and one out of reach; cellShadowOn 0, cellGoboOn 0, cellClusterOn 0 (the flat loop),
// cellLightPhys 0 (the game's curve, the app's default), cellRed 0. Every lit case sets these switches (the sun cases
// cellLightCount 0), so the case order cannot carry them. precipRun twins GL's data store (farTexelBuffer). The same
// verdict, reference checks, repeat clause, reds 64 / 128 and globals check as the lit cases. Each lamp case must also
// show (else the gate fails): GL with volTerms 3 vs 1 differs on >= 1000 covered samples (the lamps seen); GL with
// cellLightCount k vs k - 1 differs on >= 1000 for k = 1..4 (each shape seen) and on 0 for k = 5 (out of reach); every
// name the lamp branch reads is found in GL's program; cellLightCount packed non-zero. Red 256 the lights dropped (Vulkan
// binds the zero texel buffer); must FAIL in every lamp case. Still refused (precipLitWhy): the lamps' shadow cubes,
// gobos, the physical falloff, cellRed, a light count past GL's buffer (the cluster lists: lifted in term 2b below).
// Cluster lists (L8 term 2b, 11-15; bar set before its first run, never widened): case rain_near_cluster (rain_near_lamps'
// run and five lights, unfogged), volTerms 3, cellClusterOn 1 (the app's default whenever a cell has lights:
// cellclustergl.cpp wwCellClusterUniforms), globalUniforms (b0) zero but for projectionMatrix (the case's own projection;
// vpLightsBegin reads it), volRect (0, 0, 1 / W, 1 / H), a 15 x 9 x 4 grid of 64-pixel tiles on log depth over the run's
// particles and a cellCluster texture buffer (R32I, as cellclustergl.cpp): a third of the clusters walk every light
// (count -1), a third list all five, a third list 0, 2, 4 (the spot and the box left out). GL gets the same b0 bytes the
// Vulkan run gets (precipRun takes GL's bytes; the app hands it glcontext.cpp's last upload). The same verdict, reference
// checks, repeat clause, reds 64 / 128 / 256 as the lamp cases, and the lamp check's volTerms 3 vs 1 (the per-shape and
// out-of-reach checks stay with the flat-loop lamp cases: a list's own count overrides cellLightCount). The globals
// check is inverted for this case: b0 filled non-zero vs the case's own b0 must move GL's geometry stage (> 0 floats).
// The case must also show (else the gate fails): GL with cellClusterOn 0 differs on >= 1000 covered samples (the lists
// seen); GL with b0 all zero differs on >= 1000 (the projection read); every name the cluster branch reads is found in
// GL's program; cellClusterDims, cellClusterDepth, cellClusterLog, cellClusterBase and volRect packed non-zero. Red 512
// b0 dropped (Vulkan's globalUniforms zero) and red 1024 the lists dropped (the zero texel buffer at cellCluster): each
// must FAIL in the cluster case. Lifted with it: precipLitWhy's cluster refusal; precipAdmits refuses a cluster run with
// no b0 bytes, precipRun one whose list buffer is not R32I or shorter than cellClusterBase. Not covered here: the
// lights' radial falloff past (0, 1, 1) (every fixture light keeps it; the twin runs GL's own cellRadial).
// Vulkan's globals mirror (11-15, bar set before its first run): precipDraw hands every lit run the app's b0, so each
// lit case but the cluster one also runs Vulkan with the globals check's non-zero b0 as its bytes; it must be EXACT
// against the judged run (b0 read by nothing but the cluster), else the gate fails.
// Census (11-15, info, the L8 done line): the lit program set as the app's all-on defaults (exterior: the sun, volTerms
// 15, volCsm 1, airTerrOn 1, the drift on, vpSun's local law, the lamps' shadow cubes, gobos and cluster lists; interior:
// the same but no sun, no terrain air, no local law), every refusal precipLitWhys would fire printed. Done is 0 in both.
// Local sun (L8 term 3, 11-15; bar set before its first run, never widened): case rain_near_sunlocal (rain_near_lit's run,
// unfogged, volTerms 1) with vpSun (0.9, 1.3, 1.7, 1): the local law outdoors (celllights.cpp fxSunLocal, on by default),
// the sun's colour from vpSun, not cellDirColor (1.6, 1.2, 0.75). Every other lit case sets vpSun 0 (GL keeps a
// program's uniforms). The same verdict, reference checks, repeat clause, reds 64 / 128, the globals check and Vulkan's
// globals mirror as rain_near_lit. The case must also show (else the gate fails): GL with vpSun.w 0 differs on >= 1000
// covered samples (the law seen); vpSun found in GL's program; vpSun packed non-zero. Red 2048 the law dropped (Vulkan's
// packed vpSun.w 0, so cellDirColor lights it) must FAIL in the case. Lifted with it: precipLitWhy's local-sun refusal.
// Fixture move after the first run (11-15, the bar kept): vpSun (0.9, 1.3, 1.7) had cellDirColor's luminance (1.244 vs
// 1.252) and the air colours the sky terms by luminance (airEnvColour), so the law moved 8 samples; vpSun is now
// (0.5, 0.4, 0.3, 1), luminance 0.414.
// Terrain air (L8, 11-15; bar set before its first run, never widened): case snow_far_terrair (snow_far_lit's run, fogged,
// volTerms 1) with airTerrOn 1: the ground layer stands on the terrain grid (cell_volpart vpLight airGroundAt, on by default
// with the air: cellvolair.cpp 4219). airTerr is the air's own volAirTerr grid (the app binds its pass grid to both
// names), airTerrXf / airTerrFineXf its world rows, airTerrRef chosen before the run so the layer's anchor straddles the
// run's particles (the prep prints the share under it; want 0.2 - 0.8). The same verdict, reference checks, repeat
// clause, reds 64 / 128, the globals check and Vulkan's globals mirror as snow_far_lit. The case must also show (else the
// gate fails): GL with airTerrOn 0 differs on >= 1000 covered samples (the grid seen); airTerr, airTerrOn, airTerrXf,
// airTerrFineXf and airTerrRef found in GL's program; airTerrOn and airTerrXf packed non-zero. Red 4096 the grid dropped
// (Vulkan's airTerr the zero texture) must FAIL in the case. Lifted with it: precipLitWhy's terrain-air refusal; the
// app's rain / snow draw twins GL's airTerr (t17) into the lit set.
// Census roof line (11-15, info): a cell with a roof map turns pcRoofOn on (cellprecip.cpp gpuRoof), refused for every
// run, lit or not (precipAdmits, slice 3's); the census prints it beside the lit refusals and counts it.
// GI (L8 term 1, VKFAR5 11-15; bar set before its first run, never widened): the GI term (volTerms 4) reads the grid only
// when cellGiOn, and cell_lights compiles cellGiOn as the constant false unless the loader's WW_CELL_PRT (the old baked
// probe GI's red, WW_CELL_PRT_RED=1) is in the program; the default program and the lit twin hold no GI read. Case
// rain_near_gi (rain_near_lit's run, unfogged) with volTerms 5 (the sun + GI). The same verdict, reference checks, repeat
// clause, reds 64 / 128, the globals check and Vulkan's globals mirror as rain_near_lit. The case must also show (else
// the gate fails): none of cellGiOn, cellGi, cellGiSlots, cellRooms, cellGiAir, cellGiAirOn, cellDdgiOn, cellDdgiAtlas
// active in GL's default program; GL's captured geometry stage with volTerms 5 vs 1: 0 floats differ; the loader's RT GI
// variant (WW_CELL_RTDDGI, the ray-traced GI on) captured with volTerms 5 against the default program: 0 floats differ,
// and none of those names active in it; the WW_CELL_PRT variant with its GI fed (cellGiOn 1, cellGiAirOn 1, a filled
// cellGiAir) holds cellGiOn and cellGiAir and differs from GL's default frame on >= 1000 covered samples (the GI seen);
// precipAdmits refuses the WW_CELL_PRT variant's lit run (a program that reads a GI volume), 0 packs. Red 8192 that
// refusal's input dropped (precipLitRead's GI count forced 0, the run goes on to Vulkan): judged against the WW_CELL_PRT
// variant's frame it must FAIL with >= 1000 flips. Lifted with it: precipLitWhy's volTerms 4 refusal, replaced by the
// program refusal (a GL program with a GI read stays on GL).
// Known start state (VKFAR5 11-15, coordinator ruling; set before its first run, never widened): every GL program's loose
// uniforms are read right after it links (sampler units included in the snapshot's skip: those stay as set) and written
// back at the top of every bindJob, so each case starts from the linked state and sets what it needs on top; before,
// a program kept the uniforms an earlier case set and this case does not name (the fogged cases' air values on the
// unfogged lit cases). Every case reruns; a changed verdict is reported with its numbers, never absorbed. Red (a check
// on rain_near_lit, not a red bit): GL drawn with the restore skipped and a fogged case's uniforms left on the program
// must differ from rain_near_lit's judged GL frame on >= 1000 covered samples (the restore matters), else the gate fails.
#define main fxview_main
#include "fxview.cpp"
#undef main
#include <array>

namespace pv
{
using namespace fv;

const char * kSlabFs = "#version 410 core\nuniform float zd; out vec4 o;\n"
	"void main() { gl_FragDepth = zd; o = vec4( 0.6, 0.2, 0.1, 0.5 ); }\n";
const char * kRevealFs = "#version 410 core\nout vec4 o; void main() { o = vec4( 0.0, 0.0, 0.0, 1.0 ); }\n";
// the reference's 1x pass: one texel a sample; 0 the blended frame, 1 the pixel split by this particle (blend MAX), 2 the
// last written output (blend off, check R2)
const char * kCapBlendFs = "#version 410 core\n"
	"uniform sampler2DMS cap; uniform int S;\n"
	"layout(location=0) out vec4 o; layout(location=1) out vec4 sp; layout(location=2) out vec4 last;\n"
	"void main() { ivec2 q = ivec2( gl_FragCoord.xy ), p = ivec2( q.x / S, q.y ); vec4 v = texelFetch( cap, p, q.x % S );\n"
	"  if ( v.a < 0.0 ) discard;\n"
	"  vec4 v0 = texelFetch( cap, p, 0 ); int w = 0; bool same = true;\n"
	"  for ( int k = 0; k < S; k++ ) { vec4 u = texelFetch( cap, p, k ); w += u.a < 0.0 ? 0 : 1; same = same && u == v0; }\n"
	"  o = v; sp = vec4( w < S || !same ? 1.0 : 0.0 ); last = v; }\n";
const char * kProbeFs = "#version 410 core\nout vec4 o; void main() { o = vec4( 0.25, 0.5, 0.75, 0.4 ); }\n";

// float to half, round to nearest even (red 64's mean)
std::uint16_t f2h( float x )
{
	std::uint32_t b;
	std::memcpy( &b, &x, 4 );
	const std::uint16_t s = std::uint16_t( ( b >> 16 ) & 0x8000 );
	b &= 0x7FFFFFFFu;
	if ( b >= 0x7F800000u )
		return s | 0x7C00 | ( b > 0x7F800000u ? 0x200 : 0 );
	if ( b >= 0x477FF000u )
		return s | 0x7C00;
	if ( b < 0x38800000u ) {
		float v;
		std::memcpy( &v, &b, 4 );
		return s | std::uint16_t( std::nearbyint( v * 16777216.0f ) );
	}
	return s | std::uint16_t( ( b + 0xFFFu + ( ( b >> 13 ) & 1u ) - 0x38000000u ) >> 13 );
}

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
void( WINAPI * clearBufferfv )( GLenum, GLint, const GLfloat * ) = nullptr;
void( WINAPI * blendEquationi )( GLuint, GLenum ) = nullptr;
void( WINAPI * disablei )( GLenum, GLuint ) = nullptr;

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
	int w = 0, h = 0, first = 0, n = 0, hasTex = 0, farT = 0, fog = 0, look = 0, lit = 0;  // (windows.h defines far)
	unsigned seed = 0;
	float zd = 0.5f;
	UniMap uni;
	std::vector<TexIn> tex;
	int atlasAt = -1, airAt[5] = { -1, -1, -1, -1, -1 }, normAt = -1;  // into tex
	bool hasN() const { return look == 2 && hasTex; }
	struct Buf  // L8 term 2a: a texture buffer (buf.txt: name format texels file)
	{
		std::string name, fmt, file;
		int texels = 0;
		GLuint buf = 0, tex = 0;
	};
	std::vector<Buf> bufs;
	GLuint bufTex( const char * nm ) const
	{
		for ( const Buf & b : bufs )
			if ( b.name == nm )
				return b.tex;
		return 0;
	}
	bool lamps() const { return lit && !bufs.empty(); }
	bool localSun() const  // L8 term 3: vpSun.w on (the local law)
	{
		const auto it = uni.find( "vpSun" );
		return lit && it != uni.end() && it->second.size() == 4 && it->second[3] > 0.5;
	}
	bool gi() const  // L8 term 1 (VKFAR5): volTerms' GI bit (4)
	{
		const auto it = uni.find( "volTerms" );
		return lit && it != uni.end() && !it->second.empty() && ( int( it->second[0] ) & 4 ) != 0;
	}
	bool terrAir() const  // L8 terrain air: airTerrOn 1 (the ground layer on the grid)
	{
		const auto it = uni.find( "airTerrOn" );
		return lit && fog && it != uni.end() && !it->second.empty() && it->second[0] > 0.5;
	}
	std::vector<char> global;  // L8 term 2b: globalUniforms' bytes (global.bin; empty: zeros)
	bool cluster() const
	{
		for ( const Buf & b : bufs )
			if ( b.name == "cellCluster" )
				return lit != 0;
		return false;
	}
};
const int kNormUnit = 8;  // slice 3: the look's pcTexN
const int kLightsUnit = 9, kClusterUnit = 10;  // L8 term 2a: the lamps' cellLights / cellCluster
const int kTerrUnit = 11;  // L8 terrain air: airTerr
const int kGiUnit = 12;  // L8 term 1: the WW_CELL_PRT variant's cellGiAir
const GLenum kTexBuffer = 0x8C2A;  // GL_TEXTURE_BUFFER
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
			if ( !( jf >> j.lit ) )  // slice 2b's field (absent in an older prep: unlit)
				j.lit = 0;
			j.uni = readUni( dir + "/" + nm + "/uni.txt" );
			{
				std::ifstream bf( dir + "/" + nm + "/buf.txt" );
				Job::Buf b;
				while ( bf >> b.name >> b.fmt >> b.texels >> b.file )
					j.bufs.push_back( b );
				std::ifstream gf( dir + "/" + nm + "/global.bin", std::ios::binary );
				if ( gf ) {
					j.global.assign( 336, 0 );
					if ( !gf.read( j.global.data(), 336 ) )
						return fatal( nm + "/global.bin" );
				}
			}
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
		|| !load( compressedTexImage2D, "glCompressedTexImage2D" ) || !load( clearBufferfv, "glClearBufferfv" )
		|| !load( blendEquationi, "glBlendEquationi" ) || !load( disablei, "glDisablei" ) )
		return fatal( "GL entry points" );
	std::printf( "PRECIPVIEW GL on %s\n", reinterpret_cast<const char *>( glGetString( GL_RENDERER ) ) );
	glPixelStorei( GL_PACK_ALIGNMENT, 1 );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	const GLuint pUnpack = program( kTriVs, kUnpackFs, "unpack" ), pSlab = program( kTriVs, kSlabFs, "slab" ),
		pReveal = program( kTriVs, kRevealFs, "reveal" ), pCapBlend = program( kTriVs, kCapBlendFs, "1x blend" ),
		pProbe = program( kTriVs, kProbeFs, "blend probe" );
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
	// L8 term 1 (VKFAR5): the loader's GI variants of the plain program (WW_CELL_PRT, the red; WW_CELL_RTDDGI, the RT GI on)
	const GLuint pPrt = program3( readFile( dir + "/prog_prt.vert.glsl" ), readFile( dir + "/prog_prt.geom.glsl" ),
		readFile( dir + "/prog_prt.frag.glsl" ), "cell_precip_gpu (WW_CELL_PRT)" );
	const GLuint pRt = program3( readFile( dir + "/prog_rtddgi.vert.glsl" ), readFile( dir + "/prog_rtddgi.geom.glsl" ),
		readFile( dir + "/prog_rtddgi.frag.glsl" ), "cell_precip_gpu (WW_CELL_RTDDGI)" );
	const GLuint pTfRt = program3( readFile( dir + "/prog_rtddgi.vert.glsl" ), readFile( dir + "/prog_rtddgi.geom.glsl" ),
		readFile( dir + "/prog_rtddgi.frag.glsl" ), "cell_precip_gpu (WW_CELL_RTDDGI, captured)", true );
	// (the RT GI variant's twin of the default: a program as fresh as it, so no earlier case's uniforms ride on one side only)
	const GLuint pTfNew = program3( readFile( dir + "/prog.vert.glsl" ), readFile( dir + "/prog.geom.glsl" ),
		readFile( dir + "/prog.frag.glsl" ), "cell_precip_gpu (captured, fresh)", true );
	if ( !pUnpack || !pSlab || !pReveal || !pCapBlend || !pProbe || !pPrecip || !pLook || !pTf || !pNoFl || !pLookNoFl || !pPrt || !pRt || !pTfRt || !pTfNew )
		return fatal( "GL programs" );
	for ( const GLuint prog : { pPrecip, pLook, pTf, pNoFl, pLookNoFl, pPrt, pRt, pTfRt, pTfNew } ) {  // every sampler its own empty unit from 20 (unit 0's default mixes sampler
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
	// the per-sample reference's targets: the capture (4x RGBA32F on refDs: the slab's depth) and the 1x frame's three
	const GLuint capCol = msTex( W, H, fRgba32f, S );
	const GLuint fbCap = fbo( fT2DMS, capCol, 0, refDs );
	GLuint bl[3] = {};
	for ( GLuint & t : bl )
		t = tex2d( WS, H, fRgba16f, GL_RGBA, GL_FLOAT, nullptr, false );
	const GLuint fbBl0 = fbo( GL_TEXTURE_2D, bl[0], 0, 0 );  // R1's probe: the blended frame alone
	GLuint fbBl = 0;
	f.genFramebuffers( 1, &fbBl );
	f.bindFramebuffer( fFb, fbBl );
	for ( int k = 0; k < 3; k++ )
		f.framebufferTexture2D( fFb, fCol0 + GLenum( k ), GL_TEXTURE_2D, bl[k], 0 );
	const GLenum bl3[3] = { fCol0, fCol0 + 1, fCol0 + 2 };
	f.drawBuffers( 3, bl3 );
	if ( f.checkFramebufferStatus( fFb ) != fComplete )
		fbBl = 0;
	if ( !fbFrame || !fbRef || !fbUnp || !fbCap || !fbBl0 || !fbBl )
		return fatal( "GL targets" );
	// uniforms.glsl's block (the geometry stage includes it; unlit and unfogged it reads none of it): zeros
	std::vector<char> zeros( 336, 0 );
	GLuint ubo = 0;
	f.genBuffers( 1, &ubo );
	f.bindBuffer( fUbo, ubo );
	f.bufferData( fUbo, std::ptrdiff_t( zeros.size() ), zeros.data(), fStatic );
	for ( const GLuint prog : { pPrecip, pLook, pTf, pNoFl, pLookNoFl, pPrt, pRt, pTfRt, pTfNew } ) {
		const GLuint bi = f.getUniformBlockIndex( prog, "globalUniforms" );
		if ( bi != 0xFFFFFFFFu )
			f.uniformBlockBinding( prog, bi, 0 );
	}
	GLuint vao = 0;
	genVertexArrays( 1, &vao );
	constexpr int kCases = 19;
	static char twinKeys[kCases], normKeys[kCases];  // one atlas twin a case (the atlases' sizes differ), one normal-map twin a case
	static char terrKeys[kCases];  // L8 terrain air: airTerr's twin a case
	static char airKeys[kCases][5], volKeys[kCases];  // the air's twins a case, its volume's shared target a case
	if ( jobs.size() > size_t( kCases ) )
		return fatal( "more than " + std::to_string( kCases ) + " cases" );
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
	} reds[12] = { { "cull (radius 0)", 1, false }, { "corners (the strip's last two swapped)", 2, false }, { "near fade (dropped)", 4, false },
		{ "air (dropped)", 8, false }, { "look (dropped: the plain pipelines)", 16, false }, { "atlas (dropped: the zero texture)", 32, false },
		{ "light (dropped)", 128, false }, { "lamps' lights (dropped: the zero texel buffer)", 256, false },
		{ "cluster's globals (dropped: b0 zero)", 512, false }, { "cluster lists (dropped: the zero texel buffer)", 1024, false },
		{ "local sun (dropped: vpSun.w 0)", 2048, false }, { "terrain air (dropped: the zero airTerr)", 4096, false } };
	using TexBufferFn = void( WINAPI * )( GLenum, GLenum, GLuint );
	const auto texBuffer = reinterpret_cast<TexBufferFn>( wglGetProcAddress( "glTexBuffer" ) );
	if ( !texBuffer )
		return fatal( "no glTexBuffer" );
	for ( Job & j : jobs ) {
		if ( j.hasTex && !uploadAtlas( dir + "/" + j.name, j.tex[size_t( j.atlasAt )] ) )
			return fatal( j.name + ": the atlas upload" );
		for ( Job::Buf & b : j.bufs ) {  // L8 term 2a: a texture buffer as celllights.cpp makes it (glTexBuffer on a GL buffer)
			const GLenum ifmt = b.fmt == "rgba32f" ? fRgba32f : b.fmt == "r32i" ? GLenum( 0x8235 ) : 0;
			const size_t bytes = size_t( b.texels ) * ( b.fmt == "r32i" ? 4 : 16 );
			std::vector<char> data( bytes );
			std::ifstream bin( dir + "/" + j.name + "/" + b.file, std::ios::binary );
			if ( !ifmt || !bin.read( data.data(), std::streamsize( bytes ) ) )
				return fatal( j.name + ": the texture buffer " + b.name );
			f.genBuffers( 1, &b.buf );
			f.bindBuffer( kTexBuffer, b.buf );
			f.bufferData( kTexBuffer, std::ptrdiff_t( bytes ), data.data(), fStatic );
			f.bindBuffer( kTexBuffer, 0 );
			glGenTextures( 1, &b.tex );
			glBindTexture( kTexBuffer, b.tex );
			texBuffer( kTexBuffer, ifmt, b.buf );
			glBindTexture( kTexBuffer, 0 );
		}
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
		const void * uboOver = nullptr;  // the globals check's other b0 (bindJob uploads it in place of the case's)
		const void * vkGlobalOver = nullptr;  // the same bytes as Vulkan's b0 (the globals mirror)
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
			// L8 terrain air: the air's grid at airTerr too (the app binds its pass grid to both; read only with airTerrOn)
			bindUnit( kTerrUnit, fT2DA, j.fog ? j.tex[size_t( j.airAt[3] )].gl : 0 );
			u1i( prog, "airTerr", kTerrUnit );
			if ( !j.fog ) {  // an unfogged case: the air fully off (a program keeps the last fogged case's uniforms)
				static const float zero4[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				u1i( prog, "fogOn", 0 );
				u1i( prog, "volOn", 0 );
				f.uniform4fv( f.getUniformLocation( prog, "atAp" ), 1, zero4 );
			}
			if ( !j.lit ) {  // an unlit case: GL's light chain off (a program keeps a lit case's uniforms)
				u1i( prog, "cellHasDir", 0 );
				u1i( prog, "cellInterior", 0 );
				u1i( prog, "airTerrOn", 0 );
			}
			bindUnit( kNormUnit, GL_TEXTURE_2D, normMap );
			u1i( prog, "pcTexN", kNormUnit );  // (not in the plain program: no location, nothing set)
			bindUnit( kLightsUnit, kTexBuffer, j.bufTex( "cellLights" ) );  // L8 term 2a (none: the zero buffer)
			u1i( prog, "cellLights", kLightsUnit );
			bindUnit( kClusterUnit, kTexBuffer, j.bufTex( "cellCluster" ) );
			u1i( prog, "cellCluster", kClusterUnit );
			f.useProgram( prog );
			f.bindBuffer( fUbo, ubo );  // L8 term 2b: the case's own b0 (zeros but the cluster case's projection)
			f.bufferData( fUbo, std::ptrdiff_t( zeros.size() ), uboOver ? uboOver : j.global.empty() ? zeros.data() : j.global.data(), fStatic );
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
		std::vector<std::uint16_t> refPrt;  // L8 term 1: the WW_CELL_PRT variant's frame, its GI fed (the GI case)
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
		if ( j.lit ) {  // slice 2b's seen-checks (header): the light seen, thin vs thick seen, no NaN / Inf, the sun's names found
			auto drawGl = [&]( int mode ) {  // 1 cellHasDir 0, 2 pcTrans 1.0, 3 volTerms 1, 10 + k cellLightCount k; set back after
				before( fbRef, j.zd );
				bindJob( pJob );
				const float thick = 1.0f;
				if ( mode == 1 )
					u1i( pJob, "cellHasDir", 0 );
				else if ( mode == 2 )
					f.uniform1fv( f.getUniformLocation( pJob, "pcTrans" ), 1, &thick );
				else if ( mode == 3 )
					u1i( pJob, "volTerms", 1 );
				else if ( mode == 7 )  // L8 terrain air: the grid off (the absolute anchor)
					u1i( pJob, "airTerrOn", 0 );
				else if ( mode == 4 )  // L8 term 2b: the lists off (the flat loop)
					u1i( pJob, "cellClusterOn", 0 );
				else if ( mode == 5 ) {  // L8 term 2b: b0 all zero (no projection: vpLightsBegin walks every light)
					f.bindBuffer( fUbo, ubo );
					f.bufferData( fUbo, std::ptrdiff_t( zeros.size() ), zeros.data(), fStatic );
				} else if ( mode == 8 ) {  // L8 term 1: the WW_CELL_PRT variant, its GI fed (cellGiAir one bright constant)
					static GLuint giTex = 0;
					if ( !giTex ) {
						const float c[4] = { 0.9f, 0.7f, 0.5f, 1.0f };
						std::vector<float> d( 2 * 2 * 6 * 4 );
						for ( size_t i = 0; i < d.size(); i++ )
							d[i] = c[i % 4];
						glGenTextures( 1, &giTex );
						glBindTexture( fT3D, giTex );
						f.texImage3D( fT3D, 0, GLint( fRgba32f ), 2, 2, 6, 0, GL_RGBA, GL_FLOAT, d.data() );
						glTexParameteri( fT3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
						glTexParameteri( fT3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
						glBindTexture( fT3D, 0 );
					}
					bindJob( pPrt );
					bindUnit( kGiUnit, fT3D, giTex );
					u1i( pPrt, "cellGiAir", kGiUnit );
					u1i( pPrt, "cellGiOn", 1 );
					u1i( pPrt, "cellGiAirOn", 1 );
					const float org[3] = { 0.0f, 0.0f, 0.0f }, dims[3] = { 2.0f, 2.0f, 1.0f }, vox = 1000.0f;
					f.uniform3fv( f.getUniformLocation( pPrt, "cellGiAirOrigin" ), 1, org );
					f.uniform3fv( f.getUniformLocation( pPrt, "cellGiAirDims" ), 1, dims );
					f.uniform1fv( f.getUniformLocation( pPrt, "cellGiAirVoxel" ), 1, &vox );
				} else if ( mode == 6 ) {  // L8 term 3: the local law off (vpSun.w 0)
					const float off[4] = { 0.5f, 0.4f, 0.3f, 0.0f };
					f.uniform4fv( f.getUniformLocation( pJob, "vpSun" ), 1, off );
				} else
					u1i( pJob, "cellLightCount", mode - 10 );
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
				glDrawArrays( GL_POINTS, j.first, j.n );
				bindVertexArray( 0 );
				drawState0();
				glDepthFunc( GL_LESS );
				std::vector<std::uint16_t> out = readSamples( refCol );
				bindJob( pJob );  // the case's own uniforms again
				return out;
			};
			const Verdict dark = judge( bg, drawGl( 1 ), refA, refB, refB );
			const Verdict thick = judge( bg, drawGl( 2 ), refA, refB, refB );
			size_t bad = 0;
			for ( std::uint16_t hv : refA )
				bad += ( hv & 0x7C00 ) == 0x7C00;
			const char * sunNames[] = { "cellOn", "cellHasDir", "cellInterior", "cellDirColor", "cellDirTo", "cellRow[0]", "volTerms",
				"volPhase[0]", "volGi", "pcTrans", "volVeil", "airMed", "airLayer", "airGround" };
			std::string missing;
			for ( int k = 0; k < int( std::size( sunNames ) ) - ( j.fog ? 0 : 4 ); k++ )
				if ( f.getUniformLocation( pJob, sunNames[k] ) < 0 )
					missing += std::string( missing.empty() ? "" : ", " ) + sunNames[k];
			std::printf( "PRECIPVIEW %s lit checks: GL with cellHasDir 0 differs %zu of %zu covered samples, pcTrans 1.0 vs 0.5 %zu (each want >= 1000); "
				"NaN / Inf halves in GL's lit frame %zu (want 0); sun-branch names missing from GL's program: %s\n", j.name.c_str(), dark.flips, dark.covered,
				thick.flips, bad, missing.empty() ? "none" : missing.c_str() );
			ok = ok && dark.flips >= 1000 && thick.flips >= 1000 && bad == 0 && missing.empty();
			if ( j.localSun() ) {  // L8 term 3's seen-checks (header): the law seen, vpSun found
				const Verdict noLaw = judge( bg, drawGl( 6 ), refA, refB, refB );
				const bool found = f.getUniformLocation( pJob, "vpSun" ) >= 0;
				std::printf( "PRECIPVIEW %s local sun checks: GL with vpSun.w 0 differs %zu of %zu covered samples (want >= 1000); vpSun %s in GL's program\n",
					j.name.c_str(), noLaw.flips, noLaw.covered, found ? "found" : "MISSING" );
				ok = ok && noLaw.flips >= 1000 && found;
			}
			if ( j.terrAir() ) {  // L8 terrain air's seen-checks (header): the grid seen, its names found
				const Verdict flatGround = judge( bg, drawGl( 7 ), refA, refB, refB );
				std::string tmiss;
				for ( const char * nm : { "airTerr", "airTerrOn", "airTerrXf", "airTerrFineXf", "airTerrRef" } )
					if ( f.getUniformLocation( pJob, nm ) < 0 )
						tmiss += std::string( tmiss.empty() ? "" : ", " ) + nm;
				std::printf( "PRECIPVIEW %s terrain air checks: GL with airTerrOn 0 differs %zu of %zu covered samples (want >= 1000); terrain names "
					"missing from GL's program: %s\n", j.name.c_str(), flatGround.flips, flatGround.covered, tmiss.empty() ? "none" : tmiss.c_str() );
				ok = ok && flatGround.flips >= 1000 && tmiss.empty();
			}
			if ( j.cluster() ) {  // L8 term 2b's seen-checks (header): the lists seen, b0's projection read, the names, volTerms 3 vs 1
				const Verdict noLamps = judge( bg, drawGl( 3 ), refA, refB, refB );
				const Verdict flat = judge( bg, drawGl( 4 ), refA, refB, refB );
				const Verdict noB0 = judge( bg, drawGl( 5 ), refA, refB, refB );
				std::string cmiss;
				for ( const char * nm : { "cellClusterOn", "cellCluster", "cellClusterDims", "cellClusterOrigin", "cellClusterDepth", "cellClusterLog",
						"cellClusterBase", "volRect" } )
					if ( f.getUniformLocation( pJob, nm ) < 0 )
						cmiss += std::string( cmiss.empty() ? "" : ", " ) + nm;
				std::printf( "PRECIPVIEW %s cluster checks: GL with volTerms 1 differs %zu of %zu covered samples, cellClusterOn 0 %zu, b0 all zero %zu "
					"(each want >= 1000); cluster names missing from GL's program: %s\n", j.name.c_str(), noLamps.flips, noLamps.covered, flat.flips,
					noB0.flips, cmiss.empty() ? "none" : cmiss.c_str() );
				ok = ok && noLamps.flips >= 1000 && flat.flips >= 1000 && noB0.flips >= 1000 && cmiss.empty();
			} else if ( j.lamps() ) {  // L8 term 2a's seen-checks (header): the lamps seen, each shape seen, the far one unseen
				const Verdict noLamps = judge( bg, drawGl( 3 ), refA, refB, refB );
				std::vector<std::uint16_t> prev = drawGl( 10 );
				size_t shape[5] = {}, farDiff = 0;
				for ( int k = 1; k <= 5; k++ ) {
					std::vector<std::uint16_t> cur = drawGl( 10 + k );
					const Verdict s = judge( bg, cur, prev, refB, refB );
					shape[k - 1] = s.flips;
					if ( k == 5 )
						farDiff = s.diff;
					prev.swap( cur );
				}
				std::string lmiss;
				for ( const char * nm : { "cellLightCount", "cellLights", "cellShadowOn", "cellGoboOn", "cellClusterOn", "cellLightPhys", "cellRed" } )
					if ( f.getUniformLocation( pJob, nm ) < 0 )
						lmiss += std::string( lmiss.empty() ? "" : ", " ) + nm;
				std::printf( "PRECIPVIEW %s lamp checks: GL with volTerms 1 differs %zu of %zu covered samples (want >= 1000); light k vs k - 1: omni %zu, "
					"spot %zu, hemisphere %zu, box %zu (each want >= 1000), out of reach %zu flips / %zu halves (want 0); lamp names missing from GL's "
					"program: %s\n", j.name.c_str(), noLamps.flips, noLamps.covered, shape[0], shape[1], shape[2], shape[3], shape[4], farDiff,
					lmiss.empty() ? "none" : lmiss.c_str() );
				ok = ok && noLamps.flips >= 1000 && shape[0] >= 1000 && shape[1] >= 1000 && shape[2] >= 1000 && shape[3] >= 1000 && shape[4] == 0
					&& farDiff == 0 && lmiss.empty();
			}
			if ( j.gi() ) {  // L8 term 1's seen-check (header): the WW_CELL_PRT variant with its GI fed against GL's default frame
				refPrt = drawGl( 8 );
				const Verdict seen = judge( bg, refPrt, refA, refB, refB );
				std::string pmiss;
				for ( const char * nm : { "cellGiOn", "cellGiAir" } )
					if ( f.getUniformLocation( pPrt, nm ) < 0 )
						pmiss += std::string( pmiss.empty() ? "" : ", " ) + nm;
				std::printf( "PRECIPVIEW %s GI seen: the WW_CELL_PRT variant with its GI fed differs from GL's default frame %zu of %zu covered samples "
					"(want >= 1000); its GI names missing: %s\n", j.name.c_str(), seen.flips, seen.covered, pmiss.empty() ? "none" : pmiss.c_str() );
				ok = ok && seen.flips >= 1000 && pmiss.empty();
			}
		}

		// the per-sample reference (header): GL's program per particle with blending off, blended per sample at 1x
		std::vector<std::uint16_t> refPS, refPSB;
		std::vector<char> mergedAt( size_t( W ) * H, 0 );
		size_t refRep = 0;  // halves the reference's second build differs in
		{
			const double r0 = nowMs();
			auto readBl = [&]( int k ) {
				std::vector<std::uint16_t> out( size_t( WS ) * H * 4 );
				f.bindFramebuffer( fFb, fbBl );
				glReadBuffer( fCol0 + GLenum( k ) );
				glReadPixels( 0, 0, WS, H, GL_RGBA, fHalf, out.data() );
				glReadBuffer( fCol0 );
				return out;
			};
			auto loadBg = [&]() {  // the frame before the particles into the blended frame and the last-written one
				for ( int k : { 0, 2 } ) {
					glBindTexture( GL_TEXTURE_2D, bl[k] );
					glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, WS, H, GL_RGBA, fHalf, bg.data() );
				}
				glBindTexture( GL_TEXTURE_2D, 0 );
			};
			auto halvesDiff = [&]( const std::vector<std::uint16_t> & x, const std::vector<std::uint16_t> & y ) {
				size_t n = x.size() == y.size() ? 0 : x.size() + 1;
				for ( size_t i = 0; i < std::min( x.size(), y.size() ); i++ )
					n += x[i] != y[i];
				return n;
			};
			auto probe = [&]() {
				glEnable( GL_BLEND );
				glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
				f.useProgram( pProbe );
				glDrawArrays( GL_TRIANGLES, 0, 3 );
				drawState0();
			};
			// R1: one constant blended over the frame before, at 1x and at 4x
			loadBg();
			f.bindFramebuffer( fFb, fbBl0 );
			glViewport( 0, 0, WS, H );
			drawState0();
			probe();
			std::vector<std::uint16_t> p1( bg.size() );
			glReadPixels( 0, 0, WS, H, GL_RGBA, fHalf, p1.data() );
			before( fbRef, j.zd );
			f.bindFramebuffer( fFb, fbRef );
			glViewport( 0, 0, W, H );
			drawState0();
			probe();
			glDepthFunc( GL_LESS );
			const size_t r1 = halvesDiff( p1, readSamples( refCol ) );
			// R2's GL side: every particle in one 4x draw, blending off (the last written output a sample)
			before( fbRef, j.zd );
			bindJob( pJob );
			f.bindFramebuffer( fFb, fbRef );
			glViewport( 0, 0, W, H );
			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_LEQUAL );
			glDepthMask( GL_FALSE );
			bindVertexArray( vao );
			glDrawArrays( GL_POINTS, j.first, j.n );
			bindVertexArray( 0 );
			drawState0();
			glDepthFunc( GL_LESS );
			const std::vector<std::uint16_t> off4 = readSamples( refCol );
			// the loop (refDs holds the slab from before(): the capture's depth test reads it, writes none); run twice, the
			// second build is the repeat clause's (GL's 4x frame varies run to run on the merged pixels)
			auto perSample = [&]() {
			loadBg();
			f.bindFramebuffer( fFb, fbBl );
			static const float zero4[4] = { 0.0f, 0.0f, 0.0f, 0.0f }, none[4] = { 0.0f, 0.0f, 0.0f, -1.0f };
			clearBufferfv( GL_COLOR, 1, zero4 );
			u1i( pCapBlend, "cap", 16 );
			u1i( pCapBlend, "S", S );
			bindUnit( 16, fT2DMS, capCol );
			bindJob( pJob );
			for ( int i = 0; i < j.n; i++ ) {
				f.bindFramebuffer( fFb, fbCap );
				glViewport( 0, 0, W, H );
				drawState0();
				clearBufferfv( GL_COLOR, 0, none );
				f.useProgram( pJob );
				glEnable( GL_DEPTH_TEST );
				glDepthFunc( GL_LEQUAL );
				bindVertexArray( vao );
				glDrawArrays( GL_POINTS, j.first + i, 1 );
				bindVertexArray( 0 );
				f.bindFramebuffer( fFb, fbBl );
				glViewport( 0, 0, WS, H );
				drawState0();
				glEnable( GL_BLEND );
				glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
				blendEquationi( 1, 0x8008 );  // GL_MAX: the split flag
				disablei( GL_BLEND, 2 );
				f.useProgram( pCapBlend );
				glDrawArrays( GL_TRIANGLES, 0, 3 );
			}
			blendEquationi( 1, 0x8006 );  // GL_FUNC_ADD
			drawState0();
			glDepthFunc( GL_LESS );
			bindUnit( 16, fT2DMS, 0 );
			};
			perSample();
			refPS = readBl( 0 );
			const std::vector<std::uint16_t> split = readBl( 1 ), last = readBl( 2 );
			const size_t r2 = halvesDiff( last, off4 );
			const double rMs = nowMs() - r0;
			perSample();
			refRep = halvesDiff( readBl( 0 ), refPS );
			// R3 on the never-split pixels; the merge record on the split ones (a pixel's 4 samples: (p * 4 + s) * 4 + c)
			// (R3 also on GL's draws 2-5: GL's run-to-run variation must sit on split pixels only)
			size_t r3 = 0, r3n = 0, nSplit = 0, merged = 0, mergedMean = 0, splitSame = 0, rep = 0, repFlat = 0;
			bool sizes = refA.size() == refPS.size();
			for ( int k = 0; k < 4; k++ )
				sizes = sizes && refAn[k].size() == refPS.size();
			for ( size_t p = 0; p < size_t( W ) * H && sizes; p++ ) {
				const size_t s0 = p * 4;
				bool sp = false;
				for ( size_t s = 0; s < 4; s++ )
					sp = sp || split[( s0 + s ) * 4] != 0;
				if ( !sp ) {
					for ( size_t i = s0 * 4; i < ( s0 + 4 ) * 4; i++ ) {
						r3 += refPS[i] != refA[i];
						for ( int k = 0; k < 4; k++ )
							r3n += refPS[i] != refAn[k][i];
					}
					continue;
				}
				nSplit++;
				{  // GL's draw 2 against draw 1 here (info): differs, and GL one value in either draw
					bool d = false, f1 = true, f2 = true;
					for ( size_t i = s0 * 4; i < ( s0 + 4 ) * 4; i++ ) {
						d = d || refA[i] != refA2[i];
						f1 = f1 && refA[i] == refA[s0 * 4 + i % 4];
						f2 = f2 && refA2[i] == refA2[s0 * 4 + i % 4];
					}
					rep += d;
					repFlat += d && ( f1 || f2 );
				}
				bool glFlat = true, refFlat = true, same = true;
				for ( size_t s = 1; s < 4; s++ )
					for ( size_t c = 0; c < 4; c++ ) {
						glFlat = glFlat && refA[( s0 + s ) * 4 + c] == refA[s0 * 4 + c];
						refFlat = refFlat && refPS[( s0 + s ) * 4 + c] == refPS[s0 * 4 + c];
					}
				for ( size_t i = s0 * 4; i < ( s0 + 4 ) * 4; i++ )
					same = same && refPS[i] == refA[i];
				splitSame += same;
				if ( glFlat && !refFlat ) {
					merged++;
					mergedAt[p] = 1;
					bool mean = true;
					for ( size_t c = 0; c < 4; c++ ) {
						float m = 0.0f;
						for ( size_t s = 0; s < 4; s++ )
							m += h2f( refPS[( s0 + s ) * 4 + c] );
						mean = mean && std::abs( ordered( f2h( m * 0.25f ) ) - ordered( refA[s0 * 4 + c] ) ) <= 1;
					}
					mergedMean += mean;
				}
			}
			const bool refOk = r1 == 0 && r2 == 0 && r3 == 0 && r3n == 0 && sizes;
			std::printf( "PRECIPVIEW %s reference: %d particles one by one in %.0f ms; R1 constant blend 1x vs 4x %zu halves differ, R2 blend off "
				"1x vs GL 4x %zu, R3 vs GL's 4x frame on %zu never-split pixels %zu, the same on GL's draws 2-5 %zu (each want 0): %s\n", j.name.c_str(),
				j.n, rMs, r1, r2, size_t( W ) * H - nSplit, r3, r3n, refOk ? "OK" : "FAILED" );
			std::printf( "PRECIPVIEW %s GL merged pixels (info, NVIDIA's 4x blend): of %zu split pixels GL holds one value on %zu where the reference "
				"is split (%zu of them the reference's mean within 1 ulp), %zu equal per sample, %zu other; GL draw 2 vs 1 differs on %zu split pixels, "
				"%zu of them one value in either draw\n", j.name.c_str(), nSplit, merged, mergedMean, splitSame, nSplit - merged - splitSame, rep, repFlat );
			ok = ok && refOk;
			// the stencil is GL's 4x one: its samples take GL's reveal, the rest the reference
			refPSB = refPS;
			for ( size_t s = 0; s < refB.size() / 4 && refB.size() == refPS.size(); s++ )
				if ( refB[s * 4] == 0 && refB[s * 4 + 1] == 0 && refB[s * 4 + 2] == 0 && refB[s * 4 + 3] == 0x3C00 )
					for ( size_t c = 0; c < 4; c++ )
						refPSB[s * 4 + c] = refB[s * 4 + c];
		}

		const GLuint pJobCase = pJob;
		GLuint vkProgOver = 0;  // L8 term 1: Vulkan's run read from another GL program (the WW_CELL_PRT variant)
		auto runVk = [&]( int red, std::vector<std::uint16_t> & a, std::vector<std::uint16_t> & b, bool checkSeed, std::uint8_t * recOut = nullptr ) -> bool {
			const GLuint pJob = vkProgOver ? vkProgOver : pJobCase;
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
			t.look = j.look != 0;
			t.lit = j.lit != 0;
			t.global = vkGlobalOver ? vkGlobalOver : j.global.empty() ? nullptr : j.global.data();  // L8 term 2b: GL's b0 bytes (the app: glcontext.cpp's last upload)
			if ( t.lit )  // precipDraw's order: the switches from GL, the refusals, then the packs
				WwVkPresent::precipLitRead( &ga, pJob, t );
			if ( red & 8192 )  // red 8192 (L8 term 1): the GI program refusal's input dropped
				t.litIn.gi = 0;
			if ( !WwVkPresent::precipAdmits( t, why ) ) {
				std::printf( "PRECIPVIEW precipAdmits refused: %s\n", why.c_str() );
				return false;
			}
			if ( t.lit && ( t.litIn.terms & 2 ) && ( !WwVkPresent::precipTexBufOf( &ga, pJob, "cellLights", t.lights, why )
				|| !WwVkPresent::precipTexBufOf( &ga, pJob, "cellCluster", t.cluster, why ) ) ) {  // precipDraw's (L8 term 2a)
				std::printf( "PRECIPVIEW precipTexBufOf refused: %s\n", why.c_str() );
				return false;
			}
			for ( int a = 0; a < 5 && j.fog; a++ ) {
				if ( !WwVkPresent::fxTexOf( &ga, pJob, kAirTex[a], kAirType[a], t.tex[2 + a], why ) ) {
					std::printf( "PRECIPVIEW fxTexOf refused (%s): %s\n", kAirTex[a], why.c_str() );
					return false;
				}
				t.tex[2 + a].twin = reinterpret_cast<std::uintptr_t>( &airKeys[jobAt][a] );
			}
			if ( t.lit && t.litIn.airTerr > 0 ) {  // precipDraw's (L8 terrain air): GL's airTerr into the lit set's t17
				if ( !WwVkPresent::fxTexOf( &ga, pJob, "airTerr", 2, t.tex[8], why ) ) {
					std::printf( "PRECIPVIEW fxTexOf refused (airTerr): %s\n", why.c_str() );
					return false;
				}
				t.tex[8].twin = reinterpret_cast<std::uintptr_t>( &terrKeys[jobAt] );
			}
			WwVkPresent::fxPack( &ga, pJob, WwVkPresent::kT_precipcs, int( std::size( WwVkPresent::kT_precipcs ) ), t.cs );
			WwVkPresent::fxPack( &ga, pJob, WwVkPresent::kT_precipps, int( std::size( WwVkPresent::kT_precipps ) ), t.ps );
			t.packs = 2;
			if ( j.look ) {  // slice 3: cell_precip_look.prog's run (its fields, pcTexN at tex[7])
				WwVkPresent::fxPack( &ga, pJob, WwVkPresent::kT_preciplookcs, int( std::size( WwVkPresent::kT_preciplookcs ) ), t.cs );
				WwVkPresent::fxPack( &ga, pJob, WwVkPresent::kT_preciplookps, int( std::size( WwVkPresent::kT_preciplookps ) ), t.ps );
				if ( !WwVkPresent::fxTexOf( &ga, pJob, "pcTexN", 0, t.tex[7], why, true ) ) {
					std::printf( "PRECIPVIEW fxTexOf refused (pcTexN): %s\n", why.c_str() );
					return false;
				}
				t.tex[7].twin = reinterpret_cast<std::uintptr_t>( &normKeys[jobAt] );
			}
			if ( j.lit ) {  // slice 2b: the light chain's block, as precipDraw packs it
				WwVkPresent::fxPack( &ga, pJob, WwVkPresent::kT_preciplitcs, int( std::size( WwVkPresent::kT_preciplitcs ) ), t.cs );
				t.litBlock = true;
				if ( checkSeed ) {  // the sun's inputs packed, not zero (header)
					std::string zero;
					for ( const char * nm : { "cellDirColor", "cellDirTo", "cellRow", "volPhase", "volGi", "cellLightCount" } ) {
						if ( !j.lamps() && std::strcmp( nm, "cellLightCount" ) == 0 )
							continue;  // (the sun cases: 0)
						const int at = precipAt( WwVkPresent::kT_preciplitcs, int( std::size( WwVkPresent::kT_preciplitcs ) ), nm );
						bool any = false;
						for ( int b = 0; b < ( std::strcmp( nm, "cellLightCount" ) == 0 ? 4 : 12 ) && at >= 0; b++ )
							any = any || t.cs[at + b] != 0;
						if ( !any )
							zero += std::string( zero.empty() ? "" : ", " ) + nm;
					}
					if ( j.cluster() ) {  // the grid and volRect packed, not zero (cluster bar)
						const struct { const char * nm; int bytes; } cf[5] = {
							{ "cellClusterDims", 16 }, { "cellClusterDepth", 16 }, { "cellClusterLog", 8 }, { "cellClusterBase", 4 }, { "volRect", 16 } };
						for ( const auto & f : cf ) {
							int at = precipAt( WwVkPresent::kT_preciplitcs, int( std::size( WwVkPresent::kT_preciplitcs ) ), f.nm );
							const unsigned char * src = t.cs;
							if ( at < 0 ) {
								at = precipAt( WwVkPresent::kT_precipcs, int( std::size( WwVkPresent::kT_precipcs ) ), f.nm );
							}
							bool any = false;
							for ( int b = 0; b < f.bytes && at >= 0; b++ )
								any = any || src[at + b] != 0;
							if ( !any )
								zero += std::string( zero.empty() ? "" : ", " ) + f.nm;
						}
					}
					if ( j.localSun() ) {  // vpSun packed, not zero (local sun bar)
						const int at = precipAt( WwVkPresent::kT_preciplitcs, int( std::size( WwVkPresent::kT_preciplitcs ) ), "vpSun" );
						bool any = false;
						for ( int b = 0; b < 16 && at >= 0; b++ )
							any = any || t.cs[at + b] != 0;
						if ( !any )
							zero += std::string( zero.empty() ? "" : ", " ) + "vpSun";
					}
					if ( j.terrAir() ) {  // airTerrOn and airTerrXf packed, not zero (terrain air bar)
						for ( const auto & tf : { std::make_pair( "airTerrOn", 4 ), std::make_pair( "airTerrXf", 16 ) } ) {
							const int at = precipAt( WwVkPresent::kT_preciplitcs, int( std::size( WwVkPresent::kT_preciplitcs ) ), tf.first );
							bool any = false;
							for ( int b = 0; b < tf.second && at >= 0; b++ )
								any = any || t.cs[at + b] != 0;
							if ( !any )
								zero += std::string( zero.empty() ? "" : ", " ) + tf.first;
						}
					}
					std::printf( "PRECIPVIEW %s lit packed: zero or absent in the light block: %s\n", j.name.c_str(), zero.empty() ? "none" : zero.c_str() );
					if ( !zero.empty() )
						return false;
				}
			}
			if ( checkSeed ) {
				std::uint32_t seed = 0;
				std::memcpy( &seed, t.cs, 4 );  // pcSeedS at 0
				std::printf( "PRECIPVIEW %s pcSeedS: set %u, fxPack read %u (%s)\n", j.name.c_str(), j.seed, seed, seed == j.seed ? "same bits" : "DIFFERS" );
				if ( seed != j.seed )
					return false;
			}
			t.red = red & ~8192;
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
				const Verdict v = judge( bg, a, refPS, b, refPSB );
				std::printf( "PRECIPVIEW %s %s (%zu samples covered, %zu halves differ, %zu edge flips, %zu halves past 1 ulp outside them, "
					"max %d ulp outside them, stencil differs on %zu; flips by worst half 3-8 / 9-64 / past 64 ulp: %zu / %zu / %zu; "
					"in whole / partly flipped pixels: %zu / %zu)\n", j.name.c_str(), v.word(), v.covered, v.diff, v.flips, v.past1, v.maxu,
					v.stencil, v.flipBy[0], v.flipBy[1], v.flipBy[2], v.flipWhole, v.flipPart );
				ok = ok && v.ok();
				{  // info, never the bar: against GL's 4x frame, and how many of those flipped pixels are GL's merged ones
					const Verdict g4 = judge( bg, a, refA, b, refB );
					size_t fp = 0, fpm = 0;
					for ( size_t p = 0; p < mergedAt.size() && a.size() == refA.size(); p++ ) {
						int m = 0;
						for ( size_t i = p * 16; i < p * 16 + 16; i++ )
							m = std::max( m, std::abs( ordered( a[i] ) - ordered( refA[i] ) ) );
						fp += m > 2;
						fpm += m > 2 && mergedAt[p];
					}
					std::printf( "PRECIPVIEW %s against GL's 4x frame (info): %s (%zu flips, %zu past 1 ulp, stencil %zu); %zu flipped pixels, %zu of them "
						"GL's merged ones\n", j.name.c_str(), g4.word(), g4.flips, g4.past1, g4.stencil, fp, fpm );
				}
				{  // red 64 (harness only): Vulkan's 4 samples averaged per pixel, the reveal's samples kept
					std::vector<std::uint16_t> am( a.size() ), bm( b );
					for ( size_t p = 0; p < a.size() / 16; p++ )
						for ( size_t c = 0; c < 4; c++ ) {
							float m = 0.0f;
							for ( size_t s = 0; s < 4; s++ )
								m += h2f( a[( p * 4 + s ) * 4 + c] );
							for ( size_t s = 0; s < 4; s++ )
								am[( p * 4 + s ) * 4 + c] = f2h( m * 0.25f );
						}
					for ( size_t s = 0; s < b.size() / 4; s++ )
						if ( !( b[s * 4] == 0 && b[s * 4 + 1] == 0 && b[s * 4 + 2] == 0 && b[s * 4 + 3] == 0x3C00 ) )
							for ( size_t c = 0; c < 4; c++ )
								bm[s * 4 + c] = am[s * 4 + c];
					const Verdict r = judge( bg, am, refPS, bm, refPSB );
					const bool must = j.name.find( "_near" ) != std::string::npos;
					std::printf( "PRECIPVIEW %s red 64 (Vulkan's samples averaged): %s (%zu covered, %zu flips, %zu past 1 ulp, stencil %zu)%s\n", j.name.c_str(),
						r.word(), r.covered, r.flips, r.past1, r.stencil, must && r.ok() ? " PASSED in a near case: the gate cannot see it" : "" );
					ok = ok && !( must && r.ok() );
				}
				// repeatability: the judged reference against its second build, GL's stencil against its second draw, Vulkan
				// against a second run (each must be EXACT); GL's 4x colour against its second draw is info (the merge varies)
				std::vector<std::uint16_t> a2, b2;
				const Verdict g = judge( bg, refA2, refA, refB2, refB );
				const bool vkRan = runVk( 0, a2, b2, false );
				const Verdict w = vkRan ? judge( bg, a2, a, b2, b ) : Verdict();
				std::printf( "PRECIPVIEW %s repeat: reference vs its second build %zu halves differ, GL's stencil %zu, Vulkan vs Vulkan %s (%zu halves "
					"differ, %zu flips, stencil %zu); GL 4x vs GL 4x (info) %s (%zu halves differ, %zu flips)\n", j.name.c_str(), refRep, g.stencil,
					vkRan ? w.word() : "FAIL (run failed)", w.diff, w.flips, w.stencil, g.word(), g.diff, g.flips );
				ok = ok && refRep == 0 && g.stencil == 0 && vkRan && w.diff == 0 && w.stencil == 0;
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
					auto capture = [&]( std::vector<float> & out, GLuint & prims, GLuint prog = 0, int terms = 0 ) {
						bindJob( prog ? prog : pTf );
						if ( terms )  // L8 term 1: volTerms other than the case's
							u1i( prog ? prog : pTf, "volTerms", terms );
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
					if ( j.lit && !j.fog ) {  // unfogged, gA is the light: distinct values with unequal channels (header)
						std::vector<std::array<float, 3>> ga3;
						for ( size_t v0 = 0; v0 + size_t( kTfFloats ) <= tf1.size(); v0 += size_t( kTfFloats ) ) {
							const float * q = &tf1[v0 + size_t( kTfAt[6] )];
							if ( q[0] != q[1] || q[1] != q[2] )
								ga3.push_back( { q[0], q[1], q[2] } );
						}
						std::sort( ga3.begin(), ga3.end() );
						const size_t distinct = size_t( std::unique( ga3.begin(), ga3.end() ) - ga3.begin() );
						std::printf( "PRECIPVIEW %s lit gA: %zu distinct captured values with unequal channels (want >= 100)\n", j.name.c_str(), distinct );
						ok = ok && distinct >= 100;
					}
					if ( j.lit ) {  // the globals check (header): b0 filled non-zero moves GL's geometry stage only in the cluster case
						std::vector<float> g( 84 );
						for ( size_t k = 0; k < g.size(); k++ )
							g[k] = 0.37f + 0.11f * float( k % 13 ) - 0.05f * float( k % 7 );
						uboOver = g.data();  // (bindJob uploads b0 itself: the other bytes go through it)
						std::vector<float> tfG;
						GLuint prG = 0;
						capture( tfG, prG );
						uboOver = nullptr;
						f.bindBuffer( fUbo, ubo );
						f.bufferData( fUbo, std::ptrdiff_t( zeros.size() ), j.global.empty() ? zeros.data() : j.global.data(), fStatic );
						size_t gDiff = tfG.size() == tf1.size() ? 0 : std::max( tfG.size(), tf1.size() );
						for ( size_t i = 0; i < std::min( tfG.size(), tf1.size() ); i++ )
							gDiff += std::memcmp( &tfG[i], &tf1[i], 4 ) != 0;
						const bool blk = f.getUniformBlockIndex( pTf, "globalUniforms" ) != 0xFFFFFFFFu;
						std::printf( "PRECIPVIEW %s lit globals: b0 other vs the case's, %u / %u triangles, %zu floats differ (want %s; block %s in GL's program)\n",
							j.name.c_str(), prG, pr1, gDiff, j.cluster() ? "> 0, the cluster reads b0" : "0", blk ? "active" : "not active" );
						ok = ok && ( j.cluster() ? gDiff > 0 : gDiff == 0 ) && prG == pr1 && prG > 0;
						if ( !j.cluster() ) {  // Vulkan's globals mirror (header): the app's b0 must not move a run without the lists
							std::vector<std::uint16_t> a4, b4;
							vkGlobalOver = g.data();
							const bool ran = runVk( 0, a4, b4, false );
							vkGlobalOver = nullptr;
							const char * word = ran ? judge( bg, a4, a, b4, b ).word() : "the run failed";
							std::printf( "PRECIPVIEW %s lit globals on Vulkan: b0 other vs none, against the judged run %s (want EXACT)\n", j.name.c_str(), word );
							ok = ok && ran && std::strcmp( word, "EXACT" ) == 0;
						}
					}
					if ( j.gi() ) {  // L8 term 1's checks (header): GI idle in the default program and the RT GI variant; WW_CELL_PRT refused
						std::string actDef, actRt;
						for ( const char * nm : { "cellGiOn", "cellGi", "cellGiSlots", "cellRooms", "cellGiAir", "cellGiAirOn", "cellDdgiOn", "cellDdgiAtlas" } ) {
							if ( f.getUniformLocation( pJob, nm ) >= 0 )
								actDef += std::string( actDef.empty() ? "" : ", " ) + nm;
							if ( f.getUniformLocation( pRt, nm ) >= 0 || f.getUniformLocation( pTfRt, nm ) >= 0 )
								actRt += std::string( actRt.empty() ? "" : ", " ) + nm;
						}
						std::vector<float> tfT1, tfRt, tfNew;
						GLuint prT1 = 0, prRt = 0, prNew = 0;
						capture( tfT1, prT1, 0, 1 );
						capture( tfRt, prRt, pTfRt );
						capture( tfNew, prNew, pTfNew );
						auto floatsDiff = [&]( const std::vector<float> & x, std::string & by, const std::vector<float> & ref ) {
							size_t d = x.size() == ref.size() ? 0 : std::max( x.size(), ref.size() ), var[8] = {}, ulp[8] = {};
							for ( size_t i = 0; i < std::min( x.size(), ref.size() ); i++ )
								if ( std::memcmp( &x[i], &ref[i], 4 ) != 0 ) {
									d++;
									std::int32_t ia, ib;
									std::memcpy( &ia, &x[i], 4 );
									std::memcpy( &ib, &ref[i], 4 );
									const size_t u = size_t( std::llabs( std::int64_t( ia ) - std::int64_t( ib ) ) );
									const int at = int( i % size_t( kTfFloats ) );
									for ( int q = 0; q < 8; q++ )
										if ( at >= kTfAt[q] && at < kTfAt[q + 1] ) {
											var[q]++;
											ulp[q] = std::max( ulp[q], u );
										}
								}
							const char * nm[8] = { "gl_Position", "gUv", "gQ", "gView", "gCol", "gBox", "gA", "gB" };
							for ( int q = 0; q < 8; q++ )
								if ( var[q] )
									by += " " + std::string( nm[q] ) + " " + std::to_string( var[q] ) + " (max " + std::to_string( ulp[q] ) + " ulp)";
							return d;
						};
						{  // (info) which loose uniforms differ between the gate's own program and a fresh one, both after the case's bindJob
							bindJob( pTfNew );
							bindJob( pTf );
							std::string stale;
							GLint n = 0;
							f.getProgramiv( pTf, fActiveUnis, &n );
							for ( GLint i = 0; i < n; i++ ) {
								char nm[256] = {};
								GLint sz = 0;
								GLenum ty = 0;
								f.getActiveUniform( pTf, GLuint( i ), 256, nullptr, &sz, &ty, nm );
								std::string base( nm );
								if ( base.size() > 3 && base.compare( base.size() - 3, 3, "[0]" ) == 0 )
									base.resize( base.size() - 3 );
								for ( GLint e = 0; e < std::max( sz, 1 ); e++ ) {
									const std::string el = sz > 1 ? base + "[" + std::to_string( e ) + "]" : std::string( nm );
									const GLint la = f.getUniformLocation( pTf, el.c_str() ), lb = f.getUniformLocation( pTfNew, el.c_str() );
									if ( la < 0 || lb < 0 )
										continue;
									float va[16] = {}, vb[16] = {};
									f.getUniformfv( pTf, la, va );
									f.getUniformfv( pTfNew, lb, vb );
									if ( std::memcmp( va, vb, sizeof( va ) ) != 0 )
										stale += " " + el + " (" + std::to_string( va[0] ) + " vs fresh " + std::to_string( vb[0] ) + ")";
								}
							}
							std::printf( "PRECIPVIEW %s GI checks (info): loose uniforms the gate's own program holds unlike a fresh one:%s\n", j.name.c_str(),
								stale.empty() ? " none" : stale.c_str() );
						}
						std::string byT1, byRt, byNew;
						const size_t dT1 = floatsDiff( tfT1, byT1, tf1 ), dRt = floatsDiff( tfRt, byRt, tfNew ), dNew = floatsDiff( tfNew, byNew, tf1 );
						std::printf( "PRECIPVIEW %s GI checks by field: volTerms 5 vs 1:%s; the RT GI variant vs a fresh default program:%s; (info) the fresh "
							"default vs the gate's own (uniforms earlier cases left on it): %zu floats,%s\n", j.name.c_str(), byT1.empty() ? " none" : byT1.c_str(),
							byRt.empty() ? " none" : byRt.c_str(), dNew, byNew.empty() ? " none" : byNew.c_str() );
						ok = ok && prNew == pr1;
						std::printf( "PRECIPVIEW %s GI checks: GI names active in GL's default program: %s; in the RT GI variant: %s (want none); captured "
							"volTerms 5 vs 1 %zu floats differ, the RT GI variant vs a fresh default program %zu (%u / %u / %u triangles; each want 0)\n", j.name.c_str(),
							actDef.empty() ? "none" : actDef.c_str(), actRt.empty() ? "none" : actRt.c_str(), dT1, dRt, pr1, prT1, prRt );
						ok = ok && actDef.empty() && actRt.empty() && dT1 == 0 && dRt == 0 && prT1 == pr1 && prRt == pr1 && pr1 > 0;
						std::vector<std::uint16_t> aP, bP, aR, bR;
						vkProgOver = pPrt;
						why.clear();
						const bool prtRan = runVk( 0, aP, bP, false );
						const std::string prtWhy = why;
						const bool redRan = runVk( 8192, aR, bR, false );
						vkProgOver = 0;
						const bool refused = !prtRan && prtWhy.find( "GI volume" ) != std::string::npos;
						const Verdict r = redRan && !refPrt.empty() ? judge( bg, aR, refPrt, bR, refB ) : Verdict();
						std::printf( "PRECIPVIEW %s GI program refusal: the WW_CELL_PRT variant's lit run %s%s (want refused, a GI volume named); red 8192 (the "
							"refusal's input dropped) against its frame: %s (%zu covered, %zu flips; want FAIL, >= 1000 flips)\n", j.name.c_str(),
							prtRan ? "RAN" : "refused: ", prtRan ? "" : prtWhy.c_str(), redRan ? r.word() : "the run failed", r.covered, r.flips );
						ok = ok && refused && redRan && !r.ok() && r.flips >= 1000;
					}
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
						particleDiff( j.name, tf1, pr1, rec, j.n, M, atl, W, H, a, refPS );
					}
				}
			}
		}
		for ( RedSeen & r : reds ) {
			if ( ( r.red == 16 && !j.look ) || ( r.red == 32 && !j.hasTex ) || ( r.red == 128 && !j.lit ) || ( r.red == 256 && !j.lamps() )
				|| ( ( r.red == 512 || r.red == 1024 ) && !j.cluster() ) || ( r.red == 2048 && !j.localSun() ) || ( r.red == 4096 && !j.terrAir() ) )
				continue;  // a plain run has no look to drop, a case with no atlas no atlas, an unlit one no light, no lamps no lights
			std::vector<std::uint16_t> a, b;
			if ( !runVk( r.red, a, b, false ) ) {
				std::printf( "PRECIPVIEW %s red %s: the run failed (counts as a FAIL)\n", j.name.c_str(), r.name );
				r.seen = true;
				continue;
			}
			const Verdict v = judge( bg, a, refPS, b, refPSB );
			std::printf( "PRECIPVIEW %s red %s: %s (%zu covered, %zu flips, %zu past 1 ulp, stencil differs on %zu)\n", j.name.c_str(), r.name,
				v.word(), v.covered, v.flips, v.past1, v.stencil );
			r.seen = r.seen || !v.ok();
			if ( r.red == 32 && v.ok() ) {  // every case that reads its atlas must see it dropped (the drop, pcLook 1, reads none)
				std::printf( "PRECIPVIEW %s red atlas %s\n", j.name.c_str(), j.look == 1 ? "PASSED: bound, not read (the drop's look reads no texture)"
					: "PASSED: the case does not see its atlas" );
				ok = ok && j.look == 1;
			}
			if ( r.red == 128 && v.ok() ) {  // every lit case must see its light dropped
				std::printf( "PRECIPVIEW %s red light PASSED: the case does not see its light\n", j.name.c_str() );
				ok = false;
			}
			if ( r.red == 256 && v.ok() ) {  // every lamp case must see its lights dropped
				std::printf( "PRECIPVIEW %s red lamps' lights PASSED: the case does not see its lights\n", j.name.c_str() );
				ok = false;
			}
			if ( ( r.red == 512 || r.red == 1024 || r.red == 2048 || r.red == 4096 ) && v.ok() ) {  // the cluster case must see its b0 and its lists dropped
				std::printf( "PRECIPVIEW %s red %s PASSED: the case does not see it\n", j.name.c_str(), r.name );
				ok = false;
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
	{  // the pack-order check (header)
		f.useProgram( pPrecip );
		u1i( pPrecip, "cellHasDir", 1 );
		u1i( pPrecip, "cellInterior", 0 );
		u1i( pPrecip, "volTerms", 1 );
		u1i( pPrecip, "volPartRed", 1 );  // refused for good (header's fixture move)
		f.bindFramebuffer( fFb, fbFrame );
		glViewport( 0, 0, W, H );
		auto probe = [&]( bool packFirst ) {
			WwVkPresent::PrecipRun t;
			t.w = W;
			t.h = H;
			glGetIntegerv( GL_VIEWPORT, t.vp );
			t.color = fc;
			t.first = jobs[0].first;
			t.n = jobs[0].n;
			t.lit = true;
			WwVkPresent::precipLitRead( &ga, pPrecip, t );
			bool drew = false;
			if ( packFirst || WwVkPresent::precipAdmits( t, why ) ) {
				WwVkPresent::fxPack( &ga, pPrecip, WwVkPresent::kT_precipcs, int( std::size( WwVkPresent::kT_precipcs ) ), t.cs );
				WwVkPresent::fxPack( &ga, pPrecip, WwVkPresent::kT_precipps, int( std::size( WwVkPresent::kT_precipps ) ), t.ps );
				WwVkPresent::fxPack( &ga, pPrecip, WwVkPresent::kT_preciplitcs, int( std::size( WwVkPresent::kT_preciplitcs ) ), t.cs );
				t.litBlock = true;
				t.packs = 3;
				drew = WwVkPresent::precipRun( t, why );
			}
			const WwVkPresent::FxStat s = WwVkPresent::takePrecipStat();
			std::printf( "PRECIPVIEW pack order (%s): %s, %d refused, %d packs on refused runs (%s)\n",
				packFirst ? "red: packed first" : "the app's: refusals first", drew ? "DREW" : "refused", s.fails, s.refusedPacks,
				s.failWhy.c_str() );
			return drew ? -1 : s.fails == 1 ? s.refusedPacks : -1;
		};
		const int asked = probe( false ), red = probe( true );
		if ( asked != 0 ) {
			std::printf( "PRECIPVIEW pack order FAILED: a refused run must pay 0 packs\n" );
			ok = false;
		}
		if ( red <= 0 ) {
			std::printf( "PRECIPVIEW red pack order PASSED: the count cannot see packs on a refused run\n" );
			ok = false;
		} else {
			std::printf( "PRECIPVIEW red pack order FAILS as it must (%d packs counted)\n", red );
		}
		u1i( pPrecip, "volPartRed", 0 );
	}
	{  // the census (header): the app's all-on lit defaults, every refusal that would fire (info; 0 is the L8 done line)
		const float drift[4] = { 0.5f, 0.0f, 1.0f, 0.5f }, sun[4] = { 1.0f, 0.9f, 0.8f, 1.0f }, zero4[4] = {};
		struct { const char * name; int interior; } where[2] = { { "exterior", 0 }, { "interior", 1 } };
		for ( const auto & w : where ) {
			f.useProgram( pPrecip );
			u1i( pPrecip, "cellHasDir", 1 - w.interior );          // celllights.cpp: the sun outdoors
			u1i( pPrecip, "cellInterior", w.interior );
			u1i( pPrecip, "volTerms", 15 );                         // cellvolair.cpp 4234: all four terms
			u1i( pPrecip, "volCsm", 1 );                            // cellvolair.cpp 4233: the cascades drawn
			u1i( pPrecip, "airTerrOn", 1 - w.interior );            // cellvolair.cpp 4219: the terrain air outdoors
			f.uniform4fv( f.getUniformLocation( pPrecip, "volAirDrift" ), 1, drift );  // cellvolair.cpp 3575: drift on
			f.uniform4fv( f.getUniformLocation( pPrecip, "vpSun" ), 1, w.interior ? zero4 : sun );  // celllights.cpp 2100
			u1i( pPrecip, "cellShadowOn", 1 );                      // celllights.cpp 1849
			u1i( pPrecip, "cellGoboOn", 1 );                        // celllights.cpp 1904
			u1i( pPrecip, "cellClusterOn", 1 );                     // cellclustergl.cpp 370
			WwVkPresent::PrecipRun t;
			t.lit = true;
			WwVkPresent::precipLitRead( &ga, pPrecip, t );
			const char * whys[24] = {};
			int n = WwVkPresent::precipLitWhys( t.litIn, whys, 24 );
			if ( n < 24 )  // the roof map (header's census roof line): precipAdmits refuses pcRoofOn for every run, lit or not
				whys[n] = "the roof map on (pcRoofOn: a cell with a roof map; precipAdmits, every run)";
			n++;
			std::printf( "PRECIPVIEW census %s (the app's all-on lit defaults): %d refusals (0 is the L8 done line)\n", w.name, n );
			for ( int k = 0; k < n && k < 24; k++ )
				std::printf( "PRECIPVIEW census %s refusal %d: %s\n", w.name, k + 1, whys[k] );
		}
		u1i( pPrecip, "cellInterior", 0 );
		u1i( pPrecip, "volTerms", 1 );
		u1i( pPrecip, "volCsm", 0 );
		u1i( pPrecip, "airTerrOn", 0 );
		f.uniform4fv( f.getUniformLocation( pPrecip, "volAirDrift" ), 1, zero4 );
		f.uniform4fv( f.getUniformLocation( pPrecip, "vpSun" ), 1, zero4 );
		u1i( pPrecip, "cellShadowOn", 0 );
		u1i( pPrecip, "cellGoboOn", 0 );
		u1i( pPrecip, "cellClusterOn", 0 );
	}
	for ( char & k : twinKeys )  // a BC atlas is a farTwin, an uncompressed one a shared target
		WwVkPresent::farTwinDrop( reinterpret_cast<std::uintptr_t>( &k ) ), WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &k ) );
	for ( char & k : normKeys )
		WwVkPresent::farTwinDrop( reinterpret_cast<std::uintptr_t>( &k ) ), WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &k ) );
	for ( int c = 0; c < kCases; c++ ) {
		WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &volKeys[c] ) );
		WwVkPresent::sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &terrKeys[c] ) );
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
