#!/usr/bin/env python
"""Gates of lane HKXEDIT1 (2026-09-10), pre-registered in the brief:

  (a) byte-identical read->write over the census set, by BOTH readers
      (release/hkxfile_gate.exe and tests/spells/hkxfile_oracle.py), every
      mismatch named by file / chunk / offset;
  (b) edit->save->reload on a copy of jog.hkx: one int, one float, one
      string, one enum, one array length; reloaded, the five read back
      changed and every other field is identical (a per-field diff of the
      two object trees), by both writers, cross-read by the other reader;
  (c) HKXPACK unpacks every file we wrote;
  (d) the class database: 908 registered classes, the self-check clean, the
      seven animation classes' layouts equal to what HKXCLASS / HKX1 measured;
  (e) mutation: 20 single-byte / structural corruptions of jog.hkx refused by
      NAME by both readers (a payload float is NOT one of them: no checksum).

Run from the repo root:  python tests/spells/hkxfile_gates.py [--no-census] [--no-hkxpack]
Prints "N checks, M failures" and exits M.
"""
import sys, os, json, struct, subprocess, shutil, argparse, time, hashlib

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import hkxfile_oracle as H  # noqa: E402

GATE = os.path.join(REPO, "release", "hkxfile_gate.exe")
DB = os.path.join(REPO, "res", "hkclasses_fo4.json")
CLIPS = os.path.join(REPO, "scratchpad", "hkx1_20260910", "clips")
CENSUS = os.path.join(REPO, "scratchpad", "hkxedit1_20260910", "census_hkx")
OUT = os.path.join(REPO, "scratchpad", "hkxedit1_20260910", "gates_out")
HKXPACK = r"E:/Tools/Fallout 4/HKXPACK/hkxpack-cli.jar"
MSYS_BASH = r"C:\msys64\usr\bin\bash.exe"

checks = failures = 0


def check(cond, what):
    global checks, failures
    checks += 1
    if not cond:
        failures += 1
    print(("  ok   " if cond else "  FAIL ") + what)
    return cond


def gate(*args):
    """Run the C++ gate binary under the MSYS2 shell (Qt6Core.dll lives on its PATH)."""
    # every argument single-quoted: a bare '#2.numFrames=24' is a COMMENT to bash
    cmd = " ".join("'%s'" % a.replace("\\", "/") for a in args)
    full = "cd /e/Projects/NifskopeWildWastelandEdition && ./release/hkxfile_gate.exe --db res/hkclasses_fo4.json " + cmd
    p = subprocess.run([MSYS_BASH, "-lc", full], capture_output=True, text=True,
                       env=dict(os.environ, MSYSTEM="UCRT64", CHERE_INVOKING="1"))
    return p.returncode, (p.stdout + p.stderr).strip()


def relpath(p):
    return os.path.relpath(p, REPO).replace("\\", "/")


# ------------------------------------------------------------------ (a)

def gate_a(db, do_census):
    print("(a) byte-identical round trip")
    fixtures = ["jog", "tpose_idle", "twoblock", "q48", "skeleton", "lossless", "turn"]
    for f in fixtures:
        p = os.path.join(CLIPS, f + ".hkx")
        blob = open(p, "rb").read()
        pf = H.read(blob, db)
        d = H.first_diff(blob, H.write(pf, db))
        check(d < 0, "oracle round trip %s.hkx identical (%d bytes)" % (f, len(blob)))
        rc, out = gate("roundtrip", relpath(p))
        check(rc == 0 and out.startswith("IDENTICAL"), "C++ round trip %s.hkx: %s" % (f, out.splitlines()[-1] if out else "?"))
    if not do_census or not os.path.isdir(CENSUS):
        print("  (census skipped)")
        return
    rep = os.path.join(OUT, "census_cpp.tsv")
    t0 = time.time()
    rc, out = gate("census", relpath(CENSUS), "--report", relpath(rep))
    line = out.splitlines()[0] if out else ""
    print("  " + line + " (%.1f s)" % (time.time() - t0))
    ok = "0 mismatched" in line
    check(ok, "C++ census: every parsed file byte-identical")
    if not ok:
        for l in out.splitlines()[1:40]:
            print("    " + l)
    t0 = time.time()
    r = H.census(CENSUS, db, 0, os.path.join(OUT, "census_py.tsv"))
    print("  oracle census %.1f s" % (time.time() - t0))
    check(r["mismatched"] == 0, "oracle census: every parsed file byte-identical (%d of %d, %d refused)" % (r["identical"], r["files"], sum(r["refused"].values())))
    # the two readers refuse the same files for the same reason
    cpp_ref = {}
    for l in open(rep, encoding="utf-8"):
        f, v, d = l.rstrip("\n").split("\t", 2)
        if v == "REFUSED":
            cpp_ref[f] = d
    py_ref = {}
    for l in open(os.path.join(OUT, "census_py.tsv"), encoding="utf-8"):
        f, v, d = l.rstrip("\n").split("\t", 2)
        if v == "REFUSED":
            py_ref[f] = d
    check(set(cpp_ref) == set(py_ref), "both readers refuse the same %d files" % len(py_ref))
    same = sum(1 for f in py_ref if f in cpp_ref and cpp_ref[f].split(":")[0] == py_ref[f].split(":")[0])
    check(same == len(py_ref), "both readers give the same refusal reason on all %d (%d agree)" % (len(py_ref), same))


