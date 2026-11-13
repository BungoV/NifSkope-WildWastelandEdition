#pragma once
/* lane VKFAR1 round 2 (row G3, 2026-11-13): the far cards' (impostor_cellinstez) inputs on the present device
 * (vkfarcards.inc, built into vkpresent.cpp). Nothing draws the cards there yet: these are the pieces their pass needs. */
#include "features/vkframe/vkpresent.h"  // FxField, FxStat, kFxGlobalBytes (lane VKFAR2)
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace WwVkPresent
{
struct FarTwinStat
{
	int made = 0, dropped = 0, fails = 0;
	double mb = 0.0;
	std::string failWhy;
};
/*! a twin sharedTarget does not make: type 0 2D, 2 2D array (layers), 3 cube-map array (layers = 6 x cubes);
 * glFmt a BC name (S3TC / RGTC / BPTC, sRGB too), GL_DEPTH_COMPONENT16, or one of sharedTarget's colour formats.
 * GL draws / copies into tex as into its own texture (GL_TEXTURE_2D, _2D_ARRAY, _CUBE_MAP_ARRAY); the present device
 * samples the same memory. Keyed like sharedTarget (its own map); refused without textureCompressionBC / imageCubeArray. */
bool farTwin( std::uintptr_t key, int type, int w, int h, int layers, unsigned glFmt, unsigned & tex, std::string & why, int levels = 1 );
void farTwinDrop( std::uintptr_t key );
//! one level of one layer as the present device sees it (packed blocks / texels); for the gates
bool farTwinRead( std::uintptr_t key, int level, int layer, std::vector<std::uint8_t> & out, std::string & why );
FarTwinStat farTwinStat();  //!< the twins and the texel buffers together
/*! a texel buffer both APIs see (cellCluster int4, cellLights / cellFarSlots / cellFarRecs float4): glFmt GL_RGBA32F,
 * GL_RGBA32I or GL_RGBA32UI, bytes whole 16-byte texels within the device's maxTexelBufferElements. glBuf is a GL buffer
 * with immutable storage on the present device's memory (write it with glBufferSubData / copies, never glBufferData);
 * glTex its GL_TEXTURE_BUFFER view; the present device holds a VkBufferView of the same bytes. Keyed, own map. */
bool farTexelBuffer( std::uintptr_t key, unsigned glFmt, std::size_t bytes, unsigned & glBuf, unsigned & glTex, std::string & why );
void farTexelBufferDrop( std::uintptr_t key );
bool farTexelBufferRead( std::uintptr_t key, std::vector<std::uint8_t> & out, std::string & why );  //!< for the gates
std::uint32_t farTexelBufferMax();  //!< the device's maxTexelBufferElements (0: no device)

//! one binding of the far cards' set as the SPIR-V declares it (registers shifted: b n -> n, t n -> 64 + n, s n -> 128 + n)
struct FarSetSlot
{
	std::uint32_t binding = 0;
	int kind = 0;  // 0 uniform buffer, 1 sampled image, 2 uniform texel buffer, 3 sampler
	int dim = 0;   // SPIR-V Dim: 0 1D, 1 2D, 2 3D, 3 Cube, 5 Buffer
	bool arrayed = false;
	bool compare = false;  // a sampler a depth-compare sample reads (its VkSampler needs compareEnable)
};
struct FarSetCounts
{
	int ubos = 0, images = 0, texelBuffers = 0, samplers = 0, compareSamplers = 0, cubeArrays = 0;
	std::vector<FarSetSlot> binds;  // by binding (never `slots`: a Qt keyword macro in the app build)
};
//! reflect a fragment module's descriptor set 0 (pure: the gates feed it altered words for their reds)
bool farSetReflect( const std::uint32_t * words, std::size_t count, FarSetCounts & out, std::string & why );
/*! the shipped far-card module (farcards_view_ps, impostor_cellinstez_ps FOGLEAK): reflected, then its set layout, pool,
 * one set, pipeline layout and shader module on the present device (once; farCardsFree releases them) */
bool farCardsSet( FarSetCounts & out, std::string & why );
void farCardsFree( bool glAlive );  //!< shutdown(): everything above goes with the view

/* lane VKFAR2 (row G3): the far cards drawn on the present device (vkfarcardsdraw.inc): impostor_instez_vs (farcards_view_vs,
 * its b31 at 32, cardData / cardFrames at 96 / 97 in the same set) and the pixel stage above, one instanced draw a set
 * into the tone frame and its depth, GL's state (depth LEQUAL with write, no blend, no cull) and GL's clip z. */
// kT_farcardsVs: 6 fields, 100 bytes (spirv-cross --reflect of the shipped SPIR-V; tools: notes/vkair1/tools/reflect_table.py)
inline constexpr FxField kT_farcardsVs[] = {
	{ "cardBase", 0, 1, 1, 1, 0 }, { "frameTableBase", 4, 1, 1, 1, 0 }, { "cardOrtho", 8, 1, 1, 1, 0 },
	{ "cardEyeModel", 16, 3, 1, 0, 0 }, { "modelViewMatrix", 32, 4, 1, 0, 4 }, { "useDepthOffset", 96, 1, 1, 1, 0 },
};
inline constexpr int kT_farcardsVsBytes = 112;
// kT_farcards: 159 fields, 3560 bytes (spirv-cross --reflect of the shipped SPIR-V; tools: notes/vkair1/tools/reflect_table.py)
inline constexpr FxField kT_farcards[] = {
	{ "fogNightHue", 0, 1, 1, 0, 0 }, { "fogNight", 16, 4, 1, 0, 0 }, { "atAp", 32, 4, 1, 0, 0 },
	{ "atApProj", 48, 4, 1, 0, 0 }, { "fogRed", 64, 1, 1, 1, 0 }, { "fogK", 80, 4, 6, 0, 0 },
	{ "volAirTerrU", 176, 4, 1, 0, 0 }, { "volAirTerrV", 192, 4, 1, 0, 0 }, { "volAirTerrFU", 208, 4, 1, 0, 0 },
	{ "volAirTerrFV", 224, 4, 1, 0, 0 }, { "volAirFlags", 240, 1, 1, 1, 0 }, { "volAirGround", 256, 4, 1, 0, 0 },
	{ "volAirTerrRef", 272, 1, 1, 0, 0 }, { "fogDistScale", 276, 1, 1, 0, 0 }, { "volAirMed", 288, 4, 1, 0, 0 },
	{ "volAirLayer", 304, 4, 1, 0, 0 }, { "volAirRain", 320, 4, 1, 0, 0 }, { "volAirRainXf", 336, 4, 1, 0, 4 },
	{ "volAirRainFar", 400, 4, 1, 0, 0 }, { "volPackDims", 416, 4, 1, 0, 0 }, { "fogView", 432, 4, 1, 0, 0 },
	{ "volFar", 448, 4, 1, 0, 0 }, { "volSlices", 464, 4, 1, 0, 0 }, { "volPack", 480, 4, 1, 0, 0 },
	{ "volFarClamp", 496, 1, 1, 1, 0 }, { "volRect", 512, 4, 1, 0, 0 }, { "volAirDrift", 528, 4, 1, 0, 0 },
	{ "volAirDriftUv", 544, 4, 6, 0, 0 }, { "fogSun", 640, 4, 1, 0, 0 }, { "volAirDriftRed", 656, 1, 1, 1, 0 },
	{ "fogOn", 660, 1, 1, 1, 0 }, { "volOn", 664, 1, 1, 1, 0 }, { "atApDouble", 668, 1, 1, 0, 0 },
	{ "volFogBlend", 672, 1, 1, 1, 0 }, { "fogSunColour", 688, 4, 1, 0, 0 }, { "fogUnit", 704, 1, 1, 0, 0 },
	{ "cellClusterDims", 720, 4, 1, 0, 0 }, { "cellClusterOrigin", 736, 2, 1, 0, 0 },
	{ "cellClusterDepth", 752, 4, 1, 0, 0 }, { "cellClusterLog", 768, 2, 1, 0, 0 }, { "cellLightCount", 776, 1, 1, 1, 0 },
	{ "cellClusterOn", 780, 1, 1, 1, 0 }, { "cellClusterBase", 784, 1, 1, 1, 0 }, { "cellIsBloomOn", 788, 1, 1, 1, 0 },
	{ "cellIsBloomRect", 800, 4, 1, 0, 0 }, { "cellIsRed", 816, 1, 1, 1, 0 }, { "cellIsExposure", 820, 1, 1, 0, 0 },
	{ "cellIsWbD", 832, 3, 1, 0, 0 }, { "cellIsVanCurve", 844, 1, 1, 1, 0 }, { "cellIsE", 848, 1, 1, 0, 0 },
	{ "cellIsCine", 852, 3, 1, 0, 0 }, { "cellIsTint", 864, 4, 1, 0, 0 }, { "cellIsAdapted", 880, 1, 1, 0, 0 },
	{ "cellIsLutOn", 884, 1, 1, 1, 0 }, { "cellRow", 896, 4, 3, 0, 0 }, { "cellLightPhys", 944, 4, 1, 0, 0 },
	{ "cellLightPhysB", 960, 4, 1, 0, 0 }, { "cellRed", 976, 1, 1, 1, 0 }, { "cellShadowOn", 980, 1, 1, 1, 0 },
	{ "cellShadowTexel", 984, 1, 1, 0, 0 }, { "cellShadowFadeOn", 988, 1, 1, 1, 0 },
	{ "cellShadowFade", 992, 1, 16, 0, 0 }, { "cellCtOn", 1248, 1, 1, 1, 0 }, { "cellCtRed", 1252, 1, 1, 1, 0 },
	{ "cellCtProj", 1264, 4, 1, 0, 0 }, { "cellCtRect", 1280, 4, 1, 0, 0 }, { "cellGiSky", 1296, 1, 1, 1, 0 },
	{ "cellInterior", 1300, 1, 1, 1, 0 }, { "cellGoboOn", 1304, 1, 1, 1, 0 }, { "cellGoboTbl", 1308, 1, 1, 1, 0 },
	{ "cellGoboMap", 1312, 1, 1, 1, 0 }, { "cellGoboFlat", 1316, 1, 1, 1, 0 }, { "cellGoboGam", 1320, 1, 1, 1, 0 },
	{ "cellGoboRot", 1324, 1, 1, 1, 0 }, { "cellFarOn", 1328, 1, 1, 1, 0 }, { "cellFarRed", 1332, 1, 1, 1, 0 },
	{ "cellFarBand", 1336, 2, 1, 0, 0 }, { "cellFarCell", 1344, 1, 1, 0, 0 }, { "cellFarBits", 1348, 1, 1, 1, 0 },
	{ "cellFarProbe", 1352, 1, 1, 1, 0 }, { "cellFarPlacedOn", 1356, 1, 1, 1, 0 },
	{ "cellFarPlacedVoxel", 1360, 1, 1, 0, 0 }, { "cellFarPlacedOrigin", 1364, 3, 1, 0, 0 },
	{ "cellFarPlacedDims", 1376, 3, 1, 0, 0 }, { "cellAmboCount", 1388, 1, 1, 1, 0 }, { "cellAmboK", 1392, 4, 16, 0, 0 },
	{ "cellAmboBox", 1648, 4, 48, 0, 0 }, { "cellAmbo", 2416, 4, 16, 0, 0 }, { "cellDalc", 2672, 4, 3, 0, 0 },
	{ "cellGiAmb", 2720, 1, 1, 1, 0 }, { "cellSsgiOn", 2724, 1, 1, 1, 0 }, { "cellSsgiRect", 2736, 4, 1, 0, 0 },
	{ "cellSsgiRed", 2752, 1, 1, 1, 0 }, { "cellSsgiK", 2756, 2, 1, 0, 0 }, { "cellHasDir", 2764, 1, 1, 1, 0 },
	{ "cellDirTo", 2768, 3, 1, 0, 0 }, { "cellDirColor", 2784, 3, 1, 0, 0 }, { "cellHasDalc", 2796, 1, 1, 1, 0 },
	{ "cardCovBase", 2800, 1, 1, 0, 0 }, { "cardCovFloor", 2804, 1, 1, 0, 0 }, { "ussCloudOn", 2808, 1, 1, 1, 0 },
	{ "ussCloudFromView", 2816, 4, 1, 0, 4 }, { "ussCloudFar", 2880, 4, 1, 0, 0 }, { "cardZcull", 2896, 1, 1, 1, 0 },
	{ "modelViewMatrix", 2912, 4, 1, 0, 4 }, { "cardZcullRed", 2976, 1, 1, 1, 0 }, { "useHeightBlend", 2980, 1, 1, 1, 0 },
	{ "depthSearchSteps", 2984, 1, 1, 1, 0 }, { "alphaThreshold", 2988, 1, 1, 0, 0 },
	{ "swayAmplitude", 2992, 1, 1, 0, 0 }, { "cutRule", 2996, 1, 1, 1, 0 }, { "hasMaskSheet", 3000, 1, 1, 1, 0 },
	{ "hasEmissiveSheet", 3004, 1, 1, 1, 0 }, { "cardFadeOut", 3008, 1, 1, 0, 0 }, { "cardFadeFrame", 3012, 1, 1, 0, 0 },
	{ "useDepthOffset", 3016, 1, 1, 1, 0 }, { "debugChannel", 3020, 1, 1, 1, 0 }, { "emissiveScale", 3024, 1, 1, 0, 0 },
	{ "cellOn", 3028, 1, 1, 1, 0 }, { "cellSunScale", 3032, 1, 1, 0, 0 }, { "useBakedAo", 3036, 1, 1, 1, 0 },
	{ "cardLightRed", 3040, 1, 1, 1, 0 }, { "cellExtDalc", 3044, 1, 1, 1, 0 }, { "cardSunRed", 3048, 1, 1, 1, 0 },
	{ "cellIsOn", 3052, 1, 1, 1, 0 }, { "cellIsLinear", 3056, 1, 1, 1, 0 }, { "cardLodTint", 3060, 3, 1, 0, 0 },
	{ "cardLodStripe", 3072, 1, 1, 0, 0 }, { "fogProbe", 3088, 4, 1, 0, 0 }, { "volProbe", 3104, 1, 1, 1, 0 },
	{ "cellGiFill", 3108, 1, 1, 1, 0 }, { "cellCenter", 3120, 3, 1, 0, 0 }, { "cellProbe", 3132, 1, 1, 1, 0 },
	{ "cellProbeGain", 3136, 1, 1, 0, 0 }, { "cellGiOn", 3140, 1, 1, 1, 0 }, { "cellGiOrigin", 3152, 3, 1, 0, 0 },
	{ "cellGiVoxel", 3164, 1, 1, 0, 0 }, { "cellGiDims", 3168, 3, 1, 0, 0 }, { "cellGiPad", 3180, 1, 1, 0, 0 },
	{ "cellGiRooms", 3184, 1, 1, 1, 0 }, { "cellRoomsOrigin", 3188, 3, 1, 0, 0 }, { "cellRoomsCell", 3200, 1, 1, 0, 0 },
	{ "cellRoomsDims", 3204, 3, 1, 0, 0 }, { "cellGiFineOn", 3216, 1, 1, 1, 0 }, { "cellGiFineN", 3220, 1, 1, 1, 0 },
	{ "cellGiFineRed", 3224, 1, 1, 1, 0 }, { "cellGiFineBox", 3232, 4, 8, 0, 0 }, { "cellGiFineDim", 3360, 4, 8, 0, 0 },
	{ "cellPass", 3488, 1, 1, 1, 0 }, { "cellPassRed", 3492, 1, 1, 1, 0 }, { "cellGiLoop8", 3496, 1, 1, 1, 0 },
	{ "cellAoDecalOn", 3500, 1, 1, 1, 0 }, { "cellAoDecalRect", 3504, 4, 1, 0, 0 }, { "cellSkyVis", 3520, 1, 1, 0, 0 },
	{ "cellSpecOcc", 3524, 1, 1, 1, 0 }, { "cellSpecSkyBase", 3528, 1, 1, 0, 0 }, { "cellCubeMat", 3536, 4, 1, 0, 0 },
	{ "familyPbr", 3552, 1, 1, 1, 0 }, { "cardMipCap", 3556, 1, 1, 0, 0 },
};
inline constexpr int kT_farcardsBytes = 3568;
//! one texture / texture buffer the cards read, as GL holds it now (fcTexOf); the sampler is built from GL's parameters
struct FarCardTex
{
	unsigned tex = 0;  //!< GL's name (0: the zero texture / buffer of the slot's type, as GL's unbound unit reads)
	int type = 0;      //!< 0 2D, 1 3D, 2 2D array, 3 cube-map array, 5 texture buffer
	int w = 0, h = 0, d = 1, levels = 1;
	unsigned glFmt = 0;
	int minF = 0x2601, magF = 0x2601, wrap[3] = { 0x2901, 0x2901, 0x2901 }, cmpMode = 0, cmpFunc = 0x0203;
	int minLod = -1000, maxLod = 1000, bias = 0, aniso = 1;
	unsigned buf = 0;  //!< a texture buffer's GL buffer, the range it views
	long long bufOff = 0, bufBytes = 0;
};
struct FarCardsSet
{
	int count = 0;                                     //!< instances (the VS's cardBase in vsLoose places them)
	FarCardTex sheet[4];                               //!< t16 ColourSheet, t19 NormalSheet, t20 MaskSheet, t21 EmissiveSheet
	std::uint8_t vsLoose[kT_farcardsVsBytes] = {};     //!< the vertex stage's b31
	std::uint8_t loose[kT_farcardsBytes] = {};         //!< the pixel stage's b31 as GL holds it for this set
};
struct FarCardsStencil  //!< GL's stencil state at the draw (the cards do not set it)
{
	bool on = false;
	int func = 0x0207, ref = 0, mask = 0xFF, write = 0xFF, sfail = 0x1E00, zfail = 0x1E00, zpass = 0x1E00;
};
struct FarCardsRun
{
	int w = 0, h = 0, vp[4] = {};      //!< the tone frame's size, GL's viewport
	unsigned color = 0;                //!< the tone frame's colour (toneFrameIs)
	FarCardTex tex[26];                //!< the pixel stage's t0..t25 (the sheets' four come from each set)
	FarCardTex cardData, cardFrames;   //!< the vertex stage's texel buffers
	FarCardsStencil stencil;
	std::uint8_t global[kFxGlobalBytes] = {};
	std::vector<FarCardsSet> sets;
	int red = 0;  //!< gates only: 1 no depth write, 2 the vertex stage's cardBase one card on, 4 the shadow compare inverted
};
/* GL current, the tone frame bound, impostor_cellinstez's uniforms set: the sets drawn on Vulkan, one crossing.
 * false: why; nothing drawn (GL draws them) */
bool farCardsRun( const FarCardsRun & t, std::string & why );
void farCardsOnGl( const char * why );
FxStat takeFarCardsStat();

//! one field of a b31 table written by name (bytes from v); false: not in the table
inline bool fcPut( std::uint8_t * out, const FxField * f, int n, const char * name, const void * v, int bytes )
{
	for ( int i = 0; i < n; i++ )
		if ( std::strcmp( f[i].name, name ) == 0 ) {
			std::memcpy( out + f[i].offset, v, std::size_t( bytes ) );
			return true;
		}
	return false;
}

/* a slot from what the program samples now: the sampler's unit, the name bound there for type (0 2D, 1 3D, 2 2D array,
 * 3 cube-map array, 5 texture buffer), its size, format, levels and every sampling parameter GL holds on the texture.
 * false: why (a sampler object on the unit). GL enums as numbers: this header carries no GL header */
template <class Fn> bool fcTexOf( Fn * fn, unsigned prog, const char * sampler, int type, FarCardTex & t, std::string & why )
{
	t = FarCardTex();
	t.type = type;
	const int l = fn->glGetUniformLocation( prog, sampler );
	if ( l < 0 )
		return true;  // not in the program: the zero texture
	static const unsigned kTarget[6] = { 0x0DE1u, 0x806Fu, 0x8C1Au, 0x9009u, 0u, 0x8C2Au };
	static const unsigned kBinding[6] = { 0x8069u, 0x806Au, 0x8C1Du, 0x900Au, 0u, 0x8C2Cu };
	if ( type < 0 || type > 5 || type == 4 ) {
		why = std::string( sampler ) + ": no such slot type";
		return false;
	}
	int unit = 0, active = 0x84C0;
	fn->glGetUniformiv( prog, l, &unit );
	fn->glGetIntegerv( 0x84E0, &active );  // GL_ACTIVE_TEXTURE
	fn->glActiveTexture( unsigned( 0x84C0 + unit ) );
	const unsigned target = kTarget[type];
	int name = 0, sobj = 0;
	fn->glGetIntegerv( kBinding[type], &name );
	fn->glGetIntegerv( 0x8919, &sobj );  // GL_SAMPLER_BINDING
	bool ok = true;
	if ( name && type == 5 ) {
		int buf = 0, off = 0, size = 0, fmt = 0;
		fn->glGetTexLevelParameteriv( target, 0, 0x8C2D, &buf );   // GL_TEXTURE_BUFFER_DATA_STORE_BINDING
		fn->glGetTexLevelParameteriv( target, 0, 0x919D, &off );   // GL_TEXTURE_BUFFER_OFFSET
		fn->glGetTexLevelParameteriv( target, 0, 0x919E, &size );  // GL_TEXTURE_BUFFER_SIZE
		fn->glGetTexLevelParameteriv( target, 0, 0x1003, &fmt );   // GL_TEXTURE_INTERNAL_FORMAT
		t.tex = unsigned( name );
		t.buf = unsigned( buf );
		t.bufOff = off;
		t.bufBytes = size;
		t.glFmt = unsigned( fmt );
	} else if ( name ) {
		int w = 0, h = 0, d = 1, fmt = 0, maxL = 0, base = 0;
		fn->glGetTexLevelParameteriv( target, 0, 0x1000, &w );  // GL_TEXTURE_WIDTH
		fn->glGetTexLevelParameteriv( target, 0, 0x1001, &h );
		if ( type )
			fn->glGetTexLevelParameteriv( target, 0, 0x8071, &d );  // GL_TEXTURE_DEPTH (a cube array's: 6 x cubes)
		fn->glGetTexLevelParameteriv( target, 0, 0x1003, &fmt );
		fn->glGetTexParameteriv( target, 0x2801, &t.minF );
		fn->glGetTexParameteriv( target, 0x2800, &t.magF );
		fn->glGetTexParameteriv( target, 0x2802, &t.wrap[0] );
		fn->glGetTexParameteriv( target, 0x2803, &t.wrap[1] );
		fn->glGetTexParameteriv( target, 0x8072, &t.wrap[2] );
		fn->glGetTexParameteriv( target, 0x884C, &t.cmpMode );  // GL_TEXTURE_COMPARE_MODE
		fn->glGetTexParameteriv( target, 0x884D, &t.cmpFunc );
		fn->glGetTexParameteriv( target, 0x813A, &t.minLod );
		fn->glGetTexParameteriv( target, 0x813B, &t.maxLod );
		fn->glGetTexParameteriv( target, 0x813C, &base );       // GL_TEXTURE_BASE_LEVEL
		fn->glGetTexParameteriv( target, 0x813D, &maxL );
		fn->glGetTexParameteriv( target, 0x8501, &t.bias );     // GL_TEXTURE_LOD_BIAS
		fn->glGetTexParameteriv( target, 0x84FE, &t.aniso );    // GL_TEXTURE_MAX_ANISOTROPY
		const bool mip = t.minF >= 0x2700 && t.minF <= 0x2703;
		int levels = 1;
		while ( mip && levels <= maxL && levels < 16 ) {
			int lw = 0;
			fn->glGetTexLevelParameteriv( target, levels, 0x1000, &lw );
			if ( lw < 1 )
				break;
			levels++;
		}
		t.tex = unsigned( name );
		t.w = w;
		t.h = h;
		t.d = type ? d : 1;
		t.glFmt = unsigned( fmt );
		t.levels = levels;
		if ( base != 0 ) {
			ok = false;
			why = std::string( sampler ) + ": a base level other than 0";
		} else if ( w < 1 || h < 1 ) {
			ok = false;
			why = std::string( sampler ) + ": no level 0";
		}
	}
	if ( name && sobj ) {
		ok = false;
		why = std::string( sampler ) + ": a sampler object on its unit";
	}
	fn->glActiveTexture( unsigned( active ) );
	return ok;
}
}  // namespace WwVkPresent
