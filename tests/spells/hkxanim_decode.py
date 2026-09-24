#!/usr/bin/env python3
"""Independent decoder for FO4 hkaSplineCompressedAnimation clips, written from
docs/HKX_ANIMATION_FORMAT.md alone (lane HKX1, 2026-09-10). It is the oracle
that gate (a) holds the C++ reader (src/hkxanim.cpp) against: every bone,
every frame, translation within 1e-4, rotation within 0.01 degrees, scale exact.

Two routes behind one call, like the C++ reader:
  route A  an HKXPACK unpack (.xml)      -- parse_xml()
  route B  the packfile itself (.hkx)    -- parse_hkx()   (the walker is the
           one tools/hkparse.py / census.py use, with the 0x3e predicate padding)

Usage:
  hkxanim_decode.py CLIP(.xml|.hkx) [--out frames.tsv] [--skeleton skeleton.xml]
                    [--block-overlap] [--json]
Prints a header (frames, tracks, duration, blocks, blend hint, skeleton name)
and writes one TSV row per (frame, track):
  frame track bone tx ty tz qx qy qz qw sx sy sz
Root motion rows are written as track -1 (tx ty tz yaw).
Exit code 0 on a decode, 2 on a refusal (the refusal is printed as a sentence).
"""
import math
import re
import struct
import sys
import xml.etree.ElementTree as ET

LF = "\n"

# ---------------------------------------------------------------- refusal
class Refusal(Exception):
    pass

# ---------------------------------------------------------------- packed quaternions (doc 4.6)
SQRT_HALF = math.sqrt(0.5)

def _insert_missing(a, b, c, d, missing):
    q = [a, b, c]
    q.insert(missing, d)
    return tuple(q)

def unpack40(b5):
    v = int.from_bytes(b5, "little")
    a = v & 0xFFF
    b = (v >> 12) & 0xFFF
    c = (v >> 24) & 0xFFF
    missing = (v >> 36) & 3
    negate = (v >> 38) & 1
    f = SQRT_HALF / 2047.0
    x = (a - 2047) * f
    y = (b - 2047) * f
    z = (c - 2047) * f
    d = math.sqrt(max(0.0, 1.0 - x * x - y * y - z * z))
    if negate:
        d = -d
    return _insert_missing(x, y, z, d, missing)

def unpack48(b6):
    w0, w1, w2 = struct.unpack("<HHH", b6)
    a = w0 & 0x7FFF
    b = w1 & 0x7FFF
    c = w2 & 0x7FFF
    missing = (w0 >> 15) | ((w1 >> 15) << 1)
    negate = w2 >> 15
    f = SQRT_HALF / 16383.0
    x = (a - 16383) * f
    y = (b - 16383) * f
    z = (c - 16383) * f
    d = math.sqrt(max(0.0, 1.0 - x * x - y * y - z * z))
    if negate:
        d = -d
    return _insert_missing(x, y, z, d, missing)

ROT_NAMES = {0: "POLAR32", 1: "THREECOMP40", 2: "THREECOMP48", 3: "THREECOMP24",
             4: "STRAIGHT16", 5: "UNCOMPRESSED"}
ROT_SIZE = {1: 5, 2: 6}
ROT_ALIGN = {1: 1, 2: 2}
ROT_UNPACK = {1: unpack40, 2: unpack48}
SCALAR_NAMES = {0: "BITS8", 1: "BITS16"}

# ---------------------------------------------------------------- NURBS (doc 4.5)
def find_span(u, knots, n, degree):
    if u >= knots[n + 1]:
        return n
    if u <= knots[0]:
        return degree
    lo, hi = degree, n + 1
    mid = (lo + hi) // 2
    while u < knots[mid] or u >= knots[mid + 1]:
        if u < knots[mid]:
            hi = mid
        else:
            lo = mid
        mid = (lo + hi) // 2
    return mid

