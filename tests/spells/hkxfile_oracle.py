#!/usr/bin/env python
"""The INDEPENDENT Havok packfile oracle for lane HKXEDIT1 (2026-09-10):
a generic reader and writer for FO4 `hk_2014.1.0-r1` binary packfiles driven
by the class database `res/hkclasses_fo4.json` (extracted from the 1.10.155
exe by tools/hkclassdb_extract.py). It shares no code with src/hkxfile.cpp;
it is written from docs/HKX_PACKFILE_MODEL.md and holds the C++ against
itself and against the shipped bytes.

What it does:
  read(blob)        -> Packfile: header, classnames, objects as typed trees,
                       plus the CHUNK LIST (every object body, array payload
                       and string, in file order) so a layout study can ask
                       where the writer's rule and the file disagree.
  write(pf)         -> bytes, canonical layout (the rules in the contract:
                       depth-first, 16-aligned chunks, strings padded after,
                       fixup tables in write order, 0xFF fill between tables).
  dump(pf)          -> text, every object and field (the human check).
  edit helpers      -> set_scalar / set_string / resize_array by path, the
                       independent side of the edit->save->reload gate.

CLI:
  python hkxfile_oracle.py dump <file.hkx>
  python hkxfile_oracle.py roundtrip <file.hkx> [--out x.hkx]   (exit 0 = byte-identical)
  python hkxfile_oracle.py census <dir-or-ba2> [--limit N] [--report out.tsv]
"""
import sys, os, struct, json, zlib, hashlib, argparse, time, collections

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
DEFAULT_DB = os.path.join(REPO, "res", "hkclasses_fo4.json")

MAGIC = b"\x57\xe0\xe0\x57\x10\xc0\xc0\x10"
SIZE = {"BOOL": 1, "CHAR": 1, "INT8": 1, "UINT8": 1, "INT16": 2, "UINT16": 2, "INT32": 4, "UINT32": 4,
        "INT64": 8, "UINT64": 8, "REAL": 4, "VECTOR4": 16, "QUATERNION": 16, "MATRIX3": 48, "ROTATION": 48,
        "QSTRANSFORM": 48, "MATRIX4": 64, "TRANSFORM": 64, "POINTER": 8, "FUNCTIONPOINTER": 8, "ARRAY": 16,
        "SIMPLEARRAY": 16, "HOMOGENEOUSARRAY": 24, "VARIANT": 16, "CSTRING": 8, "ULONG": 8, "HALF": 2,
        "STRINGPTR": 8, "RELARRAY": 4, "ZERO": 0, "VOID": 0}
INT_FMT = {"BOOL": "<B", "CHAR": "<B", "INT8": "<b", "UINT8": "<B", "INT16": "<h", "UINT16": "<H", "INT32": "<i",
           "UINT32": "<I", "INT64": "<q", "UINT64": "<Q", "ULONG": "<Q", "HALF": "<H"}
SERIALIZE_IGNORED = 0x400


class Refusal(Exception):
    pass


class ClassDb:
    def __init__(self, path=DEFAULT_DB):
        self.raw = json.load(open(path, encoding="utf-8"))
        self.classes = {c["name"]: c for c in self.raw["classes"]}
        self._all = {}

    def get(self, name):
        c = self.classes.get(name)
        if c is None:
            raise Refusal("class %s is not in the class database (%d classes)" % (name, len(self.classes)))
        return c

    def all_members(self, name):
        """Parent chain first, then the class's own, in declaration order."""
        if name in self._all:
            return self._all[name]
        c = self.get(name)
        out = list(self.all_members(c["parent"])) if c["parent"] else []
        out += c["members"]
        self._all[name] = out
        return out

    def elem_size(self, m):
        """Byte size of one element of member m (its type, or the subtype for arrays)."""
        t = m["type"]
        if t == "STRUCT":
            return self.get(m["class"])["objectSize"]
        if t in ("ENUM", "FLAGS"):
            return SIZE.get(m["subtype"], 4)
        return SIZE[t]

    def array_elem_size(self, m):
        st = m["subtype"]
        if st == "STRUCT":
            return self.get(m["class"])["objectSize"]
        if st in ("ENUM", "FLAGS"):
            fl = m["flags"]
            return 1 if fl & 8 else 2 if fl & 16 else 4
        return SIZE[st]


# ------------------------------------------------------------------ values
# A field value is a dict {"k": kind, ...}:
#   int/float/bool/half  : {"k":"scalar","t":TYPE,"v":value}   (cArraySize>1 -> "vals": [..])
#   vector types         : {"k":"raw","t":TYPE,"b":bytes}
#   string               : {"k":"str","s":bytes|None}          (None = null pointer)
#   pointer              : {"k":"ptr","o":objindex|-1}
#   array                : {"k":"arr","cap":int,"n":int,"has":bool,"e":[values]}
#   struct               : {"k":"struct","c":classname,"m":[values],"hole":bytes}
#   ignored              : {"k":"ignored","b":bytes}

