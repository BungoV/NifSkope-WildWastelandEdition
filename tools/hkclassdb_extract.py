#!/usr/bin/env python
"""Build res/hkclasses_fo4.json -- the Havok hkClass reflection database that
NifSkope's .hkx reader and writer use -- from YOUR OWN installed Fallout4.exe.

The database is not shipped with NifSkope: it is read out of the game's
executable, so each user builds it once on their own machine.

Usage:
  pip install capstone
  python tools/hkclassdb_extract.py --exe "<Fallout 4 folder>/Fallout4.exe"
        [--out res/hkclasses_fo4.json] [--report <folder for the log>]
        [--hkxpack <hkxpack-core.jar>]     (optional second check, see below)

Run it from the NifSkope source folder (or pass --out). NifSkope looks for the
file next to its exe, in ../res/ beside the exe, and in res/ under the current
folder.

What is read (no symbols needed; everything is found by its shape):

* Every hkClass OBJECT is built at run time by a small initializer function,
  because the on-disk data page holds no bytes for it. Each initializer is
  straight-line code that fills the class constructor's thirteen arguments
  (this, name, parent, objectSize, numImplementedInterfaces, enums, numEnums,
  members, numMembers, defaults, attributes, flags, describedVersion) and calls
  it; it is emulated here with capstone (a register/stack tracker over ~20
  instructions). Candidate initializers are the small functions listed in the
  exe's own unwind table (.pdata); the constructor is the one function that
  most of them call with all thirteen arguments filled.
* The class registry (the null-terminated array of the classes the loader
  knows) is the longest run of pointers to those class objects in the data
  sections.
* Member arrays are const arrays of hkClassMember (0x28 bytes: name* +0,
  class* +8, enum* +0x10, type u8 +0x18, subtype u8 +0x19, cArraySize u16
  +0x1a, flags u16 +0x1c, offset u16 +0x1e). The enum* slot is ZERO on disk and
  patched at run time by tiny leaf initializers (`mov reg, [rip+ptr]; mov
  [rip+slot], reg`), found by a byte-pattern scan of the code section.
* hkClassEnum objects are const (0x28 bytes: name* +0, items* +8, numItems
  +0x10, attributes* +0x18, flags +0x20); items are 16-byte
  {int value; pad; const char* name}.
* The class SIGNATURE (what a packfile's __classnames__ stores) is ~crc32 over
  the reflected description of the class and each parent, re-implemented here.

--hkxpack is OPTIONAL: given HKXPACK's hkxpack-core.jar, every class both know
is cross-checked (layout and signature) and the tables are written to the
--report folder. Without it the exe alone is used.
"""
import sys, os, struct, json, zlib, hashlib, argparse, zipfile, re, time, collections
import xml.etree.ElementTree as ET

try:
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64  # noqa: E402
    from capstone.x86 import X86_OP_REG, X86_OP_IMM, X86_OP_MEM  # noqa: E402
except ImportError:
    sys.exit("hkclassdb_extract: needs the capstone module -- run: pip install capstone")

EXE = ""
BASE = 0x140000000   # replaced by the exe's own image base on load

# hkClassMember::Type (Havok 2014 hkClassMember.h); the numbers are what the
# exe's member arrays store and what HKXPACK's TYPE_* names mean.
TYPES = {0: "VOID", 1: "BOOL", 2: "CHAR", 3: "INT8", 4: "UINT8", 5: "INT16", 6: "UINT16", 7: "INT32",
         8: "UINT32", 9: "INT64", 10: "UINT64", 11: "REAL", 12: "VECTOR4", 13: "QUATERNION", 14: "MATRIX3",
         15: "ROTATION", 16: "QSTRANSFORM", 17: "MATRIX4", 18: "TRANSFORM", 19: "ZERO", 20: "POINTER",
         21: "FUNCTIONPOINTER", 22: "ARRAY", 23: "INPLACEARRAY", 24: "ENUM", 25: "STRUCT", 26: "SIMPLEARRAY",
         27: "HOMOGENEOUSARRAY", 28: "VARIANT", 29: "CSTRING", 30: "ULONG", 31: "FLAGS", 32: "HALF",
         33: "STRINGPTR", 34: "RELARRAY"}
TYPE_ID = {v: k for k, v in TYPES.items()}
# hkClassMember::getClassMemberTypeProperties(type).m_size, the byte width of a
# plain type on a 64-bit layout (only the integer widths enter the signature)
TYPE_SIZE = {"BOOL": 1, "CHAR": 1, "INT8": 1, "UINT8": 1, "INT16": 2, "UINT16": 2, "INT32": 4, "UINT32": 4,
             "INT64": 8, "UINT64": 8, "REAL": 4, "VECTOR4": 16, "QUATERNION": 16, "MATRIX3": 48,
             "ROTATION": 48, "QSTRANSFORM": 48, "MATRIX4": 64, "TRANSFORM": 64, "POINTER": 8,
             "FUNCTIONPOINTER": 8, "ARRAY": 16, "ENUM": 4, "SIMPLEARRAY": 16, "HOMOGENEOUSARRAY": 24,
             "VARIANT": 16, "CSTRING": 8, "ULONG": 8, "FLAGS": 4, "HALF": 2, "STRINGPTR": 8, "RELARRAY": 4,
             "VOID": 0, "ZERO": 0, "STRUCT": 0, "INPLACEARRAY": 0}
