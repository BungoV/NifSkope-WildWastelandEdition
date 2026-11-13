// Lane VKOBJ4 (2026-11-13, plan row G4 slice 4): the bl twins' shaded pipelines, both stages' resources read from the
// modules. Included by vkpresent.cpp after vkbindless.inc (blSetGet: the table is set 1). The API: vkobjpipe.h.
#include "features/vkframe/vkobjpipe.h"

#include <algorithm>
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
};

// a class pool's twins: one exported buffer per attribute, the indices (GL imports each: the copy target)
struct OpPool
{
	CsmBuf vb[16], ib;
	std::uint64_t attrMask = 0;
	bool gone = false;
};

struct Op
{
	std::vector<OpEntry> made;
	int refused = 0;
	std::map<std::uintptr_t, OpPool> pools;
	CsmBuf constVb;  // GL's generic attribute default (0,0,0,1) for the locations a class does not carry (host-visible)
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
		case 25: if ( wc >= 9 ) types[a[0]] = { op, a[2], a[6], a[4] }; break;      // OpTypeImage: dim, sampled, arrayed
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
		if ( x.op == 27 ) {
			b.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			x = ty( x.a );
			b.dim = x.a;
			b.arrayed = x.c;
		} else if ( x.op == 25 ) {
			b.type = x.a == 5 ? ( x.b == 2 ? VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER )
				: ( x.b == 2 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE );
			b.dim = x.a;
			b.arrayed = x.c;
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
	if ( ( !O.made.empty() || !O.pools.empty() ) && vk().ok ) {
		drain();
		vk().vkDeviceWaitIdle( vk().dev );
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
	for ( const SpvMember & m : block.members ) {
		st.members++;
		if ( m.base > 2 || m.comps > 4 || m.cols > 4 || ( m.cols > 1 && !m.colStride ) || ( m.count > 1 && !m.arrayStride ) ) {
			why = block.name + "." + m.name + ": a member type the mirror does not take";
			return false;
		}
		const std::uint32_t stride = red == 2 ? m.cols * m.comps * 4 : m.arrayStride;
		bool any = false;
		for ( std::uint32_t e = 0; e < m.count; e++ ) {
			const std::string nm = m.count > 1 ? m.name + "[" + std::to_string( e ) + "]" : m.name;
			const int loc = g.getUniformLocation( prog, nm.c_str() );
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
	}
	return true;
}

bool objBlBlock( GlProc gp, unsigned prog, const SpvBind & block, std::vector<std::uint8_t> & out, std::string & why, int red )
{
	out.clear();
	OuGl g;
	if ( !ouGl( gp, g, why ) )
		return false;
	const unsigned idx = g.getUniformBlockIndex( prog, block.name.c_str() );
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
		const int loc = g.getUniformLocation( prog, u.name.c_str() );
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
}  // namespace WwVkPresent
