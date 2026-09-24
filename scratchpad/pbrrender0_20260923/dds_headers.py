import struct, os
D = r"E:\Tools\Fallout 4\DataUnpacked\Data"
FILES = [
    r"textures\shared\cubemaps\mipblur_DefaultOutside1.dds",
    r"textures\shared\cubemaps\mipblur_DefaultOutside1_dielectric.dds",
    r"textures\shared\cubemaps\OutsideDay01.dds",
    r"textures\shared\cubemaps\QuickSky_e.dds",
    r"textures\shared\cubemaps\mipblur_InstInterior.dds",
    r"textures\shared\cubemaps\ShinyDull_e.dds",
    r"textures\shared\cubemaps\CityBuildingsCube.dds",
    r"textures\sky\Sun.DDS",
    r"textures\sky\SunGlare.DDS",
    r"textures\sky\Sun_d.DDS",
    r"textures\sky\CloudsLower01_d.DDS",
    r"textures\sky\CloudsUpper01_d.DDS",
    r"textures\sky\CloudsHorizon01_d.DDS",
    r"textures\sky\SkyStars.DDS",
    r"textures\landscape\Ground\CommonwealthDefault01_d.DDS",
    r"textures\landscape\Ground\CommonwealthDefault01_N.DDS",
    r"textures\landscape\Ground\CommonwealthDefault01_s.DDS",
]
DXGI = {71: "BC1", 74: "BC2", 77: "BC3", 80: "BC4", 83: "BC5", 95: "BC6H_UF16", 96: "BC6H_SF16", 98: "BC7",
        99: "BC7_SRGB", 72: "BC1_SRGB", 78: "BC3_SRGB", 28: "RGBA8", 29: "RGBA8_SRGB", 87: "BGRA8", 10: "RGBA16F", 2: "RGBA32F"}
for f in FILES:
    p = os.path.join(D, f)
    if not os.path.exists(p):
        print("MISSING", f); continue
    b = open(p, "rb").read(148)
    h, w = struct.unpack_from("<II", b, 12)
    mips = struct.unpack_from("<I", b, 28)[0]
    fourcc = b[84:88]
    rgbbits = struct.unpack_from("<I", b, 88)[0]
    caps2 = struct.unpack_from("<I", b, 112)[0]
    cube = bool(caps2 & 0x200)
    fmt = fourcc.decode("ascii", "replace")
    if fourcc == b"DX10":
        dx = struct.unpack_from("<I", b, 128)[0]
        misc = struct.unpack_from("<I", b, 136)[0]
        cube = cube or bool(misc & 4)
        fmt = "DX10:" + DXGI.get(dx, str(dx))
    elif fourcc == b"\0\0\0\0":
        fmt = "uncompressed%dbpp" % rgbbits
    print("%-62s %4dx%-4d mips=%-2d %-14s cube=%s size=%d" % (f, w, h, mips, fmt, cube, os.path.getsize(p)))