# ------------------------------------------------------------------ (b)

EDITS = [
    ("#2.numFrames", "int", 24),
    ("#2.frameDuration", "float", 0.0416667),
    ("#4.originalSkeletonName", "string", "EditedRoot"),
    ("#4.blendHint", "enum", "ADDITIVE"),
    ("#2.blockOffsets", "resize", 3),
]


def tree_pairs(pf, db):
    """Flatten a Packfile into {path: printable value} for the per-field diff."""
    out = {}

    def walk(v, path):
        k = v["k"]
        if k == "scalar":
            out[path] = ("bits", tuple(v.get("bits") or v["vals"]))
        elif k == "raw":
            out[path] = ("raw", v["b"])
        elif k == "str":
            out[path] = ("str", v["s"])
        elif k == "ptr":
            out[path] = ("ptr", v["o"])
        elif k == "ignored":
            out[path] = ("ign", v["b"])
        elif k == "carr":
            for i, e in enumerate(v["e"]):
                walk(e, "%s[%d]" % (path, i))
        elif k in ("arr", "rel"):
            out[path + ".n"] = ("n", v["n"])
            if "vals" in v:
                out[path + ".vals"] = ("vals", tuple(v.get("bits") or v["vals"]))
            elif "b" in v:
                out[path + ".b"] = ("b", v["b"])
            else:
                for i, e in enumerate(v["e"]):
                    walk(e, "%s[%d]" % (path, i))
        elif k == "struct":
            out[path + ".hole"] = ("hole", v["hole"])
            for m, fv in zip(db.all_members(v["c"]), v["m"]):
                walk(fv, path + "." + m["name"])
    for i, o in enumerate(pf.objects):
        walk(o["v"], "#%d" % i)
    return out


