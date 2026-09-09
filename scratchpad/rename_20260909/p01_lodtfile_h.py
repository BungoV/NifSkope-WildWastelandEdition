# Patch 1 -- src/lodtfile.h : the LAND file becomes .lodl (bungo 2026-09-09).
# LF-only file; CR count must stay 0.
import os
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")
P = "src/lodtfile.h"
b = open(P, "rb").read()
assert b.count(b"\r") == 0, "expected LF-only"
s = b.decode("utf-8")


def sub(old, new, n=1):
    global s
    c = s.count(old)
    assert c == n, "anchor count %d != %d for %r" % (c, n, old[:70])
    s = s.replace(old, new)


# --- 1. the class doc-comment: what this file writes, and the 2026-09-09 names
sub(
    """/*! Writer for the `.lodt` whole-worldspace landscape file.
 *
 *  The format is specified in docs/LODGEN_BTD_FORMAT.md; that document is the
 *  contract, not this header.""",
    """/*! Writer for the `.lodl` whole-worldspace LANDSCAPE file.
 *
 *  THE NAME MOVED (bungo's ruling, 2026-09-09). This file used to be written
 *  as `.lodt`; `.lodt` now names the terrain TEXTURE sheets (src/io/lodvfile.h,
 *  magic LDTX), and the landscape file is `.lodl`. The C++ names in this header
 *  -- LodtFile, lodtWrite, LodtOptions -- were NOT renamed with it: they are
 *  internal, they appear in no file on disk and in no command, and renaming
 *  three hundred call sites is churn a reader gains nothing from. Read
 *  every `lodt` identifier here as "the .lodl landscape file". The magic bytes
 *  did not move either (LODL_MAGIC still spells LODT on disk), because the
 *  rename must leave the file byte-identical.
 *
 *  The format is specified in docs/LODGEN_BTD_FORMAT.md; that document is the
 *  contract, not this header.""",
)

# --- 2. the magic, promoted out of lodtfile.cpp's anonymous namespace so the
#        TEXTURE reader can name this file when it is handed one
sub(
    """class EsmWorld;
""",
    """class EsmWorld;

/*! The landscape file's magic, first four bytes, little-endian.
 *
 *  It spells `LODT` on disk and it is NOT changing: the 2026-09-09 rename is an
 *  extension rename, and the gate on it is that the bytes of a `.lodl` are
 *  identical to the bytes the same worldspace wrote as a `.lodt` yesterday.
 *  What distinguishes the two formats is that the TEXTURE file has its own
 *  magic (LODTEX_MAGIC, `LDTX`, src/io/lodvfile.h) -- so each reader can NAME
 *  the other's file instead of misparsing it. */
constexpr quint32 LODL_MAGIC = 0x54444F4CU;   // 'L','O','D','T' little-endian
""",
)

# --- 3. the environment fallback's name
sub("The\n\t *  environment variable WW_LODT_VERSION overrides it, so the fallback is\n"
    "\t *  reachable without a rebuild. */",
    "The\n\t *  environment variable WW_LODL_VERSION overrides it, so the fallback is\n"
    "\t *  reachable without a rebuild (WW_LODT_VERSION is refused by name). */")

# --- 4. the remaining `.lodt` prose
sub("/*! Recompute ONLY the AO plane of an existing .lodt, in place.",
    "/*! Recompute ONLY the AO plane of an existing .lodl, in place.")
sub("//! Write <outDir>/Terrain/<EDID>.lodt for one worldspace.",
    "//! Write <outDir>/Terrain/<EDID>.lodl for one worldspace.")
sub("/*! Convert a Fallout 76 .btd to .lodt.",
    "/*! Convert a Fallout 76 .btd to .lodl.")
sub("""class LodtFile
{""",
    """class LodtFile
{""")

out = s.encode("utf-8")
assert out.count(b"\r") == 0
for i, ln in enumerate(s.split("\n"), 1):
    if ".lodt" in ln:
        print("  remaining .lodt @%d: %s" % (i, ln.strip()))
open(P, "wb").write(out)
print("OK", P, len(out), "bytes, CR", out.count(b"\r"), "LF", out.count(b"\n"))
