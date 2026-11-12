"""A2 interface gate (HLSLBULK1 R5): for every program, the GLSL program (the loader's base variant) and the SPIR-V program
built from the converted HLSL must present the same interface to the app, by NAME: active uniforms (name, type, size),
uniform blocks (name, data size, member names and offsets), vertex attributes (name, location).
  python -I -X utf8 tests/hlsllive/a2_iface.py <tree> <spv dir from a1_sweep> [--only substr] [--plant]
A uniform the GLSL keeps active and the SPIR-V program lacks is 'missing' (the app's glUniform* on it would be silently
dropped: the program must fall back unless the GLSL one is dead); a type/size/offset difference is 'differ'.
--plant renames one uniform in memory: the gate must then report it."""
import ctypes, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(HERE)), 'tests', 'spells'))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import remap, spirv_probe, glctx
from glctx import GLuint, GLint, GLenum, fn
import shader_link_check as slc

# GL sampler / image / buffer-sampler types (the ones whose loss would be a real hole, not a dead uniform)
SAMPLER_TYPES = set(range(0x8B5E, 0x8B63)) | set(range(0x8DC0, 0x8DD0)) | set(range(0x8DD0, 0x8DDA)) | {0x8DC1, 0x8DC2, 0x8DC3, 0x8DC4, 0x8DC5, 0x8DC6, 0x8DC7, 0x8DC8, 0x8DC9, 0x8DCA, 0x8DCB, 0x8DCC, 0x8DCD, 0x8DCE, 0x8DCF, 0x8DD2, 0x8DD3, 0x8DD4, 0x8DD5, 0x8DD6, 0x8DD7, 0x8DD8, 0x8DD9, 0x8DC0, 0x9108, 0x9109, 0x910A, 0x910B, 0x910C, 0x910D, 0x910E, 0x910F, 0x9110, 0x9111, 0x9112, 0x9113, 0x9114, 0x9115, 0x9116, 0x900D, 0x900E, 0x900F, 0x9010, 0x9011}

STG = {'vert': 'vs', 'frag': 'ps', 'geom': 'gs', 'tesc': 'hs', 'tese': 'ds'}
GLTYPE = {'vert': 0x8B31, 'frag': 0x8B30, 'geom': 0x8DD9, 'tesc': 0x8E88, 'tese': 0x8E87}


# R6: the loader's injected defines and the text that makes it inject each one (glcontext.cpp Shader::load; same table as variants.py)
MARKERS = {'WW_FRAME_UBO': 'WW_FRAME_UBO', 'WW_CELL_RTDDGI': '_CELL_RTDDGI', 'WW_CELL_RESTIR': '_CELL_RESTIR',
           'WW_CELL_AIRZONE': '_CELL_AIRZONE', 'WW_CELL_AIR_SKYVIEW': '_CELL_AIR_SKYVIEW'}


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
            if all(os.path.exists(x[1]) for x in st2):
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
    gaa = fn('glGetActiveAttrib', None, GLuint, GLuint, ctypes.c_int, ctypes.POINTER(GLint), ctypes.POINTER(GLint), ctypes.POINTER(GLenum), ctypes.c_char_p)
    gal = fn('glGetAttribLocation', GLint, GLuint, ctypes.c_char_p)
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
    na = GLint(); gpi(p, 0x8B89, ctypes.byref(na))
    attr = {}
    for i in range(na.value):
        nm = ctypes.create_string_buffer(256); ln, sz, ty = GLint(), GLint(), GLenum()
        gaa(p, i, 256, ctypes.byref(ln), ctypes.byref(sz), ctypes.byref(ty), nm)
        if not nm.value.startswith(b'gl_'):
            attr[gal(p, nm.value)] = ty.value     # by location: the GLSL has explicit layout(location=n); the app never asks a name
    return uni, blocku, blocks, attr


def main():
    a = sys.argv[1:]
    tree, spvdir = a[0], a[1]
    only = a[a.index('--only') + 1] if '--only' in a else None
    plant = '--plant' in a
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
