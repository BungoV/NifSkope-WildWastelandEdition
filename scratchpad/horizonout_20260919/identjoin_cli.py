#!/usr/bin/env python3
"""HORIZONOUT step 9: `--identity-join-gap <u>` and `--identity-join legacy`."""
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


# --- the signature --------------------------------------------------------
sub("""\tbool scrappable, bool treesOnly,
\tbool aggregate, int aggMin, int aggTile, int aggViews )""",
    """\tbool scrappable, bool identityJoinLegacy, float identityJoinGap, bool treesOnly,
\tbool aggregate, int aggMin, int aggTile, int aggViews )""",
    "runLodgen signature")

# --- the call into the emitter -------------------------------------------
sub("""\t\t\tlodgenNativeScrappableOption( scrappable );
""",
    """\t\t\tlodgenNativeScrappableOption( scrappable );
\t\t\tlodgenNativeIdentityJoinOption( identityJoinLegacy, identityJoinGap );
""",
    "option call site")

# --- usage ----------------------------------------------------------------
sub("""\t  << "  lodgen ... --native <dir> --scrappable  (off by default; .lodi v9 instance bit 6)\\n"
""",
    """\t  << "  lodgen ... --native <dir> --scrappable  (off by default; .lodi v9 instance bit 6)\\n"
\t  << "  lodgen ... --native <dir> --identity-join-gap 64 | --identity-join legacy\\n"
\t  << "                                          v7 GROUPING: a non-tree placement joins\\n"
\t  << "                                          a group when its LOD MESH is within the\\n"
\t  << "                                          gap of another's (bungo 2026-09-19);\\n"
\t  << "                                          `legacy` is the old architecture-only\\n"
\t  << "                                          16-unit BOX rule, byte for byte\\n"
""",
    "usage lines")

# --- the option docs ------------------------------------------------------
sub(""" *   --scrappable              mark the placements a player can scrap at a
 *                             workshop (.lodi v9 instance bit 6; off = v7) */""",
    """ *   --scrappable              mark the placements a player can scrap at a
 *                             workshop (.lodi v9 instance bit 6; off = v7)
 *   --identity-join-gap <u>   v7 GROUPING (bungo's ruling 2026-09-19): how close
 *                             two placements' LOD MESHES must come, in world
 *                             units, before they are one identity. Default 64,
 *                             measured by lane IDENTPROX; 128 is the last gap at
 *                             which no identity holds two different reference
 *                             buildings. Trees never join.
 *   --identity-join legacy    the way back: the pre-2026-09-19 rule, only an
 *                             `architecture`-pathed placement, joined on a WORLD
 *                             AXIS-ALIGNED BOX gap of 16 u. `--identity-join
 *                             proximity` says the default out loud. */""",
    "option docs")

# --- the defaults ---------------------------------------------------------
sub("""\tbool lgScrappable = false;
""",
    """\tbool lgScrappable = false;
\t/* v7 GROUPING, bungo's ruling of 2026-09-19 (lane IDENTPROX measured it,
\t * lane HORIZONOUT shipped it). This one is NOT off by default: it is a
\t * RULING, not a module, and the rule it replaces is the way back. */
\tbool lgIdentityJoinLegacy = false;
\tfloat lgIdentityJoinGap = 64.0f;
""",
    "defaults")

# --- the switches ---------------------------------------------------------
sub("""\t\telse if ( t == QLatin1String( "--scrappable" ) ) lgScrappable = true;
""",
    """\t\telse if ( t == QLatin1String( "--scrappable" ) ) lgScrappable = true;
\t\t/* v7 GROUPING. The MEASURE is not a knob and the GAP is: lane IDENTPROX
\t\t * measured that a box gap of any size bridges a street, so `legacy` gets
\t\t * the whole old rule (measure and number together) and there is no way to
\t\t * ask for the old measure at a new number. */
\t\telse if ( t == QLatin1String( "--identity-join" ) ) {
\t\t\tconst QString v = next().toLower();
\t\t\tif ( v == QLatin1String( "legacy" ) ) {
\t\t\t\tlgIdentityJoinLegacy = true;
\t\t\t} else if ( v == QLatin1String( "proximity" ) ) {
\t\t\t\tlgIdentityJoinLegacy = false;
\t\t\t} else {
\t\t\t\terr() << "error: --identity-join takes proximity or legacy, not '" << v << "'" << Qt::endl;
\t\t\t\treturn 2;
\t\t\t}
\t\t}
\t\telse if ( t == QLatin1String( "--identity-join-gap" ) ) {
\t\t\tbool ok = false;
\t\t\tconst QString sv = next();
\t\t\tconst float v = sv.toFloat( &ok );
\t\t\tif ( !ok || !( v >= 0.0f ) || v > 100000.0f ) {
\t\t\t\terr() << "error: --identity-join-gap takes world units in 0..100000, not '"
\t\t\t\t\t  << sv << "'" << Qt::endl;
\t\t\t\treturn 2;
\t\t\t}
\t\t\tlgIdentityJoinGap = v;
\t\t}
""",
    "switches")

# --- the call site --------------------------------------------------------
sub("""\t\t\tlgScrappable,
\t\t\tlgTreesOnly, lgAggregate, lgAggMin, lgAggTile, lgAggViews );""",
    """\t\t\tlgScrappable, lgIdentityJoinLegacy, lgIdentityJoinGap,
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