class Packfile:
    def __init__(self):
        self.head = b""            # bytes 0..sectionHeadersStart (kept verbatim; the class-name offset is patched)
        self.section_tags = []     # ["__classnames__", "__types__", "__data__"]
        self.classnames = []       # [(signature, name)] in file order
        self.objects = []          # [{"c": name, "v": structvalue, "off": fileoff}]
        self.root = 0
        self.chunks = []           # [(off, size, kind, label)] read order, for the layout study
        self.notes = []            # layout-rule observations (informational)


def read(blob, db):
    if len(blob) < 0x40 or blob[:8] != MAGIC:
        raise Refusal("not a Havok packfile: magic %s" % blob[:8].hex())
    userTag, fileVersion = struct.unpack_from("<iI", blob, 8)
    layout = blob[0x10:0x14]
    numSections, contentsSection, contentsOffset, cnSection, cnOffset = struct.unpack_from("<iiiii", blob, 0x14)
    if fileVersion != 11:
        raise Refusal("fileVersion %d, this reader knows 11" % fileVersion)
    if layout[0] != 8 or layout[1] != 1:
        raise Refusal("layout %s: bytesInPointer %d littleEndian %d, this reader knows 8/1" % (layout.hex(), layout[0], layout[1]))
    pad = struct.unpack_from("<H", blob, 0x3e)[0]
    shs = 0x40 + pad
    if numSections < 1 or numSections > 16 or shs + numSections * 0x40 > len(blob):
        raise Refusal("numSections %d with section headers at 0x%x does not fit %d bytes" % (numSections, shs, len(blob)))
    pf = Packfile()
    pf.head = blob[:shs]
    secs = []
    for s in range(numSections):
        o = shs + s * 0x40
        tag = blob[o:o + 19].split(b"\0")[0].decode("latin-1")
        absStart, local, glob, virt, exports, imports, end = struct.unpack_from("<7i", blob, o + 20)
        secs.append(dict(tag=tag, abs=absStart, local=local, glob=glob, virt=virt, exports=exports, imports=imports, end=end))
        pf.section_tags.append(tag)
    try:
        cn = next(s for s in secs if s["tag"] == "__classnames__")
        dt = next(s for s in secs if s["tag"] == "__data__")
    except StopIteration:
        raise Refusal("sections %s: no __classnames__ / __data__" % pf.section_tags)
    if pf.section_tags != ["__classnames__", "__types__", "__data__"]:
        raise Refusal("sections %s: this reader knows the three-section layout __classnames__ / __types__ / __data__" % pf.section_tags)
    if dt["abs"] + dt["end"] > len(blob):
        raise Refusal("__data__ end 0x%x past the file (%d bytes)" % (dt["abs"] + dt["end"], len(blob)))
    # class names: [u32 sig][0x09][name\0]... then 0xFF
    p, endp = cn["abs"], cn["abs"] + cn["local"]
    byoff = {}
    while p + 5 <= endp and blob[p + 4] == 0x09:
        sig = struct.unpack_from("<I", blob, p)[0]
        e = blob.index(b"\0", p + 5)
        name = blob[p + 5:e].decode("latin-1")
        byoff[p + 5 - cn["abs"]] = name
        pf.classnames.append((sig, name))
        p = e + 1
    base = dt["abs"]
    local = {}
    for q in range(base + dt["local"], base + dt["glob"] - 7, 8):
        src, dst = struct.unpack_from("<ii", blob, q)
        if src != -1:
            local[base + src] = base + dst
    glob = {}
    for q in range(base + dt["glob"], base + dt["virt"] - 11, 12):
        src, sec, dst = struct.unpack_from("<iii", blob, q)
        if src != -1:
            if sec != 2:
                raise Refusal("global fixup at src 0x%x points into section %d, only __data__ (2) is known" % (src, sec))
            glob[base + src] = base + dst
    objs = []
    for q in range(base + dt["virt"], base + dt["exports"] - 11, 12):
        off, sec, cno = struct.unpack_from("<iii", blob, q)
        if off != -1:
            if cno not in byoff:
                raise Refusal("virtual fixup at 0x%x names class-name offset %d, which is not a class name" % (off, cno))
            objs.append((base + off, byoff[cno]))
    if not objs:
        raise Refusal("no virtual fixups: the file has no objects")
    if cnSection != 0 or byoff.get(cnOffset) is None:
        raise Refusal("contents class name (section %d offset %d) is not a class name" % (cnSection, cnOffset))
    objindex = {off: i for i, (off, _) in enumerate(objs)}
    if contentsSection != 2 or (base + contentsOffset) not in objindex:
        raise Refusal("contents object (section %d offset %d) is not an object" % (contentsSection, contentsOffset))
    pf.root = objindex[base + contentsOffset]
    payload_end = base + dt["local"]
    used_local = set()
    used_glob = set()

    def chunk(off, size, kind, label):
        pf.chunks.append((off, size, kind, label))

    def read_struct(cname, at, label):
        c = db.get(cname)
        size = c["objectSize"]
        if at + size > payload_end:
            raise Refusal("%s at 0x%x (%d bytes) runs past the payload end 0x%x" % (label, at - base, size, payload_end - base))
        hole = bytearray(blob[at:at + size])
        vals = []
        for m in db.all_members(cname):
            v = read_member(m, at, label + "." + m["name"], hole)
            vals.append(v)
        return {"k": "struct", "c": cname, "m": vals, "hole": bytes(hole)}

    def clear(hole, off, n):
        hole[off:off + n] = b"\0" * n

    def read_member(m, at, label, hole):
        t = m["type"]
        off = m["offset"]
        p = at + off
        n = max(1, m["cArraySize"])
        if m["flags"] & SERIALIZE_IGNORED:
            esz = db.elem_size(m) if t != "STRUCT" else db.get(m["class"])["objectSize"]
            b = blob[p:p + esz * n]
            return {"k": "ignored", "b": b}
        if t in INT_FMT or t == "REAL":
            fmt = "<f" if t == "REAL" else INT_FMT[t]
            sz = struct.calcsize(fmt)
            vals = [struct.unpack_from(fmt, blob, p + i * sz)[0] for i in range(n)]
            clear(hole, off, sz * n)
            if t == "REAL":
                # keep the exact bits (NaN payloads survive; a float re-encode would not)
                bits = [struct.unpack_from("<I", blob, p + i * 4)[0] for i in range(n)]
                return {"k": "scalar", "t": t, "vals": vals, "bits": bits}
            return {"k": "scalar", "t": t, "vals": vals}
        if t in ("ENUM", "FLAGS"):
            st = m["subtype"]
            if st not in INT_FMT:
                raise Refusal("%s: %s with storage %s" % (label, t, st))
            fmt = INT_FMT[st]
            sz = struct.calcsize(fmt)
            vals = [struct.unpack_from(fmt, blob, p + i * sz)[0] for i in range(n)]
            clear(hole, off, sz * n)
            return {"k": "scalar", "t": t, "st": st, "vals": vals, "enum": m.get("enum")}
        if t in ("VECTOR4", "QUATERNION", "MATRIX3", "ROTATION", "QSTRANSFORM", "MATRIX4", "TRANSFORM"):
            sz = SIZE[t] * n
            b = blob[p:p + sz]
            clear(hole, off, sz)
            return {"k": "raw", "t": t, "b": b, "n": n}
        if t in ("CSTRING", "STRINGPTR"):
            out = []
            for i in range(n):
                q = p + i * 8
                clear(hole, off + i * 8, 8)
                if q in local:
                    d = local[q]
                    used_local.add(q)
                    e = blob.index(b"\0", d)
                    if e >= payload_end:
                        raise Refusal("%s: string at 0x%x has no terminator inside the payload" % (label, d - base))
                    chunk(d, e + 1 - d, "str", label)
                    out.append({"k": "str", "s": blob[d:e]})
                else:
                    if struct.unpack_from("<Q", blob, q)[0] != 0:
                        raise Refusal("%s: string pointer bytes are not zero and have no local fixup" % label)
                    out.append({"k": "str", "s": None})
            return out[0] if n == 1 else {"k": "carr", "e": out}
        if t == "POINTER":
            out = []
            for i in range(n):
                q = p + i * 8
                clear(hole, off + i * 8, 8)
                if q in glob:
                    used_glob.add(q)
                    d = glob[q]
                    if d not in objindex:
                        raise Refusal("%s: pointer to 0x%x, which is not an object start" % (label, d - base))
                    out.append({"k": "ptr", "o": objindex[d]})
                elif q in local:
                    raise Refusal("%s: an object pointer with a LOCAL fixup (to 0x%x) is not modelled" % (label, local[q] - base))
                else:
                    if struct.unpack_from("<Q", blob, q)[0] != 0:
                        raise Refusal("%s: pointer bytes are not zero and have no fixup" % label)
                    out.append({"k": "ptr", "o": -1})
            return out[0] if n == 1 else {"k": "carr", "e": out}
        if t == "STRUCT":
            if n == 1:
                return read_struct(m["class"], p, label)
            sz = db.get(m["class"])["objectSize"]
            return {"k": "carr", "e": [read_struct(m["class"], p + i * sz, "%s[%d]" % (label, i)) for i in range(n)]}
        if t == "ARRAY":
            if n != 1:
                raise Refusal("%s: a C array of hkArray is not modelled" % label)
            size, cap = struct.unpack_from("<iI", blob, p + 8)
            clear(hole, off, 16)
            st = m["subtype"]
            if st in ("VOID",):
                if p in local:
                    raise Refusal("%s: hkArray<void> with a payload" % label)
                return {"k": "arr", "cap": cap, "n": size, "has": False, "e": [], "st": st}
            esz = db.array_elem_size(m)
            has = p in local
            elems = []
            if has:
                used_local.add(p)
                d = local[p]
                if d + esz * size > payload_end:
                    raise Refusal("%s: %d elements of %d bytes at 0x%x run past the payload" % (label, size, esz, d - base))
                chunk(d, esz * size, "arr", label)
                if size < 0 or size > 50_000_000:
                    raise Refusal("%s: array size %d" % (label, size))
                if st == "STRUCT":
                    cname = m["class"]
                    for i in range(size):
                        elems.append(read_struct(cname, d + i * esz, "%s[%d]" % (label, i)))
                elif st == "POINTER":
                    for i in range(size):
                        q = d + i * 8
                        if q in glob:
                            used_glob.add(q)
                            tgt = glob[q]
                            if tgt not in objindex:
                                raise Refusal("%s[%d]: pointer to 0x%x, not an object" % (label, i, tgt - base))
                            elems.append({"k": "ptr", "o": objindex[tgt]})
                        else:
                            if struct.unpack_from("<Q", blob, q)[0] != 0:
                                raise Refusal("%s[%d]: pointer bytes are not zero and have no fixup" % (label, i))
                            elems.append({"k": "ptr", "o": -1})
                elif st in ("CSTRING", "STRINGPTR"):
                    for i in range(size):
                        q = d + i * 8
                        if q in local:
                            used_local.add(q)
                            s0 = local[q]
                            e = blob.index(b"\0", s0)
                            chunk(s0, e + 1 - s0, "str", "%s[%d]" % (label, i))
                            elems.append({"k": "str", "s": blob[s0:e]})
                        else:
                            elems.append({"k": "str", "s": None})
                elif st in INT_FMT or st == "REAL":
                    fmt = "<f" if st == "REAL" else INT_FMT[st]
                    vals = list(struct.unpack_from("<%d%s" % (size, fmt[1]), blob, d)) if size else []
                    bits = list(struct.unpack_from("<%dI" % size, blob, d)) if (st == "REAL" and size) else None
                    return {"k": "arr", "cap": cap, "n": size, "has": True, "st": st, "vals": vals, "bits": bits, "e": []}
                elif st in ("VECTOR4", "QUATERNION", "MATRIX3", "ROTATION", "QSTRANSFORM", "MATRIX4", "TRANSFORM"):
                    return {"k": "arr", "cap": cap, "n": size, "has": True, "st": st, "b": blob[d:d + esz * size], "e": []}
                else:
                    raise Refusal("%s: hkArray of %s is not modelled" % (label, st))
            else:
                if struct.unpack_from("<Q", blob, p)[0] != 0:
                    raise Refusal("%s: array pointer bytes are not zero and have no fixup" % label)
                if size != 0:
                    raise Refusal("%s: array size %d with no payload" % (label, size))
            return {"k": "arr", "cap": cap, "n": size, "has": has, "e": elems, "st": st}
        if t == "RELARRAY":
            if n != 1:
                raise Refusal("%s: a C array of hkRelArray is not modelled" % label)
            size, rel = struct.unpack_from("<HH", blob, p)
            clear(hole, off, 4)
            st = m["subtype"]
            esz = db.array_elem_size(m)
            d = p + rel
            if size and (rel == 0 or d + esz * size > payload_end):
                raise Refusal("%s: hkRelArray of %d x %d bytes at +%d runs past the payload" % (label, size, esz, rel))
            chunk(d, esz * size, "rel", label)
            if st == "STRUCT":
                return {"k": "rel", "n": size, "st": st, "e": [read_struct(m["class"], d + i * esz, "%s[%d]" % (label, i)) for i in range(size)]}
            if st in INT_FMT or st == "REAL":
                fmt = "<f" if st == "REAL" else INT_FMT[st]
                vals = list(struct.unpack_from("<%d%s" % (size, fmt[1]), blob, d)) if size else []
                bits = list(struct.unpack_from("<%dI" % size, blob, d)) if (st == "REAL" and size) else None
                return {"k": "rel", "n": size, "st": st, "vals": vals, "bits": bits, "e": []}
            if st in ("VECTOR4", "QUATERNION", "MATRIX3", "ROTATION", "QSTRANSFORM", "MATRIX4", "TRANSFORM"):
                return {"k": "rel", "n": size, "st": st, "b": blob[d:d + esz * size], "e": []}
            raise Refusal("%s: hkRelArray of %s is not modelled" % (label, st))
        raise Refusal("%s: member type %s is not modelled by this reader" % (label, t))

    for i, (off, cname) in enumerate(objs):
        c = db.get(cname)
        chunk(off, c["objectSize"], "obj", "%s#%d" % (cname, i))
        pf.objects.append({"c": cname, "off": off - base, "v": read_struct(cname, off, "%s#%d" % (cname, i))})
    unused = sorted(set(local) - used_local)
    if unused:
        raise Refusal("%d local fixups were not consumed by any modelled member (first at src 0x%x)" % (len(unused), unused[0] - base))
    ug = sorted(set(glob) - used_glob)
    if ug:
        raise Refusal("%d global fixups were not consumed (first at src 0x%x)" % (len(ug), ug[0] - base))
    pf.data_abs = base
    pf.payload_end = dt["local"]
    pf.sections = secs
    return pf


