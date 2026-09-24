"""PBRLODFIX1: the glTF export flag parser only runs for the gltf commands.

GLTFEXPORT1's hook-up (2026-09-19) called gltfExportParseFlag() for EVERY
command, above lodgen's `--data-root` and collision's `--skeleton`, so both
were swallowed. --revert puts the old line back (the red control).
"""
import sys

P = r"E:\Projects\NifskopeWildWastelandEdition\src\nifcli.cpp"
OLD = (b"\t\t// lane GLTFEXPORT1: every export option, through the SAME function the\n"
       b"\t\t// dialog's rows drive, so a flag and a row cannot mean different things.\n"
       b"\t\telse if ( int gr = gltfExportParseFlag( t, ( i + 1 < a.size() ) ? a.at( i + 1 ) : QString(),\n"
       b"\t\t\t\t\t\t\t\t\t\t\t\tgltfOpts, gltfUsedNext, gltfFlagError ) ) {\n")
NEW = (b"\t\t// lane GLTFEXPORT1: every export option, through the SAME function the\n"
       b"\t\t// dialog's rows drive, so a flag and a row cannot mean different things.\n"
       b"\t\t// ONLY for the gltf commands (lane PBRLODFIX1, 2026-09-24): this loop is\n"
       b"\t\t// shared by every command, and the export's `--data-root` and `--skeleton`\n"
       b"\t\t// shadowed lodgen's `--data-root` and collision's `--skeleton` further\n"
       b"\t\t// down -- every lodgen bake since 2026-09-19 ran without its loose root.\n"
       b"\t\telse if ( int gr = ( cmd == QLatin1String( \"gltf\" ) || cmd == QLatin1String( \"gltf-export\" ) )\n"
       b"\t\t\t\t? gltfExportParseFlag( t, ( i + 1 < a.size() ) ? a.at( i + 1 ) : QString(),\n"
       b"\t\t\t\t\tgltfOpts, gltfUsedNext, gltfFlagError ) : 0 ) {\n")

revert = "--revert" in sys.argv
src, dst = (NEW, OLD) if revert else (OLD, NEW)
b = open(P, "rb").read()
cr0 = b.count(b"\r")
n = b.count(src)
if n != 1:
    sys.exit("REFUSED: anchor found %d times (want 1); already applied? %d" % (n, b.count(dst)))
b2 = b.replace(src, dst)
assert b2.count(b"\r") == cr0 == 0, "CR count moved or file not LF-only"
open(P, "wb").write(b2)
print("%s OK; CR %d; LF %d -> %d" % ("reverted" if revert else "applied", cr0, b.count(b"\n"), b2.count(b"\n")))
