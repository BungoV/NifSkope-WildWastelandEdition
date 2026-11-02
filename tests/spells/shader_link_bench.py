"""Cold link bench (lane COLDLINK1): link every NifSkope .prog through the real GL driver with N worker threads, each
with its own hidden-window context SHARED with one main context (what a threaded link in glcontext.cpp would do).
Runs no NifSkope. A per-run nonce define makes every source new to the driver's disk cache, so every arm is COLD.

    python -I shader_link_bench.py <shaders dir> --out <dir> [--workers 1,2,4,8] [--procs 0] [--set base|rtx]
                                   [--only N] [--savebin] [--minfree GB] [--repeat 1]

Each arm runs in its own child process (clean driver state, its own peak memory). --procs P adds an arm of P separate
processes with one worker each (tells a per-process driver lock from a machine-wide one). Prints one ARM line an arm
and writes <out>/arm_<tag>.tsv (program, worker, seconds, ok, binary bytes) and <out>/summary.txt.
Loader-faithful sources: #include expansion, WW_FRAME_UBO only into files that mention it (glcontext.cpp Shader::load),
NUM_TEXTURE_UNITS in a .frag; --set rtx adds WW_CELL_RTDDGI (+ WW_CELL_RESTIR) where the files mention them.
A child stops taking programs when free RAM falls under --minfree GB (ABORT line; default 6).
"""
import ctypes, ctypes.wintypes as W, os, re, subprocess, sys, threading, time, random

# ---------------------------------------------------------------- sources (the loader's view)
def expand(path, depth=0):
    with open(path, 'rb') as f:
        data = f.read().decode('latin-1')
    if depth > 0 and data.startswith('#version'):
        data = data[data.find('\n') + 1:]
    out = []
    for line in data.split('\n'):
        m = re.match(r'^[ \t]*#[ \t]*include[ \t]*"([^"]+)"[ \t\r]*$', line)
        out.append(expand(os.path.join(os.path.dirname(path), m.group(1)), depth + 1) if m else line)
    return '\n'.join(out)

def put_define(src, name):
    eol = src.find('\n') + 1
    return src[:eol] + '#define %s 1\n' % name + src[eol:]

def load_programs(sdir, dset, nonce, units):
    progs = []
    for name in sorted(n for n in os.listdir(sdir) if n.lower().endswith('.prog')):
        files = None
        for line in open(os.path.join(sdir, name), encoding='latin-1'):
            if line.startswith('shaders'):
                files = line.split()[1:]
        if not files:
            continue
        stages = []
        for f in files:
            src = expand(os.path.join(sdir, f))
            if src.startswith('#version'):
                if 'WW_FRAME_UBO' in src:
                    src = put_define(src, 'WW_FRAME_UBO')
                if dset == 'rtx' and '_CELL_RTDDGI' in src:
                    src = put_define(src, 'WW_CELL_RTDDGI')
                if dset == 'rtx' and '_CELL_RESTIR' in src:
                    src = put_define(src, 'WW_CELL_RESTIR')
                src = put_define(src, 'WW_COLDLINK_NONCE_%s' % nonce)   # cold: a source the driver cache never saw
            if f.endswith('frag') and 'NUM_TEXTURE_UNITS' in src:
                src = src.replace('NUM_TEXTURE_UNITS', str(units), 1)
            kind = {'frag': 0x8B30, 'geom': 0x8DD9, 'tesc': 0x8E88, 'tese': 0x8E87}.get(f.rsplit('.', 1)[-1], 0x8B31)
            stages.append((f, kind, src.encode('latin-1')))
        progs.append((name, stages, sum(len(s[2]) for s in stages)))
    progs.sort(key=lambda p: -p[2])   # biggest source first: a cost order a loader can know before linking
    return progs

# ---------------------------------------------------------------- WGL
u32, gdi, gl, k32 = ctypes.windll.user32, ctypes.windll.gdi32, ctypes.windll.opengl32, ctypes.windll.kernel32
u32.CreateWindowExW.restype = W.HWND
u32.CreateWindowExW.argtypes = [W.DWORD, W.LPCWSTR, W.LPCWSTR, W.DWORD, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                               ctypes.c_int, W.HWND, W.HMENU, W.HINSTANCE, W.LPVOID]
u32.GetDC.restype = W.HDC; u32.GetDC.argtypes = [W.HWND]
gl.wglCreateContext.restype = ctypes.c_void_p; gl.wglCreateContext.argtypes = [W.HDC]
gl.wglMakeCurrent.argtypes = [W.HDC, ctypes.c_void_p]; gl.wglMakeCurrent.restype = W.BOOL
gl.wglDeleteContext.argtypes = [ctypes.c_void_p]
gl.wglGetProcAddress.restype = ctypes.c_void_p; gl.wglGetProcAddress.argtypes = [ctypes.c_char_p]
gl.glGetString.restype = ctypes.c_char_p
gl.glGetIntegerv.argtypes = [ctypes.c_uint, ctypes.c_void_p]

