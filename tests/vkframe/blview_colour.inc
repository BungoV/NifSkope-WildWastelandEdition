/* lane VKOBJ4 step 7: the colour arm. GL's own fo4_cellbl (fo4_defaultbl.vert + fo4_cellbl.frag linked from the HLSL SPIR-V
 * set as the app's wwSpvAttach links it: the binaries unchanged, no uniform GL can mirror by name) draws the shade fixture's
 * class (blview_shade.inc: its pool, commands and wwDrawBase 5) with glMultiDrawElementsIndirect into the shared targets;
 * then objBlShade (the app's crossing: GL's framebuffer and fixed state, the mirror through the locs.txt registry, the
 * texture units, the pool, objBlRun) draws the same class through the Vulkan twin into the same targets.
 * State: the rows hold real resident bindless handles (the harness's 12 textures, every slot; texture anisoAt as its
 * test-local anisotropy-1 twin, below), globalUniforms /
 * skinningUniforms on buffers at bindings 0 / 1, a lit LooseUniforms state at the registry's locations (cellOn, the sun
 * cellHasDir, the DALC ambient, a specular map, glow), every active sampler on its own empty unit.
 * BAR (stated before the first run, never widened): depth as the shade bar (DepthCmp EXACT or BOUND). Colour, both
 * RGBA16F targets, over the pixels both sides cover: EXACT (0 values differ in bits), or BOUND (every channel
 * |Vulkan - GL| <= 4e-3 * max(1, |GL|), four half steps at 1; a NaN on one side only is over). The run is lit: >= 2000
 * pixels both cover, GL's colour rgb non-zero on >= 90 % of them. The crossing: one run, 0 failed, objBlShade not refused.
 * Correction (written before the anisotropy-1 run, manager 11-13): the first statement also asked GL's glow target
 * non-zero on >= 50 % of those pixels. fo4_cellbl writes SV_Target1 (cellEmitOut) as the constant 0 (fo4_cellbl_ps.hlsl;
 * only the debug probe cellProbe 60 writes it, and this state never sets it), so no mirror could meet that clause: it was
 * a wrong statement of the subject and is removed, not a wider bound. The 4e-3 bound is unchanged; the glow count stays
 * printed; the glow red still moves colour target 0 through glowMult.
 * Anisotropy (manager 11-13): texture 5 is the fixture's one def at anisotropy 4, and NV GL's anisotropic filter and
 * Vulkan's differ on it (the per-texture reading of the first runs). The bar runs at anisotropy 1 on both sides through a
 * test-local twin of it (blview.cpp: the same def and levels, 0x84FE 1, registered as its arm registers; the Vulkan
 * sampler mirrors GL's own 0x84FE), so it tests the mirroring itself; no shipped code path changes. Texture 5 at the
 * fixture's anisotropy is still read and printed as the known driver difference (a reading, not the bar).
 * Reds (each must FAIL against GL): glow (glowMult 0 after GL drew: the mirror carries GL's state), slot (each row's maps
 * one slot on in the rows given to objBlShade). Each red must also move the Vulkan colour off the Vulkan base (a red
 * that changes nothing tests nothing). No registry red here: NV reflects OpName, so this program's names resolve and
 * the registry is not on its path; the locs gate holds that red on a nameless program.
 * 1-buffer arm (manager 11-13; stated before its first run, never widened): the app's cell frame has one draw buffer while
 * fo4_cellbl writes two (SV_Target1 the constant 0). GL's frame with draw buffer 0 only, both targets cleared to 0.25: GL
 * draws, then objBlShade takes the frame as 1 colour target and objBlPipe builds the twin with SV_Target1 dropped (Vulkan
 * discards an output with no attachment). BAR: not refused, 1 run 0 failed, 1 output dropped; depth vs GL as the shade
 * bar; target 0 vs GL over the pixels both cover as the colour bar above (EXACT, or BOUND 4e-3), >= 2000 of them; target
 * 0 vs the 2-buffer Vulkan run EXACT over those pixels (dropping the output moves nothing); target 1 still 0.25 in every
 * value on both sides. Red strict (ObjBlShade.red 1: objBlPipe's old strict count) must refuse with "the pixel stage writes
 * 2 targets, the frame has 1". Check under: a 3-colour target still refuses ("writes 2 targets, the frame has 3").
 * Gap 4 (manager 11-13; stated before its first run, never widened): twin 1, GL's fo4_cellcsmbl (fo4_cellcsmbl.frag from the
 * set) against the Vulkan fo4_cellcsmbl twin, every arm above the same way with the same bars (2-buffer, 1-buffer, the
 * 4-sample frame per sample, reds glow / slot / strict / unshared / dropped sample), the lines `BLVIEW colourcsm`. The sun
 * cascades are on (the app's unified path: ussOn, ussCsmOn, the Vogel filter, cascade 0 only; terrain, contact and cloud
 * off) and read a shared D16 2D array (sharedTarget type 2, 3 layers of 256, a 32-texel checker of 0.25 / 1.0, LINEAR,
 * compare LEQUAL) on csmMap's unit, so both sides compare the same texels. Lit-by-the-map clause: csmForce 1 (no shadow)
 * moves GL's colour on >= 20 % of the pixels GL covers (the map reaches the colour). Red csmmap: the map's checker
 * inverted after GL drew must FAIL and move the Vulkan colour (Vulkan reads the map, not a stand-in). cellShadow: no
 * light reaches the fixture (the cluster and light units unbound: the zero texel on both sides), so its unit is not read.
 * RT row (lane VKWATER3 ruling 3, 11-15; stated before its first run, never widened): `rt` draws the RT GI variant, GL built on
 * the app's all-on key (blview.sh's spvrt: WW_CELL_FOGLEAK + WW_FRAME_UBO + WW_CELL_RESTIR + RTDDGI + RTDDGI_CASCADES) against
 * the shipped objbl_cell(csm)_rt_ps twins (objBlShade s.rt), the lines `BLVIEW colour rt` / `colourcsm rt`. Inputs as the cell
 * gate's RT row: 3 omni lights over the shapes, each shadowed (texel 4 y 1: the ReSTIR ratio is the shadow), the flat loop;
 * the ratio (GL's RG32F buffer texture; its depth on GL's surfaces, 1 in 8 a miss, 1 in 8 off the surface), the atlas (a
 * store, one cascade over the shapes), the screen GI and its key (shared targets, the key from GL's first draw). BAR: the
 * 2-buffer arm's bars above (depth, colour EXACT or BOUND 4e-3, >= 2000 lit pixels, 1 run 0 failed). Probes: cellLightCount 0,
 * cellRestirOn 0, cellDdgiOn 0, cellGiHalfOn 0 each move GL's colour on >= 20 % of the covered pixels. Reds (each must FAIL
 * and move the Vulkan colour): glow, slot, csmmap as above, then restir (the ratio copied from two texels on), atlas (read a
 * texel on), gihalf / gikey (dropped to the stand-in). The 1-buffer, 4-sample and texture arms stay the base rows'. */
/* lane VKOBJ5 (plan row G4: skinned and alpha-tested, the app's vkBlDraw): colourGate's kind 1 skin / 2 alpha runs the main arm
 * only (GL's draw, objBlShade, depth and colour vs GL, one red), the lines `BLVIEW colour[csm] skin` / `... alpha`.
 * Skin (stated before the first run, never widened): the class gains the bone codes at locations 5 / 6 (bone + weight: 4 bones
 * at 0.5 / 0.25 / 0.125 / 0.0625 on every vertex, a fifth at 0.0625 on the odd ones, bones v % 8 on), 8 distinct bone
 * transforms in skinningUniforms (a turn about z and a shift, row-major 3 x 4), globalUniforms.doSkinning 1 (a uint).
 * GL probe: doSkinning 0 moves GL's depth on >= 20 % of the covered pixels. Red bone (ObjBlShade.skinRed): bone i reads
 * bone i + 1's transform. Alpha: alphaFlags 4 ('>') at alphaThreshold 0.5 over the rows' maps' alpha. GL probe: alphaFlags 0
 * covers >= 5 % more pixels than the test, and the test leaves >= 2000. Red flags (ObjBlShade.alphaRed): the mirrored
 * alphaFlags 0. Bars as the main arm: depth EXACT or BOUND (the word printed), colour EXACT or BOUND 4e-3, lit >= 90 %;
 * the red must FAIL and move the Vulkan colour or depth. */
/* lane VKOBJ5 round 2 (the rigid opaque single draws and the unpooled shapes; stated before the first run, never widened):
 * kind 3 single: GL draws the class one shape at a time as the app's single draw does (base instance 0, wwDrawBase the
 * shape's row, set before each draw), Vulkan crosses once per shape with that one row (rows from it, nRows 1, rowsAtBase).
 * Bars as the main arm, crossing 64 runs 0 failed. Red base (rowsAtBase off: the mirrored wwDrawBase kept, the rows padded
 * so the read stays in the buffer) must FAIL and move: it is the defect the flag fixes. Kind 4 own: GL and Vulkan draw from a
 * shape VAO (no pool): locations 0..4 interleaved fp32 at stride 64 from byte 32 of one buffer, the uv (location 7) two
 * halves at stride 4 from byte 8 of another, the class's index buffer; Vulkan through ownVao. Bars as the main arm, 1 run.
 * Reds offset (every attribute read from offset 0) and half (the halves read as 16-bit unorm) must FAIL and move. Check
 * (VKFLIGHT11): location 1 off at (0.5,0.75,0.25,0.625) and location 5 off at (0,0,0,0) (the flight's rigid weights0): GL's
 * draw moves, Vulkan BOUND to it; red 16 const (the old (0,0,0,1)) must FAIL and move. The kinds run GL on the
 * app's key, as the shipped twins (VKWATER3 ruling 1). */
static int gColTwin = 0;  // colPrintf: 1 prints `BLVIEW colourcsm` for `BLVIEW colour`
static const char * gColTag = "";  // the plain row: `BLVIEW colour plain`; the RT row: `BLVIEW colour rt`
static int gColKind = 0;  // lane VKOBJ5: 1 ` skin`, 2 ` alpha`, 3 ` single`, 4 ` own` after it; lane VKWATER3: 5 ` ao`; VKOBJ19: 6 ` fade`
static const char * kColKindName[9] = { "", " skin", " alpha", " single", " own", " ao", " fade", " ssgi", " stepb" };  // VKOBJ21: 7 ` ssgi`; VKOBJ22: 8 ` stepb`
/* lane VKWATER3 (manager ruling 11-15, the AO note; stated before its first run, never widened): kind 5 `ao` runs the main arm
 * on the app's key with a GL R32F texture on cellAo (half the view, a ramp in y and a 7-texel saw in x, bilinear, clamped),
 * cellAoOn 1, cellAoRect the frame. GL probe: cellAoOn 0 moves GL's colour on >= 20 % of the covered pixels. Ruling item 3:
 * cellAo reads GL's texture through its copy twin, so the main arm draws at the main arm's bars with one copy of the texture's
 * bytes (half the view, 4 bytes a texel). Reds: standin (ObjBlShade.standInRed: no copy twin, the old stand-in) and copy (red 26 =
 * objBlShade's 8: copied from one texel on in x) must FAIL. Check: the same texture on cellSsgi (GL's own, no twin) must refuse
 * naming cellSsgi (`REFUSED OK`). */
/* lane VKOBJ19 (11-16; stated before its first run, never widened): kind 6 `fade` is bsshape.cpp's decal-fade band on the bl route
 * (vkBlDraw tried in the band loop). Kind 3's singles (a crossing a piece, its shape's row, rowsAtBase), alphaFlags 8 (the blend
 * bit only: rigid, no test, no skin), GL_BLEND on with SRC_ALPHA / ONE_MINUS_SRC_ALPHA, each shape's alpha a0 = 1, 0.9, 0.8 in turn.
 * Each shape's command is split by triangles: the first 2/5 kept at a0, the rest three band pieces (the last takes the remainder)
 * at a0 x 0.75, 0.5, 0.25 (a shape under 4 triangles stays whole); GL's loose alpha is set before each piece on both sides (the bl
 * row holds no alpha: objBlLoose mirrors GL's alpha at each crossing). Bars as the main arm (depth EXACT or BOUND, colour EXACT or
 * BOUND 4e-3 of max(1, |GL|), lit >= 0.9 of covered), crossing one run a piece, 0 failed. The frame is the main arm's 1 sample (no
 * 4x blend merge). GL probe: every fade 1 (the bands at a0) moves GL's colour on >= 20 % of the covered pixels. Red fade (red 27:
 * Vulkan's band crossings at a0, unscaled) must FAIL and move the Vulkan colour off its base. Check: the band's constant-alpha mode
 * (glBlendFunc CONSTANT_ALPHA / ONE_MINUS_CONSTANT_ALPHA, effect programs) must refuse naming the constant (`REFUSED OK`). The
 * harness calls objBlShade, not vkBlDraw: vkBlDraw's admission of a blended rigid band is the app's, read in the census. */
/* lane VKOBJ20 (11-16; stated before its first run, never widened): kinds 3 `single` and 6 `fade` on the all-on route's RT GI variant
 * (`colour rt single` / `colour rt fade`, both twins): GL built on the all-on key (blview.sh's spvrt) against the shipped _rt_ps twins
 * (objBlShade's s.rt), the RT inputs of the `colour rt` row (lights, ReSTIR ratio, DDGI atlas, screen GI + key). Bars as kind 3 / kind 6
 * (depth EXACT or BOUND, colour EXACT or BOUND 4e-3 of max(1, |GL|), lit >= 0.9 of covered, one crossing a shape / a piece, 0 failed);
 * GL probes: each RT input off moves GL's colour on >= 20 % of the covered pixels, and kind 6's fade probe as above. Reds that must
 * FAIL with Vulkan's colour moved off its base (a refusal or 0 moved is not a FAIL): base (13, kind 3), fade (27, kind 6), and the RT
 * reds restir / atlas / gihalf / gikey (21 .. 24) on each row. GL's redraws (the RT key's frame, the RT probes) draw in the kind's own
 * form (glDraws: a draw a shape / a piece with its wwDrawBase and alpha); for kind 0 that is the same one multi-draw as before. The
 * harness calls objBlShade, not vkBlDraw: vkBlDraw's variant pick (objBlVariant, the RT twin for the decal-fade pieces only) is
 * the app's, checked by the syntax check and the census, not by this gate. */
/* lane VKOBJ21 (11-16; stated before its first run, never widened): kind 7 `ssgi` is the all-on route's screen-space GI input
 * (WW_RENDER_SSGI=1: cellssgi.cpp binds its result on cellSsgi for every opaque cell-lit draw). Its owner now makes the result a
 * shared target, so the twin reads it in place. The fixture: a shared RGBA16F target of half the view (rgb 0.1 .. 0.9 and a 0.3 .. 1
 * in small saws, bilinear, clamped, one level) on cellSsgi's unit, cellSsgiOn 1, cellSsgiRect the frame, cellSsgiK (1, 1),
 * cellSsgiRed 0; the main arm's form (one multi-draw), on the app's key (spvapp, both twins) and on the all-on RT key (spvrt, both
 * twins). Bars as the main arm (depth EXACT or BOUND, colour EXACT or BOUND 4e-3 of max(1, |GL|), lit >= 0.9 of covered, one
 * crossing, 0 failed), every cellSsgi value with an active location, 0 copies (in place). GL probe: cellSsgiOn 0 moves GL's colour on
 * >= 20 % of the covered pixels. Red ssgi (red 28 = objBlShade's 13: cellSsgi dropped to the stand-in) must FAIL with Vulkan's colour
 * moved off its base; on the RT rows the RT reds 21 .. 24 too. Kinds 1 skin, 2 alpha and 4 own on the RT key (`colour rt skin` /
 * `rt alpha` / `rt own`, both twins): bars, probes and reds as kind 3 on that key (VKOBJ20), each kind's own reds (bone, flags,
 * offset / half) and the RT reds 21 .. 24. These rows run after every older row, which stay byte for byte. */
