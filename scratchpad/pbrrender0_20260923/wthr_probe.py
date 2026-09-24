"""PBRRENDER0 research probe: measure WTHR / CLMT / IMGS / LGTM layouts in Fallout4.esm.
Prints verdict lines only. Independent of NifSkope's reader (pure struct + zlib)."""
import struct, zlib, sys, collections

ESM = r"X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
WANT = {b"WTHR", b"CLMT", b"IMGS", b"LGTM"}

def records(data, pos, end):
    while pos < end:
        typ, size, flags, fid = struct.unpack_from("<4sIII", data, pos)
        if typ == b"GRUP":
            yield ("GRUP", data[pos+8:pos+12], pos + 24, pos + size)
            pos += size
        else:
            yield ("REC", typ, flags, fid, struct.unpack_from("<H", data, pos + 20)[0], pos + 24, size)
            pos += 24 + size

def fields(body):
    out = []; p = 0; big = None
    while p + 6 <= len(body):
        t, n = struct.unpack_from("<4sH", body, p); p += 6
        if t == b"XXXX":
            big = struct.unpack_from("<I", body, p)[0]; p += n; continue
        if big is not None: n = big; big = None
        out.append((t, body[p:p+n])); p += n
    return out

def body_of(data, flags, off, size):
    b = data[off:off+size]
    if flags & 0x00040000:
        b = zlib.decompress(b[4:])
    return b

data = open(ESM, "rb").read()
hsz = struct.unpack_from("<I", data, 4)[0]
pos = 24 + hsz
recs = collections.defaultdict(list)
while pos < len(data):
    typ, size, label = struct.unpack_from("<4sI4s", data, pos)
    if label in WANT:
        for r in records(data, pos + 24, pos + size):
            if r[0] == "REC":
                _, t, flags, fid, fver, off, sz = r
                recs[t].append((fid, flags, fver, fields(body_of(data, flags, off, sz))))
    pos += size

def edid(fl):
    for t, v in fl:
        if t == b"EDID": return v.rstrip(b"\0").decode("latin1")
    return "?"

w = recs[b"WTHR"]
comp = sum(1 for r in w if r[1] & 0x40000)
vers = collections.Counter(r[2] for r in w)
print(f"WTHR count={len(w)} compressed={comp} formVersions={dict(vers)}")
sizes = collections.defaultdict(collections.Counter)
dalc_n = collections.Counter()
for fid, fl_, fv, fl in w:
    dalc_n[sum(1 for t, _ in fl if t == b"DALC")] += 1
    for t, v in fl:
        if t in (b"NAM0", b"FNAM", b"IMSP", b"DALC", b"PNAM", b"JNAM", b"RNAM", b"QNAM", b"DATA", b"WGDR", b"HNAM", b"NAM4", b"LNAM", b"GNAM", b"UNAM"):
            sizes[t.decode()][len(v)] += 1
print("WTHR subrecord sizes:", {k: dict(v) for k, v in sorted(sizes.items())})
print("WTHR DALC per record:", dict(dalc_n))

NAM0 = ["SkyUpper","FogNear","Unused","Ambient","Sunlight","Sun","Stars","SkyLower","Horizon","EffectLighting",
        "CloudLODDiffuse","CloudLODAmbient","FogFar","SkyStatics","WaterMult","SunGlare","MoonGlare","FogNearHigh","FogFarHigh"]
TOD = ["Sunrise","Day","Sunset","Night","EarlySunrise","LateSunrise","EarlySunset","LateSunset"]
for want in ("CommonwealthClear", "CommonwealthDefault"):
    for fid, fl_, fv, fl in w:
        if edid(fl) != want: continue
        d = dict((t, v) for t, v in fl)
        n0 = d[b"NAM0"]
        def col(ci, ti): return tuple(n0[(ci*8+ti)*4:(ci*8+ti)*4+3])
        print(f"GATE {want} {fid:08X} fv={fv} NAM0.Sunlight.Day={col(4,1)} NAM0.Ambient.Day={col(3,1)} NAM0.Sun.Day={col(5,1)} NAM0.SkyUpper.Day={col(0,1)} NAM0.Horizon.Day={col(8,1)} NAM0.Sunlight.Night={col(4,3)}")
        dal = [v for t, v in fl if t == b"DALC"]
        dd = dal[1]
        print(f"GATE {want} DALC[Day] X+{tuple(dd[0:3])} X-{tuple(dd[4:7])} Y+{tuple(dd[8:11])} Y-{tuple(dd[12:15])} Z+{tuple(dd[16:19])} Z-{tuple(dd[20:23])} spec{tuple(dd[24:27])} fresnel={struct.unpack_from('<f',dd,28)[0] if len(dd)>=32 else None}")
        f = struct.unpack_from(f"<{len(d[b'FNAM'])//4}f", d[b"FNAM"])
        print(f"GATE {want} FNAM dayNear={f[0]} dayFar={f[1]} nightNear={f[2]} nightFar={f[3]} dayPow={f[4]} nightPow={f[5]} dayMax={f[6]} nightMax={f[7]}")
        im = struct.unpack_from(f"<{len(d[b'IMSP'])//4}I", d[b"IMSP"])
        print(f"GATE {want} IMSP " + " ".join(f"{TOD[i]}={x:08X}" for i, x in enumerate(im)))
        tx = [t.decode("latin1") for t, _ in fl if t.endswith(b"0TX")]
        print(f"GATE {want} cloudTex={len(tx)} first={tx[:3]} DATA={d[b'DATA'].hex()}")
        imgs = {r[0]: r for r in recs[b"IMGS"]}
        day = imgs.get(im[1])
        if day:
            dh = dict((t, v) for t, v in day[3])
            hn = struct.unpack_from("<9f", dh[b"HNAM"]) if b"HNAM" in dh else None
            cn = struct.unpack_from("<3f", dh[b"CNAM"]) if b"CNAM" in dh else None
            print(f"GATE IMGS {edid(day[3])} HNAM(eyeAdapt,tonemapE,bloomThr,bloomScale,aeMax,aeMin,sunScale,skyScale,midGray)={tuple(round(x,4) for x in hn) if hn else None} CNAM(sat,bri,con)={tuple(round(x,4) for x in cn) if cn else None}")

c = recs[b"CLMT"]
for fid, fl_, fv, fl in c:
    d = dict((t, v) for t, v in fl)
    if edid(fl) in ("CommonwealthClimate", "DefaultClimate") or fid == 0x0000015E:
        tn = d.get(b"TNAM", b"")
        z = bytes(1)
        sun = d.get(b'FNAM', b'').rstrip(z).decode(); gl = d.get(b'GNAM', b'').rstrip(z).decode()
        print(f"CLMT {edid(fl)} {fid:08X} TNAM={list(tn)} (x10 min) sun={sun} glare={gl} weathers={len(d.get(b'WLST',b''))//12}")
tn_all = collections.Counter(tuple(dict((t, v) for t, v in fl).get(b"TNAM", b"")[:4]) for _, _, _, fl in c)
print(f"CLMT count={len(c)} distinct sunrise/sunset TNAM={tn_all.most_common(4)}")
print("CLMT edids:", [edid(fl) for _, _, _, fl in c][:12])

l = recs[b"LGTM"]
ls = collections.defaultdict(collections.Counter)
for _, _, _, fl in l:
    for t, v in fl:
        ls[t.decode("latin1")][len(v)] += 1
print(f"LGTM count={len(l)} subrecords={ {k: dict(v) for k, v in ls.items()} }")