# hkClassMember::FlagValues
MEMBER_FLAGS = {1: "POINTER_OPTIONAL", 2: "POINTER_VOIDSTAR", 8: "ENUM_8", 16: "ENUM_16", 32: "ENUM_32",
                64: "ARRAY_RAWDATA", 128: "ALIGN_8", 256: "ALIGN_16", 512: "NOT_OWNED",
                1024: "SERIALIZE_IGNORED", 2048: "ALIGN_32"}


def flag_names(f):
    out = [n for b, n in sorted(MEMBER_FLAGS.items()) if f & b]
    rest = f & ~sum(MEMBER_FLAGS)
    if rest:
        out.append("0x%x" % rest)
    return "|".join(out) if out else "FLAGS_NONE"




class PEImage:
    """Minimal PE32+ reader: sections, image base and the .pdata function list."""

    def __init__(self, path):
        self.data = open(path, "rb").read()
        d = self.data
        (e_lfanew,) = struct.unpack_from("<I", d, 0x3C)
        if d[e_lfanew:e_lfanew + 4] != b"PE\0\0":
            raise ValueError("not a PE image")
        coff = e_lfanew + 4
        (_machine, nsec, _, _, _, opt_size, _) = struct.unpack_from("<HHIIIHH", d, coff)
        opt = coff + 20
        if struct.unpack_from("<H", d, opt)[0] != 0x20B:
            raise ValueError("expected a 64-bit (PE32+) exe")
        (self.image_base,) = struct.unpack_from("<Q", d, opt + 24)
        # data directory 3 = the exception (unwind) table, .pdata
        self.pdata_rva, self.pdata_size = struct.unpack_from("<II", d, opt + 112 + 3 * 8)
        self.sections = []
        s = opt + opt_size
        for _ in range(nsec):
            name = d[s:s + 8].rstrip(b"\0").decode("ascii", "replace")
            vsize, vaddr, rsize, rptr = struct.unpack_from("<IIII", d, s + 8)
            (chars,) = struct.unpack_from("<I", d, s + 36)
            self.sections.append((name, vaddr, vsize, rptr, rsize, chars))
            s += 40

    def read(self, rva, n):
        for _, va, vs, rp, rs, _c in self.sections:
            if va <= rva < va + max(vs, rs):
                off = rva - va
                avail = min(n, rs - off)
                if avail <= 0:
                    return b""
                return self.data[rp + off:rp + off + avail]
        raise ValueError("rva %#x not in any section" % rva)

    def section_bytes(self, sec):
        _, va, vs, rp, rs, _c = sec
        return self.data[rp:rp + min(vs, rs)]

    def functions(self):
        """(begin, end) rvas from the unwind table."""
        b = self.read(self.pdata_rva, self.pdata_size)
        for k in range(0, len(b) - 11, 12):
            beg, end, _u = struct.unpack_from("<III", b, k)
            if beg:
                yield beg, end


