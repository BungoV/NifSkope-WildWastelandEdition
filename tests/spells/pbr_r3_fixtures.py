"""pbr_r3_fixtures.py -- the loose test data for the R3 gates (lane PBRR3,
docs/NIFSKOPE_PBR_RENDERER.md s3.2 + the R3 row).

Writes tests/fixtures/pbr_r3_data (or argv[1]):
  Meshes/WWPbrTest03/<case>.nif     vanilla Preview/PreviewSphere01.nif (radius 128, centre 0)
                                    with its shader Name set to Materials\\WWPbrTest03\\<case>.BGSM
  Materials/WWPbrTest03/<case>.BGSM a copy of the duct BGSM (the sibling route needs a material)
  Materials/WWPbrTest03/<case>.pbrm the case's material: constants only, no textures
  Textures/WWPbrTest03/WhiteCube.dds  a uniform white cube (B8G8R8A8, 64 px, 7 mips; the
                                    Studio loader tags it sRGB, so 255 = linear 1.0)
  cases.json                        the case table the judge reads

Vanilla data is read only. Every case name is 9 characters so the NIF string is patched in
place from one template.
"""
import json, os, shutil, struct, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(ROOT, "tests", "fixtures", "pbr_r3_data")
DATA = r"E:\Tools\Fallout 4\DataUnpacked\Data"
SPHERE = os.path.join(DATA, "Meshes", "Preview", "PreviewSphere01.nif")
BGSM = os.path.join(DATA, "Materials", "SetDressing", "AcDuctsRusted.BGSM")
DIR = "WWPbrTest03"


