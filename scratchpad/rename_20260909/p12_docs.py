# Patch 12 -- docs/LODGEN_*.md : bungo's 2026-09-09 FINAL FILE NAMES.
#   .lodl = land (was .lodt), .lodt = terrain textures (was .lodv),
#   .lodo = objects (was to be .lodg), .lodi = instances, .lodm = materials.
# Every doc is LF-only and stays so.
import os
import re
import subprocess
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")

REPORT = []


def load(p):
    b = open(p, "rb").read()
    assert b.count(b"\r") == 0, p
    return b.decode("utf-8")


def save(p, s):
    out = s.encode("utf-8")
    assert out.count(b"\r") == 0
    open(p, "wb").write(out)
    print("OK %-40s %7d bytes  CR %d" % (p, len(out), out.count(b"\r")))


def rep(s, pairs, p):
    n = 0
    for a, b in pairs:
        c = s.count(a)
        if c:
            s = s.replace(a, b)
            n += c
            REPORT.append((p, a, b, c))
    return s, n


# The land pairs, applied FIRST so that `.lodv` -> `.lodt` afterwards cannot be
# caught by them. `LODT` the MAGIC and the C++ names (LodtFile, lodtfile.cpp,
# lodtWrite, LodtOptions, FarFieldLodtFormat.h, lodt_format_tests.cpp) are
# deliberately NOT in this list: the bytes did not move and neither did the
# identifiers.
LAND = [
    (".lodt", ".lodl"),
    ("WW_LODT_VERSION", "WW_LODL_VERSION"),
    ("WW_LODT_REGION", "WW_LODL_REGION"),
    ("WW_LODT_PLANE", "WW_LODL_PLANE"),
    ("--lodt ", "--lodl "),
    ("`--lodt`", "`--lodl`"),
    ("--lodt <", "--lodl <"),
    ("lodt_write.sh", "lodl_write.sh"),
    ("lodt_open.sh", "lodl_open.sh"),
    ("lodt_btd.sh", "lodl_btd.sh"),
    ("lodt_open_authority.py", "lodl_open_authority.py"),
    ("-no-gui lodt ", "-no-gui lodl "),
]
# The texture pairs, applied SECOND.
TEX = [
    (".lodv", ".lodt"),
    ("--lodv-check", "--lodt-check"),
    ("`.lodv`", "`.lodt`"),
    ("magic 'LODV'", "magic 'LDTX'"),
    ("magic `LODV`", "magic `LDTX`"),
    ("`LODV`", "`LDTX`"),
    ("lodv ok 1", "lodt ok 1"),
    ("lodv refused", "lodt refused"),
]

DOCS = [
    "docs/LODGEN_BTD_FORMAT.md",
    "docs/LODGEN_TERRAIN_VT.md",
    "docs/LODGEN_CARD_SHEETS.md",
    "docs/LODGEN_IMPOSTOR_SPEC.md",
    "docs/LODGEN_LODM_FORMAT.md",
    "docs/LODGEN_MANIFEST_FORMAT.md",
    "docs/LODGEN_TEXTURE_ARRAYS.md",
    "docs/LODGEN_VERTEX_PACKING.md",
    "docs/LODGEN_PARITY.md",
    "docs/LODGEN_PLAN.md",
    "docs/LODGEN_ESM_LAYOUTS.md",
]
for p in DOCS:
    s = load(p)
    s, _ = rep(s, LAND, p)
    s, _ = rep(s, TEX, p)
    save(p, s)

# ---------------------------------------------------------------- the banners
b = load("docs/LODGEN_BTD_FORMAT.md")
assert b.startswith("# `.lodl` v1 / v2 - the whole-worldspace landscape file\n"), b[:60]
b = b.replace(
    "# `.lodl` v1 / v2 - the whole-worldspace landscape file\n",
    "# `.lodl` v1 / v2 - the whole-worldspace landscape file\n"
    "\n"
    "**THE EXTENSION CHANGED ON 2026-09-09 AND THE BYTES DID NOT.** bungo's\n"
    "ruling: this file is `.lodl`; `.lodt`, which it used to be, now names the\n"
    "TERRAIN TEXTURE sheets (`docs/LODGEN_TERRAIN_VT.md`). The magic is still\n"
    "`LODT` on disk, deliberately, because the gate on the rename is that a\n"
    "`.lodl` is byte-identical to the `.lodt` the same worldspace wrote the day\n"
    "before. What separates the two formats is the texture container's own new\n"
    "magic, `LDTX`: **each reader refuses the other's file BY NAME**, so a\n"
    "yesterday's `.lodt` opened as a terrain texture says it is the landscape\n"
    "file, and vice versa. The C++ names (`LodtFile`, `lodtWrite`,\n"
    "`src/lodtfile.cpp`) did NOT move -- they are internal, and they appear in\n"
    "no file on disk and in no command.\n", 1)
