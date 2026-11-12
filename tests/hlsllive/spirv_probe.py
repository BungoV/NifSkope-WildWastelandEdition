"""A0 probe (HLSLBULK1 round 5): does the driver take SPIR-V (GL_ARB_gl_spirv) in the app's 4.1 core context, and does a
set of .spv files link as one program?
    python -I tests/hlsllive/spirv_probe.py vs:a_vs.spv [gs:a_gs.spv] ps:a_ps.spv
Prints the extension lines, each stage's specialize status and the link log, then the active uniforms and blocks."""
import ctypes, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from glctx import *

STAGE = {'vs': 0x8B31, 'ps': 0x8B30, 'gs': 0x8DD9, 'hs': 0x8E88, 'ds': 0x8E87, 'cs': 0x91B9}


def build(specs):
    """specs: [(stage, spv path)] -> (program id, log); raises on a stage that does not specialize"""
    create, shbin = fn('glCreateShader', GLuint, GLenum), fn('glShaderBinary', None, GLsizei, ctypes.c_void_p, GLenum, ctypes.c_void_p, GLsizei)
    spec = fn('glSpecializeShader', None, GLuint, ctypes.c_char_p, GLuint, ctypes.c_void_p, ctypes.c_void_p)
    getsh = fn('glGetShaderiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
    shlog = fn('glGetShaderInfoLog', None, GLuint, GLsizei, ctypes.c_void_p, ctypes.c_char_p)
    cp, att, link = fn('glCreateProgram', GLuint), fn('glAttachShader', None, GLuint, GLuint), fn('glLinkProgram', None, GLuint)
    getp = fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
    plog = fn('glGetProgramInfoLog', None, GLuint, GLsizei, ctypes.c_void_p, ctypes.c_char_p)
    p = cp()
    for st, path in specs:
        data = open(path, 'rb').read()
        s = create(STAGE[st])
        sid = GLuint(s)
        shbin(1, ctypes.byref(sid), 0x9551, data, len(data))   # GL_SHADER_BINARY_FORMAT_SPIR_V
        spec(s, b'main', 0, None, None)
        ok = GLint(); getsh(s, 0x8B81, ctypes.byref(ok))
        if not ok.value:
            lg = ctypes.create_string_buffer(1 << 16); shlog(s, 1 << 16, None, lg)
            return None, 'SPECIALIZE FAIL %s %s: %s' % (st, path, lg.value.decode(errors='replace')[:1500])
        att(p, s)
    link(p)
    ok = GLint(); getp(p, 0x8B82, ctypes.byref(ok))
    lg = ctypes.create_string_buffer(1 << 16); plog(p, 1 << 16, None, lg)
    return (p if ok.value else None), lg.value.decode(errors='replace').strip()


def main():
    specs = [tuple(a.split(':', 1)) for a in sys.argv[1:] if ':' in a]
    context()
    print('driver:', gl.glGetString(0x1F01).decode(), '|', gl.glGetString(0x1F02).decode(), '|', gl.glGetString(0x1F03 - 0x1F03 + 0x1F02).decode())
    ex = extensions()
    for e in ('GL_ARB_gl_spirv', 'GL_ARB_spirv_extensions', 'GL_ARB_separate_shader_objects'):
        print(e, e in ex)
    p, log = build(specs)
    print('LINK OK' if p else 'LINK FAIL', log[:1500])
    if not p:
        return 1
    getp = fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
    for what, tok in (('uniforms', 0x8B86), ('blocks', 0x8A36), ('attribs', 0x8B89)):
        n = GLint(); getp(p, tok, ctypes.byref(n))
        print(what, n.value)
    return 0


if __name__ == '__main__':
    sys.exit(main())
