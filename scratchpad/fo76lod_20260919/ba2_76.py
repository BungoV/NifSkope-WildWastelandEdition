"""FO76 BA2 reader (GNRL + DX10), tolerant of version 7/8 headers.

Usage:
  python ba2_76.py list <archive> [substr]
  python ba2_76.py get  <archive> <internal\\path> <out>
"""
import struct
import sys
import zlib
import os


def open_ba2(path):
    f = open(path, "rb")
    magic, version, kind, numFiles, nameTableOffset = struct.unpack("<4sIIIQ", f.read(24))
    if magic != b"BTDX":
        raise SystemExit("not a BA2: %r" % magic)
    kind = kind.to_bytes(4, "little")
    # FO76 v7/v8 add fields after nameTableOffset before the record table.
    extra = b""
    if version >= 7:
        extra = f.read(8)  # unknown u64 (v7/v8). v8 adds more; probe below.
    return f, version, kind, numFiles, nameTableOffset, extra


def read_index(path):
    f, version, kind, numFiles, nto, extra = open_ba2(path)
    if kind == b"DX10":
        # DX10 entries are VARIABLE length: a 24-byte header (nameHash, ext,
        # dirHash, unk8, numChunks, chunkHdrSize, height, width, numMips,
        # format, isCubemap, tileMode) followed by numChunks x 24-byte chunk
        # records.  A fixed 24-byte stride lands on 0xBAADF00D fill instead.
        start = f.tell()
        for pad in (0, -8, 8, 16):
            f.seek(start + pad)
            hdrs, ok = [], True
            try:
                for _ in range(numFiles):
                    h = f.read(24)
                    if len(h) < 24:
                        ok = False
                        break
                    nchunk = h[13]
                    chdr = struct.unpack_from("<H", h, 14)[0]
                    if chdr != 24 or nchunk == 0 or nchunk > 32:
                        ok = False
                        break
                    f.seek(nchunk * chdr, 1)
                    hdrs.append(h)
            except Exception:
                ok = False
            if not ok:
                continue
            f.seek(nto)
            names = []
            for _ in range(numFiles):
                nb = f.read(2)
                if len(nb) < 2:
                    ok = False
                    break
                n = struct.unpack("<H", nb)[0]
                if n == 0 or n > 400:
                    ok = False
                    break
                names.append(f.read(n).decode("latin-1"))
            if ok and len(names) == numFiles:
                return f, version, kind, hdrs, names
        raise SystemExit("could not parse DX10 index of %s" % path)
    recsize = 36
    start = f.tell()
    # Probe: try candidate header paddings until the name table parses cleanly.
    for pad in (0, -8, 8, 16):
        f.seek(start + pad)
        try:
            recs = []
            for _ in range(numFiles):
                b = f.read(recsize)
                if len(b) < recsize:
                    raise ValueError
                recs.append(b)
            f.seek(nto)
            names = []
            ok = True
            for _ in range(numFiles):
                nb = f.read(2)
                if len(nb) < 2:
                    ok = False
                    break
                n = struct.unpack("<H", nb)[0]
                if n == 0 or n > 400:
                    ok = False
                    break
                s = f.read(n)
                if len(s) < n:
                    ok = False
                    break
                names.append(s.decode("latin-1"))
            if ok and len(names) == numFiles:
                return f, version, kind, recs, names
        except Exception:
            continue
    raise SystemExit("could not parse index of %s (v%d %s, %d files)"
                     % (path, version, kind.decode(), numFiles))


def extract(f, kind, rec):
    if kind == b"GNRL":
        _, _, _, _, offset, packed, unpacked, _ = struct.unpack("<IIIIQIII", rec)
        f.seek(offset)
        data = f.read(packed if packed else unpacked)
        if packed:
            data = zlib.decompress(data)
        return data
    raise SystemExit("DX10 extraction not implemented here")


if __name__ == "__main__":
    cmd = sys.argv[1]
    arc = sys.argv[2]
    f, version, kind, recs, names = read_index(arc)
    if cmd == "tex":
        # DX10 record: nameHash, ext, dirHash, numChunks u8, chunkHdrSize u16,
        # height u16, width u16, numMips u8, format u8, isCubemap u8, tileMode u8
        sub = sys.argv[3].lower() if len(sys.argv) > 3 else ""
        for rec, nm in zip(recs, names):
            if sub and sub not in nm.lower():
                continue
            # 24 bytes: nameHash, ext, dirHash, unk8, numChunks, chunkHdrSize,
            # height, width, numMips, format, isCubemap, tileMode
            _, _, _, unk, nchunk, chdr, h, w, nmip, fmt, cube, tile = \
                struct.unpack("<IIIBBHHHBBBB", rec)
            print("%-90s %dx%d mips=%d dxgi=%d chunks=%d" % (nm, w, h, nmip, fmt, nchunk))
    elif cmd == "list":
        sub = sys.argv[3].lower() if len(sys.argv) > 3 else ""
        sys.stderr.write("%s v%d %s files=%d\n" % (os.path.basename(arc), version, kind.decode(), len(names)))
        n = 0
        for nm in names:
            if sub in nm.lower():
                print(nm)
                n += 1
        sys.stderr.write("matched %d\n" % n)
    elif cmd == "get":
        want = sys.argv[3].lower().replace("\\", "/")
        out = sys.argv[4]
        for rec, nm in zip(recs, names):
            if nm.lower().replace("\\", "/") == want:
                d = extract(f, kind, rec)
                open(out, "wb").write(d)
                print("%s -> %s (%d bytes)" % (nm, out, len(d)))
                break
        else:
            raise SystemExit("not found: %s" % want)
