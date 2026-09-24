#!/usr/bin/env python3
"""HORIZONOUT: remove the baked-horizon bake side from src/nativeemit.cpp.

Refuses on any anchor it cannot find EXACTLY ONCE. Writes nothing unless every
edit resolves. --check writes nothing at all.
"""
import sys, io

PATH = r"E:/Projects/NifskopeWildWastelandEdition/src/nativeemit.cpp"
CHECK = "--check" in sys.argv

with open(PATH, "rb") as f:
    raw = f.read()
CR = raw.count(b"\r")
if CR != 0:
    sys.exit("REFUSED: %s carries %d CR bytes; this script assumes LF-only" % (PATH, CR))
d = raw.decode("utf-8")
orig_len = len(d)

edits = []          # (kind, a, b, replacement) resolved on the ORIGINAL string


def find1(s, what):
    i = d.find(s)
    if i < 0:
        sys.exit("REFUSED: anchor not found (%s): %r" % (what, s[:80]))
    j = d.find(s, i + 1)
    if j >= 0:
        sys.exit("REFUSED: anchor found twice (%s) at %d and %d: %r" % (what, i, j, s[:80]))
    return i


def cut(start, end, what, repl=""):
    """Delete from the start of `start` through the end of the line holding `end`."""
    a = find1(start, what + " [start]")
    b = d.find(end, a)
    if b < 0:
        sys.exit("REFUSED: end anchor not found after start (%s): %r" % (what, end[:80]))
    b += len(end)
    nl = d.find("\n", b)
    b = len(d) if nl < 0 else nl + 1
    edits.append((what, a, b, repl))


def sub(old, new, what):
    a = find1(old, what)
    edits.append((what, a, a + len(old), new))


# ---- 1. the three headers -------------------------------------------------
cut('#include "lodghorizon.h"', '#include "lodgsubdiv.h"', "includes")

# ---- 2. NativeLadderOptions: the horizon + tier2/3 knobs ------------------
cut("\t/*! v8 (2026-09-18, lane HORIZON1): the per-vertex HORIZON stream, s4.11.",
    "\tfloat horizonFaceSheet = 0.0f;", "options struct knobs")

sub("""\t/*! v9 (lane HORIZON3, 2026-09-19): mark the placements a player can scrap
\t *  at a workshop, `LODI_INST_SCRAPPABLE`. OFF by default, and off is the
\t *  exact way back -- no bit, `.lodi` version 8, byte for byte. */
\tbool scrappable = false;""",
    """\t/*! v9 (lane HORIZON3, 2026-09-19; kept by lane HORIZONOUT when the rest of
\t *  that lane's work was dropped): mark the placements a player can scrap at
\t *  a workshop, `LODI_INST_SCRAPPABLE`. It is what excludes a workshop-owned
\t *  caster from the runtime far shadow map, which is the identity route's
\t *  own need and outlived the baked-horizon route it was written beside.
\t *  OFF by default, and off is the exact way back -- no bit, `.lodi`
\t *  version 7, byte for byte. */
\tbool scrappable = false;""",
    "scrappable doc comment")

# ---- 3. the mirror struct -------------------------------------------------
cut("\tbool lodiV7 = true;\n\tbool lodiHorizon = true;\n\tint horizonAzimuths",
    "\tfloat horizonFaceSheet = 0.0f;      //!< v5 tier 3: smallest face that earns a sheet, u^2; 0 = off",
    "mirror struct knobs", "\tbool lodiV7 = true;\n")
sub("\tbool scrappable = false;            //!< v9: mark workshop-scrappable placements; off = .lodi v8",
    "\tbool scrappable = false;            //!< v9: mark workshop-scrappable placements; off = .lodi v7",
    "mirror struct scrappable comment")

# ---- 4. the copy ----------------------------------------------------------
cut("\ts.lodiHorizon = o.lodiHorizon;", "\ts.horizonFaceSheet = o.horizonFaceSheet;", "options copy")

# ---- 5. lodgenNativeHorizonOption ----------------------------------------
cut("/*! v8: the horizon module and its two knobs, one call because the three are",
    "\to.horizonRefute = ( refute > 0 ) ? refute : 0;\n}\n", "lodgenNativeHorizonOption")

# ---- 6. lodgenNativeSubdivideOption -> lodgenNativeScrappableOption -------
a = find1("/*! v5 (lane HORIZON3, 2026-09-19): the two knobs that cut the LIBRARY rather",
          "lodgenNativeSubdivideOption [start]")
b = d.find("\to.scrappable = scrappable;\n}\n", a)
if b < 0:
    sys.exit("REFUSED: lodgenNativeSubdivideOption body end not found")
