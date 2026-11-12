"""SPIR-V patcher: DXC's Vulkan-flavoured module -> what GL_ARB_gl_spirv takes (HLSLBULK1 round 5, A1).
A word-level editor, no SPIRV-Tools needed. What it changes (each one found with the driver, tests/hlsllive/spirv_probe.py):
  1. an OpStore of an OpConstantNull array is expanded per element (the NV compiler throws on the whole-array store);
  2. vertex-stage inputs get Location = the TEXCOORD index in their name (DXC numbers them by declaration order);
  3. the LooseUniforms cbuffer (the converted GLSL's loose `uniform`s) becomes default-block uniforms with explicit
     Locations, named as in the GLSL, so the app's glGetUniformLocation / glUniform* keep working unchanged;
  4. a separate Texture + SamplerState pair becomes ONE combined sampler uniform (GL has no separate samplers), named after
     the texture, with a Location; a texture with no sampler (texelFetch only) gets a combined sampler too;
  5. a real uniform block (globalUniforms, skinningUniforms, ...) keeps its layout and gets Binding = BLOCK_BINDING[name];
  6. VertexIndex / InstanceIndex built-ins become VertexId / InstanceId (the GL names).
Usage: patch(spv_bytes, block_bindings) -> (new bytes, report dict)."""
import re
import struct

OP_NAME, OP_MEMBER_NAME = 5, 6
OP_ENTRY, OP_EXECMODE, OP_CAP, OP_EXT = 15, 16, 17, 10
OP_TYPE_VOID, OP_TYPE_BOOL, OP_TYPE_INT, OP_TYPE_FLOAT, OP_TYPE_VECTOR, OP_TYPE_MATRIX = 19, 20, 21, 22, 23, 24
OP_TYPE_IMAGE, OP_TYPE_SAMPLER, OP_TYPE_SAMPLED_IMAGE, OP_TYPE_ARRAY, OP_TYPE_RTARRAY = 25, 26, 27, 28, 29
OP_TYPE_STRUCT, OP_TYPE_POINTER, OP_TYPE_FUNCTION = 30, 32, 33
OP_CONSTANT, OP_CONSTANT_NULL = 43, 46
OP_FUNCTION, OP_LABEL = 54, 248
OP_VARIABLE, OP_LOAD, OP_STORE, OP_ACCESS_CHAIN, OP_IN_BOUNDS_AC = 59, 61, 62, 65, 66
OP_DECORATE, OP_MEMBER_DECORATE = 71, 72
OP_SAMPLED_IMAGE, OP_IMAGE, OP_BITCAST = 86, 100, 124
SAMPLED_IMAGE_CONSUMERS = {87, 88, 89, 90, 91, 92, 93, 94, 96, 97, 105}   # operand [2] is a sampled image
DEC_BLOCK, DEC_BUFFER_BLOCK, DEC_ROW_MAJOR, DEC_COL_MAJOR, DEC_ARRAY_STRIDE, DEC_MATRIX_STRIDE = 2, 3, 4, 5, 6, 7
DEC_BUILTIN, DEC_LOCATION, DEC_BINDING, DEC_DESCRIPTOR_SET, DEC_OFFSET = 11, 30, 33, 34, 35
SC_UNIFORM_CONSTANT, SC_INPUT, SC_UNIFORM, SC_OUTPUT, SC_FUNCTION = 0, 1, 2, 3, 7
BI_VERTEX_INDEX, BI_INSTANCE_INDEX, BI_VERTEX_ID, BI_INSTANCE_ID = 42, 43, 5, 6
EM_VERTEX = 0

HEAD_OPS = {OP_CAP, OP_EXT, 11, 14, OP_ENTRY, OP_EXECMODE, 3, 4, 5, 6, 7, 8}   # up to the debug names
ANNOT_OPS = {71, 72, 73, 74, 75, 5632}