def de_boor(u, knots, points, degree, span):
    """de Boor on the control points span-degree..span; points are tuples."""
    dim = len(points[0])
    d = [list(points[span - degree + j]) for j in range(degree + 1)]
    for r in range(1, degree + 1):
        for j in range(degree, r - 1, -1):
            i = j + span - degree
            denom = knots[i + degree - r + 1] - knots[i]
            alpha = 0.0 if denom == 0 else (u - knots[i]) / denom
            d[j] = [(1.0 - alpha) * d[j - 1][k] + alpha * d[j][k] for k in range(dim)]
    return tuple(d[degree])

# ---------------------------------------------------------------- the block walk (doc 4.4)
class Cursor:
    def __init__(self, data, start, end, where):
        self.d = data
        self.p = start
        self.end = end
        self.where = where

    def need(self, n):
        if self.p + n > self.end:
            raise Refusal("%s: the track walk leaves the block (needs %d bytes at %d, block ends at %d)"
                          % (self.where, n, self.p, self.end))

    def align(self, a):
        self.p = (self.p + a - 1) // a * a

    def u8(self):
        self.need(1)
        v = self.d[self.p]
        self.p += 1
        return v

    def u16(self):
        self.need(2)
        v = struct.unpack_from("<H", self.d, self.p)[0]
        self.p += 2
        return v

    def f32(self):
        self.need(4)
        v = struct.unpack_from("<f", self.d, self.p)[0]
        self.p += 4
        return v

    def raw(self, n):
        self.need(n)
        v = self.d[self.p:self.p + n]
        self.p += n
        return v

def read_knots(c):
    n = c.u16()
    degree = c.u8()
    if degree < 1 or degree > 3:
        raise Refusal("%s: spline degree %d, the engine evaluates 1..3 only" % (c.where, degree))
    if n + 1 > 256:
        raise Refusal("%s: %d control points, more than a 256-frame block can hold" % (c.where, n + 1))
    knots = [float(k) for k in c.raw(n + degree + 2)]
    return n, degree, knots

def read_vector_track(c, mask, quant, default, u):
    """Translation or scale: returns the (x, y, z) at local frame u."""
    if mask == 0:
        return default
    if quant != 1:
        raise Refusal("%s: scalar quantization %s is not decoded by this reader"
                      % (c.where, SCALAR_NAMES.get(quant, quant)))
    spline_axes = [(mask >> (4 + i)) & 1 for i in range(3)]
    static_axes = [(mask >> i) & 1 for i in range(3)]
    n = degree = 0
    knots = None
    if mask & 0xF0:
        n, degree, knots = read_knots(c)
    c.align(4)
    static = list(default)
    rng = [None, None, None]
    for i in range(3):
        if static_axes[i]:
            static[i] = c.f32()
        elif spline_axes[i]:
            rng[i] = (c.f32(), c.f32())
    out = list(static)
    if mask & 0xF0:
        c.align(2)
        na = sum(spline_axes)
        pts = []
        for _ in range(n + 1):
            row = []
            for i in range(3):
                if spline_axes[i]:
                    q = c.u16()
                    lo, hi = rng[i]
                    row.append(lo + q * (1.0 / 65535.0) * (hi - lo))
            pts.append(tuple(row))
        span = find_span(u, knots, n, degree)
        val = de_boor(u, knots, pts, degree, span)
        k = 0
        for i in range(3):
            if spline_axes[i]:
                out[i] = val[k]
                k += 1
    return tuple(out)

