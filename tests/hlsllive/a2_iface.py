"""A2 interface gate (HLSLBULK1 R5): for every program, the GLSL program (the loader's base variant) and the SPIR-V program
built from the converted HLSL must present the same interface to the app, by NAME: active uniforms (name, type, size),
uniform blocks (name, data size, member names and offsets), vertex attributes (name, location).
  python -I -X utf8 tests/hlsllive/a2_iface.py <tree> <spv dir from a1_sweep> [--only substr] [--plant]
A uniform the GLSL keeps active and the SPIR-V program lacks is 'missing' (the app's glUniform* on it would be silently
dropped: the program must fall back unless the GLSL one is dead); a type/size/offset difference is 'differ'.
--plant renames one uniform in memory: the gate must then report it. A loose bool uniform reported GL_BOOL by GLSL and
GL_INT by SPIR-V (spvgl declares it int on purpose) is the same; --plant-type turns one such uniform's SPIR-V type into
GL_FLOAT in memory and the program must then show 'differ'. A GLSL sampler*Shadow against the plain SPIR-V sampler of the
same dimension is the same only when the SPIR-V compares through it (SHADOW_AS_PLAIN); --plant-shadow takes that proof away
from one sampler, --plant-attr turns one SPIR-V attribute into GL_FLOAT, --plant-matcol drops a column of a matrix
attribute the twin declares per column (MAT_COLS): each must show 'differ'."""
import ctypes, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(HERE)), 'tests', 'spells'))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import remap, spirv_probe, glctx, stamps
from glctx import GLuint, GLint, GLenum, fn
import shader_link_check as slc

# GL sampler / image / buffer-sampler types (the ones whose loss would be a real hole, not a dead uniform)
SAMPLER_TYPES = set(range(0x8B5E, 0x8B63)) | set(range(0x8DC0, 0x8DD0)) | set(range(0x8DD0, 0x8DDA)) | {0x8DC1, 0x8DC2, 0x8DC3, 0x8DC4, 0x8DC5, 0x8DC6, 0x8DC7, 0x8DC8, 0x8DC9, 0x8DCA, 0x8DCB, 0x8DCC, 0x8DCD, 0x8DCE, 0x8DCF, 0x8DD2, 0x8DD3, 0x8DD4, 0x8DD5, 0x8DD6, 0x8DD7, 0x8DD8, 0x8DD9, 0x8DC0, 0x9108, 0x9109, 0x910A, 0x910B, 0x910C, 0x910D, 0x910E, 0x910F, 0x9110, 0x9111, 0x9112, 0x9113, 0x9114, 0x9115, 0x9116, 0x900D, 0x900E, 0x900F, 0x9010, 0x9011}

# HLSLSRC1 11-14: a loose bool uniform is declared int on purpose by spvgl (the app sets it with glUniform1i; see
# spvgl.int_twin), so the GLSL program reports GL_BOOL[_VECn] where the SPIR-V one reports GL_INT[_VECn]. Same size: not
# a difference. Loose uniforms only (block members keep their own bool/uint rule); --plant-type proves a real type change
# on such a uniform still differs.
BOOL_AS_INT = {0x8B56: 0x1404, 0x8B57: 0x8B53, 0x8B58: 0x8B54, 0x8B59: 0x8B55}


def same_loose(g, s):
    return g == s or (BOOL_AS_INT.get(g[0]) == s[0] and g[1:] == s[1:])


# HLSLSRC1 11-14: DXC writes every OpTypeImage with Depth 2 (unknown), so NV reports a GLSL sampler*Shadow's twin as the
# plain sampler of the same dimension. The app binds both with glUniform1i(unit) and the depth texture keeps its compare
# mode, so the pair is the same binding only when the SPIR-V really compares through that variable: its image type says
# Depth 2 and some stage feeds a load of it to an OpImage*Dref* instruction (dref_vars). --plant-shadow takes that proof
# away from one such sampler and the program must then show 'differ'.
SHADOW_AS_PLAIN = {0x8B61: 0x8B5D, 0x8B62: 0x8B5E, 0x8DC3: 0x8DC0, 0x8DC4: 0x8DC1, 0x8DC5: 0x8B60, 0x900D: 0x900C, 0x8B64: 0x8B63}
DREF_OPS = {89, 90, 93, 94, 97, 307, 308, 311, 312, 316}

