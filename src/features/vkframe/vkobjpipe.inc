// Lane VKOBJ4 (2026-11-13, plan row G4 slice 4): the bl twins' shaded pipelines, both stages' resources read from the
// modules. Included by vkpresent.cpp after vkbindless.inc (blSetGet: the table is set 1). The API: vkobjpipe.h.
#include "features/vkframe/vkobjpipe.h"
#include "features/vkframe/objbl_vs.spv.h"  // the twins (vkframe_spirv.sh fxb: VS_SHIFT / PS_SHIFT, -D WW_VK_BINDLESS)
#include "features/vkframe/objbl_cell_ps.spv.h"
#include "features/vkframe/objbl_cellcsm_ps.spv.h"
#include "features/vkframe/objbl_samples_cs.spv.h"  // the cell frame's per-sample readback (objBlFrameSamples)

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>
#include <type_traits>
#include <vector>

namespace
{
std::string spvString( const std::uint32_t * a, std::size_t words )
{
	std::string s;
	for ( std::size_t i = 0; i < words * 4; i++ ) {
		const char c = char( ( a[i / 4] >> ( 8 * ( i % 4 ) ) ) & 0xFFu );
		if ( !c )
			break;
		s += c;
	}
	return s;
}

// a vertex attribute format's base type and component count (0 comps: a format the builder does not take)
void opFormat( VkFormat f, std::uint32_t & base, std::uint32_t & comps )
{
	base = 0;
	comps = 0;
	switch ( f ) {
	case VK_FORMAT_R32_SFLOAT: case VK_FORMAT_R16_SFLOAT: case VK_FORMAT_R8_UNORM: case VK_FORMAT_R8_SNORM: comps = 1; break;
	case VK_FORMAT_R32G32_SFLOAT: case VK_FORMAT_R16G16_SFLOAT: case VK_FORMAT_R16G16_UNORM: case VK_FORMAT_R16G16_SNORM:
	case VK_FORMAT_R8G8_UNORM: case VK_FORMAT_R8G8_SNORM: comps = 2; break;
	case VK_FORMAT_R32G32B32_SFLOAT: comps = 3; break;
	case VK_FORMAT_R32G32B32A32_SFLOAT: case VK_FORMAT_R16G16B16A16_SFLOAT: case VK_FORMAT_R16G16B16A16_UNORM:
	case VK_FORMAT_R16G16B16A16_SNORM: case VK_FORMAT_R8G8B8A8_UNORM: case VK_FORMAT_R8G8B8A8_SNORM: case VK_FORMAT_B8G8R8A8_UNORM:
	case VK_FORMAT_A2B10G10R10_UNORM_PACK32: case VK_FORMAT_A2B10G10R10_SNORM_PACK32: comps = 4; break;
	case VK_FORMAT_R32_SINT: base = 1; comps = 1; break;
	case VK_FORMAT_R32G32_SINT: base = 1; comps = 2; break;
	case VK_FORMAT_R32G32B32_SINT: base = 1; comps = 3; break;
	case VK_FORMAT_R32G32B32A32_SINT: base = 1; comps = 4; break;
	case VK_FORMAT_R32_UINT: base = 2; comps = 1; break;
	case VK_FORMAT_R32G32_UINT: base = 2; comps = 2; break;
	case VK_FORMAT_R32G32B32_UINT: base = 2; comps = 3; break;
	case VK_FORMAT_R32G32B32A32_UINT: base = 2; comps = 4; break;
	default: break;
	}
}

struct OpEntry
{
	int twin = 0;
	const std::uint32_t * vs = nullptr, * ps = nullptr;
	std::size_t vsN = 0, psN = 0;
	WwVkPresent::ObjBlTarget target;
	std::vector<WwVkPresent::ObjBlBuffer> buffers;
	std::vector<WwVkPresent::ObjBlAttr> attrs;
	WwVkPresent::ObjBlPipeInfo info;
	VkDescriptorSetLayout dsl0 = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkRenderPass pass = VK_NULL_HANDLE;
	VkPipeline pipe = VK_NULL_HANDLE;
	std::vector<WwVkPresent::SpvBind> set0;  // both stages' set-0 bindings (stages merged)
};

// one prepared draw (slice 7): its pipeline, set 0 (own pool), a host buffer per uniform block, the commands (on the device
// and a host copy for the direct route), the framebuffer of its targets
struct OpDraw
{
	std::size_t entry = 0;
	VkDescriptorPool dpool = VK_NULL_HANDLE;
	VkDescriptorSet dset = VK_NULL_HANDLE;
	std::map<std::uint32_t, CsmBuf> ubo;
	CsmBuf cmds;
	std::vector<std::uint8_t> host;
	std::size_t cmdCount = 0;
	std::uintptr_t poolKey = 0;
	VkFramebuffer fb = VK_NULL_HANDLE;
	std::vector<VkImageView> fbViews;
	int w = 0, h = 0;
	VkRect2D scissor {};  // lane VKOBJ4 open item 3: GL's scissor box (objBlFrameRead), the whole frame when GL's test is off
	std::vector<std::uintptr_t> keys;  // the shared targets it draws into or reads (the crossing hands them over)
};

// a set-0 image no one gives: a 1x1 (0,0,0,1) of its shape and element type (GL's incomplete texture)
struct OpStandIn
{
	VkImage img = VK_NULL_HANDLE;
	VkDeviceMemory mem = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
};

// a class pool's twins: one exported buffer per attribute, the indices (GL imports each: the copy target)
struct OpPool
{
	CsmBuf vb[16], ib;
	std::uint64_t attrMask = 0;
	bool gone = false;
};

// GL's buffer texture's twin (objBlTexelCopy: a buffer GL imports) and its view (opTexelView)
struct OpTexel
{
	CsmBuf buf;
	VkFormat fmt = VK_FORMAT_UNDEFINED;
	std::uint32_t cls = 0;  // the element class as SPIR-V's sampled type: 0 float, 1 int, 2 uint
	VkDeviceSize range = 0;
	VkBufferView view = VK_NULL_HANDLE;
	VkBuffer viewBuf = VK_NULL_HANDLE;
	VkFormat viewFmt = VK_FORMAT_UNDEFINED;
	VkDeviceSize viewRange = 0;
};

struct Op
{
	std::vector<OpEntry> made;
	int refused = 0;
	std::map<std::uintptr_t, OpPool> pools;
	CsmBuf constVb;  // GL's generic attribute default (0,0,0,1) for the locations a class does not carry (host-visible)
	// the draws (slice 7): the frame's rows (wwDraws), the prepared draws (`live` of them this frame), the stand-ins
	CsmBuf rows;
	std::size_t rowCount = 0;
	std::vector<OpDraw> draws;
	std::size_t live = 0;
	std::map<std::uint32_t, OpStandIn> stand;  // key: view type * 4 + element type
	CsmBuf zeroTexel;
	VkBufferView texelView[3] = {};             // float / int / uint
	std::map<unsigned, OpTexel> texTwins;       // GL's buffer texture -> its twin
	std::map<std::string, VkSampler> smps;      // GL's sampler states met (key: the ObjBlSampler's bytes)
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	// the crossing (objBlRun): GL's signal -> the worker's submit -> GL's wait; its own command buffer, fence, semaphores
	VkCommandBuffer runCmd = VK_NULL_HANDLE;
	VkFence runFence = VK_NULL_HANDLE;
	VkSemaphore glDone = VK_NULL_HANDLE, vkDone = VK_NULL_HANDLE;
	GLuint glDoneGl = 0, vkDoneGl = 0;
	void * semCtx = nullptr;
	bool pending = false;
	int runMode = 0;
	WwVkPresent::ObjBlRunStat st;
	// the cell frame's per-sample readback (objBlFrameSamples): a compute pass on the crossing's command buffer
	VkDescriptorSetLayout smpDsl = VK_NULL_HANDLE;
	VkPipelineLayout smpLayout = VK_NULL_HANDLE;
	VkPipeline smpPipe = VK_NULL_HANDLE;
	VkDescriptorPool smpPool = VK_NULL_HANDLE;
	VkDescriptorSet smpSet = VK_NULL_HANDLE;
	VkImageView smpDepth = VK_NULL_HANDLE;    // the depth target's depth aspect, made per readback (a borrowed one's view has both)
	VkImageView smpStencil = VK_NULL_HANDLE;  // the depth target's stencil aspect, made per readback
	CsmBuf smpOut;                            // host-visible: one uint4 per sample
	std::uint32_t smpJob[4] = {};             // w, h, samples, red (objbl_samples_cs.hlsl's ReadJob)
};

Op & opS()
{
	static Op o;
	return o;
}

bool opSame( const OpEntry & e, int twin, const std::uint32_t * vs, std::size_t vsN, const std::uint32_t * ps, std::size_t psN,
	const WwVkPresent::ObjBlTarget & t, const std::vector<WwVkPresent::ObjBlBuffer> & b, const std::vector<WwVkPresent::ObjBlAttr> & a )
{
	if ( e.twin != twin || e.vs != vs || e.vsN != vsN || e.ps != ps || e.psN != psN || b.size() != e.buffers.size() || a.size() != e.attrs.size() )
		return false;
	const WwVkPresent::ObjBlTarget & u = e.target;
	if ( std::memcmp( t.colors, u.colors, sizeof( t.colors ) ) || t.colorCount != u.colorCount || t.depth != u.depth || t.samples != u.samples
		|| t.depthOp != u.depthOp || t.depthWrite != u.depthWrite || t.twoSided != u.twoSided
		|| std::memcmp( &t.blend, &u.blend, sizeof( t.blend ) ) || std::memcmp( &t.stencil, &u.stencil, sizeof( t.stencil ) ) )
		return false;
	for ( std::size_t i = 0; i < b.size(); i++ )
		if ( b[i].binding != e.buffers[i].binding || b[i].stride != e.buffers[i].stride || b[i].perInstance != e.buffers[i].perInstance )
			return false;
	for ( std::size_t i = 0; i < a.size(); i++ )
		if ( a[i].glLocation != e.attrs[i].glLocation || a[i].binding != e.attrs[i].binding || a[i].format != e.attrs[i].format
			|| a[i].offset != e.attrs[i].offset )
			return false;
	return true;
}

void opDestroy( OpEntry & e )
{
	Vk & v = vk();
	if ( e.pipe )
		v.vkDestroyPipeline( v.dev, e.pipe, nullptr );
	if ( e.layout )
		v.vkDestroyPipelineLayout( v.dev, e.layout, nullptr );
	if ( e.dsl0 )
		v.vkDestroyDescriptorSetLayout( v.dev, e.dsl0, nullptr );
	if ( e.pass )
		v.vkDestroyRenderPass( v.dev, e.pass, nullptr );
	e.pipe = VK_NULL_HANDLE;
	e.layout = VK_NULL_HANDLE;
	e.dsl0 = VK_NULL_HANDLE;
	e.pass = VK_NULL_HANDLE;
}

void opDrawFree( OpDraw & d )
{
	Vk & v = vk();
	if ( d.fb )
		v.vkDestroyFramebuffer( v.dev, d.fb, nullptr );
	if ( d.dpool )
		v.vkDestroyDescriptorPool( v.dev, d.dpool, nullptr );
	for ( auto & u : d.ubo )
		csmBufFree( u.second, false );
	csmBufFree( d.cmds, false );
	d = OpDraw();
}

void opRunSemsDrop( bool glAlive )
{
	Vk & v = vk();
	Gl & g = gl();
	Op & O = opS();
	if ( glAlive )
		for ( GLuint * s : { &O.glDoneGl, &O.vkDoneGl } )
			if ( *s )
				g.deleteSemaphores( 1, s );
	for ( VkSemaphore * s : { &O.glDone, &O.vkDone } )
		if ( *s )
			v.vkDestroySemaphore( v.dev, *s, nullptr );
	O.glDoneGl = O.vkDoneGl = 0;
	O.glDone = O.vkDone = VK_NULL_HANDLE;
	O.semCtx = nullptr;
}

const char * opStage( std::uint32_t s )
{
	return s == VK_SHADER_STAGE_VERTEX_BIT ? "the vertex stage" : "the pixel stage";
}
}  // namespace

