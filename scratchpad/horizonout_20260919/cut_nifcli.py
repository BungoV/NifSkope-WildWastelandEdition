#!/usr/bin/env python3
"""HORIZONOUT: the horizon SWITCHES leave the command line.

`--horizon-azimuths`, `--horizon-reach`, `--horizon-near-skip`,
`--horizon-refute`, `--horizon-selftest`, `--horizon-subdivide`,
`--horizon-face-sheet`, `--no-terrain-horizon` and `--vt-horizon-texel` are
GONE: an unknown switch now fails by name exactly as any other unknown switch
does, which is the brief's test. `--horizon-scrappable` is renamed
`--scrappable` -- the one piece of lane HORIZON3 that is kept. `--lodi-v6` and
`--lodi-v7` stay, and `--lodi-v7` is now the default a bake already writes.
"""
import sys

CHECK = "--check" in sys.argv
P = r"E:/Projects/NifskopeWildWastelandEdition/src/nifcli.cpp"

with open(P, "rb") as f:
    raw = f.read()
cr = raw.count(b"\r")
d = raw.decode("utf-8")
orig = d
N = 0


def sub(old, new, what):
    global d, N
    c = d.count(old)
    if c != 1:
        sys.exit("REFUSED [%s]: anchor appears %d times, want 1" % (what, c))
    d = d.replace(old, new)
    N += 1


def cut(start, end, what, repl=""):
    global d, N
    a = d.find(start)
    if a < 0 or d.find(start, a + 1) >= 0:
        sys.exit("REFUSED [%s]: start anchor not unique" % what)
    b = d.find(end, a)
    if b < 0:
        sys.exit("REFUSED [%s]: end anchor not found after start" % what)
    b += len(end)
    nl = d.find("\n", b)
    b = len(d) if nl < 0 else nl + 1
    d = d[:a] + repl + d[b:]
    N += 1


# --- the include ----------------------------------------------------------
sub('#include "lodghorizonrefute.h"\n', "", "include lodghorizonrefute.h")

# --- runLodgen's signature ------------------------------------------------
sub("""\tbool libraryNear, bool ladderFoliage, float silhouetteMin, bool placementAo, bool vertexAo, bool lodiV7,
\tbool lodiHorizon, int horizonAzimuths, float horizonReach, bool terrainHorizon,
\tint horizonRefute, float horizonNearSkip,
\tfloat horizonSubdivide, float horizonFaceSheet, bool horizonScrappable, bool treesOnly,
\tbool aggregate, int aggMin, int aggTile, int aggViews )""",
    """\tbool libraryNear, bool ladderFoliage, float silhouetteMin, bool placementAo, bool vertexAo, bool lodiV7,
\tbool scrappable, bool treesOnly,
\tbool aggregate, int aggMin, int aggTile, int aggViews )""",
    "runLodgen signature")

# --- the vt options copy --------------------------------------------------
cut("""\t/* v8 (lane HORIZON1, 2026-09-18). The terrain horizon sheet takes the SAME""",
    """\tvtOpts.horizonRefute = horizonRefute;
""", "vtOpts horizon copy")

# --- the native call site -------------------------------------------------
sub("""\t\t\tlodgenNativeHorizonOption( lodiHorizon, horizonAzimuths, horizonReach, horizonRefute,
\t\t\t\thorizonNearSkip );
\t\t\tlodgenNativeSubdivideOption( horizonSubdivide, horizonFaceSheet, horizonScrappable );
""",
    "\t\t\tlodgenNativeScrappableOption( scrappable );\n",
    "native option call site")

# --- usage ----------------------------------------------------------------
sub("""\t  << "  lodgen ... --native <dir> --lodi-v7 | --horizon-azimuths 16 --horizon-reach 127561 | --no-terrain-horizon\\n"
\t  << "  lodgen ... --horizon-near-skip 1        (0 = read a lattice inside the receiver's own square)\\n"
\t  << "  lodgen ... --native <dir> --horizon-subdivide <u> --horizon-face-sheet <a>   (both 0 = off, the default)\\n"
""",
    """\t  << "  lodgen ... --native <dir> --lodi-v7\\n"
\t  << "  lodgen ... --native <dir> --scrappable  (off by default; .lodi v9 instance bit 6)\\n"
""",
    "usage lines")

