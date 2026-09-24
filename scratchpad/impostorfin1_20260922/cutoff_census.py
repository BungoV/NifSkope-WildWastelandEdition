"""IMPOSTORFIN1 job 1 -- the vanilla alpha cut-off census, whole corpus, offline.

Reads every NIF under Meshes/LOD and every .BTO under Meshes/Terrain of the
vanilla corpus (E:/Tools/Fallout 4/DataUnpacked/Data), parses each shape's
NiAlphaProperty (flags + threshold) and its BSLightingShaderProperty name
(the BGSM path in FO4), and reads that BGSM's own alpha fields
(blend mode byte, alpha test ref, alpha test bool). Also every BGSM under
Materials/LOD.

Class of a shape: 'tree' when the NIF lives under LOD/Landscape/Trees, or the
material/texture path names a tree LOD material (Materials/LOD/Trees or a
Textures/LOD/.../Trees path) -- the BTO case, where the path of the chunk says
nothing.

Prints TABLES: value -> count with 3 example paths, per class.
"""
import os, struct, sys, collections, glob

DATA = "E:/Tools/Fallout 4/DataUnpacked/Data"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cutoff_census.txt")


def rd_nif(path):
    b = open(path, "rb").read()
    nl = b.index(b"\n")
    p = nl + 1
    ver, = struct.unpack_from("<I", b, p); p += 4
    p += 1  # endian
    uver, nblk, bsver = struct.unpack_from("<III", b, p); p += 12
    # BSStreamHeader for bsver 130: author, process script, export script, max filepath
    nstr = 4 if bsver <= 130 else 3
    if bsver > 130:
        n = b[p]; p += 1 + n; p += 4
        nstr = 2
        for _ in range(nstr):
            n = b[p]; p += 1 + n
    else:
        for _ in range(4):
            n = b[p]; p += 1 + n
    ntypes, = struct.unpack_from("<H", b, p); p += 2
    types = []
    for _ in range(ntypes):
        n, = struct.unpack_from("<I", b, p); p += 4
        types.append(b[p:p + n].decode("latin1")); p += n
    tidx = struct.unpack_from("<%dH" % nblk, b, p); p += 2 * nblk
    sizes = struct.unpack_from("<%dI" % nblk, b, p); p += 4 * nblk
    nstrings, maxlen = struct.unpack_from("<II", b, p); p += 8
    strings = []
    for _ in range(nstrings):
        n, = struct.unpack_from("<I", b, p); p += 4
        strings.append(b[p:p + n].decode("latin1")); p += n
    ngroups, = struct.unpack_from("<I", b, p); p += 4 + 4 * ngroups
    blocks = []
    for i in range(nblk):
        blocks.append((types[tidx[i]], p, sizes[i]))
        p += sizes[i]
    return b, bsver, blocks, strings


def objnet(b, p, shader_type=False):
    if shader_type:
        p += 4
    name, = struct.unpack_from("<i", b, p); p += 4
    nx, = struct.unpack_from("<I", b, p); p += 4 + 4 * nx
    p += 4  # controller
    return name, p


def parse_file(path):
    b, bsver, blocks, strings = rd_nif(path)
    if bsver != 130:
        return None
    alpha = {}
    shader = {}
    texset = {}
    shapes = []
    for i, (t, p, sz) in enumerate(blocks):
        if t == "NiAlphaProperty":
            _, q = objnet(b, p)
            flags, thr = struct.unpack_from("<HB", b, q)
            alpha[i] = (flags, thr)
        elif t == "BSLightingShaderProperty":
            name, q = objnet(b, p, True)
            q += 8 + 16
            ts, = struct.unpack_from("<i", b, q)
            shader[i] = (strings[name] if 0 <= name < len(strings) else "", ts)
        elif t == "BSShaderTextureSet":
            n, = struct.unpack_from("<I", b, p); q = p + 4
            tex = []
            for _ in range(n):
                m, = struct.unpack_from("<I", b, q); q += 4
                tex.append(b[q:q + m].decode("latin1")); q += m
            texset[i] = tex
        elif t in ("BSTriShape", "BSMeshLODTriShape", "BSSubIndexTriShape"):
            name, q = objnet(b, p)
            q += 4 + 12 + 36 + 4 + 4 + 16 + 4
            sh, al = struct.unpack_from("<ii", b, q)
            shapes.append((strings[name] if 0 <= name < len(strings) else "", sh, al))
    out = []
    for nm, sh, al in shapes:
        mat, ts = shader.get(sh, ("", -1))
        tex = texset.get(ts, [])
        out.append((nm, mat, tex[0] if tex else "", alpha.get(al)))
    return out


def rd_bgsm(rel):
    if not rel:
        return None
    r = rel.replace("\\", "/")
    if not r.lower().startswith("materials/"):
        r = "Materials/" + r
    f = os.path.join(DATA, r)
    if not os.path.exists(f):
        # case-insensitive fallback
        lo = r.lower()
        for cand in BGSM_ALL:
            if cand.lower().endswith(lo):
                f = cand; break
        else:
            return "missing"
    b = open(f, "rb").read()
    if b[:4] != b"BGSM":
        return "notbgsm"
    # BaseMaterialFile: magic, version, tileflags, uoff voff uscale vscale, alpha,
    # blend enable byte, src u32, dst u32, alphaTestRef byte, alphaTest bool
    ver, tile = struct.unpack_from("<II", b, 4)
    alpha, = struct.unpack_from("<f", b, 28)
    blend = b[32]
    src, dst = struct.unpack_from("<II", b, 33)
    ref = b[41]; test = b[42]
    return (blend, src, dst, ref, test, round(alpha, 3))


