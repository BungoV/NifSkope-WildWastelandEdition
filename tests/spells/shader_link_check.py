"""Offline shader link check (lane GIBLACKLINK1): compile + link a NifSkope .prog through the real GL driver in a
hidden window, the loader's own #include expansion and the defines glcontext.cpp injects. Runs no NifSkope.

    python -I tests/spells/shader_link_check.py <shaders dir> <prog name> [--define NAME[=V]]... [--dump out.glsl] [--save-binary out.bin]

Prints one verdict line per program: LINK OK / COMPILE FAIL / LINK FAIL, plus the driver log's first lines.
Exit 0 only when it linked. WW_CELL_GI_MARCH is the define WW_CELL_GI_VIS=march (or a march PV red) injects.
"""
import ctypes, ctypes.wintypes as W, os, re, sys

def expand(path, depth=0):
    with open(path, 'rb') as f:
        data = f.read().decode('latin-1')
    if depth > 0 and data.startswith('#version'):
        data = data[data.find('\n') + 1:]
    out = []
    for line in data.split('\n'):
        m = re.match(r'^[ \t]*#[ \t]*include[ \t]*"([^"]+)"[ \t\r]*$', line)
        if m:
            out.append(expand(os.path.join(os.path.dirname(path), m.group(1)), depth + 1))
        else:
            out.append(line)
    return '\n'.join(out)

def inject(src, defines):
    if not src.startswith('#version'):
        return src
    eol = src.find('\n') + 1
    return src[:eol] + ''.join('#define %s %s\n' % (d.split('=')[0], d.split('=')[1] if '=' in d else '1') for d in defines) + src[eol:]

u32, gdi, gl = ctypes.windll.user32, ctypes.windll.gdi32, ctypes.windll.opengl32
u32.CreateWindowExW.restype = W.HWND
u32.CreateWindowExW.argtypes = [W.DWORD, W.LPCWSTR, W.LPCWSTR, W.DWORD, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                               ctypes.c_int, W.HWND, W.HMENU, W.HINSTANCE, W.LPVOID]
u32.GetDC.restype = W.HDC; u32.GetDC.argtypes = [W.HWND]
gl.wglCreateContext.restype = ctypes.c_void_p; gl.wglCreateContext.argtypes = [W.HDC]
gl.wglMakeCurrent.argtypes = [W.HDC, ctypes.c_void_p]
gl.wglGetProcAddress.restype = ctypes.c_void_p; gl.wglGetProcAddress.argtypes = [ctypes.c_char_p]
gl.glGetString.restype = ctypes.c_char_p

class PFD(ctypes.Structure):
    _fields_ = [('nSize', W.WORD), ('nVersion', W.WORD), ('dwFlags', W.DWORD), ('iPixelType', ctypes.c_ubyte),
                ('cColorBits', ctypes.c_ubyte)] + [('r%d' % i, ctypes.c_ubyte) for i in range(13)] + \
               [('cDepthBits', ctypes.c_ubyte), ('cStencilBits', ctypes.c_ubyte), ('cAuxBuffers', ctypes.c_ubyte),
                ('iLayerType', ctypes.c_ubyte), ('bReserved', ctypes.c_ubyte), ('dwLayerMask', W.DWORD),
                ('dwVisibleMask', W.DWORD), ('dwDamageMask', W.DWORD)]

def context():
    hwnd = u32.CreateWindowExW(0, 'STATIC', 'shaderlink', 0, 0, 0, 16, 16, None, None, None, None)  # never shown
    hdc = u32.GetDC(hwnd)
    pfd = PFD(); pfd.nSize = ctypes.sizeof(PFD); pfd.nVersion = 1; pfd.dwFlags = 0x4 | 0x20 | 0x1
    pfd.cColorBits = 32; pfd.cDepthBits = 24
    gdi.ChoosePixelFormat.argtypes = [W.HDC, ctypes.POINTER(PFD)]; gdi.SetPixelFormat.argtypes = [W.HDC, ctypes.c_int, ctypes.POINTER(PFD)]
    gdi.SetPixelFormat(hdc, gdi.ChoosePixelFormat(hdc, ctypes.byref(pfd)), ctypes.byref(pfd))
    tmp = gl.wglCreateContext(hdc); gl.wglMakeCurrent(hdc, tmp)
    cca = ctypes.WINFUNCTYPE(ctypes.c_void_p, W.HDC, ctypes.c_void_p, ctypes.POINTER(ctypes.c_int))(
        gl.wglGetProcAddress(b'wglCreateContextAttribsARB'))
    attrs = (ctypes.c_int * 9)(0x2091, 4, 0x2092, 1, 0x9126, 0x1, 0, 0, 0)   # 4.1 core, as glview.cpp's CoreProfile
    ctx = cca(hdc, None, attrs)
    gl.wglMakeCurrent(hdc, ctx)
    return hdc