# --- the option docs ------------------------------------------------------
sub(""" *   --lodi-v7                 no per-vertex horizon stream and no role-7 terrain
 *                             horizon sheet; the .lodi stays at version 7 and the
 *                             .lodt at its roles, byte for byte
 *   --horizon-azimuths <n>    bins a receiver, 4..64 in steps of 4 (default 16)
 *   --horizon-reach <u>       how far the horizon march walks, world units
 *   --horizon-near-skip <c>   cells of its own square each lattice is not read
 *                             within (default 1; 0 = the pre-lane bytes)
 *                             (default 127,561 = the tallest Commonwealth
 *                             placement at a 5-degree sun)
 *   --horizon-subdivide <u>   longest edge a library triangle may keep,
 *                             world units at scale 1 (0 = off, the default)
 *   --horizon-face-sheet <a>  smallest face that earns a horizon sheet,
 *                             square world units (0 = off, the default)
 *   --horizon-scrappable      mark the placements a player can scrap at a
 *                             workshop (.lodi v9 instance bit 6; off = v8)
 *   --no-terrain-horizon      the object stream only; the .lodt keeps today's
 *                             roles, byte for byte
 *   --vt-horizon-texel <u>    world units a horizon texel, which picks the
 *                             pyramid level the role-7 sheets are authored on
 *                             (default 128 = the LAND node spacing)
 *   --horizon-refute <n>      re-cast <n> receivers a tile (and one vertex in
 *                             <n>) at full resolution and print the agreement
 *                             at eight sun positions, with the 90-degree
 *                             rotation as the control; 0 = off (default)
 *   --horizon-selftest        cast one 1,000-unit box on flat ground and check
 *                             the answer against atan(); needs no <file>, no
 *                             data root and no bake. `WW_HORIZON_TEST=1` too */""",
    """ *   --lodi-v7                 accepted and a NO-OP since 2026-09-19: version 7 is
 *                             what a default bake writes. It is kept because
 *                             command lines and gates carry it, and because it
 *                             still says what it always said -- "this .lodi is
 *                             version 7". The baked-horizon route it used to
 *                             turn off no longer exists; see the history
 *                             paragraph in docs/LODGEN_NATIVE_LODO_LODI.md.
 *   --scrappable              mark the placements a player can scrap at a
 *                             workshop (.lodi v9 instance bit 6; off = v7) */""",
    "option docs")

# --- the defaults block ---------------------------------------------------
cut("""\t/* v8 (lane HORIZON1, 2026-09-18). ON by default like the two above it, and""",
    """\tbool lgHorizonSelfTest = !qEnvironmentVariableIsEmpty( "WW_HORIZON_TEST" )
\t\t&& qEnvironmentVariable( "WW_HORIZON_TEST" ) != QLatin1String( "0" );
""",
    "lgHorizon* defaults",
    """\t/* v9 (lane HORIZON3, 2026-09-19; kept by lane HORIZONOUT when the rest of
\t * that lane's baked-horizon route was dropped the same day): mark the
\t * placements a player can scrap at a workshop. OFF, and off is the exact
\t * way back -- no bit is written and the .lodi stays at version 7, byte for
\t * byte. */
\tbool lgScrappable = false;
""")

# --- the --vt-horizon-texel switch ---------------------------------------
cut("""\t\t/* WHICH LEVEL the horizon sheet is authored on, by texel width: the""",
    """\t\t\tlgVt.horizonTexel = v;
\t\t}
""", "--vt-horizon-texel")

# --- the --lodi-v7 / --no-terrain-horizon / refuter / azimuths / reach /
#     near-skip / subdivide / face-sheet / scrappable switches -------------
cut("""\t\telse if ( t == QLatin1String( "--lodi-v7" ) ) lgLodiHorizon = false;""",
    """\t\telse if ( t == QLatin1String( "--horizon-scrappable" ) ) lgHorizonScrappable = true;""",
    "horizon switch block",
    """\t\t/* ACCEPTED AND A NO-OP since lane HORIZONOUT (2026-09-19): a default
\t\t * bake writes version 7 already. It stays on the command line because
\t\t * gates and saved command lines carry it and because it still states a
\t\t * true fact about the file that comes out. */
\t\telse if ( t == QLatin1String( "--lodi-v7" ) ) { }
\t\t/* v9 (lane HORIZON3, 2026-09-19). A placement a player can scrap is a
\t\t * placement that WILL NOT BE THERE, and a far field that keeps drawing
\t\t * it is wrong about a settlement from the first hour of a save onwards.
\t\t * The bit says which ones those are; the three-clause rule is read out
\t\t * of the plugin (src/esmdata.h, EsmScrapIndex). */
\t\telse if ( t == QLatin1String( "--scrappable" ) ) lgScrappable = true;
""")

# --- the self-test entry --------------------------------------------------
cut("""\t/* THE SYNTHETIC SELF-TEST (lane HORIZON1, 2026-09-18). Here, above the""",
    """\t\treturn ok ? 0 : 1;
\t}
""", "--horizon-selftest entry")

# --- the call site's argument list ---------------------------------------
sub("""\t\t\tlgLibraryNear, lgNativeLadderFoliage, lgNativeSilhouette, lgNativePlacementAo, lgNativeVertexAo, lgLodiV7,
\t\t\tlgLodiHorizon, lgHorizonAzimuths, lgHorizonReach, lgTerrainHorizon, lgHorizonRefute,
\t\t\tlgHorizonNearSkip,
\t\t\tlgHorizonSubdivide, lgHorizonFaceSheet, lgHorizonScrappable,
\t\t\tlgTreesOnly, lgAggregate, lgAggMin, lgAggTile, lgAggViews );""",
    """\t\t\tlgLibraryNear, lgNativeLadderFoliage, lgNativeSilhouette, lgNativePlacementAo, lgNativeVertexAo, lgLodiV7,
\t\t\tlgScrappable,
\t\t\tlgTreesOnly, lgAggregate, lgAggMin, lgAggTile, lgAggViews );""",
    "runLodgen call site")

print("nifcli.cpp %2d edits, %d -> %d chars, %d -> %d lines (CR %d -> %d)"
      % (N, len(orig), len(d), orig.count("\n"), d.count("\n"), cr, d.count("\r")))
if d.count("\r") != cr:
    sys.exit("REFUSED: CR count moved")
if not CHECK:
    with open(P, "wb") as f:
        f.write(d.encode("utf-8"))
    print("OK")
else:
    print("--check: nothing written")
