# ww-contract-provenance step 1: hash + line-count every source the doc pages cite.
import hashlib, os, sys
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")

FILES = [
    "src/lodtfile.cpp", "src/lodtfile.h", "src/btdterrain.cpp", "src/btdterrain.h",
    "tests/spells/lodl_water.sh",
    "src/lodgen.cpp", "src/lodgen.h", "src/nifskope_ui.cpp", "src/nifcli.cpp",
    "src/lodgenmanager.cpp",
    "src/io/lodmfile.cpp", "src/io/lodmfile.h",
    "src/io/lodvfile.cpp", "src/io/lodvfile.h",
    "src/gl/glmesh.cpp",
    "src/lodofile.h", "src/lodofile.cpp", "src/lodifile.h", "src/lodifile.cpp",
    "src/nativeemit.h", "src/nativeemit.cpp",
    "tests/spells/lodgen_native_decode.py",
    "src/data/niftypes.h",
    "src/hkxanim.h", "src/hkxanim.cpp", "tests/spells/hkxanim_decode.py",
    "src/watermark.h", "src/watermark.cpp", "src/watercurves.h", "src/watercurves.cpp",
]
print("| file | sha256 (16) | bytes | lines | CR |")
for f in FILES:
    if not os.path.exists(f):
        print("| %-40s | MISSING |" % f)
        continue
    b = open(f, "rb").read()
    print("| %-40s | %s | %d | %d | %d |"
          % (f, hashlib.sha256(b).hexdigest()[:16], len(b), b.count(b"\n"), b.count(b"\r")))
