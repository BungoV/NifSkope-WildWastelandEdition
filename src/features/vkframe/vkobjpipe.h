#pragma once

/* Lane VKOBJ4 (2026-11-13, plan row G4 slice 4): the shaded cell shapes' pipelines on the present device, one per bl twin
 * (fo4_defaultbl_vs + fo4_cellbl_ps / fo4_cellcsmbl_ps, -D WW_VK_BINDLESS). Both stages' resources are read from the
 * modules (spvReflect): set 0 = the twin's own bindings (the pixel stage on fx()'s shifts b 0 / t 64 / s 128 / u 192, the
 * vertex stage on its own, b 256 / t 320 / s 384 / u 448: its b0 skinningUniforms would land on the pixel stage's b0
 * globalUniforms), set 1 = the bindless table (vkbindless.inc) and nothing else. Refused with a reason: a binding two stages
 * declare differently, a set 1 that is not the table, a varying the vertex stage does not write, a target count the pixel
 * stage does not write, a vertex input no attribute feeds. Attributes are given by GL's attribute location (the HLSL twin's
 * TEXCOORDn); DXC numbers the inputs on its own (fo4_defaultbl_vs: TEXCOORD9 is Location 8), so the semantic maps them.
 * Not drawn yet: the mirrored uniforms, the pools as vertex buffers, MDI and the crossing are the next slices.
 * Gate: tests/vkframe/blview.sh (the `BLVIEW pipe` lines; reds shift / table / varying / attr must FAIL). */

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
	std::vector<SpvMember> members;                           //!< a uniform block's members
	std::uint32_t bytes = 0;                                  //!< a uniform block's size (the last member's end, to 16)
};
//! one stage input / output with a Location (a matrix or array takes `slots` locations)
struct SpvIo
{
	std::uint32_t location = 0, slots = 1, comps = 1, base = 0;  //!< base: 0 float, 1 int, 2 uint
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
//! the frame the pipeline draws into, as GL's cell pass draws (colour formats as VkFormat, GL's depth state)
struct ObjBlTarget
{
	std::uint32_t colors[4] = {}, colorCount = 0, depth = 0, samples = 1;
	std::uint32_t depthOp = 3;  //!< a VkCompareOp (3 = LESS_OR_EQUAL)
	bool depthWrite = false, twoSided = false;
};
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
};
/*! GL program `prog`'s loose uniforms into a block laid out as `block` (glGetUniform*, by the member's name, an array's
 * elements by name[i]; a GL matrix's column c is the module's column c). false with a reason: a member of a type the mirror
 * does not take. red 1 matrices transposed, red 2 arrays packed tight (gates only: they must FAIL) */
bool objBlLoose( GlProc gp, unsigned prog, const SpvBind & block, std::vector<std::uint8_t> & out, ObjBlUniStat & st, std::string & why,
	int red = 0 );
/*! GL's std140 block of the same name (globalUniforms, skinningUniforms): the bytes of the buffer range bound at its binding.
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
}  // namespace WwVkPresent