b += len("\to.scrappable = scrappable;\n}\n")
edits.append(("lodgenNativeSubdivideOption", a, b,
    """/*! v9 (lane HORIZON3, kept by lane HORIZONOUT): the workshop-scrappable bit.
 *  OFF by default, and off is the exact way back -- no bit is written and the
 *  `.lodi` stays at version 7, byte for byte.
 *
 *  It ships OFF because it has not been flown. bungo's standing rule of
 *  2026-09-17 is that an owed ruling never ships as a default. */
void lodgenNativeScrappableOption( bool scrappable )
{
\tladderOpts().scrappable = scrappable;
}
"""))

# ---- 7. subdivStats declaration ------------------------------------------
cut("\t//! v5 tier 2 (lane HORIZON3): what the library cut did, for the census line.",
    "\tLodgenSubdivStats subdivStats;", "subdivStats decl")

# ---- 8. the tier-2 cut call site -----------------------------------------
cut("\t\t/* v5 TIER 2, THE CUT (lane HORIZON3, 2026-09-19). It runs HERE, after",
    "\t\t\tif ( !lodoSubdivideLibrary( lib, s.horizonSubdivide, meshMaxScale, &subdivStats, &err ) )\n\t\t\t\treturn fail( err );\n\t\t}\n",
    "tier-2 cut call site")

# ---- 9. the vhor census accumulators -------------------------------------
cut("\tint vhorInstances = 0, vhorSteps = 0, vhorLandCells = 0;",
    "\tLodgenHorizonRefute vhorRefute;", "vhor accumulators")

# ---- 10. set.horizon arming ----------------------------------------------
cut("\t\t/* v8: the horizon rides the same loop too, but NOT the same scene and",
    "\t\tset.horizonReach = s.horizonReach;", "set.horizon arming")

# ---- 11. the far lattice + hcast -----------------------------------------
a = find1("\t\t/* ---- v8: THE FAR LATTICE (src/lodghorizon.h) ----------------------",
          "far lattice [start]")
END11 = "\t\t// which chunk each instance belongs to, and which instances a chunk lights"
b = d.find(END11, a)
if b < 0:
    # fall back: the far-lattice block ends where the chunk table begins
    for cand in ("\t\tstd::vector<std::tuple<int, int, int>> chunkKeys;",
                 "\t\t// the chunk table"):
        b = d.find(cand, a)
        if b >= 0:
            break
if b < 0:
    sys.exit("REFUSED: far-lattice block end not found")
edits.append(("far lattice", a, b, ""))

# ---- 12. the refuter table -----------------------------------------------
cut("\t\t/* THE REFUTER, one table a chunk and merged serially afterwards: no",
    "\t\tconst int refuteEvery = ( s.horizonRefute > 0 ) ? s.horizonRefute : 0;", "refuter table")

# ---- 13. the near lattice ------------------------------------------------
cut("\t\t\t/* v8: the NEAR lattice, the same triangles the AO scene gets and in",
    "\t\t\t\t\tLODGEN_HORIZON_NEAR_CELL );", "near lattice")

# ---- 14. nearHz.raiseBox inside addTriangle ------------------------------
sub("""\t\t\t\t\t\tscene.addTriangle( ta, tb, tc );
\t\t\t\t\t\tif ( set.horizon )
\t\t\t\t\t\t\tnearHz.raiseBox( std::min( ta[0], std::min( tb[0], tc[0] ) ) * dimF,
\t\t\t\t\t\t\t\tstd::min( ta[1], std::min( tb[1], tc[1] ) ) * dimF,
\t\t\t\t\t\t\t\tstd::max( ta[0], std::max( tb[0], tc[0] ) ) * dimF,
\t\t\t\t\t\t\t\tstd::max( ta[1], std::max( tb[1], tc[1] ) ) * dimF,
\t\t\t\t\t\t\t\tstd::max( ta[2], std::max( tb[2], tc[2] ) ) * dimF );
\t\t\t\t\t}""",
    """\t\t\t\t\t\tscene.addTriangle( ta, tb, tc );
\t\t\t\t\t}""",
    "nearHz.raiseBox")

# ---- 15. nearHz.buildMips ------------------------------------------------
sub("\t\t\tif ( set.horizon )\n\t\t\t\tnearHz.buildMips();\n", "", "nearHz.buildMips")

# ---- 16. the per-vertex horizon cast in the AO loop ----------------------
sub("""\t\t\t\tconst bool wantSky = set.vertexSky;
\t\t\t\tconst bool wantHorizon = set.horizon;
\t\t\t\tconst int hzA = int( set.horizonAzimuths );
""",
    "\t\t\t\tconst bool wantSky = set.vertexSky;\n", "wantHorizon locals")