class Inst:
    __slots__ = ('op', 'a')

    def __init__(self, op, a):
        self.op, self.a = op, a

    def words(self):
        return [((len(self.a) + 1) << 16) | self.op] + self.a


def lit_string(words):
    """(string, words consumed) of a nul-terminated UTF-8 literal packed in words"""
    b = b''.join(struct.pack('<I', w) for w in words)
    s = b.split(b'\0', 1)[0]
    return s.decode('utf-8', 'replace'), len(s) // 4 + 1


def str_words(s):
    b = s.encode('utf-8') + b'\0'
    b += b'\0' * (-len(b) % 4)
    return list(struct.unpack('<%dI' % (len(b) // 4), b))


class Module:
    def __init__(self, data):
        w = struct.unpack('<%dI' % (len(data) // 4), data)
        assert w[0] == 0x07230203, 'not SPIR-V'
        self.hdr = list(w[:5])
        self.bound = w[3]
        insts, i = [], 5
        while i < len(w):
            wc, op = w[i] >> 16, w[i] & 0xFFFF
            insts.append(Inst(op, list(w[i + 1:i + wc])))
            i += wc
        k = next((n for n, x in enumerate(insts) if x.op == OP_FUNCTION), len(insts))
        pre, self.funcs = insts[:k], insts[k:]
        self.head = [x for x in pre if x.op in HEAD_OPS]
        self.annot = [x for x in pre if x.op in ANNOT_OPS]
        self.glob = [x for x in pre if x.op not in HEAD_OPS and x.op not in ANNOT_OPS]
        self.index()

    def index(self):
        self.defn = {}       # id -> Inst (types, constants, variables, global)
        self.names, self.mnames = {}, {}
        for x in self.head:
            if x.op == OP_NAME:
                self.names[x.a[0]] = lit_string(x.a[1:])[0]
            elif x.op == OP_MEMBER_NAME:
                self.mnames[(x.a[0], x.a[1])] = lit_string(x.a[2:])[0]
        for x in self.glob:
            if x.op >= 19 and x.op not in (OP_STORE,) and len(x.a) > 0 and x.op not in (OP_FUNCTION,):
                rid = x.a[0] if x.op < 30 or x.op in (OP_TYPE_STRUCT, OP_TYPE_POINTER, OP_TYPE_FUNCTION) else x.a[1]
                if x.op in (OP_TYPE_STRUCT, OP_TYPE_POINTER, OP_TYPE_FUNCTION) or x.op < 30 or x.op >= 41:
                    self.defn[rid] = x
        self.decs = {}       # id -> [[decoration, operands...]]
        self.mdecs = {}      # (id, member) -> [[decoration, operands...]]
        for x in self.annot:
            if x.op == OP_DECORATE:
                self.decs.setdefault(x.a[0], []).append(x.a[1:])
            elif x.op == OP_MEMBER_DECORATE:
                self.mdecs.setdefault((x.a[0], x.a[1]), []).append(x.a[2:])

    def new_id(self):
        self.bound += 1
        return self.bound - 1

    def dec(self, i, d):
        for e in self.decs.get(i, []):
            if e[0] == d:
                return e[1:]
        return None

    def add_dec(self, i, *d):
        self.annot.append(Inst(OP_DECORATE, [i] + list(d)))
        self.decs.setdefault(i, []).append(list(d))

    def drop_dec(self, i, d):
        self.annot = [x for x in self.annot if not (x.op == OP_DECORATE and x.a[0] == i and x.a[1] == d)]
        self.decs[i] = [e for e in self.decs.get(i, []) if e[0] != d]

    def name_of(self, i):
        return self.names.get(i, '')

    def add_name(self, i, s):
        self.head.append(Inst(OP_NAME, [i] + str_words(s)))
        self.names[i] = s

    def add_glob(self, x):
        self.glob.append(x)
        # a defining instruction: result id at a[0] for types, a[1] for constants / variables
        if x.op in (OP_TYPE_STRUCT, OP_TYPE_POINTER, OP_TYPE_FUNCTION) or x.op < 30:
            self.defn[x.a[0]] = x
        else:
            self.defn[x.a[1]] = x

    def emit(self):
        out = []
        self.hdr[3] = self.bound
        out += self.hdr
        for lst in (self.head, self.annot, self.glob, self.funcs):
            for x in lst:
                out += x.words()
        return struct.pack('<%dI' % len(out), *out)

    def stage(self):
        for x in self.head:
            if x.op == OP_ENTRY:
                return x.a[0]
        return -1


def scalar_slots(mod, tid):
    """default-block uniform locations a variable of this type consumes: array length, else 1"""
    t = mod.defn[tid]
    if t.op == OP_TYPE_ARRAY:
        return const_value(mod, t.a[2]) * scalar_slots(mod, t.a[1])
    if t.op == OP_TYPE_STRUCT:
        return sum(scalar_slots(mod, m) for m in t.a[1:])
    return 1


def const_value(mod, cid):
    return mod.defn[cid].a[2]


def strip_type(mod, tid, cache):
    """the type without block layout decorations (ArrayStride): same id when there is nothing to strip"""
    if tid in cache:
        return cache[tid]
    return tid      # layout decorations are left on loose types: the driver ignores them on a UniformConstant


def int_twin(mod, tid, cache):
    """the signed twin of an unsigned 32-bit int type (scalar, vector, array): a HLSL bool is a uint in SPIR-V, and glUniform1i on a
    uint uniform is INVALID_OPERATION; the app sets bools with glUniform1i, so a bool uniform is declared int and bitcast on load"""
    if tid in cache:
        return cache[tid]
    t = mod.defn[tid]
    if t.op == OP_TYPE_INT and t.a[1] == 32 and t.a[2] == 0:
        for x in mod.glob:
            if x.op == OP_TYPE_INT and x.a[1] == 32 and x.a[2] == 1:
                cache[tid] = x.a[0]
                return x.a[0]
        i = mod.new_id()
        mod.add_glob(Inst(OP_TYPE_INT, [i, 32, 1]))
        cache[tid] = i
        return i
    if t.op == OP_TYPE_VECTOR:
        i = mod.new_id()
        c = int_twin(mod, t.a[1], cache)
        mod.add_glob(Inst(OP_TYPE_VECTOR, [i, c, t.a[2]]))
        cache[tid] = i
        return i
    if t.op == OP_TYPE_ARRAY:
        c = int_twin(mod, t.a[1], cache)
        i = mod.new_id()
        mod.add_glob(Inst(OP_TYPE_ARRAY, [i, c, t.a[2]]))
        cache[tid] = i
        return i
    raise ValueError('bool uniform of an unexpected type (op %d)' % t.op)


def ptr_type(mod, sc, pointee, cache):
    k = (sc, pointee)
    if k not in cache:
        i = mod.new_id()
        mod.add_glob(Inst(OP_TYPE_POINTER, [i, sc, pointee]))
        cache[k] = i
    return cache[k]


def patch(data, block_bindings, report=None, locs=None, bool_names=()):
    locs = locs if locs is not None else {}   # name -> (location, slots), shared by every stage of one program
    mod = Module(data)
    rep = report if report is not None else {}
    rep.setdefault('loose', []); rep.setdefault('samplers', []); rep.setdefault('blocks', []); rep.setdefault('notes', [])
    stage = mod.stage()
    mod.hdr[1] = 0x00010000   # DXC for Vulkan 1.0 already writes 1.0; a later target is not accepted by the GL path
    # ---- variables of the module
    gvars = [x for x in mod.glob if x.op == OP_VARIABLE]   # a = [ptr type, id, storage class, (init)]
    vtype = {x.a[1]: x for x in gvars}
    ptrs = {}   # (storage, pointee) -> pointer type id, existing ones first
    for x in mod.glob:
        if x.op == OP_TYPE_POINTER:
            ptrs.setdefault((x.a[1], x.a[2]), x.a[0])
    stripc = {}
    twinc = {}

    # ---- 0. OpUndef -> OpConstantNull (the NV GL SPIR-V path mis-compiles a conditional insert into an undef, particles falloff)
    for x in mod.glob:
        if x.op == 1:
            x.op = OP_CONSTANT_NULL

    # ---- 6. built-ins
    for e in [e for e in mod.annot if e.op == OP_DECORATE and e.a[1] == DEC_BUILTIN]:
        if e.a[2] == BI_VERTEX_INDEX:
            e.a[2] = BI_VERTEX_ID
        elif e.a[2] == BI_INSTANCE_INDEX:
            e.a[2] = BI_INSTANCE_ID
    for lst in mod.decs.values():
        for e in lst:
            if e[0] == DEC_BUILTIN and e[1] in (BI_VERTEX_INDEX, BI_INSTANCE_INDEX):
                e[1] = BI_VERTEX_ID if e[1] == BI_VERTEX_INDEX else BI_INSTANCE_ID

    # ---- 2. vertex inputs: Location = TEXCOORD index
    if stage == EM_VERTEX:
        for v in gvars:
            vid, sc = v.a[1], v.a[2]
            if sc == SC_INPUT and mod.dec(vid, DEC_LOCATION) is not None:
                nm = mod.name_of(vid)
                if nm.startswith('in.var.TEXCOORD'):
                    # a matrix attribute's columns are TEXCOORD<loc>_<column> (spirv-cross); int('8_0') is 80 in Python: add the column
                    tm = re.match(r'(\d+)(?:_(\d+))?$', nm[len('in.var.TEXCOORD'):])
                    n = int(tm.group(1)) + int(tm.group(2) or 0)
                    for e in mod.decs[vid]:
                        if e[0] == DEC_LOCATION:
                            e[1] = n
                    for x in mod.annot:
                        if x.op == OP_DECORATE and x.a[0] == vid and x.a[1] == DEC_LOCATION:
                            x.a[2] = n

    # ---- 5. real uniform blocks: Binding by name; the loose block is split below
    loose_var = None
    for v in gvars:
        vid, sc = v.a[1], v.a[2]
        if sc != SC_UNIFORM:
            continue
        pt = mod.defn[v.a[0]]            # OpTypePointer
        st = pt.a[2]
        nm = mod.name_of(vid)
        if nm == 'LooseUniforms':
            loose_var = v
            continue
        if nm in block_bindings:
            b = block_bindings[nm]
            # GL names an interface block by its type's name: DXC names the type type_<var>
            mod.head = [h for h in mod.head if not (h.op == OP_NAME and h.a[0] in (st, vid))]   # no instance name: members read plain
            mod.add_name(st, nm)
            for x in mod.annot:
                if x.op == OP_DECORATE and x.a[0] == vid and x.a[1] == DEC_BINDING:
                    x.a[2] = b
            for e in mod.decs.get(vid, []):
                if e[0] == DEC_BINDING:
                    e[1] = b
            rep['blocks'].append((nm, b))
        else:
            rep['notes'].append('block %s has no GL binding (kept %s)' % (nm, mod.dec(vid, DEC_BINDING)))
            rep['blocks'].append((nm, (mod.dec(vid, DEC_BINDING) or [None])[0]))

    # ---- 3. LooseUniforms -> default-block uniforms
    # one name, two shapes: game_dof_blur declares `int taps`, game_dof_down `vec4 taps[4]`; whichever shader comes first
    # must reserve the larger one (the name keeps one base location in every shader)
    MIN_RESERVE = {'taps': 4}

    def alloc(name, slots, is_array=False):
        # an array is reserved 64 locations: the same name is declared with different lengths in different shaders
        # (textureUnits[10] here, textureUnits[30] there) and every shader must see the same base location
        if name in locs:
            if slots > locs[name][1]:
                raise ValueError('uniform %s needs %d locations, %d reserved' % (name, slots, locs[name][1]))
            return locs[name][0]
        n = sum(v[1] for v in locs.values())
        locs[name] = (n, max(MIN_RESERVE.get(name, 1), slots) if slots == 1 and not is_array else max(slots, 64))
        return n
    loc_next = 0
    if loose_var is not None:
        lid = loose_var.a[1]
        st_id = mod.defn[loose_var.a[0]].a[2]
        st = mod.defn[st_id]
        members = st.a[1:]
        mvar = {}
        def reach(tid, acc):
            if tid in acc or tid not in mod.defn:
                return acc
            acc.add(tid)
            t = mod.defn[tid]
            if t.op == OP_TYPE_ARRAY:
                reach(t.a[1], acc)
            elif t.op == OP_TYPE_STRUCT:
                for m in t.a[1:]:
                    reach(m, acc)
            return acc
        keep = set()
        for v in gvars:
            if v.a[2] == SC_UNIFORM and v.a[1] != lid:
                reach(mod.defn[v.a[0]].a[2], keep)
        lt = set()
        for mt in members:
            reach(mt, lt)
        lt -= keep
        mod.annot = [x for x in mod.annot if not ((x.op == OP_DECORATE and x.a[0] in lt and x.a[1] in (DEC_ARRAY_STRIDE, DEC_BLOCK))
                                                  or (x.op == OP_MEMBER_DECORATE and x.a[0] in lt and x.a[2] in (DEC_OFFSET, DEC_MATRIX_STRIDE, DEC_COL_MAJOR)))]
        for k, mt in enumerate(members):
            for e in mod.mdecs.get((st_id, k), []):
                if e[0] == DEC_ROW_MAJOR:
                    raise ValueError('RowMajor loose uniform %s' % mod.mnames.get((st_id, k)))
            nt = strip_type(mod, mt, stripc)
            isb = mod.mnames.get((st_id, k), '') in bool_names
            if isb:
                nt = int_twin(mod, nt, twinc)
            vid = mod.new_id()
            pid = ptr_type(mod, SC_UNIFORM_CONSTANT, nt, ptrs)
            mod.add_glob(Inst(OP_VARIABLE, [pid, vid, SC_UNIFORM_CONSTANT]))
            mod.add_name(vid, mod.mnames.get((st_id, k), 'u%d' % k))
            nm_ = mod.mnames.get((st_id, k), 'u%d' % k)
            lc = alloc(nm_, scalar_slots(mod, nt), mod.defn[nt].op == OP_TYPE_ARRAY)
            mod.add_dec(vid, DEC_LOCATION, lc)
            rep['loose'].append((nm_, lc))
            mvar[k] = (vid, nt, isb)
        # a Uniform pointer type -> its UniformConstant twin (pointee stripped)
        up_cache = {}

        def uc_ptr(old_ptr_id, isb=False):
            if (old_ptr_id, isb) not in up_cache:
                pt = mod.defn[old_ptr_id]
                pe = strip_type(mod, pt.a[2], stripc)
                up_cache[(old_ptr_id, isb)] = ptr_type(mod, SC_UNIFORM_CONSTANT, int_twin(mod, pe, twinc) if isb else pe, ptrs)
            return up_cache[(old_ptr_id, isb)]
        bptr = set()      # pointers into a bool uniform (declared int, see int_twin)
        derived = set()   # ids of pointers reached through the block
        newf = []
        for x in mod.funcs:
            if x.op in (OP_ACCESS_CHAIN, OP_IN_BOUNDS_AC) and x.a[2] == lid:
                k = const_value(mod, x.a[3])
                vid, nt, isb = mvar[k]
                rest = x.a[4:]
                derived.add(x.a[1])
                if isb:
                    bptr.add(x.a[1])
                if rest:
                    newf.append(Inst(OP_ACCESS_CHAIN, [uc_ptr(x.a[0], isb), x.a[1], vid] + rest))
                else:
                    newf.append(Inst(OP_ACCESS_CHAIN, [uc_ptr(x.a[0], isb), x.a[1], vid]))
            elif x.op in (OP_ACCESS_CHAIN, OP_IN_BOUNDS_AC) and x.a[2] in derived:
                derived.add(x.a[1])
                if x.a[2] in bptr:
                    bptr.add(x.a[1])
                newf.append(Inst(x.op, [uc_ptr(x.a[0], x.a[2] in bptr)] + x.a[1:]))
            elif x.op == OP_LOAD and x.a[2] in bptr:
                tmp = mod.new_id()
                newf.append(Inst(OP_LOAD, [int_twin(mod, x.a[0], twinc), tmp, x.a[2]] + x.a[3:]))
                newf.append(Inst(OP_BITCAST, [x.a[0], x.a[1], tmp]))
            else:
                bad = (x.op == OP_LOAD and x.a[2] == lid) or (x.op == OP_STORE and x.a[0] in (lid,)) or x.op == 57 and lid in x.a[3:]
                if bad:
                    raise ValueError('loose block used outside an access chain (op %d)' % x.op)
                newf.append(x)
        mod.funcs = newf
        # drop the block variable, its type and its decorations
        mod.glob = [x for x in mod.glob if not (x.op == OP_VARIABLE and x.a[1] == lid)]
        mod.annot = [x for x in mod.annot if not (x.op in (OP_DECORATE, OP_MEMBER_DECORATE) and x.a[0] in (lid,) )]

    # ---- 4. separate texture + sampler -> combined sampler uniform
    images = {}      # var id -> (var inst, image type id, array length or 0)
    samplers = {}    # var id -> (var inst, array length or 0)
    for v in [x for x in mod.glob if x.op == OP_VARIABLE and x.a[2] == SC_UNIFORM_CONSTANT]:
        pt = mod.defn[v.a[0]]
        t = mod.defn[pt.a[2]]
        n = 0
        if t.op == OP_TYPE_ARRAY:
            n, t = const_value(mod, t.a[2]), mod.defn[t.a[1]]
        if t.op == OP_TYPE_IMAGE and t.a[6] == 1:   # Sampled = 1: a texture (2 = a storage image keeps its own form)
            images[v.a[1]] = (v, t.a[0], n)
        elif t.op == OP_TYPE_SAMPLER:
            samplers[v.a[1]] = (v, n)
    if images or samplers:
        defs = {}     # result id -> Inst for the function bodies
        for x in mod.funcs:
            if x.op in (OP_LOAD, OP_ACCESS_CHAIN, OP_IN_BOUNDS_AC, OP_SAMPLED_IMAGE, OP_IMAGE):
                defs[x.a[1]] = x
        def root_of(pid):
            """(variable id, [index ids]) of a pointer produced by loads/access chains"""
            idx = []
            while pid in defs and defs[pid].op in (OP_ACCESS_CHAIN, OP_IN_BOUNDS_AC):
                d = defs[pid]
                idx = d.a[3:] + idx
                pid = d.a[2]
            return pid, idx
        # pairs from the OpSampledImage instructions
        pair_of = {}   # (image var, sampler var or None) -> combined var id
        si_pairs = []
        for x in mod.funcs:
            if x.op == OP_SAMPLED_IMAGE:
                li, ls = defs[x.a[2]], defs[x.a[3]]
                iv, _ = root_of(li.a[2]) if li.op == OP_LOAD else (None, None)
                sv, _ = root_of(ls.a[2]) if ls.op == OP_LOAD else (None, None)
                if iv not in images or sv not in samplers:
                    raise ValueError('a sampled image the patcher cannot trace')
                si_pairs.append((iv, sv))
        used_imgs = set()
        # every image variable becomes a combined sampler variable
        comb = {}     # image var -> {sampler var or None: (combined var id, SampledImage type id, array type or None)}
        for iv, (v, ityp, n) in images.items():
            sis = sorted({sv for (i2, sv) in si_pairs if i2 == iv})
            lst = sis if sis else [None]
            comb[iv] = {}
            for j, sv in enumerate(lst):
                sit = None
                for x in mod.glob:
                    if x.op == OP_TYPE_SAMPLED_IMAGE and x.a[1] == ityp:
                        sit = x.a[0]
                if sit is None:
                    sit = mod.new_id()
                    mod.add_glob(Inst(OP_TYPE_SAMPLED_IMAGE, [sit, ityp]))
                full = sit
                if n:
                    arr_len = mod.defn[mod.defn[v.a[0]].a[2]].a[2]
                    full = mod.new_id()
                    mod.add_glob(Inst(OP_TYPE_ARRAY, [full, sit, arr_len]))
                pid = ptr_type(mod, SC_UNIFORM_CONSTANT, full, ptrs)
                cid = mod.new_id()
                mod.add_glob(Inst(OP_VARIABLE, [pid, cid, SC_UNIFORM_CONSTANT]))
                nm = mod.name_of(iv) or ('tex%d' % iv)
                mod.add_name(cid, nm if j == 0 else '%s_%d' % (nm, j + 1))
                nm2 = nm if j == 0 else '%s_%d' % (nm, j + 1)
                lc = alloc(nm2, n or 1, bool(n))
                mod.add_dec(cid, DEC_LOCATION, lc)
                rep['samplers'].append((nm2, lc, n or 1))
                comb[iv][sv] = (cid, sit, full)
        # rewrite the function bodies
        newf = []
        cl_of = {}     # image load result id -> combined load id (for the sampled-image consumers)
        si_to = {}     # OpSampledImage result id -> combined load id
        for x in mod.funcs:
            if x.op in (OP_ACCESS_CHAIN, OP_IN_BOUNDS_AC) and x.a[2] in images:
                iv = x.a[2]
                (sv0, (cid, sit, full)) = sorted(comb[iv].items(), key=lambda kv: -1 if kv[0] is None else kv[0])[0]
                pid_t = ptr_type(mod, SC_UNIFORM_CONSTANT, sit, ptrs)
                newf.append(Inst(x.op, [pid_t, x.a[1], cid] + x.a[3:]))
                continue
            if x.op in (OP_ACCESS_CHAIN, OP_IN_BOUNDS_AC) and x.a[2] in samplers:
                continue                                   # re-made at the OpSampledImage
            if x.op == OP_LOAD and (x.a[2] in samplers or (x.a[2] in defs and root_of(x.a[2])[0] in samplers)):
                continue                                   # the sampler load goes: the combined load carries it
            if x.op == OP_LOAD:
                src = defs.get(x.a[2]) if x.a[2] in defs else None
                root = root_of(x.a[2])[0] if x.a[2] in defs or x.a[2] in images else None
                if root in images:
                    iv = root
                    (sv0, (cid, sit, full)) = sorted(comb[iv].items(), key=lambda kv: -1 if kv[0] is None else kv[0])[0]
                    ptr = x.a[2] if x.a[2] != iv else cid
                    cl = mod.new_id()
                    newf.append(Inst(OP_LOAD, [sit, cl, ptr]))
                    newf.append(Inst(OP_IMAGE, [x.a[0], x.a[1], cl]))
                    cl_of[x.a[1]] = cl
                    continue
            if x.op == OP_SAMPLED_IMAGE:
                si_to[x.a[1]] = cl_of[x.a[2]]
                continue
            if x.op in SAMPLED_IMAGE_CONSUMERS and x.a[2] in si_to:
                x = Inst(x.op, x.a[:2] + [si_to[x.a[2]]] + x.a[3:])
            newf.append(x)
        mod.funcs = newf
        # drop the old image and sampler variables with their decorations
        dead = set(images) | set(samplers)
        mod.glob = [x for x in mod.glob if not (x.op == OP_VARIABLE and x.a[1] in dead)]
        mod.annot = [x for x in mod.annot if not (x.op == OP_DECORATE and x.a[0] in dead)]

    # ---- 1. whole-array stores of OpConstantNull
    nulls = {x.a[1]: x.a[0] for x in mod.glob if x.op == OP_CONSTANT_NULL}
    newf = []
    fdefs = {}
    for x in mod.funcs:
        if x.op == OP_STORE and x.a[1] in nulls and mod.defn.get(nulls[x.a[1]]) is not None and mod.defn[nulls[x.a[1]]].op == OP_TYPE_ARRAY:
            at = mod.defn[nulls[x.a[1]]]
            n = const_value(mod, at.a[2])
            et = at.a[1]
            nullel = None
            for g in mod.glob:
                if g.op == OP_CONSTANT_NULL and g.a[0] == et:
                    nullel = g.a[1]
            if nullel is None:
                nullel = mod.new_id()
                mod.add_glob(Inst(OP_CONSTANT_NULL, [et, nullel]))
            fp = ptr_type(mod, SC_FUNCTION, et, ptrs)
            u32 = next((g.a[0] for g in mod.glob if g.op == OP_TYPE_INT and g.a[1] == 32 and g.a[2] == 0), None)
            for k in range(n):
                kc = next((g.a[1] for g in mod.glob if g.op == OP_CONSTANT and g.a[0] == u32 and g.a[2] == k), None)
                if kc is None:
                    kc = mod.new_id()
                    mod.add_glob(Inst(OP_CONSTANT, [u32, kc, k]))
                p = mod.new_id()
                newf.append(Inst(OP_ACCESS_CHAIN, [fp, p, x.a[0], kc]))
                newf.append(Inst(OP_STORE, [p, nullel]))
            continue
        newf.append(x)
    mod.funcs = newf
    # ---- names the app needs (uniforms, samplers, blocks) stay; the rest of the debug names go (a smaller .spv)
    keep_ids, keep_struct = set(), set()
    for x in mod.glob:
        if x.op == OP_VARIABLE and x.a[2] == SC_UNIFORM_CONSTANT:
            keep_ids.add(x.a[1])
            pt = mod.defn.get(x.a[0])
            stack = [pt.a[2]] if pt is not None else []
            while stack:   # a struct uniform (textures[0].coordSet): GL names its members from the member names
                tt = mod.defn.get(stack.pop())
                if tt is None:
                    continue
                if tt.op == OP_TYPE_ARRAY:
                    stack.append(tt.a[1])
                elif tt.op == OP_TYPE_STRUCT:
                    keep_struct.add(tt.a[0])
                    stack.extend(tt.a[1:])
        if x.op == OP_VARIABLE and x.a[2] == SC_UNIFORM:
            pt = mod.defn.get(x.a[0])
            if pt is not None:
                keep_ids.add(pt.a[2])
                keep_struct.add(pt.a[2])
    # R6: a stage-interface variable keeps a name of its own (by storage and location). The driver names an unnamed one after its
    # SPIR-V id, and a geometry output and a fragment input with the same id read as one variable of two types (cell_precip_look).
    for x in mod.glob:
        if x.op == OP_VARIABLE and x.a[2] in (SC_INPUT, SC_OUTPUT) and not (stage == EM_VERTEX and x.a[2] == SC_INPUT) \
                and not (stage == 4 and x.a[2] == SC_OUTPUT):
            lc = mod.dec(x.a[1], DEC_LOCATION)
            if lc is not None:
                mod.head = [h for h in mod.head if not (h.op == OP_NAME and h.a[0] == x.a[1])]
                mod.add_name(x.a[1], ('wwi%d' if x.a[2] == SC_INPUT else 'wwo%d') % lc[0])
                keep_ids.add(x.a[1])
    mod.head = [h for h in mod.head if not (h.op in (3, 4, 7, 8) or (h.op == OP_NAME and h.a[0] not in keep_ids)
                                            or (h.op == OP_MEMBER_NAME and h.a[0] not in keep_struct))]
    rep['locations'] = sum(v[1] for v in locs.values())
    return mod.emit(), rep