class Exe:
    def __init__(self, path):
        global BASE
        self.pe = PEImage(path)
        BASE = self.pe.image_base
        self.md = Cs(CS_ARCH_X86, CS_MODE_64)
        self.md.detail = True
        self.code = [s for s in self.pe.sections if s[5] & 0x20000000]                 # executable
        self.datas = [s for s in self.pe.sections if not (s[5] & 0x20000000) and (s[5] & 0x40000000)]
        self.writable = [s for s in self.pe.sections if s[5] & 0x80000000]

    def in_secs(self, rva, secs):
        return any(va <= rva < va + max(vs, rs) for _, va, vs, _rp, rs, _c in secs)
    def read(self, rva, n):
        try:
            return self.pe.read(rva, n)
        except Exception:
            return b""

    def cstr(self, va, n=200):
        rva = va - BASE
        if rva <= 0 or rva > 0x10000000:
            return None
        b = self.read(rva, n)
        e = b.find(b"\0")
        return b[:e].decode("latin-1") if e >= 0 else None

    def u64(self, rva):
        b = self.read(rva, 8)
        return struct.unpack("<Q", b)[0] if len(b) == 8 else None

    def emulate_initializer(self, rva, maxlen=0x200):
        """Run a straight-line dynamic initializer up to its first call.
        Returns (call_target_rva, regs, stackargs) with pointer values as RVAs;
        stackargs[k] is the qword the callee sees at [rsp + 8 + k] (k = 0x20 for
        the fifth argument). Stack stores are tracked by ABSOLUTE address, so
        `mov rax, rsp` before `sub rsp, N` followed by `mov [rax - 0x10], ecx`
        (the shape hkcdShapeTypeClass and 60 others use) lands where it should."""
        code = self.read(rva, maxlen)
        SP0 = 0x7FFF0000
        regs, mem = {"rsp": SP0}, {}

        def rname(r):
            n = r.lower()
            m = {"eax": "rax", "ecx": "rcx", "edx": "rdx", "ebx": "rbx", "esi": "rsi", "edi": "rdi",
                 "r8d": "r8", "r9d": "r9", "r10d": "r10", "r11d": "r11", "al": "rax", "cl": "rcx", "dl": "rdx",
                 "ax": "rax", "cx": "rcx", "dx": "rdx", "r8w": "r8", "r9w": "r9"}
            return m.get(n, n)

        def memaddr(insn, m):
            base = insn.reg_name(m.base) if m.base else None
            if base == "rip":
                return insn.address + insn.size + m.disp, True
            v = regs.get(rname(base), 0) if base else 0
            if m.index:
                v += regs.get(rname(insn.reg_name(m.index)), 0) * m.scale
            return (v + m.disp) & 0xFFFFFFFFFFFFFFFF, False

        for insn in self.md.disasm(code, rva):
            mn = insn.mnemonic
            ops = insn.operands
            if mn == "call":
                sp = regs["rsp"]
                stack = {}
                for k in range(0x20, 0x80, 8):
                    if (sp + k) in mem:
                        stack[k] = mem[sp + k]
                return (ops[0].imm if ops and ops[0].type == X86_OP_IMM else None), regs, stack
            if mn in ("ret", "jmp"):
                return None, regs, {}
            if mn == "nop":
                continue
            if mn in ("sub", "add") and len(ops) == 2 and ops[0].type == X86_OP_REG and ops[1].type == X86_OP_IMM:
                r = rname(insn.reg_name(ops[0].reg))
                regs[r] = (regs.get(r, 0) + (ops[1].imm if mn == "add" else -ops[1].imm)) & 0xFFFFFFFFFFFFFFFF
                continue
            if mn == "xor" and len(ops) == 2 and ops[0].type == X86_OP_REG and ops[1].type == X86_OP_REG                     and rname(insn.reg_name(ops[0].reg)) == rname(insn.reg_name(ops[1].reg)):
                regs[rname(insn.reg_name(ops[0].reg))] = 0
                continue
            if mn == "lea" and len(ops) == 2 and ops[1].type == X86_OP_MEM:
                v, _rip = memaddr(insn, ops[1].mem)
                regs[rname(insn.reg_name(ops[0].reg))] = v & 0xFFFFFFFFFFFFFFFF
                continue
            if mn in ("mov", "movsxd", "movzx", "movaps", "movups") and len(ops) == 2:
                dst, src = ops
                if dst.type == X86_OP_MEM:
                    addr, isrip = memaddr(insn, dst.mem)
                    if isrip:
                        continue
                    if src.type == X86_OP_IMM:
                        v = src.imm & 0xFFFFFFFFFFFFFFFF
                    elif src.type == X86_OP_REG:
                        rn = insn.reg_name(src.reg)
                        if rn.startswith("xmm"):
                            v = 0   # a zeroed xmm store (pxor/movaps pairs) -- treat as zero
                        else:
                            v = regs.get(rname(rn), 0)
                        if dst.size == 4:
                            v &= 0xFFFFFFFF
                    else:
                        continue
                    if dst.size == 16:
                        mem[addr] = v & 0xFFFFFFFFFFFFFFFF
                        mem[addr + 8] = 0
                    else:
                        # a dword store into a slot whose high half was set earlier keeps that half
                        if dst.size == 4 and addr in mem:
                            v = (mem[addr] & ~0xFFFFFFFF) | v
                        mem[addr] = v
                    continue
                if dst.type == X86_OP_REG:
                    rn = insn.reg_name(dst.reg)
                    if rn.startswith("xmm"):
                        continue
                    if src.type == X86_OP_IMM:
                        regs[rname(rn)] = src.imm & 0xFFFFFFFFFFFFFFFF
                    elif src.type == X86_OP_REG:
                        regs[rname(rn)] = regs.get(rname(insn.reg_name(src.reg)), 0)
                    elif src.type == X86_OP_MEM:
                        addr, isrip = memaddr(insn, src.mem)
                        if isrip:
                            v = self.u64(addr)
                            regs[rname(rn)] = (v - BASE) if v else 0
                        else:
                            regs[rname(rn)] = mem.get(addr, 0)
                    continue
            if mn in ("pxor", "xorps") and len(ops) == 2:
                continue
        return None, regs, {}