def fn(name, res, *args):
    return ctypes.WINFUNCTYPE(res, *args)(gl.wglGetProcAddress(name.encode()))

def main():
    a = sys.argv[1:]
    sdir, prog = a[0], a[1]
    defines = [a[i + 1] for i in range(len(a)) if a[i] == '--define']
    dump = a[a.index('--dump') + 1] if '--dump' in a else None
    save = a[a.index('--save-binary') + 1] if '--save-binary' in a else None
    files = None
    for line in open(os.path.join(sdir, prog)):
        if line.startswith('shaders'):
            files = line.split()[1:]
    context()
    print('driver:', gl.glGetString(0x1F01).decode(), '|', gl.glGetString(0x1F02).decode())
    GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
    createShader = fn('glCreateShader', GLuint, GLenum)
    shaderSource = fn('glShaderSource', None, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_char_p), ctypes.c_void_p)
    compileShader = fn('glCompileShader', None, GLuint)
    getShaderiv = fn('glGetShaderiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
    getShaderInfoLog = fn('glGetShaderInfoLog', None, GLuint, ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p)
    createProgram = fn('glCreateProgram', GLuint)
    attach = fn('glAttachShader', None, GLuint, GLuint)
    link = fn('glLinkProgram', None, GLuint)
    getProgramiv = fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint))
    getProgramInfoLog = fn('glGetProgramInfoLog', None, GLuint, ctypes.c_int, ctypes.c_void_p, ctypes.c_char_p)
    p = createProgram()
    for f in files:
        src = inject(expand(os.path.join(sdir, f)), defines)
        if dump and f.endswith('frag'):
            open(dump, 'w').write(src)
        s = createShader({'frag': 0x8B30, 'geom': 0x8DD9}.get(f.rsplit('.', 1)[-1], 0x8B31))
        b = ctypes.c_char_p(src.encode('latin-1'))
        shaderSource(s, 1, ctypes.byref(b), None)
        compileShader(s)
        ok = GLint(); getShaderiv(s, 0x8B81, ctypes.byref(ok))
        if not ok.value:
            log = ctypes.create_string_buffer(1 << 16); getShaderInfoLog(s, 1 << 16, None, log)
            print('COMPILE FAIL', prog, f, defines); print(log.value.decode(errors='replace')[:1500]); return 2
        attach(p, s)
    link(p)
    ok = GLint(); getProgramiv(p, 0x8B82, ctypes.byref(ok))
    log = ctypes.create_string_buffer(1 << 16); getProgramInfoLog(p, 1 << 16, None, log)
    txt = log.value.decode(errors='replace').strip()
    blen = GLint(0)
    if ok.value:
        getProgramiv(p, 0x8741, ctypes.byref(blen))   # GL_PROGRAM_BINARY_LENGTH: the size metric between variants
        if save:   # the driver's binary: equal bytes before/after an edit = the same machine code, the same pixels
            buf = ctypes.create_string_buffer(blen.value); got = GLint(); fmt = ctypes.c_uint()
            fn('glGetProgramBinary', None, GLuint, ctypes.c_int, ctypes.POINTER(GLint), ctypes.POINTER(ctypes.c_uint),
               ctypes.c_void_p)(p, blen.value, ctypes.byref(got), ctypes.byref(fmt), buf)
            open(save, 'wb').write(buf.raw[:got.value])
    print(('LINK OK' if ok.value else 'LINK FAIL'), prog, defines, 'binary %d bytes' % blen.value if ok.value else '')
    if txt:
        print(txt[:1500])
    return 0 if ok.value else 1

if __name__ == '__main__':
    sys.exit(main())