BGSM_ALL = glob.glob(os.path.join(DATA, "Materials", "**", "*.bgsm"), recursive=True) + \
           glob.glob(os.path.join(DATA, "Materials", "**", "*.BGSM"), recursive=True)
BGSM_ALL = sorted(set(p.replace("\\", "/") for p in BGSM_ALL))


def is_tree(relpath, mat, tex):
    s = (relpath + "|" + mat + "|" + tex).lower().replace("\\", "/")
    return ("landscape/trees" in s) or ("lod/trees" in s) or ("/trees/" in s and "lod" in s)


def func_name(f):
    return ["ALWAYS", "LESS", "EQUAL", "LEQUAL", "GREATER", "NOTEQUAL", "GEQUAL", "NEVER"][(f >> 10) & 7]


def main():
    lines = []
    def say(s=""):
        print(s); lines.append(s)
    nifs = sorted(glob.glob(os.path.join(DATA, "Meshes", "LOD", "**", "*.nif"), recursive=True))
    btos = sorted(glob.glob(os.path.join(DATA, "Meshes", "Terrain", "**", "*.bto"), recursive=True) +
                  glob.glob(os.path.join(DATA, "Meshes", "Terrain", "**", "*.BTO"), recursive=True))
    btos = sorted(set(btos))
    say("corpus %s" % DATA)
    say("files: %d LOD NIFs under Meshes/LOD, %d BTO chunks under Meshes/Terrain" % (len(nifs), len(btos)))
    tabs = collections.defaultdict(lambda: collections.defaultdict(list))
    bgsm_tabs = collections.defaultdict(lambda: collections.defaultdict(list))
    shape_counts = collections.Counter()
    bad = 0
    mats_seen = {}
    for kind, files in (("LOD nif", nifs), ("BTO", btos)):
        for f in files:
            rel = os.path.relpath(f, DATA).replace("\\", "/")
            try:
                shapes = parse_file(f)
            except Exception as e:
                bad += 1; continue
            if shapes is None:
                bad += 1; continue
            for nm, mat, tex, al in shapes:
                cls = "tree" if is_tree(rel if kind == "LOD nif" else "", mat, tex) else "other"
                key = (kind, cls)
                shape_counts[key] += 1
                if al is None:
                    v = "no NiAlphaProperty"
                else:
                    fl, thr = al
                    v = "flags 0x%04X (blend %d, test %d %s) threshold %d" % (
                        fl, fl & 1, (fl >> 9) & 1, func_name(fl), thr)
                ex = "%s [%s | %s]" % (rel, mat or "-", tex or "-")
                tabs[key][v].append(ex)
                if mat and mat.lower().endswith(".bgsm"):
                    if mat not in mats_seen:
                        mats_seen[mat] = rd_bgsm(mat)
                    bm = mats_seen[mat]
                    if isinstance(bm, tuple):
                        bv = "BGSM alphaTest %d ref %d | blend %d src %d dst %d | alpha %s" % (
                            bm[4], bm[3], bm[0], bm[1], bm[2], bm[5])
                    else:
                        bv = "BGSM " + str(bm)
                    bgsm_tabs[key][bv].append("%s [%s]" % (rel, mat))
    say("unparsed files: %d" % bad)
    for key in sorted(tabs):
        say("")
        say("== %s / %s : %d shapes -- NiAlphaProperty" % (key[0], key[1], shape_counts[key]))
        for v, ex in sorted(tabs[key].items(), key=lambda kv: -len(kv[1])):
            say("  %6d  %s" % (len(ex), v))
            seen = []
            for e in ex:
                fp = e.split(" [")[0]
                if fp not in [s.split(" [")[0] for s in seen]:
                    seen.append(e)
                if len(seen) == 3: break
            for e in seen:
                say("            e.g. %s" % e)
        if key in bgsm_tabs:
            say("  -- the BGSM each of those shapes names:")
            for v, ex in sorted(bgsm_tabs[key].items(), key=lambda kv: -len(kv[1])):
                say("  %6d  %s" % (len(ex), v))
                seen = []
                for e in ex:
                    if e not in seen: seen.append(e)
                    if len(seen) == 3: break
                for e in seen:
                    say("            e.g. %s" % e)
    # every BGSM under Materials/LOD
    say("")
    lodb = [p for p in BGSM_ALL if "/materials/lod/" in p.lower()]
    say("== every BGSM under Materials/LOD: %d files" % len(lodb))
    t2 = collections.defaultdict(list)
    for p in lodb:
        rel = os.path.relpath(p, DATA).replace("\\", "/")
        bm = rd_bgsm(rel)
        cls = "tree" if "/trees/" in rel.lower() else "other"
        if isinstance(bm, tuple):
            v = "%s: alphaTest %d ref %d | blend %d" % (cls, bm[4], bm[3], bm[0])
        else:
            v = "%s: %s" % (cls, bm)
        t2[v].append(rel)
    for v, ex in sorted(t2.items(), key=lambda kv: (kv[0].split(":")[0], -len(kv[1]))):
        say("  %4d  %s" % (len(ex), v))
        for e in ex[:3]:
            say("          e.g. %s" % e)
    open(OUT, "w").write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