def read_rotation_track(c, mask, rq, u):
    """Returns the (x, y, z, w) quaternion (Havok order) at local frame u, and its pre-normalisation length."""
    if mask == 0:
        return (0.0, 0.0, 0.0, 1.0), 1.0
    if rq not in ROT_UNPACK:
        raise Refusal("%s: rotation quantization %s is not decoded by this reader"
                      % (c.where, ROT_NAMES.get(rq, rq)))
    size, unpack = ROT_SIZE[rq], ROT_UNPACK[rq]
    if mask & 0xF0:
        n, degree, knots = read_knots(c)
        c.align(ROT_ALIGN[rq])
        pts = [unpack(c.raw(size)) for _ in range(n + 1)]
        span = find_span(u, knots, n, degree)
        q = de_boor(u, knots, pts, degree, span)
    else:
        c.align(ROT_ALIGN[rq])
        q = unpack(c.raw(size))
    length = math.sqrt(sum(v * v for v in q))
    if length == 0.0:
        raise Refusal("%s: a zero-length rotation" % c.where)
    return tuple(v / length for v in q), length

def decode_frame_in_block(anim, block, u, track_filter=None):
    """Walk block `block` once and evaluate every transform track at local frame u."""
    data = anim["data"]
    nT = anim["numberOfTransformTracks"]
    nF = anim["numberOfFloatTracks"]
    bo = anim["blockOffsets"][block]
    mqs = anim["maskAndQuantizationSize"]
    block_end = bo + anim["floatBlockOffsets"][block]
    if block_end > len(data) or bo + mqs > len(data):
        raise Refusal("block %d: offsets %d / %d lie outside the %d-byte data" % (block, bo, block_end, len(data)))
    c = Cursor(data, bo + mqs, block_end, "block %d" % block)
    out = []
    for t in range(nT):
        c.where = "block %d track %d" % (block, t)
        q, pm, rm, sm = data[bo + 4 * t: bo + 4 * t + 4]
        pq, rq, sq = q & 3, (q >> 2) & 0xF, (q >> 6) & 3
        tr = read_vector_track(c, pm, pq, (0.0, 0.0, 0.0), u)
        c.align(4)
        ro, length = read_rotation_track(c, rm, rq, u)
        c.align(4)
        sc = read_vector_track(c, sm, sq, (1.0, 1.0, 1.0), u)
        c.align(4)
        out.append((tr, ro, sc, length))
    # the walk must end exactly at the float-track data (nothing else is in a block)
    if c.p != block_end and nF == 0:
        raise Refusal("block %d: the track walk ended at %d, the block ends at %d" % (block, c.p, block_end))
    return out