/* lane VKOBJ22 (11-16; stated before its first run, never widened): kind 8 `stepb` is GPU cull step B on the all-on route
 * (cellgpucull.cpp GpuDrivenB::draw hands a bl shape to vkBlDraw with the CPU cull's kept ranges, no fadeBand, so the RT twin).
 * Each shape keeps ranges by its index c % 4: 0 whole, 1 (0, t/3) + (t/2, t - t/2), 2 (0, t/4) + an empty range + (t/2, t - t/2),
 * 3 (t/5, t - t/5), t its triangles. GL draws every kept range as 8-triangle cluster commands (the GPU cull's form), wwDrawBase the
 * shape's row; Vulkan crosses once a shape with cellgpucullvk.h rangeDraws' commands, the shape's one row (rowsAtBase), as
 * vkBlDraw does. Rows: the RT key (spvrt), both twins. Bars as kind 3 on that key (depth EXACT or BOUND, colour EXACT or BOUND
 * 4e-3 of max(1, |GL|), lit >= 0.9 of covered, one crossing a shape, 0 failed). GL probe: each shape whole (kind 3's form) covers
 * pixels the ranges do not, on > 0 and >= 1 % of the whole draw's covered pixels. Red droprange (29: rangeDraws red 1, the last
 * kept range of each multi-range shape dropped) must FAIL with Vulkan moved off its base; the RT reds 21 .. 24 too. The harness
 * calls objBlShade: vkBlDraw's lifted refusal is the app's, checked by the syntax check and the census. Appended after every
 * older row. */