# ------------------------------------------------------------------ write

def align16(buf):
    while len(buf) % 16:
        buf.append(0)


def write(pf, db):
    """Canonical layout. Returns bytes."""
    data = bytearray()
    wchunks = []        # (off, size, kind, label) in write order, mirrors Packfile.chunks
    local_fix = []      # (src, dst)
    glob_fix = []       # (src, dst_objindex)  -- resolved after all objects are placed
    virt_fix = []       # (objoff, classname)
    placed = {}         # objindex -> offset
    pending_globals = []  # (src, objindex)

    def put(off, fmt, *vals):
        struct.pack_into(fmt, data, off, *vals)

    def encode_struct(v, at, label):
        """Write struct value v's body at `at` (already reserved); collect extras to write after."""
        c = db.get(v["c"])
        size = c["objectSize"]
        data[at:at + size] = v["hole"]
        extras = []
        for m, fv in zip(db.all_members(v["c"]), v["m"]):
            encode_member(m, fv, at, label + "." + m["name"], extras)
        return extras

    def encode_member(m, fv, at, label, extras):
        t = m["type"]
        p = at + m["offset"]
        k = fv["k"]
        if k == "ignored":
            data[p:p + len(fv["b"])] = fv["b"]
            return
        if k == "scalar":
            if fv["t"] == "REAL" and fv.get("bits") is not None:
                for i, b in enumerate(fv["bits"]):
                    put(p + 4 * i, "<I", b)
                return
            st = fv.get("st", fv["t"])
            fmt = "<f" if fv["t"] == "REAL" else INT_FMT[st]
            sz = struct.calcsize(fmt)
            for i, x in enumerate(fv["vals"]):
                put(p + sz * i, fmt, x)
            return
        if k == "raw":
            data[p:p + len(fv["b"])] = fv["b"]
            return
        if k == "carr":
            esz = db.elem_size(m)
            for i, e in enumerate(fv["e"]):
                encode_one(m, e, p + i * esz, "%s[%d]" % (label, i), extras)
            return
        encode_one(m, fv, p, label, extras)

    def encode_one(m, fv, p, label, extras):
        k = fv["k"]
        if k == "str":
            if fv["s"] is not None:
                extras.append(("str", p, fv["s"], label))
            return
        if k == "ptr":
            if fv["o"] >= 0:
                # the global fixup is recorded when the extras are FLUSHED, so
                # a pointer inside an earlier member's array payload precedes a
                # direct pointer member declared later (skeleton.hkx:
                # hknpRagdollData's bodyCinfos[i].shape fixups come before its
                # own `skeleton` at +0x88)
                extras.append(("ptr", fv["o"], p))
            return
        if k == "struct":
            extras.extend(encode_struct(fv, p, label))
            return
        if k == "arr":
            put(p + 8, "<iI", fv["n"], fv["cap"])
            if fv["has"]:
                extras.append(("arr", p, m, fv, label))
            return
        if k == "rel":
            extras.append(("rel", p, m, fv, label))
            return
        raise Refusal("%s: cannot encode %s" % (label, k))

    def flush(extras):
        """Write the extras of one body in order, depth first. Returns pointee list."""
        pointees = []
        for ex in extras:
            kind = ex[0]
            if kind == "str":
                _, src, s, label = ex
                local_fix.append((src, len(data)))
                wchunks.append((len(data), len(s) + 1, "str", label))
                data.extend(s + b"\0")
                align16(data)
            elif kind == "strA":
                # the strings of an hkArray<hkStringPtr> are packed: each one
                # starts at the next EVEN offset (AlienRootBehavior.hkx:
                # eventNames[0] 9 bytes at 27736, [1] at 27746, [2] at 27756);
                # the run is padded to 16 once, after the last one
                _, src, s, label = ex
                if len(data) % 2:
                    data.append(0)
                local_fix.append((src, len(data)))
                wchunks.append((len(data), len(s) + 1, "str", label))
                data.extend(s + b"\0")
            elif kind == "pad16":
                align16(data)
            elif kind == "ptr":
                pending_globals.append((ex[2], ex[1]))
                pointees.append(ex[1])
            elif kind == "arr":
                _, src, m, fv, label = ex
                align16(data)
                local_fix.append((src, len(data)))
                st = fv["st"]
                esz = db.array_elem_size(m)
                start = len(data)
                wchunks.append((start, esz * fv["n"], "arr", label))
                data.extend(b"\0" * (esz * fv["n"]))
                sub = []
                if st == "STRUCT":
                    for i, e in enumerate(fv["e"]):
                        sub.append(encode_struct(e, start + i * esz, "%s[%d]" % (label, i)))
                elif st == "POINTER":
                    for i, e in enumerate(fv["e"]):
                        if e["o"] >= 0:
                            sub.append([("ptr", e["o"], start + i * 8)])
                elif st in ("CSTRING", "STRINGPTR"):
                    for i, e in enumerate(fv["e"]):
                        if e["s"] is not None:
                            sub.append([("strA", start + i * 8, e["s"], "%s[%d]" % (label, i))])
                    if fv["e"]:
                        sub.append([("pad16",)])
                elif st in INT_FMT or st == "REAL":
                    if fv.get("bits") is not None:
                        struct.pack_into("<%dI" % fv["n"], data, start, *fv["bits"])
                    elif fv["n"]:
                        fmt = "<f" if st == "REAL" else INT_FMT[st]
                        struct.pack_into("<%d%s" % (fv["n"], fmt[1]), data, start, *fv["vals"])
                else:
                    data[start:start + esz * fv["n"]] = fv["b"]
                # Padding after an array payload depends on the ELEMENT KIND:
                # a payload of STRUCTS is not padded (jog.hkx: annotationTracks
                # ends at 0xac8 and track 0's "" trackName is AT 0xac8); a
                # payload of pointers or plain values is (skeleton.hkx:
                # referencedObjects ends at 0x4af8, the name string is at
                # 0x4b00). The census over 15,320 files is the gate on this.
                # ... and a payload of STRING POINTERS is not padded either:
                # its strings start right at its end (AlienRootBehavior.hkx:
                # eventNames' pointers end at 27736 and eventNames[0] is AT
                # 27736); the run of strings is padded to 16 once, after.
                if st not in ("STRUCT", "STRINGPTR", "CSTRING"):
                    align16(data)
                for s in sub:
                    pointees.extend(flush(s))
        return pointees

    order = []

    def write_object(i):
        if i in placed:
            return
        align16(data)
        obj = pf.objects[i]
        c = db.get(obj["c"])
        off = len(data)
        placed[i] = off
        order.append(i)
        virt_fix.append((off, obj["c"]))
        wchunks.append((off, c["objectSize"], "obj", "%s#%d" % (obj["c"], i)))
        data.extend(b"\0" * c["objectSize"])
        extras = encode_struct(obj["v"], off, "%s#%d" % (obj["c"], i))
        # hkRelArray payloads sit INSIDE the object's chunk, right after the
        # body, each 16-aligned, in member order; the u16 offset is measured
        # from the member's own address (skeleton.hkx's hknpCapsuleShape:
        # body 112 bytes, vertices at +0x70, planes +0xf0, faces +0x170,
        # indices +0x190, chunk 432 bytes).
        rest = []
        for ex in extras:
            if ex[0] != "rel":
                rest.append(ex)
                continue
            _, src, m, fv, label = ex
            align16(data)
            start = len(data)
            esz = db.array_elem_size(m)
            wchunks.append((start, esz * fv["n"], "rel", label))
            data.extend(b"\0" * (esz * fv["n"]))
            st = fv["st"]
            if st == "STRUCT":
                for k, e in enumerate(fv["e"]):
                    if encode_struct(e, start + k * esz, "%s[%d]" % (label, k)):
                        raise Refusal("%s: an hkRelArray element with extras is not modelled" % label)
            elif st in INT_FMT or st == "REAL":
                if fv.get("bits") is not None:
                    struct.pack_into("<%dI" % fv["n"], data, start, *fv["bits"])
                elif fv["n"]:
                    fmt = "<f" if st == "REAL" else INT_FMT[st]
                    struct.pack_into("<%d%s" % (fv["n"], fmt[1]), data, start, *fv["vals"])
            else:
                data[start:start + esz * fv["n"]] = fv["b"]
            put(src, "<HH", fv["n"], start - src)
        extras = rest
        align16(data)
        pointees = flush(extras)
        for p in pointees:
            write_object(p)

    write_object(pf.root)
    for i in range(len(pf.objects)):
        write_object(i)     # unreachable objects, in file order
    align16(data)
    payload_len = len(data)
    for src, oi in pending_globals:
        glob_fix.append((src, placed[oi]))
    # fixup tables: local, global, virtual, each padded to 16 with 0xFF
    def pad_ff(buf):
        while len(buf) % 16:
            buf.append(0xFF)
    local_off = len(data)
    for s, d in local_fix:
        data.extend(struct.pack("<ii", s, d))
    pad_ff(data)
    glob_off = len(data)
    for s, d in glob_fix:
        data.extend(struct.pack("<iii", s, 2, d))
    pad_ff(data)
    virt_off = len(data)
    # class names: keep the file's order, append any class not yet named
    names = list(pf.classnames)
    have = {n for _, n in names}
    for _, cname in virt_fix:
        if cname not in have:
            names.append((int(db.get(cname)["signature"], 16), cname))
            have.add(cname)
    cn = bytearray()
    name_off = {}
    for sig, n in names:
        cn.extend(struct.pack("<I", sig) + b"\x09")
        name_off[n] = len(cn)
        cn.extend(n.encode("latin-1") + b"\0")
    pad_ff(cn)
    for off, cname in virt_fix:
        data.extend(struct.pack("<iii", off, 0, name_off[cname]))
    pad_ff(data)
    end_off = len(data)
    # header
    head = bytearray(pf.head)
    shs = len(head)
    struct.pack_into("<iiiii", head, 0x14, 3, 2, placed[pf.root], 0, name_off[pf.objects[pf.root]["c"]])
    out = bytearray(head)
    cn_abs = shs + 3 * 0x40
    dt_abs = cn_abs + len(cn)

    def sechdr(tag, absStart, l, g, v, e):
        h = bytearray(tag.encode("latin-1") + b"\0" * (19 - len(tag)) + b"\xff")
        h += struct.pack("<7i", absStart, l, g, v, e, e, e)
        h += b"\xff" * 16
        return h
    out += sechdr("__classnames__", cn_abs, len(cn), len(cn), len(cn), len(cn))
    out += sechdr("__types__", dt_abs, 0, 0, 0, 0)
    out += sechdr("__data__", dt_abs, local_off, glob_off, virt_off, end_off)
    out += cn
    out += data
    pf.wchunks = [(o + dt_abs, sz, k, l) for o, sz, k, l in wchunks]
    return bytes(out)