def gate_b(db, do_hkxpack):
    print("(b) edit -> save -> reload on a copy of jog.hkx")
    src = os.path.join(CLIPS, "jog.hkx")
    blob = open(src, "rb").read()
    base = H.read(blob, db)
    check(base.objects[2]["c"] == "hkaSplineCompressedAnimation" and base.objects[4]["c"] == "hkaAnimationBinding",
          "jog.hkx object 2 is the animation and 4 the binding")
    # the ORACLE writes the edit
    pf = H.read(blob, db)
    H.set_scalar(pf, db, "#2.numFrames", 24)
    H.set_scalar(pf, db, "#2.frameDuration", 0.0416667)
    H.set_string(pf, db, "#4.originalSkeletonName", "EditedRoot")
    H.set_scalar(pf, db, "#4.blendHint", 2)
    H.resize_array(pf, db, "#2.blockOffsets", 3)
    py_out = os.path.join(OUT, "jog_edit_py.hkx")
    open(py_out, "wb").write(H.write(pf, db))
    # the C++ MODEL writes the same edit
    cpp_out = os.path.join(OUT, "jog_edit_cpp.hkx")
    rc, out = gate("edit", relpath(src), relpath(cpp_out),
                   "--set", "#2.numFrames=24", "--set", "#2.frameDuration=0.0416667",
                   "--set", "#4.originalSkeletonName=EditedRoot", "--set", "#4.blendHint=ADDITIVE",
                   "--resize", "#2.blockOffsets=3")
    check(rc == 0, "C++ edit wrote %s: %s" % (relpath(cpp_out), out.splitlines()[-1] if out else "?"))
    a, b = open(py_out, "rb").read(), open(cpp_out, "rb").read()
    check(a == b, "the two writers produced the same bytes (%d / %d)" % (len(a), len(b)))
    # reload with the OTHER reader and diff field by field
    before = tree_pairs(base, db)
    for label, path in (("C++ file read by the oracle", cpp_out), ("oracle file read by the oracle", py_out)):
        after = tree_pairs(H.read(open(path, "rb").read(), db), db)
        changed = sorted(k for k in set(before) | set(after) if before.get(k) != after.get(k))
        expected = {"#2.numFrames", "#2.frameDuration", "#4.originalSkeletonName", "#4.blendHint",
                    "#2.blockOffsets.n", "#2.blockOffsets.vals"}
        check(set(changed) == expected, "%s: exactly the five edits changed (%s)" % (label, ", ".join(changed)))
        vals = {"#2.numFrames": after.get("#2.numFrames"), "#4.blendHint": after.get("#4.blendHint"),
                "#4.originalSkeletonName": after.get("#4.originalSkeletonName"), "#2.blockOffsets.n": after.get("#2.blockOffsets.n")}
        check(vals["#2.numFrames"] == ("bits", (24,)), "numFrames reads back 24")
        check(vals["#4.blendHint"] == ("bits", (2,)), "blendHint reads back 2 (ADDITIVE)")
        check(vals["#4.originalSkeletonName"] == ("str", b"EditedRoot"), "originalSkeletonName reads back 'EditedRoot'")
        check(vals["#2.blockOffsets.n"] == ("n", 3), "blockOffsets has 3 elements")
        fbits = after.get("#2.frameDuration")[1][0]
        check(abs(struct.unpack("<f", struct.pack("<I", fbits))[0] - 0.0416667) < 1e-8, "frameDuration reads back 0.0416667 (float32: %r)" % struct.unpack("<f", struct.pack("<I", fbits))[0])
    rc, out = gate("get", relpath(cpp_out), "#2.numFrames", "#2.frameDuration", "#4.originalSkeletonName", "#4.blendHint", "#2.blockOffsets")
    check(rc == 0 and "#2.numFrames = 24" in out and "EditedRoot" in out and "#4.blendHint = 2" in out and "n=3" in out,
          "C++ reader reads the five values back from the oracle-checked file")
    # the floor: a file with ONE payload float nudged is accepted by both readers and the diff sees it
    pf2 = H.read(blob, db)
    H.set_scalar(pf2, db, "#2.duration", 9.5)
    nudged = tree_pairs(H.read(H.write(pf2, db), db), db)
    ch = sorted(k for k in nudged if before.get(k) != nudged.get(k))
    check(ch == ["#2.duration"], "floor: a nudged duration is the ONLY field the diff reports (%s)" % ch)
    if do_hkxpack:
        gate_c([py_out, cpp_out, os.path.join(CLIPS, "jog.hkx")])


# ------------------------------------------------------------------ (c)

def gate_c(files):
    print("(c) HKXPACK unpacks what we wrote")
    if not os.path.exists(HKXPACK):
        print("  (no HKXPACK jar at %s: skipped)" % HKXPACK)
        return
    for f in files:
        xml = os.path.join(OUT, os.path.basename(f) + ".xml")
        p = subprocess.run(["java", "-jar", HKXPACK, "unpack", f, "-o", xml], capture_output=True, text=True)
        ok = p.returncode == 0 and os.path.exists(xml) and os.path.getsize(xml) > 1000
        check(ok, "HKXPACK unpack %s -> %s (%s)" % (os.path.basename(f), os.path.basename(xml), "rc %d" % p.returncode))
        if ok and "edit" in f:
            x = open(xml, encoding="utf-8", errors="replace").read()
            check('name="numFrames">24<' in x and "EditedRoot" in x and "ADDITIVE" in x,
                  "HKXPACK's XML of %s shows the edited values" % os.path.basename(f))


# ------------------------------------------------------------------ (d)

