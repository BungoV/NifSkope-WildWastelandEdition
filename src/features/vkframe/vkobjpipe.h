#pragma once

/* Lane VKOBJ4 (2026-11-13, plan row G4 slice 4): the shaded cell shapes' pipelines on the present device, one per bl twin
 * (fo4_defaultbl_vs + fo4_cellbl_ps / fo4_cellcsmbl_ps, -D WW_VK_BINDLESS). Both stages' resources are read from the
 * modules (spvReflect): set 0 = the twin's own bindings (the pixel stage on fx()'s shifts b 0 / t 64 / s 128 / u 192, the
 * vertex stage on its own, b 256 / t 320 / s 384 / u 448: its b0 skinningUniforms would land on the pixel stage's b0
 * globalUniforms), set 1 = the bindless table (vkbindless.inc) and nothing else. Refused with a reason: a binding two stages
 * declare differently, a set 1 that is not the table, a varying the vertex stage does not write, a target count the pixel
 * stage does not write, a vertex input no attribute feeds. Attributes are given by GL's attribute location (the HLSL twin's
 * TEXCOORDn); DXC numbers the inputs on its own (fo4_defaultbl_vs: TEXCOORD9 is Location 8), so the semantic maps them.
 * Slices 5-7 below: GL's uniforms mirrored, the class pools as vertex buffers, the multi-draw, the crossing (objBlRun),
 * GL's texture units (objBlBindUnits: a shared target such as the CSM or the air volume as a set-0 image, GL's sampler
 * state with the depth compare). GL's texel buffers (the lights) stay the zero texel.
 * Gate: tests/vkframe/blview.sh (the `BLVIEW pipe` / `uni` / `pool` / `shade` / `units` lines; their reds must FAIL). */