# ------------------------------------------------------------------ helpers

def first_diff(a, b):
    n = min(len(a), len(b))
    for i in range(n):
        if a[i] != b[i]:
            return i
    return n if len(a) != len(b) else -1


def locate(pf, blob_off):
    """Name the chunk of the ORIGINAL file that holds file offset blob_off."""
    for off, size, kind, label in pf.chunks:
        if off <= blob_off < off + max(size, 1):
            return "%s %s (+0x%x)" % (kind, label, blob_off - off)
    return "outside every chunk"


def dump(pf, db, out):
    def val(v, ind, m=None):
        k = v["k"]
        if k == "scalar":
            if v["t"] == "REAL":
                return " ".join("%r" % x for x in v["vals"])
            if v.get("enum"):
                return " ".join("%d" % x for x in v["vals"]) + " (%s)" % v["enum"]
            return " ".join("%d" % x for x in v["vals"])
        if k == "raw":
            fl = struct.unpack("<%df" % (len(v["b"]) // 4), v["b"])
            return "(" + " ".join("%g" % x for x in fl) + ")"
        if k == "str":
            return "null" if v["s"] is None else repr(v["s"].decode("latin-1"))
        if k == "ptr":
            return "null" if v["o"] < 0 else "#%d %s" % (v["o"], pf.objects[v["o"]]["c"])
        if k == "ignored":
            return "<ignored %d bytes>" % len(v["b"])
        if k == "carr":
            return "[" + ", ".join(val(e, ind) for e in v["e"]) + "]"
        if k == "arr":
            head = "hkArray<%s> n=%d cap=0x%x%s" % (v["st"], v["n"], v["cap"], "" if v["has"] else " (no payload)")
            if "vals" in v:
                return head + " " + " ".join("%r" % x for x in v["vals"][:64]) + (" ..." if v["n"] > 64 else "")
            if "b" in v:
                return head + " %d raw bytes" % len(v["b"])
            lines = [head]
            for i, e in enumerate(v["e"][:2000]):
                lines.append(ind + "  [%d] " % i + val(e, ind + "  "))
            return "\n".join(lines)
        if k == "struct":
            lines = ["%s {" % v["c"]]
            for m, fv in zip(db.all_members(v["c"]), v["m"]):
                lines.append(ind + "  %s = %s" % (m["name"], val(fv, ind + "  ", m)))
            lines.append(ind + "}")
            return "\n".join(lines)
        return "?"
    out.write("classnames: %s\n" % ", ".join("%s 0x%08x" % (n, s) for s, n in pf.classnames))
    for i, o in enumerate(pf.objects):
        out.write("#%d @0x%x %s\n" % (i, o["off"], val(o["v"], "")))
        out.write("\n")


def find(pf, db, path):
    """Path: '#2.numFrames' or '#2.annotationTracks[3].annotations[0].text' -> (container, key, value)."""
    obj, _, rest = path.partition(".")
    oi = int(obj.lstrip("#"))
    v = pf.objects[oi]["v"]
    parent, key = None, None
    for part in rest.split(".") if rest else []:
        name, _, idx = part.partition("[")
        members = db.all_members(v["c"])
        mi = next((k for k, m in enumerate(members) if m["name"] == name), None)
        if mi is None:
            raise KeyError("%s has no member %s" % (v["c"], name))
        parent, key = v["m"], mi
        v = v["m"][mi]
        while idx:
            i, _, idx = idx.rstrip("]").partition("][")
            i = int(i)
            parent, key = v["e"], i
            v = v["e"][i]
    return parent, key, v


def set_scalar(pf, db, path, value):
    _, _, v = find(pf, db, path)
    assert v["k"] == "scalar", path
    v["vals"] = [value]
    if v["t"] == "REAL":
        v["bits"] = [struct.unpack("<I", struct.pack("<f", value))[0]]


def set_string(pf, db, path, s):
    _, _, v = find(pf, db, path)
    assert v["k"] == "str", path
    v["s"] = None if s is None else s.encode("latin-1")


def resize_array(pf, db, path, n):
    _, _, v = find(pf, db, path)
    assert v["k"] == "arr", path
    st = v["st"]
    if "vals" in v:
        v["vals"] = (v["vals"] + [0] * n)[:n]
        if v.get("bits") is not None:
            v["bits"] = (v["bits"] + [0] * n)[:n]
    elif "b" in v:
        esz = len(v["b"]) // max(1, v["n"]) if v["n"] else SIZE[st]
        v["b"] = (v["b"] + b"\0" * (esz * n))[:esz * n]
    else:
        while len(v["e"]) > n:
            v["e"].pop()
        while len(v["e"]) < n:
            v["e"].append(default_value(db, st, v.get("cls")))
    v["n"] = n
    v["cap"] = n | 0x80000000
    v["has"] = n > 0 or v["has"]


def default_value(db, st, cls=None):
    if st == "POINTER":
        return {"k": "ptr", "o": -1}
    if st in ("CSTRING", "STRINGPTR"):
        return {"k": "str", "s": None}
    raise Refusal("no default for array element %s" % st)


# ------------------------------------------------------------------ census

def iter_files(src):
    if os.path.isdir(src):
        for dp, _, fns in os.walk(src):
            for fn in sorted(fns):
                if fn.lower().endswith(".hkx"):
                    p = os.path.join(dp, fn)
                    yield os.path.relpath(p, src).replace("\\", "/"), open(p, "rb").read()
    else:
        with open(src, "rb") as f:
            magic, version, kind, numFiles, nameTableOffset = struct.unpack("<4sII I Q", f.read(24))
            recs = [struct.unpack("<IIIIQIII", f.read(36)) for _ in range(numFiles)]
            f.seek(nameTableOffset)
            names = []
            for _ in range(numFiles):
                ln = struct.unpack("<H", f.read(2))[0]
                names.append(f.read(ln).decode("latin-1"))
            for r, nm in zip(recs, names):
                if not nm.lower().endswith(".hkx"):
                    continue
                f.seek(r[4])
                data = f.read(r[5] if r[5] else r[6])
                if r[5]:
                    data = zlib.decompress(data)
                yield nm.replace("\\", "/"), data


def census(src, db, limit=0, report=None):
    t0 = time.time()
    n = ok = ident = 0
    refused = collections.Counter()
    mism = []
    rows = []
    classes_seen = collections.Counter()
    for name, blob in iter_files(src):
        n += 1
        if limit and n > limit:
            break
        try:
            pf = read(blob, db)
        except Refusal as e:
            key = str(e).split(":")[0][:80]
            refused[key] += 1
            rows.append((name, "REFUSED", str(e)))
            continue
        except Exception as e:
            refused["EXC " + type(e).__name__] += 1
            rows.append((name, "EXC", "%s: %s" % (type(e).__name__, e)))
            continue
        ok += 1
        for o in pf.objects:
            classes_seen[o["c"]] += 1
        try:
            out = write(pf, db)
        except Exception as e:
            rows.append((name, "WRITE-EXC", "%s: %s" % (type(e).__name__, e)))
            refused["WRITE-EXC"] += 1
            continue
        d = first_diff(blob, out)
        if d < 0:
            ident += 1
            rows.append((name, "IDENTICAL", ""))
        else:
            where = locate(pf, d)
            mism.append((name, d, where, len(blob), len(out)))
            rows.append((name, "MISMATCH", "first diff at 0x%x in %s; sizes %d -> %d" % (d, where, len(blob), len(out))))
    dt = time.time() - t0
    print("census %s: %d files, %d parsed, %d byte-identical, %d mismatched, %d refused, %.1f s" %
          (src, n if not limit else min(n, limit), ok, ident, len(mism), sum(refused.values()), dt))
    for k, v in refused.most_common():
        print("  refused %5d  %s" % (v, k))
    for name, d, where, a, b in mism[:30]:
        print("  MISMATCH %s @0x%x %s (%d -> %d)" % (name, d, where, a, b))
    print("  classes seen: %d distinct" % len(classes_seen))
    if report:
        with open(report, "w", newline="\n") as f:
            f.write("file\tverdict\tdetail\n")
            for r in rows:
                f.write("\t".join(r) + "\n")
        with open(report + ".classes.tsv", "w", newline="\n") as f:
            for c, v in classes_seen.most_common():
                f.write("%s\t%d\n" % (c, v))
    return dict(files=n, parsed=ok, identical=ident, mismatched=len(mism), refused=dict(refused), classes=len(classes_seen))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["dump", "roundtrip", "census", "layout"])
    ap.add_argument("src")
    ap.add_argument("--db", default=DEFAULT_DB)
    ap.add_argument("--out")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--report")
    a = ap.parse_args()
    db = ClassDb(a.db)
    if a.cmd == "dump":
        pf = read(open(a.src, "rb").read(), db)
        dump(pf, db, sys.stdout)
        return 0
    if a.cmd == "roundtrip":
        blob = open(a.src, "rb").read()
        pf = read(blob, db)
        out = write(pf, db)
        if a.out:
            open(a.out, "wb").write(out)
        d = first_diff(blob, out)
        if d < 0:
            print("IDENTICAL %d bytes" % len(blob))
            return 0
        print("MISMATCH first diff at 0x%x in %s; sizes %d -> %d" % (d, locate(pf, d), len(blob), len(out)))
        return 1
    if a.cmd == "layout":
        blob = open(a.src, "rb").read()
        pf = read(blob, db)
        out = write(pf, db)
        rc = sorted(pf.chunks)
        wc = pf.wchunks
        print("read chunks %d, written chunks %d, sizes %d -> %d" % (len(rc), len(wc), len(blob), len(out)))
        shown = 0
        for i in range(max(len(rc), len(wc))):
            r = rc[i] if i < len(rc) else None
            w = wc[i] if i < len(wc) else None
            if r != w:
                print("  [%d] file: %s" % (i, r))
                print("       ours: %s" % (w,))
                shown += 1
                if shown >= (a.limit or 8):
                    break
        return 0
    if a.cmd == "census":
        r = census(a.src, db, a.limit, a.report)
        return 0 if r["mismatched"] == 0 else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Refusal as e:
        print("REFUSED: %s" % e)
        sys.exit(2)
