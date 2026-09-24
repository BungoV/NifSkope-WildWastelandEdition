"""PBRRENDER0 fixture picker: read FO4 NIF headers + a few blocks, one verdict line per file.

usage: python nifstrings.py [--scan N] <file-or-dir> ...
  file  -> one verdict line
  dir   -> walks up to N .nif files (default 200) and prints only the classified hits, capped.

Classes:
  BGSM       BSLightingShaderProperty whose name ends .bgsm (material exists? under Data/materials)
  BGEM       BSEffectShaderProperty whose name ends .bgem, on a BSTriShape-family shape
  EMB-LIT    BSLightingShaderProperty with empty/non-material name and a BSShaderTextureSet with a .dds
  EMB-EFF    BSEffectShaderProperty with empty/non-material name and a .dds in the block
  PSYS       NiParticleSystem family present; STRIP if BSStripParticleSystem
"""
import os, re, struct, sys

DATA = r"E:\Tools\Fallout 4\DataUnpacked\Data"
MAT = os.path.join(DATA, "materials")


def parse(path):
    b = open(path, "rb").read()
    nl = b.index(b"\n")
    if b"20.2.0.7" not in b[:nl]:
        return None
    p = nl + 1
    ver, = struct.unpack_from("<I", b, p); p += 4
    p += 1  # endian
    uver, nblocks = struct.unpack_from("<II", b, p); p += 8
    bsver, = struct.unpack_from("<I", b, p); p += 4

    def estr():
        nonlocal p
        n = b[p]; p += 1
        s = b[p:p + n]; p += n
        return s
    estr()                      # author
    if bsver > 130:
        p += 4
    if bsver < 131:
        estr()                  # process script
    estr()                      # export script
    if bsver >= 103:
        estr()                  # max filepath
    ntypes, = struct.unpack_from("<H", b, p); p += 2
    types = []
    for _ in range(ntypes):
        n, = struct.unpack_from("<I", b, p); p += 4
        types.append(b[p:p + n].decode("latin1")); p += n
    tidx = struct.unpack_from("<%dH" % nblocks, b, p); p += 2 * nblocks
    sizes = struct.unpack_from("<%dI" % nblocks, b, p); p += 4 * nblocks
    nstr, _mx = struct.unpack_from("<II", b, p); p += 8
    strings = []
    for _ in range(nstr):
        n, = struct.unpack_from("<I", b, p); p += 4
        strings.append(b[p:p + n].decode("latin1")); p += n
    ngroups, = struct.unpack_from("<I", b, p); p += 4 + 4 * ngroups
    blocks = []
    for i in range(nblocks):
        t = types[tidx[i] & 0x7FFF]
        blocks.append((t, b[p:p + sizes[i]]))
        p += sizes[i]
    return bsver, types, strings, blocks


def dds_in(blob):
    return [m.group(0).decode("latin1") for m in re.finditer(rb"[ -~]{3,200}?\.dds", blob, re.I)]


def mat_exists(name):
    n = name.replace("/", "\\")
    if n.lower().startswith("materials\\"):
        n = n[10:]
    return os.path.exists(os.path.join(MAT, n))


def classify(path):
    try:
        r = parse(path)
    except Exception as e:  # noqa: BLE001
        return ["ERR %s" % type(e).__name__]
    if not r:
        return ["NOT-FO4"]
    bsver, types, strings, blocks = r
    out = []
    shapes = sum(1 for t, _ in blocks if t in ("BSTriShape", "BSSubIndexTriShape", "BSMeshLODTriShape", "BSDynamicTriShape"))
    psys = [t for t, _ in blocks if t in ("NiParticleSystem", "BSStripParticleSystem", "NiMeshParticleSystem", "BSMasterParticleSystem")]
    for t, blob in blocks:
        if t == "BSLightingShaderProperty":
            idx, = struct.unpack_from("<i", blob, 4)      # after the uint32 Shader Type
            name = strings[idx] if 0 <= idx < len(strings) else ""
            if name.lower().endswith(".bgsm"):
                out.append("BGSM %s exists=%d" % (name, mat_exists(name)))
            else:
                tex = []
                for t2, blob2 in blocks:
                    if t2 == "BSShaderTextureSet":
                        tex += dds_in(blob2)
                out.append("EMB-LIT name=%r texsets_dds=%d first=%s" % (name, len(tex), tex[0] if tex else "-"))
        elif t == "BSEffectShaderProperty":
            idx, = struct.unpack_from("<i", blob, 0)
            name = strings[idx] if 0 <= idx < len(strings) else ""
            if name.lower().endswith(".bgem"):
                out.append("BGEM %s exists=%d" % (name, mat_exists(name)))
            else:
                tex = dds_in(blob)
                out.append("EMB-EFF name=%r dds=%d first=%s" % (name, len(tex), tex[0] if tex else "-"))
    if psys:
        out.append("PSYS %s%s" % (",".join(sorted(set(psys))), " STRIP" if "BSStripParticleSystem" in psys else ""))
    out.append("bsver=%d shapes=%d" % (bsver, shapes))
    return out


def main():
    args = sys.argv[1:]
    scan = 200
    only = None
    if args and args[0] == "--scan":
        scan = int(args[1]); args = args[2:]
    if args and args[0] == "--only":
        only = args[1]; args = args[2:]
    for a in args:
        if os.path.isdir(a):
            n = hits = 0
            for root, _d, files in os.walk(a):
                for f in files:
                    if not f.lower().endswith(".nif"):
                        continue
                    n += 1
                    if n > scan:
                        break
                    c = classify(os.path.join(root, f))
                    tags = " | ".join(x for x in c if x.split()[0] in ("EMB-LIT", "EMB-EFF", "BGEM", "PSYS"))
                    if only and only not in " ".join(c):
                        continue
                    if tags and hits < 25:
                        hits += 1
                        print("%s :: %s :: %s" % (os.path.relpath(os.path.join(root, f), DATA), tags, c[-1]))
                if n > scan:
                    break
            print("# scanned %d nif under %s, %d printed" % (min(n, scan), a, hits))
        else:
            print("%s :: %s" % (os.path.relpath(a, DATA), " | ".join(dict.fromkeys(classify(a)))))


if __name__ == "__main__":
    main()
