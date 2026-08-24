"""Extract one file from a Bethesda BA2 (GNRL), to check that the unpacked corpus
this whole investigation used as "vanilla" really is what the game loads.

Every comparison in this session has been against
E:\\Tools\\Fallout 4\\DataUnpacked. That tree was unpacked by hand at some point, and
it is the one assumption nothing has tested.

BA2 GNRL layout: 'BTDX', version u32, type u32 ('GNRL'), numFiles u32,
nameTableOffset u64; then numFiles x 36-byte records
(nameHash, ext, dirHash, flags, offset u64, packedSize, unpackedSize, align);
then the name table, u16 length + bytes per file, in record order.
"""
import struct
import sys
import zlib

ARCHIVE = sys.argv[1]
WANT = sys.argv[2].lower().replace("/", "\\")
OUT = sys.argv[3]

with open(ARCHIVE, "rb") as f:
    head = f.read(24)
    magic, version, kind, numFiles, nameTableOffset = struct.unpack("<4sII I Q", head)
    if magic != b"BTDX":
        sys.exit("not a BA2: %r" % magic)
    kind = kind.to_bytes(4, "little")
    print("%s  version %d  type %s  files %d" % (ARCHIVE.rsplit("\\", 1)[-1],
                                                 version, kind.decode(), numFiles))
    if kind != b"GNRL":
        sys.exit("only GNRL handled, this is %s" % kind.decode())
    recs = []
    for _ in range(numFiles):
        recs.append(struct.unpack("<IIIIQIII", f.read(36)))
    f.seek(nameTableOffset)
    names = []
    for _ in range(numFiles):
        n = struct.unpack("<H", f.read(2))[0]
        names.append(f.read(n).decode("latin-1"))
    hit = None
    for r, nm in zip(recs, names):
        if nm.lower() == WANT:
            hit = (r, nm)
            break
    if hit is None:
        cand = [n for n in names if WANT.rsplit("\\", 1)[-1] in n.lower()][:5]
        sys.exit("not found. similar: %s" % cand)
    r, nm = hit
    _, _, _, _, offset, packed, unpacked, _ = r
    f.seek(offset)
    data = f.read(packed if packed else unpacked)
    if packed:
        data = zlib.decompress(data)
    if len(data) != unpacked:
        print("  WARNING: got %d bytes, header says %d" % (len(data), unpacked))
    open(OUT, "wb").write(data)
    print("  %s -> %s (%d bytes)" % (nm, OUT, len(data)))