# ---------------------------------------------------------------- classes

def read_enum(exe, rva):
    b = exe.read(rva, 0x28)
    if len(b) < 0x28:
        return None
    name_va, items_va, n = struct.unpack_from("<QQi", b, 0)
    attrs_va, flags = struct.unpack_from("<QI", b, 0x18)
    name = exe.cstr(name_va)
    if not name or n < 0 or n > 4096:
        return None
    items = []
    for i in range(n):
        ib = exe.read(items_va - BASE + 16 * i, 16)
        if len(ib) < 16:
            return None
        val, _p, nm_va = struct.unpack_from("<iIQ", ib, 0)
        nm = exe.cstr(nm_va)
        if nm is None:
            return None
        items.append([val, nm])
    return {"rva": rva, "name": name, "items": items, "flags": flags}


def find_ctor_calls(exe, log):
    """Emulate every small function of the unwind table to its first call and
    keep the calls shaped like the class constructor's: rcx a writable object,
    rdx a name string, and all nine stack arguments stored."""
    want = set(range(0x28, 0x70, 8))
    calls = collections.defaultdict(list)
    n = 0
    for beg, end in exe.pe.functions():
        if end - beg > 0x300:
            continue
        n += 1
        callee, regs, stack = exe.emulate_initializer(beg)
        if callee is None or not want.issubset(stack):
            continue
        rcx, rdx = regs.get("rcx", 0), regs.get("rdx", 0)
        if not rcx or not rdx or not exe.in_secs(rcx, exe.writable):
            continue
        name = exe.cstr(rdx + BASE)
        if not name or not re.match(r"^[A-Za-z_][A-Za-z0-9_:<>]*$", name):
            continue
        calls[callee].append((beg, regs, stack, name))
    if not calls:
        sys.exit("hkclassdb_extract: no class constructor calls found -- is this Fallout4.exe?")
    ctor = max(calls, key=lambda c: len(calls[c]))
    log("functions emulated: %d; constructor-shaped calls: %d" % (n, len(calls[ctor])))
    return calls[ctor]


def find_enum_patches(exe):
    """`mov reg, [rip+ptr]` followed by one or more `mov [rip+slot], reg`: the
    leaf initializers that patch a member's enum pointer in at run time."""
    patches = {}
    load = re.compile(rb"[\x48\x4c]\x8b([\x05\x0d\x15\x1d\x25\x2d\x35\x3d])(....)", re.S)
    for sec in exe.code:
        va = sec[1]
        b = exe.pe.section_bytes(sec)
        for m in load.finditer(b):
            rex, modrm = b[m.start()], m.group(1)[0]
            disp = struct.unpack("<i", m.group(2))[0]
            at = m.end()
            v = exe.u64(va + at + disp)
            held = (v - BASE) if v else 0
            while held and at + 7 <= len(b) and b[at] == rex and b[at + 1] == 0x89 and b[at + 2] == modrm:
                d2 = struct.unpack_from("<i", b, at + 3)[0]
                patches[va + at + 7 + d2] = held
                at += 7
    return patches


def find_registry(exe, class_rvas, log):
    """The longest null-terminated run of pointers to known class objects."""
    best = (0, None)
    for sec in exe.datas + exe.writable:
        va = sec[1]
        b = exe.pe.section_bytes(sec)
        k, run, start = 0, 0, 0
        while k + 8 <= len(b):
            (v,) = struct.unpack_from("<Q", b, k)
            if v and (v - BASE) in class_rvas:
                if run == 0:
                    start = k
                run += 1
            else:
                if v == 0 and run > best[0]:
                    best = (run, va + start)
                run = 0
            k += 8
    if not best[1]:
        sys.exit("hkclassdb_extract: class registry not found")
    log("class registry: %d entries" % best[0])
    return best[1]


