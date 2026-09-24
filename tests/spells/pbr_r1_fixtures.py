"""pbr_r1_fixtures.py -- build the loose PBR test folder the R1 gates read
(lane PBRR1, docs/NIFSKOPE_PBR_RENDERER.md s13 + RULINGS 2026-09-23).

    python tests/spells/pbr_r1_fixtures.py [<out>]      (default tests/fixtures/pbr_data)

The folder is a data root: NifSkope takes the parent of `Meshes` as the NIF's own
resource root, and the game (WW_LODGEN_RESOURCES) behind it supplies the vanilla
textures. Nothing is written outside <out>; the vanilla tree is only read.

Every case NIF is a byte copy of the vanilla AC duct connector with its ONE
material string rewritten to a same-length name, so nothing else in the file
moves. The cases, and the route each must resolve through by the default order
swap > nifx > (direct | sibling) > fo76 > legacy:

  sibling_v6    BGSM + same-name v6 .pbrm (weight 1, ior 1.5)       -> sibling
  sibling_v5    BGSM + same-name v5 .pbrm (f0 0.04)                 -> sibling
  direct        material names a v6 .pbrm directly                  -> direct
  nifx          BGSM + a competing sibling + .nifx -> NifxTarget    -> nifx
  direct_nifx   direct .pbrm + .nifx -> NifxTarget                  -> nifx (direct when the order puts direct first)
  swap          BGSM + a competing .nifx; WW_PBRM_SWAP maps it to a
                BGSM whose diffuse stem names SwapDuct.pbrm          -> swap
  fo76          a Fallout 76 BGSM v22 (bPBR) with the duct textures -> fo76
  texfail       BGSM + sibling v6 whose base texture does not exist -> sibling resolved, bind aborted, legacy drawn
  (legacy = the vanilla duct itself, from $DATA)

Also writes nifx_roundtrip.nifx (unknown section, odd key order, mixed spacing)
for gate (d), and cases.json -- the case table the gate judge reads.
"""
import json, os, shutil, struct, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(ROOT, "tests", "fixtures", "pbr_data")
DATA = r"E:\Tools\Fallout 4\DataUnpacked\Data"
F76 = r"E:\Projects\F76\Data\materials\architecture\asylumbasewall01.bgsm"
DUCT_NIF = os.path.join(DATA, "Meshes", "SetDressing", "ACDucts", "ACDuctConnector01.nif")
DUCT_BGSM = os.path.join(DATA, "Materials", "SetDressing", "AcDuctsRusted.BGSM")
ORIG_MAT = b"Materials\\SetDressing\\AcDuctsRusted.BGSM"
SHAPE = "ACDuctConnector01:0"
DIR = "WWPbrTest01"
TEX_D = "SetDressing/AC ducts01Rubble_d.dds"
TEX_N = "SetDressing/AC ducts01Rubble_n.dds"


def mat_name(stem, ext):
    s = "Materials\\%s\\%s.%s" % (DIR, stem, ext)
    assert len(s) == len(ORIG_MAT), (s, len(s))
    return s