sub("""\t\t\t\t\tif ( wantSky )
\t\t\t\t\t\tr.vertexSky.resize( d.count );
\t\t\t\t\tif ( wantHorizon )
\t\t\t\t\t\tr.vertexHorizon.assign( size_t( d.count ) * size_t( hzA ), 0 );
""",
    "\t\t\t\t\tif ( wantSky )\n\t\t\t\t\t\tr.vertexSky.resize( d.count );\n",
    "vertexHorizon.assign")

a = find1("\t\t\t\t\t\tif ( wantHorizon ) {", "per-vertex horizon cast [start]")
END16 = """\t\t\t\t\t\t}
\t\t\t\t\t}
\t\t\t\t}, false );"""
b = d.find(END16, a)
if b < 0:
    sys.exit("REFUSED: per-vertex horizon cast end not found")
b += len("\t\t\t\t\t\t}\n")
edits.append(("per-vertex horizon cast", a, b, ""))

# ---- 17. the refuter merge -----------------------------------------------
sub("\t\tfor ( const LodgenHorizonRefute & c : chunkRefute )\n\t\t\tvhorRefute.merge( c );\n",
    "", "refuter merge")

# ---- 18. the vhor read-back ----------------------------------------------
cut("\t\tif ( set.horizon ) {\n\t\t\tvhorAz = int( set.horizonAzimuths );",
    "\t\t\t\t\t\tvhorZero++;\n\t\t\t\t}\n\t\t\t}\n\t\t}\n", "vhor read-back")

# ---- 19. the census: horizon line + refuter line -------------------------
cut("\t\t/* v8 (2026-09-18, lane HORIZON1). The words the brief names, and every",
    "\t\t\t\t.arg( vhorRefute.census( QStringLiteral( \"vhorRefute\" ) ).join( QStringLiteral( \" \" ) ) );",
    "census horizon + refuter lines")

# ---- 20. the census: the tier-2 subdivide line ---------------------------
cut("\t\t/* v5 TIER 2 (lane HORIZON3, 2026-09-19), on its own prefix. Every",
    "\t\t\t\t\t\"byte for byte what it was before this knob existed\" ) );",
    "census subdivide line")

# ---- 21. the scrappable census: the OFF text and the switch name ---------
sub("""\t\t\t\t: QStringLiteral( "OFF (--horizon-scrappable, the default); no bit is written and the .lodi "
\t\t\t\t\t"stays at version 8, byte for byte; scrappablePlacements 0" ) );""",
    """\t\t\t\t: QStringLiteral( "OFF (--scrappable, the default); no bit is written and the .lodi "
\t\t\t\t\t"stays at version 7, byte for byte; scrappablePlacements 0" ) );""",
    "scrappable census OFF text")

# ---- 22. the scrappable census comment cites a gate this lane deletes -----
sub(""" * `scratchpad/horizon3_20260919/scrap_rule.py` and re-derived from the
\t\t * written file, without calling any of this code, by
\t\t * `tests/spells/lodgen_horizon_witness.py`. A bake whose number is not""",
    """ * `scratchpad/horizon3_20260919/scrap_rule.py` and re-derived from the
\t\t * written file, without calling any of this code, by
\t\t * `tests/spells/lodgen_scrappable.sh`. A bake whose number is not""",
    "scrappable census witness citation")

# ---- apply, back to front -------------------------------------------------
edits.sort(key=lambda e: e[1])
for i in range(1, len(edits)):
    if edits[i][1] < edits[i - 1][2]:
        sys.exit("REFUSED: edits overlap: %s and %s" % (edits[i - 1][0], edits[i][0]))
out = d
for what, a, b, repl in reversed(edits):
    out = out[:a] + repl + out[b:]

removed = orig_len - len(out)
print("edits resolved: %d" % len(edits))
for what, a, b, repl in edits:
    print("  %-32s  -%d chars%s" % (what, (b - a) - len(repl), "  (replacement)" if repl else ""))
print("chars %d -> %d  (-%d)" % (orig_len, len(out), removed))
print("lines %d -> %d" % (d.count("\n"), out.count("\n")))

leftovers = [(n + 1, ln) for n, ln in enumerate(out.split("\n"))
             if ("horizon" in ln.lower() or "subdiv" in ln.lower())
             and "horizontal" not in ln.lower()]
print("LEFTOVER horizon/subdiv lines: %d" % len(leftovers))
for n, ln in leftovers:
    print("  %5d: %s" % (n, ln.strip()[:110]))

if CHECK:
    print("--check: nothing written")
    sys.exit(0)
if out.count("\r"):
    sys.exit("REFUSED: result carries CR bytes")
with open(PATH, "wb") as f:
    f.write(out.encode("utf-8"))
print("WRITTEN %s" % PATH)