def extract(exe, log):
    # 1. every hkClass built by an initializer
    classes_by_rva = {}
    for rva, regs, stack, name in find_ctor_calls(exe, log):
        this_rva = regs.get("rcx", 0)
        classes_by_rva[this_rva] = {
            "rva": this_rva, "name": name,
            "parentRva": regs.get("r8", 0) or 0,
            "objectSize": regs.get("r9", 0) & 0xFFFFFFFF,
            "numImplementedInterfaces": stack.get(0x28, 0) & 0xFFFFFFFF,
            "enumsRva": stack.get(0x30, 0), "numEnums": stack.get(0x38, 0) & 0xFFFFFFFF,
            "membersRva": stack.get(0x40, 0), "numMembers": stack.get(0x48, 0) & 0xFFFFFFFF,
            "defaultsRva": stack.get(0x50, 0), "attributesRva": stack.get(0x58, 0),
            "flags": stack.get(0x60, 0) & 0xFFFFFFFF, "version": stack.get(0x68, 0) & 0xFFFFFFFF,
            "initializerRva": rva,
        }
    log("hkClass objects: %d" % len(classes_by_rva))

    # 2. the registry
    reg_rva = find_registry(exe, set(classes_by_rva), log)
    reg = []
    i = 0
    while True:
        v = exe.u64(reg_rva + i * 8)
        if not v:
            break
        reg.append(v - BASE)
        i += 1
    for idx, r in enumerate(reg):
        if r in classes_by_rva:
            classes_by_rva[r]["registryIndex"] = idx
        else:
            log("WARN registry[%d] 0x%x has no emulated initializer" % (idx, r))
    # constructor calls with an empty layout that the registry does not list
    # are not real class descriptions; drop them
    for r in [r for r, c in classes_by_rva.items()
              if "registryIndex" not in c and c["objectSize"] == 0 and c["numMembers"] == 0]:
        del classes_by_rva[r]

    # 3. the enum patches on the member arrays: patch address -> enum object rva
    enum_patch = find_enum_patches(exe)
    log("enum patch candidates: %d" % len(enum_patch))

    # 4. members, enums
    enums_by_rva = {}

    def enum_at(rva):
        if rva not in enums_by_rva:
            enums_by_rva[rva] = read_enum(exe, rva)
        return enums_by_rva[rva]

    for cls in classes_by_rva.values():
        cls["enums"] = []
        for k in range(cls["numEnums"]):
            e = enum_at(cls["enumsRva"] + 0x28 * k)
            if e is None:
                log("WARN %s: declared enum %d unreadable at 0x%x" % (cls["name"], k, cls["enumsRva"] + 0x28 * k))
                continue
            cls["enums"].append(e)
        mem = []
        for k in range(cls["numMembers"]):
            at = cls["membersRva"] + 0x28 * k
            b = exe.read(at, 0x28)
            if len(b) < 0x28:
                log("WARN %s: member %d unreadable" % (cls["name"], k))
                break
            name_va, cls_va, enum_va, ty, sub, carr, flags, off = struct.unpack_from("<QQQBBHHH", b, 0)
            nm = exe.cstr(name_va)
            if nm is None:
                log("WARN %s: member %d has no name" % (cls["name"], k))
                nm = "?"
            erva = enum_patch.get(at + 0x10) or ((enum_va - BASE) if enum_va else 0)
            en = enum_at(erva) if erva else None
            mem.append({
                "name": nm, "offset": off, "type": TYPES.get(ty, str(ty)), "subtype": TYPES.get(sub, str(sub)),
                "cArraySize": carr, "flags": flags, "classRva": (cls_va - BASE) if cls_va else 0,
                "enumRva": erva, "enumName": en["name"] if en else None,
            })
            if erva and not en:
                log("WARN %s.%s: enum at 0x%x unreadable" % (cls["name"], nm, erva))
        cls["members"] = mem
    return classes_by_rva, enums_by_rva, reg


# ---------------------------------------------------------------- signature

def crc32_signature(classes_by_rva, enums_by_rva):
    """hkClass::getSignature(0): ~crc32(writeSignature(this) . writeSignature(parent) ...)."""
    cache = {}

    def enum_bytes(e):
        out = bytearray(e["name"].encode("latin-1"))
        for val, nm in e["items"]:
            out += nm.encode("latin-1") + struct.pack("<i", val)
        out += struct.pack("<i", len(e["items"]))
        return bytes(out)

    def class_bytes(cls):
        r = cls["rva"]
        if r in cache:
            return cache[r]
        out = bytearray(struct.pack("<i", cls["numImplementedInterfaces"]))
        for e in cls["enums"]:
            out += enum_bytes(e)
        out += struct.pack("<i", len(cls["enums"]))
        for m in cls["members"]:
            ty, sub, flags, carr = TYPE_ID[m["type"]], TYPE_ID[m["subtype"]], m["flags"], m["cArraySize"]
            extra = 0
            if ty in (TYPE_ID["ENUM"], TYPE_ID["FLAGS"]):
                flags ^= (TYPE_SIZE.get(m["subtype"], 0) << 3)
                sub = 0
            if flags & 0x400:
                flags ^= 0x400
                if sub != 0:
                    extra = sub
                sub = ty
                ty = TYPE_ID["ZERO"]
            if m["classRva"] and TYPE_ID[m["type"]] != TYPE_ID["POINTER"] and TYPE_ID[m["subtype"]] != TYPE_ID["POINTER"]:
                c = classes_by_rva.get(m["classRva"])
                while c:
                    out += class_bytes(c)
                    c = classes_by_rva.get(c["parentRva"]) if c["parentRva"] else None
            if m["enumRva"]:
                e = enums_by_rva.get(m["enumRva"])
                if e:
                    out += enum_bytes(e)
            out += m["name"].encode("latin-1")
            out += struct.pack("<H", ty) + struct.pack("<H", sub)
            if extra:
                out += struct.pack("<H", extra)
            out += struct.pack("<H", carr) + struct.pack("<H", flags & 0xFFFF)
        out += struct.pack("<i", len(cls["members"]))
        cache[r] = bytes(out)
        return cache[r]

    for cls in classes_by_rva.values():
        buf = bytearray()
        c = cls
        while c:
            buf += class_bytes(c)
            c = classes_by_rva.get(c["parentRva"]) if c["parentRva"] else None
        cls["signature"] = zlib.crc32(bytes(buf)) & 0xFFFFFFFF


