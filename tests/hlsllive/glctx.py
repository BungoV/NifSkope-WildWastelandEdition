"""Hidden 4.1 core GL context through the real driver, shared by the hlsllive tools (HLSLBULK1 round 5).
Same context as tests/spells/shader_link_check.py: no NifSkope window, nothing shown."""
import ctypes, ctypes.wintypes as W

u32, gdi, gl = ctypes.windll.user32, ctypes.windll.gdi32, ctypes.windll.opengl32
u32.CreateWindowExW.restype = W.HWND
u32.CreateWindowExW.argtypes = [W.DWORD, W.LPCWSTR, W.LPCWSTR, W.DWORD, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                               ctypes.c_int, W.HWND, W.HMENU, W.HINSTANCE, W.LPVOID]
u32.GetDC.restype = W.HDC; u32.GetDC.argtypes = [W.HWND]
gl.wglCreateContext.restype = ctypes.c_void_p; gl.wglCreateContext.argtypes = [W.HDC]
gl.wglMakeCurrent.argtypes = [W.HDC, ctypes.c_void_p]
gl.wglGetProcAddress.restype = ctypes.c_void_p; gl.wglGetProcAddress.argtypes = [ctypes.c_char_p]
gl.glGetString.restype = ctypes.c_char_p
gl.glGetIntegerv.argtypes = [ctypes.c_uint, ctypes.c_void_p]

GLuint, GLint, GLenum, GLsizei = ctypes.c_uint, ctypes.c_int, ctypes.c_uint, ctypes.c_int


class PFD(ctypes.Structure):
    _fields_ = [('nSize', W.WORD), ('nVersion', W.WORD), ('dwFlags', W.DWORD), ('iPixelType', ctypes.c_ubyte),
                ('cColorBits', ctypes.c_ubyte)] + [('r%d' % i, ctypes.c_ubyte) for i in range(13)] + \
               [('cDepthBits', ctypes.c_ubyte), ('cStencilBits', ctypes.c_ubyte), ('cAuxBuffers', ctypes.c_ubyte),
                ('iLayerType', ctypes.c_ubyte), ('bReserved', ctypes.c_ubyte), ('dwLayerMask', W.DWORD),
                ('dwVisibleMask', W.DWORD), ('dwDamageMask', W.DWORD)]


def context(major=4, minor=1):
    hwnd = u32.CreateWindowExW(0, 'STATIC', 'hlsllive', 0, 0, 0, 16, 16, None, None, None, None)
    hdc = u32.GetDC(hwnd)
    pfd = PFD(); pfd.nSize = ctypes.sizeof(PFD); pfd.nVersion = 1; pfd.dwFlags = 0x4 | 0x20 | 0x1
    pfd.cColorBits = 32; pfd.cDepthBits = 24
    gdi.ChoosePixelFormat.argtypes = [W.HDC, ctypes.POINTER(PFD)]; gdi.SetPixelFormat.argtypes = [W.HDC, ctypes.c_int, ctypes.POINTER(PFD)]
    gdi.SetPixelFormat(hdc, gdi.ChoosePixelFormat(hdc, ctypes.byref(pfd)), ctypes.byref(pfd))
    tmp = gl.wglCreateContext(hdc); gl.wglMakeCurrent(hdc, tmp)
    cca = ctypes.WINFUNCTYPE(ctypes.c_void_p, W.HDC, ctypes.c_void_p, ctypes.POINTER(ctypes.c_int))(
        gl.wglGetProcAddress(b'wglCreateContextAttribsARB'))
    attrs = (ctypes.c_int * 9)(0x2091, major, 0x2092, minor, 0x9126, 0x1, 0, 0, 0)
    ctx = cca(hdc, None, attrs)
    gl.wglMakeCurrent(hdc, ctx)
    return hdc


_cache = {}


def fn(name, res, *args):
    k = (name, res, args)
    if k not in _cache:
        p = gl.wglGetProcAddress(name.encode())
        if not p:
            p = getattr(gl, name, None)
            if p is None:
                raise RuntimeError('no GL function ' + name)
            p = ctypes.cast(p, ctypes.c_void_p).value
        _cache[k] = ctypes.WINFUNCTYPE(res, *args)(p)
    return _cache[k]


def extensions():
    n = GLint(); gl.glGetIntegerv(0x821D, ctypes.byref(n))   # GL_NUM_EXTENSIONS
    gs = fn('glGetStringi', ctypes.c_char_p, GLenum, GLuint)
    return [gs(0x1F03, i).decode() for i in range(n.value)]