#include "features/vkframe/vkpresent.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace WwVkPresent
{
/*! one member of a uniform block as the module lays it out (a cbuffer bool is a uint). A matrix is `cols` vectors of `comps`
 * (DXC: an HLSL row_major matrix's rows, which are the GLSL matrix's columns), each `colStride` apart (rowMajor: the other way) */
struct SpvMember
{
	std::string name;
	std::uint32_t offset = 0, base = 0, comps = 1, cols = 1, colStride = 0, count = 1, arrayStride = 0;  //!< base: 0 float, 1 int, 2 uint, 3 other
	bool rowMajor = false;
};
//! one descriptor binding a module declares (count 0 = a runtime array)
struct SpvBind
{
	std::uint32_t set = 0, binding = 0, type = 0, count = 1;  //!< type: a VkDescriptorType
	std::uint32_t stages = 0;                                 //!< VkShaderStageFlags
	std::string name;                                         //!< the block's name for a buffer, else the variable's
	std::uint32_t dim = 0, arrayed = 0;                       //!< an image's SPIR-V Dim (0 1D, 1 2D, 2 3D, 3 cube, 5 buffer)
	std::uint32_t sampled = 0;                                //!< an image's element: 0 float, 1 int, 2 uint
	std::vector<SpvMember> members;                           //!< a uniform block's members
	std::uint32_t bytes = 0;                                  //!< a uniform block's size (the last member's end, to 16)
};
//! one stage input / output with a Location (a matrix or array takes `locs` locations; not `slots`: Qt's macro)
struct SpvIo
{
	std::uint32_t location = 0, locs = 1, comps = 1, base = 0;  //!< base: 0 float, 1 int, 2 uint
	std::string name;
};
struct SpvModule
{
	std::uint32_t stage = 0;  //!< the entry point's VkShaderStageFlagBits
	std::vector<SpvBind> binds;
	std::vector<SpvIo> inputs, outputs;
};
//! a module's descriptors and interface, read from the words (false with a reason: not SPIR-V, a type it does not take)
bool spvReflect( const std::uint32_t * words, std::size_t n, SpvModule & out, std::string & why );

//! an attribute GL feeds: its location (the twin's TEXCOORDn), the Vulkan buffer binding, format and offset
struct ObjBlAttr
{
	std::uint32_t glLocation = 0, binding = 0, format = 0, offset = 0;  //!< format: a VkFormat
};
struct ObjBlBuffer
{
	std::uint32_t binding = 0, stride = 0;
	bool perInstance = false;
};
//! GL's blend and colour mask as one state for every colour target (VkBlendFactor / VkBlendOp; writeMask: VkColorComponentFlags)
struct ObjBlBlend
{
	std::uint32_t on = 0, srcColor = 1, dstColor = 0, colorOp = 0, srcAlpha = 1, dstAlpha = 0, alphaOp = 0, writeMask = 0xF;
};
//! GL's stencil test as the pipeline's: [0] front, [1] back (func a VkCompareOp, the ops VkStencilOp; reference and masks GL's)
struct ObjBlStencil
{
	std::uint32_t on = 0, func[2] = { 7, 7 }, fail[2] = {}, depthFail[2] = {}, pass[2] = {}, ref[2] = {}, readMask[2] = { 0xFF, 0xFF },
		writeMask[2] = { 0xFF, 0xFF };
};
//! the frame the pipeline draws into, as GL's cell pass draws (colour formats as VkFormat, GL's depth, stencil and blend state)
struct ObjBlTarget
{
	std::uint32_t colors[4] = {}, colorCount = 0, depth = 0, samples = 1;
	std::uint32_t depthOp = 3;  //!< a VkCompareOp (3 = LESS_OR_EQUAL)
	bool depthWrite = false, twoSided = false;
	ObjBlBlend blend;
	ObjBlStencil stencil;  //!< lane VKOBJ4 open item 3 (off: the defaults)
};
/*! GL's fixed state as the pipeline takes it, for `colorCount` draw buffers: depth test / function / mask, culling, stencil
 * (front and back: function, reference, masks, ops), blend (per draw buffer: they must agree; a constant factor is refused)
 * and the colour mask. Refused with a reason: scissor on, depth test off, culling other than back faces with CCW front.
 * red 1 reads source and destination swapped, red 2 reads the stencil test as off (gates only: each must FAIL) */
bool objBlTargetState( GlProc gp, std::uint32_t colorCount, ObjBlTarget & t, std::string & why, int red = 0 );
struct ObjBlPipeInfo
{
	int set0 = 0, ubos = 0, images = 0, texels = 0, samplers = 0, ssbos = 0, vsOnly = 0, psOnly = 0, both = 0;
	int inputs = 0, varyings = 0, targets = 0;
	double ms = 0.0;
};
/*! the pipeline of twin `twin` (0 fo4_cellbl, 1 fo4_cellcsmbl) from these modules for this frame and these attributes: the
 * set-0 layout, the pipeline layout {set 0, the table} and the graphics pipeline, made once per twin and target. false with
 * a reason (nothing made) when the modules disagree with each other, with the table or with the attributes. The words are
 * keyed by address: they must outlive the pipeline (the app's are the .spv.h arrays) */
bool objBlPipe( int twin, const std::uint32_t * vs, std::size_t vsWords, const std::uint32_t * ps, std::size_t psWords,
	const ObjBlTarget & target, const std::vector<ObjBlBuffer> & buffers, const std::vector<ObjBlAttr> & attrs,
	ObjBlPipeInfo & info, std::string & why );
//! the twins' pipelines made so far (and refused)
int objBlPipes( int * refused = nullptr );
//! shutdown(): the pipelines, layouts and render passes (before blFree: the layouts name the table's)
void objBlPipeFree();

/* Slice 5 (the mirrored state): GL's program state into set 0, read by name through the module's own layout.
 * the loose uniforms (the twin's LooseUniforms block) */
struct ObjBlUniStat
{
	int members = 0, mirrored = 0, absent = 0;  //!< absent: GL has no location (the GLSL never reads it): left 0
	int registry = 0;  //!< of mirrored: found through the location registry (a SPIR-V-linked program: no uniform names)
};
/*! the SPIR-V set's location registry (build_all.py's spirv/locs.txt: `name \t location \t slots`; an array's element i at
 * location + i). A program whose active uniforms have no names (linked from SPIR-V) is mirrored through it: a name GL does
 * not find takes the registry's location when GL reports that location active. Returns the names read (0: none, the
 * registry is cleared). red 1 shifts every location by one (gates only: the mirror must FAIL) */
int objBlLocsLoad( const std::string & file, std::string & why, int red = 0 );
/*! GL program `prog`'s loose uniforms into a block laid out as `block` (glGetUniform*, by the member's name, an array's
 * elements by name[i]; a GL matrix's column c is the module's column c). false with a reason: a member of a type the mirror
 * does not take. red 1 matrices transposed, red 2 arrays packed tight (gates only: they must FAIL) */
bool objBlLoose( GlProc gp, unsigned prog, const SpvBind & block, std::vector<std::uint8_t> & out, ObjBlUniStat & st, std::string & why,
	int red = 0 );
/*! GL's std140 block of the same name (globalUniforms, skinningUniforms): the bytes of the buffer range bound at its binding.
 * A SPIR-V-linked program's block GL does not find by name is found by build_spv.py's binding (globalUniforms 0,
 * skinningUniforms 1, wwShadowFrame 2).
 * false with a reason: no such block, no buffer bound, a member GL places elsewhere than the module, a smaller block.
 * red 1 reads the binding after GL's (gates only) */
bool objBlBlock( GlProc gp, unsigned prog, const SpvBind & block, std::vector<std::uint8_t> & out, std::string & why, int red = 0 );
//! one set-0 texture-like binding and the GL texture its sampler uniform's unit holds (a sampler binding `_X_sampler`: X's)
struct ObjBlTexUnit
{
	std::uint32_t binding = 0, type = 0, dim = 0, arrayed = 0;
	std::string name;   //!< GL's sampler uniform
	int unit = -1;      //!< -1: GL has no such uniform (the GLSL never reads it)
	unsigned glTex = 0, glTarget = 0;
};
//! the module's set-0 images, texel buffers and samplers with GL's units and textures (the active unit restored)
bool objBlTextures( GlProc gp, unsigned prog, const SpvModule & m, std::vector<ObjBlTexUnit> & out, std::string & why );

/* Slice 6 (the pools as vertex buffers): a class pool of cellbindless.cpp (setupVao: attribute i = nibble i of attrMask
 * floats, its own buffer, tightly packed GL_FLOATs; GL_UNSIGNED_SHORT indices) */
struct ObjBlPool
{
	std::uint64_t attrMask = 0;
	unsigned vbo[16] = {}, ebo = 0;  //!< GL's names
	std::size_t verts = 0, indices = 0;
};
/*! the class's VAO as the pipeline's vertex input: attribute i on buffer binding i, stride 4 n, R32..._SFLOAT, GL location i.
 * A GL location in `need` (bit L) the class does not carry reads GL's generic default (0,0,0,1: the cell pass never sets a
 * current value): binding 16, stride 0, one R32G32B32A32 (the twins' constant buffer) */
constexpr std::uint32_t kObjBlConstBinding = 16;
void objBlPoolLayout( std::uint64_t attrMask, std::vector<ObjBlBuffer> & buffers, std::vector<ObjBlAttr> & attrs, std::uint32_t need = 0 );
/*! GL's pool into the class's twins on the present device (glCopyNamedBufferSubData into buffers GL imports; the twins grow
 * by doubling), GL's order: call before the run's signal and list `glNames` in it. false with a reason (nothing copied).
 * red 1 copies each attribute from the buffer of the attribute before it (gates only) */
bool objBlPoolCopy( std::uintptr_t key, const ObjBlPool & p, std::vector<unsigned> & glNames, std::string & why, int red = 0 );
//! the class went (its arena dropped): its twins go with the next free
void objBlPoolDrop( std::uintptr_t key );
//! the twins held (and their bytes)
int objBlPoolCount( std::size_t * bytes = nullptr );

/* Slice 7 (the draw): one class's multi-draw through twin `twin`'s pipeline into shared targets (sharedTarget keys), GL's
 * state in set 0, the table as set 1, GL's DrawElementsIndirectCommand array as Vulkan's (the same 20 bytes: count,
 * instances, first index, base vertex, base instance; the row = wwDrawBase + base instance, BaseInstance). A set-0 image
 * not given reads a 1x1 (0,0,0,1) of its shape (GL's incomplete texture), a texel buffer not given one zero texel, a
 * sampler not given the table's default. Prep runs on GL's thread before the run's signal; the record is the worker's. */
//! a GL texture's sampler state as a set-0 sampler takes it: wrap R and the depth compare too (the CSM's)
struct ObjBlSampler
{
	unsigned minFilter = 0x2702, magFilter = 0x2601;  // GL's defaults
	unsigned wrapS = 0x2901, wrapT = 0x2901, wrapR = 0x2901;
	unsigned compareMode = 0, compareFunc = 0x0203;  // GL_NONE, GL_LEQUAL
	float lodBias = 0.0f, minLod = -1000.0f, maxLod = 1000.0f, aniso = 1.0f;
	float border[4] = {};
};
//! GL's texture state reads (glGetTextureParameter*, GL 4.5)
bool objBlSamplerRead( GlProc gp, unsigned tex, ObjBlSampler & s, std::string & why );
struct ObjBlDraw
{
	int twin = 0;
	const std::uint32_t * vs = nullptr, * ps = nullptr;
	std::size_t vsWords = 0, psWords = 0;
	ObjBlTarget target;
	std::uintptr_t colorKeys[4] = {}, depthKey = 0;
	int w = 0, h = 0;
	std::uintptr_t poolKey = 0;  //!< the class objBlPoolCopy filled
	std::uint64_t attrMask = 0;
	std::uint32_t need = 0;      //!< the GL locations the vertex stage reads (bit L)
	std::vector<std::pair<std::uint32_t, std::vector<std::uint8_t>>> ubos;  //!< binding -> bytes (objBlMirror)
	std::vector<std::pair<std::uint32_t, std::uintptr_t>> images;          //!< binding -> a shared target
	std::vector<std::pair<std::uint32_t, ObjBlSampler>> samplers;          //!< binding -> GL's state, else the default
	std::vector<std::pair<std::uint32_t, unsigned>> texels;                //!< binding -> GL's buffer texture (objBlTexelCopy made its twin)
	const void * glRows = nullptr;  //!< GL's draw rows (the bindless ring, GL form), translated into wwDraws
	std::size_t rows = 0;
	const void * cmds = nullptr;    //!< GL's indirect commands
	std::size_t cmdCount = 0;
};
struct ObjBlDrawStat
{
	int ubos = 0, images = 0, standIns = 0, texels = 0, samplers = 0, rowBuffers = 0;
	int glSamplers = 0;     //!< samplers with GL's state (the rest the default)
	int texelsGl = 0;       //!< texel buffers from GL's twins (the rest the zero texel)
	std::size_t whole = 0;  //!< rows with every handle known
};
//! what objBlBindUnits found on GL's units
struct ObjBlBindStat
{
	int shared = 0;    //!< images from a shared target
	int glOwn = 0;     //!< a GL texture that is no shared target (a stand-in)
	int misfit = 0;    //!< a shared target the binding cannot take (shape or samples; a stand-in)
	int unbound = 0;   //!< no unit or no texture
	int texelGl = 0;   //!< a texel buffer GL binds
	int texelShared = 0, texelRefused = 0;  //!< of those, copied into a twin (d.texels) / refused (the zero texel on Vulkan)
	int samplers = 0;  //!< samplers given GL's state
	int compare = 0;   //!< of those, depth compare
	std::string misfitWhy, texelWhy;
	std::vector<unsigned> glBufs;  //!< the texel twins GL wrote (the crossing's signal takes them)
};
/*! GL's texture units of `prog` for both of d's modules: a set-0 image GL binds a shared target to joins d.images
 * (shape and samples checked), each sampler takes GL's state of its texture, each texel buffer GL binds is copied into its
 * twin (objBlTexelCopy) and joins d.texels. The rest stay stand-ins. */
bool objBlBindUnits( GlProc gp, unsigned prog, ObjBlDraw & d, ObjBlBindStat & st, std::string & why );
/*! GL's buffer texture `tex` (its range: glTexBufferRange's, else the whole buffer) copied into its twin on the present device,
 * a buffer GL imports (glCopyNamedBufferSubData, ordered before the crossing's signal; glNames gets the twin's GL name).
 * Formats: R / RG / RGBA 32F, 32I, 32UI, RGBA16F, RGBA8; others are refused. red 1 copies from 16 bytes past the start. */
bool objBlTexelCopy( GlProc gp, unsigned tex, std::vector<unsigned> & glNames, std::string & why, int red = 0 );
//! the shared target `key` fits a set-0 image of SPIR-V Dim `dim` / Arrayed (2D, 3D, 2D array, cube array; one sample)
bool objBlSharedFits( std::uintptr_t key, std::uint32_t dim, std::uint32_t arrayed, std::string & why );
//! GL program `prog`'s state for both modules' uniform blocks (LooseUniforms by objBlLoose, the named blocks by objBlBlock)
bool objBlMirror( GlProc gp, unsigned prog, const std::uint32_t * vs, std::size_t vsWords, const std::uint32_t * ps, std::size_t psWords,
	std::vector<std::pair<std::uint32_t, std::vector<std::uint8_t>>> & ubos, ObjBlUniStat & st, std::string & why );
//! a frame's start: no draws prepared, no rows (the slots' allocations stay for the next prep)
void objBlDrawsReset();
/*! one more draw ready to record (the worker's objBlDrawRecord, in prep order): the pipeline (objBlPipe from the class's
 * layout), its own set 0 written, the rows (this draw's glRows, else the frame's so far) and commands on the device, the
 * framebuffer of the targets. The host buffers are rewritten: the slot's last submit must be done. false with a reason
 * (nothing added) */
bool objBlDrawPrep( const ObjBlDraw & d, ObjBlDrawStat & st, std::string & why );

/* the crossing (as objRun's): GL current, the draws prepared, every GL name the copies wrote (`glBufs`, objBlPoolCopy's):
 * GL's signal over them and the draws' shared targets (GENERAL), the worker records the prepared draws (`mode` as
 * objBlDrawRecord's: 0 MDI, 1 one indirect per command, 2 direct) and submits, GL waits ours. The next prep / reset waits
 * the submit. false: why (nothing signalled; GL draws them) */
struct ObjBlRunStat
{
	int runs = 0, draws = 0, cmds = 0, gl = 0, fails = 0;
	int shared = 0, glOwn = 0, texelGl = 0, compare = 0;  //!< objBlShade's units: shared images, GL-only textures, GL texel buffers, compare samplers
	int texelShared = 0;  //!< of the texel buffers, those drawn from their twins
	double cpuMs = 0.0, maxMs = 0.0;
	std::string glWhy, failWhy;
};
bool objBlRun( const std::vector<unsigned> & glBufs, int mode, std::string & why );
void objBlOnGl( const char * why );
ObjBlRunStat takeObjBlRunStat();  //!< and starts the next count

/* the app's crossing (cellbindless.cpp runIssue, WW_VKOBJ_SHADE=1): one multi-draw run of class `poolKey` on Vulkan instead
 * of GL's glMultiDrawElementsIndirect. Read from GL here: the draw framebuffer (every colour draw buffer and the depth must
 * be shared targets), the depth / cull / blend state (objBlTargetState; refused: a viewport that is not the frame),
 * program `prog`'s uniforms (objBlMirror). The rows are the run's own from its leader: Vulkan's wwDrawBase is 0 (GL's
 * wwDrawIdx is not a module member: the mirror leaves it 0). false: why (nothing drawn; GL draws the run) */
struct ObjBlShade
{
	int twin = 0;           //!< 0 fo4_cellbl, 1 fo4_cellcsmbl
	unsigned prog = 0;      //!< GL's current program
	std::uintptr_t poolKey = 0;
	ObjBlPool pool;
	const void * rows = nullptr;  //!< GL form, the leader's first
	std::size_t nRows = 0;
	const void * cmds = nullptr;  //!< 5 words a command (the base instance relative to the leader)
	std::size_t nCmds = 0;
};
bool objBlShade( GlProc gp, const ObjBlShade & s, std::string & why );
/*! objBlShade's frame read on its own: GL's draw framebuffer as d's targets (every colour draw buffer and the depth / stencil
 * a shared target: keys, formats, size, samples), the viewport the whole frame, the fixed state (objBlTargetState with `red`).
 * false: why (d's target untouched past the first refusal) */
bool objBlFrameRead( GlProc gp, ObjBlDraw & d, std::string & why, int red = 0 );
/*! lane VKOBJ4 open item 3: the cell frame's colour (GL format colorFmt) and depth / stencil (dsFmt) as shared targets of
 * w x h at `samples` (GL names in color / ds: GL attaches them; a Vulkan run draws into them). Kept while the shape holds.
 * objBlFrameDrop frees both; objBlFrameIs: `color` is this frame's colour. */
bool objBlFrame( int w, int h, int samples, unsigned colorFmt, unsigned dsFmt, unsigned & color, unsigned & ds, std::string & why );
void objBlFrameDrop();
bool objBlFrameIs( unsigned color );
}  // namespace WwVkPresent