save("docs/LODGEN_BTD_FORMAT.md", b)

t = load("docs/LODGEN_TERRAIN_VT.md")
assert t.startswith("# The terrain virtual texture — `.lodt` v1, and the ground-cover plane\n"), t[:80]
t = t.replace(
    "# The terrain virtual texture — `.lodt` v1, and the ground-cover plane\n",
    "# The terrain virtual texture — `.lodt` v1, and the ground-cover plane\n"
    "\n"
    "**THIS EXTENSION WAS REPURPOSED ON 2026-09-09, AND THE MAGIC MOVED WITH\n"
    "IT.** bungo's ruling: the terrain texture sheets are `.lodt` (they were\n"
    "`.lodv`), and the whole-worldspace LANDSCAPE file, which was `.lodt`, is\n"
    "`.lodl` (`docs/LODGEN_BTD_FORMAT.md`). Because the extension now means\n"
    "something else, the container takes its OWN magic, `LDTX` -- it was `LODV`\n"
    "-- and both readers refuse the other's file by name: `lodvValidate` names\n"
    "the landscape file when handed `LODT`, and names a stale `LODV` container\n"
    "too; `LodtFile::open` names this one when handed `LDTX`. No `.lodv` was\n"
    "ever written to disk anywhere, so nothing needs converting. The C++ names\n"
    "(`LodvWriter`, `lodvValidate`, `LODV_ROLE_*`, `src/io/lodvfile.cpp`) did\n"
    "NOT move.\n", 1)
save("docs/LODGEN_TERRAIN_VT.md", t)

# ------------------------------------------------- the native far field: .lodo
OLD = "docs/LODGEN_NATIVE_LODG_LODI.md"
NEW = "docs/LODGEN_NATIVE_LODO_LODI.md"
if os.path.exists(OLD):
    r = subprocess.run(["git", "mv", OLD, NEW], capture_output=True, text=True)
    if r.returncode != 0:
        os.rename(OLD, NEW)
    print("renamed", OLD, "->", NEW)
n = load(NEW)
n, _ = rep(n, LAND, NEW)
n, _ = rep(n, TEX, NEW)
n, _ = rep(n, [
    (".lodg", ".lodo"),
    ("`LODG`", "`LODO`"),
    ("lodgIdentity", "lodoIdentity"),
    ("LODGEN_NATIVE_LODG_LODI.md", "LODGEN_NATIVE_LODO_LODI.md"),
], NEW)
# the ruling this page quotes was SUPERSEDED the same afternoon
n = n.replace(
    """**bungo's ruling, 2026-09-09 ~16:1x**, verbatim: *"lodg sounds better"*, after""",
    """**bungo's FINAL ruling, 2026-09-09 ~16:4x**, which SUPERSEDES the ~16:1x
*"lodg sounds better"*: the object library is **`.lodo`**, `.lodg` is retired,
and the rest of the family is `.lodl` land, `.lodt` terrain textures, `.lodi`
instances, `.lodm` materials. The earlier exchange, kept because it is what the
rest of this page was written against:""", 1)
save(NEW, n)

# every other file that names the renamed document
for p in ("docs/LODGEN_TEXTURE_ARRAYS.md",):
    s = load(p)
    s = s.replace("LODGEN_NATIVE_LODG_LODI.md", "LODGEN_NATIVE_LODO_LODI.md")
    save(p, s)

print("\n---- replacements ----")
for p, a, bb, c in REPORT:
    print("  %-42s %-28s -> %-28s x%d" % (p, a, bb, c))

print("\n---- anything still saying lodt/lodv/lodg where it should not ----")
for p in DOCS + [NEW]:
    for i, ln in enumerate(load(p).split("\n"), 1):
        low = ln.lower()
        if ("lodv" in low or ".lodg" in low
                or re.search(r"(?<![a-z_])lodt(?![a-z_])", low)):
            print("  %s @%d: %s" % (p, i, ln.strip()[:110]))