# ---------------------------------------------------------------- HKXPACK

def load_hkxpack(jar):
    db = {}
    with zipfile.ZipFile(jar) as z:
        for n in z.namelist():
            if not n.startswith("classxml/") or not n.endswith(".xml"):
                continue
            root = ET.fromstring(z.read(n))
            cname = root.get("name")
            entry = {"kind": root.tag, "version": int(root.get("version", "0")),
                     "signature": int(root.get("signature", "0"), 16), "parent": root.get("parent"),
                     "members": [], "enums": {}}
            for en in root.iter("enum"):
                entry["enums"][en.get("name")] = [[int(it.get("value")), it.get("name")] for it in en.iter("enumitem")]
            for m in root.iter("member"):
                entry["members"].append({
                    "name": m.get("name"), "offset": int(m.get("offset")),
                    "type": m.get("vtype", "TYPE_VOID")[5:], "subtype": m.get("vsubtype", "TYPE_VOID")[5:],
                    "ctype": m.get("ctype"), "etype": m.get("etype"),
                    "cArraySize": int(m.get("arrsize", "0")), "flags": m.get("flags", "FLAGS_NONE"),
                })
            db[cname] = entry
    return db


def crosscheck(classes, hkx, log):
    """Per class: agreement with HKXPACK. Returns rows for the TSV and a summary."""
    byname = {c["name"]: c for c in classes}
    rows = []
    agree = disagree = only_exe = only_hkx = 0
    sig_agree = sig_disagree = 0
    for name in sorted(set(byname) | set(hkx)):
        c, h = byname.get(name), hkx.get(name)
        if c and not h:
            only_exe += 1
            rows.append((name, "EXE_ONLY", ""))
            continue
        if h and not c:
            only_hkx += 1
            rows.append((name, "HKXPACK_ONLY", ""))
            continue
        diffs = []
        if (c["parent"] or None) != (h["parent"] or None):
            diffs.append("parent exe=%s hkx=%s" % (c["parent"], h["parent"]))
        if c["version"] != h["version"]:
            diffs.append("version exe=%d hkx=%d" % (c["version"], h["version"]))
        if len(c["members"]) != len(h["members"]):
            diffs.append("memberCount exe=%d hkx=%d" % (len(c["members"]), len(h["members"])))
        for a, b in zip(c["members"], h["members"]):
            if a["name"] != b["name"]:
                diffs.append("member name exe=%s hkx=%s" % (a["name"], b["name"]))
                continue
            if a["offset"] != b["offset"]:
                diffs.append("%s.offset exe=%d hkx=%d" % (a["name"], a["offset"], b["offset"]))
            if a["type"] != b["type"]:
                diffs.append("%s.type exe=%s hkx=%s" % (a["name"], a["type"], b["type"]))
            if a["subtype"] != b["subtype"]:
                diffs.append("%s.subtype exe=%s hkx=%s" % (a["name"], a["subtype"], b["subtype"]))
            if a["cArraySize"] != b["cArraySize"]:
                diffs.append("%s.cArraySize exe=%d hkx=%d" % (a["name"], a["cArraySize"], b["cArraySize"]))
            if flag_names(a["flags"]) != b["flags"].replace(" ", ""):
                diffs.append("%s.flags exe=%s hkx=%s" % (a["name"], flag_names(a["flags"]), b["flags"]))
            if (a.get("class") or None) != (b["ctype"] or None):
                diffs.append("%s.class exe=%s hkx=%s" % (a["name"], a.get("class"), b["ctype"]))
            if (a.get("enum") or None) != (b["etype"] or None):
                diffs.append("%s.enum exe=%s hkx=%s" % (a["name"], a.get("enum"), b["etype"]))
        ex_enums = {e["name"]: e["items"] for e in c["enums"]}
        for en, items in h["enums"].items():
            if en not in ex_enums:
                diffs.append("enum %s only in hkx" % en)
            elif ex_enums[en] != items:
                diffs.append("enum %s items differ" % en)
        for en in ex_enums:
            if en not in h["enums"]:
                diffs.append("enum %s only in exe" % en)
        if c["signature"] == h["signature"]:
            sig_agree += 1
            sig = "SIG_OK"
        else:
            sig_disagree += 1
            sig = "SIG_DIFF exe=0x%08x hkx=0x%08x" % (c["signature"], h["signature"])
            diffs.append(sig)
        c["signatureHkxpack"] = h["signature"]
        c["signatureAgrees"] = c["signature"] == h["signature"]
        if diffs:
            disagree += 1
            rows.append((name, "DIFF", "; ".join(diffs)))
        else:
            agree += 1
            rows.append((name, "AGREE", sig))
    summary = dict(both=agree + disagree, agree=agree, disagree=disagree, exe_only=only_exe, hkxpack_only=only_hkx,
                   signature_agree=sig_agree, signature_disagree=sig_disagree)
    log("cross-check: %s" % summary)
    return rows, summary