def decode(anim, block_overlap_check=False):
    """All frames. Returns frames[f][t] = (trans, quat_xyzw, scale, qlen)."""
    stride = anim["maxFramesPerBlock"] - 1
    frames = []
    overlap = []
    for f in range(anim["numFrames"]):
        b = min(f // stride, anim["numBlocks"] - 1)
        local = f - b * stride
        frames.append(decode_frame_in_block(anim, b, float(local)))
        if block_overlap_check and b > 0 and local == 0:
            # frame b*stride is the last control point of block b-1 as well
            other = decode_frame_in_block(anim, b - 1, float(stride))
            overlap.append((f, frames[-1], other))
    return frames, overlap

# ---------------------------------------------------------------- route A: HKXPACK XML
def _param(obj, name):
    for p in obj.findall("hkparam"):
        if p.get("name") == name:
            return p
    return None

def _text(obj, name, default=None):
    p = _param(obj, name)
    if p is None:
        return default
    return (p.text or "").strip()

def _ints(obj, name):
    p = _param(obj, name)
    if p is None or not (p.text or "").strip():
        return []
    return [int(t) for t in p.text.split()]

def _vec4s(text):
    return [tuple(float(x) for x in m.group(1).split()) for m in re.finditer(r"\(([^()]*)\)", text)]

def parse_xml(path):
    try:
        root = ET.parse(path).getroot()
    except ET.ParseError as e:
        raise Refusal("%s: not well-formed XML: %s" % (path, e))
    if root.tag != "hkpackfile":
        raise Refusal("%s: not an hkpackfile XML (root element %s)" % (path, root.tag))
    objs = {}
    for o in root.iter("hkobject"):
        if o.get("name") and o.get("class"):
            objs[o.get("name")] = o
    container = None
    for o in objs.values():
        if o.get("class") == "hkaAnimationContainer":
            container = o
    if container is None:
        raise Refusal("%s: no hkaAnimationContainer" % path)
    result = {"skeletons": [], "animations": [], "bindings": [], "source": path}
    for ref in (_text(container, "skeletons") or "").split():
        result["skeletons"].append(_skeleton_from_xml(objs[ref]))
    anim_refs = (_text(container, "animations") or "").split()
    for ref in anim_refs:
        o = objs[ref]
        if o.get("class") != "hkaSplineCompressedAnimation":
            raise Refusal("%s: animation %s is a %s, not decoded by this reader" % (path, ref, o.get("class")))
        a = {
            "ref": ref,
            "type": _text(o, "type"),
            "duration": float(_text(o, "duration")),
            "numberOfTransformTracks": int(_text(o, "numberOfTransformTracks")),
            "numberOfFloatTracks": int(_text(o, "numberOfFloatTracks")),
            "numFrames": int(_text(o, "numFrames")),
            "numBlocks": int(_text(o, "numBlocks")),
            "maxFramesPerBlock": int(_text(o, "maxFramesPerBlock")),
            "maskAndQuantizationSize": int(_text(o, "maskAndQuantizationSize")),
            "blockDuration": float(_text(o, "blockDuration")),
            "blockInverseDuration": float(_text(o, "blockInverseDuration")),
            "frameDuration": float(_text(o, "frameDuration")),
            "blockOffsets": _ints(o, "blockOffsets"),
            "floatBlockOffsets": _ints(o, "floatBlockOffsets"),
            "transformOffsets": _ints(o, "transformOffsets"),
            "floatOffsets": _ints(o, "floatOffsets"),
            "endian": int(_text(o, "endian")),
            "annotations": [],
            "rootMotion": None,
        }
        dp = _param(o, "data")
        if dp is None:
            raise Refusal("%s: animation %s has no data" % (path, ref))
        vals = [int(t) for t in (dp.text or "").split()]
        bad = [v for v in vals if v < 0 or v > 255]
        if bad:
            raise Refusal("%s: data byte %d out of range" % (path, bad[0]))
        a["data"] = bytes(vals)
        if len(a["data"]) != int(dp.get("numelements")):
            raise Refusal("%s: data holds %d bytes, numelements says %s" % (path, len(a["data"]), dp.get("numelements")))
        ann = _param(o, "annotationTracks")
        for tr in (ann.findall("hkobject") if ann is not None else []):
            lst = []
            ap = _param(tr, "annotations")
            for an in (ap.findall("hkobject") if ap is not None else []):
                lst.append((float(_text(an, "time")), _text(an, "text")))
            a["annotations"].append(lst)
        em = _text(o, "extractedMotion")
        if em and em != "null" and em in objs:
            m = objs[em]
            if m.get("class") != "hkaDefaultAnimatedReferenceFrame":
                raise Refusal("%s: extracted motion class %s is not decoded" % (path, m.get("class")))
            a["rootMotion"] = {
                "up": _vec4s(_text(m, "up"))[0],
                "forward": _vec4s(_text(m, "forward"))[0],
                "duration": float(_text(m, "duration")),
                "samples": _vec4s(_text(m, "referenceFrameSamples") or ""),
            }
        result["animations"].append(a)
    for ref in (_text(container, "bindings") or "").split():
        o = objs[ref]
        result["bindings"].append({
            "originalSkeletonName": _text(o, "originalSkeletonName") or "",
            "animation": _text(o, "animation"),
            "transformTrackToBoneIndices": _ints(o, "transformTrackToBoneIndices"),
            "floatTrackToFloatSlotIndices": _ints(o, "floatTrackToFloatSlotIndices"),
            "partitionIndices": _ints(o, "partitionIndices"),
            "blendHint": _text(o, "blendHint"),
        })
    validate(result)
    return result

def _skeleton_from_xml(o):
    parents = [(-1 if p == 65535 else p) for p in _ints(o, "parentIndices")]
    names = []
    bp = _param(o, "bones")
    for b in bp.findall("hkobject"):
        names.append(_text(b, "name"))
    pose = _vec4s(_text(o, "referencePose") or "")
    if len(pose) != 3 * len(names):
        raise Refusal("skeleton %s: %d reference-pose vectors for %d bones" % (_text(o, "name"), len(pose), len(names)))
    ref = [(pose[3 * i], pose[3 * i + 1], pose[3 * i + 2]) for i in range(len(names))]
    return {"name": _text(o, "name"), "boneNames": names, "parents": parents, "referencePose": ref}

# ---------------------------------------------------------------- route B: the packfile
def _packfile(blob):
    if len(blob) < 0x50 or struct.unpack_from("<II", blob, 0) != (0x57E0E057, 0x10C0C010):
        raise Refusal("not a Havok binary packfile (magic)")
    numSections = struct.unpack_from("<i", blob, 20)[0]
    if numSections < 2 or numSections > 8:
        raise Refusal("packfile: %d sections" % numSections)
    sechdr = 0x40 + struct.unpack_from("<H", blob, 0x3e)[0]
    secs = {}
    for s in range(numSections):
        off = sechdr + s * 0x40
        if off + 0x30 > len(blob):
            raise Refusal("packfile: section header %d lies outside the file" % s)
        tag = blob[off:off + 19].split(b"\0")[0].decode("latin-1")
        secs[tag] = struct.unpack_from("<7i", blob, off + 20)
    if "__classnames__" not in secs or "__data__" not in secs:
        raise Refusal("packfile: missing __classnames__ / __data__ section")
    cn = secs["__classnames__"]
    dt = secs["__data__"]
    cnames = {}
    p = cn[0]
    endp = cn[0] + cn[1]
    while p + 5 < endp and p + 5 < len(blob):
        if blob[p + 4] != 0x09:
            break
        e = blob.index(b"\0", p + 5)
        cnames[p + 5 - cn[0]] = blob[p + 5:e].decode("latin-1")
        p = e + 1
    base = dt[0]
    if base + dt[6] > len(blob) or dt[1] > dt[2] or dt[2] > dt[3] or dt[3] > dt[4]:
        raise Refusal("packfile: __data__ fixup tables are truncated or out of order")
    local, glob, objs = {}, {}, []
    for p in range(base + dt[1], base + dt[2] - 7, 8):
        src, dst = struct.unpack_from("<ii", blob, p)
        if src != -1:
            local[base + src] = base + dst
    for p in range(base + dt[2], base + dt[3] - 11, 12):
        src, sec, dst = struct.unpack_from("<iii", blob, p)
        if src != -1:
            glob[base + src] = base + dst
    for p in range(base + dt[3], base + dt[4] - 11, 12):
        src, sec, cno = struct.unpack_from("<iii", blob, p)
        if src != -1:
            objs.append((base + src, cnames.get(cno, "?")))
    return base, local, glob, objs

def _arr(blob, local, at, esize):
    size = struct.unpack_from("<i", blob, at + 8)[0]
    ptr = local.get(at)
    if size and ptr is None:
        raise Refusal("hkArray at %d: size %d but no payload fixup" % (at, size))
    if ptr is not None and ptr + size * esize > len(blob):
        raise Refusal("hkArray at %d: payload leaves the file" % at)
    return ptr, size

def _cstr(blob, local, at):
    p = local.get(at)
    if p is None:
        return ""
    return blob[p:blob.index(b"\0", p)].decode("latin-1")

def parse_hkx(path):
    blob = open(path, "rb").read()
    base, local, glob, objs = _packfile(blob)
    cls = dict(objs)
    container = [o for o, c in objs if c == "hkaAnimationContainer"]
    if not container:
        raise Refusal("%s: no hkaAnimationContainer" % path)
    co = container[0]
    result = {"skeletons": [], "animations": [], "bindings": [], "source": path}
    sp, sn = _arr(blob, local, co + 0x10, 8)
    for i in range(sn):
        so = glob.get(sp + 8 * i)
        result["skeletons"].append(_skeleton_from_hkx(blob, local, so))
    ap, an = _arr(blob, local, co + 0x20, 8)
    for i in range(an):
        ao = glob.get(ap + 8 * i)
        if cls.get(ao) != "hkaSplineCompressedAnimation":
            raise Refusal("%s: animation %d is a %s, not decoded by this reader" % (path, i, cls.get(ao)))
        atype, dur, nT, nF = struct.unpack_from("<ifii", blob, ao + 0x10)
        nFrames, nBlocks, maxFPB, mqs, bd, bid, fd = struct.unpack_from("<iiiifff", blob, ao + 0x38)
        def u32s(at):
            p, n = _arr(blob, local, at, 4)
            return list(struct.unpack_from("<%dI" % n, blob, p)) if n else []
        dp, dn = _arr(blob, local, ao + 0x98, 1)
        a = {
            "ref": "#%d" % (ao - base),
            "type": {3: "HK_SPLINE_COMPRESSED_ANIMATION"}.get(atype, str(atype)),
            "duration": dur, "numberOfTransformTracks": nT, "numberOfFloatTracks": nF,
            "numFrames": nFrames, "numBlocks": nBlocks, "maxFramesPerBlock": maxFPB,
            "maskAndQuantizationSize": mqs, "blockDuration": bd, "blockInverseDuration": bid,
            "frameDuration": fd, "blockOffsets": u32s(ao + 0x58), "floatBlockOffsets": u32s(ao + 0x68),
            "transformOffsets": u32s(ao + 0x78), "floatOffsets": u32s(ao + 0x88),
            "data": bytes(blob[dp:dp + dn]) if dn else b"",
            "endian": struct.unpack_from("<i", blob, ao + 0xa8)[0],
            "annotations": [], "rootMotion": None,
        }
        tp, tn = _arr(blob, local, ao + 0x28, 0x18)
        for k in range(tn):
            lst = []
            anp, ann = _arr(blob, local, tp + k * 0x18 + 8, 16)
            for j in range(ann):
                t = struct.unpack_from("<f", blob, anp + 16 * j)[0]
                lst.append((t, _cstr(blob, local, anp + 16 * j + 8)))
            a["annotations"].append(lst)
        em = glob.get(ao + 0x20)
        if em is not None:
            if cls.get(em) != "hkaDefaultAnimatedReferenceFrame":
                raise Refusal("%s: extracted motion class %s is not decoded" % (path, cls.get(em)))
            rp, rn = _arr(blob, local, em + 0x48, 16)
            a["rootMotion"] = {
                "up": struct.unpack_from("<4f", blob, em + 0x20),
                "forward": struct.unpack_from("<4f", blob, em + 0x30),
                "duration": struct.unpack_from("<f", blob, em + 0x40)[0],
                "samples": [struct.unpack_from("<4f", blob, rp + 16 * j) for j in range(rn)],
            }
        result["animations"].append(a)
    bp, bn = _arr(blob, local, co + 0x30, 8)
    for i in range(bn):
        bo = glob.get(bp + 8 * i)
        def i16s(at):
            p, n = _arr(blob, local, at, 2)
            return list(struct.unpack_from("<%dh" % n, blob, p)) if n else []
        ref = glob.get(bo + 0x18)
        result["bindings"].append({
            "originalSkeletonName": _cstr(blob, local, bo + 0x10),
            "animation": "#%d" % (ref - base) if ref is not None else "",
            "transformTrackToBoneIndices": i16s(bo + 0x20),
            "floatTrackToFloatSlotIndices": i16s(bo + 0x30),
            "partitionIndices": i16s(bo + 0x40),
            "blendHint": {0: "NORMAL", 1: "ADDITIVE_DEPRECATED", 2: "ADDITIVE"}.get(blob[bo + 0x50], str(blob[bo + 0x50])),
        })
    validate(result)
    return result

def _skeleton_from_hkx(blob, local, so):
    name = _cstr(blob, local, so + 0x10)
    pp, pn = _arr(blob, local, so + 0x18, 2)
    parents = list(struct.unpack_from("<%dh" % pn, blob, pp)) if pn else []
    bp, bn = _arr(blob, local, so + 0x28, 16)
    names = [_cstr(blob, local, bp + 16 * i) for i in range(bn)]
    rp, rn = _arr(blob, local, so + 0x38, 48)
    if rn != bn or pn != bn:
        raise Refusal("skeleton %s: %d bones, %d parents, %d reference transforms" % (name, bn, pn, rn))
    ref = []
    for i in range(rn):
        v = struct.unpack_from("<12f", blob, rp + 48 * i)
        ref.append((v[0:4], v[4:8], v[8:12]))
    return {"name": name, "boneNames": names, "parents": parents, "referencePose": ref}

# ---------------------------------------------------------------- validation (doc 7)
def validate(result):
    if not result["animations"] and not result["skeletons"]:
        raise Refusal("%s: the container holds no animation and no skeleton" % result["source"])
    for a in result["animations"]:
        if a["type"] != "HK_SPLINE_COMPRESSED_ANIMATION":
            raise Refusal("animation type %s is not HK_SPLINE_COMPRESSED_ANIMATION" % a["type"])
        if a["endian"] != 0:
            raise Refusal("endian %d: only little-endian data is decoded" % a["endian"])
        if a["maxFramesPerBlock"] < 2:
            raise Refusal("maxFramesPerBlock %d" % a["maxFramesPerBlock"])
        if a["numFrames"] < 1 or a["numBlocks"] < 1:
            raise Refusal("numFrames %d / numBlocks %d" % (a["numFrames"], a["numBlocks"]))
        if len(a["blockOffsets"]) != a["numBlocks"] or len(a["floatBlockOffsets"]) != a["numBlocks"]:
            raise Refusal("blockOffsets has %d entries, floatBlockOffsets %d, numBlocks is %d"
                          % (len(a["blockOffsets"]), len(a["floatBlockOffsets"]), a["numBlocks"]))
        if a["maskAndQuantizationSize"] != 4 * (a["numberOfTransformTracks"] + a["numberOfFloatTracks"]):
            raise Refusal("maskAndQuantizationSize %d is not 4 * (%d + %d)"
                          % (a["maskAndQuantizationSize"], a["numberOfTransformTracks"], a["numberOfFloatTracks"]))
        need_blocks = (a["numFrames"] - 1) // (a["maxFramesPerBlock"] - 1) + 1
        if a["numFrames"] > 1 and need_blocks > a["numBlocks"]:
            raise Refusal("%d frames need %d blocks of %d, the file has %d"
                          % (a["numFrames"], need_blocks, a["maxFramesPerBlock"], a["numBlocks"]))
        for b in range(a["numBlocks"]):
            bo, fo = a["blockOffsets"][b], a["floatBlockOffsets"][b]
            if bo + a["maskAndQuantizationSize"] > len(a["data"]) or bo + fo > len(a["data"]) or fo < a["maskAndQuantizationSize"]:
                raise Refusal("block %d: offset %d / float offset %d against %d data bytes" % (b, bo, fo, len(a["data"])))
        if abs(a["duration"] - (a["numFrames"] - 1) * a["frameDuration"]) > 1e-3 * max(1.0, a["duration"]):
            raise Refusal("duration %.6f is not (numFrames-1) * frameDuration = %.6f"
                          % (a["duration"], (a["numFrames"] - 1) * a["frameDuration"]))
        if a["rootMotion"] is not None and len(a["rootMotion"]["samples"]) != a["numFrames"]:
            raise Refusal("%d root-motion samples for %d frames" % (len(a["rootMotion"]["samples"]), a["numFrames"]))
    for b in result["bindings"]:
        anim = [x for x in result["animations"] if x["ref"] == b["animation"]]
        if not anim:
            raise Refusal("binding refers to animation %s which is not in the container" % b["animation"])
        # An EMPTY transformTrackToBoneIndices is the IDENTITY map, not a
        # missing one (lane FIXTURE, 2026-09-10, measured on the Mixamo clip
        # Running_To_Slide_And_Back_To_Running.hkx: 95 tracks, binding count 0
        # with no local fixup). src/hkxanim.cpp holds the same rule, so the two
        # decoders stay a matched pair. A non-empty vector of the wrong length
        # is still refused by name.
        if b["transformTrackToBoneIndices"] and \
                len(b["transformTrackToBoneIndices"]) != anim[0]["numberOfTransformTracks"]:
            raise Refusal("binding maps %d tracks, the animation has %d"
                          % (len(b["transformTrackToBoneIndices"]), anim[0]["numberOfTransformTracks"]))

# ---------------------------------------------------------------- driver
def load(path):
    return parse_xml(path) if path.lower().endswith(".xml") else parse_hkx(path)

def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    path = argv[1]
    out = None
    overlap = "--block-overlap" in argv
    if "--out" in argv:
        out = argv[argv.index("--out") + 1]
    try:
        r = load(path)
        if not r["animations"]:
            for sk in r["skeletons"]:
                print("skeleton %s bones %d" % (sk["name"], len(sk["boneNames"])))
            return 0
        a = r["animations"][0]
        bind = r["bindings"][0] if r["bindings"] else None
        frames, ov = decode(a, overlap)
    except Refusal as e:
        print("REFUSED: %s" % e)
        return 2
    print("frames %d tracks %d duration %.6f frameDuration %.6f blocks %d maxFramesPerBlock %d blendHint %s skeleton %s rootMotion %s"
          % (a["numFrames"], a["numberOfTransformTracks"], a["duration"], a["frameDuration"], a["numBlocks"],
             a["maxFramesPerBlock"], bind["blendHint"] if bind else "-", bind["originalSkeletonName"] if bind else "-",
             "yes" if a["rootMotion"] else "no"))
    for sk in r["skeletons"]:
        print("skeleton %s bones %d" % (sk["name"], len(sk["boneNames"])))
    if ov:
        worst = 0.0
        for f, a1, a2 in ov:
            for (t1, q1, s1, _), (t2, q2, s2, _) in zip(a1, a2):
                worst = max(worst, max(abs(x - y) for x, y in zip(t1 + q1 + s1, t2 + q2 + s2)))
        print("block-overlap frames %d worst component difference %.3g" % (len(ov), worst))
    if out:
        with open(out, "w", newline="") as fh:
            fh.write("# frame\ttrack\tbone\ttx\tty\ttz\tqx\tqy\tqz\tqw\tsx\tsy\tsz" + LF)
            for f, fr in enumerate(frames):
                for t, (tr, q, sc, _) in enumerate(fr):
                    # identity when there is no binding, or an empty one
                    tb = bind["transformTrackToBoneIndices"] if bind else []
                    bone = tb[t] if t < len(tb) else t
                    fh.write("%d\t%d\t%d\t" % (f, t, bone) + "\t".join("%.9g" % v for v in tr + q + sc) + LF)
                if a["rootMotion"]:
                    s = a["rootMotion"]["samples"][f]
                    fh.write("%d\t-1\t-1\t%.9g\t%.9g\t%.9g\t%.9g" % (f, s[0], s[1], s[2], s[3]) + LF)
        print("wrote %s" % out)
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))