class PFD(ctypes.Structure):
    _fields_ = [('nSize', W.WORD), ('nVersion', W.WORD), ('dwFlags', W.DWORD), ('iPixelType', ctypes.c_ubyte),
                ('cColorBits', ctypes.c_ubyte)] + [('r%d' % i, ctypes.c_ubyte) for i in range(13)] + \
               [('cDepthBits', ctypes.c_ubyte), ('cStencilBits', ctypes.c_ubyte), ('cAuxBuffers', ctypes.c_ubyte),
                ('iLayerType', ctypes.c_ubyte), ('bReserved', ctypes.c_ubyte), ('dwLayerMask', W.DWORD),
                ('dwVisibleMask', W.DWORD), ('dwDamageMask', W.DWORD)]
gdi.ChoosePixelFormat.argtypes = [W.HDC, ctypes.POINTER(PFD)]
gdi.SetPixelFormat.argtypes = [W.HDC, ctypes.c_int, ctypes.POINTER(PFD)]

def hidden_dc():
    hwnd = u32.CreateWindowExW(0, 'STATIC', 'coldlink', 0, 0, 0, 16, 16, None, None, None, None)   # never shown
    hdc = u32.GetDC(hwnd)
    pfd = PFD(); pfd.nSize = ctypes.sizeof(PFD); pfd.nVersion = 1; pfd.dwFlags = 0x4 | 0x20 | 0x1
    pfd.cColorBits = 32; pfd.cDepthBits = 24
    gdi.SetPixelFormat(hdc, gdi.ChoosePixelFormat(hdc, ctypes.byref(pfd)), ctypes.byref(pfd))
    return hdc

ATTRS = (ctypes.c_int * 9)(0x2091, 4, 0x2092, 1, 0x9126, 0x1, 0, 0, 0)   # 4.1 core, as glview.cpp's CoreProfile
CCA = None

def main_context():
    global CCA
    hdc = hidden_dc()
    tmp = gl.wglCreateContext(hdc); gl.wglMakeCurrent(hdc, tmp)
    CCA = ctypes.WINFUNCTYPE(ctypes.c_void_p, W.HDC, ctypes.c_void_p, ctypes.POINTER(ctypes.c_int))(
        gl.wglGetProcAddress(b'wglCreateContextAttribsARB'))
    ctx = CCA(hdc, None, ATTRS)
    gl.wglMakeCurrent(hdc, ctx); gl.wglDeleteContext(tmp)
    return hdc, ctx

def fn(name, res, *args):
    return ctypes.WINFUNCTYPE(res, *args)(gl.wglGetProcAddress(name.encode()))

class MEMSTAT(ctypes.Structure):
    _fields_ = [('dwLength', W.DWORD), ('dwMemoryLoad', W.DWORD), ('ullTotalPhys', ctypes.c_ulonglong),
                ('ullAvailPhys', ctypes.c_ulonglong), ('ullTotalPageFile', ctypes.c_ulonglong),
                ('ullAvailPageFile', ctypes.c_ulonglong), ('ullTotalVirtual', ctypes.c_ulonglong),
                ('ullAvailVirtual', ctypes.c_ulonglong), ('ullAvailExtendedVirtual', ctypes.c_ulonglong)]

def free_gb():
    m = MEMSTAT(); m.dwLength = ctypes.sizeof(MEMSTAT); k32.GlobalMemoryStatusEx(ctypes.byref(m))
    return m.ullAvailPhys / 2**30

class PMC(ctypes.Structure):
    _fields_ = [('cb', W.DWORD), ('PageFaultCount', W.DWORD)] + [(n, ctypes.c_size_t) for n in (
        'PeakWorkingSetSize', 'WorkingSetSize', 'QuotaPeakPagedPoolUsage', 'QuotaPagedPoolUsage',
        'QuotaPeakNonPagedPoolUsage', 'QuotaNonPagedPoolUsage', 'PagefileUsage', 'PeakPagefileUsage', 'PrivateUsage')]

def peak_private_gb():
    p = PMC(); p.cb = ctypes.sizeof(PMC)
    k32.GetCurrentProcess.restype = W.HANDLE
    ctypes.windll.psapi.GetProcessMemoryInfo.argtypes = [W.HANDLE, ctypes.c_void_p, W.DWORD]
    ctypes.windll.psapi.GetProcessMemoryInfo(k32.GetCurrentProcess(), ctypes.byref(p), p.cb)
    return p.PeakPagefileUsage / 2**30

