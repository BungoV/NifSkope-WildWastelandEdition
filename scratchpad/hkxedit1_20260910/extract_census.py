"""Extract every .hkx of `Fallout4 - Animations.ba2` (GNRL) to census_hkx/<path>
so the C++ standalone gate can read them from disk. The BA2 walk is the one
scratchpad/hkx1_20260910/census.py uses (24-byte header, 36-byte records,
name table at nameTableOffset, zlib per file when packed != 0).
Usage: python extract_census.py [ba2] [outdir]
"""
import os, sys, struct, zlib, time, hashlib

BA2 = sys.argv[1] if len(sys.argv) > 1 else r"X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4 - Animations.ba2"
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "census_hkx")
t0 = time.time()
n = 0
manifest = []
with open(BA2, "rb") as f:
    magic, version, kind, numFiles, nameTableOffset = struct.unpack("<4sII I Q", f.read(24))
    assert magic == b"BTDX" and kind == b"GNRL"[0] * 0 + kind, (magic, kind)
    recs = [struct.unpack("<IIIIQIII", f.read(36)) for _ in range(numFiles)]
    f.seek(nameTableOffset)
    names = []
    for _ in range(numFiles):
        ln = struct.unpack("<H", f.read(2))[0]
        names.append(f.read(ln).decode("latin-1"))
    for r, nm in zip(recs, names):
        if not nm.lower().endswith(".hkx"):
            continue
        _, _, _, _, offset, packed, unpacked, _ = r
        f.seek(offset)
        data = f.read(packed if packed else unpacked)
        if packed:
            data = zlib.decompress(data)
        rel = nm.replace("\\", "/")
        dst = os.path.join(OUT, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(dst, "wb") as o:
            o.write(data)
        manifest.append("%s\t%d\t%s" % (rel, len(data), hashlib.sha256(data).hexdigest()[:16]))
        n += 1
with open(os.path.join(OUT, "..", "census_manifest.tsv"), "w", newline="\n") as m:
    m.write("path\tbytes\tsha256_16\n" + "\n".join(manifest) + "\n")
print("extracted %d .hkx to %s in %.1f s" % (n, OUT, time.time() - t0))