namespace WwVkPresent
{
bool spvReflect( const std::uint32_t * w, std::size_t n, SpvModule & out, std::string & why )
{
	out = SpvModule();
	if ( !w || n < 5 || w[0] != 0x07230203u ) {
		why = "not SPIR-V";
		return false;
	}
	struct Ty
	{
		std::uint32_t op = 0, a = 0, b = 0, c = 0, d = 0;  // the type's opcode and its operands (image: dim / sampled)
	};
	std::map<std::uint32_t, Ty> types;
	std::map<std::uint32_t, std::uint32_t> binding, setOf, location, constants;
	std::map<std::uint32_t, int> builtin, block, bufferBlock;
	std::map<std::uint32_t, std::string> names;
	// a block's members: their type ids, names and layout decorations; an array type's stride
	std::map<std::uint32_t, std::vector<std::uint32_t>> structMembers;
	std::map<std::uint32_t, std::map<std::uint32_t, std::string>> memberNames;
	std::map<std::uint32_t, std::map<std::uint32_t, std::uint32_t>> memberOffset, memberStride;
	std::map<std::uint32_t, std::map<std::uint32_t, int>> memberRowMajor;
	std::map<std::uint32_t, std::uint32_t> arrayStride;
	struct Var
	{
		std::uint32_t id = 0, ptr = 0, storage = 0;
	};
	std::vector<Var> vars;
	for ( std::size_t i = 5; i < n; ) {
		const std::uint32_t op = w[i] & 0xFFFFu, wc = w[i] >> 16;
		if ( !wc || i + wc > n ) {
			why = "a broken instruction at word " + std::to_string( i );
			return false;
		}
		const std::uint32_t * a = w + i + 1;
		switch ( op ) {
		case 5:  // OpName
			if ( wc >= 3 )
				names[a[0]] = spvString( a + 1, wc - 2 );
			break;
		case 6:  // OpMemberName
			if ( wc >= 4 )
				memberNames[a[0]][a[1]] = spvString( a + 2, wc - 3 );
			break;
		case 72:  // OpMemberDecorate: RowMajor 4, ColMajor 5, MatrixStride 7, Offset 35
			if ( wc >= 4 && a[2] == 4 )
				memberRowMajor[a[0]][a[1]] = 1;
			else if ( wc >= 5 && a[2] == 7 )
				memberStride[a[0]][a[1]] = a[3];
			else if ( wc >= 5 && a[2] == 35 )
				memberOffset[a[0]][a[1]] = a[3];
			break;
		case 15:  // OpEntryPoint
			if ( wc >= 3 && !out.stage )
				out.stage = a[0] == 0 ? VK_SHADER_STAGE_VERTEX_BIT : a[0] == 4 ? VK_SHADER_STAGE_FRAGMENT_BIT : a[0] == 5 ? VK_SHADER_STAGE_COMPUTE_BIT : 0;
			break;
		case 71:  // OpDecorate
			if ( wc >= 3 && a[1] == 2 )
				block[a[0]] = 1;
			else if ( wc >= 3 && a[1] == 3 )
				bufferBlock[a[0]] = 1;
			else if ( wc >= 4 && a[1] == 11 )
				builtin[a[0]] = 1;
			else if ( wc >= 4 && a[1] == 30 )
				location[a[0]] = a[2];
			else if ( wc >= 4 && a[1] == 33 )
				binding[a[0]] = a[2];
			else if ( wc >= 4 && a[1] == 34 )
				setOf[a[0]] = a[2];
			else if ( wc >= 4 && a[1] == 6 )
				arrayStride[a[0]] = a[2];
			break;
		case 20: types[a[0]] = { op }; break;                                       // OpTypeBool
		case 21: if ( wc >= 4 ) types[a[0]] = { op, a[1], a[2] }; break;            // OpTypeInt: width, signed
		case 22: if ( wc >= 3 ) types[a[0]] = { op, a[1] }; break;                  // OpTypeFloat
		case 23: case 24: if ( wc >= 4 ) types[a[0]] = { op, a[1], a[2] }; break;  // OpTypeVector / Matrix: element, count
		case 25: if ( wc >= 9 ) types[a[0]] = { op, a[2], a[6], a[4], a[1] }; break;  // OpTypeImage: dim, sampled, arrayed, element
		case 26: types[a[0]] = { op }; break;                                       // OpTypeSampler
		case 27: if ( wc >= 3 ) types[a[0]] = { op, a[1] }; break;                  // OpTypeSampledImage
		case 28: if ( wc >= 4 ) types[a[0]] = { op, a[1], a[2] }; break;            // OpTypeArray: element, length id
		case 29: if ( wc >= 3 ) types[a[0]] = { op, a[1] }; break;                  // OpTypeRuntimeArray
		case 30:                                                                    // OpTypeStruct
			types[a[0]] = { op };
			structMembers[a[0]].assign( a + 1, a + wc - 1 );
			break;
		case 32: if ( wc >= 4 ) types[a[0]] = { op, a[1], a[2] }; break;            // OpTypePointer: storage, type
		case 43: if ( wc >= 4 ) constants[a[1]] = a[2]; break;                      // OpConstant
		case 59: if ( wc >= 4 ) vars.push_back( { a[1], a[0], a[2] } ); break;      // OpVariable
		default: break;
		}
		i += wc;
	}
	if ( !out.stage ) {
		why = "no vertex / fragment / compute entry point";
		return false;
	}
	auto ty = [&]( std::uint32_t id ) {
		auto it = types.find( id );
		return it == types.end() ? Ty() : it->second;
	};
	for ( const Var & v : vars ) {
		const Ty p = ty( v.ptr );
		std::uint32_t t = p.b;
		const std::string nm = names.count( v.id ) ? names[v.id] : "%" + std::to_string( v.id );
		if ( v.storage == 1 || v.storage == 3 ) {  // Input / Output
			if ( builtin.count( v.id ) )
				continue;
			if ( !location.count( v.id ) ) {
				why = nm + " has no Location";
				return false;
			}
			SpvIo io;
			io.location = location[v.id];
			io.name = nm;
			Ty x = ty( t );
			if ( x.op == 28 ) {
				io.locs = constants.count( x.b ) ? constants[x.b] : 1;
				x = ty( x.a );
			}
			if ( x.op == 24 ) {
				io.locs *= x.b;
				x = ty( x.a );
			}
			if ( x.op == 23 ) {
				io.comps = x.b;
				x = ty( x.a );
			}
			if ( x.op == 22 )
				io.base = 0;
			else if ( x.op == 21 )
				io.base = x.b ? 1 : 2;
			else {
				why = nm + ": an interface type that is not float / int / uint";
				return false;
			}
			( v.storage == 1 ? out.inputs : out.outputs ).push_back( io );
			continue;
		}
		if ( v.storage == 9 ) {
			why = nm + ": push constants (the twins' pipeline layout has none)";
			return false;
		}
		if ( v.storage != 0 && v.storage != 2 && v.storage != 12 )
			continue;  // Private / Function / Workgroup
		if ( !binding.count( v.id ) ) {
			why = nm + " has no Binding";
			return false;
		}
		SpvBind b;
		b.set = setOf.count( v.id ) ? setOf[v.id] : 0;
		b.binding = binding[v.id];
		b.stages = out.stage;
		b.name = nm;
		Ty x = ty( t );
		if ( x.op == 28 ) {
			b.count = constants.count( x.b ) ? constants[x.b] : 1;
			t = x.a;
			x = ty( t );
		} else if ( x.op == 29 ) {
			b.count = 0;
			t = x.a;
			x = ty( t );
		}
		if ( x.op == 27 ) {
			b.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			x = ty( x.a );
			b.dim = x.a;
			b.arrayed = x.c;
			const Ty el = ty( x.d );
			b.sampled = el.op == 21 ? ( el.b ? 1u : 2u ) : 0u;
		} else if ( x.op == 25 ) {
			b.type = x.a == 5 ? ( x.b == 2 ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER )
				: ( x.b == 2 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE );
			b.dim = x.a;
			b.arrayed = x.c;
			const Ty el = ty( x.d );
			b.sampled = el.op == 21 ? ( el.b ? 1u : 2u ) : 0u;
		} else if ( x.op == 26 )
			b.type = VK_DESCRIPTOR_TYPE_SAMPLER;
		else if ( x.op == 30 && ( v.storage == 12 || bufferBlock.count( t ) ) )
			b.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		else if ( x.op == 30 && v.storage == 2 && block.count( t ) ) {
			b.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			// the members: scalar / vector / matrix, or an array of one (a nested struct is base 3: the mirror refuses it)
			const std::vector<std::uint32_t> & mt = structMembers[t];
			for ( std::uint32_t k = 0; k < mt.size(); k++ ) {
				SpvMember m;
				m.name = memberNames[t].count( k ) ? memberNames[t][k] : "%" + std::to_string( k );
				m.offset = memberOffset[t].count( k ) ? memberOffset[t][k] : 0;
				m.colStride = memberStride[t].count( k ) ? memberStride[t][k] : 0;
				m.rowMajor = memberRowMajor[t].count( k ) != 0;
				Ty e = ty( mt[k] );
				if ( e.op == 28 ) {
					m.count = constants.count( e.b ) ? constants[e.b] : 1;
					m.arrayStride = arrayStride.count( mt[k] ) ? arrayStride[mt[k]] : 0;
					e = ty( e.a );
				}
				if ( e.op == 24 ) {
					m.cols = e.b;
					e = ty( e.a );
				}
				if ( e.op == 23 ) {
					m.comps = e.b;
					e = ty( e.a );
				}
				m.base = e.op == 22 ? 0 : e.op == 21 ? ( e.b ? 1 : 2 ) : 3;
				// the member's extent: the last element's last component
				const std::uint32_t one = m.cols > 1 ? ( m.rowMajor ? ( m.comps - 1 ) * m.colStride + m.cols * 4 : ( m.cols - 1 ) * m.colStride + m.comps * 4 )
					: m.comps * 4;
				b.bytes = std::max( b.bytes, m.offset + ( m.count - 1 ) * m.arrayStride + one );
				b.members.push_back( m );
			}
			b.bytes = ( b.bytes + 15u ) & ~15u;
		} else {
			why = nm + ": a descriptor type the reader does not take";
			return false;
		}
		out.binds.push_back( b );
	}
	return true;
}

bool objBlPipe( int twin, const std::uint32_t * vs, std::size_t vsN, const std::uint32_t * ps, std::size_t psN, const ObjBlTarget & target,
	const std::vector<ObjBlBuffer> & buffers, const std::vector<ObjBlAttr> & attrs, ObjBlPipeInfo & info, std::string & why, int red )
{
	Vk & v = vk();
	Op & O = opS();
	for ( const OpEntry & e : O.made )
		if ( !red && opSame( e, twin, vs, vsN, ps, psN, target, buffers, attrs ) ) {
			info = e.info;
			return true;
		}
	const double t0 = nowMs();
	auto refuse = [&]( const std::string & r ) {
		why = r;
		O.refused++;
		return false;
	};
	if ( !v.ok )
		return refuse( "no present device" );
	SpvModule m[2];
	std::string w;
	if ( !spvReflect( vs, vsN, m[0], w ) || m[0].stage != VK_SHADER_STAGE_VERTEX_BIT )
		return refuse( "the vertex module: " + ( w.empty() ? std::string( "not a vertex stage" ) : w ) );
	if ( !spvReflect( ps, psN, m[1], w ) || m[1].stage != VK_SHADER_STAGE_FRAGMENT_BIT )
		return refuse( "the pixel module: " + ( w.empty() ? std::string( "not a fragment stage" ) : w ) );
	ObjBlPipeInfo in;
	// set 0: one binding, one resource; a binding both stages declare must be the same resource in both
	std::map<std::uint32_t, SpvBind> set0;
	std::vector<SpvBind> set1;
	for ( const SpvModule & s : m )
		for ( const SpvBind & b : s.binds ) {
			if ( b.set == 1 ) {
				if ( s.stage != VK_SHADER_STAGE_FRAGMENT_BIT )
					return refuse( "set 1 in the vertex stage (" + b.name + "): the table is the pixel stage's" );
				set1.push_back( b );
				continue;
			}
			if ( b.set != 0 )
				return refuse( b.name + " is in set " + std::to_string( b.set ) + ": the twins use set 0 and the table" );
			if ( !b.count )
				return refuse( b.name + ": a runtime array in set 0" );
			auto it = set0.find( b.binding );
			if ( it == set0.end() ) {
				set0[b.binding] = b;
				continue;
			}
			if ( it->second.type != b.type || it->second.count != b.count || it->second.name != b.name )
				return refuse( "binding " + std::to_string( b.binding ) + ": " + opStage( it->second.stages ) + "'s " + it->second.name + " and "
					+ opStage( b.stages ) + "'s " + b.name + " (a register shift lands two resources on one binding)" );
			it->second.stages |= b.stages;
		}
	// set 1 = the table: binding 0 the images, binding 1 the samplers, both runtime arrays, nothing else
	bool table = set1.size() == 2;
	for ( const SpvBind & b : set1 )
		table = table && b.count == 0
			&& ( ( b.binding == 0 && b.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ) || ( b.binding == 1 && b.type == VK_DESCRIPTOR_TYPE_SAMPLER ) );
	if ( table && set1[0].binding == set1[1].binding )
		table = false;
	if ( !table )
		return refuse( "set 1 is not the table (binding 0 = the images, binding 1 = the samplers, runtime arrays)" );
	// the varyings: every pixel input is a vertex output of the same shape
	for ( const SpvIo & i : m[1].inputs ) {
		bool found = false;
		for ( const SpvIo & o : m[0].outputs )
			found = found || ( o.location == i.location && o.locs == i.locs && o.comps == i.comps && o.base == i.base );
		if ( !found )
			return refuse( "the pixel stage's " + i.name + " (Location " + std::to_string( i.location ) + ") is no vertex output of its shape" );
		in.varyings++;
	}
	// the targets: Locations 0 .. colorCount - 1, one each. An output past the frame's attachments has no attachment and
	// Vulkan discards its writes (the frame matches GL's draw buffers; fo4_cellbl's SV_Target1 is a constant 0); red 1 is
	// the old strict count
	const std::size_t outs = m[1].outputs.size();
	if ( target.colorCount > 4 || outs < target.colorCount || ( red == 1 && outs != target.colorCount ) )
		return refuse( "the pixel stage writes " + std::to_string( outs ) + " targets, the frame has "
			+ std::to_string( target.colorCount ) );
	std::uint32_t written = 0;
	for ( const SpvIo & o : m[1].outputs ) {
		if ( o.locs != 1 || o.location >= 4 )
			return refuse( "the pixel stage's " + o.name + " is at Location " + std::to_string( o.location ) );
		if ( o.location < target.colorCount )
			written |= 1u << o.location;
		else
			in.dropped++;
	}
	if ( written != ( 1u << target.colorCount ) - 1u )
		return refuse( "the pixel stage writes " + std::to_string( outs ) + " targets, the frame has "
			+ std::to_string( target.colorCount ) + " (a frame Location has no output)" );
	// the attributes: an input in_var_TEXCOORD<k> (locs s) reads GL's locations k .. k + s - 1
	std::vector<VkVertexInputAttributeDescription> va;
	for ( const SpvIo & i : m[0].inputs ) {
		const std::string pre = "in.var.TEXCOORD", pre2 = "in_var_TEXCOORD";
		std::string nm = i.name;
		std::uint32_t k = 0;
		if ( nm.compare( 0, pre.size(), pre ) == 0 || nm.compare( 0, pre2.size(), pre2 ) == 0 )
			nm = nm.substr( pre.size() );
		else
			return refuse( "the vertex input " + i.name + " has no GL location (not a TEXCOORDn)" );
		if ( nm.empty() || nm.find_first_not_of( "0123456789" ) != std::string::npos )
			return refuse( "the vertex input " + i.name + " has no GL location (not a TEXCOORDn)" );
		k = std::uint32_t( std::stoul( nm ) );
		for ( std::uint32_t j = 0; j < i.locs; j++ ) {
			const ObjBlAttr * a = nullptr;
			for ( const ObjBlAttr & x : attrs )
				if ( x.glLocation == k + j )
					a = &x;
			if ( !a )
				return refuse( "no attribute feeds GL location " + std::to_string( k + j ) + " (" + i.name + ")" );
			std::uint32_t base = 0, comps = 0;
			opFormat( VkFormat( a->format ), base, comps );
			if ( !comps )
				return refuse( "GL location " + std::to_string( k + j ) + ": format " + std::to_string( a->format ) + " is not one the builder takes" );
			if ( base != i.base )
				return refuse( "GL location " + std::to_string( k + j ) + ": the format's type is not " + i.name + "'s" );
			bool bound = false;
			for ( const ObjBlBuffer & b : buffers )
				bound = bound || b.binding == a->binding;
			if ( !bound )
				return refuse( "GL location " + std::to_string( k + j ) + ": no buffer at binding " + std::to_string( a->binding ) );
			va.push_back( { i.location + j, a->binding, VkFormat( a->format ), a->offset } );
		}
		in.inputs++;
	}
	VkDescriptorSetLayout tableDsl = VK_NULL_HANDLE;
	VkDescriptorSet tableSet = VK_NULL_HANDLE;
	if ( !blSetGet( tableDsl, tableSet, w ) )
		return refuse( "the table: " + w );
	OpEntry e;
	e.twin = twin;
	e.vs = vs;
	e.ps = ps;
	e.vsN = vsN;
	e.psN = psN;
	e.target = target;
	e.buffers = buffers;
	e.attrs = attrs;
	std::vector<VkDescriptorSetLayoutBinding> lb;
	for ( const auto & kv : set0 ) {
		const SpvBind & b = kv.second;
		lb.push_back( { b.binding, VkDescriptorType( b.type ), b.count, b.stages, nullptr } );
		in.ubos += b.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		in.images += b.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		in.texels += b.type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
		in.samplers += b.type == VK_DESCRIPTOR_TYPE_SAMPLER;
		in.ssbos += b.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		in.vsOnly += b.stages == VK_SHADER_STAGE_VERTEX_BIT;
		in.psOnly += b.stages == VK_SHADER_STAGE_FRAGMENT_BIT;
		in.both += b.stages == ( VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT );
	}
	for ( const auto & kv : set0 )
		e.set0.push_back( kv.second );
	in.set0 = int( lb.size() );
	in.targets = int( target.colorCount );
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = std::uint32_t( lb.size() );
	lci.pBindings = lb.data();
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &e.dsl0 ) != VK_SUCCESS )
		return refuse( "vkCreateDescriptorSetLayout (the twin's set 0) failed" );
	const VkDescriptorSetLayout dsls[2] = { e.dsl0, tableDsl };
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 2;
	plci.pSetLayouts = dsls;
	// the frame: the colour targets and the depth GL's prepass left (LOAD / STORE, GENERAL as the shared targets are)
	std::vector<VkAttachmentDescription> at;
	std::vector<VkAttachmentReference> cr;
	for ( std::uint32_t c = 0; c <= target.colorCount; c++ ) {
		const bool isDepth = c == target.colorCount;
		if ( isDepth && !target.depth )
			break;
		VkAttachmentDescription d {};
		d.format = VkFormat( isDepth ? target.depth : target.colors[c] );
		d.samples = VkSampleCountFlagBits( target.samples );
		d.loadOp = d.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		d.storeOp = d.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
		d.initialLayout = d.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
		at.push_back( d );
		if ( !isDepth )
			cr.push_back( { c, VK_IMAGE_LAYOUT_GENERAL } );
	}
	const VkAttachmentReference dr { target.colorCount, VK_IMAGE_LAYOUT_GENERAL };
	VkSubpassDescription sp {};
	sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sp.colorAttachmentCount = std::uint32_t( cr.size() );
	sp.pColorAttachments = cr.data();
	sp.pDepthStencilAttachment = target.depth ? &dr : nullptr;
	VkRenderPassCreateInfo rci { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	rci.attachmentCount = std::uint32_t( at.size() );
	rci.pAttachments = at.data();
	rci.subpassCount = 1;
	rci.pSubpasses = &sp;
	if ( v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &e.layout ) != VK_SUCCESS || v.vkCreateRenderPass( v.dev, &rci, nullptr, &e.pass ) != VK_SUCCESS ) {
		opDestroy( e );
		return refuse( "vkCreatePipelineLayout / vkCreateRenderPass (the twin's) failed" );
	}
	VkShaderModule mods[2] = {};
	const std::uint32_t * code[2] = { vs, ps };
	const std::size_t words[2] = { vsN, psN };
	VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	bool modsOk = true;
	for ( int s = 0; s < 2; s++ ) {
		VkShaderModuleCreateInfo mci { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		mci.codeSize = words[s] * 4;
		mci.pCode = code[s];
		modsOk = modsOk && v.vkCreateShaderModule( v.dev, &mci, nullptr, &mods[s] ) == VK_SUCCESS;
		st[s].stage = s ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT;
		st[s].module = mods[s];
		st[s].pName = "main";
	}
	std::vector<VkVertexInputBindingDescription> vb;
	for ( const ObjBlBuffer & b : buffers )
		vb.push_back( { b.binding, b.stride, b.perInstance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX } );
	VkPipelineVertexInputStateCreateInfo vi { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vi.vertexBindingDescriptionCount = std::uint32_t( vb.size() );
	vi.pVertexBindingDescriptions = vb.data();
	vi.vertexAttributeDescriptionCount = std::uint32_t( va.size() );
	vi.pVertexAttributeDescriptions = va.data();
	VkPipelineInputAssemblyStateCreateInfo ia { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	// GL's clip z as the prepass takes it (objPipe), GL's CCW front = Vulkan's CW with no y flip
	VkPipelineViewportDepthClipControlCreateInfoEXT cc { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_DEPTH_CLIP_CONTROL_CREATE_INFO_EXT };
	cc.negativeOneToOne = VK_TRUE;
	VkPipelineViewportStateCreateInfo vps { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	vps.pNext = v.clipControl ? &cc : nullptr;
	vps.viewportCount = vps.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo rs { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rs.cullMode = target.twoSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
	rs.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms.rasterizationSamples = VkSampleCountFlagBits( target.samples );
	VkPipelineDepthStencilStateCreateInfo ds { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	ds.depthTestEnable = target.depth ? VK_TRUE : VK_FALSE;
	ds.depthWriteEnable = target.depth && target.depthWrite ? VK_TRUE : VK_FALSE;
	ds.depthCompareOp = VkCompareOp( target.depthOp );
	if ( target.stencil.on ) {  // lane VKOBJ4 open item 3: GL's front is the pipeline's front (CLOCKWISE above, the flip)
		const ObjBlStencil & sg = target.stencil;
		ds.stencilTestEnable = VK_TRUE;
		VkStencilOpState * so[2] = { &ds.front, &ds.back };
		for ( int k = 0; k < 2; k++ ) {
			so[k]->failOp = VkStencilOp( sg.fail[k] );
			so[k]->passOp = VkStencilOp( sg.pass[k] );
			so[k]->depthFailOp = VkStencilOp( sg.depthFail[k] );
			so[k]->compareOp = VkCompareOp( sg.func[k] );
			so[k]->compareMask = sg.readMask[k];
			so[k]->writeMask = sg.writeMask[k];
			so[k]->reference = sg.ref[k];
		}
	}
	std::vector<VkPipelineColorBlendAttachmentState> cba( target.colorCount );
	for ( VkPipelineColorBlendAttachmentState & c : cba ) {
		const ObjBlBlend & b = target.blend;
		c.blendEnable = b.on ? VK_TRUE : VK_FALSE;
		c.srcColorBlendFactor = VkBlendFactor( b.srcColor );
		c.dstColorBlendFactor = VkBlendFactor( b.dstColor );
		c.colorBlendOp = VkBlendOp( b.colorOp );
		c.srcAlphaBlendFactor = VkBlendFactor( b.srcAlpha );
		c.dstAlphaBlendFactor = VkBlendFactor( b.dstAlpha );
		c.alphaBlendOp = VkBlendOp( b.alphaOp );
		c.colorWriteMask = b.writeMask;
	}
	VkPipelineColorBlendStateCreateInfo cbs { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	cbs.attachmentCount = std::uint32_t( cba.size() );
	cbs.pAttachments = cba.data();
	const VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dy { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dy.dynamicStateCount = 2;
	dy.pDynamicStates = dyn;
	VkGraphicsPipelineCreateInfo gp { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	gp.stageCount = 2;
	gp.pStages = st;
	gp.pVertexInputState = &vi;
	gp.pInputAssemblyState = &ia;
	gp.pViewportState = &vps;
	gp.pRasterizationState = &rs;
	gp.pMultisampleState = &ms;
	gp.pDepthStencilState = &ds;
	gp.pColorBlendState = &cbs;
	gp.pDynamicState = &dy;
	gp.layout = e.layout;
	gp.renderPass = e.pass;
	const bool made = modsOk && v.vkCreateGraphicsPipelines( v.dev, tone().cache, 1, &gp, nullptr, &e.pipe ) == VK_SUCCESS;
	for ( VkShaderModule & md : mods )
		if ( md )
			v.vkDestroyShaderModule( v.dev, md, nullptr );
	if ( !made ) {
		opDestroy( e );
		return refuse( "vkCreateGraphicsPipelines (the twin's) failed" );
	}
	toneCacheSave();
	in.ms = nowMs() - t0;
	e.info = in;
	info = in;
	O.made.push_back( e );
	return true;
}

int objBlPipes( int * refused )
{
	if ( refused )
		*refused = opS().refused;
	return int( opS().made.size() );
}

void objBlPoolLayout( std::uint64_t attrMask, std::vector<ObjBlBuffer> & buffers, std::vector<ObjBlAttr> & attrs, std::uint32_t need )
{
	buffers.clear();
	attrs.clear();
	const VkFormat fmt[5] = { VK_FORMAT_UNDEFINED, VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R32G32B32A32_SFLOAT };
	std::uint32_t have = 0;
	std::uint64_t m = attrMask;
	for ( std::uint32_t i = 0; m && i < 16; i++, m >>= 4 ) {
		const std::uint32_t n = std::uint32_t( m & 15 );
		if ( !n || n > 4 )
			continue;
		buffers.push_back( { i, n * 4, false } );
		attrs.push_back( { i, i, std::uint32_t( fmt[n] ), 0 } );
		have |= 1u << i;
	}
	const std::uint32_t miss = need & ~have;
	if ( !miss )
		return;
	buffers.push_back( { kObjBlConstBinding, 0, false } );
	for ( std::uint32_t l = 0; l < 32; l++ )
		if ( miss >> l & 1u )
			attrs.push_back( { l, kObjBlConstBinding, std::uint32_t( VK_FORMAT_R32G32B32A32_SFLOAT ), 0 } );
}

// the twins of class `key` as vertex / index buffers ([16] = the constant (0,0,0,1)); false: no such class or none copied yet
bool objBlPoolBinds( std::uintptr_t key, VkBuffer ( &vb )[17], VkBuffer & ib )
{
	Op & O = opS();
	const auto it = O.pools.find( key );
	if ( it == O.pools.end() || it->second.gone || !it->second.ib.buf )
		return false;
	for ( int i = 0; i < 16; i++ )
		vb[i] = it->second.vb[i].buf;
	vb[16] = O.constVb.buf;
	ib = it->second.ib.buf;
	return true;
}

bool objBlPoolCopy( std::uintptr_t key, const ObjBlPool & p, std::vector<unsigned> & glNames, std::string & why, int red )
{
	Gl & g = gl();
	glNames.clear();
	if ( !vk().ok || !g.ok || !g.copyNamedBufferSubData || !g.namedBufferStorageMem )
		return why = "no present device or no glCopyNamedBufferSubData / glNamedBufferStorageMemEXT", false;
	if ( !p.ebo || !p.indices || !p.verts )
		return why = "an empty pool", false;
	if ( p.attrMask >> 60 )
		return why = "more than 15 attributes", false;
	std::string w;
	CsmBuf & cv = opS().constVb;
	if ( !cv.buf ) {
		if ( !csmBuf( cv, 16, false, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, w ) )
			return why = "the constant attribute buffer: " + w, false;
		const float one[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		std::memcpy( cv.ptr, one, 16 );
	}
	OpPool & t = opS().pools[key];
	t.gone = false;
	t.attrMask = p.attrMask;
	std::uint64_t m = p.attrMask;
	unsigned prev = 0;
	std::size_t prevBytes = 0;
	for ( int i = 0; m && i < 16; i++, m >>= 4 ) {
		const std::size_t n = std::size_t( m & 15 );
		if ( !n )
			continue;
		if ( n > 4 || !p.vbo[i] )
			return why = "attribute " + std::to_string( i ) + ": " + ( n > 4 ? "more than 4 floats" : "no GL buffer" ), false;
		const std::size_t bytes = p.verts * n * 4;
		if ( !csmBuf( t.vb[i], VkDeviceSize( bytes ), true, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, w ) )
			return why = "attribute " + std::to_string( i ) + "'s twin: " + w, false;
		const bool r = red == 1 && prev;
		g.copyNamedBufferSubData( GLuint( r ? prev : p.vbo[i] ), t.vb[i].gl, 0, 0, std::intptr_t( r ? std::min( bytes, prevBytes ) : bytes ) );
		glNames.push_back( t.vb[i].gl );
		prev = p.vbo[i];
		prevBytes = bytes;
	}
	if ( !csmBuf( t.ib, VkDeviceSize( p.indices * 2 ), true, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, w ) )
		return why = "the indices' twin: " + w, false;
	g.copyNamedBufferSubData( GLuint( p.ebo ), t.ib.gl, 0, 0, std::intptr_t( p.indices * 2 ) );
	glNames.push_back( t.ib.gl );
	return true;
}

void objBlPoolDrop( std::uintptr_t key )
{
	auto it = opS().pools.find( key );
	if ( it != opS().pools.end() )
		it->second.gone = true;
}

int objBlPoolCount( std::size_t * bytes )
{
	std::size_t b = 0;
	int n = 0;
	for ( auto & e : opS().pools ) {
		if ( e.second.gone )
			continue;
		n++;
		for ( const CsmBuf & c : e.second.vb )
			b += std::size_t( c.cap );
		b += std::size_t( e.second.ib.cap );
	}
	if ( bytes )
		*bytes = b;
	return n;
}

void objBlPipeFree()
{
	Op & O = opS();
	if ( ( !O.made.empty() || !O.pools.empty() || !O.draws.empty() || !O.stand.empty() || O.zeroTexel.buf || O.fence || O.runFence || O.glDone
		|| !O.smps.empty() || !O.texTwins.empty() || O.smpDsl ) && vk().ok ) {
		Vk & v = vk();
		drain();
		v.vkDeviceWaitIdle( v.dev );
		if ( O.smpStencil )
			v.vkDestroyImageView( v.dev, O.smpStencil, nullptr );
		if ( O.smpDepth )
			v.vkDestroyImageView( v.dev, O.smpDepth, nullptr );
		O.smpDepth = VK_NULL_HANDLE;
		if ( O.smpPipe )
			v.vkDestroyPipeline( v.dev, O.smpPipe, nullptr );
		if ( O.smpLayout )
			v.vkDestroyPipelineLayout( v.dev, O.smpLayout, nullptr );
		if ( O.smpPool )
			v.vkDestroyDescriptorPool( v.dev, O.smpPool, nullptr );
		if ( O.smpDsl )
			v.vkDestroyDescriptorSetLayout( v.dev, O.smpDsl, nullptr );
		O.smpStencil = VK_NULL_HANDLE;
		O.smpPipe = VK_NULL_HANDLE;
		O.smpLayout = VK_NULL_HANDLE;
		O.smpPool = VK_NULL_HANDLE;
		O.smpDsl = VK_NULL_HANDLE;
		O.smpSet = VK_NULL_HANDLE;
		csmBufFree( O.smpOut, false );
		for ( OpDraw & d : O.draws )
			opDrawFree( d );
		O.draws.clear();
		O.live = 0;
		for ( auto & s : O.stand ) {
			if ( s.second.view )
				v.vkDestroyImageView( v.dev, s.second.view, nullptr );
			if ( s.second.img )
				v.vkDestroyImage( v.dev, s.second.img, nullptr );
			if ( s.second.mem )
				v.vkFreeMemory( v.dev, s.second.mem, nullptr );
		}
		O.stand.clear();
		for ( auto & s : O.smps )
			v.vkDestroySampler( v.dev, s.second, nullptr );
		O.smps.clear();
		const auto destroyView = reinterpret_cast<PFN_vkDestroyBufferView>( v.vkGetDeviceProcAddr( v.dev, "vkDestroyBufferView" ) );
		for ( VkBufferView & bv : O.texelView ) {
			if ( bv && destroyView )
				destroyView( v.dev, bv, nullptr );
			bv = VK_NULL_HANDLE;
		}
		for ( auto & t : O.texTwins ) {
			if ( t.second.view && destroyView )
				destroyView( v.dev, t.second.view, nullptr );
			csmBufFree( t.second.buf, gl().ok && t.second.buf.glCtx == gl().ctx );
		}
		O.texTwins.clear();
		csmBufFree( O.zeroTexel, false );
		csmBufFree( O.rows, false );
		O.rowCount = 0;
		if ( O.cmd )
			v.vkFreeCommandBuffers( v.dev, v.pool, 1, &O.cmd );
		if ( O.fence )
			v.vkDestroyFence( v.dev, O.fence, nullptr );
		O.cmd = VK_NULL_HANDLE;
		O.fence = VK_NULL_HANDLE;
		if ( O.runCmd )
			v.vkFreeCommandBuffers( v.dev, v.pool, 1, &O.runCmd );
		if ( O.runFence )
			v.vkDestroyFence( v.dev, O.runFence, nullptr );
		O.runCmd = VK_NULL_HANDLE;
		O.runFence = VK_NULL_HANDLE;
		O.pending = false;
		opRunSemsDrop( gl().ok && O.semCtx == gl().ctx );
		for ( OpEntry & e : O.made )
			opDestroy( e );
		for ( auto & p : O.pools ) {
			for ( CsmBuf & c : p.second.vb )
				csmBufFree( c, gl().ok && c.glCtx == gl().ctx );
			csmBufFree( p.second.ib, gl().ok && p.second.ib.glCtx == gl().ctx );
		}
		O.pools.clear();
		csmBufFree( O.constVb, false );
	}
	O.made.clear();
	O.refused = 0;
}
}  // namespace WwVkPresent

namespace
{
// the GL entry points the mirror reads (GL 4.5 core; looked up per call: the mirror runs on GL's thread, any context)
struct OuGl
{
	int( WINAPI * getUniformLocation )( unsigned, const char * ) = nullptr;
	void( WINAPI * getUniformfv )( unsigned, int, float * ) = nullptr;
	void( WINAPI * getUniformiv )( unsigned, int, int * ) = nullptr;
	void( WINAPI * getUniformuiv )( unsigned, int, unsigned * ) = nullptr;
	unsigned( WINAPI * getUniformBlockIndex )( unsigned, const char * ) = nullptr;
	void( WINAPI * getActiveUniformBlockiv )( unsigned, unsigned, unsigned, int * ) = nullptr;
	void( WINAPI * getUniformIndices )( unsigned, int, const char * const *, unsigned * ) = nullptr;
	void( WINAPI * getActiveUniformsiv )( unsigned, int, const unsigned *, unsigned, int * ) = nullptr;
	void( WINAPI * getIntegeri_v )( unsigned, unsigned, int * ) = nullptr;
	void( WINAPI * getInteger64i_v )( unsigned, unsigned, long long * ) = nullptr;
	void( WINAPI * getNamedBufferSubData )( unsigned, std::intptr_t, std::intptr_t, void * ) = nullptr;
	void( WINAPI * getIntegerv )( unsigned, int * ) = nullptr;
	void( WINAPI * activeTexture )( unsigned ) = nullptr;
};

bool ouGl( WwVkPresent::GlProc gp, OuGl & g, std::string & why )
{
	if ( !gp ) {
		why = "no GL entry points";
		return false;
	}
	auto ld = [&]( auto & f, const char * n ) {
		f = reinterpret_cast<std::remove_reference_t<decltype( f )>>( gp( n ) );
		if ( !f && why.empty() )
			why = std::string( "GL has no " ) + n;
	};
	why.clear();
	ld( g.getUniformLocation, "glGetUniformLocation" );
	ld( g.getUniformfv, "glGetUniformfv" );
	ld( g.getUniformiv, "glGetUniformiv" );
	ld( g.getUniformuiv, "glGetUniformuiv" );
	ld( g.getUniformBlockIndex, "glGetUniformBlockIndex" );
	ld( g.getActiveUniformBlockiv, "glGetActiveUniformBlockiv" );
	ld( g.getUniformIndices, "glGetUniformIndices" );
	ld( g.getActiveUniformsiv, "glGetActiveUniformsiv" );
	ld( g.getIntegeri_v, "glGetIntegeri_v" );
	ld( g.getInteger64i_v, "glGetInteger64i_v" );
	ld( g.getNamedBufferSubData, "glGetNamedBufferSubData" );
	ld( g.getIntegerv, "glGetIntegerv" );
	ld( g.activeTexture, "glActiveTexture" );
	return why.empty();
}

// GL's texture target and its binding query for an image's Dim / Arrayed (0: none)
void ouTarget( std::uint32_t dim, std::uint32_t arrayed, unsigned & target, unsigned & query )
{
	target = query = 0;
	if ( dim == 0 ) {
		target = arrayed ? 0x8C18u : 0x0DE0u;  // TEXTURE_1D_ARRAY / 1D
		query = arrayed ? 0x8C1Cu : 0x8068u;
	} else if ( dim == 1 ) {
		target = arrayed ? 0x8C1Au : 0x0DE1u;  // TEXTURE_2D_ARRAY / 2D
		query = arrayed ? 0x8C1Du : 0x8069u;
	} else if ( dim == 2 ) {
		target = 0x806Fu;  // TEXTURE_3D
		query = 0x806Au;
	} else if ( dim == 3 ) {
		target = arrayed ? 0x9009u : 0x8513u;  // TEXTURE_CUBE_MAP_ARRAY / CUBE_MAP
		query = arrayed ? 0x900Au : 0x8514u;
	} else if ( dim == 5 ) {
		target = 0x8C2Au;  // TEXTURE_BUFFER
		query = 0x8C2Cu;
	}
}

// the location registry (objBlLocsLoad): name -> base location; red 1 shifts every one
struct OuReg
{
	std::map<std::string, int> loc;
	int red = 0;
};
OuReg & ouReg()
{
	static OuReg r;
	return r;
}

// a program's default-block locations GL keeps, and whether it is linked from SPIR-V (a shader attached as SPIR-V, or no
// uniform has a name): the registry's locations hold then
struct OuLocs
{
	bool nameless = false;
	std::vector<int> active;  // sorted
	bool has( int l ) const { return std::binary_search( active.begin(), active.end(), l ); }
	// the registry's location of `name`'s element e when GL keeps it, else -1
	int at( const std::string & name, std::uint32_t e ) const
	{
		if ( !nameless )
			return -1;
		const auto it = ouReg().loc.find( name );
		if ( it == ouReg().loc.end() )
			return -1;
		const int l = it->second + ouReg().red + int( e );
		return has( l ) ? l : -1;
	}
};

OuLocs ouLocs( WwVkPresent::GlProc gp, unsigned prog )
{
	using IfaceIv = void( WINAPI * )( unsigned, unsigned, unsigned, int * );
	using ResIv = void( WINAPI * )( unsigned, unsigned, unsigned, int, const unsigned *, int, int *, int * );
	OuLocs o;
	const auto iv = gp ? reinterpret_cast<IfaceIv>( gp( "glGetProgramInterfaceiv" ) ) : nullptr;
	const auto rv = gp ? reinterpret_cast<ResIv>( gp( "glGetProgramResourceiv" ) ) : nullptr;
	if ( !iv || !rv || ouReg().loc.empty() )
		return o;
	int n = 0, named = 0;
	iv( prog, 0x92E1u, 0x92F5u, &n );  // UNIFORM, ACTIVE_RESOURCES
	for ( int i = 0; i < n; i++ ) {
		const unsigned props[3] = { 0x930Eu, 0x92FBu, 0x92F9u };  // LOCATION, ARRAY_SIZE, NAME_LENGTH
		int v[3] = { -1, 0, 0 };
		rv( prog, 0x92E1u, unsigned( i ), 3, props, 3, nullptr, v );
		named += v[2] > 1;
		for ( int e = 0; v[0] >= 0 && e < std::max( v[1], 1 ); e++ )
			o.active.push_back( v[0] + e );
	}
	std::sort( o.active.begin(), o.active.end() );
	o.nameless = n > 0 && !named;
	/* a shader attached as SPIR-V (SPIR_V_BINARY_ARB): the registry's locations hold whatever names the driver reports
	 * (NV reflects OpName; a uniform both stages hold must keep one name there, or the link fails) */
	using Attached = void( WINAPI * )( unsigned, int, int *, unsigned * );
	using ShaderIv = void( WINAPI * )( unsigned, unsigned, int * );
	const auto att = reinterpret_cast<Attached>( gp( "glGetAttachedShaders" ) );
	const auto shIv = reinterpret_cast<ShaderIv>( gp( "glGetShaderiv" ) );
	unsigned sh[8] = {};
	int nSh = 0;
	if ( att && shIv )
		att( prog, 8, &nSh, sh );
	for ( int i = 0; i < nSh && !o.nameless; i++ ) {
		int spv = 0;
		shIv( sh[i], 0x9552u, &spv );  // GL_SPIR_V_BINARY_ARB
		o.nameless = spv != 0;
	}
	return o;
}

// a SPIR-V-linked program's block by build_spv.py's binding (BLOCKS): its index, or 0xFFFFFFFF
unsigned ouBlockByBinding( WwVkPresent::GlProc gp, unsigned prog, const std::string & name )
{
	using IfaceIv = void( WINAPI * )( unsigned, unsigned, unsigned, int * );
	using ResIv = void( WINAPI * )( unsigned, unsigned, unsigned, int, const unsigned *, int, int *, int * );
	const char * names[3] = { "globalUniforms", "skinningUniforms", "wwShadowFrame" };
	int want = -1;
	for ( int k = 0; k < 3; k++ )
		if ( name.find( names[k] ) != std::string::npos )
			want = k;
	const auto iv = gp ? reinterpret_cast<IfaceIv>( gp( "glGetProgramInterfaceiv" ) ) : nullptr;
	const auto rv = gp ? reinterpret_cast<ResIv>( gp( "glGetProgramResourceiv" ) ) : nullptr;
	if ( want < 0 || !iv || !rv )
		return 0xFFFFFFFFu;
	int n = 0;
	iv( prog, 0x92E2u, 0x92F5u, &n );  // UNIFORM_BLOCK, ACTIVE_RESOURCES
	for ( int i = 0; i < n; i++ ) {
		const unsigned props[2] = { 0x9302u, 0x92F9u };  // BUFFER_BINDING, NAME_LENGTH
		int v[2] = { -1, 0 };
		rv( prog, 0x92E2u, unsigned( i ), 2, props, 2, nullptr, v );
		if ( v[0] == want && v[1] <= 1 )
			return unsigned( i );
	}
	return 0xFFFFFFFFu;
}
}  // namespace

namespace WwVkPresent
{
bool objBlLoose( GlProc gp, unsigned prog, const SpvBind & block, std::vector<std::uint8_t> & out, ObjBlUniStat & st, std::string & why,
	int red )
{
	st = ObjBlUniStat();
	out.assign( block.bytes, 0 );
	OuGl g;
	if ( !ouGl( gp, g, why ) )
		return false;
	const OuLocs al = ouLocs( gp, prog );
	for ( const SpvMember & m : block.members ) {
		st.members++;
		bool viaReg = false;
		if ( m.base > 2 || m.comps > 4 || m.cols > 4 || ( m.cols > 1 && !m.colStride ) || ( m.count > 1 && !m.arrayStride ) ) {
			why = block.name + "." + m.name + ": a member type the mirror does not take";
			return false;
		}
		const std::uint32_t stride = red == 2 ? m.cols * m.comps * 4 : m.arrayStride;
		bool any = false;
		for ( std::uint32_t e = 0; e < m.count; e++ ) {
			const std::string nm = m.count > 1 ? m.name + "[" + std::to_string( e ) + "]" : m.name;
			int loc = g.getUniformLocation( prog, nm.c_str() );
			if ( loc < 0 && ( loc = al.at( m.name, e ) ) >= 0 )
				viaReg = true;  // linked from SPIR-V: no names, the registry's location
			if ( loc < 0 )
				continue;  // GL dropped it (an array's tail too): 0 as GL reads it
			any = true;
			std::uint32_t val[16] = {};
			if ( m.base == 0 )
				g.getUniformfv( prog, loc, reinterpret_cast<float *>( val ) );
			else if ( m.base == 1 )
				g.getUniformiv( prog, loc, reinterpret_cast<int *>( val ) );
			else
				g.getUniformuiv( prog, loc, val );
			// GL returns a matrix column after column: column c row r is val[c * comps + r]
			for ( std::uint32_t c = 0; c < m.cols; c++ )
				for ( std::uint32_t r = 0; r < m.comps; r++ ) {
					const bool across = m.cols > 1 && ( m.rowMajor != ( red == 1 ) );
					const std::uint32_t at = m.offset + e * stride + ( m.cols > 1 ? ( across ? r * m.colStride + c * 4 : c * m.colStride + r * 4 ) : r * 4 );
					if ( at + 4 <= out.size() )
						std::memcpy( out.data() + at, &val[c * m.comps + r], 4 );
				}
		}
		( any ? st.mirrored : st.absent )++;
		st.registry += any && viaReg;
	}
	return true;
}

int objBlLocsLoad( const std::string & file, std::string & why, int red )
{
	OuReg & r = ouReg();
	r.loc.clear();
	r.red = red == 1 ? 1 : 0;
	std::FILE * fp = std::fopen( file.c_str(), "rb" );
	if ( !fp ) {
		why = "no " + file;
		return 0;
	}
	char line[512];
	while ( std::fgets( line, sizeof( line ), fp ) ) {
		char * t1 = std::strchr( line, '\t' );
		char * t2 = t1 ? std::strchr( t1 + 1, '\t' ) : nullptr;
		if ( !t1 || !t2 || t1 == line )
			continue;
		r.loc[std::string( line, std::size_t( t1 - line ) )] = std::atoi( t1 + 1 );
	}
	std::fclose( fp );
	if ( r.loc.empty() )
		why = file + " holds no locations";
	return int( r.loc.size() );
}

bool objBlBlock( GlProc gp, unsigned prog, const SpvBind & block, std::vector<std::uint8_t> & out, std::string & why, int red )
{
	out.clear();
	OuGl g;
	if ( !ouGl( gp, g, why ) )
		return false;
	unsigned idx = g.getUniformBlockIndex( prog, block.name.c_str() );
	if ( idx == 0xFFFFFFFFu )
		idx = ouBlockByBinding( gp, prog, block.name );  // linked from SPIR-V: no block names
	if ( idx == 0xFFFFFFFFu ) {
		why = "GL's program has no block " + block.name;
		return false;
	}
	int bind = -1, size = 0;
	g.getActiveUniformBlockiv( prog, idx, 0x8A3Fu, &bind );  // UNIFORM_BLOCK_BINDING
	g.getActiveUniformBlockiv( prog, idx, 0x8A40u, &size );  // UNIFORM_BLOCK_DATA_SIZE
	if ( std::uint32_t( size ) < block.bytes ) {
		why = block.name + ": GL's block is " + std::to_string( size ) + " bytes, the module's " + std::to_string( block.bytes );
		return false;
	}
	// every member GL keeps sits where the module reads it
	for ( const SpvMember & m : block.members ) {
		const std::string nm = m.count > 1 ? m.name + "[0]" : m.name;
		const char * p = nm.c_str();
		unsigned ui = 0xFFFFFFFFu;
		g.getUniformIndices( prog, 1, &p, &ui );
		if ( ui == 0xFFFFFFFFu )
			continue;
		int off = -1;
		g.getActiveUniformsiv( prog, 1, &ui, 0x8A3Bu, &off );  // UNIFORM_OFFSET
		if ( off != int( m.offset ) ) {
			why = block.name + "." + m.name + ": GL's offset " + std::to_string( off ) + ", the module's " + std::to_string( m.offset );
			return false;
		}
	}
	const unsigned at = unsigned( bind + ( red == 1 ? 1 : 0 ) );
	int buf = 0;
	long long start = 0, range = 0;
	g.getIntegeri_v( 0x8A28u, at, &buf );        // UNIFORM_BUFFER_BINDING
	g.getInteger64i_v( 0x8A29u, at, &start );    // UNIFORM_BUFFER_START
	g.getInteger64i_v( 0x8A2Au, at, &range );    // UNIFORM_BUFFER_SIZE (0: the whole buffer, glBindBufferBase)
	if ( !buf ) {
		why = block.name + ": no buffer at GL's binding " + std::to_string( at );
		return false;
	}
	if ( range && range < block.bytes ) {
		why = block.name + ": GL binds " + std::to_string( range ) + " bytes, the module reads " + std::to_string( block.bytes );
		return false;
	}
	out.assign( block.bytes, 0 );
	g.getNamedBufferSubData( unsigned( buf ), std::intptr_t( start ), std::intptr_t( block.bytes ), out.data() );
	return true;
}

bool objBlTextures( GlProc gp, unsigned prog, const SpvModule & m, std::vector<ObjBlTexUnit> & out, std::string & why )
{
	out.clear();
	OuGl g;
	if ( !ouGl( gp, g, why ) )
		return false;
	std::map<std::string, const SpvBind *> images;
	for ( const SpvBind & b : m.binds )
		if ( b.set == 0 && b.type != VK_DESCRIPTOR_TYPE_SAMPLER && b.type != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && b.type != VK_DESCRIPTOR_TYPE_STORAGE_BUFFER )
			images[b.name] = &b;
	int active = 0;
	g.getIntegerv( 0x84E0u, &active );  // ACTIVE_TEXTURE
	const OuLocs al = ouLocs( gp, prog );
	for ( const SpvBind & b : m.binds ) {
		if ( b.set != 0 || b.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || b.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER )
			continue;
		ObjBlTexUnit u;
		u.binding = b.binding;
		u.type = b.type;
		u.name = b.name;
		const SpvBind * img = &b;
		if ( b.type == VK_DESCRIPTOR_TYPE_SAMPLER ) {
			// spirv-cross names a texture's sampler _<texture>_sampler
			const std::size_t n = b.name.size();
			if ( n > 9 && b.name[0] == '_' && b.name.compare( n - 8, 8, "_sampler" ) == 0 )
				u.name = b.name.substr( 1, n - 9 );
			auto it = images.find( u.name );
			img = it == images.end() ? nullptr : it->second;
		}
		if ( img ) {
			u.dim = img->dim;
			u.arrayed = img->arrayed;
		}
		int loc = g.getUniformLocation( prog, u.name.c_str() );
		if ( loc < 0 )
			loc = al.at( u.name, 0 );  // linked from SPIR-V: the registry's location
		if ( loc >= 0 && img ) {
			g.getUniformiv( prog, loc, &u.unit );
			unsigned query = 0;
			ouTarget( u.dim, u.arrayed, u.glTarget, query );
			if ( query && u.unit >= 0 ) {
				int tex = 0;
				g.activeTexture( 0x84C0u + unsigned( u.unit ) );  // TEXTURE0 + unit
				g.getIntegerv( query, &tex );
				u.glTex = unsigned( tex );
			}
		} else
			u.unit = -1;
		out.push_back( u );
	}
	g.activeTexture( unsigned( active ) );
	return true;
}

bool objBlSamplerRead( GlProc gp, unsigned tex, ObjBlSampler & s, std::string & why )
{
	using GetIv = void( WINAPI * )( unsigned, unsigned, int * );
	using GetFv = void( WINAPI * )( unsigned, unsigned, float * );
	const auto iv = gp ? reinterpret_cast<GetIv>( gp( "glGetTextureParameteriv" ) ) : nullptr;
	const auto fv = gp ? reinterpret_cast<GetFv>( gp( "glGetTextureParameterfv" ) ) : nullptr;
	if ( !iv || !fv || !tex )
		return why = "no glGetTextureParameteriv / fv (GL 4.5) or no texture", false;
	s = ObjBlSampler();
	int i = 0;
	const std::pair<unsigned, unsigned *> ints[] = { { 0x2801u, &s.minFilter }, { 0x2800u, &s.magFilter }, { 0x2802u, &s.wrapS },
		{ 0x2803u, &s.wrapT }, { 0x8072u, &s.wrapR }, { 0x884Cu, &s.compareMode }, { 0x884Du, &s.compareFunc } };
	for ( const auto & p : ints ) {
		iv( tex, p.first, &i );
		*p.second = unsigned( i );
	}
	fv( tex, 0x8501, &s.lodBias );
	fv( tex, 0x813A, &s.minLod );
	fv( tex, 0x813B, &s.maxLod );
	fv( tex, 0x84FE, &s.aniso );
	fv( tex, 0x1004, s.border );
	return true;
}

bool objBlSharedFits( std::uintptr_t key, std::uint32_t dim, std::uint32_t arrayed, std::string & why )
{
	const Shared * sh = sharedFind( key );
	if ( !sh )
		return why = "no shared target", false;
	const int want = dim == 1 && !arrayed ? 0 : dim == 2 ? 1 : dim == 1 && arrayed ? 2 : dim == 3 && arrayed ? 3 : -1;
	static const char * names[4] = { "2D", "3D", "2D array", "cube array" };
	if ( want < 0 )
		return why = "the binding is no 2D, 3D, 2D array or cube array image", false;
	if ( sh->type != want )
		return why = std::string( "the binding takes a " ) + names[want] + " image, the shared target is " + ( sh->type >= 0 && sh->type <= 3 ? names[sh->type] : "?" ), false;
	if ( sh->samples > 1 )
		return why = "the shared target is multisampled", false;
	return true;
}

bool objBlBindUnits( GlProc gp, unsigned prog, ObjBlDraw & d, ObjBlBindStat & st, std::string & why )
{
	const std::uint32_t * words[2] = { d.vs, d.ps };
	const std::size_t counts[2] = { d.vsWords, d.psWords };
	for ( int m = 0; m < 2; m++ ) {
		SpvModule mod;
		if ( !spvReflect( words[m], counts[m], mod, why ) )
			return false;
		std::vector<ObjBlTexUnit> units;
		if ( !objBlTextures( gp, prog, mod, units, why ) )
			return false;
		for ( const ObjBlTexUnit & u : units ) {
			if ( u.type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ) {
				if ( !u.glTex ) {
					st.unbound++;
					continue;
				}
				st.texelGl++;
				std::string w;
				if ( objBlTexelCopy( gp, u.glTex, st.glBufs, w ) ) {
					d.texels.emplace_back( u.binding, u.glTex );
					st.texelShared++;
				} else {
					st.texelRefused++;
					if ( st.texelWhy.empty() )
						st.texelWhy = u.name + ": " + w;
				}
				continue;
			}
			if ( !u.glTex ) {
				st.unbound++;
				continue;
			}
			if ( u.type == VK_DESCRIPTOR_TYPE_SAMPLER ) {
				ObjBlSampler s;
				if ( !objBlSamplerRead( gp, u.glTex, s, why ) )
					return false;
				d.samplers.emplace_back( u.binding, s );
				st.samplers++;
				st.compare += s.compareMode != 0;
				continue;
			}
			std::uintptr_t key = 0;
			for ( const auto & e : shareds() )
				if ( e.second.tex == u.glTex && e.second.glCtx == gl().ctx )
					key = e.first;
			if ( !key ) {
				st.glOwn++;
				continue;
			}
			std::string w;
			if ( !objBlSharedFits( key, u.dim, u.arrayed, w ) ) {
				st.misfit++;
				if ( st.misfitWhy.empty() )
					st.misfitWhy = u.name + ": " + w;
				continue;
			}
			d.images.emplace_back( u.binding, key );
			st.shared++;
		}
	}
	return true;
}
}  // namespace WwVkPresent

namespace
{
// GL's sampler state as a Vulkan sampler (made once per state): filters, wraps, LOD range, anisotropy, border, depth compare
bool opSampler( const WwVkPresent::ObjBlSampler & s, VkSampler & out, std::string & why )
{
	Vk & v = vk();
	Op & O = opS();
	const std::string key( reinterpret_cast<const char *>( &s ), sizeof( s ) );
	if ( const auto it = O.smps.find( key ); it != O.smps.end() )
		return out = it->second, true;
	VkSamplerCreateInfo ci { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	if ( s.magFilter != 0x2600 && s.magFilter != 0x2601 )
		return why = "a magnification filter Vulkan has no twin of (" + std::to_string( s.magFilter ) + ")", false;
	ci.magFilter = s.magFilter == 0x2600 ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
	bool mips = true;
	switch ( s.minFilter ) {
	case 0x2600: ci.minFilter = VK_FILTER_NEAREST; ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST; mips = false; break;
	case 0x2601: ci.minFilter = VK_FILTER_LINEAR; ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST; mips = false; break;
	case 0x2700: ci.minFilter = VK_FILTER_NEAREST; ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST; break;
	case 0x2701: ci.minFilter = VK_FILTER_LINEAR; ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST; break;
	case 0x2702: ci.minFilter = VK_FILTER_NEAREST; ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR; break;
	case 0x2703: ci.minFilter = VK_FILTER_LINEAR; ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR; break;
	default: return why = "a minification filter Vulkan has no twin of (" + std::to_string( s.minFilter ) + ")", false;
	}
	auto wrap = []( unsigned w, VkSamplerAddressMode & m ) {
		switch ( w ) {
		case 0x2901: m = VK_SAMPLER_ADDRESS_MODE_REPEAT; return true;
		case 0x812F: m = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; return true;
		case 0x8370: m = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT; return true;
		case 0x812D: m = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER; return true;
		default: return false;
		}
	};
	if ( !wrap( s.wrapS, ci.addressModeU ) || !wrap( s.wrapT, ci.addressModeV ) || !wrap( s.wrapR, ci.addressModeW ) )
		return why = "a wrap mode Vulkan has no twin of here", false;
	const float * b = s.border;
	if ( b[0] == 0.0f && b[1] == 0.0f && b[2] == 0.0f )
		ci.borderColor = b[3] == 0.0f ? VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK : b[3] == 1.0f ? VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK : VK_BORDER_COLOR_MAX_ENUM;
	else if ( b[0] == 1.0f && b[1] == 1.0f && b[2] == 1.0f && b[3] == 1.0f )
		ci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	else
		ci.borderColor = VK_BORDER_COLOR_MAX_ENUM;
	const bool border = ci.addressModeU == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER || ci.addressModeV == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
		|| ci.addressModeW == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	if ( ci.borderColor == VK_BORDER_COLOR_MAX_ENUM ) {
		if ( border )
			return why = "a border colour other than transparent black, opaque black or opaque white", false;
		ci.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	}
	if ( s.compareMode == 0x884E ) {  // GL_COMPARE_REF_TO_TEXTURE: GL_NEVER + k is VkCompareOp k
		if ( s.compareFunc < 0x0200 || s.compareFunc > 0x0207 )
			return why = "an unknown compare function", false;
		ci.compareEnable = VK_TRUE;
		ci.compareOp = VkCompareOp( s.compareFunc - 0x0200 );
	} else if ( s.compareMode != 0 )
		return why = "an unknown compare mode", false;
	if ( s.maxLod < 0.0f || s.maxLod < s.minLod )
		return why = "a max LOD below 0 or below the min LOD", false;
	ci.mipLodBias = s.lodBias;
	ci.minLod = std::max( s.minLod, 0.0f );
	ci.maxLod = mips ? s.maxLod : std::min( s.maxLod, 0.25f );
	if ( s.aniso > 1.0f ) {
		if ( !v.anisotropy )
			return why = "anisotropic filtering without Vulkan's samplerAnisotropy", false;
		ci.anisotropyEnable = VK_TRUE;
		ci.maxAnisotropy = std::min( s.aniso, v.maxAniso );
	}
	if ( v.vkCreateSampler( v.dev, &ci, nullptr, &out ) != VK_SUCCESS )
		return why = "vkCreateSampler (a set-0 sampler's) failed", false;
	O.smps[key] = out;
	return true;
}

// the 1x1 (0,0,0,1) of an image binding's shape (SPIR-V Dim / Arrayed) and element type, made and cleared once
bool opStandIn( std::uint32_t dim, std::uint32_t arrayed, std::uint32_t sampled, VkImageView & out, std::string & why )
{
	Vk & v = vk();
	Op & O = opS();
	VkImageViewType vt = VK_IMAGE_VIEW_TYPE_2D;
	VkImageType it = VK_IMAGE_TYPE_2D;
	std::uint32_t layers = 1;
	VkImageCreateFlags fl = 0;
	if ( dim == 0 ) {
		vt = arrayed ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
		it = VK_IMAGE_TYPE_1D;
	} else if ( dim == 1 )
		vt = arrayed ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
	else if ( dim == 2 && !arrayed ) {
		vt = VK_IMAGE_VIEW_TYPE_3D;
		it = VK_IMAGE_TYPE_3D;
	} else if ( dim == 3 ) {
		vt = arrayed ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
		layers = 6;
		fl = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	} else
		return why = "an image of Dim " + std::to_string( dim ) + ( arrayed ? " (arrayed)" : "" ) + " has no stand-in", false;
	if ( sampled > 2 )
		return why = "an image element the stand-ins do not take", false;
	const std::uint32_t key = std::uint32_t( vt ) * 4 + sampled;
	const auto found = O.stand.find( key );
	if ( found != O.stand.end() ) {
		out = found->second.view;
		return true;
	}
	const VkFormat fmt = sampled == 1 ? VK_FORMAT_R32G32B32A32_SINT : sampled == 2 ? VK_FORMAT_R32G32B32A32_UINT : VK_FORMAT_R8G8B8A8_UNORM;
	OpStandIn s;
	VkImageCreateInfo ci { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	ci.flags = fl;
	ci.imageType = it;
	ci.format = fmt;
	ci.extent = { 1, 1, 1 };
	ci.mipLevels = 1;
	ci.arrayLayers = layers;
	ci.samples = VK_SAMPLE_COUNT_1_BIT;
	ci.tiling = VK_IMAGE_TILING_OPTIMAL;
	ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	auto fail = [&]( const char * r ) {
		if ( s.view )
			v.vkDestroyImageView( v.dev, s.view, nullptr );
		if ( s.img )
			v.vkDestroyImage( v.dev, s.img, nullptr );
		if ( s.mem )
			v.vkFreeMemory( v.dev, s.mem, nullptr );
		why = r;
		return false;
	};
	if ( v.vkCreateImage( v.dev, &ci, nullptr, &s.img ) != VK_SUCCESS )
		return fail( "a stand-in image failed" );
	VkMemoryRequirements r;
	v.vkGetImageMemoryRequirements( v.dev, s.img, &r );
	VkMemoryAllocateInfo ai { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	ai.allocationSize = r.size;
	ai.memoryTypeIndex = memType( r.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( ai.memoryTypeIndex == UINT32_MAX || v.vkAllocateMemory( v.dev, &ai, nullptr, &s.mem ) != VK_SUCCESS
		|| v.vkBindImageMemory( v.dev, s.img, s.mem, 0 ) != VK_SUCCESS )
		return fail( "a stand-in image's memory failed" );
	VkImageViewCreateInfo vci { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	vci.image = s.img;
	vci.viewType = vt;
	vci.format = fmt;
	vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, layers };
	if ( v.vkCreateImageView( v.dev, &vci, nullptr, &s.view ) != VK_SUCCESS )
		return fail( "a stand-in image's view failed" );
	if ( !O.fence ) {
		VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		cai.commandPool = v.pool;
		cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cai.commandBufferCount = 1;
		if ( v.vkCreateFence( v.dev, &fi, nullptr, &O.fence ) != VK_SUCCESS || v.vkAllocateCommandBuffers( v.dev, &cai, &O.cmd ) != VK_SUCCESS )
			return fail( "the stand-ins' command buffer failed" );
	}
	drain();  // the queue is the worker's
	v.vkResetCommandBuffer( O.cmd, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( O.cmd, &bi );
	VkImageMemoryBarrier ib { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	ib.srcQueueFamilyIndex = ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	ib.image = s.img;
	ib.subresourceRange = vci.subresourceRange;
	ib.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ib.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	ib.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	v.vkCmdPipelineBarrier( O.cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &ib );
	VkClearColorValue one {};
	if ( sampled )
		one.uint32[3] = 1u;
	else
		one.float32[3] = 1.0f;
	v.vkCmdClearColorImage( O.cmd, s.img, VK_IMAGE_LAYOUT_GENERAL, &one, 1, &ib.subresourceRange );
	ib.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	ib.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	ib.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	v.vkCmdPipelineBarrier( O.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &ib );
	if ( v.vkEndCommandBuffer( O.cmd ) != VK_SUCCESS || !oneShot( O.cmd, O.fence ) )
		return fail( "a stand-in image's clear failed" );
	O.stand[key] = s;
	out = s.view;
	return true;
}

// one zero texel of the element type (GL: a buffer texture with no buffer reads 0)
bool opZeroTexel( std::uint32_t sampled, VkBufferView & out, std::string & why )
{
	Vk & v = vk();
	Op & O = opS();
	if ( sampled > 2 )
		return why = "a texel buffer element the stand-ins do not take", false;
	if ( !O.zeroTexel.buf ) {
		if ( !csmBuf( O.zeroTexel, 16, false, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, why ) )
			return false;
		std::memset( O.zeroTexel.ptr, 0, 16 );
	}
	if ( !O.texelView[sampled] ) {
		const auto create = reinterpret_cast<PFN_vkCreateBufferView>( v.vkGetDeviceProcAddr( v.dev, "vkCreateBufferView" ) );
		VkBufferViewCreateInfo ci { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
		ci.buffer = O.zeroTexel.buf;
		ci.format = sampled == 1 ? VK_FORMAT_R32G32B32A32_SINT : sampled == 2 ? VK_FORMAT_R32G32B32A32_UINT : VK_FORMAT_R32G32B32A32_SFLOAT;
		ci.range = 16;
		if ( !create || create( v.dev, &ci, nullptr, &O.texelView[sampled] ) != VK_SUCCESS )
			return why = "the zero texel's view failed", false;
	}
	out = O.texelView[sampled];
	return true;
}

// GL buffer texture `tex`'s twin as a view of the binding's element class (objBlTexelCopy first; remade when the twin moved)
bool opTexelView( unsigned tex, std::uint32_t sampled, VkBufferView & out, std::string & why )
{
	Vk & v = vk();
	const auto it = opS().texTwins.find( tex );
	if ( it == opS().texTwins.end() || !it->second.buf.buf || !it->second.range )
		return why = "GL's buffer texture " + std::to_string( tex ) + " has no twin (objBlTexelCopy first)", false;
	OpTexel & t = it->second;
	static const char * cls[3] = { "float", "int", "uint" };
	if ( t.cls != sampled )
		return why = std::string( "GL's buffer texture holds " ) + cls[t.cls % 3] + " texels, the binding reads " + ( sampled < 3 ? cls[sampled] : "another class" ), false;
	if ( !t.view || t.viewBuf != t.buf.buf || t.viewFmt != t.fmt || t.viewRange != t.range ) {
		const auto create = reinterpret_cast<PFN_vkCreateBufferView>( v.vkGetDeviceProcAddr( v.dev, "vkCreateBufferView" ) );
		const auto destroy = reinterpret_cast<PFN_vkDestroyBufferView>( v.vkGetDeviceProcAddr( v.dev, "vkDestroyBufferView" ) );
		if ( t.view && destroy )
			destroy( v.dev, t.view, nullptr );
		t.view = VK_NULL_HANDLE;
		VkBufferViewCreateInfo ci { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
		ci.buffer = t.buf.buf;
		ci.format = t.fmt;
		ci.range = t.range;
		if ( !create || create( v.dev, &ci, nullptr, &t.view ) != VK_SUCCESS )
			return why = "the texel twin's view failed", false;
		t.viewBuf = t.buf.buf;
		t.viewFmt = t.fmt;
		t.viewRange = t.range;
	}
	out = t.view;
	return true;
}

// a host buffer at least `need` bytes (growing: the device idle first, the old one may still be read)
bool opHostBuf( CsmBuf & b, std::size_t need, VkBufferUsageFlags usage, std::string & why )
{
	if ( b.buf && b.cap < VkDeviceSize( need ) ) {
		drain();
		vk().vkDeviceWaitIdle( vk().dev );
	}
	return csmBuf( b, VkDeviceSize( std::max<std::size_t>( need, 16 ) ), false, usage, why );
}

/* the prepared draws into cmd (the worker's, inside its submit after GL's signal): per draw its render pass on the
 * targets (LOAD / STORE), the pipeline, {set 0, the table}, the class's twins, its commands. mode 0: one
 * vkCmdDrawIndexedIndirect for up to 65535 commands (multiDrawIndirect), 1: one indirect per command, 2: direct draws from
 * the host copy. Without drawIndirectFirstInstance every mode draws directly (the base instance is the row). */
bool objBlDrawRecord( VkCommandBuffer cmd, int mode, std::string & why )
{
	Vk & v = vk();
	Op & O = opS();
	VkDescriptorSetLayout tdsl = VK_NULL_HANDLE;
	VkDescriptorSet table = VK_NULL_HANDLE;
	if ( !blSetGet( tdsl, table, why ) )
		return false;
	for ( std::size_t i = 0; i < O.live; i++ ) {
		const OpDraw & d = O.draws[i];
		if ( d.entry >= O.made.size() )
			return why = "a draw's pipeline went", false;
		const OpEntry & e = O.made[d.entry];
		VkBuffer vb[17] = {}, ib = VK_NULL_HANDLE;
		if ( !WwVkPresent::objBlPoolBinds( d.poolKey, vb, ib ) )
			return why = "a draw's class has no twins", false;
		// the draw before wrote the same targets: its writes before this pass's tests and writes
		VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		mb.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		mb.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
			| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		const VkPipelineStageFlags att = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
			| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		if ( i )
			v.vkCmdPipelineBarrier( cmd, att, att, 0, 1, &mb, 0, nullptr, 0, nullptr );
		VkRenderPassBeginInfo rbi { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		rbi.renderPass = e.pass;
		rbi.framebuffer = d.fb;
		rbi.renderArea = { { 0, 0 }, { std::uint32_t( d.w ), std::uint32_t( d.h ) } };
		v.vkCmdBeginRenderPass( cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE );
		const VkViewport vp { 0.0f, 0.0f, float( d.w ), float( d.h ), 0.0f, 1.0f };
		const VkRect2D sc = d.scissor;
		v.vkCmdSetViewport( cmd, 0, 1, &vp );
		v.vkCmdSetScissor( cmd, 0, 1, &sc );
		v.vkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, e.pipe );
		const VkDescriptorSet sets[2] = { d.dset, table };
		v.vkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, e.layout, 0, 2, sets, 0, nullptr );
		for ( const WwVkPresent::ObjBlBuffer & b : e.buffers ) {
			const VkDeviceSize zero = 0;
			v.vkCmdBindVertexBuffers( cmd, b.binding, 1, &vb[std::min<std::uint32_t>( b.binding, 16 )], &zero );
		}
		v.vkCmdBindIndexBuffer( cmd, ib, 0, VK_INDEX_TYPE_UINT16 );
		const int m = !v.firstInstance ? 2 : mode == 0 && !v.multiDrawIndirect ? 1 : mode;
		if ( m == 0 ) {
			for ( std::size_t at = 0; at < d.cmdCount; at += 65535 )
				v.vkCmdDrawIndexedIndirect( cmd, d.cmds.buf, VkDeviceSize( at * 20 ), std::uint32_t( std::min<std::size_t>( 65535, d.cmdCount - at ) ), 20 );
		} else if ( m == 1 ) {
			for ( std::size_t c = 0; c < d.cmdCount; c++ )
				v.vkCmdDrawIndexedIndirect( cmd, d.cmds.buf, VkDeviceSize( c * 20 ), 1, 20 );
		} else {
			const auto drawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>( v.vkGetDeviceProcAddr( v.dev, "vkCmdDrawIndexed" ) );
			if ( !drawIndexed ) {
				v.vkCmdEndRenderPass( cmd );
				return why = "no vkCmdDrawIndexed", false;
			}
			for ( std::size_t c = 0; c < d.cmdCount; c++ ) {
				std::uint32_t k[5];
				std::memcpy( k, d.host.data() + c * 20, 20 );
				drawIndexed( cmd, k[0], k[1], k[2], std::int32_t( k[3] ), k[4] );
			}
		}
		v.vkCmdEndRenderPass( cmd );
	}
	return true;
}

// the crossing's last submit done (its command buffer, the slots' host buffers and the rows are free again)
void opRunWait()
{
	Op & O = opS();
	if ( !O.pending )
		return;
	vk().vkWaitForFences( vk().dev, 1, &O.runFence, VK_TRUE, 5000000000ull );
	O.pending = false;
}

// the worker's half of objBlRun: the prepared draws recorded, submitted after GL's signal, ours signalled back
bool opRunWork( std::string & why )
{
	Vk & v = vk();
	Op & O = opS();
	v.vkResetFences( v.dev, 1, &O.runFence );
	v.vkResetCommandBuffer( O.runCmd, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( O.runCmd, &bi );
	if ( !objBlDrawRecord( O.runCmd, O.runMode, why ) ) {
		v.vkEndCommandBuffer( O.runCmd );
		return false;
	}
	if ( v.vkEndCommandBuffer( O.runCmd ) != VK_SUCCESS )
		return why = "vkEndCommandBuffer (the shaded objects) failed", false;
	const VkPipelineStageFlags waitAt = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	si.waitSemaphoreCount = 1;
	si.pWaitSemaphores = &O.glDone;
	si.pWaitDstStageMask = &waitAt;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &O.runCmd;
	si.signalSemaphoreCount = 1;
	si.pSignalSemaphores = &O.vkDone;
	const VkResult r = v.vkQueueSubmit( v.queue, 1, &si, O.runFence );
	if ( r != VK_SUCCESS )
		return why = "vkQueueSubmit (the shaded objects) returned " + std::to_string( int( r ) ), false;
	return true;
}

// the crossing's fence, command buffer and semaphores (this GL context's), made once
bool opRunMake( std::string & why )
{
	Vk & v = vk();
	Gl & g = gl();
	Op & O = opS();
	if ( !O.runFence ) {
		VkFenceCreateInfo fi { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		VkCommandBufferAllocateInfo cai { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		cai.commandPool = v.pool;
		cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cai.commandBufferCount = 1;
		if ( v.vkCreateFence( v.dev, &fi, nullptr, &O.runFence ) != VK_SUCCESS || v.vkAllocateCommandBuffers( v.dev, &cai, &O.runCmd ) != VK_SUCCESS )
			return why = "the crossing's fence / command buffer failed", false;
	}
	if ( !O.glDone || O.semCtx != g.ctx ) {
		opRunSemsDrop( false );  // another context's names: that context went
		if ( !exportSem( O.glDone, O.glDoneGl, why ) || !exportSem( O.vkDone, O.vkDoneGl, why ) ) {
			opRunSemsDrop( true );
			return false;
		}
		O.semCtx = g.ctx;
	}
	return true;
}

// the readback's pipeline: objbl_samples_cs.hlsl (0-2 the frame's colour, depth, stencil; 3 the samples; the job pushed)
bool opSmpMake( std::string & why )
{
	Vk & v = vk();
	Op & O = opS();
	if ( O.smpPipe )
		return true;
	if ( !v.vkCmdDispatch || !v.vkCreateComputePipelines )
		return why = "no vkCmdDispatch / vkCreateComputePipelines", false;
	VkDescriptorSetLayoutBinding b[4] = {};
	for ( std::uint32_t k = 0; k < 4; k++ ) {
		b[k].binding = k;
		b[k].descriptorType = k < 3 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		b[k].descriptorCount = 1;
		b[k].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}
	VkDescriptorSetLayoutCreateInfo lci { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	lci.bindingCount = 4;
	lci.pBindings = b;
	const VkDescriptorPoolSize ps[2] = { { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 3 }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 } };
	VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pci.maxSets = 1;
	pci.poolSizeCount = 2;
	pci.pPoolSizes = ps;
	if ( v.vkCreateDescriptorSetLayout( v.dev, &lci, nullptr, &O.smpDsl ) != VK_SUCCESS
		|| v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &O.smpPool ) != VK_SUCCESS )
		return why = "the readback's descriptor set layout / pool failed", false;
	VkDescriptorSetAllocateInfo dai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	dai.descriptorPool = O.smpPool;
	dai.descriptorSetCount = 1;
	dai.pSetLayouts = &O.smpDsl;
	const VkPushConstantRange pc { VK_SHADER_STAGE_COMPUTE_BIT, 0, std::uint32_t( sizeof( O.smpJob ) ) };
	VkPipelineLayoutCreateInfo plci { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &O.smpDsl;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pc;
	if ( v.vkAllocateDescriptorSets( v.dev, &dai, &O.smpSet ) != VK_SUCCESS
		|| v.vkCreatePipelineLayout( v.dev, &plci, nullptr, &O.smpLayout ) != VK_SUCCESS )
		return why = "the readback's descriptor set / pipeline layout failed", false;
	VkShaderModuleCreateInfo mci { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	mci.codeSize = sizeof( vkframe_objbl_samples_cs_spv );
	mci.pCode = vkframe_objbl_samples_cs_spv;
	VkShaderModule cs = VK_NULL_HANDLE;
	if ( v.vkCreateShaderModule( v.dev, &mci, nullptr, &cs ) != VK_SUCCESS )
		return why = "the readback's shader module failed", false;
	VkComputePipelineCreateInfo cp { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	cp.stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	cp.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	cp.stage.module = cs;
	cp.stage.pName = "main";
	cp.layout = O.smpLayout;
	const bool ok = v.vkCreateComputePipelines( v.dev, VK_NULL_HANDLE, 1, &cp, nullptr, &O.smpPipe ) == VK_SUCCESS;
	v.vkDestroyShaderModule( v.dev, cs, nullptr );
	if ( !ok )
		return why = "vkCreateComputePipelines (the readback) failed", false;
	return true;
}

// the worker's half of objBlFrameSamples: the readback recorded, submitted after GL's signal, ours signalled back
bool opSmpWork( std::string & why )
{
	Vk & v = vk();
	Op & O = opS();
	v.vkResetFences( v.dev, 1, &O.runFence );
	v.vkResetCommandBuffer( O.runCmd, 0 );
	VkCommandBufferBeginInfo bi { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	v.vkBeginCommandBuffer( O.runCmd, &bi );
	v.vkCmdBindPipeline( O.runCmd, VK_PIPELINE_BIND_POINT_COMPUTE, O.smpPipe );
	v.vkCmdBindDescriptorSets( O.runCmd, VK_PIPELINE_BIND_POINT_COMPUTE, O.smpLayout, 0, 1, &O.smpSet, 0, nullptr );
	v.vkCmdPushConstants( O.runCmd, O.smpLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( O.smpJob ), O.smpJob );
	v.vkCmdDispatch( O.runCmd, ( O.smpJob[0] + 7 ) / 8, ( O.smpJob[1] + 7 ) / 8, 1 );
	VkMemoryBarrier mb { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	v.vkCmdPipelineBarrier( O.runCmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr );
	if ( v.vkEndCommandBuffer( O.runCmd ) != VK_SUCCESS )
		return why = "vkEndCommandBuffer (the readback) failed", false;
	const VkPipelineStageFlags waitAt = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	VkSubmitInfo si { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	si.waitSemaphoreCount = 1;
	si.pWaitSemaphores = &O.glDone;
	si.pWaitDstStageMask = &waitAt;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &O.runCmd;
	si.signalSemaphoreCount = 1;
	si.pSignalSemaphores = &O.vkDone;
	const VkResult r = v.vkQueueSubmit( v.queue, 1, &si, O.runFence );
	if ( r != VK_SUCCESS )
		return why = "vkQueueSubmit (the readback) returned " + std::to_string( int( r ) ), false;
	return true;
}

// a job handed to the worker (k = -1), waited for; its result
bool opWorkerRun( bool ( *fn )( std::string & ), std::string & why )
{
	Worker & W = wk();
	std::uint64_t want = 0;
	{
		std::lock_guard<std::mutex> lk( W.m );
		want = W.toneDone + 1;
	}
	Work j;
	j.k = -1;
	j.fn = fn;
	hand( j );
	std::unique_lock<std::mutex> lk( W.m );
	W.idle.wait( lk, [&W, want]() { return W.toneDone >= want; } );
	why = W.toneWhy;
	return W.toneOk;
}
}  // namespace

namespace WwVkPresent
{
bool objBlMirror( GlProc gp, unsigned prog, const std::uint32_t * vs, std::size_t vsWords, const std::uint32_t * ps, std::size_t psWords,
	std::vector<std::pair<std::uint32_t, std::vector<std::uint8_t>>> & ubos, ObjBlUniStat & st, std::string & why )
{
	ubos.clear();
	st = ObjBlUniStat();
	const std::uint32_t * w[2] = { vs, ps };
	const std::size_t n[2] = { vsWords, psWords };
	for ( int s = 0; s < 2; s++ ) {
		SpvModule m;
		if ( !spvReflect( w[s], n[s], m, why ) )
			return false;
		for ( const SpvBind & b : m.binds ) {
			if ( b.type != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || b.set != 0 )
				continue;
			std::vector<std::uint8_t> bytes;
			if ( b.name == "LooseUniforms" || b.name == "type.LooseUniforms" ) {
				ObjBlUniStat one;
				if ( !objBlLoose( gp, prog, b, bytes, one, why ) )
					return false;
				st.members += one.members;
				st.mirrored += one.mirrored;
				st.absent += one.absent;
				st.registry += one.registry;
			} else {
				SpvBind named = b;
				if ( named.name.compare( 0, 5, "type." ) == 0 )
					named.name = named.name.substr( 5 );
				if ( !objBlBlock( gp, prog, named, bytes, why ) )
					return false;
			}
			ubos.emplace_back( b.binding, std::move( bytes ) );
		}
	}
	return true;
}

void objBlDrawsReset()
{
	opRunWait();
	opS().live = 0;
	opS().rowCount = 0;
}

bool objBlRun( const std::vector<unsigned> & glBufs, int mode, std::string & why )
{
	Vk & v = vk();
	Gl & g = gl();
	Op & O = opS();
	const double t0 = nowMs();
	auto refuse = [&]( const std::string & w ) {
		O.st.fails++;
		O.st.failWhy = w;
		return why = w, false;
	};
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return refuse( "the present device or GL's entry points are not ready in this context" );
	if ( !g.signalSemaphore || !g.waitSemaphore )
		return refuse( "no glSignalSemaphoreEXT / glWaitSemaphoreEXT" );
	if ( !O.live )
		return true;  // nothing prepared: nothing crosses
	if ( mode < 0 || mode > 2 )
		return refuse( "mode " + std::to_string( mode ) );
	opRunWait();
	std::string w;
	if ( !opRunMake( w ) )
		return refuse( w );
	// the textures: every shared target a draw writes or reads, once
	std::vector<GLuint> texs;
	for ( std::size_t i = 0; i < O.live; i++ )
		for ( std::uintptr_t k : O.draws[i].keys ) {
			const Shared * sh = sharedFind( k );
			if ( !sh )
				return refuse( "a draw's shared target went before the crossing" );
			if ( std::find( texs.begin(), texs.end(), sh->tex ) == texs.end() )
				texs.push_back( sh->tex );
		}
	std::vector<GLuint> bufs( glBufs.begin(), glBufs.end() );
	std::vector<GLenum> lay( texs.size(), kLayoutGeneral );
	O.runMode = mode;
	g.signalSemaphore( O.glDoneGl, GLuint( bufs.size() ), bufs.data(), GLuint( texs.size() ), texs.data(), lay.data() );
	g.flush();
	if ( !opWorkerRun( opRunWork, w ) )
		return refuse( w );  // nothing signals vkDone: GL does not wait it
	g.waitSemaphore( O.vkDoneGl, GLuint( bufs.size() ), bufs.data(), GLuint( texs.size() ), texs.data(), lay.data() );
	O.pending = true;
	const double ms = nowMs() - t0;
	O.st.runs++;
	O.st.draws += int( O.live );
	for ( std::size_t i = 0; i < O.live; i++ )
		O.st.cmds += int( O.draws[i].cmdCount );
	O.st.cpuMs += ms;
	O.st.maxMs = std::max( O.st.maxMs, ms );
	return true;
}

void objBlOnGl( const char * why )
{
	opS().st.gl++;
	opS().st.glWhy = why ? why : "";
}

ObjBlRunStat takeObjBlRunStat()
{
	ObjBlRunStat s = opS().st;
	opS().st = ObjBlRunStat();
	return s;
}

bool objBlTargetState( GlProc gp, std::uint32_t colorCount, ObjBlTarget & t, std::string & why, int red )
{
	using GetIv = void( WINAPI * )( GLenum, GLint * );
	using GetIiv = void( WINAPI * )( GLenum, GLuint, GLint * );
	using IsOn = GLubyte( WINAPI * )( GLenum );
	using IsOni = GLubyte( WINAPI * )( GLenum, GLuint );
	using GetBv = void( WINAPI * )( GLenum, GLubyte * );
	using GetBiv = void( WINAPI * )( GLenum, GLuint, GLubyte * );
	const auto getIv = reinterpret_cast<GetIv>( gp( "glGetIntegerv" ) );
	const auto getIiv = reinterpret_cast<GetIiv>( gp( "glGetIntegeri_v" ) );
	const auto isOn = reinterpret_cast<IsOn>( gp( "glIsEnabled" ) );
	const auto isOni = reinterpret_cast<IsOni>( gp( "glIsEnabledi" ) );
	const auto getBv = reinterpret_cast<GetBv>( gp( "glGetBooleanv" ) );
	const auto getBiv = reinterpret_cast<GetBiv>( gp( "glGetBooleani_v" ) );
	if ( !getIv || !getIiv || !isOn || !isOni || !getBv || !getBiv )
		return why = "GL's state queries are not loaded", false;
	if ( !isOn( 0x0B71 ) )  // GL_DEPTH_TEST
		return why = "depth test is off", false;
	GLint func = 0, cullMode = 0, front = 0;
	GLubyte dm = 0;
	getIv( 0x0B74, &func );  // GL_DEPTH_FUNC: GL_NEVER + k is VkCompareOp k
	if ( func < 0x0200 || func > 0x0207 )
		return why = "an unknown depth function", false;
	getBv( 0x0B72, &dm );  // GL_DEPTH_WRITEMASK
	t.depthOp = std::uint32_t( func - 0x0200 );
	t.depthWrite = dm != 0;
	const bool cull = isOn( 0x0B44 ) != 0;  // GL_CULL_FACE
	getIv( 0x0B45, &cullMode );
	getIv( 0x0B46, &front );
	if ( cull && ( cullMode != 0x0405 || front != 0x0901 ) )  // GL_BACK, GL_CCW
		return why = "culling other than back faces with CCW front", false;
	t.twoSided = !cull;
	// the stencil test, front and back (GL's function and op enums as Vulkan's)
	t.stencil = ObjBlStencil();
	if ( isOn( 0x0B90 ) && red != 2 ) {  // GL_STENCIL_TEST
		auto sop = []( GLint g, std::uint32_t & v ) {
			switch ( g ) {
			case 0x1E00: v = 0; return true;  // KEEP
			case 0: v = 1; return true;       // ZERO
			case 0x1E01: v = 2; return true;  // REPLACE
			case 0x1E02: v = 3; return true;  // INCR
			case 0x1E03: v = 4; return true;  // DECR
			case 0x150A: v = 5; return true;  // INVERT
			case 0x8507: v = 6; return true;  // INCR_WRAP
			case 0x8508: v = 7; return true;  // DECR_WRAP
			default: return false;
			}
		};
		// FUNC, REF, VALUE_MASK, WRITEMASK, FAIL, PASS_DEPTH_FAIL, PASS_DEPTH_PASS; then the BACK_ ones
		static const GLenum q[2][7] = { { 0x0B92, 0x0B97, 0x0B93, 0x0B98, 0x0B94, 0x0B95, 0x0B96 },
			{ 0x8800, 0x8CA3, 0x8CA4, 0x8CA5, 0x8801, 0x8802, 0x8803 } };
		ObjBlStencil & sg = t.stencil;
		sg.on = 1;
		for ( int k = 0; k < 2; k++ ) {
			GLint v[7] = {};
			for ( int j = 0; j < 7; j++ )
				getIv( q[k][j], &v[j] );
			if ( v[0] < 0x0200 || v[0] > 0x0207 )
				return why = "an unknown stencil function", false;
			if ( !sop( v[4], sg.fail[k] ) || !sop( v[5], sg.depthFail[k] ) || !sop( v[6], sg.pass[k] ) )
				return why = "an unknown stencil op", false;
			sg.func[k] = std::uint32_t( v[0] - 0x0200 );
			sg.ref[k] = std::uint32_t( std::min( std::max( v[1], 0 ), 255 ) );  // GL clamps to the 8 stencil bits
			sg.readMask[k] = std::uint32_t( v[2] ) & 0xFFu;
			sg.writeMask[k] = std::uint32_t( v[3] ) & 0xFFu;
		}
	}
	// GL's factor / equation enums as Vulkan's (the constant factors refused: the pipeline has no blend constants)
	auto factor = []( GLint g, std::uint32_t & v ) {
		switch ( g ) {
		case 0: v = 0; return true;        // ZERO
		case 1: v = 1; return true;        // ONE
		case 0x0300: v = 2; return true;   // SRC_COLOR
		case 0x0301: v = 3; return true;
		case 0x0306: v = 4; return true;   // DST_COLOR
		case 0x0307: v = 5; return true;
		case 0x0302: v = 6; return true;   // SRC_ALPHA
		case 0x0303: v = 7; return true;
		case 0x0304: v = 8; return true;   // DST_ALPHA
		case 0x0305: v = 9; return true;
		case 0x0308: v = 14; return true;  // SRC_ALPHA_SATURATE
		default: return false;
		}
	};
	auto op = []( GLint g, std::uint32_t & v ) {
		switch ( g ) {
		case 0x8006: v = 0; return true;  // FUNC_ADD
		case 0x800A: v = 1; return true;  // FUNC_SUBTRACT
		case 0x800B: v = 2; return true;  // FUNC_REVERSE_SUBTRACT
		case 0x8007: v = 3; return true;  // MIN
		case 0x8008: v = 4; return true;  // MAX
		default: return false;
		}
	};
	t.blend = ObjBlBlend();
	for ( std::uint32_t c = 0; c < colorCount; c++ ) {
		ObjBlBlend b;
		GLint sc = 1, dc = 0, sa = 1, da = 0, ec = 0x8006, ea = 0x8006;
		b.on = isOni( 0x0BE2, c ) ? 1u : 0u;  // GL_BLEND
		getIiv( 0x80C9, c, &sc );  // GL_BLEND_SRC_RGB
		getIiv( 0x80C8, c, &dc );  // GL_BLEND_DST_RGB
		getIiv( 0x80CB, c, &sa );  // GL_BLEND_SRC_ALPHA
		getIiv( 0x80CA, c, &da );  // GL_BLEND_DST_ALPHA
		getIiv( 0x8009, c, &ec );  // GL_BLEND_EQUATION_RGB
		getIiv( 0x883D, c, &ea );  // GL_BLEND_EQUATION_ALPHA
		if ( red == 1 ) {
			std::swap( sc, dc );
			std::swap( sa, da );
		}
		GLubyte cm[4] = {};
		getBiv( 0x0C23, c, cm );  // GL_COLOR_WRITEMASK
		b.writeMask = ( cm[0] ? 1u : 0u ) | ( cm[1] ? 2u : 0u ) | ( cm[2] ? 4u : 0u ) | ( cm[3] ? 8u : 0u );
		if ( b.on ) {
			if ( !factor( sc, b.srcColor ) || !factor( dc, b.dstColor ) || !factor( sa, b.srcAlpha ) || !factor( da, b.dstAlpha ) )
				return why = "draw buffer " + std::to_string( c ) + " blends by a constant or unknown factor", false;
			if ( !op( ec, b.colorOp ) || !op( ea, b.alphaOp ) )
				return why = "draw buffer " + std::to_string( c ) + " blends by an unknown equation", false;
		}
		if ( c && std::memcmp( &b, &t.blend, sizeof( b ) ) )
			return why = "draw buffer " + std::to_string( c ) + " blends or masks other than draw buffer 0", false;
		t.blend = b;
	}
	return true;
}

bool objBlFrameRead( GlProc gp, ObjBlDraw & d, std::string & why, int red )
{
	auto fail = [&]( const std::string & w ) {
		return why = w, false;
	};
	using GetIv = void( WINAPI * )( GLenum, GLint * );
	using GetAtt = void( WINAPI * )( GLenum, GLenum, GLenum, GLint * );
	const auto getIv = reinterpret_cast<GetIv>( gp( "glGetIntegerv" ) );
	const auto getAtt = reinterpret_cast<GetAtt>( gp( "glGetFramebufferAttachmentParameteriv" ) );
	if ( !getIv || !getAtt )
		return fail( "GL's state queries are not loaded" );
	// the frame: GL's draw framebuffer, every target a shared one
	GLint fbo = 0;
	getIv( 0x8CA6, &fbo );  // GL_DRAW_FRAMEBUFFER_BINDING
	if ( !fbo )
		return fail( "GL draws into the default framebuffer" );
	auto keyOf = [&]( GLenum att, std::uintptr_t & key, const Shared *& sh ) {
		GLint type = 0, name = 0;
		getAtt( 0x8CA9, att, 0x8CD0, &type );  // GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE
		if ( type != 0x1702 )                   // GL_TEXTURE
			return false;
		getAtt( 0x8CA9, att, 0x8CD1, &name );  // _OBJECT_NAME
		for ( const auto & e : shareds() )
			if ( e.second.tex == GLuint( name ) && e.second.glCtx == gl().ctx && !( red == 4 && e.second.borrowed ) ) {
				key = e.first;
				sh = &e.second;
				return true;
			}
		return false;
	};
	int w = 0, h = 0, samples = 0;
	for ( std::uint32_t c = 0; c < 4; c++ ) {
		GLint db = 0;
		getIv( GLenum( 0x8825 + c ), &db );  // GL_DRAW_BUFFERc
		if ( db == 0 )
			break;
		if ( db != GLint( 0x8CE0 + c ) )
			return fail( "draw buffer " + std::to_string( c ) + " is not colour attachment " + std::to_string( c ) );
		const Shared * sh = nullptr;
		if ( !keyOf( GLenum( db ), d.colorKeys[c], sh ) )
			return fail( "colour attachment " + std::to_string( c ) + " is not a shared target" );
		if ( c && ( sh->w != w || sh->h != h || sh->samples != samples ) )
			return fail( "the colour targets differ in size or samples" );
		w = sh->w;
		h = sh->h;
		samples = sh->samples;
		d.target.colors[c] = std::uint32_t( sh->fmt );
		d.target.colorCount = c + 1;
	}
	const Shared * dsh = nullptr;
	if ( !keyOf( 0x8D00, d.depthKey, dsh ) && !keyOf( 0x821A, d.depthKey, dsh ) )  // GL_DEPTH_ATTACHMENT, GL_DEPTH_STENCIL_ATTACHMENT
		return fail( "the depth attachment is not a shared target" );
	if ( d.target.colorCount && ( dsh->w != w || dsh->h != h || dsh->samples != samples ) )
		return fail( "the depth target differs from the colour targets in size or samples" );
	d.w = dsh->w;
	d.h = dsh->h;
	d.target.depth = std::uint32_t( dsh->fmt );
	d.target.samples = std::uint32_t( std::max( 1, dsh->samples ) );
	// GL's fixed state as the pipeline takes it
	GLint vp[4] = {};
	getIv( 0x0BA2, vp );  // GL_VIEWPORT
	if ( vp[0] || vp[1] || vp[2] != d.w || vp[3] != d.h )
		return fail( "the viewport is not the whole frame" );
	if ( !objBlTargetState( gp, d.target.colorCount, d.target, why, red ) )
		return false;
	// GL's scissor as the draw's (a dynamic state): its box clamped to the frame (GL's rows are the image's rows); off: the frame
	using IsOn = GLubyte( WINAPI * )( GLenum );
	const auto isOn = reinterpret_cast<IsOn>( gp( "glIsEnabled" ) );
	d.scissor[0] = d.scissor[1] = 0;
	d.scissor[2] = d.w;
	d.scissor[3] = d.h;
	if ( isOn && isOn( 0x0C11 ) && red != 3 ) {  // GL_SCISSOR_TEST
		GLint box[4] = {};
		getIv( 0x0C10, box );  // GL_SCISSOR_BOX
		const int x0 = std::min( std::max( int( box[0] ), 0 ), d.w ), y0 = std::min( std::max( int( box[1] ), 0 ), d.h );
		const int x1 = std::min( std::max( int( box[0] ) + std::max( int( box[2] ), 0 ), x0 ), d.w );
		const int y1 = std::min( std::max( int( box[1] ) + std::max( int( box[3] ), 0 ), y0 ), d.h );
		d.scissor[0] = x0;
		d.scissor[1] = y0;
		d.scissor[2] = x1 - x0;
		d.scissor[3] = y1 - y0;
	}
	if ( d.target.stencil.on && dsh->fmt != VK_FORMAT_D24_UNORM_S8_UINT && dsh->fmt != VK_FORMAT_D32_SFLOAT_S8_UINT )
		d.target.stencil = ObjBlStencil();  // no stencil buffer: GL's test passes, as none
	return true;
}

namespace
{
char kFrameColor, kFrameDs;  // the cell frame's shared keys (their addresses)
}

bool objBlFrame( int w, int h, int samples, unsigned colorFmt, unsigned dsFmt, unsigned & color, unsigned & ds, std::string & why )
{
	color = ds = 0;
	if ( !sharedTarget( reinterpret_cast<std::uintptr_t>( &kFrameColor ), 0, w, h, 1, colorFmt, color, why, 1, samples ) )
		return false;
	if ( !sharedTarget( reinterpret_cast<std::uintptr_t>( &kFrameDs ), 0, w, h, 1, dsFmt, ds, why, 1, samples ) ) {
		objBlFrameDrop();
		color = 0;
		return false;
	}
	return true;
}

void objBlFrameDrop()
{
	sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &kFrameColor ) );
	sharedTargetDrop( reinterpret_cast<std::uintptr_t>( &kFrameDs ) );
}

bool objBlFrameIs( unsigned color )
{
	return sharedTargetIs( reinterpret_cast<std::uintptr_t>( &kFrameColor ), color );
}

bool objBlFrameSamples( unsigned colorTex, unsigned dsTex, std::vector<std::uint32_t> & out, std::string & why, int red )
{
	Vk & v = vk();
	Gl & g = gl();
	Op & O = opS();
	out.clear();
	if ( !v.ok || !g.ok || g.ctx != reinterpret_cast<void *>( wglGetCurrentContext() ) )
		return why = "the present device or GL's entry points are not ready in this context", false;
	if ( !g.signalSemaphore || !g.waitSemaphore )
		return why = "no glSignalSemaphoreEXT / glWaitSemaphoreEXT", false;
	// the frame: GL's two textures, both shared targets of this context, one size and sample count
	const Shared * col = nullptr, * ds = nullptr;
	for ( const auto & e : shareds() ) {
		if ( e.second.glCtx != g.ctx )
			continue;
		if ( e.second.tex == GLuint( colorTex ) )
			col = &e.second;
		if ( e.second.tex == GLuint( dsTex ) )
			ds = &e.second;
	}
	if ( !col || !ds )
		return why = std::string( !col ? "the colour" : "the depth-stencil" ) + " texture is not a shared target", false;
	if ( col->w != ds->w || col->h != ds->h || col->samples != ds->samples )
		return why = "the colour and depth-stencil targets differ in size or samples", false;
	if ( ds->fmt != VK_FORMAT_D24_UNORM_S8_UINT && ds->fmt != VK_FORMAT_D32_SFLOAT_S8_UINT )
		return why = "the depth target has no stencil (the readback reads a depth-stencil frame)", false;
	if ( col->samples < 2 || !col->view || !ds->img )
		return why = "the readback reads a multisampled frame", false;
	opRunWait();
	if ( !opRunMake( why ) || !opSmpMake( why ) )
		return false;
	const std::size_t n = std::size_t( col->w ) * std::size_t( col->h ) * std::size_t( col->samples );
	if ( !opHostBuf( O.smpOut, n * 16, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, why ) )
		return false;
	for ( VkImageView * iv : { &O.smpDepth, &O.smpStencil } )
		if ( *iv )
			v.vkDestroyImageView( v.dev, *iv, nullptr );
	O.smpDepth = imageView( ds->img, VK_IMAGE_VIEW_TYPE_2D, ds->fmt, VK_IMAGE_ASPECT_DEPTH_BIT );
	O.smpStencil = imageView( ds->img, VK_IMAGE_VIEW_TYPE_2D, ds->fmt, VK_IMAGE_ASPECT_STENCIL_BIT );
	if ( !O.smpDepth || !O.smpStencil )
		return why = "the depth target's depth / stencil view failed", false;
	VkDescriptorImageInfo ii[3] = { { VK_NULL_HANDLE, col->view, VK_IMAGE_LAYOUT_GENERAL }, { VK_NULL_HANDLE, O.smpDepth, VK_IMAGE_LAYOUT_GENERAL },
		{ VK_NULL_HANDLE, O.smpStencil, VK_IMAGE_LAYOUT_GENERAL } };
	const VkDescriptorBufferInfo bi { O.smpOut.buf, 0, VkDeviceSize( n * 16 ) };
	VkWriteDescriptorSet wr[4] = {};
	for ( std::uint32_t k = 0; k < 4; k++ ) {
		wr[k].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		wr[k].dstSet = O.smpSet;
		wr[k].dstBinding = k;
		wr[k].descriptorCount = 1;
		wr[k].descriptorType = k < 3 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		if ( k < 3 )
			wr[k].pImageInfo = &ii[k];
		else
			wr[k].pBufferInfo = &bi;
	}
	v.vkUpdateDescriptorSets( v.dev, 4, wr, 0, nullptr );
	O.smpJob[0] = std::uint32_t( col->w );
	O.smpJob[1] = std::uint32_t( col->h );
	O.smpJob[2] = std::uint32_t( col->samples );
	O.smpJob[3] = std::uint32_t( red == 1 ? 1 : 0 );
	// GL's draws into the frame before the read; the read before GL's next write into it
	GLuint texs[2] = { col->tex, ds->tex };
	GLenum lay[2] = { kLayoutGeneral, kLayoutGeneral };
	g.signalSemaphore( O.glDoneGl, 0, nullptr, 2, texs, lay );
	g.flush();
	if ( !opWorkerRun( opSmpWork, why ) )
		return false;  // nothing signals vkDone: GL does not wait it
	g.waitSemaphore( O.vkDoneGl, 0, nullptr, 2, texs, lay );
	O.pending = true;
	opRunWait();
	out.resize( n * 4 );
	std::memcpy( out.data(), O.smpOut.ptr, n * 16 );
	return true;
}

bool objBlShade( GlProc gp, const ObjBlShade & s, std::string & why )
{
	auto fail = [&]( const std::string & w ) {
		objBlOnGl( w.c_str() );
		return why = w, false;
	};
	if ( s.twin < 0 || s.twin > 1 || !s.prog || !s.rows || !s.nRows || !s.cmds || !s.nCmds )
		return fail( "no twin, program, rows or commands" );
	ObjBlDraw d;
	d.twin = s.twin;
	d.pipeRed = s.red;
	d.vs = vkframe_objbl_vs_spv;
	d.vsWords = sizeof( vkframe_objbl_vs_spv ) / 4;
	d.ps = s.twin ? vkframe_objbl_cellcsm_ps_spv : vkframe_objbl_cell_ps_spv;
	d.psWords = s.twin ? sizeof( vkframe_objbl_cellcsm_ps_spv ) / 4 : sizeof( vkframe_objbl_cell_ps_spv ) / 4;
	std::string w2;
	if ( !objBlFrameRead( gp, d, w2 ) )
		return fail( w2 );
	// the vertex stage's GL locations (TEXCOORDn) and the class
	SpvModule vm;
	if ( !spvReflect( d.vs, d.vsWords, vm, w2 ) )
		return fail( "the vertex twin: " + w2 );
	for ( const SpvIo & in : vm.inputs ) {
		const std::size_t at = in.name.find( "TEXCOORD" );
		if ( at != std::string::npos )
			for ( std::uint32_t j = 0; j < in.locs; j++ )
				d.need |= 1u << ( ( std::atoi( in.name.c_str() + at + 8 ) + int( j ) ) & 31 );
	}
	d.poolKey = s.poolKey;
	d.attrMask = s.pool.attrMask;
	ObjBlUniStat ust;
	if ( !objBlMirror( gp, s.prog, d.vs, d.vsWords, d.ps, d.psWords, d.ubos, ust, w2 ) )
		return fail( "the uniforms: " + w2 );
	if ( !ust.mirrored )
		return fail( "GL names none of the loose uniforms (a program linked from SPIR-V)" );
	ObjBlBindStat bst;
	if ( !objBlBindUnits( gp, s.prog, d, bst, w2 ) )
		return fail( "the texture units: " + w2 );
	{
		ObjBlRunStat & rs = opS().st;
		rs.shared += bst.shared;
		rs.glOwn += bst.glOwn + bst.misfit;
		rs.texelGl += bst.texelGl;
		rs.texelShared += bst.texelShared;
		rs.compare += bst.compare;
	}
	std::vector<unsigned> names;
	if ( !objBlPoolCopy( s.poolKey, s.pool, names, w2 ) )
		return fail( "the pool: " + w2 );
	names.insert( names.end(), bst.glBufs.begin(), bst.glBufs.end() );  // the texel twins GL wrote
	d.glRows = s.rows;
	d.rows = s.nRows;
	d.cmds = s.cmds;
	d.cmdCount = s.nCmds;
	objBlDrawsReset();
	ObjBlDrawStat dst;
	if ( !objBlDrawPrep( d, dst, w2 ) )
		return fail( "the draw: " + w2 );
	if ( !objBlRun( names, 0, w2 ) )
		return fail( "the crossing: " + w2 );
	return true;
}

bool objBlTexelCopy( GlProc gp, unsigned tex, std::vector<unsigned> & glNames, std::string & why, int red )
{
	Gl & g = gl();
	using LevelIv = void( WINAPI * )( unsigned, int, unsigned, int * );
	using BufIv = void( WINAPI * )( unsigned, unsigned, int * );
	const auto lv = gp ? reinterpret_cast<LevelIv>( gp( "glGetTextureLevelParameteriv" ) ) : nullptr;
	const auto bv = gp ? reinterpret_cast<BufIv>( gp( "glGetNamedBufferParameteriv" ) ) : nullptr;
	if ( !vk().ok || !g.ok || !g.copyNamedBufferSubData || !g.namedBufferStorageMem || !lv || !bv || !tex )
		return why = "no present device, no glCopyNamedBufferSubData / glGetTextureLevelParameteriv (GL 4.5), or no texture", false;
	int buf = 0, fmt = 0, off = 0, size = 0, whole = 0;
	lv( tex, 0, 0x8C2D, &buf );   // TEXTURE_BUFFER_DATA_STORE_BINDING
	lv( tex, 0, 0x1003, &fmt );   // TEXTURE_INTERNAL_FORMAT
	lv( tex, 0, 0x919D, &off );   // TEXTURE_BUFFER_OFFSET
	lv( tex, 0, 0x919E, &size );  // TEXTURE_BUFFER_SIZE
	if ( !buf )
		return why = "the buffer texture has no buffer", false;
	bv( unsigned( buf ), 0x8764, &whole );  // BUFFER_SIZE
	if ( size <= 0 )
		size = whole - off;
	struct Fmt
	{
		int gl;
		VkFormat vk;
		std::uint32_t cls, bytes;
	};
	static const Fmt fmts[] = { { 0x8814, VK_FORMAT_R32G32B32A32_SFLOAT, 0, 16 }, { 0x8D82, VK_FORMAT_R32G32B32A32_SINT, 1, 16 },
		{ 0x8D70, VK_FORMAT_R32G32B32A32_UINT, 2, 16 }, { 0x822E, VK_FORMAT_R32_SFLOAT, 0, 4 }, { 0x8235, VK_FORMAT_R32_SINT, 1, 4 },
		{ 0x8236, VK_FORMAT_R32_UINT, 2, 4 }, { 0x8230, VK_FORMAT_R32G32_SFLOAT, 0, 8 }, { 0x823B, VK_FORMAT_R32G32_SINT, 1, 8 },
		{ 0x823C, VK_FORMAT_R32G32_UINT, 2, 8 }, { 0x881A, VK_FORMAT_R16G16B16A16_SFLOAT, 0, 8 }, { 0x8058, VK_FORMAT_R8G8B8A8_UNORM, 0, 4 } };
	const Fmt * f = nullptr;
	for ( const Fmt & e : fmts )
		if ( e.gl == fmt )
			f = &e;
	if ( !f ) {
		char hex[16];
		std::snprintf( hex, sizeof( hex ), "0x%X", unsigned( fmt ) );
		return why = std::string( "GL internal format " ) + hex + " has no texel twin", false;
	}
	const int from = off + ( red == 1 ? 16 : 0 );
	const int bytes = std::min( size, whole - from );
	if ( off < 0 || bytes < int( f->bytes ) )
		return why = "the buffer texture's range holds no texel", false;
	OpTexel & t = opS().texTwins[tex];
	const VkDeviceSize need = VkDeviceSize( bytes );
	if ( t.buf.buf && ( t.buf.cap < need || t.buf.glCtx != g.ctx ) ) {
		if ( opS().live )
			return why = "the texel twin would move under prepared draws", false;
		opRunWait();
	}
	if ( !csmBuf( t.buf, need, true, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, why ) )
		return false;
	g.copyNamedBufferSubData( GLuint( buf ), t.buf.gl, std::intptr_t( from ), 0, std::intptr_t( bytes ) );
	t.fmt = f->vk;
	t.cls = f->cls;
	t.range = need / f->bytes * f->bytes;
	glNames.push_back( t.buf.gl );
	return true;
}

bool objBlDrawPrep( const ObjBlDraw & d, ObjBlDrawStat & st, std::string & why )
{
	Vk & v = vk();
	Op & O = opS();
	st = ObjBlDrawStat();
	if ( !v.ok )
		return why = "no present device", false;
	opRunWait();  // the slots' host buffers and the rows are rewritten below
	if ( !d.cmds || !d.cmdCount || d.w < 1 || d.h < 1 )
		return why = "no commands or no frame", false;
	// the pipeline of the class's layout
	std::vector<ObjBlBuffer> bufs;
	std::vector<ObjBlAttr> attrs;
	objBlPoolLayout( d.attrMask, bufs, attrs, d.need );
	ObjBlPipeInfo info;
	if ( !objBlPipe( d.twin, d.vs, d.vsWords, d.ps, d.psWords, d.target, bufs, attrs, info, why, d.pipeRed ) )
		return false;
	O.st.dropped += info.dropped;
	std::size_t entry = O.made.size();
	for ( std::size_t i = 0; i < O.made.size(); i++ )
		if ( opSame( O.made[i], d.twin, d.vs, d.vsWords, d.ps, d.psWords, d.target, bufs, attrs ) )
			entry = i;
	if ( entry == O.made.size() )
		return why = "the pipeline was made but is not found", false;
	const OpEntry & e = O.made[entry];
	VkBuffer vb[17] = {}, ib = VK_NULL_HANDLE;
	if ( !objBlPoolBinds( d.poolKey, vb, ib ) )
		return why = "the class has no twins (objBlPoolCopy first)", false;
	// the rows: this draw's, else the frame's so far
	if ( d.glRows && d.rows ) {
		if ( !opHostBuf( O.rows, d.rows * kBlRowBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, why ) )
			return false;
		st.whole = blTranslate( d.glRows, O.rows.ptr, d.rows, 0 );
		O.rowCount = d.rows;
	}
	if ( !O.rowCount )
		return why = "no rows (wwDraws) this frame", false;
	// the slot: a set 0 of this pipeline's layout
	if ( O.live == O.draws.size() )
		O.draws.emplace_back();
	OpDraw & s = O.draws[O.live];
	if ( s.dpool && s.entry != entry ) {
		drain();
		v.vkDeviceWaitIdle( v.dev );
		opDrawFree( s );
	}
	if ( !s.dpool ) {
		std::map<VkDescriptorType, std::uint32_t> count;
		for ( const SpvBind & b : e.set0 )
			count[VkDescriptorType( b.type )] += b.count;
		std::vector<VkDescriptorPoolSize> sizes;
		for ( const auto & c : count )
			sizes.push_back( { c.first, c.second } );
		VkDescriptorPoolCreateInfo pci { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		pci.maxSets = 1;
		pci.poolSizeCount = std::uint32_t( sizes.size() );
		pci.pPoolSizes = sizes.data();
		VkDescriptorSetAllocateInfo ai { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &e.dsl0;
		if ( v.vkCreateDescriptorPool( v.dev, &pci, nullptr, &s.dpool ) != VK_SUCCESS
			|| ( ai.descriptorPool = s.dpool, v.vkAllocateDescriptorSets( v.dev, &ai, &s.dset ) != VK_SUCCESS ) )
			return why = "the draw's set 0 failed", false;
		s.entry = entry;
	}
	// set 0, binding by binding (the frame's previous submit of this slot is done: the caller's fence)
	std::string w;
	BlSmpKey k0;
	std::uint32_t at0 = 0;
	if ( !blSamplerKey( BlSampler(), k0, w ) || !blSamplerIndex( k0, at0, w ) )
		return why = "the default sampler: " + w, false;
	const VkSampler smp = blS().smps[at0];
	std::vector<VkWriteDescriptorSet> writes;
	std::vector<VkDescriptorBufferInfo> bis;
	std::vector<VkDescriptorImageInfo> iis;
	std::vector<VkBufferView> tvs;
	std::size_t total = 0;  // the arrays are reserved whole: the writes point into them
	for ( const SpvBind & b : e.set0 )
		total += b.count;
	bis.reserve( total );
	iis.reserve( total );
	tvs.reserve( total );
	for ( const SpvBind & b : e.set0 ) {
		VkWriteDescriptorSet wr { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		wr.dstSet = s.dset;
		wr.dstBinding = b.binding;
		wr.descriptorCount = b.count;
		wr.descriptorType = VkDescriptorType( b.type );
		const std::string where = "set 0 binding " + std::to_string( b.binding ) + " (" + b.name + ")";
		if ( b.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ) {
			const std::vector<std::uint8_t> * src = nullptr;
			for ( const auto & u : d.ubos )
				if ( u.first == b.binding )
					src = &u.second;
			if ( !src )
				return why = where + ": no state given (objBlMirror)", false;
			if ( src->size() < b.bytes )
				return why = where + ": " + std::to_string( src->size() ) + " bytes given, the module reads " + std::to_string( b.bytes ), false;
			CsmBuf & u = s.ubo[b.binding];
			if ( !opHostBuf( u, b.bytes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, why ) )
				return false;
			std::memcpy( u.ptr, src->data(), b.bytes );
			bis.push_back( { u.buf, 0, b.bytes } );
			wr.pBufferInfo = &bis.back();
			st.ubos++;
		} else if ( b.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ) {
			if ( b.name != "wwDraws" && b.name != "type.StructuredBuffer.WwDraw" )
				return why = where + ": a storage buffer the draw does not know", false;
			bis.push_back( { O.rows.buf, 0, VkDeviceSize( O.rowCount * kBlRowBytes ) } );
			wr.pBufferInfo = &bis.back();
			st.rowBuffers++;
		} else if ( b.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ) {
			const std::size_t first = iis.size();
			for ( std::uint32_t a = 0; a < b.count; a++ ) {
				VkImageView view = VK_NULL_HANDLE;
				for ( const auto & im : d.images )
					if ( im.first == b.binding + a ) {
						if ( !objBlSharedFits( im.second, b.dim, b.arrayed, w ) )
							return why = where + ": " + w, false;
						view = sharedFind( im.second )->view;
						st.images++;
					}
				if ( !view ) {
					if ( !opStandIn( b.dim, b.arrayed, b.sampled, view, w ) )
						return why = where + ": " + w, false;
					st.standIns++;
				}
				iis.push_back( { VK_NULL_HANDLE, view, VK_IMAGE_LAYOUT_GENERAL } );
			}
			wr.pImageInfo = &iis[first];
		} else if ( b.type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ) {
			const std::size_t first = tvs.size();
			for ( std::uint32_t a = 0; a < b.count; a++ ) {
				VkBufferView tv = VK_NULL_HANDLE;
				for ( const auto & tx : d.texels )
					if ( tx.first == b.binding + a ) {
						if ( !opTexelView( tx.second, b.sampled, tv, w ) )
							return why = where + ": " + w, false;
						st.texelsGl++;
					}
				if ( !tv && !opZeroTexel( b.sampled, tv, w ) )
					return why = where + ": " + w, false;
				tvs.push_back( tv );
				st.texels++;
			}
			wr.pTexelBufferView = &tvs[first];
		} else if ( b.type == VK_DESCRIPTOR_TYPE_SAMPLER ) {
			const std::size_t first = iis.size();
			for ( std::uint32_t a = 0; a < b.count; a++ ) {
				VkSampler one = smp;
				for ( const auto & sm : d.samplers )
					if ( sm.first == b.binding + a ) {
						if ( !opSampler( sm.second, one, w ) )
							return why = where + ": " + w, false;
						st.glSamplers++;
					}
				iis.push_back( { one, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED } );
			}
			wr.pImageInfo = &iis[first];
			st.samplers += int( b.count );
		} else
			return why = where + ": a descriptor type the draw does not write", false;
		writes.push_back( wr );
	}
	// the commands
	if ( !opHostBuf( s.cmds, d.cmdCount * 20, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, why ) )
		return false;
	std::memcpy( s.cmds.ptr, d.cmds, d.cmdCount * 20 );
	s.host.assign( static_cast<const std::uint8_t *>( d.cmds ), static_cast<const std::uint8_t *>( d.cmds ) + d.cmdCount * 20 );
	s.cmdCount = d.cmdCount;
	s.poolKey = d.poolKey;
	// the framebuffer of the targets (made again when a view or the size changed)
	std::vector<VkImageView> views;
	for ( std::uint32_t c = 0; c < d.target.colorCount; c++ ) {
		const Shared * sh = sharedFind( d.colorKeys[c] );
		if ( !sh || sh->w < d.w || sh->h < d.h )
			return why = "colour target " + std::to_string( c ) + ( sh ? " is smaller than the frame" : " is not a shared target" ), false;
		views.push_back( sh->view );
	}
	if ( d.target.depth ) {
		const Shared * sh = sharedFind( d.depthKey );
		if ( !sh || sh->w < d.w || sh->h < d.h )
			return why = std::string( "the depth target " ) + ( sh ? "is smaller than the frame" : "is not a shared target" ), false;
		views.push_back( sh->attView ? sh->attView : sh->view );  // a stencil format: the depth and stencil aspects
	}
	if ( s.fb && ( s.fbViews != views || s.w != d.w || s.h != d.h ) ) {
		drain();
		v.vkDeviceWaitIdle( v.dev );
		v.vkDestroyFramebuffer( v.dev, s.fb, nullptr );
		s.fb = VK_NULL_HANDLE;
	}
	if ( !s.fb ) {
		VkFramebufferCreateInfo fci { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		fci.renderPass = e.pass;
		fci.attachmentCount = std::uint32_t( views.size() );
		fci.pAttachments = views.data();
		fci.width = std::uint32_t( d.w );
		fci.height = std::uint32_t( d.h );
		fci.layers = 1;
		if ( v.vkCreateFramebuffer( v.dev, &fci, nullptr, &s.fb ) != VK_SUCCESS )
			return why = "the draw's framebuffer failed", false;
		s.fbViews = views;
	}
	s.w = d.w;
	s.h = d.h;
	const bool whole = d.scissor[2] < 0 || d.scissor[3] < 0;  // not read from GL (a hand-made draw): the whole frame
	s.scissor.offset = { whole ? 0 : d.scissor[0], whole ? 0 : d.scissor[1] };
	s.scissor.extent = { std::uint32_t( whole ? d.w : d.scissor[2] ), std::uint32_t( whole ? d.h : d.scissor[3] ) };
	s.keys.assign( d.colorKeys, d.colorKeys + d.target.colorCount );
	if ( d.target.depth )
		s.keys.push_back( d.depthKey );
	for ( const auto & im : d.images )
		s.keys.push_back( im.second );
	v.vkUpdateDescriptorSets( v.dev, std::uint32_t( writes.size() ), writes.data(), 0, nullptr );
	O.live++;
	return true;
}
}  // namespace WwVkPresent
