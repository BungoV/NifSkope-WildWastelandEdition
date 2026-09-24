"""Split Shaders011.fxp into DXBC blobs, sha1 each, disassemble requested ones with d3dcompiler_47.
Usage: python fxp_dis.py <sha-prefix> [<sha-prefix> ...]   -> writes dis_<prefix>.txt beside this script
       python fxp_dis.py --list-ps-with <substring>          -> sha of every ps blob whose disasm contains it
Nothing is written outside this folder."""
import ctypes, hashlib, os, struct, sys

FXP = r"E:\Projects\Fo4CommunityShaders\Codex\2026-07-18-truepbr-renderdoc\work\fallout4_shaders_extract\ShadersFX\Shaders011.fxp"
HERE = os.path.dirname(os.path.abspath(__file__))

d3d = ctypes.WinDLL("d3dcompiler_47.dll")
D3DDisassemble = d3d.D3DDisassemble
D3DDisassemble.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_uint, ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
D3DDisassemble.restype = ctypes.c_long


def blob_text(buf):
    out = ctypes.c_void_p()
    cbuf = ctypes.create_string_buffer(buf, len(buf))
    hr = D3DDisassemble(cbuf, len(buf), 0, None, ctypes.byref(out))
    if hr != 0:
        return None
    vt = ctypes.cast(ctypes.cast(out, ctypes.POINTER(ctypes.c_void_p))[0], ctypes.POINTER(ctypes.c_void_p))
    getp = ctypes.WINFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p)(vt[3])
    gets = ctypes.WINFUNCTYPE(ctypes.c_size_t, ctypes.c_void_p)(vt[4])
    rel = ctypes.WINFUNCTYPE(ctypes.c_ulong, ctypes.c_void_p)(vt[2])
    p, n = getp(out), gets(out)
    s = ctypes.string_at(p, n).replace(b"\0", b"").decode("latin1")
    rel(out)
    return s


def blobs():
    data = open(FXP, "rb").read()
    i = 0
    while True:
        j = data.find(b"DXBC", i)
        if j < 0:
            break
        size = struct.unpack_from("<I", data, j + 24)[0]
        b = data[j:j + size]
        tech = struct.unpack_from("<I", data, j - 12)[0] if j >= 12 else 0
        yield hashlib.sha1(b).hexdigest(), b, j, tech
        i = j + max(size, 4)


def main():
    args = sys.argv[1:]
    if args and args[0] == "--list-ps-with":
        needles = args[1:]
        hits = 0
        for sha, b, off, tech in blobs():
            t = blob_text(b)
            if t and "\nps_" in t and all(n in t for n in needles):
                hits += 1
                if hits <= 40:
                    print(sha[:12], hex(off), hex(tech), len(b))
        print("hits", hits)
        return
    want = [a.lower() for a in args]
    found = 0
    for sha, b, off, tech in blobs():
        for w in want:
            if sha.startswith(w):
                t = blob_text(b)
                open(os.path.join(HERE, "dis_%s.txt" % w), "w").write(t or "FAILED")
                print(w, "->", sha, hex(off), "tech?", hex(tech), "lines", (t or "").count("\n"))
                found += 1
    print("found", found, "of", len(want))


main()
