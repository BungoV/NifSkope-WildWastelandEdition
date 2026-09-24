# Count the helper bones of a FO4 skeleton.nif by the EXACT rule the exporter
# uses (src/gltfexportopts.cpp gltfExportIsHelperBone). The numbers the
# ROWS FOR BUNGO table quotes for option (6) come from this run, not from
# memory. Lane GLTFEXPORT1, 2026-09-19.
import struct
import sys

HELPER_EXACT = ["camera", "camera control", "camtarget", "camtargetparent",
                "charbumper", "characterbumper"]
HELPER_PREFIX = ["animobject", "weapon", "cam", "prop", "loot", "ladder", "bumper"]

NUL = chr(0)


def is_helper(name):
    n = name.strip().lower()
    if not n:
        return False
    if n in HELPER_EXACT:
        return True
    return any(n.startswith(p) for p in HELPER_PREFIX)


class R:
    def __init__(self, b):
        self.b = b
        self.o = 0

    def u8(self):
        v = self.b[self.o]
        self.o += 1
        return v

    def u16(self):
        v = struct.unpack_from("<H", self.b, self.o)[0]
        self.o += 2
        return v

    def u32(self):
        v = struct.unpack_from("<I", self.b, self.o)[0]
        self.o += 4
        return v

    def line(self):
        e = self.b.index(b"\x0a", self.o)
        s = self.b[self.o:e].decode("latin-1")
        self.o = e + 1
        return s

    def sstr(self):
        n = self.u32()
        s = self.b[self.o:self.o + n].decode("latin-1")
        self.o += n
        return s

    def estr(self):
        # nif.xml ExportString: a BYTE length that INCLUDES the NUL.
        n = self.u8()
        s = self.b[self.o:self.o + n].decode("latin-1").rstrip(NUL)
        self.o += n
        return s


def nif_strings(path):
    b = open(path, "rb").read()
    r = R(b)
    hdr = r.line()
    ver = r.u32()
    r.u8()                       # endian
    r.u32()                      # user version
    nblocks = r.u32()
    bsver = r.u32()
    r.estr()                     # author
    if bsver > 130:
        r.u32()
    r.estr()                     # process script
    r.estr()                     # export script
    if bsver == 130:
        r.estr()                 # max filepath
    ntypes = r.u16()
    types = [r.sstr() for _ in range(ntypes)]
    r.o += 2 * nblocks           # block type index
    r.o += 4 * nblocks           # block sizes
    nstr = r.u32()
    r.u32()                      # max string length
    strs = [r.sstr() for _ in range(nstr)]
    return hdr, ver, bsver, nblocks, types, strs


def main(path):
    hdr, ver, bsver, nblocks, types, strs = nif_strings(path)
    print("file        : %s" % path)
    print("header      : %s" % hdr)
    print("version     : 0x%08x  BSVersion %d  blocks %d" % (ver, bsver, nblocks))
    print("block types : %d  (%s)" % (len(types), ", ".join(types)))
    print("strings     : %d" % len(strs))
    # The string table is a SUPERSET of the node names -- it also holds
    # extra-data keys and controller target names -- so both numbers are
    # printed and the helper names are listed in full rather than filtered
    # away in silence.
    helpers = [s for s in strs if is_helper(s)]
    skins = [s for s in strs if s.lower().endswith("_skin")]
    print("")
    print("strings total          : %d" % len(strs))
    print("helpers by the rule    : %d" % len(helpers))
    print("kept with 'body only'  : %d" % (len(strs) - len(helpers)))
    print("*_skin strings         : %d" % len(skins))
    print("")
    print("the helper names, in file order:")
    for h in helpers:
        print("    %s" % h)
    print("")
    print("a sample of what is KEPT (first 30):")
    for s in [x for x in strs if not is_helper(x)][:30]:
        print("    %s" % s)


if __name__ == "__main__":
    main(sys.argv[1])