# ---------------------------------------------------------------- one arm (child process)
def arm(sdir, out, workers, dset, only, savebin, minfree, tag, slice_k, slice_n):
    t_start = time.perf_counter()
    mhdc, mctx = main_context()
    driver = '%s | %s' % (gl.glGetString(0x1F01).decode(), gl.glGetString(0x1F02).decode())
    units = ctypes.c_int(0); gl.glGetIntegerv(0x8872, ctypes.byref(units)); units = min(max(units.value, 1), 32) - 2
    GLuint, GLint, GLenum = ctypes.c_uint, ctypes.c_int, ctypes.c_uint
    F = dict(
        createShader=fn('glCreateShader', GLuint, GLenum),
        shaderSource=fn('glShaderSource', None, GLuint, ctypes.c_int, ctypes.POINTER(ctypes.c_char_p), ctypes.c_void_p),
        compileShader=fn('glCompileShader', None, GLuint),
        getShaderiv=fn('glGetShaderiv', None, GLuint, GLenum, ctypes.POINTER(GLint)),
        deleteShader=fn('glDeleteShader', None, GLuint),
        createProgram=fn('glCreateProgram', GLuint),
        attach=fn('glAttachShader', None, GLuint, GLuint),
        param=fn('glProgramParameteri', None, GLuint, GLenum, GLint),
        link=fn('glLinkProgram', None, GLuint),
        getProgramiv=fn('glGetProgramiv', None, GLuint, GLenum, ctypes.POINTER(GLint)),
        getBinary=fn('glGetProgramBinary', None, GLuint, ctypes.c_int, ctypes.POINTER(GLint),
                     ctypes.POINTER(ctypes.c_uint), ctypes.c_void_p),
        isProgram=fn('glIsProgram', ctypes.c_ubyte, GLuint),
        deleteProgram=fn('glDeleteProgram', None, GLuint))
    nonce = '%016x' % random.getrandbits(64)
    progs = load_programs(sdir, dset, nonce, units)
    if slice_n > 1:
        progs = progs[slice_k::slice_n]
    if only:   # N biggest; -N = the N smallest (a cheap smoke run)
        progs = progs[:only] if only > 0 else progs[only:]
    t_prep = time.perf_counter()
    # worker contexts: created on this thread (as QOffscreenSurface does on Windows), shared with the main context
    wctx = []
    for i in range(workers):
        hdc = hidden_dc()
        c = CCA(hdc, ctypes.c_void_p(mctx), ATTRS)
        if not c:
            print('ABORT shared context %d not created' % i); return 3
        wctx.append((hdc, c))
    gl.wglMakeCurrent(None, None)   # the main context stays free while the workers link (as the GUI thread would)
    t_ctx = time.perf_counter()
    rows, lock, nxt, stop = [], threading.Lock(), [0], [None]
    def work(wi):
        hdc, c = wctx[wi]
        if not gl.wglMakeCurrent(hdc, c):
            stop[0] = 'worker %d MakeCurrent failed' % wi; return
        while True:
            with lock:
                if stop[0] or nxt[0] >= len(progs):
                    break
                if free_gb() < minfree:
                    stop[0] = 'free RAM %.1f GB under %.1f' % (free_gb(), minfree); break
                name, stages, _ = progs[nxt[0]]; nxt[0] += 1
            t0 = time.perf_counter()
            p = F['createProgram'](); ok = GLint(1); shs = []
            for f, kind, src in stages:
                s = F['createShader'](kind); b = ctypes.c_char_p(src)
                F['shaderSource'](s, 1, ctypes.byref(b), None); F['compileShader'](s)
                cs = GLint(); F['getShaderiv'](s, 0x8B81, ctypes.byref(cs))
                if not cs.value:
                    ok = GLint(0)
                F['attach'](p, s); shs.append(s)
            blen = GLint(0); got = GLint(0)
            if ok.value:
                F['param'](p, 0x8257, 1)   # GL_PROGRAM_BINARY_RETRIEVABLE_HINT, as Program::load
                F['link'](p)
                F['getProgramiv'](p, 0x8B82, ctypes.byref(ok))   # waits for the link
                if ok.value:   # the cache write's read-back is part of a cold start (finishLink)
                    F['getProgramiv'](p, 0x8741, ctypes.byref(blen))
                    buf = ctypes.create_string_buffer(max(blen.value, 1)); fmt = ctypes.c_uint()
                    F['getBinary'](p, blen.value, ctypes.byref(got), ctypes.byref(fmt), buf)
                    if savebin:
                        with open(os.path.join(out, 'bin_' + tag, name + '.bin'), 'wb') as fb:
                            fb.write(buf.raw[:got.value])
            for s in shs:
                F['deleteShader'](s)
            dt = time.perf_counter() - t0
            with lock:
                rows.append((name, wi, dt, ok.value, got.value, p, time.perf_counter() - t_ctx))
        gl.wglMakeCurrent(None, None)
    if savebin:
        os.makedirs(os.path.join(out, 'bin_' + tag), exist_ok=True)
    th = [threading.Thread(target=work, args=(i,)) for i in range(workers)]
    for t in th: t.start()
    for t in th: t.join()
    t_link = time.perf_counter()
    gl.wglMakeCurrent(mhdc, mctx)
    adopted = sum(1 for r in rows if r[3] and F['isProgram'](r[5]))   # the main context sees the workers' programs
    for r in rows:
        F['deleteProgram'](r[5])
    with open(os.path.join(out, 'arm_%s.tsv' % tag), 'w') as fo:
        fo.write('program\tworker\tseconds\tok\tbinary\tdone_at\n')
        for r in sorted(rows, key=lambda r: r[6]):
            fo.write('%s\t%d\t%.3f\t%d\t%d\t%.3f\n' % (r[0], r[1], r[2], r[3], r[4], r[6]))
    okn = sum(1 for r in rows if r[3]); big = max(rows, key=lambda r: r[2]) if rows else ('-', 0, 0)
    print('ARM %s workers=%d programs=%d linked=%d adopted=%d wall=%.1fs (prep %.1f ctx %.2f link %.1f) sum=%.1fs '
          'max=%.1fs %s peak_private=%.2fGB free_end=%.1fGB driver=%s%s' % (
              tag, workers, len(rows), okn, adopted, t_link - t_start, t_prep - t_start, t_ctx - t_prep,
              t_link - t_ctx, sum(r[2] for r in rows), big[2], big[0], peak_private_gb(), free_gb(), driver,
              (' ABORT ' + stop[0]) if stop[0] else ''), flush=True)
    return 0 if (not stop[0] and okn == len(progs) == adopted) else 1

