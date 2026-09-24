import struct, sys, re
ARCHIVE = sys.argv[1]; pat = re.compile(sys.argv[2], re.I)
with open(ARCHIVE, "rb") as f:
    magic, version, kind, numFiles, nameTableOffset = struct.unpack("<4sII I Q", f.read(24))
    recs = [struct.unpack("<IIIIQIII", f.read(36)) for _ in range(numFiles)]
    f.seek(nameTableOffset)
    names = []
    for _ in range(numFiles):
        n = struct.unpack("<H", f.read(2))[0]
        names.append(f.read(n).decode("latin-1"))
for r, nm in zip(recs, names):
    if pat.search(nm):
        print(r[6], nm)