# ---------------------------------------------------------------- self-check

def selfcheck(classes, log):
    """Every member offset + size fits the class size; a class's first member is
    at or past its parent's size; parents resolve; the walk is acyclic."""
    byname = {c["name"]: c for c in classes}
    problems = []

    def member_size(m):
        t = m["type"]
        if t == "STRUCT":
            c = byname.get(m.get("class") or "")
            sz = c["objectSize"] if c else 0
        elif t in ("ENUM", "FLAGS"):
            sz = TYPE_SIZE.get(m["subtype"], 4)
        else:
            sz = TYPE_SIZE.get(t, 0)
        return sz * max(1, m["cArraySize"])

    for c in classes:
        p = byname.get(c["parent"]) if c["parent"] else None
        if c["parent"] and not p:
            problems.append("%s: parent %s not in the database" % (c["name"], c["parent"]))
        seen = set()
        q = c
        while q:
            if q["name"] in seen:
                problems.append("%s: parent cycle" % c["name"])
                break
            seen.add(q["name"])
            q = byname.get(q["parent"]) if q["parent"] else None
        base = p["objectSize"] if p else 0
        for m in c["members"]:
            end = m["offset"] + member_size(m)
            if end > c["objectSize"]:
                problems.append("%s.%s: offset %d + size %d = %d > objectSize %d" %
                                (c["name"], m["name"], m["offset"], member_size(m), end, c["objectSize"]))
            # emptyBaseClassOpt (header layout byte 3 = 1): a parent that
            # declares nothing occupies no bytes in the child, so its 1-byte
            # objectSize is not a floor for the child's first member.
            if p and m["offset"] < base and c["objectSize"] > 0 and not (base <= 1 and not p["members"]):
                problems.append("%s.%s: offset %d inside parent %s (size %d)" %
                                (c["name"], m["name"], m["offset"], p["name"], base))
    log("self-check: %d classes, %d problems" % (len(classes), len(problems)))
    for pr in problems[:40]:
        log("  " + pr)
    return problems