# ---------------------------------------------------------------- parent
def main():
    a = sys.argv[1:]
    def opt(k, d):
        return a[a.index(k) + 1] if k in a else d
    sdir, out = os.path.abspath(a[0]), os.path.abspath(opt('--out', 'coldlink_out'))
    dset, only, minfree = opt('--set', 'base'), int(opt('--only', '0')), float(opt('--minfree', '6'))
    savebin = '--savebin' in a
    if '--arm' in a:   # child
        k, n = (int(x) for x in opt('--slice', '0/1').split('/'))
        sys.exit(arm(sdir, out, int(opt('--arm', '1')), dset, only, savebin, minfree, opt('--tag', 'x'), k, n))
    os.makedirs(out, exist_ok=True)
    workers = [int(x) for x in opt('--workers', '1,2,4,8').split(',') if x]
    procs, repeat = int(opt('--procs', '0')), int(opt('--repeat', '1'))
    me = os.path.abspath(__file__)
    base = [sys.executable, '-I', me, sdir, '--out', out, '--set', dset, '--only', str(only), '--minfree', str(minfree)]
    if savebin:
        base.append('--savebin')
    lines = []
    for r in range(repeat):
        for w in workers:
            tag = 'w%d_r%d' % (w, r)
            res = subprocess.run(base + ['--arm', str(w), '--tag', tag], capture_output=True, text=True)
            line = (res.stdout.strip().splitlines() or ['ARM %s rc=%d %s' % (tag, res.returncode, res.stderr[-400:])])[-1]
            print(line, flush=True); lines.append(line)
            if 'ABORT' in line or res.returncode not in (0, 1):
                print('STOP', res.stderr[-800:]); break
        if procs > 1:
            tag = 'p%d_r%d' % (procs, r); t0 = time.perf_counter()
            ch = [subprocess.Popen(base + ['--arm', '1', '--tag', '%s_%d' % (tag, k), '--slice', '%d/%d' % (k, procs)],
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) for k in range(procs)]
            outs = [c.communicate() for c in ch]
            line = 'PROCS %s procs=%d wall=%.1fs children: %s' % (tag, procs, time.perf_counter() - t0, ' || '.join(
                (o[0].strip().splitlines() or ['rc?'])[-1] for o in outs))
            print(line, flush=True); lines.append(line)
    with open(os.path.join(out, 'summary.txt'), 'a') as fs:
        fs.write('# %s set=%s only=%d\n' % (time.strftime('%m-%d %H:%M:%S'), dset, only))
        fs.write('\n'.join(lines) + '\n')

if __name__ == '__main__':
    main()