def w(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def nif_set_shader_name(nif, block, name):
    """Append `name` to the header string table and point block `block`'s Name (its
    first uint after Shader Type) at it. NIF 20.2.0.7, BS version 130 header."""
    p = nif.index(b"\n") + 1
    ver, endian, user, nblocks = struct.unpack_from("<IBII", nif, p); p += 13
    assert ver == 0x14020007 and user == 12, (hex(ver), user)
    bsver, = struct.unpack_from("<I", nif, p); p += 4
    assert bsver == 130, bsver
    for _ in range(4):                  # author, process script, export script, max filepath
        p += 1 + nif[p]
    ntypes, = struct.unpack_from("<H", nif, p); p += 2
    for _ in range(ntypes):
        n, = struct.unpack_from("<I", nif, p); p += 4 + n
    p += 2 * nblocks
    sizes = struct.unpack_from("<%dI" % nblocks, nif, p); p += 4 * nblocks
    nstr_at = p
    nstr, maxlen = struct.unpack_from("<II", nif, p); p += 8
    for _ in range(nstr):
        n, = struct.unpack_from("<I", nif, p); p += 4 + n
    table_end = p
    ngroups, = struct.unpack_from("<I", nif, p); p += 4 + 4 * ngroups
    body = p                            # block data starts here
    off = body + sum(sizes[:block])
    shader_type, cur = struct.unpack_from("<II", nif, off)
    assert cur == 0xFFFFFFFF, cur
    nb = name.encode("ascii")
    out = bytearray(nif[:nstr_at]) + struct.pack("<II", nstr + 1, max(maxlen, len(nb)))
    out += nif[nstr_at + 8:table_end] + struct.pack("<I", len(nb)) + nb + nif[table_end:]
    grow = 4 + len(nb)
    struct.pack_into("<I", out, off + grow + 4, nstr)
    return bytes(out)


def pbrm(version, base="#FFFFFF", rough=0.5, metal=0.0, weight=1.0, ior=1.5, tint=None, f0=0.04,
         diff_rough=None):
    """A real PBRM envelope with constants only (every slot's texture off)."""
    bv = {"overrideColor": True, "color": base, "overrideOpacity": True, "opacity": 1.0}
    if diff_rough is not None:
        bv["diffuseRoughness"] = diff_rough
    rm = {"overrideRoughness": True, "roughness": rough, "overrideMetallic": True, "metallic": metal,
          "overrideAo": True, "ao": 1.0}
    if version >= 6:
        rm.update({"specularWeight": weight, "overrideSpecularWeight": True})
    else:
        rm.update({"f0": f0, "overrideF0": True})
    uv = {
        "primaryBaseColor": {"enabled": False, "path": "", "values": bv},
        "primaryNormal": {"enabled": False, "path": "", "values": {"overrideNormal": True, "strength": 1.0}},
        "primaryRmaos": {"enabled": False, "path": "", "values": rm},
    }
    if version >= 6:
        sv = {"ior": ior, "iorMax": 4.25, "overrideIor": True, "overrideColor": True}
        if tint:
            sv["color"] = tint
        uv["primarySpecularColor"] = {"enabled": bool(tint), "path": "", "values": sv}
    doc = {"schema": "FO4.PBRM.Material", "schemaVersion": version, "shader": "Standard", "primaryUv": uv}
    payload = json.dumps(doc, separators=(",", ":")).encode("utf-8")
    return b"PBRM" + struct.pack("<II", version, len(payload)) + payload


def white_cube(size=64, mips=7):
    """Legacy DDS, B8G8R8A8, cube flags in caps2, all texels 255."""
    h = bytearray(128)
    struct.pack_into("<4sIIIIIII", h, 0, b"DDS ", 124, 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000, size, size,
                     size * 4, 0, mips)
    struct.pack_into("<IIIIIIII", h, 76, 32, 0x41, 0, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
    struct.pack_into("<II", h, 108, 0x1000 | 0x8 | 0x400000, 0x200 | 0xFC00)
    body = b""
    for _ in range(6):
        s = size
        for _ in range(mips):
            body += b"\xff" * (s * s * 4)
            s = max(1, s // 2)
    return bytes(h) + body


CASES = {
    # the white furnace: metal base 1 at three roughnesses, a dielectric F0 0.04
    "metal_r10": dict(version=6, metal=1.0, rough=0.1),
    "metal_r50": dict(version=6, metal=1.0, rough=0.5),
    "metal_r99": dict(version=6, metal=1.0, rough=1.0),
    "diel_f004": dict(version=6, metal=0.0, rough=0.5),
    # v5 / v6 twins: F0 0.04 each way
    "twin_v5aa": dict(version=5, metal=0.0, rough=0.5, f0=0.04),
    "twin_v6aa": dict(version=6, metal=0.0, rough=0.5, weight=1.0, ior=1.5),
    # s1: weight 0 vs weight 1 (dielectric)
    "spec_w000": dict(version=6, metal=0.0, rough=0.3, weight=0.0),
    "spec_w100": dict(version=6, metal=0.0, rough=0.3, weight=1.0),
    # s2: IOR 1.5 vs 2.0
    "ior_150aa": dict(version=6, metal=0.0, rough=0.5, ior=1.5),
    "ior_200aa": dict(version=6, metal=0.0, rough=0.5, ior=2.0),
    # s3: a red specular colour on a white dielectric
    "tint_redd": dict(version=6, metal=0.0, rough=0.3, tint="#FF0000"),
    # EON: a diffuse-roughness sphere (pictures; no verdict)
    "eon_dr100": dict(version=6, metal=0.0, rough=0.5, diff_rough=1.0),
}


def main():
    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    template = "Materials\\%s\\Case00000.BGSM" % DIR
    nif = nif_set_shader_name(open(SPHERE, "rb").read(), 2, template)
    bgsm = open(BGSM, "rb").read()
    table = {}
    for name, kw in CASES.items():
        assert len(name) == 9, name
        mat = "Materials\\%s\\%s.BGSM" % (DIR, name)
        w(os.path.join(OUT, "Meshes", DIR, name + ".nif"), nif.replace(template.encode(), mat.encode()))
        w(os.path.join(OUT, "Materials", DIR, name + ".BGSM"), bgsm)
        w(os.path.join(OUT, "Materials", DIR, name + ".pbrm"), pbrm(**kw))
        table[name] = dict(kw, nif="Meshes/%s/%s.nif" % (DIR, name), material=mat)
    w(os.path.join(OUT, "Textures", DIR, "WhiteCube.dds"), white_cube())
    w(os.path.join(OUT, "cases.json"), (json.dumps(table, indent=1) + "\n").encode("utf-8"))
    print("wrote %d cases to %s" % (len(table), OUT))


if __name__ == "__main__":
    main()