def main():
    global EXE
    ap = argparse.ArgumentParser(description="Build res/hkclasses_fo4.json from your installed Fallout4.exe.")
    ap.add_argument("--exe", default=os.environ.get("WW_FO4_EXE", ""),
                    help='path to Fallout4.exe, e.g. "C:/Program Files (x86)/Steam/steamapps/common/Fallout 4/Fallout4.exe"')
    ap.add_argument("--out", default="res/hkclasses_fo4.json")
    ap.add_argument("--hkxpack", default="", help="optional: hkxpack-core.jar, for a second cross-check")
    ap.add_argument("--report", default="", help="optional: folder for the log and the check tables")
    a = ap.parse_args()
    if not a.exe or not os.path.isfile(a.exe):
        ap.error('give --exe "<Fallout 4 folder>/Fallout4.exe"')
    EXE = a.exe
    logf = None
    if a.report:
        os.makedirs(a.report, exist_ok=True)
        logf = open(os.path.join(a.report, "classdb_extract.log"), "w", newline="\n")

    def log(s):
        print(s)
        if logf:
            logf.write(s + "\n")

    t0 = time.time()
    exe = Exe(EXE)
    exe_sha = hashlib.sha256(exe.pe.data).hexdigest()[:16]
    log("exe %s sha256 %s" % (os.path.basename(EXE), exe_sha))
    classes_by_rva, enums_by_rva, reg = extract(exe, log)
    crc32_signature(classes_by_rva, enums_by_rva)
    # which class DECLARES each enum (a member's enum is named by its owner)
    enum_owner = {}
    for c in classes_by_rva.values():
        for k in range(c["numEnums"]):
            enum_owner.setdefault(c["enumsRva"] + 0x28 * k, c["name"])
    # resolve rvas to names
    for c in classes_by_rva.values():
        p = classes_by_rva.get(c["parentRva"]) if c["parentRva"] else None
        c["parent"] = p["name"] if p else None
        if c["parentRva"] and not p:
            log("WARN %s: parent rva 0x%x unknown" % (c["name"], c["parentRva"]))
        for m in c["members"]:
            mc = classes_by_rva.get(m["classRva"]) if m["classRva"] else None
            m["class"] = mc["name"] if mc else None
            if m["classRva"] and not mc:
                log("WARN %s.%s: member class rva 0x%x unknown" % (c["name"], m["name"], m["classRva"]))
            m["enum"] = m.pop("enumName")
            m["enumClass"] = enum_owner.get(m["enumRva"]) if m["enumRva"] else None
            e = enums_by_rva.get(m["enumRva"]) if m["enumRva"] else None
            # an enum declared by no class in the registry is carried inline
            m["enumItems"] = e["items"] if (e and m["enumClass"] is None) else None
            if m["enumRva"] and m["enumClass"] is None:
                log("NOTE %s.%s: enum %s at 0x%x is declared by no class; items carried inline" % (c["name"], m["name"], m["enum"], m["enumRva"]))
    # A name can be built more than once: the four Havok META classes (hkClass,
    # hkClassMember, hkClassEnum, hkClassEnumItem) carry older, UNREGISTERED
    # variants for versioning old files. The registered object is THE class; the
    # others are kept under "variants" so nothing read is thrown away.
    dup = {}
    for c in classes_by_rva.values():
        dup.setdefault(c["name"], []).append(c)
    classes = []
    for n, lst in sorted(dup.items()):
        lst.sort(key=lambda x: (x.get("registryIndex", -1) < 0, x["rva"]))
        main = lst[0]
        main["variants"] = lst[1:]
        if len(lst) > 1:
            log("NOTE %s built %d times; registered one at 0x%x kept, %d variant(s) recorded" % (n, len(lst), main["rva"], len(lst) - 1))
        classes.append(main)

    summary = None
    if a.hkxpack:
        hkx = load_hkxpack(a.hkxpack)
        log("HKXPACK classxml: %d classes" % len(hkx))
        rows, summary = crosscheck(classes, hkx, log)
        if a.report:
            with open(os.path.join(a.report, "classdb_crosscheck.tsv"), "w", newline="\n") as f:
                f.write("class\tverdict\tdetail\n")
                for r in rows:
                    f.write("\t".join(r) + "\n")
    problems = selfcheck(classes, log)
    if a.report:
        with open(os.path.join(a.report, "classdb_selfcheck.txt"), "w", newline="\n") as f:
            f.write("\n".join(problems) + ("\n" if problems else "no problems\n"))

    out = {
        "format": "ww-hkclassdb-1",
        "source": {
            "exe": os.path.basename(EXE), "exeSha256_16": exe_sha, "registryEntries": len(reg),
            "hkxpackJar": os.path.basename(a.hkxpack) if a.hkxpack else None,
            "extractedBy": "tools/hkclassdb_extract.py", "extractedOn": time.strftime("%Y-%m-%d"),
        },
        "havokVersion": "hk_2014.1.0-r1",
        "typeNames": TYPES, "memberFlagNames": MEMBER_FLAGS,
        "crosscheck": summary, "selfcheckProblems": len(problems),
        "classes": [],
    }
    for c in classes:
        out["classes"].append({
            "name": c["name"], "rva": "0x%08x" % c["rva"], "registryIndex": c.get("registryIndex", -1),
            "parent": c["parent"], "objectSize": c["objectSize"], "version": c["version"], "flags": c["flags"],
            "numImplementedInterfaces": c["numImplementedInterfaces"],
            "signature": "0x%08x" % c["signature"],
            "signatureHkxpack": ("0x%08x" % c["signatureHkxpack"]) if "signatureHkxpack" in c else None,
            "hasDefaults": bool(c["defaultsRva"]),
            "variants": [{"rva": "0x%08x" % v["rva"], "signature": "0x%08x" % v["signature"], "version": v["version"],
                          "objectSize": v["objectSize"], "numMembers": len(v["members"])} for v in c.get("variants", [])],
            "enums": [{"name": e["name"], "items": e["items"]} for e in c["enums"]],
            "members": [{"name": m["name"], "offset": m["offset"], "type": m["type"], "subtype": m["subtype"],
                         "cArraySize": m["cArraySize"], "flags": m["flags"], "flagNames": flag_names(m["flags"]),
                         "class": m["class"], "enum": m["enum"], "enumClass": m["enumClass"],
                         "enumItems": m["enumItems"]} for m in c["members"]],
        })
    os.makedirs(os.path.dirname(a.out) or ".", exist_ok=True)
    with open(a.out, "w", newline="\n") as f:
        json.dump(out, f, indent=1)
    log("wrote %s: %d classes (%d registered), %.1f s" % (a.out, len(classes), sum(1 for c in classes if c.get("registryIndex", -1) >= 0), time.time() - t0))
    if logf:
        logf.close()


if __name__ == "__main__":
    main()