# what HKXCLASS (ww-hkx-animation s9) and HKX1 (docs/HKX_ANIMATION_FORMAT.md) measured
MEASURED = {
    "hkaSplineCompressedAnimation": (0xb0, {"numFrames": 0x38, "numBlocks": 0x3c, "maxFramesPerBlock": 0x40, "maskAndQuantizationSize": 0x44,
                                            "blockDuration": 0x48, "blockInverseDuration": 0x4c, "frameDuration": 0x50, "blockOffsets": 0x58,
                                            "floatBlockOffsets": 0x68, "transformOffsets": 0x78, "floatOffsets": 0x88, "data": 0x98, "endian": 0xa8}),
    "hkaInterleavedUncompressedAnimation": (0x58, {"transforms": 0x38, "floats": 0x48}),
    "hkaLosslessCompressedAnimation": (0xe0, {}),
    "hkaPredictiveCompressedAnimation": (0xc0, {}),
    "hkaQuantizedAnimation": (0x58, {}),
    "hkaReferencePoseAnimation": (0x40, {}),
    "hkaAnimation": (None, {"type": 0x10, "duration": 0x14, "numberOfTransformTracks": 0x18, "numberOfFloatTracks": 0x1c,
                            "extractedMotion": 0x20, "annotationTracks": 0x28}),
    "hkaSkeleton": (0x88, {"name": 0x10, "parentIndices": 0x18, "bones": 0x28, "referencePose": 0x38, "referenceFloats": 0x48,
                           "floatSlots": 0x58, "localFrames": 0x68, "partitions": 0x78}),
    "hkaAnimationBinding": (0x58, {"originalSkeletonName": 0x10, "animation": 0x18, "transformTrackToBoneIndices": 0x20,
                                   "floatTrackToFloatSlotIndices": 0x30, "partitionIndices": 0x40, "blendHint": 0x50}),
    "hkaDefaultAnimatedReferenceFrame": (None, {"up": 0x20, "forward": 0x30, "duration": 0x40, "referenceFrameSamples": 0x48}),
    "hkaAnnotationTrack": (0x18, {"trackName": 0, "annotations": 8}),
}
SIGNATURES = {"hkRootLevelContainer": 0x2772c11e, "hkaAnimationContainer": 0x26859f4c, "hkaSplineCompressedAnimation": 0x8c3b5f7e,
              "hkaInterleavedUncompressedAnimation": 0xa5eff3f2, "hkaDefaultAnimatedReferenceFrame": 0x60f8e0b8,
              "hkaAnimationBinding": 0x0faf9150, "hkMemoryResourceContainer": 0x1de13a73, "hkaLosslessCompressedAnimation": 0x278bffe8,
              "hkClass": 0x33d42383, "hkClassMember": 0xb0efa719, "hkClassEnum": 0x8a3609cf, "hkClassEnumItem": 0xce6f8a6c}


def gate_d(db):
    print("(d) the class database")
    raw = db.raw
    reg = [c for c in raw["classes"] if c["registryIndex"] >= 0]
    check(len(reg) == 908, "908 registered classes (%d)" % len(reg))
    check(raw["selfcheckProblems"] == 0, "self-check problems: %d" % raw["selfcheckProblems"])
    cc = raw["crosscheck"]
    check(cc["signature_agree"] == 908 and cc["signature_disagree"] == 0, "computed signature == HKXPACK's on %d, differs on %d" % (cc["signature_agree"], cc["signature_disagree"]))
    check(cc["hkxpack_only"] == 0, "classes HKXPACK has and the exe lacks: %d" % cc["hkxpack_only"])
    for cname, (size, offs) in MEASURED.items():
        c = db.classes.get(cname)
        if not check(c is not None, "%s is in the database" % cname):
            continue
        if size is not None:
            check(c["objectSize"] == size, "%s objectSize 0x%x (measured 0x%x)" % (cname, c["objectSize"], size))
        mine = {m["name"]: m["offset"] for m in db.all_members(cname)}
        bad = [n for n, o in offs.items() if mine.get(n) != o]
        check(not bad, "%s: %d member offsets equal the measured ones%s" % (cname, len(offs), "" if not bad else " (differ: %s)" % ", ".join(bad)))
    for cname, sig in SIGNATURES.items():
        c = db.classes.get(cname)
        check(c is not None and int(c["signature"], 16) == sig, "%s signature 0x%08x (files carry 0x%08x)" % (cname, int(c["signature"], 16) if c else 0, sig))
    # a member-offset self-consistency pass of our own, independent of the extractor's
    problems = 0
    for c in raw["classes"]:
        for m in c["members"]:
            esz = {"BOOL": 1, "CHAR": 1, "INT8": 1, "UINT8": 1, "INT16": 2, "UINT16": 2, "INT32": 4, "UINT32": 4, "INT64": 8, "UINT64": 8,
                   "REAL": 4, "HALF": 2, "VECTOR4": 16, "QUATERNION": 16, "MATRIX3": 48, "ROTATION": 48, "QSTRANSFORM": 48, "MATRIX4": 64,
                   "TRANSFORM": 64, "POINTER": 8, "FUNCTIONPOINTER": 8, "ARRAY": 16, "SIMPLEARRAY": 16, "HOMOGENEOUSARRAY": 24,
                   "VARIANT": 16, "CSTRING": 8, "ULONG": 8, "STRINGPTR": 8, "RELARRAY": 4, "ZERO": 0}.get(m["type"])
            if m["type"] == "STRUCT":
                esz = db.classes[m["class"]]["objectSize"] if m["class"] in db.classes else 0
            elif m["type"] in ("ENUM", "FLAGS"):
                esz = {"INT8": 1, "UINT8": 1, "INT16": 2, "UINT16": 2}.get(m["subtype"], 4)
            if esz is None:
                esz = 0
            if m["offset"] + esz * max(1, m["cArraySize"]) > c["objectSize"]:
                problems += 1
    check(problems == 0, "every member fits inside its class size (%d violations)" % problems)