#include "../../src/features/near/cellgpucullvk.h"  // lane VKOBJ22: step B's ranges (kind 8)
void colPrintf( const char * fmt, ... )
{
	std::string f2( fmt );
	const std::size_t at = f2.find( "colour" );
	if ( at != std::string::npos )
		f2.insert( at + 6, std::string( gColTwin ? "csm" : "" ) + gColTag + kColKindName[gColKind] );
	va_list ap;
	va_start( ap, fmt );
	std::vprintf( f2.c_str(), ap );
	va_end( ap );
}
bool colourGate( const std::string & spirvDir, const std::vector<GLuint64> & fixture, int anisoAt, GLuint64 aniso1, int twin = 0,
	int kind = 0, const std::string & plainSpv = std::string(), bool rt = false )
{
	using namespace WwVkPresent;
	gColTwin = twin;
	gColKind = kind;
	gColTag = plainSpv.empty() ? ( rt ? " rt" : "" ) : " plain";
	const std::uint32_t * psWords = twin ? vkframe_objbl_cellcsm_ps_spv : vkframe_objbl_cell_ps_spv;
	std::size_t psCount = twin ? sizeof( vkframe_objbl_cellcsm_ps_spv ) / 4 : sizeof( vkframe_objbl_cell_ps_spv ) / 4;
	if ( rt ) {  // ruling 3: the RT GI variant's twins (objBlShade's s.rt)
		psWords = twin ? vkframe_objbl_cellcsm_rt_ps_spv : vkframe_objbl_cell_rt_ps_spv;
		psCount = twin ? sizeof( vkframe_objbl_cellcsm_rt_ps_spv ) / 4 : sizeof( vkframe_objbl_cell_rt_ps_spv ) / 4;
	}
	// the plain row (ruling 11-15): the twin with no loader define (blview.sh's pipe form) against GL's plain set
	std::vector<std::uint32_t> plainWords;
	if ( !plainSpv.empty() ) {
		const std::string b = readFile( plainSpv );
		plainWords.resize( b.size() / 4 );
		if ( !plainWords.empty() )
			std::memcpy( plainWords.data(), b.data(), plainWords.size() * 4 );
		if ( plainWords.size() < 5 || plainWords[0] != 0x07230203u ) {
			colPrintf( "BLVIEW colour FAIL (no plain module at %s)\n", plainSpv.c_str() );
			gColTag = "";
			return false;
		}
		psWords = plainWords.data();
		psCount = plainWords.size();
	}
	std::vector<GLuint64> handles = fixture;  // the fixture's handles, texture anisoAt as its anisotropy-1 twin
	handles[std::size_t( anisoAt )] = aniso1;
	struct ColGl
	{
		void( WINAPI * shaderBinary )( GLsizei, const GLuint *, GLenum, const void *, GLsizei ) = nullptr;
		void( WINAPI * specialize )( GLuint, const char *, GLuint, const GLuint *, const GLuint * ) = nullptr;
		void( WINAPI * ifaceIv )( GLuint, GLenum, GLenum, GLint * ) = nullptr;
		void( WINAPI * resIv )( GLuint, GLenum, GLuint, GLsizei, const GLenum *, GLsizei, GLsizei *, GLint * ) = nullptr;
		void( WINAPI * uf[4] )( GLint, GLsizei, const GLfloat * ) = {};
		void( WINAPI * ui1 )( GLint, GLint ) = nullptr;
		void( WINAPI * uu1 )( GLint, GLuint ) = nullptr;
		void( WINAPI * bindTextures )( GLuint, GLsizei, const GLuint * ) = nullptr;
		void( WINAPI * bindSamplers )( GLuint, GLsizei, const GLuint * ) = nullptr;
		void( WINAPI * deleteProgram )( GLuint ) = nullptr;
		void( WINAPI * um4 )( GLint, GLsizei, GLboolean, const GLfloat * ) = nullptr;
		void( WINAPI * clearTexSub )( GLuint, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void * ) = nullptr;
		void( WINAPI * texParami )( GLuint, GLenum, GLint ) = nullptr;
		void( WINAPI * texBuffer )( GLenum, GLenum, GLuint ) = nullptr;
		void( WINAPI * ui[4] )( GLint, GLsizei, const GLint * ) = {};  // ivec2..4 (the RT row's cellDdgiCDims)
	} u;
	const char * n4 = "1234";
	bool okLoad = load( u.shaderBinary, "glShaderBinary" ) && load( u.specialize, "glSpecializeShader" ) && load( u.ifaceIv, "glGetProgramInterfaceiv" )
		&& load( u.resIv, "glGetProgramResourceiv" ) && load( u.ui1, "glUniform1i" ) && load( u.uu1, "glUniform1ui" )
		&& load( u.bindTextures, "glBindTextures" ) && load( u.bindSamplers, "glBindSamplers" ) && load( u.deleteProgram, "glDeleteProgram" )
		&& load( u.um4, "glUniformMatrix4fv" ) && load( u.clearTexSub, "glClearTexSubImage" ) && load( u.texParami, "glTextureParameteri" )
		&& load( u.texBuffer, "glTexBuffer" );
	for ( int i = 0; i < 4; i++ )
		okLoad = okLoad && load( u.uf[i], ( std::string( "glUniform" ) + n4[i] + "fv" ).c_str() )
			&& load( u.ui[i], ( std::string( "glUniform" ) + n4[i] + "iv" ).c_str() );
	if ( !okLoad || !sx.prog ) {
		colPrintf( "BLVIEW colour FAIL (GL entry points missing, or no shade fixture)\n" );
		return false;
	}
	std::string why;
	if ( !objBlLocsLoad( spirvDir + "/locs.txt", why ) ) {
		colPrintf( "BLVIEW colour FAIL (no registry: %s)\n", why.c_str() );
		return false;
	}
	std::map<std::string, std::string> index;
	std::map<std::string, int> reg;
	for ( int k = 0; k < 2; k++ ) {
		std::istringstream in( readFile( spirvDir + ( k ? "/locs.txt" : "/index.txt" ) ) );
		std::string ln;
		while ( std::getline( in, ln ) ) {
			const std::size_t t = ln.find( '\t' );
			if ( t == std::string::npos )
				continue;
			if ( k )
				reg[ln.substr( 0, t )] = std::atoi( ln.c_str() + t + 1 );
			else
				index[ln.substr( 0, t )] = ln.substr( t + 1 );
		}
	}
	// GL's program from the set, the binaries as they are
	const GLuint prog = f.createProgram();
	const char * stage[2] = { "fo4_defaultbl.vert", twin ? "fo4_cellcsmbl.frag" : "fo4_cellbl.frag" };
	for ( int s = 0; s < 2; s++ ) {
		const std::string bin = index.count( stage[s] ) ? readFile( spirvDir + "/" + index[stage[s]] ) : std::string();
		const GLuint sh = f.createShader( s ? kFrag : kVert );
		GLint okS = 0;
		if ( !bin.empty() ) {
			u.shaderBinary( 1, &sh, 0x9551, bin.data(), GLsizei( bin.size() ) );  // SHADER_BINARY_FORMAT_SPIR_V
			u.specialize( sh, "main", 0, nullptr, nullptr );
			f.getShaderiv( sh, kCompiled, &okS );
		}
		if ( !okS ) {
			colPrintf( "BLVIEW colour FAIL (%s did not specialize)\n", stage[s] );
			return false;
		}
		f.attachShader( prog, sh );
	}
	f.linkProgram( prog );
	GLint linked = 0;
	f.getProgramiv( prog, kLinked, &linked );
	if ( !linked ) {
		char log[1024] = {};
		f.getProgramInfoLog( prog, GLsizei( sizeof( log ) - 1 ), nullptr, log );
		colPrintf( "BLVIEW colour FAIL (%s from SPIR-V did not link: %s)\n", stage[1], log );
		return false;
	}
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		colPrintf( "BLVIEW colour FAIL (GL error 0x%x building the program)\n", unsigned( e ) );
		return false;
	}
	// GL's active default-block locations and their types
	std::map<int, GLint> glType;
	GLint nRes = 0;
	u.ifaceIv( prog, 0x92E1, 0x92F5, &nRes );  // UNIFORM, ACTIVE_RESOURCES
	for ( GLint i = 0; i < nRes; i++ ) {
		const GLenum props[4] = { 0x930E, 0x92FB, 0x92FA, 0x92FD };  // LOCATION, ARRAY_SIZE, TYPE, BLOCK_INDEX
		GLint v[4] = { -1, 0, 0, -1 };
		u.resIv( prog, 0x92E1, GLuint( i ), 4, props, 4, nullptr, v );
		for ( GLint e = 0; v[0] >= 0 && v[3] < 0 && e < std::max( v[1], 1 ); e++ )
			glType[v[0] + e] = v[2];
	}
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		colPrintf( "BLVIEW colour FAIL (GL error 0x%x reading the program's %d uniforms)\n", unsigned( e ), int( nRes ) );
		return false;
	}
	{  // the registry against the names NV reflects: a stale or shifted locs.txt shows here
		GLint( WINAPI * loc )( GLuint, const char * ) = nullptr;
		int named = 0, agree = 0;
		std::string firstOff;
		if ( load( loc, "glGetUniformLocation" ) )
			for ( const auto & e : reg ) {
				const GLint l = loc( prog, e.first.c_str() );
				if ( l < 0 )
					continue;
				named++;
				if ( l == e.second )
					agree++;
				else if ( firstOff.empty() )
					firstOff = e.first + " registry " + std::to_string( e.second ) + " GL " + std::to_string( l );
			}
		colPrintf( "BLVIEW colour registry vs GL's names: %d named, %d agree%s%s\n", named, agree, firstOff.empty() ? "" : "; first off: ",
			firstOff.c_str() );
	}
	auto isSampler = []( GLint t ) {
		return ( t >= 0x8B5D && t <= 0x8B64 ) || ( t >= 0x8DC0 && t <= 0x8DD8 ) || ( t >= 0x900C && t <= 0x900F ) || ( t >= 0x9108 && t <= 0x910D );
	};
	f.useProgram( prog );
	int units = 0, inactive = 0;
	std::string putBad;
	std::map<int, int> unitAt;  // a sampler's location -> its unit
	for ( const auto & e : glType )
		if ( isSampler( e.second ) ) {
			unitAt[e.first] = 1 + units;
			u.ui1( e.first, 1 + units++ );  // each on its own empty unit
		}
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		colPrintf( "BLVIEW colour FAIL (GL error 0x%x placing %d samplers on units)\n", unsigned( e ), units );
		return false;
	}
	// the twin's blocks: globalUniforms (binding 0); on the app's key (WW_FRAME_UBO) the csm twin's cascade values sit in
	// wwShadowFrame (GL's binding 2, build_spv.py BLOCKS): put() writes those members there and frameState uploads them
	SpvModule psM;
	const SpvBind * gb = nullptr, * sfB = nullptr;
	if ( spvReflect( psWords, psCount, psM, why ) ) {
		gb = uniBlock( psM, "globalUniforms" );
		sfB = uniBlock( psM, "wwShadowFrame" );
	}
	std::vector<std::uint8_t> sfBytes( sfB ? std::max<std::size_t>( sfB->bytes, 16 ) : 0, 0 );
	int sfPuts = 0;
	auto put = [&]( const char * name, std::initializer_list<float> vals, int elem = 0 ) {
		const auto it = reg.find( name );
		const int at = it == reg.end() ? -1 : it->second + elem;
		if ( at < 0 || !glType.count( at ) ) {
			if ( sfB )
				for ( const SpvMember & m : sfB->members ) {
					if ( m.name != name )
						continue;
					SpvMember me = m;
					me.offset += std::uint32_t( elem ) * m.arrayStride;
					if ( vals.size() == 16 && m.cols == 4 )
						shadePutMat( sfBytes, me, vals.begin() );
					else if ( me.offset + vals.size() * 4 <= sfBytes.size() )
						std::memcpy( sfBytes.data() + me.offset, vals.begin(), std::min<std::size_t>( vals.size(), m.comps ) * 4 );
					sfPuts++;
					return;
				}
			inactive++;
			return;
		}
		const GLint t = glType[at];
		const float * v = vals.begin();
		f.useProgram( prog );  // glUniform writes the current program: a red set after frameOff must still reach it
		if ( t == 0x1406 || ( t >= 0x8B50 && t <= 0x8B52 ) )  // float, vec2..4
			u.uf[t == 0x1406 ? 0 : t - 0x8B50 + 1]( at, 1, v );
		else if ( t == 0x8B5C && vals.size() == 16 )  // mat4, GL column-major as the app's glUniformMatrix4fv
			u.um4( at, 1, GL_FALSE, v );
		else if ( t == 0x1405 )  // uint
			u.uu1( at, GLuint( v[0] ) );
		else if ( t >= 0x8B53 && t <= 0x8B55 ) {  // ivec2..4
			GLint iv[4] = {};
			for ( std::size_t k = 0; k < vals.size() && k < 4; k++ )
				iv[k] = GLint( v[k] );
			u.ui[t - 0x8B53 + 1]( at, 1, iv );
		} else  // int, bool
			u.ui1( at, GLint( v[0] ) );
		if ( glGetError() != GL_NO_ERROR && putBad.empty() )
			putBad = std::string( name ) + " (GL type 0x" + std::to_string( t ) + ")";
	};
	put( "cellOn", { 1 } );
	put( "cellHasDir", { 1 } );
	put( "cellDirTo", { 0.32f, 0.45f, 0.834f } );
	put( "cellDirColor", { 1.6f, 1.4f, 1.1f } );
	put( "cellSunScale", { 1 } );
	put( "cellExtDalc", { 1 } );
	const float dalc[3][4] = { { 0.10f, 0.05f, 0.02f, 0.12f }, { 0.09f, 0.06f, 0.03f, 0.11f }, { 0.12f, 0.04f, 0.02f, 0.10f } };
	for ( int c = 0; c < 3; c++ ) {
		put( "cellDalc", { dalc[c][0], dalc[c][1], dalc[c][2], dalc[c][3] }, c );
		put( "cellRow", { c == 0 ? 1.0f : 0.0f, c == 1 ? 1.0f : 0.0f, c == 2 ? 1.0f : 0.0f, 0.0f }, c );
	}
	put( "specStrength", { 0.7f } );
	put( "specGlossiness", { 0.6f } );
	put( "hasSpecularMap", { 1 } );
	put( "specColor", { 1.0f, 0.9f, 0.8f } );
	put( "hasEmit", { 1 } );
	put( "glowColor", { 0.3f, 0.2f, 0.1f } );
	put( "glowMult", { 1.5f } );
	put( "hasGlowMap", { 1 } );
	put( "alpha", { 1 } );
	put( "fresnelPower", { 5 } );
	put( "uvScale", { 1, 1 } );
	put( "paletteScale", { 1 } );
	put( "cellIsLinear", { 1 } );
	put( "wwDrawBase", { float( sx.base ) } );
	if ( kind == 2 ) {  // lane VKOBJ5: the alpha test '>' 0.5
		put( "alphaFlags", { 4 } );
		put( "alphaThreshold", { 0.5f } );
	}
	if ( kind == 6 )  // lane VKOBJ19: the blend bit only (a rigid blended decal)
		put( "alphaFlags", { 8 } );
	// gap 4: the sun cascades on a shared D16 2D array (cascade 0: u = 0.5 + x / 80, v = 0.5 + y / 80, depth 0.5)
	static int csmKeyAt = 0;
	const std::uintptr_t csmKey = reinterpret_cast<std::uintptr_t>( &csmKeyAt );
	GLuint csmTex = 0;
	int csmUnit = -1;
	auto csmFill = [&]( bool invert ) {
		for ( int l = 0; l < 3; l++ )
			for ( int by = 0; by < 8; by++ )
				for ( int bx = 0; bx < 8; bx++ ) {
					const float d = ( ( ( bx + by ) & 1 ) != 0 ) != invert ? 0.25f : 1.0f;
					u.clearTexSub( csmTex, 0, bx * 32, by * 32, l, 32, 32, 1, 0x1902, GL_FLOAT, &d );  // DEPTH_COMPONENT
				}
	};
	if ( twin ) {
		if ( !sharedTarget( csmKey, 2, 256, 256, 3, 0x81A5, csmTex, why ) ) {
			colPrintf( "BLVIEW colour FAIL (the shared cascade map: %s)\n", why.c_str() );
			return false;
		}
		const GLenum pv[7][2] = { { 0x2801, 0x2601 }, { 0x2800, 0x2601 }, { 0x2802, 0x812F }, { 0x2803, 0x812F }, { 0x8072, 0x812F },
			{ 0x884C, 0x884E }, { 0x884D, 0x0203 } };  // LINEAR, CLAMP_TO_EDGE, COMPARE_REF_TO_TEXTURE, LEQUAL
		for ( const auto & p : pv )
			u.texParami( csmTex, p[0], GLint( p[1] ) );
		csmFill( false );
		GLint( WINAPI * loc )( GLuint, const char * ) = nullptr;
		GLint l = load( loc, "glGetUniformLocation" ) ? loc( prog, "csmMap" ) : -1;
		if ( l < 0 && reg.count( "csmMap" ) )
			l = reg["csmMap"];
		csmUnit = unitAt.count( l ) ? unitAt[l] : -1;
		const float k = 1.0f / 80.0f;
		const float m[16] = { k, 0, 0, 0, 0, k, 0, 0, 0, 0, 0, 0, 0.5f, 0.5f, 0.5f, 1 };
		for ( int c = 0; c < 3; c++ )
			put( "csmMat", { m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15] }, c );
		put( "csmParams", { 1.0f, 1000.0f, 256.0f, 10.0f } );
		put( "csmSplitOffset", { 1000.0f, 2000.0f, 0.0f, 0.0f } );
		put( "csmForce", { -1.0f } );
		put( "csmRed", { 0 } );
		put( "ussOn", { 1 } );
		put( "ussCsmOn", { 1 } );
		put( "ussCsmFilter", { 1.0f, 1.5f, 12.0f, 0.0f } );
		if ( csmUnit < 0 || glGetError() != GL_NO_ERROR ) {
			colPrintf( "BLVIEW colour FAIL (csmMap has no unit (location %d), or a GL error filling the map)\n", int( l ) );
			return false;
		}
	}
	/* the RT row: the lights (8 RGBA32F texels each, view space; cellRow is the identity), the ratio, the atlas (one cascade
	 * of 17 x 9 x 7 probes, spacing 4, over x -32 .. 32, y -16 .. 16, z -32 .. -8: the shapes), the screen GI and its key */
	static int giHalfAt = 0, giKeyAt = 0;
	const std::uintptr_t giHalfKey = reinterpret_cast<std::uintptr_t>( &giHalfAt ), giKeyKey = reinterpret_cast<std::uintptr_t>( &giKeyAt );
	GLuint lightTex = 0, lightBuf = 0, restirTex = 0, restirBuf = 0, atlasTex = 0, giHalfTex = 0, giKeyTex = 0;
	unsigned atlasBuf = 0;
	int rtUnit[5] = { -1, -1, -1, -1, -1 };  // cellLights, cellRestir, cellDdgiAtlas, cellGiHalf, cellGiHalfKey
	const int hw = kSW / 2, hh = kSH / 2, cdx = 17, cdy = 9, cdz = 7, nP = cdx * cdy * cdz;
	if ( rt ) {
		const float lt[3][8][4] = {
			{ { -10.0f, 5.0f, -14.0f, 28.0f }, { 1.2f, 0.9f, 0.6f, -2.0f }, { 0, 0, -1, 1 }, { 0, 1, 2, 0 }, { -1, 1, 0, 0 } },
			{ { 10.0f, -3.0f, -18.0f, 28.0f }, { 0.6f, 0.9f, 1.4f, -2.0f }, { 0, 0, -1, 1 }, { 0, 1, 2, 0 }, { -1, 1, 0, 0 } },
			{ { 0.0f, 2.0f, -26.0f, 24.0f }, { 0.8f, 1.3f, 0.7f, -2.0f }, { 0, 0, -1, 1 }, { 0, 1, 2, 1 }, { -1, 1, 0, 0 } } };  // no specular
		f.genBuffers( 1, &lightBuf );
		f.bindBuffer( 0x8C2A, lightBuf );  // TEXTURE_BUFFER
		f.bufferData( 0x8C2A, std::ptrdiff_t( sizeof( lt ) ), lt, 0x88E4 );
		std::vector<float> rs( std::size_t( kSW ) * std::size_t( kSH ) * 2, 1.0e6f );  // keyed on GL's surfaces after GL's first draw
		f.genBuffers( 1, &restirBuf );
		f.bindBuffer( 0x8C2A, restirBuf );
		f.bufferData( 0x8C2A, std::ptrdiff_t( rs.size() * 4 ), rs.data(), 0x88E4 );
		f.bindBuffer( 0x8C2A, 0 );
		glGenTextures( 1, &lightTex );
		glBindTexture( 0x8C2A, lightTex );
		u.texBuffer( 0x8C2A, kRgba32fi, lightBuf );
		glGenTextures( 1, &restirTex );
		glBindTexture( 0x8C2A, restirTex );
		u.texBuffer( 0x8C2A, 0x8230, restirBuf );  // RG32F
		// irradiance tiles in 0.3 .. 0.9, distances far (no Chebyshev cut), every probe usable (2)
		std::vector<float> atl( std::size_t( 193 * nP ) * 4, 0.0f );
		std::mt19937 ar( 31 );
		std::uniform_real_distribution<float> a01( 0.0f, 1.0f );
		for ( std::size_t t = 0; t < std::size_t( 64 * nP ); t++ )
			for ( int c = 0; c < 4; c++ )
				atl[t * 4 + std::size_t( c )] = c < 3 ? 0.3f + 0.6f * a01( ar ) : 0.4f + 0.6f * a01( ar );
		for ( std::size_t t = std::size_t( 64 * nP ); t < std::size_t( 192 * nP ); t++ ) {
			atl[t * 4] = atl[t * 4 + 2] = 1000.0f;
			atl[t * 4 + 1] = atl[t * 4 + 3] = 1.0e6f;
		}
		for ( std::size_t t = std::size_t( 192 * nP ); t < std::size_t( 193 * nP ); t++ )
			atl[t * 4 + 3] = 2.0f;
		const std::size_t aBytes = atl.size() * 4;
		if ( !objBlTexelStore( atlasBuf, aBytes, why ) || !objBlTexelStoreWrite( atlasBuf, 0, aBytes, atl.data(), why ) ) {
			colPrintf( "BLVIEW colour FAIL (the atlas store: %s)\n", why.c_str() );
			return false;
		}
		glGenTextures( 1, &atlasTex );
		glBindTexture( 0x8C2A, atlasTex );
		u.texBuffer( 0x8C2A, kRgba32fi, GLuint( atlasBuf ) );
		glBindTexture( 0x8C2A, 0 );
		if ( !sharedTarget( giHalfKey, 0, hw, hh, 1, kRgba32fi, giHalfTex, why ) || !sharedTarget( giKeyKey, 0, hw, hh, 1, kRgba32fi, giKeyTex, why ) ) {
			colPrintf( "BLVIEW colour FAIL (the screen GI's shared targets: %s)\n", why.c_str() );
			return false;
		}
		std::vector<float> gh( std::size_t( hw ) * std::size_t( hh ) * 4 ), gk( gh.size(), -1.0f );
		for ( int y = 0; y < hh; y++ )
			for ( int x = 0; x < hw; x++ ) {
				float * p = gh.data() + ( std::size_t( y ) * std::size_t( hw ) + std::size_t( x ) ) * 4;
				p[0] = 0.2f + 0.075f * float( ( x + 2 * y ) % 9 );
				p[1] = 0.2f + 0.1f * float( ( 3 * x + y ) % 7 );
				p[2] = 0.3f + 0.125f * float( ( x * y ) % 5 );
				p[3] = 0.5f + 0.25f * float( ( x + y ) % 3 );
			}
		for ( int k = 0; k < 2; k++ ) {
			glBindTexture( GL_TEXTURE_2D, k ? giKeyTex : giHalfTex );
			glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, hw, hh, GL_RGBA, GL_FLOAT, k ? gk.data() : gh.data() );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		}
		glBindTexture( GL_TEXTURE_2D, 0 );
		GLint( WINAPI * loc )( GLuint, const char * ) = nullptr;
		load( loc, "glGetUniformLocation" );
		const char * nm[5] = { "cellLights", "cellRestir", "cellDdgiAtlas", "cellGiHalf", "cellGiHalfKey" };
		std::string noUnit;
		for ( int k = 0; k < 5; k++ ) {
			GLint l = loc ? loc( prog, nm[k] ) : -1;
			if ( l < 0 && reg.count( nm[k] ) )
				l = reg[nm[k]];
			rtUnit[k] = unitAt.count( l ) ? unitAt[l] : -1;
			if ( rtUnit[k] < 0 )
				noUnit += std::string( " " ) + nm[k];
		}
		const int inactive0 = inactive;
		put( "cellLightCount", { 3 } );
		put( "cellClusterOn", { 0 } );
		put( "cellShadowOn", { 1 } );
		put( "cellRestirOn", { 1 } );
		put( "cellRestirSize", { float( kSW ), float( kSH ), 0.0f, 0.0f } );
		put( "cellDdgiOn", { 1 } );
		put( "cellDdgiCount", { 1 } );
		put( "cellDdgiCOrigin", { -32.0f, -16.0f, -32.0f, 4.0f }, 0 );  // w: the smallest spacing
		put( "cellDdgiCStep", { 4.0f, 4.0f, 4.0f, 0.1f }, 0 );          // w: the self-shadow bias
		put( "cellDdgiCDims", { float( cdx ), float( cdy ), float( cdz ), float( nP ) }, 0 );
		put( "cellDdgiCBase", { 0 }, 0 );
		put( "cellDdgiRed", { 0 } );
		put( "cellGiLoop8", { 8 } );
		put( "cellGiHalfOn", { 0 } );  // on once the key holds GL's surfaces
		put( "cellGiHalfRect", { 0.0f, 0.0f, float( hw ), float( hh ) } );
		put( "cellGiHalfScale", { 0.5f } );
		colPrintf( "BLVIEW colour RT state: %d of 16 values with no active location or member\n", inactive - inactive0 );
		if ( !noUnit.empty() || glGetError() != GL_NO_ERROR ) {
			colPrintf( "BLVIEW colour FAIL (no unit for:%s; or a GL error making the RT inputs)\n", noUnit.c_str() );
			return false;
		}
	}
	// lane VKWATER3 kind 5: GL's own obscurance texture on cellAo
	GLuint aoTex = 0;
	int aoUnit = -1, ownUnit = -1;  // ownUnit: the refusal check's unit (cellSsgi), else none
	if ( kind == 5 ) {
		const int aw = kSW / 2, ah = kSH / 2;
		std::vector<float> av( std::size_t( aw ) * std::size_t( ah ) );
		for ( int y = 0; y < ah; y++ )
			for ( int x = 0; x < aw; x++ )
				av[std::size_t( y ) * std::size_t( aw ) + std::size_t( x )] = 0.15f + 0.5f * float( y ) / float( ah - 1 ) + 0.3f * float( x % 7 ) / 6.0f;
		glGenTextures( 1, &aoTex );
		glBindTexture( GL_TEXTURE_2D, aoTex );
		glTexImage2D( GL_TEXTURE_2D, 0, 0x822E, aw, ah, 0, 0x1903, GL_FLOAT, av.data() );  // R32F from RED
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F );  // CLAMP_TO_EDGE
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F );
		glTexParameteri( GL_TEXTURE_2D, 0x813D, 0 );  // MAX_LEVEL
		glBindTexture( GL_TEXTURE_2D, 0 );
		GLint( WINAPI * loc )( GLuint, const char * ) = nullptr;
		load( loc, "glGetUniformLocation" );
		GLint l = loc ? loc( prog, "cellAo" ) : -1;
		if ( l < 0 && reg.count( "cellAo" ) )
			l = reg["cellAo"];
		aoUnit = unitAt.count( l ) ? unitAt[l] : -1;
		put( "cellAoOn", { 1 } );
		put( "cellAoRect", { 0.0f, 0.0f, 1.0f / float( kSW ), 1.0f / float( kSH ) } );
		if ( aoUnit < 0 || glGetError() != GL_NO_ERROR ) {
			colPrintf( "BLVIEW colour FAIL (no unit for cellAo, or a GL error making its texture)\n" );
			return false;
		}
	}
	// lane VKOBJ21 kind 7: the SSGI result as its owner makes it under the view (a shared RGBA16F target) on cellSsgi
	static int ssgiAt = 0;
	const std::uintptr_t ssgiKey = reinterpret_cast<std::uintptr_t>( &ssgiAt );
	GLuint ssgiTex = 0;
	int ssgiUnit = -1;
	if ( kind == 7 ) {
		const int sw = kSW / 2, sh = kSH / 2;
		if ( !sharedTarget( ssgiKey, 0, sw, sh, 1, 0x881A, ssgiTex, why ) ) {
			colPrintf( "BLVIEW colour FAIL (the SSGI result's shared target: %s)\n", why.c_str() );
			return false;
		}
		std::vector<float> sv( std::size_t( sw ) * std::size_t( sh ) * 4 );
		for ( int y = 0; y < sh; y++ )
			for ( int x = 0; x < sw; x++ ) {
				float * p = sv.data() + ( std::size_t( y ) * std::size_t( sw ) + std::size_t( x ) ) * 4;
				p[0] = 0.1f + 0.1f * float( ( x + 3 * y ) % 9 );
				p[1] = 0.1f + 0.8f * float( y ) / float( sh - 1 );
				p[2] = 0.1f + 0.2f * float( ( 2 * x + y ) % 5 );
				p[3] = 0.3f + 0.1f * float( ( x + y ) % 8 );
			}
		glBindTexture( GL_TEXTURE_2D, ssgiTex );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, sw, sh, GL_RGBA, GL_FLOAT, sv.data() );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F );  // CLAMP_TO_EDGE
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F );
		glTexParameteri( GL_TEXTURE_2D, 0x813D, 0 );  // MAX_LEVEL
		glBindTexture( GL_TEXTURE_2D, 0 );
		GLint( WINAPI * loc )( GLuint, const char * ) = nullptr;
		load( loc, "glGetUniformLocation" );
		GLint l = loc ? loc( prog, "cellSsgi" ) : -1;
		if ( l < 0 && reg.count( "cellSsgi" ) )
			l = reg["cellSsgi"];
		ssgiUnit = unitAt.count( l ) ? unitAt[l] : -1;
		const int inactive0 = inactive;
		put( "cellSsgiOn", { 1 } );
		put( "cellSsgiRect", { 0.0f, 0.0f, 1.0f / float( kSW ), 1.0f / float( kSH ) } );
		put( "cellSsgiK", { 1.0f, 1.0f } );
		const int inactive3 = inactive;
		put( "cellSsgiRed", { 0 } );
		/* without RT GI (CELL_GI_UP false) cellLit passes cellSsgiE a zero probe, so keep (the only cellSsgiRed read) is
		 * dead and GL drops cellSsgiRed: required active on the RT set only; the first three always */
		const bool redDead = inactive != inactive3;
		colPrintf( "BLVIEW colour ssgi state: unit %d, %d of 3 values with no active location or member; cellSsgiRed %s\n", ssgiUnit,
			inactive3 - inactive0, redDead ? ( rt ? "not read (FAIL on the RT set)" : "not read (dead: zero probe without RT GI)" ) : "read" );
		if ( ssgiUnit < 0 || inactive3 != inactive0 || ( rt && redDead ) || glGetError() != GL_NO_ERROR ) {
			colPrintf( "BLVIEW colour FAIL (no unit for cellSsgi, a cellSsgi value not read, or a GL error making its target)\n" );
			sharedTargetDrop( ssgiKey );
			return false;
		}
	}
	// globalUniforms (binding 0) at the twin's layout, skinningUniforms (binding 1) zero
	if ( !gb ) {
		colPrintf( "BLVIEW colour FAIL (the twin's globalUniforms: %s)\n", why.c_str() );
		return false;
	}
	std::vector<std::uint8_t> gBytes( std::max<std::size_t>( gb->bytes, 16 ), 0 ), sBytes( 256 * 48, 0 );
	const float ident[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	for ( const SpvMember & m : gb->members ) {
		auto vec = [&]( std::initializer_list<float> v, std::uint32_t e = 0 ) {
			std::memcpy( gBytes.data() + m.offset + e * m.arrayStride, v.begin(), std::min<std::size_t>( v.size(), m.comps ) * 4 );
		};
		if ( m.name == "projectionMatrix" )
			shadePutMat( gBytes, m, sx.proj );
		else if ( m.name == "viewMatrix" || m.name == "envMapRotation" )
			shadePutMat( gBytes, m, ident );
		else if ( m.name == "lightSourcePosition" )
			vec( { 0.32f, 0.45f, 0.834f, 0.0f } );
		else if ( m.name == "lightSourceDiffuse" )
			vec( { 1.0f, 0.95f, 0.9f, 1.0f } );
		else if ( m.name == "lightSourceAmbient" )
			vec( { 0.2f, 0.2f, 0.25f, 1.0f } );
		else if ( m.name == "toneMapScale" || m.name == "brightnessScale" || m.name == "glowScale" || m.name == "glowScaleSRGB" )
			vec( { 1.0f } );
		else if ( m.name == "viewportDimensions" ) {
			const std::int32_t vp[4] = { 0, 0, kSW, kSH };
			std::memcpy( gBytes.data() + m.offset, vp, 16 );
		} else if ( m.name == "doSkinning" && kind == 1 ) {  // lane VKOBJ5: a uint, written as one
			const std::uint32_t one = 1;
			std::memcpy( gBytes.data() + m.offset, &one, 4 );
		}
	}
	std::size_t skinAt = 0;  // lane VKOBJ5: doSkinning's offset (the GL probe turns it off)
	for ( const SpvMember & m : gb->members )
		skinAt = m.name == "doSkinning" ? m.offset : skinAt;
	if ( kind == 1 && !skinAt ) {
		colPrintf( "BLVIEW colour FAIL (no globalUniforms.doSkinning in the twin)\n" );
		return false;
	}
	if ( kind == 1 )  // lane VKOBJ5: 8 bones, a turn about z and a shift each (row-major 3 x 4: rows dot (x, y, z, 1))
		for ( int j = 0; j < 8; j++ ) {
			const float a = 0.05f * float( j + 1 ), c = std::cos( a ), s = std::sin( a );
			const float bone[12] = { c, -s, 0, 0.03f * float( j ) - 0.1f, s, c, 0, 0.02f * float( j % 3 ) - 0.02f, 0, 0, 1, 0.01f * float( j % 4 ) };
			std::memcpy( sBytes.data() + std::size_t( j ) * 48, bone, 48 );
		}
	GLuint ubo[3] = {}, rowsBuf = 0, vao = 0;
	f.genBuffers( 3, ubo );
	f.bindBuffer( 0x8A11, ubo[0] );
	f.bufferData( 0x8A11, std::ptrdiff_t( gBytes.size() ), gBytes.data(), 0x88E4 );
	f.bindBuffer( 0x8A11, ubo[1] );
	f.bufferData( 0x8A11, std::ptrdiff_t( sBytes.size() ), sBytes.data(), 0x88E4 );
	f.bindBuffer( 0x8A11, 0 );
	// the rows: the fixture's matrices, 8 real resident handles each (every texture in every slot somewhere)
	std::vector<unsigned char> rows = sx.rows, rowsSlot = sx.rows, rowsT = sx.rows;
	const std::size_t nRows = rows.size() / kBlRowBytes;
	for ( std::size_t r = 0; r < nRows; r++ )
		for ( std::size_t i = 0; i < 8; i++ ) {
			const GLuint64 h = handles[( r * 5 + i * 7 ) % handles.size()], hn = handles[( r * 5 + ( ( i + 1 ) % 8 ) * 7 ) % handles.size()];
			std::memcpy( &rows[r * kBlRowBytes + kBlTexAt + 8 * i], &h, 8 );
			std::memcpy( &rowsSlot[r * kBlRowBytes + kBlTexAt + 8 * i], &hn, 8 );
		}
	std::vector<unsigned char> rowsPad = rows;  // lane VKOBJ5 red base: twice the rows (a read at 2 x the row stays inside)
	rowsPad.insert( rowsPad.end(), rows.begin(), rows.end() );
	f.genBuffers( 1, &rowsBuf );
	f.bindBuffer( kSsbo, rowsBuf );
	f.bufferData( kSsbo, std::ptrdiff_t( rows.size() ), rows.data(), 0x88E4 );
	f.bindBuffer( kSsbo, 0 );
	// the vertex attributes at their TEXCOORD locations, the class's index buffer
	f.genVertexArrays( 1, &vao );
	f.bindVertexArray( vao );
	for ( int i = 0; i < 16; i++ ) {
		const int n = int( ( sx.pool.attrMask >> ( 4 * i ) ) & 15 );
		if ( !n )
			continue;
		f.bindBuffer( 0x8892, sx.pool.vbo[i] );
		sf.vertexAttribPointer( GLuint( i ), n, GL_FLOAT, GL_FALSE, 0, nullptr );
		sf.enableVertexAttribArray( GLuint( i ) );
	}
	// lane VKOBJ5: the skin kind's bone codes at locations 5 / 6 (the class's own buffers there, as the app's pool has them)
	ObjBlPool kindPool = sx.pool;
	GLuint boneBuf[2] = {};
	if ( kind == 1 ) {
		std::vector<float> codes[2];
		for ( std::size_t v = 0; v < sx.pool.verts; v++ ) {
			const float b = float( v % 8 );
			const float c5[4] = { b + 0.5f, float( ( v + 1 ) % 8 ) + 0.25f, float( ( v + 2 ) % 8 ) + 0.125f, float( ( v + 3 ) % 8 ) + 0.0625f };
			const float c6[4] = { ( v & 1 ) ? float( ( v + 4 ) % 8 ) + 0.0625f : 0.0f, 0, 0, 0 };
			codes[0].insert( codes[0].end(), c5, c5 + 4 );
			codes[1].insert( codes[1].end(), c6, c6 + 4 );
		}
		f.genBuffers( 2, boneBuf );
		for ( int k = 0; k < 2; k++ ) {
			f.bindBuffer( 0x8892, boneBuf[k] );
			f.bufferData( 0x8892, std::ptrdiff_t( codes[k].size() * 4 ), codes[k].data(), 0x88E4 );
			sf.vertexAttribPointer( GLuint( 5 + k ), 4, GL_FLOAT, GL_FALSE, 0, nullptr );
			sf.enableVertexAttribArray( GLuint( 5 + k ) );
			kindPool.vbo[5 + k] = boneBuf[k];
			kindPool.attrMask |= 4ull << ( 4 * ( 5 + k ) );
		}
	}
	// lane VKOBJ5 kind 4: the shape's own VAO (locations 0..4 fp32 interleaved, stride 64 from byte 32; uv half2, stride 4 from 8)
	GLuint ownBuf[2] = {};
	if ( kind == 4 ) {
		void( WINAPI * getSub )( GLuint, std::intptr_t, std::ptrdiff_t, void * ) = nullptr;
		if ( !load( getSub, "glGetNamedBufferSubData" ) ) {
			colPrintf( "BLVIEW colour FAIL (no glGetNamedBufferSubData)\n" );
			return false;
		}
		const std::size_t nv = sx.pool.verts;
		std::vector<float> inter( 8 + nv * 16, 0.0f ), src;
		std::vector<std::uint16_t> uv( 4 + nv * 2, 0 );
		std::size_t at = 0;
		for ( int i = 0; i < 5; i++ ) {
			const int n = int( ( sx.pool.attrMask >> ( 4 * i ) ) & 15 );
			src.assign( nv * std::size_t( n ), 0.0f );
			getSub( sx.pool.vbo[i], 0, std::ptrdiff_t( src.size() * 4 ), src.data() );
			for ( std::size_t v = 0; v < nv; v++ )
				for ( int c = 0; c < n; c++ )
					inter[8 + v * 16 + at + std::size_t( c )] = src[v * std::size_t( n ) + std::size_t( c )];
			at += std::size_t( n );
		}
		src.assign( nv * 2, 0.0f );
		getSub( sx.pool.vbo[7], 0, std::ptrdiff_t( src.size() * 4 ), src.data() );
		for ( std::size_t k = 0; k < nv * 2; k++ )
			uv[4 + k] = toHalf( src[k] );
		if ( at != 16 || ( ( sx.pool.attrMask >> 28 ) & 15 ) != 2 ) {
			colPrintf( "BLVIEW colour FAIL (the fixture is not locations 0..4 = 16 floats and a 2-float uv: %zu floats)\n", at );
			return false;
		}
		f.genBuffers( 2, ownBuf );
		f.bindBuffer( 0x8892, ownBuf[0] );
		f.bufferData( 0x8892, std::ptrdiff_t( inter.size() * 4 ), inter.data(), 0x88E4 );
		for ( int i = 0, off = 32; i < 5; i++ ) {
			const int n = int( ( sx.pool.attrMask >> ( 4 * i ) ) & 15 );
			sf.vertexAttribPointer( GLuint( i ), n, GL_FLOAT, GL_FALSE, 64, reinterpret_cast<const void *>( std::uintptr_t( off ) ) );
			off += n * 4;
		}
		f.bindBuffer( 0x8892, ownBuf[1] );
		f.bufferData( 0x8892, std::ptrdiff_t( uv.size() * 2 ), uv.data(), 0x88E4 );
		sf.vertexAttribPointer( 7, 2, 0x140B, GL_FALSE, 4, reinterpret_cast<const void *>( std::uintptr_t( 8 ) ) );  // HALF_FLOAT
	}
	// lane VKOBJ5 kind 3: the class's commands with base instance 0 (the single draw's row is wwDrawBase)
	GLuint singleInd = 0;
	std::vector<std::uint32_t> single = sx.cmds;
	for ( std::size_t c = 0; c + 4 < single.size(); c += 5 )
		single[c + 4] = 0;
	if ( kind == 3 || kind == 8 ) {
		f.genBuffers( 1, &singleInd );
		f.bindBuffer( 0x8F3F, singleInd );
		f.bufferData( 0x8F3F, std::ptrdiff_t( single.size() * 4 ), single.data(), 0x88E4 );
		f.bindBuffer( 0x8F3F, 0 );
	}
	// lane VKOBJ22 kind 8: each shape's kept ranges; GL's 8-triangle cluster commands and rangeDraws' commands (red 29: rangeDraws red 1)
	GLuint stepInd = 0;
	std::vector<std::uint32_t> stepGl, stepVk[2];
	std::vector<std::size_t> stepGlAt, stepVkAt[2];  // each shape's first command, one entry more than shapes
	bool stepWhole = false;  // GL probe: each shape whole (kind 3's form)
	if ( kind == 8 ) {
		for ( std::size_t c = 0; c < single.size() / 5; c++ ) {
			const std::uint32_t t = single[c * 5] / 3, fi = single[c * 5 + 2], bv = single[c * 5 + 3];
			std::vector<std::uint32_t> rg;
			if ( c % 4 == 1 )
				rg = { 0, t / 3, t / 2, t - t / 2 };
			else if ( c % 4 == 2 )
				rg = { 0, t / 4, t / 4, 0, t / 2, t - t / 2 };
			else if ( c % 4 == 3 )
				rg = { t / 5, t - t / 5 };
			const bool whole = rg.empty();
			const std::vector<std::uint32_t> gr = whole ? std::vector<std::uint32_t>{ 0, t } : rg;
			stepGlAt.push_back( stepGl.size() / 5 );
			for ( std::size_t k = 0; k + 1 < gr.size(); k += 2 )
				for ( std::uint32_t tt = gr[k]; tt < gr[k] + gr[k + 1]; tt += 8 )
					stepGl.insert( stepGl.end(), { std::min( 8u, gr[k] + gr[k + 1] - tt ) * 3u, 1u, fi + tt * 3u, bv, 0u } );
			for ( int red = 0; red < 2; red++ ) {
				std::vector<int> counts;
				std::vector<const void *> offs;
				GpuDrivenBVk::rangeDraws( whole, rg.data(), rg.size(), t, t, counts, offs, red );
				stepVkAt[red].push_back( stepVk[red].size() / 5 );
				for ( std::size_t k = 0; k < counts.size(); k++ )
					stepVk[red].insert( stepVk[red].end(),
						{ std::uint32_t( counts[k] ), 1u, fi + std::uint32_t( std::uintptr_t( offs[k] ) / 2 ), bv, 0u } );
			}
		}
		stepGlAt.push_back( stepGl.size() / 5 );
		for ( int red = 0; red < 2; red++ )
			stepVkAt[red].push_back( stepVk[red].size() / 5 );
		f.genBuffers( 1, &stepInd );
		f.bindBuffer( 0x8F3F, stepInd );
		f.bufferData( 0x8F3F, std::ptrdiff_t( stepGl.size() * 4 ), stepGl.data(), 0x88E4 );
		f.bindBuffer( 0x8F3F, 0 );
	}
	// lane VKOBJ19 kind 6: each single split into the kept piece (fade 1) and three band pieces (0.75, 0.5, 0.25)
	std::vector<std::uint32_t> fadeCmd;
	std::vector<float> fadeMul;
	std::vector<std::size_t> fadeShape;
	std::size_t fadeKept = 0, fadeBands = 0;
	auto fadeA0 = []( std::size_t c ) { return 1.0f - 0.1f * float( c % 3 ); };
	bool fadeProbe = false;  // GL probe: every piece at a0
	bool fadeConst = false;  // check: the band's constant-alpha blend
	GLuint fadeInd = 0;
	if ( kind == 6 ) {
		static const float kFade[3] = { 0.75f, 0.5f, 0.25f };
		for ( std::size_t c = 0; c < single.size() / 5; c++ ) {
			const std::uint32_t n = single[c * 5] / 3, first = single[c * 5 + 2], bv = single[c * 5 + 3];
			const std::uint32_t kept = n < 4 ? n : std::max<std::uint32_t>( 1u, 2u * n / 5u ), rest = n - kept;
			const std::uint32_t piece[4] = { kept, rest / 3, rest / 3, rest - 2 * ( rest / 3 ) };
			std::uint32_t at = 0;
			for ( int k = 0; k < 4; k++ ) {
				if ( !piece[k] )
					continue;
				fadeCmd.insert( fadeCmd.end(), { piece[k] * 3, 1u, first + at * 3, bv, 0u } );
				fadeMul.push_back( k ? kFade[k - 1] : 1.0f );
				fadeShape.push_back( c );
				( k ? fadeBands : fadeKept )++;
				at += piece[k];
			}
		}
		f.genBuffers( 1, &fadeInd );
		f.bindBuffer( 0x8F3F, fadeInd );
		f.bufferData( 0x8F3F, std::ptrdiff_t( fadeCmd.size() * 4 ), fadeCmd.data(), 0x88E4 );
		f.bindBuffer( 0x8F3F, 0 );
	}
	f.bindBuffer( 0x8893, sx.pool.ebo );
	f.bindVertexArray( 0 );
	f.bindBuffer( 0x8892, 0 );
	// GL's frame and fixed state, as the app's opaque pass (nBufs: the 1-buffer arm's draw buffer 0 only)
	int nBufs = 2;
	GLuint frameFb = sx.fbo;  // the 4-sample arm binds the app's frame here
	auto frameState = [&]() {
		f.bindFramebuffer( kFb, frameFb );
		const GLenum dbs[2] = { kColorAtt0, kColorAtt0 + 1 };
		sf.drawBuffers( nBufs, dbs );
		glViewport( 0, 0, kSW, kSH );
		glDisable( GL_BLEND );
		if ( kind == 6 ) {  // lane VKOBJ19: the band blends by its own alpha (check: by a constant alpha)
			glEnable( GL_BLEND );
			glBlendFunc( fadeConst ? 0x8003 : GL_SRC_ALPHA, fadeConst ? 0x8004 : GL_ONE_MINUS_SRC_ALPHA );
		}
		glDisable( GL_SCISSOR_TEST );
		glDisable( GL_STENCIL_TEST );
		glEnable( GL_DEPTH_TEST );
		glDepthFunc( GL_LESS );
		glDepthMask( GL_TRUE );
		glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		glEnable( GL_CULL_FACE );
		glCullFace( GL_BACK );
		glFrontFace( GL_CCW );
		f.useProgram( prog );
		u.bindTextures( 0, 64, nullptr );
		u.bindSamplers( 0, 64, nullptr );
		if ( csmTex )
			u.bindTextures( GLuint( csmUnit ), 1, &csmTex );
		if ( rt ) {
			const GLuint rtTex[5] = { lightTex, restirTex, atlasTex, giHalfTex, giKeyTex };
			for ( int k = 0; k < 5; k++ )
				u.bindTextures( GLuint( rtUnit[k] ), 1, &rtTex[k] );
		}
		if ( aoTex )
			u.bindTextures( GLuint( aoUnit ), 1, &aoTex );
		if ( aoTex && ownUnit >= 0 )
			u.bindTextures( GLuint( ownUnit ), 1, &aoTex );
		if ( ssgiTex )
			u.bindTextures( GLuint( ssgiUnit ), 1, &ssgiTex );
		f.bindBufferBase( 0x8A11, 0, ubo[0] );
		f.bindBufferBase( 0x8A11, 1, ubo[1] );
		if ( sfB ) {  // the cascade values as put() left them (a red or probe set after GL drew reaches both sides)
			f.bindBuffer( 0x8A11, ubo[2] );
			f.bufferData( 0x8A11, std::ptrdiff_t( sfBytes.size() ), sfBytes.data(), 0x88E8 );
			f.bindBuffer( 0x8A11, 0 );
			f.bindBufferBase( 0x8A11, 2, ubo[2] );
		}
		f.bindBufferBase( kSsbo, 12, rowsBuf );
	};
	auto frameOff = [&]() {
		glDisable( GL_CULL_FACE );
		glDisable( GL_DEPTH_TEST );
		f.useProgram( 0 );
		f.bindFramebuffer( kFb, 0 );
	};
	struct ColOut
	{
		std::vector<float> depth;
		std::vector<std::uint16_t> c0, c1;
	};
	ColOut gl;
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR || !putBad.empty() ) {
		colPrintf( "BLVIEW colour FAIL (GL error 0x%x setting the state; first bad value %s)\n", unsigned( e ), putBad.c_str() );
		return false;
	}
	shadeClear();
	frameState();
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		colPrintf( "BLVIEW colour FAIL (GL error 0x%x binding the frame)\n", unsigned( e ) );
		return false;
	}
	GLuint( WINAPI * debugLog )( GLuint, GLsizei, GLenum *, GLenum *, GLuint *, GLenum *, GLsizei *, char * ) = nullptr;
	if ( load( debugLog, "glGetDebugMessageLog" ) ) {
		glEnable( 0x92E0 );  // GL_DEBUG_OUTPUT: the driver's reason for an error lands in the log
		glEnable( 0x8242 );  // _SYNCHRONOUS
	}
	// GL's draw of the class: one multi-draw; lane VKOBJ5 kind 3: one draw a shape, wwDrawBase its row (the first error back)
	auto glDraws = [&]() {
		GLenum e = GL_NO_ERROR;
		f.bindVertexArray( vao );
		if ( kind == 3 ) {
			f.bindBuffer( 0x8F3F, singleInd );
			for ( std::size_t c = 0; c < single.size() / 5; c++ ) {
				put( "wwDrawBase", { float( sx.base + sx.cmds[c * 5 + 4] ) } );
				sf.multiDrawElementsIndirect( GL_TRIANGLES, GL_UNSIGNED_SHORT, reinterpret_cast<const void *>( c * 20 ), 1, 20 );
				const GLenum e1 = glGetError();
				e = e == GL_NO_ERROR ? e1 : e;
			}
			put( "wwDrawBase", { float( sx.base ) } );
		} else if ( kind == 8 ) {  // lane VKOBJ22: a shape's cluster commands (the probe: the shape whole), wwDrawBase its row
			f.bindBuffer( 0x8F3F, stepWhole ? singleInd : stepInd );
			for ( std::size_t c = 0; c < single.size() / 5; c++ ) {
				put( "wwDrawBase", { float( sx.base + sx.cmds[c * 5 + 4] ) } );
				const std::size_t at = stepWhole ? c : stepGlAt[c], n = stepWhole ? 1 : stepGlAt[c + 1] - stepGlAt[c];
				if ( n )
					sf.multiDrawElementsIndirect( GL_TRIANGLES, GL_UNSIGNED_SHORT, reinterpret_cast<const void *>( at * 20 ), GLsizei( n ), 20 );
				const GLenum e1 = glGetError();
				e = e == GL_NO_ERROR ? e1 : e;
			}
			put( "wwDrawBase", { float( sx.base ) } );
		} else if ( kind == 6 ) {  // lane VKOBJ19: a draw a piece, wwDrawBase its shape's row, alpha a0 x the piece's fade
			f.bindBuffer( 0x8F3F, fadeInd );
			for ( std::size_t p = 0; p < fadeMul.size(); p++ ) {
				const std::size_t c = fadeShape[p];
				put( "wwDrawBase", { float( sx.base + sx.cmds[c * 5 + 4] ) } );
				put( "alpha", { fadeA0( c ) * ( fadeProbe ? 1.0f : fadeMul[p] ) } );
				sf.multiDrawElementsIndirect( GL_TRIANGLES, GL_UNSIGNED_SHORT, reinterpret_cast<const void *>( p * 20 ), 1, 20 );
				const GLenum e1 = glGetError();
				e = e == GL_NO_ERROR ? e1 : e;
			}
			put( "wwDrawBase", { float( sx.base ) } );
			put( "alpha", { 1 } );
		} else {
			f.bindBuffer( 0x8F3F, sx.ind );
			sf.multiDrawElementsIndirect( GL_TRIANGLES, GL_UNSIGNED_SHORT, nullptr, GLsizei( sx.cmds.size() / 5 ), 20 );
			e = glGetError();
		}
		f.bindBuffer( 0x8F3F, 0 );
		f.bindVertexArray( 0 );
		return e;
	};
	const GLenum drawErr = glDraws();
	frameOff();
	if ( drawErr != GL_NO_ERROR ) {
		std::string msg;
		char buf[4096] = {};
		GLenum ty = 0;
		for ( int i = 0; debugLog && i < 8 && debugLog( 1, sizeof( buf ), nullptr, &ty, nullptr, nullptr, nullptr, buf ); i++ )
			if ( ty == 0x824C )  // GL_DEBUG_TYPE_ERROR
				msg = buf;
		colPrintf( "BLVIEW colour FAIL (GL error 0x%x in GL's fo4_cellbl draw: %s)\n", unsigned( drawErr ), msg.c_str() );
		return false;
	}
	shadeRead( gl.depth, &gl.c0, &gl.c1 );
	glFinish();
	auto glDraw = [&]( ColOut & o ) {  // GL's draw again (a probe, the RT row's keyed frame); lane VKOBJ20: in the kind's own form
		shadeClear();
		frameState();
		glDraws();
		frameOff();
		shadeRead( o.depth, &o.c0, &o.c1 );
		glFinish();
	};
	if ( rt ) {  // the screen GI's key: GL's surfaces above (view = world), normal +z; the ratio's depth on them; then GL again
		std::vector<float> gk( std::size_t( hw ) * std::size_t( hh ) * 4, -1.0f ), rs( std::size_t( kSW ) * std::size_t( kSH ) * 2 );
		std::size_t keyed = 0, onD = 0, offD = 0, missD = 0;
		for ( int y = 0; y < hh; y++ )
			for ( int x = 0; x < hw; x++ ) {
				const int px = 2 * x, py = 2 * y;
				const float d = gl.depth[std::size_t( py ) * std::size_t( kSW ) + std::size_t( px )];
				if ( !( d < 1.0f ) )
					continue;
				const float wv = sx.proj[14] / ( 2.0f * d - 1.0f + sx.proj[10] );
				const float nx = 2.0f * ( float( px ) + 0.5f ) / float( kSW ) - 1.0f, ny = 2.0f * ( float( py ) + 0.5f ) / float( kSH ) - 1.0f;
				float * p = gk.data() + ( std::size_t( y ) * std::size_t( hw ) + std::size_t( x ) ) * 4;
				p[0] = nx * wv / sx.proj[0];
				p[1] = ny * wv / sx.proj[5];
				p[2] = -wv;
				p[3] = 1024.0f * 2048.0f + 1024.0f;  // cellGiKeyNormal: (0, 0, 1)
				keyed++;
			}
		for ( int y = 0; y < kSH; y++ )
			for ( int x = 0; x < kSW; x++ ) {
				const std::size_t p = std::size_t( y ) * std::size_t( kSW ) + std::size_t( x );
				const float d = gl.depth[p];
				rs[p * 2] = 0.1f + 0.09f * float( ( x * 7 + y * 3 ) % 11 );
				if ( !( d < 1.0f ) ) {
					rs[p * 2 + 1] = 1.0e6f;
					continue;
				}
				const float wv = sx.proj[14] / ( 2.0f * d - 1.0f + sx.proj[10] );
				const int h = ( x * 5 + y * 3 ) % 8;
				rs[p * 2 + 1] = h == 0 ? 1.0e6f : h == 1 ? 1.5f * wv + 10.0f : wv;
				( h == 0 ? missD : h == 1 ? offD : onD )++;
			}
		glBindTexture( GL_TEXTURE_2D, giKeyTex );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, hw, hh, GL_RGBA, GL_FLOAT, gk.data() );
		glBindTexture( GL_TEXTURE_2D, 0 );
		f.bindBuffer( 0x8C2A, restirBuf );
		f.bufferData( 0x8C2A, std::ptrdiff_t( rs.size() * 4 ), rs.data(), 0x88E4 );
		f.bindBuffer( 0x8C2A, 0 );
		put( "cellGiHalfOn", { 1 } );
		glDraw( gl );
		colPrintf( "BLVIEW colour RT inputs: ratio %d x %d (on GL's depth %zu, off %zu, missed %zu), atlas %u B (a store), screen GI %d x %d, %zu key "
			"texels on GL's surfaces\n", kSW, kSH, onD, offD, missD, unsigned( 193 * nP * 16 ), hw, hh, keyed );
	}
	{  // does the sun reach this fixture's colour at all (GL against GL)
		ColOut g2;
		put( "glowMult", { 0 } );
		shadeClear();
		frameState();
		glDraws();
		frameOff();
		shadeRead( g2.depth, &g2.c0, &g2.c1 );
		put( "glowMult", { 1.5f } );
		std::size_t ch = 0;
		for ( std::size_t i = 0; i < gl.c0.size(); i++ )
			ch += gl.c0[i] != g2.c0[i];
		colPrintf( "BLVIEW colour GL glow probe: glowMult 0 changes %zu GL colour values\n", ch );
	}
	bool kindReach = true;
	if ( kind == 1 || kind == 2 ) {  // lane VKOBJ5: does the kind reach GL (skin: doSkinning 0 moves the depth; alpha: alphaFlags 0 covers more)
		ColOut g2;
		std::vector<std::uint8_t> gOff = gBytes;
		std::memset( gOff.data() + skinAt, 0, 4 );
		auto gUp = [&]( const std::vector<std::uint8_t> & bytes ) {
			f.bindBuffer( 0x8A11, ubo[0] );
			f.bufferData( 0x8A11, std::ptrdiff_t( bytes.size() ), bytes.data(), 0x88E4 );
			f.bindBuffer( 0x8A11, 0 );
		};
		if ( kind == 1 )
			gUp( gOff );
		else
			put( "alphaFlags", { 0 } );
		shadeClear();
		frameState();
		glDraws();
		frameOff();
		shadeRead( g2.depth, &g2.c0, &g2.c1 );
		if ( kind == 1 )
			gUp( gBytes );
		else
			put( "alphaFlags", { 4 } );
		std::size_t cov = 0, moved = 0, cov2 = 0;
		for ( std::size_t i = 0; i < gl.depth.size(); i++ ) {
			cov2 += g2.depth[i] < 1.0f;
			if ( !( gl.depth[i] < 1.0f ) )
				continue;
			cov++;
			moved += gl.depth[i] != g2.depth[i];
		}
		if ( kind == 1 ) {
			kindReach = cov > 0 && double( moved ) >= 0.2 * double( cov );
			colPrintf( "BLVIEW colour GL skin probe %s: doSkinning 0 moves GL's depth on %zu of %zu covered pixels (bar >= 20 %%)\n",
				kindReach ? "OK" : "FAIL", moved, cov );
		} else {
			kindReach = cov >= 2000 && double( cov2 ) >= 1.05 * double( cov );
			colPrintf( "BLVIEW colour GL alpha probe %s: the test covers %zu pixels, alphaFlags 0 covers %zu (bar: >= 5 %% more, the test >= 2000)\n",
				kindReach ? "OK" : "FAIL", cov, cov2 );
		}
	}
	bool csmReach = true;
	if ( twin ) {  // does the cascade map reach GL's colour (csmForce 1: no shadow)
		ColOut g2;
		put( "csmForce", { 1.0f } );
		shadeClear();
		frameState();
		glDraws();
		frameOff();
		shadeRead( g2.depth, &g2.c0, &g2.c1 );
		put( "csmForce", { -1.0f } );
		std::size_t cov = 0, moved = 0;
		for ( std::size_t i = 0; i < gl.depth.size(); i++ ) {
			if ( !( gl.depth[i] < 1.0f ) )
				continue;
			cov++;
			bool mv = false;
			for ( int ch = 0; ch < 4; ch++ )
				mv = mv || gl.c0[i * 4 + ch] != g2.c0[i * 4 + ch];
			moved += mv;
		}
		csmReach = cov > 0 && double( moved ) >= 0.2 * double( cov );
		colPrintf( "BLVIEW colour GL map probe %s: csmForce 1 moves GL's colour on %zu of %zu covered pixels (bar >= 20 %%)\n",
			csmReach ? "OK" : "FAIL", moved, cov );
	}
	if ( rt ) {  // the lights and each RT input reach GL's colour (else a red could test nothing)
		static const char * rtOff[4] = { "cellLightCount", "cellRestirOn", "cellDdgiOn", "cellGiHalfOn" };
		for ( int k = 0; k < 4; k++ ) {
			ColOut g2;
			put( rtOff[k], { 0 } );
			glDraw( g2 );
			put( rtOff[k], { k ? 1.0f : 3.0f } );
			std::size_t cov = 0, moved = 0;
			for ( std::size_t i = 0; i < gl.depth.size(); i++ ) {
				if ( !( gl.depth[i] < 1.0f ) )
					continue;
				cov++;
				bool mv = false;
				for ( int ch = 0; ch < 4; ch++ )
					mv = mv || gl.c0[i * 4 + ch] != g2.c0[i * 4 + ch];
				moved += mv;
			}
			const bool reach = cov > 0 && double( moved ) >= 0.2 * double( cov );
			colPrintf( "BLVIEW colour GL RT probe %s: %s 0 moves GL's colour on %zu of %zu covered pixels (bar >= 20 %%)\n", reach ? "OK" : "FAIL",
				rtOff[k], moved, cov );
			csmReach = csmReach && reach;
		}
	}
	if ( kind == 5 ) {  // lane VKWATER3: the obscurance reaches GL's colour (else a stand-in could draw the same picture)
		ColOut g2;
		put( "cellAoOn", { 0 } );
		glDraw( g2 );
		put( "cellAoOn", { 1 } );
		std::size_t cov = 0, moved = 0;
		for ( std::size_t i = 0; i < gl.depth.size(); i++ ) {
			if ( !( gl.depth[i] < 1.0f ) )
				continue;
			cov++;
			bool mv = false;
			for ( int ch = 0; ch < 4; ch++ )
				mv = mv || gl.c0[i * 4 + ch] != g2.c0[i * 4 + ch];
			moved += mv;
		}
		kindReach = cov > 0 && double( moved ) >= 0.2 * double( cov );
		colPrintf( "BLVIEW colour GL ao probe %s: cellAoOn 0 moves GL's colour on %zu of %zu covered pixels (bar >= 20 %%)\n",
			kindReach ? "OK" : "FAIL", moved, cov );
	}
	if ( kind == 6 ) {  // lane VKOBJ19: the band's fade reaches GL's colour (every fade 1: the bands at a0)
		ColOut g2;
		fadeProbe = true;
		shadeClear();
		frameState();
		glDraws();
		frameOff();
		shadeRead( g2.depth, &g2.c0, &g2.c1 );
		fadeProbe = false;
		std::size_t cov = 0, moved = 0;
		for ( std::size_t i = 0; i < gl.depth.size(); i++ ) {
			if ( !( gl.depth[i] < 1.0f ) )
				continue;
			cov++;
			bool mv = false;
			for ( int ch = 0; ch < 4; ch++ )
				mv = mv || gl.c0[i * 4 + ch] != g2.c0[i * 4 + ch];
			moved += mv;
		}
		kindReach = cov > 0 && double( moved ) >= 0.2 * double( cov );
		colPrintf( "BLVIEW colour fade pieces: kept %zu, band %zu (fades 0.75 0.5 0.25), a0 1 / 0.9 / 0.8 by shape\n", fadeKept, fadeBands );
		colPrintf( "BLVIEW colour GL fade probe %s: every fade 1 moves GL's colour on %zu of %zu covered pixels (bar >= 20 %%)\n",
			kindReach ? "OK" : "FAIL", moved, cov );
	}
	if ( kind == 7 ) {  // lane VKOBJ21: the SSGI result reaches GL's colour (else a stand-in could draw the same picture)
		ColOut g2;
		put( "cellSsgiOn", { 0 } );
		glDraw( g2 );
		put( "cellSsgiOn", { 1 } );
		std::size_t cov = 0, moved = 0;
		for ( std::size_t i = 0; i < gl.depth.size(); i++ ) {
			if ( !( gl.depth[i] < 1.0f ) )
				continue;
			cov++;
			bool mv = false;
			for ( int ch = 0; ch < 4; ch++ )
				mv = mv || gl.c0[i * 4 + ch] != g2.c0[i * 4 + ch];
			moved += mv;
		}
		kindReach = cov > 0 && double( moved ) >= 0.2 * double( cov );
		colPrintf( "BLVIEW colour GL ssgi probe %s: cellSsgiOn 0 moves GL's colour on %zu of %zu covered pixels (bar >= 20 %%)\n",
			kindReach ? "OK" : "FAIL", moved, cov );
	}
	if ( kind == 8 ) {  // lane VKOBJ22: the ranges reach GL (each shape whole covers pixels the kept ranges do not)
		ColOut g2;
		stepWhole = true;
		glDraw( g2 );
		stepWhole = false;
		std::size_t covWhole = 0, dropped = 0;
		for ( std::size_t i = 0; i < g2.depth.size(); i++ ) {
			if ( !( g2.depth[i] < 1.0f ) )
				continue;
			covWhole++;
			dropped += !( gl.depth[i] < 1.0f );
		}
		kindReach = dropped > 0 && double( dropped ) >= 0.01 * double( covWhole );
		colPrintf( "BLVIEW colour cluster commands: GL %zu, Vulkan ranges %zu (red %zu), %zu shapes\n", stepGl.size() / 5, stepVk[0].size() / 5,
			stepVk[1].size() / 5, single.size() / 5 );
		colPrintf( "BLVIEW colour GL stepb probe %s: each shape whole covers %zu pixels the kept ranges do not, of %zu (bar > 0 and >= 1 %%)\n",
			kindReach ? "OK" : "FAIL", dropped, covWhole );
	}
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		colPrintf( "BLVIEW colour FAIL (GL error 0x%x reading GL's fo4_cellbl draw)\n", unsigned( e ) );
		return false;
	}
	// the app's crossing, the same state; red: 1 glow, 2 slot, 3 csmmap (the map inverted), 10 strict (objBlPipe's old target
	// count); clearK: both targets
	auto vkRun = [&]( int red, ColOut & out, std::string & w, float clearK = 0.0f ) {
		if ( red == 1 )
			put( "glowMult", { 0 } );
		if ( red == 3 )
			csmFill( true );
		shadeClear( clearK );
		frameState();
		ObjBlShade s;
		s.twin = twin;
		s.prog = prog;
		static int kindKey[5] = {};  // lane VKOBJ5: the skin kind's class (bone codes added) and the own VAO are their own pools
		s.poolKey = kind == 1 || kind == 4 ? reinterpret_cast<std::uintptr_t>( &kindKey[kind] ) : sx.key;
		s.pool = kindPool;
		s.rows = red == 9 ? rowsT.data() : red == 2 ? rowsSlot.data() : rows.data();
		s.nRows = nRows;
		s.cmds = sx.cmds.data();
		s.nCmds = sx.cmds.size() / 5;
		s.red = red == 10 ? 1 : red >= 21 && red <= 24 ? red - 12 : red == 26 ? 8 : red == 28 ? 13 : 0;  // 21 .. 24: the RT reds (objBlShade's 9 .. 12); 26 copy; 28 ssgi
		s.rt = rt;
		s.psGate = plainWords.empty() ? nullptr : plainWords.data();
		s.psGateWords = plainWords.size();
		s.skinRed = red == 11 ? 1 : 0;   // lane VKOBJ5 red bone
		s.alphaRed = red == 12 ? 1 : 0;  // lane VKOBJ5 red flags
		s.ownVao = kind == 4;            // lane VKOBJ5 round 2: the VAO bound below
		s.ownRed = red == 14 ? 1 : red == 15 ? 2 : red == 16 ? 3 : 0;  // reds offset / half; VKFLIGHT11 16 const
		s.standInRed = red == 25;        // lane VKWATER3 red standin
		bool ran = true;
		if ( kind == 3 ) {  // lane VKOBJ5 round 2: a crossing a shape, its one row (red 13 base: rowsAtBase off, padded rows)
			for ( std::size_t c = 0; c < single.size() / 5; c++ ) {
				const std::size_t row = std::size_t( sx.base ) + sx.cmds[c * 5 + 4];
				put( "wwDrawBase", { float( row ) } );
				s.cmds = &single[c * 5];
				s.nCmds = 1;
				s.rows = ( red == 13 ? rowsPad.data() : rows.data() ) + row * kBlRowBytes;
				s.nRows = red == 13 ? 2 * nRows - row : 1;
				s.rowsAtBase = red != 13;
				std::string w1;
				if ( !objBlShade( glProc, s, w1 ) && ran ) {
					ran = false;
					w = w1;
				}
			}
			put( "wwDrawBase", { float( sx.base ) } );
		} else if ( kind == 8 ) {  // lane VKOBJ22: a crossing a shape, rangeDraws' commands, its one row (red 29 droprange: red 1's)
			const int sr = red == 29 ? 1 : 0;
			for ( std::size_t c = 0; c < single.size() / 5; c++ ) {
				const std::size_t row = std::size_t( sx.base ) + sx.cmds[c * 5 + 4];
				put( "wwDrawBase", { float( row ) } );
				s.cmds = stepVk[sr].data() + stepVkAt[sr][c] * 5;
				s.nCmds = stepVkAt[sr][c + 1] - stepVkAt[sr][c];
				s.rows = rows.data() + row * kBlRowBytes;
				s.nRows = 1;
				s.rowsAtBase = true;
				std::string w1;
				if ( !objBlShade( glProc, s, w1 ) && ran ) {
					ran = false;
					w = w1;
				}
			}
			put( "wwDrawBase", { float( sx.base ) } );
		} else if ( kind == 6 ) {  // lane VKOBJ19: a crossing a piece, GL's alpha set first (red 27 fade: at a0, unscaled)
			for ( std::size_t p = 0; p < fadeMul.size(); p++ ) {
				const std::size_t c = fadeShape[p];
				const std::size_t row = std::size_t( sx.base ) + sx.cmds[c * 5 + 4];
				put( "wwDrawBase", { float( row ) } );
				put( "alpha", { fadeA0( c ) * ( red == 27 ? 1.0f : fadeMul[p] ) } );
				s.cmds = &fadeCmd[p * 5];
				s.nCmds = 1;
				s.rows = rows.data() + row * kBlRowBytes;
				s.nRows = 1;
				s.rowsAtBase = true;
				std::string w1;
				if ( !objBlShade( glProc, s, w1 ) && ran ) {
					ran = false;
					w = w1;
				}
			}
			put( "wwDrawBase", { float( sx.base ) } );
			put( "alpha", { 1 } );
		} else {
			if ( kind == 4 )
				f.bindVertexArray( vao );
			ran = objBlShade( glProc, s, w );
			f.bindVertexArray( 0 );
		}
		frameOff();
		if ( red == 1 )
			put( "glowMult", { 1.5f } );
		if ( red == 3 )
			csmFill( false );
		if ( ran )
			shadeRead( out.depth, &out.c0, &out.c1 );
		glFinish();
		const GLenum e = glGetError();
		if ( ran && e != GL_NO_ERROR )
			w = "GL error " + std::to_string( e ) + " at the crossing";
		return ran && e == GL_NO_ERROR;
	};
	struct ColCmp
	{
		std::size_t both = 0, differ = 0, over = 0, lit = 0, glow = 0, first = 0, over1 = 0, over10 = 0;
		float maxRel = 0.0f;
		bool exact() const { return differ == 0; }
		bool bound() const { return over == 0; }
	};
	auto colCmp = [&]( const ColOut & v ) {
		ColCmp c;
		const std::size_t px = gl.depth.size();
		for ( std::size_t i = 0; i < px; i++ ) {
			if ( !( gl.depth[i] < 1.0f ) || !( v.depth[i] < 1.0f ) )
				continue;
			c.both++;
			bool lit = false, glow = false;
			for ( int t = 0; t < 2; t++ )
				for ( int ch = 0; ch < 4; ch++ ) {
					const std::uint16_t hg = ( t ? gl.c1 : gl.c0 )[i * 4 + ch], hv = ( t ? v.c1 : v.c0 )[i * 4 + ch];
					const float g = fromHalf( hg ), a = fromHalf( hv );
					if ( t == 0 && ch < 3 && g != 0.0f )
						lit = true;
					if ( t == 1 && g != 0.0f )
						glow = true;
					if ( hg == hv )
						continue;
					c.differ++;
					const float rel = std::fabs( a - g ) / std::max( 1.0f, std::fabs( g ) );
					if ( !( rel <= 4e-3f ) && c.over == 0 )
							c.first = i * 8 + std::size_t( t * 4 + ch );
						c.over1 += !( rel <= 1e-2f );
						c.over10 += !( rel <= 0.1f );
						if ( !( rel <= 4e-3f ) )
						c.over++;
					if ( rel > c.maxRel || rel != rel )
						c.maxRel = rel != rel ? 1e30f : rel;
				}
			c.lit += lit;
			c.glow += glow;
		}
		return c;
	};
	bool ok = true;
	takeObjBlRunStat();
	ColOut vk0;
	std::string w;
	const bool ran = vkRun( 0, vk0, w );
	const ObjBlRunStat rs = takeObjBlRunStat();
	if ( !ran ) {
		colPrintf( "BLVIEW colour FAIL (objBlShade refused: %s)\n", w.c_str() );
		ok = false;
	} else {
		const DepthCmp dc = depthCmp( vk0.depth, gl.depth );
		const ColCmp cc = colCmp( vk0 );
		const bool litOk = cc.both >= 2000 && double( cc.lit ) >= 0.9 * double( cc.both );
		const bool crossOk = rs.runs == ( kind == 3 || kind == 8 ? int( single.size() / 5 ) : kind == 6 ? int( fadeMul.size() ) : 1 ) && rs.fails == 0;
		colPrintf( "BLVIEW colour state: %d samplers on their own units, %d values with no active location; set 0 units: %d shared, %d GL-only, "
			"%d GL texel buffers; crossing %s (%d runs, %d failed)\n", units, inactive, rs.shared, rs.glOwn, rs.texelGl, crossOk ? "OK" : "FAIL",
			rs.runs, rs.fails );
		colPrintf( "BLVIEW colour depth vs GL %s (%zu covered, %zu on one side, %zu differ, max %.3g)\n", dc.word(), dc.covered, dc.oneSide,
			dc.differ, double( dc.maxAbs ) );
		colPrintf( "BLVIEW colour vs GL %s (%zu pixels both cover, %zu lit, %zu with glow: %s; %zu values differ, %zu over the bound, max %.3g of max(1, |GL|))\n",
			cc.exact() ? "EXACT" : cc.bound() ? "BOUND" : "FAIL", cc.both, cc.lit, cc.glow, litOk ? "lit" : "NOT LIT", cc.differ, cc.over,
			double( cc.maxRel ) );
		if ( cc.over ) {
			const std::size_t p = cc.first / 8, t = ( cc.first % 8 ) / 4;
			const auto & g = t ? gl.c1 : gl.c0;
			const auto & a = t ? vk0.c1 : vk0.c0;
			colPrintf( "BLVIEW colour spread: %zu values over 1e-2, %zu over 0.1 (of max(1, |GL|))\n", cc.over1, cc.over10 );
			colPrintf( "BLVIEW colour first over: pixel (%zu, %zu) target %zu channel %zu; GL %g %g %g %g, Vulkan %g %g %g %g\n", p % kSW, p / kSW, t,
				cc.first % 4, double( fromHalf( g[p * 4] ) ), double( fromHalf( g[p * 4 + 1] ) ), double( fromHalf( g[p * 4 + 2] ) ),
				double( fromHalf( g[p * 4 + 3] ) ), double( fromHalf( a[p * 4] ) ), double( fromHalf( a[p * 4 + 1] ) ), double( fromHalf( a[p * 4 + 2] ) ),
				double( fromHalf( a[p * 4 + 3] ) ) );
		}
		ok = ok && crossOk &&( dc.exact() || dc.bound() ) && litOk && ( cc.exact() || cc.bound() ) && csmReach && kindReach;
		if ( kind == 5 ) {  // lane VKWATER3: cellAo through its copy twin, once a frame
			const std::uint64_t aoBytes = std::uint64_t( kSW / 2 ) * std::uint64_t( kSH / 2 ) * 4u;
			const bool copyOk = rs.copies == 1 && rs.copyBytes == aoBytes;
			colPrintf( "BLVIEW colour check cellAo copy twin %s (%d copies, %llu bytes, want 1 of %llu)\n", copyOk ? "OK" : "FAIL", rs.copies,
				static_cast<unsigned long long>( rs.copyBytes ), static_cast<unsigned long long>( aoBytes ) );
			ok = ok && copyOk;
		}
		if ( kind == 7 ) {  // lane VKOBJ21: cellSsgi bound in place (the owner's shared target), never copied
			const bool inPlace = rs.copies == 0 && rs.shared >= 1;
			colPrintf( "BLVIEW colour check cellSsgi in place %s (%d copies, want 0; %d shared units)\n", inPlace ? "OK" : "FAIL", rs.copies, rs.shared );
			ok = ok && inPlace;
		}
	}
	const char * redName[3] = { "glow (glowMult 0 after GL drew)", "slot (each row's maps one slot on)",
		"map (the cascade map's checker inverted after GL drew)" };
	const char * kindRedName[5] = { " bone (bone i reads bone i + 1's transform)", " flags (the mirrored alphaFlags 0)",  // lane VKOBJ5
		" base (rowsAtBase off: the mirrored wwDrawBase kept)", " offset (every attribute read from offset 0)",
		" half (the halves read as 16-bit unorm)" };
	const int redLo = kind == 0 ? 1 : kind == 4 ? 14 : kind == 5 ? 25 : kind == 6 ? 27 : kind == 7 ? 28 : kind == 8 ? 29 : 10 + kind;
	const int redHi = kind == 0 ? ( twin ? 3 : 2 ) : kind == 4 ? 15 : kind == 5 ? 26 : kind == 6 ? 27 : kind == 7 ? 28 : kind == 8 ? 29 : 10 + kind;
	for ( int r = redLo; r <= redHi; r++ ) {
		ColOut o;
		std::string rw;
		const bool rr = vkRun( r, o, rw );
		bool passes = false;
		std::size_t over = 0, vch = 0;
		if ( rr ) {
			const DepthCmp dc = depthCmp( o.depth, gl.depth );
			const ColCmp cc = colCmp( o );
			over = cc.over;
			for ( std::size_t i = 0; ran && i < o.c0.size() && i < vk0.c0.size(); i++ )
				vch += o.c0[i] != vk0.c0[i];
			for ( std::size_t i = 0; kind && ran && i < o.depth.size() && i < vk0.depth.size(); i++ )
				vch += o.depth[i] != vk0.depth[i];  // lane VKOBJ5: a kind red may move the depth alone
			passes = ( dc.exact() || dc.bound() ) && ( cc.exact() || cc.bound() );
		}
		colPrintf( "BLVIEW RED colour%s %s (%zu values over the bound, %zu Vulkan colour values moved off the Vulkan base%s%s)\n",
			r == 29 ? " droprange (the last kept range of each multi-range shape dropped)" : r == 28 ? " ssgi (cellSsgi dropped to the stand-in)" : r == 27 ? " fade (the band crossings at a0, unscaled)" : r == 25 ? " standin (no copy twin: GL's own cellAo texture stood in)" : r == 26 ? " copy (cellAo copied from one texel on in x)"
			: r > 10 ? kindRedName[r - 11] : redName[r - 1],
			passes ? "PASSED" : vch ? "FAIL" : "TESTS NOTHING", over, vch, rr ? "" : "; refused: ", rr ? "" : rw.c_str() );
		ok = ok && !passes && vch > 0;
	}
	if ( kind == 5 ) {  // lane VKWATER3: a GL-owned texture with no twin (the AO texture on cellSsgi) must refuse the draw to GL
		const GLint l = reg.count( "cellSsgi" ) ? reg["cellSsgi"] : -1;
		ownUnit = unitAt.count( l ) ? unitAt[l] : -1;
		std::string rw;
		ColOut o;
		const bool refused = ownUnit >= 0 && !vkRun( 0, o, rw ) && rw.find( "no twin" ) != std::string::npos
			&& rw.find( "cellSsgi" ) != std::string::npos;
		ownUnit = -1;
		colPrintf( "BLVIEW colour check cellSsgi (GL's own texture, no twin) %s (%s)\n", refused ? "REFUSED OK" : "FAIL",
			rw.empty() ? "no unit for cellSsgi, or it drew" : rw.c_str() );
		ok = ok && refused;
	}
	if ( kind == 6 ) {  // lane VKOBJ19: the band's constant-alpha blend must refuse, naming the constant
		fadeConst = true;
		std::string rw;
		ColOut o;
		const bool refused = !vkRun( 0, o, rw ) && rw.find( "constant" ) != std::string::npos;
		fadeConst = false;
		colPrintf( "BLVIEW colour check fade constant alpha (CONSTANT_ALPHA / ONE_MINUS_CONSTANT_ALPHA): %s (%s)\n", refused ? "REFUSED OK" : "FAIL",
			rw.empty() ? "it drew" : rw.c_str() );
		ok = ok && refused;
	}
	if ( kind == 4 ) {  // lane VKFLIGHT11 (was VKOBJ5's refusal check): a location the VAO leaves off reads GL's current value.
		// The flight's opaque refusal: location 5 (weights0 of a rigid shape) off at (0,0,0,0). The colour (location 1) off at a
		// value of its own moves GL's picture; Vulkan must match that GL draw, and red 16 const ((0,0,0,1) kept) must FAIL and move
		void( WINAPI * attrib4f )( GLuint, GLfloat, GLfloat, GLfloat, GLfloat ) = nullptr;
		void( WINAPI * attribGet )( GLuint, GLenum, GLfloat * ) = nullptr;
		void( WINAPI * arrOff )( GLuint, GLuint ) = nullptr;
		void( WINAPI * arrOn )( GLuint, GLuint ) = nullptr;
		std::string rw, rw3;
		std::size_t gMoved = 0, vMoved = 0, over = 0, over3 = 0, both = 0;
		bool loaded = false, vr = false, vr3 = false, pass = false, pass3 = true;
		if ( load( attrib4f, "glVertexAttrib4f" ) && load( attribGet, "glGetVertexAttribfv" ) && load( arrOff, "glDisableVertexArrayAttrib" )
			&& load( arrOn, "glEnableVertexArrayAttrib" ) ) {
			loaded = true;
			float was1[4] = {}, was5[4] = {};
			attribGet( 1, 0x8626, was1 );  // CURRENT_VERTEX_ATTRIB
			attribGet( 5, 0x8626, was5 );
			arrOff( vao, 1 );
			attrib4f( 1, 0.5f, 0.75f, 0.25f, 0.625f );
			attrib4f( 5, 0.0f, 0.0f, 0.0f, 0.0f );
			ColOut gc;
			glDraw( gc );
			for ( std::size_t i = 0; i < gc.c0.size() && i < gl.c0.size(); i++ )
				gMoved += gc.c0[i] != gl.c0[i];
			const ColOut keep = gl;
			gl = gc;
			ColOut o, o3;
			vr = vkRun( 0, o, rw );
			if ( vr ) {
				const DepthCmp dc = depthCmp( o.depth, gl.depth );
				const ColCmp cc = colCmp( o );
				over = cc.over;
				both = cc.both;
				pass = ( dc.exact() || dc.bound() ) && ( cc.exact() || cc.bound() ) && cc.both >= 2000;
			}
			vr3 = vkRun( 16, o3, rw3 );
			if ( vr3 ) {
				const DepthCmp dc = depthCmp( o3.depth, gl.depth );
				const ColCmp cc = colCmp( o3 );
				over3 = cc.over;
				pass3 = ( dc.exact() || dc.bound() ) && ( cc.exact() || cc.bound() );
				for ( std::size_t i = 0; vr && i < o3.c0.size() && i < o.c0.size(); i++ )
					vMoved += o3.c0[i] != o.c0[i];
			}
			gl = keep;
			arrOn( vao, 1 );
			attrib4f( 1, was1[0], was1[1], was1[2], was1[3] );
			attrib4f( 5, was5[0], was5[1], was5[2], was5[3] );
		}
		const bool constOk = loaded && gMoved > 0 && vr && pass;
		colPrintf( "BLVIEW colour check own const (location 1 off at (0.5,0.75,0.25,0.625), location 5 off at (0,0,0,0)): %s (GL moved %zu "
			"colour values off the main draw; Vulkan %s%s, %zu pixels both cover, %zu values over the bound)\n", constOk ? "OK" : "FAIL", gMoved,
			vr ? "drew" : "refused: ", vr ? "" : rw.c_str(), both, over );
		colPrintf( "BLVIEW RED colour own const (the constant locations read (0,0,0,1)) %s (%zu values over the bound, %zu Vulkan colour values "
			"moved off the const run%s%s)\n", vr3 && !pass3 ? ( vMoved ? "FAIL" : "TESTS NOTHING" ) : vr3 ? "PASSED" : "REFUSED", over3, vMoved,
			vr3 ? "" : "; refused: ", vr3 ? "" : rw3.c_str() );
		ok = ok && constOk && vr3 && !pass3 && vMoved > 0;
	}
	if ( rt ) {  // the RT reds (vkRun 21 .. 24)
		const char * rtRed[4] = { "restir (the ReSTIR ratio copied from two texels on)", "atlas (the GI atlas read one texel on)",
			"gihalf (the screen GI dropped to the stand-in)", "gikey (the screen GI's key dropped to the stand-in)" };
		for ( int r = 21; r <= 24; r++ ) {
			ColOut o;
			std::string rw;
			takeObjBlRunStat();  // a fresh frame: the texel buffers copy again
			const bool rr = vkRun( r, o, rw );
			bool passes = false;
			std::size_t over = 0, vch = 0;
			if ( rr ) {
				const DepthCmp dc = depthCmp( o.depth, gl.depth );
				const ColCmp cc = colCmp( o );
				over = cc.over;
				for ( std::size_t i = 0; ran && i < o.c0.size() && i < vk0.c0.size(); i++ )
					vch += o.c0[i] != vk0.c0[i];
				passes = ( dc.exact() || dc.bound() ) && ( cc.exact() || cc.bound() );
			}
			colPrintf( "BLVIEW RED colour %s %s (%zu values over the bound, %zu Vulkan colour values moved off the Vulkan base%s%s)\n", rtRed[r - 21],
				passes ? "PASSED" : vch ? "FAIL" : "TESTS NOTHING", over, vch, rr ? "" : "; refused: ", rr ? "" : rw.c_str() );
			ok = ok && !passes && vch > 0;
		}
	}
	if ( rt || kind ) {  // a kind (VKOBJ5) or the RT row (VKWATER3) runs the main arm only; the 1-buffer, 4-sample and texture arms are the base rows'
		u.bindTextures( 0, 64, nullptr );
		if ( rt ) {
			glDeleteTextures( 1, &lightTex );
			glDeleteTextures( 1, &restirTex );
			glDeleteTextures( 1, &atlasTex );
			objBlTexelStoreDrop( atlasBuf );
			sharedTargetDrop( giHalfKey );
			sharedTargetDrop( giKeyKey );
		}
		if ( csmTex )
			sharedTargetDrop( csmKey );
		if ( aoTex )
			glDeleteTextures( 1, &aoTex );
		if ( ssgiTex )
			sharedTargetDrop( ssgiKey );
		if ( rt || kind == 5 || kind == 6 || kind == 7 )
			colPrintf( "BLVIEW colour %s\n", ok ? "OK" : "FAILED" );
		gColTwin = gColKind = 0;
		gColTag = "";
		objBlLocsLoad( spirvDir + "/no_locs.txt", why );
		f.useProgram( 0 );
		u.deleteProgram( prog );
		if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
			std::printf( "BLVIEW colour%s%s FAIL (GL error 0x%x left)\n", rt ? " rt" : "", kColKindName[kind], unsigned( e ) );
			ok = false;
		}
		return ok;
	}
	{  // the 1-buffer arm: GL's frame with draw buffer 0 only, both targets cleared to 0.25
		const float k = 0.25f;
		nBufs = 1;
		ColOut g1;
		shadeClear( k );
		frameState();
		f.bindVertexArray( vao );
		f.bindBuffer( 0x8F3F, sx.ind );
		sf.multiDrawElementsIndirect( GL_TRIANGLES, GL_UNSIGNED_SHORT, nullptr, GLsizei( sx.cmds.size() / 5 ), 20 );
		f.bindBuffer( 0x8F3F, 0 );
		f.bindVertexArray( 0 );
		frameOff();
		shadeRead( g1.depth, &g1.c0, &g1.c1 );
		takeObjBlRunStat();
		ColOut v1;
		std::string w1;
		const bool r1 = vkRun( 0, v1, w1, k );
		const ObjBlRunStat rs1 = takeObjBlRunStat();
		std::size_t both = 0, differ = 0, over = 0, vsTwo = 0, t1Gl = 0, t1Vk = 0;
		float maxRel = 0.0f;
		bool armOk = r1 && rs1.runs == 1 && rs1.fails == 0 && rs1.dropped == 1;
		if ( r1 ) {
			const DepthCmp dc = depthCmp( v1.depth, g1.depth );
			const std::uint16_t hk = 0x3400;  // 0.25 as a half
			for ( std::size_t i = 0; i < g1.c1.size(); i++ ) {
				t1Gl += g1.c1[i] != hk;
				t1Vk += v1.c1[i] != hk;
			}
			for ( std::size_t i = 0; i < g1.depth.size(); i++ ) {
				if ( !( g1.depth[i] < 1.0f ) || !( v1.depth[i] < 1.0f ) )
					continue;
				both++;
				for ( int ch = 0; ch < 4; ch++ ) {
					const std::uint16_t hg = g1.c0[i * 4 + ch], hv = v1.c0[i * 4 + ch];
					vsTwo += ran && hv != vk0.c0[i * 4 + ch];
					if ( hg == hv )
						continue;
					differ++;
					const float g = fromHalf( hg ), rel = std::fabs( fromHalf( hv ) - g ) / std::max( 1.0f, std::fabs( g ) );
					over += !( rel <= 4e-3f );
					if ( rel > maxRel || rel != rel )
						maxRel = rel != rel ? 1e30f : rel;
				}
			}
			armOk = armOk && ran && ( dc.exact() || dc.bound() ) && both >= 2000 && over == 0 && vsTwo == 0 && t1Gl == 0 && t1Vk == 0;
			colPrintf( "BLVIEW colour 1buf depth vs GL %s (%zu covered, %zu on one side, %zu differ)\n", dc.word(), dc.covered, dc.oneSide,
				dc.differ );
		}
		colPrintf( "BLVIEW colour 1buf %s (refused %d%s%s; %d runs, %d failed, %d outputs dropped; target 0 vs GL %s: %zu pixels both cover, "
			"%zu values differ, %zu over the bound, max %.3g; vs the 2-buffer Vulkan run %zu values differ; target 1 off 0.25: GL %zu, Vulkan %zu)\n",
			armOk ? "OK" : "FAIL", int( !r1 ), r1 ? "" : ": ", r1 ? "" : w1.c_str(), rs1.runs, rs1.fails, rs1.dropped,
			differ == 0 ? "EXACT" : over == 0 ? "BOUND" : "FAIL", both, differ, over, double( maxRel ), vsTwo, t1Gl, t1Vk );
		ok = ok && armOk;
		// red strict: the old count must refuse this frame
		ColOut vr;
		std::string wr;
		const bool rr = vkRun( 10, vr, wr, k );
		takeObjBlRunStat();
		const bool said = wr.find( "the pixel stage writes 2 targets, the frame has 1" ) != std::string::npos;
		colPrintf( "BLVIEW RED colour1bufstrict %s (the strict target count; %s%s)\n", !rr && said ? "FAIL" : "PASSED",
			rr ? "drew" : "refused: ", rr ? "" : wr.c_str() );
		ok = ok && !rr && said;
		nBufs = 2;
		// under: a frame with more targets than the stage writes still refuses
		WwVkPresent::ObjBlTarget t3;
		t3.colors[0] = t3.colors[1] = t3.colors[2] = 97;  // VK_FORMAT_R16G16B16A16_SFLOAT
		t3.colorCount = 3;
		t3.depth = 129;  // VK_FORMAT_D24_UNORM_S8_UINT
		WwVkPresent::ObjBlPipeInfo i3;
		std::string w3;
		const bool b3 = objBlPipe( twin, vkframe_objbl_vs_spv, sizeof( vkframe_objbl_vs_spv ) / 4, psWords, psCount, t3, {}, {}, i3, w3 );
		const bool under = !b3 && w3.find( "the pixel stage writes 2 targets, the frame has 3" ) != std::string::npos;
		colPrintf( "BLVIEW colour 1buf under %s (a 3-colour frame: %s%s)\n", under ? "OK" : "FAIL", b3 ? "built" : "refused: ", w3.c_str() );
		ok = ok && under;
	}
	/* gap 1 (manager 11-13; stated before the first run, never widened): the app's real frame, objBlFrame at 4 samples,
	 * RGBA16F + D24S8, one draw buffer. Readback: GL's fill pass (blview_fill_*.hlsl, once per sample through the sample
	 * mask, stencil ref sample + 1) and objBlFrameSamples must give every sample EXACT: colour (x/256, y/256, s, 0.5) in
	 * half bits, the D24 value j * 4096 with j = (x & 255) + 256 s, stencil s + 1. Shade: GL's fo4_cellbl into that frame,
	 * read back; objBlShade into the same frame (cleared alike), read back; per sample, depth vs GL as the shade bar
	 * (DepthCmp EXACT or BOUND), colour over the samples both cover as the colour bar (EXACT, or BOUND 4e-3 * max(1, |GL|)),
	 * >= 2000 of them, GL's rgb non-zero on >= 90 %; one run, 0 failed. Reds (each must FAIL): the non-shared frame (GL's
	 * own 4-sample textures: objBlShade and the readback must refuse, "not a shared target"); the dropped sample
	 * (objBlFrameSamples red 1 reads sample 0 for sample 1: must move the readback off the expected values). */
	{
		using MaskI = void( WINAPI * )( GLuint, GLbitfield );
		using ClearFi = void( WINAPI * )( GLenum, GLint, GLfloat, GLint );
		using Store2DMs = void( WINAPI * )( GLuint, GLsizei, GLenum, GLsizei, GLsizei, GLboolean );
		using Create = void( WINAPI * )( GLenum, GLsizei, GLuint * );
		using DelFbo = void( WINAPI * )( GLsizei, const GLuint * );
		MaskI sampleMaski = nullptr;
		ClearFi clearBufferfi = nullptr;
		Store2DMs storageMs = nullptr;
		Create createTextures = nullptr;
		DelFbo delFbo = nullptr, delVao = nullptr;
		bool msOk = load( sampleMaski, "glSampleMaski" ) && load( clearBufferfi, "glClearBufferfi" )
			&& load( storageMs, "glTextureStorage2DMultisample" ) && load( createTextures, "glCreateTextures" )
			&& load( delFbo, "glDeleteFramebuffers" ) && load( delVao, "glDeleteVertexArrays" );
		const int kS = 4;
		const std::size_t nSmp = std::size_t( kSW ) * kSH * kS;
		unsigned mc = 0, md = 0;
		std::string mw;
		if ( msOk && !objBlFrame( kSW, kSH, kS, 0x881A, 0x88F0, mc, md, mw ) ) {  // RGBA16F, DEPTH24_STENCIL8
			colPrintf( "BLVIEW colour ms FAIL (objBlFrame at 4 samples: %s)\n", mw.c_str() );
			msOk = false;
		}
		GLuint msFb = 0, ownFb = 0, ownC = 0, ownD = 0, fillVao = 0, fillProg = 0;
		auto attach = [&]( GLuint & fb, GLuint c, GLuint d ) {
			f.genFramebuffers( 1, &fb );
			f.bindFramebuffer( kFb, fb );
			f.framebufferTexture2D( kFb, kColorAtt0, 0x9100, c, 0 );  // TEXTURE_2D_MULTISAMPLE
			f.framebufferTexture2D( kFb, 0x821A, 0x9100, d, 0 );       // DEPTH_STENCIL_ATTACHMENT
			const GLenum db = kColorAtt0;
			sf.drawBuffers( 1, &db );
			const bool done = f.checkFramebufferStatus( kFb ) == kComplete;
			f.bindFramebuffer( kFb, 0 );
			return done;
		};
		if ( msOk ) {
			createTextures( 0x9100, 1, &ownC );
			createTextures( 0x9100, 1, &ownD );
			storageMs( ownC, kS, 0x881A, kSW, kSH, GL_TRUE );
			storageMs( ownD, kS, 0x88F0, kSW, kSH, GL_TRUE );
			if ( !attach( msFb, mc, md ) || !attach( ownFb, ownC, ownD ) ) {
				colPrintf( "BLVIEW colour ms FAIL (a 4-sample framebuffer is not complete)\n" );
				msOk = false;
			}
		}
		// the fill pass's program from its SPIR-V (build_spv.py in blview.sh)
		if ( msOk ) {
			fillProg = f.createProgram();
			const char * fillSpv[2] = { "/../../blview_fill_vs.spv", "/../../blview_fill_ps.spv" };
			for ( int s = 0; s < 2 && msOk; s++ ) {
				const std::string bin = readFile( spirvDir + fillSpv[s] );
				const GLuint sh = f.createShader( s ? kFrag : kVert );
				GLint okS = 0;
				if ( !bin.empty() ) {
					u.shaderBinary( 1, &sh, 0x9551, bin.data(), GLsizei( bin.size() ) );
					u.specialize( sh, "main", 0, nullptr, nullptr );
					f.getShaderiv( sh, kCompiled, &okS );
				}
				msOk = okS != 0;
				f.attachShader( fillProg, sh );
			}
			GLint linked = 0;
			if ( msOk ) {
				f.linkProgram( fillProg );
				f.getProgramiv( fillProg, kLinked, &linked );
			}
			if ( !linked ) {
				colPrintf( "BLVIEW colour ms FAIL (the fill pass did not specialize or link)\n" );
				msOk = false;
			}
			f.genVertexArrays( 1, &fillVao );
		}
		auto msClear = [&]( GLuint fb ) {
			const GLfloat zero[4] = {};
			f.bindFramebuffer( kFb, fb );
			glDepthMask( GL_TRUE );
			glStencilMask( 0xFF );
			glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
			sf.clearBufferfv( 0x1800, 0, zero );  // GL_COLOR
			clearBufferfi( 0x84F9, 0, 1.0f, 0 );    // GL_DEPTH_STENCIL
			f.bindFramebuffer( kFb, 0 );
		};
		auto toHalf = []( float v ) {  // exact for the fill's values (0 or normal halves)
			std::uint32_t b = 0;
			std::memcpy( &b, &v, 4 );
			if ( !( b & 0x7FFFFFFFu ) )
				return std::uint32_t( ( b >> 16 ) & 0x8000u );
			return std::uint32_t( ( ( b >> 16 ) & 0x8000u ) | ( ( ( ( b >> 23 ) & 0xFFu ) - 112u ) << 10 ) | ( ( b >> 13 ) & 0x3FFu ) );
		};
		auto d24 = []( std::uint32_t bits ) {
			float v = 0.0f;
			std::memcpy( &v, &bits, 4 );
			return std::uint32_t( std::lround( double( v ) * 16777215.0 ) );
		};
		// the readback against the fill's values: samples off (any word)
		auto fillOff = [&]( const std::vector<std::uint32_t> & got ) {
			std::size_t off = 0;
			for ( std::size_t i = 0; i < nSmp && got.size() == nSmp * 4; i++ ) {
				const std::uint32_t s = std::uint32_t( i % kS ), p = std::uint32_t( i / kS ), x = p % kSW, y = p / kSW;
				const std::uint32_t want[4] = { toHalf( float( x ) / 256.0f ) | ( toHalf( float( y ) / 256.0f ) << 16 ),
					toHalf( float( s ) ) | ( toHalf( 0.5f ) << 16 ), ( ( x & 255u ) + 256u * s ) * 4096u, s + 1 };
				off += got[i * 4] != want[0] || got[i * 4 + 1] != want[1] || d24( got[i * 4 + 2] ) != want[2] || got[i * 4 + 3] != want[3];
			}
			return got.size() == nSmp * 4 ? off : nSmp;
		};
		if ( msOk ) {
			msClear( msFb );
			f.bindFramebuffer( kFb, msFb );
			glViewport( 0, 0, kSW, kSH );
			glDisable( GL_CULL_FACE );
			glDisable( GL_BLEND );
			glDisable( GL_SCISSOR_TEST );
			glEnable( GL_DEPTH_TEST );
			glDepthFunc( GL_ALWAYS );
			glEnable( GL_STENCIL_TEST );
			glStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
			glEnable( 0x8E51 );  // GL_SAMPLE_MASK
			f.useProgram( fillProg );
			f.bindVertexArray( fillVao );
			for ( int s = 0; s < kS; s++ ) {
				sampleMaski( 0, 1u << s );
				glStencilFunc( GL_ALWAYS, s + 1, 0xFF );
				glDrawArrays( GL_TRIANGLES, 0, 3 );
			}
			sampleMaski( 0, ~0u );
			glDisable( 0x8E51 );
			glDisable( GL_STENCIL_TEST );
			glDepthFunc( GL_LESS );
			glDisable( GL_DEPTH_TEST );
			f.bindVertexArray( 0 );
			f.useProgram( 0 );
			f.bindFramebuffer( kFb, 0 );
			const GLenum fe = glGetError();
			std::vector<std::uint32_t> rb, rbRed;
			std::string rw, rrw;
			const bool rOk = fe == GL_NO_ERROR && objBlFrameSamples( mc, md, rb, rw );
			const std::size_t off = rOk ? fillOff( rb ) : nSmp;
			const bool rrOk = objBlFrameSamples( mc, md, rbRed, rrw, 1 );
			const std::size_t offRed = rrOk ? fillOff( rbRed ) : 0;
			colPrintf( "BLVIEW colour ms readback %s (%zu samples, %zu off the fill's values%s%s%s)\n", rOk && off == 0 ? "EXACT" : "FAIL", nSmp, off,
				fe != GL_NO_ERROR ? "; GL error in the fill " : "", rOk ? "" : "; refused: ", rOk ? "" : rw.c_str() );
			colPrintf( "BLVIEW RED colourmsdrop %s (the readback reads sample 0 for sample 1: %zu samples off%s%s)\n", rrOk && offRed ? "FAIL" : "PASSED",
				offRed, rrOk ? "" : "; refused: ", rrOk ? "" : rrw.c_str() );
			msOk = rOk && off == 0 && rrOk && offRed > 0;
		}
		// the shade: GL's fo4_cellbl into the frame, then objBlShade into it, every sample read back
		if ( msOk ) {
			nBufs = 1;
			frameFb = msFb;
			std::vector<std::uint32_t> gS, vS;
			std::string gw, vw;
			msClear( msFb );
			frameState();
			f.bindVertexArray( vao );
			f.bindBuffer( 0x8F3F, sx.ind );
			sf.multiDrawElementsIndirect( GL_TRIANGLES, GL_UNSIGNED_SHORT, nullptr, GLsizei( sx.cmds.size() / 5 ), 20 );
			f.bindBuffer( 0x8F3F, 0 );
			f.bindVertexArray( 0 );
			frameOff();
			const bool gOk = glGetError() == GL_NO_ERROR && objBlFrameSamples( mc, md, gS, gw );
			msClear( msFb );
			takeObjBlRunStat();
			frameState();
			ObjBlShade s;
			s.twin = twin;
			s.prog = prog;
			s.poolKey = sx.key;
			s.pool = sx.pool;
			s.rows = rows.data();
			s.nRows = nRows;
			s.cmds = sx.cmds.data();
			s.nCmds = sx.cmds.size() / 5;
			s.psGate = plainWords.empty() ? nullptr : plainWords.data();
			s.psGateWords = plainWords.size();
			const bool vRan = objBlShade( glProc, s, vw );
			frameOff();
			const ObjBlRunStat rs = takeObjBlRunStat();
			const bool vOk = vRan && glGetError() == GL_NO_ERROR && objBlFrameSamples( mc, md, vS, vw );
			if ( !gOk || !vOk ) {
				colPrintf( "BLVIEW colour ms FAIL (%s: %s)\n", !gOk ? "GL's readback" : vRan ? "Vulkan's readback" : "objBlShade refused",
					!gOk ? gw.c_str() : vw.c_str() );
				msOk = false;
			} else {
				std::vector<float> gd( nSmp ), vd( nSmp );
				for ( std::size_t i = 0; i < nSmp; i++ ) {
					gd[i] = float( double( d24( gS[i * 4 + 2] ) ) / 16777215.0 );
					vd[i] = float( double( d24( vS[i * 4 + 2] ) ) / 16777215.0 );
				}
				const DepthCmp dc = depthCmp( vd, gd );
				std::size_t both = 0, lit = 0, differ = 0, over = 0, stencil = 0, edge = 0;
				float maxRel = 0.0f;
				for ( std::size_t i = 0; i < nSmp; i++ ) {
					stencil += gS[i * 4 + 3] != vS[i * 4 + 3];
					if ( !( gd[i] < 1.0f ) || !( vd[i] < 1.0f ) )
						continue;
					both++;
					const std::size_t p0 = ( i / kS ) * kS;  // a pixel with samples on both sides of an edge
					bool mixed = false;
					for ( int k = 0; k < kS; k++ )
						mixed = mixed || !( gd[p0 + std::size_t( k )] < 1.0f );
					edge += mixed;
					bool l = false;
					for ( int ch = 0; ch < 4; ch++ ) {
						const std::uint16_t hg = std::uint16_t( gS[i * 4 + std::size_t( ch / 2 )] >> ( 16 * ( ch & 1 ) ) );
						const std::uint16_t hv = std::uint16_t( vS[i * 4 + std::size_t( ch / 2 )] >> ( 16 * ( ch & 1 ) ) );
						const float g = fromHalf( hg );
						if ( ch < 3 && g != 0.0f )
							l = true;
						if ( hg == hv )
							continue;
						differ++;
						const float rel = std::fabs( fromHalf( hv ) - g ) / std::max( 1.0f, std::fabs( g ) );
						over += !( rel <= 4e-3f );
						if ( rel > maxRel || rel != rel )
							maxRel = rel != rel ? 1e30f : rel;
					}
					lit += l;
				}
				const bool litOk = both >= 2000 && double( lit ) >= 0.9 * double( both );
				const bool crossOk = rs.runs == 1 && rs.fails == 0;
				colPrintf( "BLVIEW colour ms depth vs GL %s (%zu samples covered, %zu on one side, %zu differ, max %.3g)\n", dc.word(), dc.covered,
					dc.oneSide, dc.differ, double( dc.maxAbs ) );
				colPrintf( "BLVIEW colour ms vs GL %s (%zu samples both cover, %zu in edge pixels, %zu lit: %s; %zu values differ, %zu over the bound, "
					"max %.3g; stencil differs on %zu; crossing %d runs, %d failed, %d outputs dropped)\n",
					differ == 0 ? "EXACT" : over == 0 ? "BOUND" : "FAIL", both, edge, lit, litOk ? "lit" : "NOT LIT", differ, over, double( maxRel ),
					stencil, rs.runs, rs.fails, rs.dropped );
				msOk = crossOk && ( dc.exact() || dc.bound() ) && litOk && over == 0;
			}
			// red: GL's own 4-sample frame (not shared) must refuse, the shade and the readback
			msClear( ownFb );
			frameFb = ownFb;
			frameState();
			std::string ow, orw;
			std::vector<std::uint32_t> oS;
			const bool oRan = objBlShade( glProc, s, ow );
			frameOff();
			takeObjBlRunStat();
			const bool oRead = objBlFrameSamples( ownC, ownD, oS, orw );
			const bool oSaid = ow.find( "not a shared target" ) != std::string::npos && orw.find( "not a shared target" ) != std::string::npos;
			colPrintf( "BLVIEW RED colourmsunshared %s (GL's own 4-sample frame: the shade %s%s, the readback %s%s)\n",
				!oRan && !oRead && oSaid ? "FAIL" : "PASSED", oRan ? "drew" : "refused: ", ow.c_str(), oRead ? "read" : "refused: ", orw.c_str() );
			msOk = msOk && !oRan && !oRead && oSaid;
			nBufs = 2;
			frameFb = sx.fbo;
		}
		colPrintf( "BLVIEW colour ms %s\n", msOk ? "OK" : "FAIL" );
		ok = ok && msOk;
		if ( fillProg )
			u.deleteProgram( fillProg );
		if ( fillVao )
			delVao( 1, &fillVao );
		if ( msFb )
			delFbo( 1, &msFb );
		if ( ownFb )
			delFbo( 1, &ownFb );
		if ( ownC )
			glDeleteTextures( 1, &ownC );
		if ( ownD )
			glDeleteTextures( 1, &ownD );
		objBlFrameDrop();
	}
	// where the gap lives: every slot of every row on one texture, GL against Vulkan (a reading, not the bar); the last
	// reading is the fixture's own texture anisoAt at its anisotropy (the known driver difference)
	std::string perTex, fixtureTex;
	for ( std::size_t t = 0; t <= handles.size(); t++ ) {
		const GLuint64 ht = t < handles.size() ? handles[t] : fixture[std::size_t( anisoAt )];
		for ( std::size_t r = 0; r < nRows; r++ )
			for ( std::size_t i = 0; i < 8; i++ )
				std::memcpy( &rowsT[r * kBlRowBytes + kBlTexAt + 8 * i], &ht, 8 );
		f.bindBuffer( kSsbo, rowsBuf );
		f.bufferData( kSsbo, std::ptrdiff_t( rowsT.size() ), rowsT.data(), 0x88E4 );
		f.bindBuffer( kSsbo, 0 );
		const ColOut glKeep = gl;
		shadeClear();
		frameState();
		f.bindVertexArray( vao );
		f.bindBuffer( 0x8F3F, sx.ind );
		sf.multiDrawElementsIndirect( GL_TRIANGLES, GL_UNSIGNED_SHORT, nullptr, GLsizei( sx.cmds.size() / 5 ), 20 );
		f.bindBuffer( 0x8F3F, 0 );
		f.bindVertexArray( 0 );
		frameOff();
		shadeRead( gl.depth, &gl.c0, &gl.c1 );
		ColOut o;
		std::string tw;
		const bool tr = vkRun( 9, o, tw );
		const ColCmp cc = tr ? colCmp( o ) : ColCmp();
		const std::string reading = tr ? std::to_string( cc.over ) + "/" + std::to_string( cc.over10 ) : std::string( "refused" );
		if ( t < handles.size() )
			perTex += " " + std::to_string( t ) + ":" + reading;
		else
			fixtureTex = reading;
		gl = glKeep;
	}
	colPrintf( "BLVIEW colour per texture, %d at anisotropy 1 (over the bound / over 0.1):%s\n", anisoAt, perTex.c_str() );
	colPrintf( "BLVIEW colour known driver difference: texture %d at the fixture's anisotropy, GL's anisotropic filter against Vulkan's "
		"(a reading, not the bar): %s over the bound / over 0.1\n", anisoAt, fixtureTex.c_str() );
	if ( csmTex ) {
		u.bindTextures( 0, 64, nullptr );
		sharedTargetDrop( csmKey );
	}
	colPrintf( "BLVIEW colour blocks: wwShadowFrame %zu B (%d values put there)\n", sfBytes.size(), sfPuts );
	gColTwin = 0;
	gColTag = "";
	objBlLocsLoad( spirvDir + "/no_locs.txt", why );  // the GLSL gates never see it
	f.useProgram( 0 );
	u.deleteProgram( prog );
	if ( const GLenum e = glGetError(); e != GL_NO_ERROR ) {
		colPrintf( "BLVIEW colour FAIL (GL error 0x%x left)\n", unsigned( e ) );
		ok = false;
	}
	return ok;
}
