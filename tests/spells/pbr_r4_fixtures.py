"""pbr_r4_fixtures.py -- the loose test data for the R4 gates (lane PBRR4,
docs/NIFSKOPE_PBR_RENDERER.md s3.2 items 4/7/9 + the R4 row, and the two scope
additions: s1b the OpenPBR specular weight, d1 the Burley diffuse).

Writes tests/fixtures/pbr_r4_data (or argv[1]):
  Meshes/WWPbrTest04/<case>.nif      plane cases: vanilla Preview/PreviewPlane01.nif (256 x 256,
                                     normal +Z); sphere cases: Preview/PreviewSphere01.nif (radius 128)
                                     with its shader Name set to Materials\\WWPbrTest04\\<case>.BGSM
  Materials/WWPbrTest04/<case>.BGSM  a copy of the duct BGSM (the sibling route needs a material)
  Materials/WWPbrTest04/<case>.pbrm  the case's material
  Textures/WWPbrTest04/*.dds         TintMask (the 5-region mask), Orient (the UV quadrant base),
                                     EmitGreen / EmitGreenHalf (emission maps), WhiteCube, BlackCube
  cases.json                         the case table the judge reads (regions, colours, laws)

Vanilla data is read only. Every case name is 9 characters so the NIF string is patched in place.
"""
import json, os, shutil, struct, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from pbr_r3_fixtures import nif_set_shader_name, white_cube  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(ROOT, "tests", "fixtures", "pbr_r4_data")
DATA = r"E:\Tools\Fallout 4\DataUnpacked\Data"
SPHERE = os.path.join(DATA, "Meshes", "Preview", "PreviewSphere01.nif")
PLANE = os.path.join(DATA, "Meshes", "Preview", "PreviewPlane01.nif")
BGSM = os.path.join(DATA, "Materials", "SetDressing", "AcDuctsRusted.BGSM")
DIR = "WWPbrTest04"

# the tint mask: 128 x 128, five UV regions (u right, v down in texture space)
#   region   u range      v range     R    G    B    A
TINT_REGIONS = {
    "r":    ((0.00, 0.25), (0.0, 0.5), (255, 0, 0, 0)),
    "over": ((0.25, 0.50), (0.0, 0.5), (204, 153, 102, 0)),     # sum 1.8: Normalize divides
    "g":    ((0.50, 1.00), (0.0, 0.5), (0, 200, 0, 0)),
    "b":    ((0.00, 0.50), (0.5, 1.0), (0, 0, 255, 0)),
    "a":    ((0.50, 1.00), (0.5, 1.0), (0, 0, 0, 128)),
}
TINT_COLOURS = {"colorR": "#FF4020", "colorG": "#20C040", "colorB": "#4080FF", "colorA": "#FFFF00"}
ORIENT = {  # base colour quadrants for the UV -> pixel map
    "red":   ((0.0, 0.5), (0.0, 0.5), (255, 0, 0, 255)),
    "green": ((0.5, 1.0), (0.0, 0.5), (0, 255, 0, 255)),
    "blue":  ((0.0, 0.5), (0.5, 1.0), (0, 0, 255, 255)),
    "white": ((0.5, 1.0), (0.5, 1.0), (255, 255, 255, 255)),
}