# HLSLSRC1 11-14: a GLSL matCxR vertex attribute at location L is fed by the app one column per location (L..L+C-1); the
# HLSL twin declares the columns as C float<R> inputs (TEXCOORD<L>_<i>, spvgl puts column i at L+i). Same feed when every
# column is there with the column type and the matrix is not an array. --plant-matcol removes one column: must differ.
MAT_COLS = {0x8B5A: (2, 0x8B50), 0x8B5B: (3, 0x8B51), 0x8B5C: (4, 0x8B52)}


def mat_split(sattr, loc, v):
    c = MAT_COLS.get(v[0])
    return bool(c) and v[1] == 1 and all(sattr.get(loc + i) == (c[1], 1) for i in range(c[0]))


def dref_vars(path):
    '''Names of the UniformConstant variables of a SPIR-V module whose image type has Depth 2 and that reach a Dref op.'''
    import struct
    b = open(path, 'rb').read()
    w = struct.unpack('<%dI' % (len(b) // 4), b)
    names, img, simg, ptr, var, val, used = {}, {}, {}, {}, {}, {}, set()
    i = 5
    while i < len(w):
        n, op = w[i] >> 16, w[i] & 0xFFFF
        a = w[i + 1:i + n]
        if op == 5:
            s = b[(i + 2) * 4:(i + n) * 4]
            names[a[0]] = s[:s.index(b'\0')].decode('utf-8', 'replace')
        elif op == 25:
            img[a[0]] = a[3]                                 # Depth operand
        elif op == 27:
            simg[a[0]] = a[1]
        elif op == 32:
            ptr[a[0]] = a[2]
        elif op == 59 and a[2] == 0:
            var[a[1]] = a[0]
        elif op == 61 and a[2] in var:
            val[a[1]] = a[2]
        elif op in (83, 86) and a[2] in val:                 # OpCopyObject, OpSampledImage (image operand)
            val[a[1]] = val[a[2]]
        elif op in DREF_OPS and a[2] in val:
            used.add(val[a[2]])
        i += n
    out = set()
    for v in used:
        t = ptr.get(var[v])
        t = simg.get(t, t)
        if img.get(t) == 2 and v in names:
            out.add(names[v])
    return out


STG = {'vert': 'vs', 'frag': 'ps', 'geom': 'gs', 'tesc': 'hs', 'tese': 'ds'}
GLTYPE = {'vert': 0x8B31, 'frag': 0x8B30, 'geom': 0x8DD9, 'tesc': 0x8E88, 'tese': 0x8E87}


# R6: the loader's injected defines and the text that makes it inject each one (glcontext.cpp Shader::load; same table as variants.py)
MARKERS = {'WW_FRAME_UBO': 'WW_FRAME_UBO', 'WW_CELL_RTDDGI': '_CELL_RTDDGI', 'WW_CELL_RESTIR': '_CELL_RESTIR',
           'WW_CELL_AIRZONE': '_CELL_AIRZONE', 'WW_CELL_AIR_SKYVIEW': '_CELL_AIR_SKYVIEW',
           'WW_CELL_FOGLEAK': '_CELL_FOGLEAK', 'WW_CELL_AIR_BOUNCE': '_CELL_AIR_BOUNCE',
           'WW_CELL_RTDDGI_CASCADES': '_CELL_RTDDGI', 'WW_CELL_RESTIRGI': '_CELL_RESTIRGI'}


def items(tree):
    '''(prog file, stages, defines, key): each program in its base variant, then each variant res/shaders/hlsl/variants.txt lists.'''
    var = {}
    vt = os.path.join(tree, 'res/shaders/hlsl/variants.txt')
    if os.path.exists(vt):
        for ln in open(vt, encoding='utf-8'):
            if ln.strip():
                p, un, spec = ln.rstrip('\n').split('\t')
                var.setdefault(p, []).append((un, dict(x.split('=', 1) for x in spec.split())))
    for pf, st in remap.programs(tree):
        name = os.path.basename(pf)
        yield pf, st, [], name
        for un, tags in var.get(name, []):
            st2 = [(e, p[:-5] + '__' + tags[s + '.' + e] + '.hlsl' if tags.get(s + '.' + e) else p, s) for e, p, s in st]
            if all(stamps.exists(x[1]) for x in st2):
                yield pf, st2, un.split('+'), name + '@' + un


def glsl_program(sdir, files, units, defs=()):
    cs, ss, cm = fn('glCreateShader', GLuint, GLenum), fn('glShaderSource', None, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_char_p), ctypes.c_void_p), fn('glCompileShader', None, GLuint)
    gsi = fn('glGetShaderiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
    cp, at, lk = fn('glCreateProgram', GLuint), fn('glAttachShader', None, GLuint, GLuint), fn('glLinkProgram', None, GLuint)
    gpi = fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
    p = cp()
    for f in files:
        src = slc.expand(os.path.join(sdir, f))
        dd = [d for d in defs if MARKERS[d] in src]
        if dd and src.startswith('#version'):
            e0 = src.find('\n') + 1
            src = src[:e0] + ''.join('#define %s 1\n' % d for d in dd) + src[e0:]
        if f.endswith('frag') and 'NUM_TEXTURE_UNITS' in src:
            src = src.replace('NUM_TEXTURE_UNITS', str(units), 1)
        s = cs(GLTYPE[f.rsplit('.', 1)[-1]])
        b = ctypes.c_char_p(src.encode('latin-1'))
        ss(s, 1, ctypes.byref(b), None)
        cm(s)
        ok = GLint(); gsi(s, 0x8B81, ctypes.byref(ok))
        if not ok.value:
            return None
        at(p, s)
    lk(p)
    ok = GLint(); gpi(p, 0x8B82, ctypes.byref(ok))
    return p if ok.value else None


def iface(p):
    gpi = fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
    gau = fn('glGetActiveUniform', None, GLuint, GLuint, ctypes.c_int, ctypes.POINTER(GLint), ctypes.POINTER(GLint), ctypes.POINTER(GLenum), ctypes.c_char_p)
    gaus = fn('glGetActiveUniformsiv', None, GLuint, ctypes.c_int, ctypes.POINTER(GLuint), GLenum, ctypes.POINTER(GLint))
    gabn = fn('glGetActiveUniformBlockName', None, GLuint, GLuint, ctypes.c_int, ctypes.POINTER(GLint), ctypes.c_char_p)
    gabi = fn('glGetActiveUniformBlockiv', None, GLuint, GLuint, GLenum, ctypes.POINTER(GLint))
    n = GLint(); gpi(p, 0x8B86, ctypes.byref(n))
    uni, blocku = {}, {}
    for i in range(n.value):
        nm = ctypes.create_string_buffer(256); ln, sz, ty = GLint(), GLint(), GLenum()
        gau(p, i, 256, ctypes.byref(ln), ctypes.byref(sz), ctypes.byref(ty), nm)
        idx = GLuint(i); bi, off = GLint(), GLint()
        gaus(p, 1, ctypes.byref(idx), 0x8A3A, ctypes.byref(bi))
        gaus(p, 1, ctypes.byref(idx), 0x8A3B, ctypes.byref(off))
        name = nm.value.decode()
        if name.startswith('gl_'):
            continue
        if bi.value >= 0:
            blocku[name] = (ty.value, sz.value, off.value)
        else:
            uni[name] = (ty.value, sz.value)
    nb = GLint(); gpi(p, 0x8A36, ctypes.byref(nb))
    blocks = {}
    for i in range(nb.value):
        nm = ctypes.create_string_buffer(256); ln, ds = GLint(), GLint()
        gabn(p, i, 256, ctypes.byref(ln), nm)
        gabi(p, i, 0x8A40, ctypes.byref(ds))
        blocks[nm.value.decode()] = ds.value
    # By location: the GLSL has explicit layout(location=n); the app never asks a name. HLSLSRC1 11-14: read the location
    # with the program-interface query, never glGetAttribLocation(name): NV names no SPIR-V vertex input (''), so every
    # SPIR-V attribute came back at one junk location and the dict kept one random type (701 A2 rows were mostly that).
    gpif = fn('glGetProgramInterfaceiv', None, GLuint, GLenum, GLenum, ctypes.POINTER(GLint))
    gpri = fn('glGetProgramResourceiv', None, GLuint, GLenum, GLuint, ctypes.c_int, ctypes.POINTER(GLenum), ctypes.c_int, ctypes.POINTER(GLint), ctypes.POINTER(GLint))
    na = GLint(); gpif(p, 0x92E3, 0x92F5, ctypes.byref(na))          # GL_PROGRAM_INPUT, GL_ACTIVE_RESOURCES
    props = (GLenum * 3)(0x930E, 0x92FA, 0x92FB)                     # GL_LOCATION, GL_TYPE, GL_ARRAY_SIZE
    attr = {}
    for i in range(na.value):
        out = (GLint * 3)(); ln = GLint()
        gpri(p, 0x92E3, i, 3, props, 3, ctypes.byref(ln), out)
        if out[0] >= 0:                                              # builtins (gl_VertexID) have no location
            attr[out[0]] = (out[1], out[2])
    return uni, blocku, blocks, attr


def main():
    a = sys.argv[1:]
    tree, spvdir = a[0], a[1]
    only = a[a.index('--only') + 1] if '--only' in a else None
    plant = '--plant' in a
    plant_type = '--plant-type' in a
    plant_shadow = '--plant-shadow' in a
    plant_attr = '--plant-attr' in a
    plant_matcol = '--plant-matcol' in a
    glctx.context()
    units = GLint(0); glctx.gl.glGetIntegerv(0x8872, ctypes.byref(units))
    units = min(max(units.value, 1), 32) - 2
    sdir = os.path.join(tree, 'res/shaders')
    tot = {'programs': 0, 'compared_uniforms': 0, 'compared_blocks': 0, 'compared_attribs': 0, 'clean': 0, 'missing': 0, 'differ': 0, 'skipped': 0}
    bad = []
    okp = dict(l.rstrip('\n').split('\t', 1) for l in open(os.path.join(spvdir, 'spirv', 'programs.txt')) if '\t' in l)
    okp = {k: v[:2] for k, v in okp.items()}
    for pf, st, vdefs, name in items(tree):
        if '.geom' not in open(os.path.join(tree, pf), encoding='utf-8', errors='replace').read():
            st = [x for x in st if not x[1].endswith('_points_gs.hlsl')]
        if only and only not in name or any(e not in STG for e, _p, _s in st):
            continue
        if okp.get(name) != 'OK':
            tot['skipped'] += 1
            continue
        specs = [(STG[e], os.path.join(spvdir, 'spirv', os.path.basename(pth).rsplit('.', 1)[0] + '.spv')) for e, pth, _s in st]
        if not all(os.path.exists(s[1]) for s in specs):
            tot['skipped'] += 1
            continue
        shaders = None
        for ln in open(os.path.join(tree, pf), encoding='utf-8', errors='replace'):
            if ln.startswith('shaders'):
                shaders = ln.split()[1:]
        byn = remap._byname(tree)
        pg = glsl_program(tree, [byn.get(s, 'res/shaders/' + s) for s in shaders], units, vdefs)
        ps, log = spirv_probe.build(specs)
        if pg is None or ps is None:
            tot['skipped'] += 1
            continue
        g, s = iface(pg), iface(ps)
        s = (s[0], {k.split('.', 1)[-1] if '.' in k and k.split('.', 1)[0] in s[2] else k: v for k, v in s[1].items()}, s[2], s[3])   # the driver names SPIR-V block members block.member
        if plant and s[2]:
            k = sorted(s[2])[0]
            s[2][k] += 4          # a planted block-size fault: the gate must report it
        if plant_type:
            # a planted type change on a bool uniform spvgl declared int (int -> float): the bool/int pair must not hide it
            ks = sorted(k for k, v in g[0].items() if k in s[0] and v[0] in BOOL_AS_INT and same_loose(v, s[0][k]))
            if ks:
                s[0][ks[0]] = (0x1406,) + s[0][ks[0]][1:]
                print('PLANT-TYPE', name, ks[0], 'GL_INT -> GL_FLOAT', flush=True)
        dref = set().union(*(dref_vars(x[1]) for x in specs))
        shadow_ok = lambda k, v: v[0] in SHADOW_AS_PLAIN and k in dref and s[0][k] == (SHADOW_AS_PLAIN[v[0]],) + v[1:]
        if plant_shadow:
            ks = sorted(k for k, v in g[0].items() if k in s[0] and shadow_ok(k, v))
            if ks:
                dref.discard(ks[0])
                print('PLANT-SHADOW', name, ks[0], 'no Dref use', flush=True)
        if plant_attr and s[3]:
            k = sorted(s[3])[0]
            s[3][k] = (0x1406,) + s[3][k][1:]      # a planted attribute type fault (-> float)
            print('PLANT-ATTR', name, k, flush=True)
        if plant_matcol:
            ks = sorted(k for k, v in g[3].items() if k in s[3] and mat_split(s[3], k, v))
            if ks:
                del s[3][ks[0] + MAT_COLS[g[3][ks[0]][0]][0] - 1]      # the matrix's last column gone
                print('PLANT-MATCOL', name, ks[0], flush=True)
        tot['programs'] += 1
        miss, diff, dropped = [], [], []
        for ix, label in ((0, 'uniform'), (1, 'blockmember'), (3, 'attrib')):
            for k, v in g[ix].items():
                tot['compared_' + {0: 'uniforms', 1: 'blocks', 3: 'attribs'}[ix]] += 1
                if k not in s[ix]:
                    if ix in (0, 3) and (ix == 3 or v[0] not in SAMPLER_TYPES):
                        tot['dropped_uniforms'] = tot.get('dropped_uniforms', 0) + 1
                        dropped.append(k)
                        continue
                    miss.append('%s %s' % (label, k))
                elif ix == 0 and (same_loose(v, s[ix][k]) or shadow_ok(k, v)) or ix == 3 and mat_split(s[3], k, v):
                    continue
                elif s[ix][k] != v and not (ix == 1 and s[ix][k][0] == 5125 and v[0] == 35670 and s[ix][k][1:] == v[1:]):   # a bool member is a uint in SPIR-V
                    diff.append('%s %s glsl=%s spv=%s' % (label, k, v, s[ix][k]))
        for k, v in g[2].items():
            tot['compared_blocks'] += 1
            if k not in s[2]:
                miss.append('block ' + k)
            elif s[2][k] != v:
                diff.append('block %s size glsl=%d spv=%d' % (k, v, s[2][k]))
        if dropped:
            print('DROPPED', name, ' '.join(str(x) for x in dropped[:12]), '+%d' % (len(dropped) - 12) if len(dropped) > 12 else '')
        if miss or diff:
            tot['missing'] += len(miss)
            tot['differ'] += len(diff)
            bad.append(name)
            print('IFACE', name, 'missing', len(miss), 'differ', len(diff), '|', '; '.join((miss + diff)[:6]), flush=True)
        else:
            tot['clean'] += 1
    print('A2', tot, 'programs with a difference:', len(bad))


if __name__ == "__main__":
    main()
