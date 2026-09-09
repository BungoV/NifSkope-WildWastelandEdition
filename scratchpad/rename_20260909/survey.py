import os
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")
fs = """src/lodgen.cpp
src/lodtfile.cpp
src/lodtfile.h
src/lodgenmanager.cpp
src/nifcli.cpp
src/btdterrain.cpp
src/btdterrain.h
src/io/lodvfile.cpp
src/io/lodvfile.h
tools/bake_impostor_cards.sh
tests/spells/lodt_write.sh
tests/spells/lodt_open.sh
tests/spells/lodt_btd.sh
tests/spells/lodt_open_authority.py
tests/spells/lodgen_terrain_vt.sh
tests/spells/lodgen_vt_check.py
tests/spells/lod_generation.sh
tests/spells/lodgen_terrain.sh
tests/spells/lodgen_identity.sh
WW_CHANGES.md
MISTAKES.md""".split()
for f in fs:
    b = open(f, "rb").read()
    print("%-40s bytes=%-9d CR=%-6d LF=%-6d lines=%d" % (
        f, len(b), b.count(b"\r"), b.count(b"\n"), b.count(b"\n")))
