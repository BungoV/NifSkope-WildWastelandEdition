"""Name a stripped function by the strings it references. usage: fnstrings.py <exe> <va> [<va> ...]
VA = static address (image base 0x140000000). Finds the .pdata range holding each VA, disassembles it with objdump,
and prints the .rdata C strings (ASCII or UTF-16) its rip-relative operands point at."""
import struct, subprocess, sys, re

exe = sys.argv[1]
data = open(exe, 'rb').read()
pe = struct.unpack_from('<I', data, 0x3C)[0]
nsec = struct.unpack_from('<H', data, pe + 6)[0]
optsz = struct.unpack_from('<H', data, pe + 20)[0]
base = struct.unpack_from('<Q', data, pe + 24 + 24)[0]
secs = []
for i in range(nsec):
    o = pe + 24 + optsz + 40 * i
    name = data[o:o + 8].rstrip(b'\0').decode()
    vsz, va, rsz, rptr = struct.unpack_from('<IIII', data, o + 8)
    secs.append((name, va, vsz, rptr, rsz))

def rva2off(rva):
    for n, va, vsz, rp, rs in secs:
        if va <= rva < va + max(vsz, rs):
            return rp + rva - va, n
    return None, None

pd = [s for s in secs if s[0] == '.pdata'][0]
funcs = []
for k in range(pd[2] // 12):
    b, e, u = struct.unpack_from('<III', data, pd[3] + 12 * k)
    funcs.append((b, e))

def cstr(rva):
    off, n = rva2off(rva)
    if off is None or n not in ('.rdata', '.data'):
        return None
    raw = data[off:off + 160]
    if len(raw) > 3 and raw[1] == 0 and raw[3] == 0 and 32 <= raw[0] < 127:
        s = raw.decode('utf-16-le', 'ignore').split('\0')[0]
    else:
        s = raw.split(b'\0')[0].decode('latin1')
    if len(s) >= 4 and sum(32 <= ord(c) < 127 for c in s) >= 0.9 * len(s):
        return s
    return None

for a in sys.argv[2:]:
    va = int(a, 16)
    rva = va - base
    f = [x for x in funcs if x[0] <= rva < x[1]]
    if not f:
        print(a, 'no pdata range'); continue
    b, e = f[0]
    out = subprocess.run(['C:/msys64/ucrt64/bin/objdump.exe', '-d', '--no-show-raw-insn',
                          '--start-address=0x%x' % (base + b), '--stop-address=0x%x' % (base + e), exe],
                         capture_output=True, text=True).stdout
    strs = []
    calls = set()
    for m in re.finditer(r'# (0x)?([0-9a-f]{9,})', out):
        s = cstr(int(m.group(2), 16) - base)
        if s and s not in strs:
            strs.append(s)
    print('%s in fn 0x%x..0x%x (%d bytes, +0x%x): %d strings' % (a, base + b, base + e, e - b, rva - b, len(strs)))
    for s in strs[:25]:
        print('   ', repr(s[:110]))