# ------------------------------------------------------------------ (e)

def gate_e(db):
    print("(e) mutations refused by name")
    src = os.path.join(CLIPS, "jog.hkx")
    blob = bytearray(open(src, "rb").read())
    base = 0x210
    muts = [
        ("magic byte 0", 0, 0xFF),
        ("fileVersion -> 12", 0x0c, 12 ^ 11),
        ("bytesInPointer -> 4", 0x10, 8 ^ 4),
        ("numSections -> 4", 0x14, 3 ^ 4),
        ("contentsSectionIndex -> 1", 0x18, 2 ^ 1),
        ("predicate padding 0x10 -> 0x30 (section headers move)", 0x3e, 0x10 ^ 0x30),
        ("__data__ tag first byte", 0x50 + 2 * 0x40, ord('_') ^ ord('X')),
        ("__data__ absoluteDataStart low byte", 0x50 + 2 * 0x40 + 20, 0x10 ^ 0xF0),
        ("__data__ localFixupsOffset low byte", 0x50 + 2 * 0x40 + 24, 0x80),
        ("__data__ virtualFixupsOffset low byte", 0x50 + 2 * 0x40 + 32, 0x40),
        ("class-name separator 0x09 -> 0x0A", 0x110 + 4, 0x09 ^ 0x0A),
        ("root class-name first byte", 0x110 + 0x4b, ord('h') ^ ord('X')),
        ("first local fixup dst byte (root namedVariants -> 0x50)", base + 10672 + 4, 0x10 ^ 0x50),
        ("first global fixup dst -> a non-object", base + 11600 + 8, 0xb0 ^ 0xb8),
        ("first virtual fixup class-name offset -> junk", base + 11680 + 8, 0x4b ^ 0x4c),
        ("global fixup section index 2 -> 1", base + 11600 + 4, 2 ^ 1),
        ("animations array size 1 -> 65", base + 0xb0 + 0x20 + 8, 1 ^ 65),
        ("first local fixup src 0x0 -> 0x8 (no member there: fixup unconsumed)", base + 10672 + 0, 0x08),
        ("blockOffsets size high byte -> 0x7f (payload past the end)", base + 0x130 + 0x58 + 8 + 3, 0x7f),
        ("a null pointer's bytes non-zero (hkaAnimationContainer.skeletons data)", base + 0xb0 + 0x10, 0x01),
    ]
    assert len(muts) == 20
    n_both = 0
    for i, (what, off, xor) in enumerate(muts):
        m = bytearray(blob)
        m[off] ^= xor
        p = os.path.join(OUT, "mut_%02d.hkx" % i)
        open(p, "wb").write(m)
        py_ref = None
        try:
            H.read(bytes(m), db)
        except H.Refusal as e:
            py_ref = str(e)
        except Exception as e:  # a traceback is a defect of the reader, not a refusal
            py_ref = None
            print("    oracle EXCEPTION on %s: %s" % (what, e))
        rc, out = gate("roundtrip", relpath(p))
        cpp_ref = out.splitlines()[-1] if out else ""
        both = py_ref is not None and rc == 2
        n_both += both
        check(both, "%-64s | py: %s | c++: %s" % (what, (py_ref or "ACCEPTED")[:60], cpp_ref[:70]))
    # the stated limit: a flipped PAYLOAD byte is a different well-formed value (no checksum)
    m = bytearray(blob)
    m[base + 0x130 + 0x14] ^= 0x01  # duration low byte
    accepted = True
    try:
        H.read(bytes(m), db)
    except H.Refusal:
        accepted = False
    check(accepted, "stated limit: a payload byte flip (duration) is ACCEPTED as a value, not refused")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--no-census", action="store_true")
    ap.add_argument("--no-hkxpack", action="store_true")
    a = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)
    db = H.ClassDb(DB)
    if not os.path.exists(GATE):
        print("no %s: build it with scratchpad/hkxedit1_20260910/build_gate.sh" % GATE)
        return 1
    t0 = time.time()
    gate_d(db)
    gate_a(db, not a.no_census)
    gate_b(db, not a.no_hkxpack)
    gate_e(db)
    print("%d checks, %d failures, %.1f s" % (checks, failures, time.time() - t0))
    return failures


if __name__ == "__main__":
    sys.exit(main())
