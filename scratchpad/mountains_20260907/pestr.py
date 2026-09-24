"""Read NUL-terminated strings (and dwords) at given RVAs from a PE image.
Lane IDENTITY, read-only. Used against Todd's treat (FO4 1.10.155 Fallout4.exe) to
recover BSLightingShader technique-name literals."""
import struct, sys

class PE(object):
    def __init__(self, path):
        self.b = open(path, 'rb').read()
        e_lfanew = struct.unpack_from('<I', self.b, 0x3C)[0]
        assert self.b[e_lfanew:e_lfanew + 4] == b'PE\0\0'
        coff = e_lfanew + 4
        nsec = struct.unpack_from('<H', self.b, coff + 2)[0]
        optsz = struct.unpack_from('<H', self.b, coff + 16)[0]
        opt = coff + 20
        self.imagebase = struct.unpack_from('<Q', self.b, opt + 24)[0]
        sec = opt + optsz
        self.sections = []
        for i in range(nsec):
            o = sec + i * 40
            name = self.b[o:o + 8].rstrip(b'\0').decode('latin1')
            vsize, vaddr, rsize, raddr = struct.unpack_from('<IIII', self.b, o + 8)
            self.sections.append((name, vaddr, vsize, raddr, rsize))

    def off(self, rva):
        for name, vaddr, vsize, raddr, rsize in self.sections:
            if vaddr <= rva < vaddr + max(vsize, rsize):
                d = rva - vaddr
                if d < rsize:
                    return raddr + d
                return None
        return None

    def cstr(self, rva, maxlen=128):
        o = self.off(rva)
        if o is None:
            return None
        e = self.b.find(b'\0', o, o + maxlen)
        if e < 0:
            e = o + maxlen
        return self.b[o:e].decode('latin1')

    def dwords(self, rva, n):
        o = self.off(rva)
        return list(struct.unpack_from('<%dI' % n, self.b, o))

if __name__ == '__main__':
    pe = PE(sys.argv[1])
    mode = sys.argv[2]
    if mode == 'sections':
        for s in pe.sections:
            print('%-8s vaddr=0x%08x vsize=0x%08x raddr=0x%08x rsize=0x%08x' % s)
        print('imagebase 0x%x' % pe.imagebase)
    elif mode == 'str':
        for a in sys.argv[3:]:
            rva = int(a, 16)
            print('0x%08x  %r' % (rva, pe.cstr(rva)))
    elif mode == 'jt':
        rva = int(sys.argv[3], 16)
        n = int(sys.argv[4])
        for i, v in enumerate(pe.dwords(rva, n)):
            print('  case %2d -> 0x%08x' % (i, v))
