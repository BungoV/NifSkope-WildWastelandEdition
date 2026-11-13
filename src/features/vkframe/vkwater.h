#pragma once

/* Lane VKWATER1 (2026-11-13, plan row G5): the sorted water pass on the Vulkan view. Behind WW_VKVIEW=1 (the view on
 * Vulkan), red WW_VKVIEW_WATER_RED=1 keeps the route off. Before any draw moves, the pass takes a census of what GL's
 * water program reads (every sampler: its unit, the texture bound there, type, format, levels, anisotropy; its stages;
 * its loose uniforms) and checks each input against what the present device can share (sharedTarget). GL draws the
 * water whenever one input or stage is refused; the reasons are named in the `vk water:` line, slot by slot.
 * The census is GL queries only (Qt's functions or a harness's, Fn), so tests/vkframe/waterview.cpp runs the shipped code. */

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "features/vkframe/vkwater_tables.h"

namespace WwVkPresent
{

//! one sampler of the water program
struct WaterSlot
{
	std::string name;
	unsigned glType = 0;   //!< GL's sampler type (GL_SAMPLER_2D ...)
	int unit = -1;
	unsigned tex = 0;      //!< the name bound on that unit (0: GL's zero texture)
	int kind = -1;         //!< 0 2D, 1 3D, 2 2D array, 3 cube, 4 cube array, 5 buffer, 6 other
	unsigned glFmt = 0;
	int w = 0, h = 0, d = 1, levels = 1;
	float aniso = 1.0f;
	bool samplerObj = false;  //!< a sampler object on its unit (its state, not the texture's, samples)
	// lane VKWATER3: the texture's sampling state (the draw's VkSampler), a texel buffer's data store
	unsigned magF = 0x2601, minF = 0x2601, wrap[3] = { 0x2901, 0x2901, 0x2901 }, cmpMode = 0, cmpFunc = 0x0203;
	int baseLevel = 0;
	unsigned dataBuf = 0;
	long long bufBytes = 0;
};

//! why an input cannot move (0: it takes a twin)
enum WaterBlock { kWaterOk = 0, kWaterBc, kWaterIntFormat, kWaterFormat, kWaterCubeArray, kWaterTexelBuffer, kWaterAniso,
	kWaterSamplerObj, kWaterKind, kWaterBlocks };
inline const char * waterBlockName( int b )
{
	static const char * const k[kWaterBlocks] = { "ok", "BC formats", "integer formats", "other formats", "cube-map arrays",
		"texel buffers", "anisotropic sampling", "sampler objects", "sampler kinds" };
	return b >= 0 && b < kWaterBlocks ? k[b] : "?";
}

//! what the present device takes this run (the gate's reds switch single fields)
struct WaterCaps
{
	bool bc = false, intFormats = false, cubeArray = false, texelBuffer = false, aniso = false, tess = false;
	bool shadedDraw = false;   //!< a shaded mesh draw (the water's vertex stage over the shapes' buffers) on the present device
	bool ( *sharedFmt )( unsigned glFmt ) = nullptr;  //!< sharedTarget's formats (vkFormatOf)
};

struct WaterCensus
{
	unsigned prog = 0;
	std::vector<WaterSlot> inputs;
	bool tess = false;     //!< a tessellation stage (fo4_waterlookd / dp)
	int loose = 0;         //!< active non-sampler uniforms outside blocks
	int samplersSeen = 0;  //!< sampler uniforms the program reports (the gate: every one has a slot)
};

inline int waterKindOf( unsigned t )
{
	switch ( t ) {
	case 0x8B5E: case 0x8B62: case 0x8DCA: case 0x8DD2: return 0;              // sampler2D, 2DShadow, isampler2D, usampler2D
	case 0x8B5F: case 0x8DCB: case 0x8DD3: return 1;                           // sampler3D, i, u
	case 0x8DC1: case 0x8DC4: case 0x8DCF: case 0x8DD7: return 2;              // sampler2DArray, Shadow, i, u
	case 0x8B60: case 0x8DC5: case 0x8DCC: case 0x8DD4: return 3;              // samplerCube, Shadow, i, u
	case 0x900C: case 0x900D: case 0x900E: case 0x900F: return 4;              // samplerCubeMapArray, Shadow, i, u
	case 0x8DC2: case 0x8DD0: case 0x8DD8: return 5;                           // samplerBuffer, i, u
	default: return t >= 0x8B5D && t <= 0x9108 ? 6 : -1;                       // another sampler kind; -1 not a sampler
	}
}

inline bool waterIsSampler( unsigned t )
{
	return waterKindOf( t ) >= 0;
}

inline bool waterBcFormat( unsigned f )
{
	return ( f >= 0x83F0 && f <= 0x83F3 ) || ( f >= 0x8C4C && f <= 0x8C4F ) || ( f >= 0x8DBB && f <= 0x8DBE )
		|| ( f >= 0x8E8C && f <= 0x8E8F );  // S3TC (BC1-3), sRGB S3TC, RGTC (BC4-5), BPTC (BC6-7)
}

inline bool waterIntFormat( unsigned f )
{
	return f >= 0x8231 && f <= 0x823C;  // R8I .. RG32UI
}

inline int waterSlotBlock( const WaterSlot & s, const WaterCaps & c )
{
	if ( s.kind == 5 )
		return c.texelBuffer ? kWaterOk : kWaterTexelBuffer;
	if ( s.kind == 4 )
		return c.cubeArray ? kWaterOk : kWaterCubeArray;
	if ( s.kind < 0 || s.kind > 2 )
		return kWaterKind;
	if ( !s.tex )
		return kWaterOk;  // GL's zero texture: a zero twin
	if ( s.samplerObj )
		return kWaterSamplerObj;
	if ( s.aniso > 1.0f && !c.aniso )
		return kWaterAniso;
	if ( waterBcFormat( s.glFmt ) )
		return c.bc ? kWaterOk : kWaterBc;
	if ( waterIntFormat( s.glFmt ) && !c.intFormats )
		return kWaterIntFormat;
	if ( !waterIntFormat( s.glFmt ) && !( c.sharedFmt && c.sharedFmt( s.glFmt ) ) )
		return kWaterFormat;
	return kWaterOk;
}

inline std::string waterFmtName( unsigned f )
{
	switch ( f ) {
	case 0x8236: return "R32UI";
	case 0x823C: return "RG32UI";
	case 0x8232: return "R8UI";
	case 0x8CAD: return "D32F_S8";
	case 0x8CAC: return "D32F";
	case 0x88F0: return "D24S8";
	case 0x83F0: case 0x83F1: return "BC1";
	case 0x83F2: return "BC2";
	case 0x83F3: return "BC3";
	case 0x8DBB: case 0x8DBC: return "BC4";
	case 0x8DBD: case 0x8DBE: return "BC5";
	default: break;
	}
	char b[16];
	std::snprintf( b, sizeof( b ), "0x%04X", f );
	return b;
}

/* the refusal: empty when every input and stage moves; else "<class> <slot> <slot>...; ..." in WaterBlock order, then the
 * stages. Fills blocks (one per slot) when given. */
inline std::string waterVerdict( const WaterCensus & c, const WaterCaps & caps, std::vector<int> * blocks = nullptr )
{
	std::vector<std::string> per( kWaterBlocks );
	int nBlocked = 0;
	if ( blocks )
		blocks->assign( c.inputs.size(), 0 );
	for ( std::size_t i = 0; i < c.inputs.size(); i++ ) {
		const WaterSlot & s = c.inputs[i];
		const int b = waterSlotBlock( s, caps );
		if ( blocks )
			( *blocks )[i] = b;
		if ( b == kWaterOk )
			continue;
		nBlocked++;
		per[b] += " " + ( s.name.empty() ? "unit" + std::to_string( s.unit ) : s.name );
		if ( b == kWaterIntFormat || b == kWaterFormat )
			per[b] += "(" + waterFmtName( s.glFmt ) + ")";
	}
	std::string out;
	if ( nBlocked )
		out = std::to_string( nBlocked ) + " of " + std::to_string( c.inputs.size() ) + " inputs:";
	for ( int b = 1; b < kWaterBlocks; b++ )
		if ( !per[b].empty() )
			out += std::string( out.back() == ':' ? " " : "; " ) + waterBlockName( b ) + per[b];
	auto add = [&]( const std::string & x ) { out += ( out.empty() ? "" : "; " ) + x; };
	if ( c.tess && !caps.tess )
		add( "the tessellated twin (no tessellationShader on the present device)" );
	if ( !caps.shadedDraw )
		add( "the shaded draw (fo4_default vs over the shapes' GL buffers, " + std::to_string( c.loose ) + " loose uniforms into b31)" );
	return out;
}

/* GL current, prog in use with its units set (wwCellWaterUniforms done): every sampler of prog and what is bound on its
 * unit. GL enums as numbers: this header carries no GL header. Fn: the caller's GL functions (Qt's 4.x core). */
template <class Fn> void waterCensusOf( Fn * fn, unsigned prog, WaterCensus & c )
{
	c = WaterCensus();
	c.prog = prog;
	int n = 0, active = 0x84C0;
	fn->glGetProgramiv( prog, 0x8B86, &n );  // GL_ACTIVE_UNIFORMS
	fn->glGetIntegerv( 0x84E0, &active );    // GL_ACTIVE_TEXTURE
	for ( int i = 0; i < n; i++ ) {
		char nm[256] = {};
		int len = 0, size = 0;
		unsigned type = 0;
		fn->glGetActiveUniform( prog, unsigned( i ), int( sizeof( nm ) ), &len, &size, &type, nm );
		unsigned idx = unsigned( i );
		int block = -1;
		fn->glGetActiveUniformsiv( prog, 1, &idx, 0x8A3A, &block );  // GL_UNIFORM_BLOCK_INDEX
		if ( !waterIsSampler( type ) ) {
			if ( block < 0 )
				c.loose++;
			continue;
		}
		c.samplersSeen++;
		WaterSlot s;
		s.name = nm;
		s.glType = type;
		s.kind = waterKindOf( type );
		const int l = fn->glGetUniformLocation( prog, nm );
		int unit = -1;
		if ( l >= 0 )
			fn->glGetUniformiv( prog, l, &unit );
		s.unit = unit;
		static const unsigned kTarget[6] = { 0x0DE1, 0x806F, 0x8C1A, 0x8513, 0x9009, 0x8C2A };
		static const unsigned kBinding[6] = { 0x8069, 0x806A, 0x8C1D, 0x8514, 0x900A, 0x8C2C };
		if ( unit >= 0 && s.kind >= 0 && s.kind <= 5 ) {
			fn->glActiveTexture( 0x84C0u + unsigned( unit ) );
			int name = 0, sobj = 0;
			fn->glGetIntegerv( kBinding[s.kind], &name );
			fn->glGetIntegerv( 0x8919, &sobj );  // GL_SAMPLER_BINDING
			s.tex = unsigned( name );
			s.samplerObj = sobj != 0;
			if ( name ) {
				const unsigned target = s.kind == 3 ? 0x8515u : kTarget[s.kind];  // a cube's +X face
				int w = 0, h = 0, d = 1, fmt = 0;
				fn->glGetTexLevelParameteriv( target, 0, 0x1003, &fmt );  // GL_TEXTURE_INTERNAL_FORMAT
				fn->glGetTexLevelParameteriv( target, 0, 0x1000, &w );
				fn->glGetTexLevelParameteriv( target, 0, 0x1001, &h );
				if ( s.kind == 1 || s.kind == 2 || s.kind == 4 )
					fn->glGetTexLevelParameteriv( target, 0, 0x8071, &d );  // GL_TEXTURE_DEPTH
				s.glFmt = unsigned( fmt );
				s.w = w;
				s.h = h;
				s.d = d;
				if ( s.kind != 5 ) {
					int minF = 0, maxL = 0;
					float an = 1.0f;
					fn->glGetTexParameteriv( kTarget[s.kind], 0x2801, &minF );  // GL_TEXTURE_MIN_FILTER
					fn->glGetTexParameteriv( kTarget[s.kind], 0x813D, &maxL );  // GL_TEXTURE_MAX_LEVEL
					fn->glGetTexParameterfv( kTarget[s.kind], 0x84FE, &an );    // GL_TEXTURE_MAX_ANISOTROPY
					s.aniso = an;
					int magF = 0, wr[3] = {}, cm = 0, cf = 0, bl = 0;
					fn->glGetTexParameteriv( kTarget[s.kind], 0x2800, &magF );  // GL_TEXTURE_MAG_FILTER
					fn->glGetTexParameteriv( kTarget[s.kind], 0x2802, &wr[0] );  // WRAP_S / T / R
					fn->glGetTexParameteriv( kTarget[s.kind], 0x2803, &wr[1] );
					fn->glGetTexParameteriv( kTarget[s.kind], 0x8072, &wr[2] );
					fn->glGetTexParameteriv( kTarget[s.kind], 0x884C, &cm );     // GL_TEXTURE_COMPARE_MODE / FUNC
					fn->glGetTexParameteriv( kTarget[s.kind], 0x884D, &cf );
					fn->glGetTexParameteriv( kTarget[s.kind], 0x813C, &bl );     // GL_TEXTURE_BASE_LEVEL
					s.magF = unsigned( magF );
					s.minF = unsigned( minF );
					for ( int k = 0; k < 3; k++ )
						s.wrap[k] = unsigned( wr[k] );
					s.cmpMode = unsigned( cm );
					s.cmpFunc = unsigned( cf );
					s.baseLevel = bl;
					const bool mip = minF >= 0x2700 && minF <= 0x2703;
					while ( mip && s.levels <= maxL && s.levels < 16 ) {
						int lw = 0;
						fn->glGetTexLevelParameteriv( target, s.levels, 0x1000, &lw );
						if ( lw < 1 )
							break;
						s.levels++;
					}
				} else {
					int ds = 0;
					fn->glGetTexLevelParameteriv( 0x8C2A, 0, 0x8C2D, &ds );  // GL_TEXTURE_BUFFER_DATA_STORE_BINDING
					int bytes = 0;
					fn->glGetTexLevelParameteriv( 0x8C2A, 0, 0x919E, &bytes );  // GL_TEXTURE_BUFFER_SIZE
					s.dataBuf = unsigned( ds );
					s.bufBytes = bytes;
				}
			}
		}
		c.inputs.push_back( s );
	}
	fn->glActiveTexture( unsigned( active ) );
	unsigned sh[8] = {};
	int ns = 0;
	fn->glGetAttachedShaders( prog, 8, &ns, sh );
	for ( int k = 0; k < ns; k++ ) {
		int t = 0;
		fn->glGetShaderiv( sh[k], 0x8B4F, &t );  // GL_SHADER_TYPE
		if ( t == 0x8E88 || t == 0x8E87 )      // GL_TESS_CONTROL_SHADER / GL_TESS_EVALUATION_SHADER
			c.tess = true;
	}
}

/* Lane VKWATER3: the shaded draw. The vertex inputs water_view_vs reads (SPIR-V location, GL's attribute location in
 * fo4_default.vert = the HLSL TEXCOORD index, components); DXC dropped boneWeights[1] and the far boxes. */
struct WaterVsIn
{
	int spv, gl, comps;
};
inline constexpr WaterVsIn kWaterVsIn[] = { { 0, 0, 3 }, { 1, 1, 4 }, { 2, 2, 3 }, { 3, 3, 3 }, { 4, 4, 3 }, { 5, 5, 4 }, { 7, 7, 2 },
	{ 8, 9, 2 }, { 9, 10, 4 }, { 10, 11, 4 }, { 11, 12, 4 }, { 12, 13, 4 } };
constexpr int kWaterAttribs = int( sizeof( kWaterVsIn ) / sizeof( kWaterVsIn[0] ) );

//! one vertex attribute as GL's vertex array holds it
struct WaterAttrib
{
	bool on = false;
	int size = 4, stride = 0;
	unsigned type = 0x1406, buf = 0;
	bool norm = false, integer = false;
	std::uintptr_t off = 0;
	long long bufBytes = 0;
	int divisor = 0;
	float cur[4] = { 0, 0, 0, 1 };  //!< GL's current value (a disabled array)
};
//! one uniform block's range as GL's indexed binding holds it (buf 0: no block / nothing bound)
struct WaterUbo
{
	unsigned buf = 0;
	long long off = 0, size = 0, bufBytes = 0;
	int need = 0;  //!< the block's size in the program (GL_UNIFORM_BLOCK_DATA_SIZE)
};
/* everything the shaded draw reads: the census, GL's vertex array, element range, blocks, loose uniforms, raster state.
 * Indices GL_UNSIGNED_SHORT triangles (bsshape's drawElements). */
struct WaterRun
{
	int w = 0, h = 0;               //!< the tone frame's size
	unsigned color = 0;             //!< the tone frame's colour (toneFrameIs)
	WaterCensus census;
	WaterAttrib attr[kWaterAttribs];
	unsigned ebo = 0;
	long long eboBytes = 0;
	long long idxOff = 0;           //!< bytes into the element buffer
	int count = 0, baseVertex = 0;
	WaterUbo global, shadow, skin;  //!< globalUniforms (vs b41 + ps b1), wwShadowFrame (ps b0), skinningUniforms (vs b40)
	std::uint8_t vsLoose[kT_waterVsBytes] = {}, psLoose[kT_waterPsBytes] = {};
	int vp[4] = {}, scissor[4] = {};
	bool scissorOn = false, cullOn = false, depthTest = true, depthMask = true, blend = false, stencil = false;
	bool colorMask[4] = { true, true, true, true };
	unsigned cullFace = 0x0405, frontFace = 0x0901, depthFunc = 0x0203;
	bool polyOffset = false;
	std::uintptr_t key = 0;         //!< the twins' keys: key + slot index
	int red = 0;                    //!< gates only: 1 the ps loose block a vec4 off, 2 plain samplers, 4 cull flipped, 8 depth ALWAYS
};

/* GL current, prog in use, its uniforms set, the shape's vertex array bound: GL's side of the draw into r (the census
 * first: waterCensusOf). count / idxOff / baseVertex / w / h / color / key are the caller's. */
template <class Fn> void waterStateOf( Fn * fn, unsigned prog, WaterRun & r )
{
	waterCensusOf( fn, prog, r.census );
	int was = 0;
	fn->glGetIntegerv( 0x8F36, &was );  // GL_COPY_READ_BUFFER binding
	auto bytesOf = [&]( unsigned b ) -> long long {
		if ( !b )
			return 0;
		int n = 0;
		fn->glBindBuffer( 0x8F36, b );
		fn->glGetBufferParameteriv( 0x8F36, 0x8764, &n );  // GL_BUFFER_SIZE
		return n;
	};
	for ( int i = 0; i < kWaterAttribs; i++ ) {
		WaterAttrib & a = r.attr[i];
		a = WaterAttrib();
		const unsigned l = unsigned( kWaterVsIn[i].gl );
		int on = 0, size = 4, stride = 0, type = 0x1406, norm = 0, integer = 0, div = 0, buf = 0;
		fn->glGetVertexAttribiv( l, 0x8622, &on );      // ENABLED
		fn->glGetVertexAttribiv( l, 0x8623, &size );
		fn->glGetVertexAttribiv( l, 0x8624, &stride );
		fn->glGetVertexAttribiv( l, 0x8625, &type );
		fn->glGetVertexAttribiv( l, 0x886A, &norm );
		fn->glGetVertexAttribiv( l, 0x88FD, &integer );
		fn->glGetVertexAttribiv( l, 0x88FE, &div );
		fn->glGetVertexAttribiv( l, 0x889F, &buf );     // BUFFER_BINDING
		void * p = nullptr;
		fn->glGetVertexAttribPointerv( l, 0x8645, &p );
		fn->glGetVertexAttribfv( l, 0x8626, a.cur );    // CURRENT_VERTEX_ATTRIB
		a.on = on != 0;
		a.size = size;
		a.stride = stride;
		a.type = unsigned( type );
		a.norm = norm != 0;
		a.integer = integer != 0;
		a.divisor = div;
		a.buf = unsigned( buf );
		a.off = reinterpret_cast<std::uintptr_t>( p );
		a.bufBytes = a.on ? bytesOf( a.buf ) : 0;
	}
	int ebo = 0;
	fn->glGetIntegerv( 0x8895, &ebo );  // GL_ELEMENT_ARRAY_BUFFER_BINDING (the vertex array's)
	r.ebo = unsigned( ebo );
	r.eboBytes = bytesOf( r.ebo );
	auto ubo = [&]( const char * name, WaterUbo & u ) {
		u = WaterUbo();
		const unsigned bi = fn->glGetUniformBlockIndex( prog, name );
		if ( bi == 0xFFFFFFFFu )
			return;
		int bp = 0, need = 0, b = 0;
		fn->glGetActiveUniformBlockiv( prog, bi, 0x8A3F, &bp );    // GL_UNIFORM_BLOCK_BINDING
		fn->glGetActiveUniformBlockiv( prog, bi, 0x8A40, &need );  // GL_UNIFORM_BLOCK_DATA_SIZE
		u.need = need;
		fn->glGetIntegeri_v( 0x8A28, unsigned( bp ), &b );         // GL_UNIFORM_BUFFER_BINDING
		u.buf = unsigned( b );
		if ( !u.buf )
			return;
		long long st = 0, sz = 0;
		fn->glGetInteger64i_v( 0x8A29, unsigned( bp ), &st );      // START / SIZE (0: the whole buffer)
		fn->glGetInteger64i_v( 0x8A2A, unsigned( bp ), &sz );
		u.bufBytes = bytesOf( u.buf );
		u.off = st;
		u.size = sz > 0 ? sz : u.bufBytes - st;
	};
	ubo( "globalUniforms", r.global );
	ubo( "wwShadowFrame", r.shadow );
	ubo( "skinningUniforms", r.skin );
	fn->glBindBuffer( 0x8F36, unsigned( was ) );
	fxPack( fn, prog, kT_waterVs, int( sizeof( kT_waterVs ) / sizeof( kT_waterVs[0] ) ), r.vsLoose );
	fxPack( fn, prog, kT_waterPs, int( sizeof( kT_waterPs ) / sizeof( kT_waterPs[0] ) ), r.psLoose );
	fn->glGetIntegerv( 0x0BA2, r.vp );       // GL_VIEWPORT
	fn->glGetIntegerv( 0x0C10, r.scissor );  // GL_SCISSOR_BOX
	r.scissorOn = fn->glIsEnabled( 0x0C11 ) != 0;
	r.cullOn = fn->glIsEnabled( 0x0B44 ) != 0;
	r.depthTest = fn->glIsEnabled( 0x0B71 ) != 0;
	r.blend = fn->glIsEnabled( 0x0BE2 ) != 0;
	r.stencil = fn->glIsEnabled( 0x0B90 ) != 0;
	r.polyOffset = fn->glIsEnabled( 0x8037 ) != 0;  // GL_POLYGON_OFFSET_FILL
	int cf = 0, ff = 0, df = 0;
	fn->glGetIntegerv( 0x0B45, &cf );  // GL_CULL_FACE_MODE
	fn->glGetIntegerv( 0x0B46, &ff );  // GL_FRONT_FACE
	fn->glGetIntegerv( 0x0B74, &df );  // GL_DEPTH_FUNC
	r.cullFace = unsigned( cf );
	r.frontFace = unsigned( ff );
	r.depthFunc = unsigned( df );
	unsigned char dm = 1, cm[4] = { 1, 1, 1, 1 };
	fn->glGetBooleanv( 0x0B72, &dm );  // GL_DEPTH_WRITEMASK
	fn->glGetBooleanv( 0x0C23, cm );   // GL_COLOR_WRITEMASK
	r.depthMask = dm != 0;
	for ( int k = 0; k < 4; k++ )
		r.colorMask[k] = cm[k] != 0;
}

//! lane VKWATER1: the device's side of the verdict (vkwater.inc)
WaterCaps waterCaps();
/*! lane VKWATER2: one census input's twin on the present device, GL current: sharedTarget (colour, integer, depth 2D /
 * 3D / array), VKFAR1's farTwin (BC 2D / array, cube-map arrays) or farTexelBuffer (texel buffers). false: why */
bool waterTwin( std::uintptr_t key, const WaterSlot & s, unsigned & tex, std::string & why );
void waterTwinDrop( std::uintptr_t key, const WaterSlot & s );
//! one water draw stayed on GL (why: the verdict or the route's reason)
void waterOnGl( const char * why );
struct WaterStat
{
	int gl = 0, census = 0;
	std::string glWhy;
	double censusMs = 0.0;
};
WaterStat takeWaterStat();
void waterCensusTook( double ms );
/*! lane VKWATER3: the non-tessellated shaded water draw (fo4_default vs + fo4_waterlook ps twins) into the tone frame on
 * Vulkan, one crossing. false: why; nothing drawn (GL draws the shape) */
bool waterRun( const WaterRun & r, std::string & why );
//! what waterRun cannot take on this device (empty: none)
std::string waterDrawRefusal();
FxStat takeWaterDrawStat();
}

#ifndef WW_VKPRESENT_NO_QT
namespace WwVkPresent
{
//! the water's route line ("vk water:"), one a 5 s window while the view is on Vulkan
void waterSay( bool wanted, const char * glWhy );
}
#endif