def w(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def region_image(regions, size=128):
    img = np.zeros((size, size, 4), np.uint8)
    for (u0, u1), (v0, v1), rgba in regions.values():
        img[int(v0 * size):int(v1 * size), int(u0 * size):int(u1 * size)] = rgba
    return img


def dds2d(img):
    """Legacy DDS, B8G8R8A8 UNORM (linear: nothing tags it sRGB), full box-filtered mip chain."""
    h0, w0 = img.shape[:2]
    mips = [img]
    while mips[-1].shape[0] > 1:
        m = mips[-1].astype(np.float64)
        m = (m[0::2, 0::2] + m[1::2, 0::2] + m[0::2, 1::2] + m[1::2, 1::2]) / 4
        mips.append(np.round(m).astype(np.uint8))
    h = bytearray(128)
    struct.pack_into("<4sIIIIIII", h, 0, b"DDS ", 124, 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000, h0, w0,
                     w0 * 4, 0, len(mips))
    struct.pack_into("<IIIIIIII", h, 76, 32, 0x41, 0, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
    struct.pack_into("<II", h, 108, 0x1000 | 0x8 | 0x400000, 0)
    body = b"".join(m[:, :, [2, 1, 0, 3]].tobytes() for m in mips)
    return bytes(h) + body


def black_cube(size=64, mips=7):
    c = bytearray(white_cube(size, mips))
    for i in range(128, len(c), 4):
        c[i] = c[i + 1] = c[i + 2] = 0     # B G R = 0, A = 255
    return bytes(c)


def pbrm(base="#FFFFFF", rough=0.5, metal=0.0, weight=0.0, ior=1.5, base_tex="", opacity=1.0,
         tint=None, emis=None, comp=None):
    bv = {"overrideColor": not base_tex, "color": base, "overrideOpacity": True, "opacity": opacity}
    rm = {"overrideRoughness": True, "roughness": rough, "overrideMetallic": True, "metallic": metal,
          "overrideAo": True, "ao": 1.0, "specularWeight": weight, "overrideSpecularWeight": True}
    uv = {
        "primaryBaseColor": {"enabled": bool(base_tex), "path": base_tex, "values": bv},
        "primaryNormal": {"enabled": False, "path": "", "values": {"overrideNormal": True, "strength": 1.0}},
        "primaryRmaos": {"enabled": False, "path": "", "values": rm},
        "primarySpecularColor": {"enabled": False, "path": "",
                                 "values": {"ior": ior, "iorMax": 4.25, "overrideIor": True, "overrideColor": True}},
    }
    if tint is not None:
        tv = dict(TINT_COLOURS, overlap=tint)
        for c in "RGBA":
            tv["overrideMask" + c] = False
            tv["mask" + c] = 0.0
        uv["primaryTintMask"] = {"enabled": True, "path": DIR + "/TintMask.dds", "values": tv}
    if emis is not None:
        path, ev = emis
        uv["primaryEmissive"] = {"enabled": bool(path), "path": path, "values": ev}
    doc = {"schema": "FO4.PBRM.Material", "schemaVersion": 6, "shader": "Standard", "primaryUv": uv}
    if comp is not None:
        doc["settings"] = {"Transparency/Composition": comp}
    payload = json.dumps(doc, separators=(",", ":")).encode("utf-8")
    return b"PBRM" + struct.pack("<II", 6, len(payload)) + payload


EG = DIR + "/EmitGreen.dds"
EGH = DIR + "/EmitGreenHalf.dds"
COMPBASE = "#FF8040"
CASES = {
    # --- plane: UV orientation + the untinted reference plate (furnace-lit, weight 0)
    "p_orient0": dict(mesh="plane", base_tex=DIR + "/Orient.dds"),
    "p_notint0": dict(mesh="plane"),
    # --- tint masks, the three overlap modes
    "p_tintnrm": dict(mesh="plane", tint="Normalize"),
    "p_tintadd": dict(mesh="plane", tint="Add"),
    "p_tintpri": dict(mesh="plane", tint="Priority RGBA"),
    # --- emission (black base, weight 0): 100 / 50 nits, the map replace rule
    "p_emit100": dict(mesh="plane", base="#000000", emis=("", {"color": "#FFFFFF", "luminance": 100.0})),
    "p_emit050": dict(mesh="plane", base="#000000", emis=("", {"color": "#FFFFFF", "luminance": 50.0})),
    "p_emittex": dict(mesh="plane", base="#000000", emis=(EG, {"color": "#FF0000", "luminance": 100.0})),
    "p_emittov": dict(mesh="plane", base="#000000",
                      emis=(EG, {"color": "#FF0000", "luminance": 100.0, "overrideColor": True, "overrideMask": True})),
    "p_emitmsk": dict(mesh="plane", base="#000000", emis=(EGH, {"color": "#FF0000", "luminance": 100.0})),
    # --- composition (constant opacity 0.3 unless noted)
    "p_cmpopaq": dict(mesh="plane", base=COMPBASE, opacity=0.3, comp={"mode": "Opaque"}),
    "p_cmptslo": dict(mesh="plane", base=COMPBASE, opacity=0.3, comp={"mode": "Alpha Test", "threshold": 0.5}),
    "p_cmptshi": dict(mesh="plane", base=COMPBASE, opacity=0.7, comp={"mode": "Alpha Test", "threshold": 0.5}),
    "p_cmpblnd": dict(mesh="plane", base=COMPBASE, opacity=0.3, comp={"mode": "Alpha Blend"}),
    "p_cmpprem": dict(mesh="plane", base=COMPBASE, opacity=0.3, comp={"mode": "Premultiplied"}),
    "p_cmpaddv": dict(mesh="plane", base=COMPBASE, opacity=0.3, comp={"mode": "Additive"}),
    "p_cmpmult": dict(mesh="plane", base=COMPBASE, opacity=0.3, comp={"mode": "Multiply"}),
    # --- s1b: the OpenPBR specular weight (sphere, Fresnel view)
    "s_w050aaa": dict(mesh="sphere", weight=0.5, rough=0.3),
    "s_w100aaa": dict(mesh="sphere", weight=1.0, rough=0.3),
    # --- d1: Burley (sphere, frontal sun, black cube, weight 0)
    "d_r000aaa": dict(mesh="sphere", base="#404040", rough=0.0),
    "d_r025aaa": dict(mesh="sphere", base="#404040", rough=0.25),
    "d_r100aaa": dict(mesh="sphere", base="#404040", rough=1.0),
}


def main():
    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    template = "Materials\\%s\\Case00000.BGSM" % DIR
    nifs = {"sphere": nif_set_shader_name(open(SPHERE, "rb").read(), 2, template),
            "plane": nif_set_shader_name(open(PLANE, "rb").read(), 2, template)}
    bgsm = open(BGSM, "rb").read()
    table = {}
    for name, kw in CASES.items():
        assert len(name) == 9, name
        kw = dict(kw)
        mesh = kw.pop("mesh")
        mat = "Materials\\%s\\%s.BGSM" % (DIR, name)
        w(os.path.join(OUT, "Meshes", DIR, name + ".nif"), nifs[mesh].replace(template.encode(), mat.encode()))
        w(os.path.join(OUT, "Materials", DIR, name + ".BGSM"), bgsm)
        w(os.path.join(OUT, "Materials", DIR, name + ".pbrm"), pbrm(**kw))
        table[name] = dict(kw, mesh=mesh, nif="Meshes/%s/%s.nif" % (DIR, name), material=mat)
    T = os.path.join(OUT, "Textures", DIR)
    w(os.path.join(T, "TintMask.dds"), dds2d(region_image(TINT_REGIONS)))
    w(os.path.join(T, "Orient.dds"), dds2d(region_image(ORIENT)))
    g = np.zeros((16, 16, 4), np.uint8)
    g[:, :] = (0, 255, 0, 255)
    w(os.path.join(T, "EmitGreen.dds"), dds2d(g))
    g[:, :] = (0, 255, 0, 128)
    w(os.path.join(T, "EmitGreenHalf.dds"), dds2d(g))
    w(os.path.join(T, "WhiteCube.dds"), white_cube())
    w(os.path.join(T, "BlackCube.dds"), black_cube())
    meta = {"cases": table, "tint_regions": {k: [list(a), list(b), list(c)] for k, (a, b, c) in TINT_REGIONS.items()},
            "tint_colours": TINT_COLOURS, "orient": {k: [list(a), list(b)] for k, (a, b, _) in ORIENT.items()}}
    w(os.path.join(OUT, "cases.json"), (json.dumps(meta, indent=1) + "\n").encode("utf-8"))
    print("wrote %d cases to %s" % (len(table), OUT))


if __name__ == "__main__":
    main()
