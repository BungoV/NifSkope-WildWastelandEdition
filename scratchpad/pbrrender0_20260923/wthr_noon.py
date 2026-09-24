import struct, zlib, sys
ESM = r"X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm"
f = open(ESM, "rb")
# skip TES4
hdr = f.read(24); f.seek(24 + struct.unpack_from("<I", hdr, 4)[0])
grp = None
while True:
    h = f.read(24)
    if len(h) < 24: break
    size = struct.unpack_from("<I", h, 4)[0]
    if h[8:12] == b"WTHR":
        grp = f.read(size - 24); break
    f.seek(size - 24, 1)
if grp is None:
    print("VERDICT no WTHR group"); sys.exit()

def subs(data):
    i = 0; out = []; big = None
    while i < len(data):
        t = data[i:i+4]; n = struct.unpack_from("<H", data, i+4)[0]; i += 6
        if t == b"XXXX": big = struct.unpack_from("<I", data, i)[0]; i += n; continue
        if big is not None: n = big; big = None
        out.append((t.decode(), data[i:i+n])); i += n
    return out

recs = []
i = 0
while i < len(grp):
    t = grp[i:i+4]; dsz, flags, fid = struct.unpack_from("<III", grp, i+4)
    body = grp[i+24:i+24+dsz]; i += 24 + dsz
    if t != b"WTHR": continue
    if flags & 0x40000: body = zlib.decompress(body[4:])
    s = subs(body)
    edid = next((d[:-1].decode("latin1") for k, d in s if k == "EDID"), "")
    recs.append((fid, edid, s))
print("VERDICT WTHR records:", len(recs))
print("clear-ish:", ", ".join("%s %08X" % (e, fid) for fid, e, s in recs if "clear" in e.lower() or e.lower().startswith("default"))[:900])

want = sys.argv[1] if len(sys.argv) > 1 else "CommonwealthClear"
rec = next((r for r in recs if r[1] == want), None)
if not rec:
    print("VERDICT", want, "not found"); sys.exit()
fid, edid, s = rec
print("REC", edid, "%08X" % fid, "subrecords:", " ".join("%s(%d)" % (k, len(d)) for k, d in s))
TYPES = ["SkyUpper", "FogNear", "Unknown", "Ambient", "Sunlight", "Sun", "Stars", "SkyLower", "Horizon",
         "EffectLighting", "CloudLODDiffuse", "CloudLODAmbient", "FogFar", "SkyStatics", "WaterMult",
         "SunGlare", "MoonGlare", "FogNearHigh", "FogFarHigh"]
TIMES = ["Sunrise", "Day", "Sunset", "Night", "EarlySunrise", "LateSunrise", "EarlySunset", "LateSunset"]
for k, d in s:
    if k == "NAM0":
        ntimes = 8 if len(d) % (8 * 4) == 0 and len(d) // 32 in (19, 20) else 4
        ntypes = len(d) // (4 * ntimes)
        print("NAM0 %d types x %d times; Day column (RGB 0-255):" % (ntypes, ntimes))
        for ti in range(ntypes):
            o = (ti * ntimes + 1) * 4
            r, g, b = d[o], d[o+1], d[o+2]
            print("  %-16s %3d %3d %3d" % (TYPES[ti] if ti < len(TYPES) else "t%d" % ti, r, g, b))
    if k == "FNAM":
        fl = struct.unpack_from("<%df" % (len(d) // 4), d)
        print("FNAM floats:", " ".join("%.4g" % x for x in fl))
    if k == "DALC":
        c = [tuple(d[j*4:j*4+3]) for j in range(7)]
        tail = struct.unpack_from("<f", d, 28)[0] if len(d) >= 32 else None
        print("DALC(%d) X+%s X-%s Y+%s Y-%s Z+%s Z-%s Spec%s scale=%s" % (len(d), *c, tail))
    if k.endswith("0TX"):
        print(k, d[:-1].decode("latin1"))
    if k in ("HNAM", "IMSP", "WGDR"):
        print(k, d.hex())
