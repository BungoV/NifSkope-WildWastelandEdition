"""pbr_r2a_fixtures.py -- the loose PBR test folder the R2a gates read (lane PBRR2A,
docs/NIFSKOPE_PBR_RENDERER.md s5.2 + RULINGS).

    python tests/spells/pbr_r2a_fixtures.py [<out>]      (default tests/fixtures/pbr_r2a_data)

A data root like pbr_r1_fixtures.py's: each case NIF is a byte copy of the vanilla
AC duct connector with its ONE material string rewritten to a same-length name;
each material is the vanilla duct BGSM plus a sibling v6 .pbrm (route sibling)
whose base texture is the case's. Nothing is written outside <out>; the vanilla
tree is only read.

  studio      base = the vanilla duct _d (DXT1, legacy header)   EV / grey / pictures
  srgbtag     base = DX10 twin, DXGI 72 BC1_UNORM_SRGB            tag gate
  unormtag    base = DX10 twin, DXGI 71 BC1_UNORM, same blocks    tag gate
  flatbase    base = a flat 4x4 R8G8B8A8 grey (128)               tag gate's non-vacuity control
"""
import json, os, shutil, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from pbr_r1_fixtures import pbrm, DATA, DUCT_NIF, DUCT_BGSM, ORIG_MAT, SHAPE, TEX_D, TEX_N  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(ROOT, "tests", "fixtures", "pbr_r2a_data")
DIR = "WWPbrTest02"


def mat_name(stem):
    s = "Materials\\%s\\%s.BGSM" % (DIR, stem)
    assert len(s) == len(ORIG_MAT), (s, len(s))
    return s


def w(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def dx10_header(width, height, mips, dxgi, linear_size):
    pf = struct.pack("<II4s5I", 32, 0x4, b"DX10", 0, 0, 0, 0, 0)
    h = struct.pack("<7I", 124, 0x000A1007, height, width, linear_size, 0, mips) + b"\0" * 44 + pf
    h += struct.pack("<5I", 0x401008, 0, 0, 0, 0)
    assert len(h) == 124
    return b"DDS " + h + struct.pack("<5I", dxgi, 3, 0, 1, 0)


def main():
    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    nif = open(DUCT_NIF, "rb").read()
    assert nif.count(ORIG_MAT) == 1
    duct_bgsm = open(DUCT_BGSM, "rb").read()

    # the DX10 twins of the vanilla duct diffuse: identical BC1 blocks, two tags
    src = open(os.path.join(DATA, "Textures", *TEX_D.split("/")), "rb").read()
    assert src[84:88] == b"DXT1", src[84:88]
    hgt, wid = struct.unpack_from("<II", src, 12)
    mips = struct.unpack_from("<I", src, 28)[0]
    blocks = src[128:]
    tex = lambda n: os.path.join(OUT, "Textures", DIR, n)
    w(tex("DuctSrgb_d.dds"), dx10_header(wid, hgt, mips, 72, wid * hgt // 2) + blocks)
    w(tex("DuctUnorm_d.dds"), dx10_header(wid, hgt, mips, 71, wid * hgt // 2) + blocks)
    w(tex("FlatGrey_d.dds"), dx10_header(4, 4, 1, 0x1C, 16) + bytes([128, 128, 128, 255]) * 16)

    cases = {}
    for name, stem, base in (("studio", "StudioDuct001", TEX_D),
                             ("srgbtag", "SrgbTagDuct01", DIR + "/DuctSrgb_d.dds"),
                             ("unormtag", "UnormTagDuct1", DIR + "/DuctUnorm_d.dds"),
                             ("flatbase", "FlatBaseDuct1", DIR + "/FlatGrey_d.dds")):
        m = mat_name(stem)
        w(os.path.join(OUT, "Materials", DIR, stem + ".BGSM"), duct_bgsm)
        w(os.path.join(OUT, "Materials", DIR, stem + ".pbrm"), pbrm(6, base, TEX_N))
        nifp = os.path.join(OUT, "Meshes", "WWPbrTest02", name + ".nif")
        w(nifp, nif.replace(ORIG_MAT, m.encode("ascii")))
        cases[name] = {"nif": os.path.relpath(nifp, OUT).replace("\\", "/"), "material": m, "shape": SHAPE,
                       "base": base}
    w(os.path.join(OUT, "cases.json"), (json.dumps(cases, indent=1) + "\n").encode("utf-8"))
    print("pbr_r2a_fixtures: %d cases -> %s" % (len(cases), OUT))


if __name__ == "__main__":
    main()