def w(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def pbrm(version, base, normal, extra=None):
    """A real PBRM envelope: magic, version, payload size, JSON payload."""
    rm = {"overrideRoughness": True, "roughness": 0.55, "overrideMetallic": True, "metallic": 0.0,
          "overrideAo": True, "ao": 1.0}
    if version >= 6:
        rm.update({"specularWeight": 1.0, "overrideSpecularWeight": True})
    else:
        rm.update({"f0": 0.04, "overrideF0": True})
    uv = {
        "primaryBaseColor": {"enabled": True, "path": base,
                             "values": {"overrideColor": False, "overrideOpacity": True, "opacity": 1.0}},
        "primaryNormal": {"enabled": True, "path": normal, "values": {"overrideNormal": False, "strength": 1.0}},
        "primaryRmaos": {"enabled": False, "path": "", "values": rm},
    }
    if version >= 6:
        uv["primarySpecularColor"] = {"enabled": False, "path": "",
                                      "values": {"ior": 1.5, "iorMax": 4.25, "overrideIor": True,
                                                 "overrideColor": True}}
    doc = {"schema": "FO4.PBRM.Material", "schemaVersion": version, "shader": "Standard", "primaryUv": uv}
    if extra:
        doc.update(extra)
    payload = json.dumps(doc, separators=(",", ":")).encode("utf-8")
    return b"PBRM" + struct.pack("<II", version, len(payload)) + payload


def bgsm_header_len(v):
    n = 58
    if v < 10:
        n += 4
    n += 1
    if v >= 6:
        n += 1
    return n


def bgsm_set_textures(b, repl):
    """Rewrite texture slots {index: 'path'} of a BGSM (length-prefixed strings)."""
    v = struct.unpack_from("<I", b, 4)[0]
    o = bgsm_header_len(v)
    cnt = 10 if v >= 17 else 9
    head, slots = b[:o], []
    for i in range(cnt):
        L = struct.unpack_from("<I", b, o)[0]
        slots.append(b[o + 4:o + 4 + L])
        o += 4 + L
    for i, s in repl.items():
        slots[i] = s.encode("ascii") + b"\x00"
    body = b"".join(struct.pack("<I", len(s)) + s for s in slots)
    return head + body + b[o:], v


def main():
    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    nif = open(DUCT_NIF, "rb").read()
    assert nif.count(ORIG_MAT) == 1, "duct NIF material string not found once"
    duct_bgsm = open(DUCT_BGSM, "rb").read()
    M = lambda *p: os.path.join(OUT, "Materials", DIR, *p)
    mesh = lambda n: os.path.join(OUT, "Meshes", "WWPbrTest", n + ".nif")
    cases = {}

    def case(name, material, expect_route, expect_path, pins=None, nifx=None, notes=""):
        w(mesh(name), nif.replace(ORIG_MAT, material.encode("ascii")))
        if nifx is not None:
            w(mesh(name)[:-4] + ".nifx", nifx.encode("utf-8"))
        cases[name] = {"nif": os.path.relpath(mesh(name), OUT).replace("\\", "/"), "material": material,
                       "shape": SHAPE, "expect_route": expect_route, "expect_path": expect_path,
                       "pins": pins or {}, "notes": notes}

    v6 = pbrm(6, TEX_D, TEX_N)
    v5 = pbrm(5, TEX_D, TEX_N)
    nifx_doc = lambda target: json.dumps({"version": 1, "material": {SHAPE: {"pbrm": target}}}, indent=2) + "\n"
    target = "Materials\\%s\\NifxTarget.pbrm" % DIR
    w(M("NifxTarget.pbrm"), v6)

    # sibling v6 / v5 (gate c twins)
    w(M("SiblingV6Duct.BGSM"), duct_bgsm); w(M("SiblingV6Duct.pbrm"), v6)
    case("sibling_v6", mat_name("SiblingV6Duct", "BGSM"), "sibling", "Materials\\%s\\SiblingV6Duct.pbrm" % DIR)
    w(M("SiblingV5Duct.BGSM"), duct_bgsm); w(M("SiblingV5Duct.pbrm"), v5)
    case("sibling_v5", mat_name("SiblingV5Duct", "BGSM"), "sibling", "Materials\\%s\\SiblingV5Duct.pbrm" % DIR)
    # direct link
    w(M("DirectV6Ducts.PBRM"), v6)
    case("direct", mat_name("DirectV6Ducts", "PBRM"), "direct", "Materials\\%s\\DirectV6Ducts.PBRM" % DIR)
    # nifx beats a sibling
    w(M("NifxBgsmDucts.BGSM"), duct_bgsm); w(M("NifxBgsmDucts.pbrm"), v5)
    case("nifx", mat_name("NifxBgsmDucts", "BGSM"), "nifx", target, nifx=nifx_doc(target))
    # nifx beats a direct link (the gate-e red flips this one)
    w(M("DirectNifxDct.PBRM"), v5)
    case("direct_nifx", mat_name("DirectNifxDct", "PBRM"), "nifx", target, nifx=nifx_doc(target))
    # swap beats nifx
    w(M("SwapBgsmDucts.BGSM"), duct_bgsm)
    sw, _ = bgsm_set_textures(duct_bgsm, {0: DIR + "/SwapDuct_d.dds"})
    w(M("SwapTarget.BGSM"), sw); w(M("SwapDuct.pbrm"), v6)
    case("swap", mat_name("SwapBgsmDucts", "BGSM"), "swap", "Materials\\%s\\SwapDuct.pbrm" % DIR,
         pins={"WW_PBRM_SWAP": "%s>Materials\\%s\\SwapTarget.BGSM" % (mat_name("SwapBgsmDucts", "BGSM"), DIR)},
         nifx=nifx_doc(target))
    # FO76 BGSM v22, bPBR, textures pointed at the FO4 duct
    f76, fv = bgsm_set_textures(open(F76, "rb").read(), {0: TEX_D, 1: TEX_N, 6: "", 7: ""})
    assert 20 <= fv <= 22, fv
    w(M("Fo76BgsmDucts.BGSM"), f76)
    case("fo76", mat_name("Fo76BgsmDucts", "BGSM"), "fo76", "Materials\\%s\\Fo76BgsmDucts.BGSM" % DIR)
    # texture load failure: resolves, binding aborts, legacy drawn
    w(M("TexFailDucts1.BGSM"), duct_bgsm)
    w(M("TexFailDucts1.pbrm"), pbrm(6, DIR + "/DoesNotExist_d.dds", TEX_N))
    case("texfail", mat_name("TexFailDucts1", "BGSM"), "sibling", "Materials\\%s\\TexFailDucts1.pbrm" % DIR,
         notes="bind must abort: texture load failed")
    cases["legacy"] = {"nif": "$DATA/Meshes/SetDressing/ACDucts/ACDuctConnector01.nif",
                       "material": ORIG_MAT.decode(), "shape": SHAPE, "expect_route": "legacy",
                       "expect_path": "", "pins": {}, "notes": "vanilla, no pbrm anywhere"}

    # gate (d): unknown section first, odd key order, mixed spacing, a tab, CRLF
    rt = ('{ "zUnknownTool" : {"keep": [1, 2.50, "x"], "nested": {"b":1,"a":2}},\r\n'
          '\t"material":{\r\n'
          '  "' + SHAPE + '" :  { "note":"odd order", "pbrm" : "Materials\\\\' + DIR + '\\\\NifxTarget.pbrm" },\r\n'
          '  "OtherNode":{"pbrm":"materials/x/y.pbrm"}\r\n'
          ' },\r\n'
          '"version":1 ,"aaa_later":null}\r\n')
    w(os.path.join(OUT, "nifx_roundtrip.nifx"), rt.encode("utf-8"))

    w(os.path.join(OUT, "cases.json"), (json.dumps(cases, indent=1) + "\n").encode("utf-8"))
    print("pbr_r1_fixtures: %d cases -> %s" % (len(cases), OUT))


if __name__ == "__main__":
    main()
