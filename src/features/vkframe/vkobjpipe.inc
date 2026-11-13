// Lane VKOBJ4 (2026-11-13, plan row G4 slice 4): the bl twins' shaded pipelines, both stages' resources read from the
// modules. Included by vkpresent.cpp after vkbindless.inc (blSetGet: the table is set 1). The API: vkobjpipe.h.
#include "features/vkframe/vkobjpipe.h"

#include <cstring>
#include <map>
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
};

struct Op
{
	std::vector<OpEntry> made;
	int refused = 0;
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
		|| t.depthOp != u.depthOp || t.depthWrite != u.depthWrite || t.twoSided != u.twoSided )
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
			break;
		case 20: types[a[0]] = { op }; break;                                       // OpTypeBool
		case 21: if ( wc >= 4 ) types[a[0]] = { op, a[1], a[2] }; break;            // OpTypeInt: width, signed
		case 22: if ( wc >= 3 ) types[a[0]] = { op, a[1] }; break;                  // OpTypeFloat
		case 23: case 24: if ( wc >= 4 ) types[a[0]] = { op, a[1], a[2] }; break;  // OpTypeVector / Matrix: element, count
		case 25: if ( wc >= 9 ) types[a[0]] = { op, a[2], a[6] }; break;            // OpTypeImage: dim, sampled
		case 26: types[a[0]] = { op }; break;                                       // OpTypeSampler
		case 27: if ( wc >= 3 ) types[a[0]] = { op, a[1] }; break;                  // OpTypeSampledImage
		case 28: if ( wc >= 4 ) types[a[0]] = { op, a[1], a[2] }; break;            // OpTypeArray: element, length id
		case 29: if ( wc >= 3 ) types[a[0]] = { op, a[1] }; break;                  // OpTypeRuntimeArray
		case 30: types[a[0]] = { op }; break;                                       // OpTypeStruct
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
				io.slots = constants.count( x.b ) ? constants[x.b] : 1;
				x = ty( x.a );
			}
			if ( x.op == 24 ) {
				io.slots *= x.b;
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
		if ( x.op == 25 )
			b.type = x.a == 5 ? ( x.b == 2 ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER )
				: ( x.b == 2 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE );
		else if ( x.op == 26 )
			b.type = VK_DESCRIPTOR_TYPE_SAMPLER;
		else if ( x.op == 27 )
			b.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		else if ( x.op == 30 && ( v.storage == 12 || bufferBlock.count( t ) ) )
			b.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		else if ( x.op == 30 && v.storage == 2 && block.count( t ) )
			b.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		else {
			why = nm + ": a descriptor type the reader does not take";
			return false;
		}
		out.binds.push_back( b );
	}
	return true;
}

bool objBlPipe( int twin, const std::uint32_t * vs, std::size_t vsN, const std::uint32_t * ps, std::size_t psN, const ObjBlTarget & target,
	const std::vector<ObjBlBuffer> & buffers, const std::vector<ObjBlAttr> & attrs, ObjBlPipeInfo & info, std::string & why )
{
	Vk & v = vk();
	Op & O = opS();
	for ( const OpEntry & e : O.made )
		if ( opSame( e, twin, vs, vsN, ps, psN, target, buffers, attrs ) ) {
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
			found = found || ( o.location == i.location && o.slots == i.slots && o.comps == i.comps && o.base == i.base );
		if ( !found )
			return refuse( "the pixel stage's " + i.name + " (Location " + std::to_string( i.location ) + ") is no vertex output of its shape" );
		in.varyings++;
	}
	// the targets: Locations 0 .. colorCount - 1, one each
	if ( target.colorCount > 4 || m[1].outputs.size() != target.colorCount )
		return refuse( "the pixel stage writes " + std::to_string( m[1].outputs.size() ) + " targets, the frame has "
			+ std::to_string( target.colorCount ) );
	for ( const SpvIo & o : m[1].outputs )
		if ( o.location >= target.colorCount || o.slots != 1 )
			return refuse( "the pixel stage's " + o.name + " is at Location " + std::to_string( o.location ) );
	// the attributes: an input in_var_TEXCOORD<k> (slots s) reads GL's locations k .. k + s - 1
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
		for ( std::uint32_t j = 0; j < i.slots; j++ ) {
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
	std::vector<VkPipelineColorBlendAttachmentState> cba( target.colorCount );
	for ( VkPipelineColorBlendAttachmentState & c : cba )
		c.colorWriteMask = 0xF;
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

void objBlPipeFree()
{
	Op & O = opS();
	if ( !O.made.empty() && vk().ok ) {
		drain();
		vk().vkDeviceWaitIdle( vk().dev );
		for ( OpEntry & e : O.made )
			opDestroy( e );
	}
	O.made.clear();
	O.refused = 0;
}
}  // namespace WwVkPresent
